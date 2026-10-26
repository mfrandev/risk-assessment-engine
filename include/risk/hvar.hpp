#pragma once

#include <cstddef>
#include <vector>

#include <risk/instrument_soa.hpp>

namespace risk {

struct RiskMetrics {
    double var = 0.0;
    double cvar = 0.0;
};

// VaR and expected shortfall (both reported as positive losses) from a P&L sample.
RiskMetrics metrics_from_pnls(const std::vector<double>& pnls, double alpha);

double hvarday(const InstrumentSoA& soa, const double* shocks_row);

RiskMetrics compute_hvar(const InstrumentSoA& soa,
                         const std::vector<double>& shocks_flat,
                         std::size_t Tm1,
                         std::size_t N,
                         double alpha);

} // namespace risk
