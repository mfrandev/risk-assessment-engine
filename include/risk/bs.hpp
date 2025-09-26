#pragma once

#include <cmath>

namespace risk {

namespace bs {

double normal_cdf(double x);
double price(bool is_call,
             double spot,
             double strike,
             double rate,
             double volatility,
             double time_to_maturity);

double intrinsic(bool is_call, double spot, double strike);

double safe_time_to_maturity(double time_to_maturity);

} // namespace bs

} // namespace risk
