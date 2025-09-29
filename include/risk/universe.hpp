#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace risk {

void set_universe(std::vector<std::string> symbols);

const std::vector<std::string>& universe_symbols();

std::size_t universe_size();

std::optional<std::size_t> ticker_to_id(std::string_view ticker);

} // namespace risk
