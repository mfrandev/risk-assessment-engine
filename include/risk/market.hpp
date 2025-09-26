#pragma once

#include <cstdint>

namespace risk {

struct alignas(64) MarketPoint {
    std::uint32_t id = 0;
    double close = 0.0;
};

} // namespace risk
