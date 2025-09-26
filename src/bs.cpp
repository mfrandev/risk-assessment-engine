#include "risk/bs.hpp"

#include <algorithm>
#include <cmath>

namespace risk {

namespace bs {

namespace {
constexpr double kMinTime = 1e-8;
constexpr double kMinVol = 1e-8;
}

double normal_cdf(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double normal_pdf(double x) {
    static constexpr double inv_sqrt_2pi = 0.39894228040143267794;
    return inv_sqrt_2pi * std::exp(-0.5 * x * x);
}

double safe_time_to_maturity(double time_to_maturity) {
    return std::max(time_to_maturity, kMinTime);
}

double intrinsic(bool is_call, double spot, double strike) {
    if (is_call) {
        return std::max(0.0, spot - strike);
    }
    return std::max(0.0, strike - spot);
}

double price(bool is_call,
             double spot,
             double strike,
             double rate,
             double volatility,
             double time_to_maturity) {
    if (spot <= 0.0 || strike <= 0.0) {
        return 0.0;
    }

    const double tau = safe_time_to_maturity(time_to_maturity);
    const double vol = std::max(volatility, kMinVol);

    const double sqrt_tau = std::sqrt(tau);
    const double d1 = (std::log(spot / strike) + (rate + 0.5 * vol * vol) * tau) / (vol * sqrt_tau);
    const double d2 = d1 - vol * sqrt_tau;

    const double disc = std::exp(-rate * tau);

    if (is_call) {
        return spot * normal_cdf(d1) - strike * disc * normal_cdf(d2);
    }
    return strike * disc * normal_cdf(-d2) - spot * normal_cdf(-d1);
}

namespace {

BSGreeks finish_greeks(bool is_call,
                       double spot,
                       double strike,
                       double rate,
                       double volatility,
                       double time_to_maturity) {
    const double tau = safe_time_to_maturity(time_to_maturity);
    const double vol = std::max(volatility, kMinVol);
    const double sqrt_tau = std::sqrt(tau);

    if (tau <= kMinTime || vol <= kMinVol) {
        const double price_intrinsic = intrinsic(is_call, spot, strike);
        const double delta_limit = is_call ? (spot > strike ? 1.0 : 0.0)
                                           : (spot < strike ? -1.0 : 0.0);
        return BSGreeks{
            .price = price_intrinsic,
            .delta = delta_limit,
            .gamma = 0.0,
            .vega = 0.0,
            .theta = 0.0,
            .rho = 0.0};
    }

    const double d1 = (std::log(spot / strike) + (rate + 0.5 * vol * vol) * tau) / (vol * sqrt_tau);
    const double d2 = d1 - vol * sqrt_tau;
    const double nd1 = normal_cdf(is_call ? d1 : -d1);
    const double nd2 = normal_cdf(is_call ? d2 : -d2);
    const double pdf_d1 = normal_pdf(d1);
    const double disc = std::exp(-rate * tau);

    BSGreeks g{};
    if (is_call) {
        g.price = spot * nd1 - strike * disc * nd2;
        g.delta = nd1;
        g.theta = -(spot * pdf_d1 * vol) / (2.0 * sqrt_tau) - rate * strike * disc * nd2;
        g.rho = strike * tau * disc * nd2;
    } else {
        const double nd1_put = normal_cdf(-d1);
        const double nd2_put = normal_cdf(-d2);
        g.price = strike * disc * nd2_put - spot * nd1_put;
        g.delta = nd1_put - 1.0;
        g.theta = -(spot * pdf_d1 * vol) / (2.0 * sqrt_tau) + rate * strike * disc * nd2_put;
        g.rho = -strike * tau * disc * nd2_put;
    }

    g.gamma = pdf_d1 / (spot * vol * sqrt_tau);
    g.vega = spot * pdf_d1 * sqrt_tau;

    return g;
}

} // namespace

BSGreeks call(double spot,
              double strike,
              double rate,
              double volatility,
              double time_to_maturity) {
    return finish_greeks(true, spot, strike, rate, volatility, time_to_maturity);
}

BSGreeks put(double spot,
             double strike,
             double rate,
             double volatility,
             double time_to_maturity) {
    return finish_greeks(false, spot, strike, rate, volatility, time_to_maturity);
}

} // namespace bs

} // namespace risk
