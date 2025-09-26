#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <risk/instrument.hpp>

namespace risk {

struct InstrumentSoA {
    std::vector<std::uint32_t> id;
    std::vector<std::uint8_t> type;      // 0 = equity, 1 = option
    std::vector<std::uint8_t> is_call;   // 0 = put, 1 = call (options only)

    std::vector<double> qty;
    std::vector<double> current_price;
    std::vector<double> underlying_price;
    std::vector<std::uint32_t> underlying_index;
    std::vector<double> strike;
    std::vector<double> time_to_maturity;
    std::vector<double> implied_vol;
    std::vector<double> rate;

    void reserve(std::size_t n);
    [[nodiscard]] std::size_t size() const noexcept;
};

InstrumentSoA to_struct_of_arrays(const std::vector<Instrument>& instruments);

} // namespace risk
