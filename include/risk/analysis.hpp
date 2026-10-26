#pragma once

#include <vector>

#include <risk/bs.hpp>
#include <risk/greeks.hpp>
#include <risk/hvar.hpp>
#include <risk/inputs.hpp>

namespace risk {

struct RiskResults {
    std::vector<bs::BSGreeks> position_greeks;  // one per instrument, quantity-scaled
    GreeksSummary totals;
    RiskMetrics historical;
    RiskMetrics monte_carlo;
};

// Runs historical VaR, Monte Carlo VaR and Black-Scholes greeks on the loaded inputs.
RiskResults analyze(const EngineInputs& inputs);

} // namespace risk
