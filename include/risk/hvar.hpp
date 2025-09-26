#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <risk/instrument_soa.hpp>

namespace risk {

double revalue_portfolio(const InstrumentSoA& instruments,
                         std::span<const double> shocks,
                         std::size_t scenario_index);

double historical_var(const InstrumentSoA& instruments,
                      const std::vector<std::vector<double>>& shock_matrix,
                      double confidence);

} // namespace risk
