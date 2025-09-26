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

} // namespace bs

} // namespace risk
