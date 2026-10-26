#include <risk/analysis.hpp>

#include <risk/config.hpp>
#include <risk/mcvar.hpp>

namespace risk {

RiskResults analyze(const EngineInputs& inputs) {
    RiskResults results;

    results.historical = compute_hvar(inputs.portfolio,
                                      inputs.shocks_flat,
                                      inputs.scenarios,
                                      inputs.factors,
                                      kVarConfidence);

    results.monte_carlo = compute_mcvar(inputs.portfolio,
                                        inputs.mu,
                                        inputs.cov,
                                        kVarHorizonDays,
                                        kVarConfidence,
                                        kMonteCarloPaths,
                                        kMonteCarloSeed);

    std::vector<bs::BSGreeks> per_contract;
    compute_greeks(inputs.portfolio, per_contract, results.position_greeks, results.totals);

    return results;
}

} // namespace risk
