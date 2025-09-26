#include <risk/instrument_soa.hpp>

namespace risk {

void InstrumentSoA::reserve(std::size_t n) {
    id.reserve(n);
    type.reserve(n);
    is_call.reserve(n);
    qty.reserve(n);
    current_price.reserve(n);
    underlying_price.reserve(n);
    underlying_index.reserve(n);
    strike.reserve(n);
    time_to_maturity.reserve(n);
    implied_vol.reserve(n);
    rate.reserve(n);
}

std::size_t InstrumentSoA::size() const noexcept {
    return id.size();
}

InstrumentSoA to_struct_of_arrays(const std::vector<Instrument>& instruments) {
    InstrumentSoA soa;
    soa.reserve(instruments.size());

    for (const auto& inst : instruments) {
        soa.id.push_back(inst.id);
        soa.type.push_back(static_cast<std::uint8_t>(inst.type));
        soa.is_call.push_back(inst.is_call ? static_cast<std::uint8_t>(1) : static_cast<std::uint8_t>(0));
        soa.qty.push_back(inst.qty);
        soa.current_price.push_back(inst.current_price);
        soa.underlying_price.push_back(inst.underlying_price);
        soa.underlying_index.push_back(inst.underlying_index);
        soa.strike.push_back(inst.strike);
        soa.time_to_maturity.push_back(inst.time_to_maturity);
        soa.implied_vol.push_back(inst.implied_vol);
        soa.rate.push_back(inst.rate);
    }

    return soa;
}

} // namespace risk
