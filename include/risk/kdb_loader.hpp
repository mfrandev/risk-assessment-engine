#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <risk/eigen_stub.hpp>

#include <risk/instrument_soa.hpp>

namespace risk::kdb {

struct MarketSnapshot {
    std::vector<std::string> dates;
    std::vector<std::string> tickers;
    std::vector<double> closes_flat; // row-major: dates × tickers
};

struct ShockSnapshot {
    std::vector<std::string> dates;
    std::vector<double> shocks_flat; // row-major: scenarios × tickers
};

MarketSnapshot load_market_data(int handle);
risk::InstrumentSoA load_portfolio_data(int handle, std::size_t universe_size);
ShockSnapshot load_shock_data(int handle, std::size_t expected_factors);
Eigen::VectorXd load_sample_mean(int handle, std::size_t expected_factors);
Eigen::MatrixXd load_sample_covariance(int handle, std::size_t expected_factors);

} // namespace risk::kdb
