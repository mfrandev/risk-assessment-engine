#include <risk/hvar.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include <risk/bs.hpp>
#include <risk/instrument.hpp>
#include <risk/universe.hpp>
#include <risk/utils.hpp>

namespace risk {

namespace {

bool is_option(std::uint8_t type_flag) {
    return type_flag == static_cast<std::uint8_t>(InstrumentType::Option);
}

double clamp_positive(double value, double floor_value) {
    return value > floor_value ? value : floor_value;
}

} // namespace

double hvarday(const InstrumentSoA& soa, const double* shocks_row) {
    if (shocks_row == nullptr) {
        throw std::invalid_argument("shocks_row must not be null");
    }

    const std::size_t instrument_count = soa.size();
    double value_today = 0.0;
    double value_shocked = 0.0;

    for (std::size_t i = 0; i < instrument_count; ++i) {
        const double qty = soa.qty[i];
        const double price_today = soa.current_price[i];
        double price_shocked = price_today;
        double applied_shock = 0.0;

        if (is_option(soa.type[i])) {
            const std::uint32_t underlying_idx = soa.underlying_index[i];
            if (underlying_idx >= universe_size()) {
                throw std::out_of_range("underlying index exceeds shock dimension");
            }
            const double spot_shock = shocks_row[underlying_idx];
            const double underlying_today = soa.underlying_price[i] > 0.0
                                                ? soa.underlying_price[i]
                                                : price_today;
            const double underlying_shocked = underlying_today * (1.0 + spot_shock);
            const double sigma = clamp_positive(soa.implied_vol[i], 1e-8);
            const double time_to_maturity = std::max(soa.time_to_maturity[i], 0.0);

            price_shocked = bs::price(soa.is_call[i] != 0,
                                      underlying_shocked,
                                      soa.strike[i],
                                      soa.rate[i],
                                      sigma,
                                      time_to_maturity);
            applied_shock = spot_shock;
        } else {
            const std::uint32_t id = soa.id[i];
            if (id >= universe_size()) {
                throw std::out_of_range("equity id exceeds shock dimension");
            }
            applied_shock = shocks_row[id];
            price_shocked = price_today * (1.0 + applied_shock);
        }

        const double value_today_instrument = price_today * qty;
        const double value_shocked_instrument = price_shocked * qty;

        value_today += value_today_instrument;
        value_shocked += value_shocked_instrument;
    }

    return value_shocked - value_today;
}

RiskMetrics compute_hvar(const InstrumentSoA& soa,
                         const std::vector<double>& shocks_flat,
                         std::size_t Tm1,
                         std::size_t N,
                         double alpha) {
    if (Tm1 == 0) {
        throw std::invalid_argument("compute_hvar requires at least one scenario");
    }
    if (N == 0) {
        throw std::invalid_argument("compute_hvar requires positive factor dimension");
    }
    if (N != universe_size()) {
        throw std::invalid_argument("factor dimension must equal universe size");
    }
    if (shocks_flat.size() != Tm1 * N) {
        throw std::invalid_argument("shock matrix size mismatch in compute_hvar");
    }
    if (!(alpha > 0.0 && alpha < 1.0)) {
        throw std::invalid_argument("alpha must be in (0,1)");
    }

    std::vector<double> pnls(Tm1, 0.0);
    for (std::size_t t = 0; t < Tm1; ++t) {
        const double* row = shocks_flat.data() + t * N;
        pnls[t] = hvarday(soa, row);
    }

    std::vector<double> pnls_copy = pnls;
    const double q = std::clamp(1.0 - alpha, 0.0, 1.0);
    const double var_quantile = quantile_inplace(pnls_copy, q);

    double tail_sum = 0.0;
    std::size_t tail_count = 0;
    for (double pnl : pnls) {
        if (pnl <= var_quantile) {
            tail_sum += pnl;
            ++tail_count;
        }
    }
    if (tail_count == 0) {
        tail_sum += var_quantile;
        tail_count = 1;
    }

    RiskMetrics metrics;
    metrics.var = -var_quantile;
    metrics.cvar = -(tail_sum / static_cast<double>(tail_count));
    return metrics;
}

} // namespace risk
