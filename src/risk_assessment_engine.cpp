#include <CLI/CLI.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

#include <risk/eigen_stub.hpp>

#include <algorithm>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <risk/greeks.hpp>
#include <risk/hvar.hpp>
#include <risk/instrument.hpp>
#include <risk/instrument_soa.hpp>
#include <risk/kdb_connection.hpp>
#include <risk/market.hpp>
#include <risk/mcvar.hpp>
#include <risk/portfolio.hpp>
#include <risk/universe.hpp>
#include <risk/utils.hpp>

namespace {

Eigen::VectorXd compute_sample_mean(const std::vector<double>& shocks,
                                    std::size_t scenarios,
                                    std::size_t factors) {
    if (scenarios == 0 || factors == 0) {
        throw std::invalid_argument("compute_sample_mean requires positive dimensions");
    }
    if (shocks.size() != scenarios * factors) {
        throw std::invalid_argument("shock matrix size mismatch for mean computation");
    }

    Eigen::VectorXd mean = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(factors));
    for (std::size_t t = 0; t < scenarios; ++t) {
        const std::size_t offset = t * factors;
        for (std::size_t i = 0; i < factors; ++i) {
            mean(static_cast<Eigen::Index>(i)) += shocks[offset + i];
        }
    }
    const double inv = 1.0 / static_cast<double>(scenarios);
    for (std::size_t i = 0; i < factors; ++i) {
        mean(static_cast<Eigen::Index>(i)) *= inv;
    }
    return mean;
}

Eigen::MatrixXd compute_sample_covariance(const std::vector<double>& shocks,
                                          const Eigen::VectorXd& mean,
                                          std::size_t scenarios,
                                          std::size_t factors) {
    if (factors == 0) {
        throw std::invalid_argument("compute_sample_covariance requires positive factors");
    }
    if (shocks.size() != scenarios * factors) {
        throw std::invalid_argument("shock matrix size mismatch for covariance computation");
    }
    if (mean.size() != static_cast<Eigen::Index>(factors)) {
        throw std::invalid_argument("mean vector dimension mismatch");
    }

    Eigen::MatrixXd cov = Eigen::MatrixXd::Zero(static_cast<Eigen::Index>(factors),
                                                static_cast<Eigen::Index>(factors));
    if (scenarios <= 1) {
        return cov;
    }

    std::vector<double> diff(factors, 0.0);
    for (std::size_t t = 0; t < scenarios; ++t) {
        const std::size_t offset = t * factors;
        for (std::size_t i = 0; i < factors; ++i) {
            diff[i] = shocks[offset + i] - mean(static_cast<Eigen::Index>(i));
        }
        for (std::size_t i = 0; i < factors; ++i) {
            for (std::size_t j = 0; j < factors; ++j) {
                const double increment = diff[i] * diff[j];
                cov(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) += increment;
            }
        }
    }

    const double inv = 1.0 / static_cast<double>(scenarios - 1);
    for (std::size_t i = 0; i < factors; ++i) {
        for (std::size_t j = 0; j < factors; ++j) {
            cov(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) *= inv;
        }
    }
    return cov;
}

} // namespace

