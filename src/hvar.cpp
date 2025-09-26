#include "risk/hvar.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "risk/bs.hpp"
#include "risk/utils.hpp"

namespace risk {

    namespace {
        constexpr double kMinConfidence = 1e-6;
        constexpr double kMaxConfidence = 1.0 - 1e-6;

        bool is_option(std::uint8_t type_flag) {
            return type_flag == static_cast<std::uint8_t>(InstrumentType::Option);
        }

    } // namespace

double revalue_portfolio(const InstrumentSoA& instruments,
                         std::span<const double> shocks,
                         std::size_t scenario_index) {
    const std::size_t n = instruments.size();
    if (shocks.size() != n) {
        throw std::invalid_argument("shock vector size does not match instrument count");
    }

    double value_today = 0.0;
    double value_shocked = 0.0;

    for (std::size_t i = 0; i < n; ++i) {
        const double qty = instruments.qty[i];
        const double price_today = instruments.current_price[i];
        double price_shocked = price_today * (1.0 + shocks[i]);
        double underlying_today_logged = 0.0;
        double underlying_shocked_logged = 0.0;
        double sigma_shocked_logged = instruments.implied_vol[i];
        double bs_price_shocked_logged = price_shocked;

        if (is_option(instruments.type[i])) {
            const std::uint32_t underlying_idx = instruments.underlying_index[i];
            if (underlying_idx >= n) {
                throw std::out_of_range("underlying index out of range for option instrument");
            }

            const double spot_shock = shocks[underlying_idx];
            const double vol_shock = shocks[i];

            const double underlying_today = instruments.underlying_price[i] > 0.0
                                                ? instruments.underlying_price[i]
                                                : price_today;
            const double underlying_shocked = underlying_today * (1.0 + spot_shock);
            const double sigma_today = instruments.implied_vol[i];
            const double sigma_shocked = std::max(sigma_today * (1.0 + vol_shock), 1e-8);

            price_shocked = bs::price(instruments.is_call[i] != 0,
                                      underlying_shocked,
                                      instruments.strike[i],
                                      instruments.rate[i],
                                      sigma_shocked,
                                      instruments.time_to_maturity[i]);

            underlying_today_logged = underlying_today;
            underlying_shocked_logged = underlying_shocked;
            sigma_shocked_logged = sigma_shocked;
            bs_price_shocked_logged = price_shocked;
        }

        const double value_today_instrument = price_today * qty;
        value_today += value_today_instrument;

        const double value_shocked_instrument = price_shocked * qty;
        value_shocked += value_shocked_instrument;

        const double instrument_pnl = value_shocked_instrument - value_today_instrument;
        const char* type_label = is_option(instruments.type[i]) ? "Option" : "Equity";
        std::clog << "Scenario " << scenario_index
                  << " | Instrument " << instruments.id[i]
                  << " (" << type_label << ")"
                  << " | Shock " << shocks[i]
                  << " | ValueToday " << value_today_instrument
                  << " | ValueShocked " << value_shocked_instrument
                  << " | PnL " << instrument_pnl;

        if (is_option(instruments.type[i])) {
            std::clog << " | UnderlyingToday " << underlying_today_logged
                      << " | UnderlyingShocked " << underlying_shocked_logged
                      << " | SigmaShocked " << sigma_shocked_logged
                      << " | BSPriceShocked " << bs_price_shocked_logged;
        }

        std::clog << '\n';
    }

    return value_shocked - value_today;
}

double historical_var(const InstrumentSoA& instruments,
                      const std::vector<std::vector<double>>& shock_matrix,
                      double confidence) {
        if (shock_matrix.empty()) {
            throw std::invalid_argument("shock matrix is empty");
        }

        if (!(confidence > kMinConfidence && confidence < kMaxConfidence)) {
            throw std::invalid_argument("confidence must be in (0,1)");
        }

        std::vector<double> pnls;
        pnls.reserve(shock_matrix.size());

    std::size_t scenario_index = 0;
    for (const auto& scenario : shock_matrix) {
        pnls.push_back(revalue_portfolio(instruments,
                                         std::span<const double>(scenario.data(), scenario.size()),
                                         scenario_index));
        ++scenario_index;
    }

        const double q = std::clamp(1.0 - confidence, 0.0, 1.0);
        const double var_quantile = quantile_inplace(pnls, q);
        return -var_quantile;
    }

} // namespace risk
