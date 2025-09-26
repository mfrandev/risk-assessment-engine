#pragma once

#include <cmath>

namespace risk {

namespace bs {

struct BSGreeks {
    double price = 0.0;
    double delta = 0.0;
    double gamma = 0.0;
    double vega = 0.0;
    double theta = 0.0;
    double rho = 0.0;
};

double normal_cdf(double x);
double normal_pdf(double x);
double price(bool is_call,
             double spot,
             double strike,
             double rate,
             double volatility,
             double time_to_maturity);

BSGreeks call(double spot,
              double strike,
              double rate,
              double volatility,
              double time_to_maturity);

BSGreeks put(double spot,
             double strike,
             double rate,
             double volatility,
             double time_to_maturity);

double intrinsic(bool is_call, double spot, double strike);

double safe_time_to_maturity(double time_to_maturity);

} // namespace bs

} // namespace risk