int main(int argc, char** argv) {
    CLI::App app{"risk_assessment_engine"};

    std::string portfolio_path;
    std::string market_path;
    std::string kdb_host = "localhost";
    int kdb_port = 5000;
    std::string kdb_credentials;
    bool connect_to_kdb = false;

    app.add_option("-p,--portfolio", portfolio_path, "Portfolio CSV path")->required();
    app.add_option("-m,--market", market_path, "Market closes CSV path")->required();
    app.add_option("--kdb-host", kdb_host, "KDB+ host to connect to")->default_val(kdb_host);
    app.add_option("--kdb-port", kdb_port, "KDB+ port number")->default_val(kdb_port);
    app.add_option("--kdb-auth", kdb_credentials, "KDB+ credentials in user:password form");
    app.add_flag("--connect-kdb", connect_to_kdb, "Connect to the configured KDB+ instance before processing");

    try {
        CLI11_PARSE(app, argc, argv);

        std::optional<risk::kdb::Connection> kdb_connection;
        if (connect_to_kdb) {
            kdb_connection.emplace(kdb_host, kdb_port, kdb_credentials);
            spdlog::info("Connected to KDB+ at {}:{}.", kdb_host, kdb_port);
        } else {
            spdlog::info("Skipping KDB+ connection (use --connect-kdb to enable).");
        }

        std::vector<std::string> dates;
        std::vector<double> prices_flat;
        std::size_t T = 0;
        std::size_t N = 0;

        if (!risk::load_closes_csv(market_path, dates, prices_flat, T, N)) {
            return 1;
        }
        if (N != risk::universe_size()) {
            spdlog::error("Loaded universe size does not match expected universe");
            return 1;
        }
        if (T < 2) {
            spdlog::error("Need at least two rows of market data to compute shocks");
            return 1;
        }

        spdlog::info("Loaded market data from '{}' with {} rows and {} tickers.", market_path, T, N);

        std::vector<double> shocks_flat;
        risk::compute_shocks(prices_flat, T, N, shocks_flat);
        const std::size_t scenario_count = T - 1;

        risk::InstrumentSoA portfolio;
        if (!risk::load_portfolio_csv(portfolio_path, portfolio, N)) {
            return 1;
        }
        if (portfolio.size() == 0U) {
            spdlog::error("Portfolio CSV produced no instruments");
            return 1;
        }

        std::size_t option_count = 0;
        for (std::size_t i = 0; i < portfolio.size(); ++i) {
            if (portfolio.type[i] == static_cast<std::uint8_t>(risk::InstrumentType::Option)) {
                ++option_count;
            }
        }
        const std::size_t equity_count = portfolio.size() - option_count;
        spdlog::info("Loaded portfolio from '{}' with {} instruments ({} equities, {} options).",
                     portfolio_path,
                     portfolio.size(),
                     equity_count,
                     option_count);

        const double alpha = 0.99;

        const risk::RiskMetrics hist_metrics = risk::compute_hvar(portfolio,
                                                                  shocks_flat,
                                                                  scenario_count,
                                                                  N,
                                                                  alpha);

        Eigen::VectorXd mu = compute_sample_mean(shocks_flat, scenario_count, N);
        Eigen::MatrixXd cov = compute_sample_covariance(shocks_flat, mu, scenario_count, N);

        const risk::RiskMetrics mc_metrics = risk::compute_mcvar(portfolio,
                                                                 mu,
                                                                 cov,
                                                                 /*horizon_days=*/1.0,
                                                                 alpha,
                                                                 /*paths=*/200000,
                                                                 /*seed=*/123456789ULL);

        std::vector<risk::bs::BSGreeks> greeks_per_contract;
        std::vector<risk::bs::BSGreeks> greeks_position;
        risk::GreeksSummary totals;
        risk::compute_greeks(portfolio, greeks_per_contract, greeks_position, totals);

        const auto& symbols = risk::universe_symbols();

        constexpr double days_per_year = 252.0;
        auto theta_per_day = [&](double theta_year) { return theta_year / days_per_year; };
        auto vega_per_percent = [](double vega_one) { return vega_one / 100.0; };
        auto rho_per_percent = [](double rho_one) { return rho_one / 100.0; };

        spdlog::info("==================== Portfolio ====================");
        double portfolio_value = 0.0;
        for (std::size_t i = 0; i < greeks_per_contract.size(); ++i) {
            const double qty = portfolio.qty[i];
            const bool is_option = portfolio.type[i] == static_cast<std::uint8_t>(risk::InstrumentType::Option);
            const char* label = is_option ? (portfolio.is_call[i] ? "Call" : "Put")
                                          : symbols.at(portfolio.id[i]).c_str();

            const auto& pc = greeks_per_contract[i];
            const auto& pos = greeks_position[i];

            const double pc_theta_day = theta_per_day(pc.theta);
            const double pos_theta_day = theta_per_day(pos.theta);
            const double pc_vega_pct = vega_per_percent(pc.vega);
            const double pos_vega_pct = vega_per_percent(pos.vega);
            const double pc_rho_pct = rho_per_percent(pc.rho);
            const double pos_rho_pct = rho_per_percent(pos.rho);

            spdlog::info("Instrument {} ({})", portfolio.id[i], label);
            spdlog::info("  Price:    {:.4f} (per contract)", pc.price);
            spdlog::info("  Position: {:.4f} ({} units)", pos.price, qty);
            spdlog::info(
                "  Greeks per contract: Δ={:.4f} shares, Γ={:.4f} 1/$^2, ν={:.4f} $ per 1% vol, Θ={:.4f} $ per day, ρ={:.4f} $ per 1% rate",
                pc.delta,
                pc.gamma,
                pc_vega_pct,
                pc_theta_day,
                pc_rho_pct);
            spdlog::info(
                "  Greeks for position:   Δ={:.4f} shares, Γ={:.4f} 1/$^2, ν={:.4f} $ per 1% vol, Θ={:.4f} $ per day, ρ={:.4f} $ per 1% rate",
                pos.delta,
                pos.gamma,
                pos_vega_pct,
                pos_theta_day,
                pos_rho_pct);

            portfolio_value += pos.price;
        }

        const double portfolio_theta_day = theta_per_day(totals.theta);
        const double portfolio_vega_pct = vega_per_percent(totals.vega);
        const double portfolio_rho_pct = rho_per_percent(totals.rho);

        spdlog::info("");
        spdlog::info("Portfolio totals");
        spdlog::info("  Market value: {:.4f}", portfolio_value);
        spdlog::info("  Δ: {:.4f} shares", totals.delta);
        spdlog::info("  Γ: {:.4f} 1/$^2", totals.gamma);
        spdlog::info("  ν: {:.4f} $ per 1% vol", portfolio_vega_pct);
        spdlog::info("  Θ: {:.4f} $ per day", portfolio_theta_day);
        spdlog::info("  ρ: {:.4f} $ per 1% rate", portfolio_rho_pct);

        spdlog::info("");
        spdlog::info("==================== Historical ====================");
        spdlog::info("99% one-day HVaR: ${:.4f}", hist_metrics.var);
        spdlog::info("99% one-day HVaR (ES): ${:.4f}", hist_metrics.cvar);

        spdlog::info("==================== Monte Carlo ====================");
        spdlog::info("99% one-day MCVaR: ${:.4f}", mc_metrics.var);
        spdlog::info("99% one-day MCVaR (ES): ${:.4f}", mc_metrics.cvar);

        spdlog::info("==================== Greeks ====================");
        std::string header = "Greek   |";
        for (std::size_t i = 0; i < greeks_position.size(); ++i) {
            header += fmt::format(" {} |", symbols.at(portfolio.id[i]));
        }
        header += " Portfolio | Unit";
        spdlog::info(header);

        auto print_row = [&](const char* name,
                             auto extractor,
                             const char* unit,
                             double portfolio_value_row) {
            std::string line = fmt::format("{:>7} |", name);
            for (std::size_t i = 0; i < greeks_position.size(); ++i) {
                line += fmt::format(" {:>8.4f} |", extractor(greeks_position[i]));
            }
            line += fmt::format(" {:>9.4f} | {}", portfolio_value_row, unit);
            spdlog::info(line);
        };

        auto delta_extractor = [](const risk::bs::BSGreeks& g) { return g.delta; };
        auto gamma_extractor = [](const risk::bs::BSGreeks& g) { return g.gamma; };
        auto vega_extractor = [&](const risk::bs::BSGreeks& g) { return vega_per_percent(g.vega); };
        auto theta_extractor = [&](const risk::bs::BSGreeks& g) { return theta_per_day(g.theta); };
        auto rho_extractor = [&](const risk::bs::BSGreeks& g) { return rho_per_percent(g.rho); };

        print_row("Delta", delta_extractor, "shares", totals.delta);
        print_row("Gamma", gamma_extractor, "1/$^2", totals.gamma);
        print_row("Vega", vega_extractor, "$ per 1% vol", portfolio_vega_pct);
        print_row("Theta", theta_extractor, "$ per day", portfolio_theta_day);
        print_row("Rho", rho_extractor, "$ per 1% rate", portfolio_rho_pct);
    } catch (const CLI::ParseError& parse_error) {
        return app.exit(parse_error);
    } catch (const std::exception& ex) {
        spdlog::error("Failed to compute risk metrics: {}", ex.what());
        return 1;
    }

    return 0;
}
