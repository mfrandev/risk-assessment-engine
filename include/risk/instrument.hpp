#pragma once

#include <cstdint>

namespace risk {

enum class InstrumentType : std::uint8_t { Equity = 0, Option = 1 };

struct alignas(64) Instrument {
    std::uint32_t id = 0;
    InstrumentType type = InstrumentType::Equity;
    bool is_call = false;
    std::uint8_t pad8 = 0;
    double qty = 0.0;
    double current_price = 0.0;
    double underlying_price = 0.0;
    std::uint32_t underlying_index = 0;
    double strike = 0.0;
    double time_to_maturity = 0.0;
    double implied_vol = 0.0;
    double rate = 0.0;
};

} // namespace risk
