#include <exception>
#include <iomanip>
#include <iostream>
#include <vector>

#include <risk/greeks.hpp>
#include <risk/hvar.hpp>
#include <risk/instrument.hpp>
#include <risk/instrument_soa.hpp>
#include <risk/mcvar.hpp>

int main() {
    std::vector<risk::Instrument> portfolio;
    portfolio.reserve(2);

    // Equity leg driven by a single market factor (spot return)
    risk::Instrument equity{};
    equity.id = 0;
    equity.type = risk::InstrumentType::Equity;
    equity.qty = 100.0;
    equity.current_price = 200.0;
    equity.underlying_price = equity.current_price;
    equity.underlying_index = 0;
    portfolio.push_back(equity);

    // European call option hedged off the same underlying factor
    risk::Instrument option{};
    option.id = 1;
    option.type = risk::InstrumentType::Option;
    option.is_call = true;
    option.qty = 10.0;
    option.current_price = 15.0;  // observed option premium per contract
    option.underlying_price = 200.0; // spot of the underlying asset
    option.underlying_index = 0;     // map to the equity factor
    option.strike = 200.0;
    option.time_to_maturity = 0.5;
    option.implied_vol = 0.25;
    option.rate = 0.02;
    portfolio.push_back(option);

    const auto soa = risk::to_struct_of_arrays(portfolio);

    // Historical shocks: column 0 = underlying return, column 1 = vol shock
    const std::vector<std::vector<double>> shocks{
        {-0.015, -0.030},  // -1.5% return, vol down 3%
        {0.004, 0.015},    // +0.4% return, vol up 1.5%
        {-0.007, -0.012},  // -0.7% return, vol down 1.2%
        {0.011, 0.000}     // +1.1% return, flat vol
    };

    try {
        std::clog << "==================== HVaR ====================\n";
        const risk::RiskMetrics hist_metrics = risk::historical_var(soa, shocks, 0.99);
        std::cout << "Sample 99% one-day HVaR: $" << hist_metrics.var << '\n';
        std::cout << "Sample 99% one-day HCVaR: $" << hist_metrics.cvar << '\n';

        const std::vector<double> mu{0.0003};        // 0.03% daily drift
        const std::vector<double> cov{0.0001};       // (1% daily vol)^2

        const risk::MCParams mc_params{
            .paths = 20000,
            .seed = 123456789ULL,
            .use_cholesky = true,
            .threads = 0
        };

        std::clog << "==================== MCVaR ====================\n";
        const risk::RiskMetrics mc_metrics = risk::mc_var(soa, mu, cov, 1.0, 0.99, mc_params);
        std::cout << "Sample 99% one-day MCVaR: $" << mc_metrics.var << '\n';
        std::cout << "Sample 99% one-day MCVaR (ES): $" << mc_metrics.cvar << '\n';

        std::vector<risk::bs::BSGreeks> greeks_per_contract;
        std::vector<risk::bs::BSGreeks> greeks_position;
        risk::GreeksSummary totals;
        risk::compute_greeks(soa, greeks_per_contract, greeks_position, totals);

        constexpr double days_per_year = 252.0;
        auto theta_per_day = [&](double theta_year) { return theta_year / days_per_year; };
        auto vega_per_percent = [](double vega_one) { return vega_one / 100.0; };
        auto rho_per_percent = [](double rho_one) { return rho_one / 100.0; };

        std::cout << std::fixed << std::setprecision(4);
        std::cout << "==================== Greeks ====================\n";
        for (std::size_t i = 0; i < greeks_per_contract.size(); ++i) {
            const double qty = soa.qty[i];
            const bool is_option = soa.type[i] == static_cast<std::uint8_t>(risk::InstrumentType::Option);
            const char* label = is_option ? (soa.is_call[i] ? "Call" : "Put") : "Equity";

            const auto& pc = greeks_per_contract[i];
            const auto& pos = greeks_position[i];

            const double pc_theta_day = theta_per_day(pc.theta);
            const double pos_theta_day = theta_per_day(pos.theta);
            const double pc_vega_pct = vega_per_percent(pc.vega);
            const double pos_vega_pct = vega_per_percent(pos.vega);
            const double pc_rho_pct = rho_per_percent(pc.rho);
            const double pos_rho_pct = rho_per_percent(pos.rho);

            std::cout << "Instrument " << soa.id[i] << " (" << label << ")\n";
            std::cout << "  Price:    " << pc.price << " (per contract)\n";
            std::cout << "  Position: " << pos.price << " (" << qty << " units)\n";
            std::cout << "  Greeks per contract: ";
            std::cout << "Δ=" << pc.delta << " shares, "
                      << "Γ=" << pc.gamma << " 1/$^2, "
                      << "ν=" << pc_vega_pct << " $ per 1% vol, "
                      << "Θ=" << pc_theta_day << " $ per day, "
                      << "ρ=" << pc_rho_pct << " $ per 1% rate\n";
            std::cout << "  Greeks for position:   ";
            std::cout << "Δ=" << pos.delta << " shares, "
                      << "Γ=" << pos.gamma << " 1/$^2, "
                      << "ν=" << pos_vega_pct << " $ per 1% vol, "
                      << "Θ=" << pos_theta_day << " $ per day, "
                      << "ρ=" << pos_rho_pct << " $ per 1% rate\n";
        }

        const double portfolio_theta_day = theta_per_day(totals.theta);
        const double portfolio_vega_pct = vega_per_percent(totals.vega);
        const double portfolio_rho_pct = rho_per_percent(totals.rho);

        std::cout << "\nPortfolio totals\n";
        std::cout << "  Price: " << totals.price << "\n";
        std::cout << "  Δ: " << totals.delta << " shares\n";
        std::cout << "  Γ: " << totals.gamma << " 1/$^2\n";
        std::cout << "  ν: " << portfolio_vega_pct << " $ per 1% vol\n";
        std::cout << "  Θ: " << portfolio_theta_day << " $ per day\n";
        std::cout << "  ρ: " << portfolio_rho_pct << " $ per 1% rate\n";

        std::cout << "\nGreek   |";
        for (std::size_t i = 0; i < greeks_position.size(); ++i) {
            std::cout << " Instr" << i << " |";
        }
        std::cout << " Portfolio | Unit\n";

        auto print_row = [&](const char* name,
                             auto extractor,
                             const char* unit,
                             double portfolio_value) {
            std::cout << std::setw(7) << name << " |";
            for (std::size_t i = 0; i < greeks_position.size(); ++i) {
                std::cout << ' ' << std::setw(8) << extractor(greeks_position[i]) << " |";
            }
            std::cout << ' ' << std::setw(9) << portfolio_value
                      << " | " << unit << "\n";
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
    } catch (const std::exception& ex) {
        std::cerr << "Failed to compute VaR: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
