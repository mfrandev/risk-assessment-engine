#include <risk/utils.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace risk {

double quantile_inplace(std::vector<double>& data, double q) {
    if (data.empty()) {
        throw std::invalid_argument("quantile_inplace requires non-empty data");
    }

    if (!std::isfinite(q)) {
        throw std::invalid_argument("quantile_inplace requires finite q");
    }

    q = std::clamp(q, 0.0, 1.0);
    const std::size_t n = data.size();

    if (n == 1) {
        return data.front();
    }

    const double rank = q * static_cast<double>(n - 1);
    const std::size_t idx = static_cast<std::size_t>(std::floor(rank));

    auto nth = data.begin() + static_cast<std::ptrdiff_t>(idx);
    std::nth_element(data.begin(), nth, data.end());
    return *nth;
}

} // namespace risk
