#pragma once

#include <vector>

namespace risk {

double quantile_inplace(std::vector<double>& data, double q);

} // namespace risk
