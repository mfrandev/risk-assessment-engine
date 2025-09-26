#pragma once

#include <cstdint>
#include <span>

#include "risk/hvar.hpp"
#include "risk/instrument_soa.hpp"

namespace risk {

struct MCParams {
    int paths = 10000;
    std::uint64_t seed = 42;
    bool use_cholesky = true;
    int threads = 0; // 0 => use hardware_concurrency
};

// Monte Carlo VaR/CVaR using log-normal shocks on underlying factors.
// `mu` is the per-factor drift vector, `cov` is a row-major covariance matrix (dim x dim).
RiskMetrics mc_var(const InstrumentSoA& instruments,
                   std::span<const double> mu,
                   std::span<const double> cov,
                   double horizon_days,
                   double alpha,
                   const MCParams& params);

} // namespace risk
