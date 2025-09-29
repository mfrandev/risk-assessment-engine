#pragma once

#include <cstddef>
#include <string>

#include <risk/instrument_soa.hpp>

namespace risk {

bool load_portfolio_csv(const std::string& path,
                        InstrumentSoA& soa,
                        std::size_t N);

} // namespace risk

