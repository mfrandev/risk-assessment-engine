#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <risk/eigen_stub.hpp>

#include <risk/instrument_soa.hpp>

namespace risk {

struct InputOptions {
    std::string portfolio_path;
    std::string market_path;
#ifdef ENABLE_KDB
    bool connect_to_kdb = false;
    std::string kdb_host = "localhost";
    int kdb_port = 5000;
    std::string kdb_credentials;
#endif
};

// Everything the risk calculations consume, regardless of where it was loaded from.
struct EngineInputs {
    InstrumentSoA portfolio;
    std::vector<double> shocks_flat;  // row-major: scenarios x factors
    std::size_t scenarios = 0;
    std::size_t factors = 0;
    Eigen::VectorXd mu;               // sample mean of the shocks
    Eigen::MatrixXd cov;              // sample covariance of the shocks
};

// Loads from KDB+ when requested (falling back to CSV on failure), otherwise from CSV.
// Returns std::nullopt after logging the reason when the inputs are unusable.
std::optional<EngineInputs> load_inputs(const InputOptions& options);

} // namespace risk
