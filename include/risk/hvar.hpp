#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <risk/instrument_soa.hpp>
#include <utility>

namespace risk {

struct RiskMetrics {
    double var;
    double cvar;
};

double revalue_portfolio(const InstrumentSoA& instruments,
                         std::span<const double> shocks,
                         std::size_t scenario_index);

RiskMetrics historical_var(const InstrumentSoA& instruments,
                           const std::vector<std::vector<double>>& shock_matrix,
                           double confidence);

} // namespace risk
