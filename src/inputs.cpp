#include <risk/inputs.hpp>

#include <risk/instrument.hpp>
#ifdef ENABLE_KDB
#include <risk/kdb_connection.hpp>
#include <risk/kdb_loader.hpp>
#endif
#include <risk/market.hpp>
#include <risk/portfolio.hpp>
#include <risk/stats.hpp>
#include <risk/universe.hpp>

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <utility>

namespace risk {

namespace {

std::optional<EngineInputs> load_from_csv(const InputOptions& options) {
    std::vector<std::string> dates;
    std::vector<double> prices_flat;
    std::size_t T = 0;
    std::size_t N = 0;

    if (!load_closes_csv(options.market_path, dates, prices_flat, T, N)) {
        return std::nullopt;
    }
    if (N != universe_size()) {
        spdlog::error("Loaded universe size does not match expected universe");
        return std::nullopt;
    }
    if (T < 2) {
        spdlog::error("Need at least two rows of market data to compute shocks");
        return std::nullopt;
    }
    spdlog::debug("Loaded market data from '{}' with {} rows and {} tickers.", options.market_path, T, N);

    EngineInputs inputs;
    inputs.factors = N;
    inputs.scenarios = T - 1;
    compute_shocks(prices_flat, T, N, inputs.shocks_flat);

    if (!load_portfolio_csv(options.portfolio_path, inputs.portfolio, N)) {
        return std::nullopt;
    }
    if (inputs.portfolio.size() == 0U) {
        spdlog::error("Portfolio CSV produced no instruments");
        return std::nullopt;
    }

    inputs.mu = compute_sample_mean(inputs.shocks_flat, inputs.scenarios, N);
    inputs.cov = compute_sample_covariance(inputs.shocks_flat, inputs.mu, inputs.scenarios, N);
    return inputs;
}

#ifdef ENABLE_KDB
// Throws on any failure so the caller can fall back to CSV.
EngineInputs load_from_kdb(int handle) {
    auto market_snapshot = kdb::load_market_data(handle);
    const std::size_t N = market_snapshot.tickers.size();
    const std::size_t T = market_snapshot.dates.size();
    if (universe_size() != N) {
        throw std::runtime_error("Universe size mismatch after loading market data from KDB+");
    }
    if (T < 2) {
        throw std::runtime_error("KDB+ market data requires at least two rows");
    }

    EngineInputs inputs;
    inputs.factors = N;
    inputs.portfolio = kdb::load_portfolio_data(handle, N);

    auto shock_snapshot = kdb::load_shock_data(handle, N);
    inputs.shocks_flat = std::move(shock_snapshot.shocks_flat);
    if (inputs.shocks_flat.empty()) {
        throw std::runtime_error("KDB+ shock data is empty");
    }
    inputs.scenarios = inputs.shocks_flat.size() / N;
    if (inputs.scenarios == 0 || inputs.shocks_flat.size() != inputs.scenarios * N) {
        throw std::runtime_error("KDB+ shock matrix has inconsistent dimensions");
    }

    inputs.mu = kdb::load_sample_mean(handle, N);
    inputs.cov = kdb::load_sample_covariance(handle, N);

    spdlog::info("Loaded market, portfolio, and precomputed statistics from KDB+.");
    spdlog::debug("Loaded market data from KDB+ with {} rows and {} tickers.", T, N);
    return inputs;
}

std::optional<EngineInputs> try_load_from_kdb(const InputOptions& options) {
    kdb::Connection connection(options.kdb_host, options.kdb_port, options.kdb_credentials);
    spdlog::info("Connected to KDB+ at {}:{}.", options.kdb_host, options.kdb_port);
    if (!connection.is_connected()) {
        return std::nullopt;
    }
    try {
        return load_from_kdb(connection.handle());
    } catch (const std::exception& ex) {
        spdlog::warn("KDB+ load failed: {}. Falling back to CSV inputs.", ex.what());
        return std::nullopt;
    }
}
#endif

bool is_consistent(const EngineInputs& inputs) {
    if (inputs.factors == 0) {
        spdlog::error("Universe contains no tickers");
        return false;
    }
    if (inputs.scenarios == 0 || inputs.shocks_flat.size() != inputs.scenarios * inputs.factors) {
        spdlog::error("Shock data has inconsistent dimensions");
        return false;
    }
    if (inputs.portfolio.size() == 0U) {
        spdlog::error("Portfolio data is empty.");
        return false;
    }
    return true;
}

} // namespace

std::optional<EngineInputs> load_inputs(const InputOptions& options) {
    std::optional<EngineInputs> inputs;
    std::string origin = options.portfolio_path;

#ifdef ENABLE_KDB
    if (options.connect_to_kdb) {
        inputs = try_load_from_kdb(options);
        if (inputs) {
            origin = "KDB+";
        }
    } else {
        spdlog::info("Skipping KDB+ connection (use --connect-kdb to enable).");
    }
#else
    spdlog::info("KDB+ support is disabled; using CSV inputs.");
#endif

    if (!inputs) {
        inputs = load_from_csv(options);
    }
    if (!inputs || !is_consistent(*inputs)) {
        return std::nullopt;
    }

    spdlog::info("Loaded portfolio from '{}' with {} instruments.", origin, inputs->portfolio.size());
    return inputs;
}

} // namespace risk
