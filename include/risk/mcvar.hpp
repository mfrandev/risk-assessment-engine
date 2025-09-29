#pragma once

#include <cstdint>

#include <risk/eigen_stub.hpp>

#include <risk/hvar.hpp>
#include <risk/instrument_soa.hpp>

namespace risk {

RiskMetrics compute_mcvar(const InstrumentSoA& soa,
                          const Eigen::VectorXd& mu,
                          const Eigen::MatrixXd& cov,
                          double horizon_days,
                          double alpha,
                          int paths,
                          std::uint64_t seed);

} // namespace risk
