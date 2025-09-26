#pragma once

#include <cstdint>

#include <Eigen/Dense>

#include "risk/instrument_soa.hpp"

namespace risk {

struct MCParams {
    int paths = 10000;
    std::uint64_t seed = 42;
    bool use_cholesky = true;
    int threads = 0; // 0 => use hardware_concurrency
};

// Monte Carlo VaR using log-normal shocks on underlying factors.
double mc_var(const InstrumentSoA& instruments,
              const Eigen::VectorXd& mu,
              const Eigen::MatrixXd& cov,
              double horizon_days,
              double alpha,
              const MCParams& params);

} // namespace risk
