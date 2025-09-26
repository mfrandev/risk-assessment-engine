#pragma once

#include <limits>
#include <vector>

#include "risk/bs.hpp"
#include "risk/instrument_soa.hpp"

namespace risk {

struct GreeksSummary {
    double price = 0.0;  // portfolio value in dollars
    double delta = 0.0;  // shares
    double gamma = 0.0;  // per $^2
    double vega = 0.0;   // dollars per 1.00 volatility move
    double theta = 0.0;  // dollars per year (will scale to per day in reporting)
    double rho = 0.0;    // dollars per 1.00 rate move
};

void compute_greeks(const InstrumentSoA& instruments,
                    std::vector<bs::BSGreeks>& per_contract,
                    std::vector<bs::BSGreeks>& per_position,
                    GreeksSummary& totals,
                    double spot_override = std::numeric_limits<double>::quiet_NaN());

} // namespace risk
