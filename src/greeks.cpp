#include "risk/greeks.hpp"

#include <cmath>
#include <limits>

namespace risk {

void compute_greeks(const InstrumentSoA& instruments,
                    std::vector<bs::BSGreeks>& per_contract,
                    std::vector<bs::BSGreeks>& per_position,
                    GreeksSummary& totals,
                    double spot_override) {
    const std::size_t n = instruments.size();
    per_contract.resize(n);
    per_position.resize(n);

    totals = GreeksSummary{};

    for (std::size_t i = 0; i < n; ++i) {
        const double qty = instruments.qty[i];
        bs::BSGreeks g{};

        if (instruments.type[i] == static_cast<std::uint8_t>(InstrumentType::Option)) {
            const double spot = std::isnan(spot_override)
                                     ? (instruments.underlying_price[i] > 0.0 ? instruments.underlying_price[i]
                                                                            : instruments.current_price[i])
                                     : spot_override;
            if (instruments.is_call[i] != 0) {
                g = bs::call(spot,
                             instruments.strike[i],
                             instruments.rate[i],
                             instruments.implied_vol[i],
                             instruments.time_to_maturity[i]);
            } else {
                g = bs::put(spot,
                            instruments.strike[i],
                            instruments.rate[i],
                            instruments.implied_vol[i],
                            instruments.time_to_maturity[i]);
            }
        } else { // Equity treated as delta-one
            g.price = instruments.current_price[i];
            g.delta = 1.0;
            g.gamma = 0.0;
            g.vega = 0.0;
            g.theta = 0.0;
            g.rho = 0.0;
        }

        per_contract[i] = g;

        bs::BSGreeks pos = g;
        pos.price *= qty;
        pos.delta *= qty;
        pos.gamma *= qty;
        pos.vega *= qty;
        pos.theta *= qty;
        pos.rho *= qty;

        totals.price += pos.price;
        totals.delta += pos.delta;
        totals.gamma += pos.gamma;
        totals.vega += pos.vega;
        totals.theta += pos.theta;
        totals.rho += pos.rho;

        per_position[i] = pos;
    }
}

} // namespace risk
