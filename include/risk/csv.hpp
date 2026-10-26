#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace risk {

// Strips leading and trailing spaces, tabs, CR and LF.
std::string trim(std::string_view input);

// Splits on commas and trims each field. A trailing comma yields a final empty field.
std::vector<std::string> split_csv_line(const std::string& line);

} // namespace risk
