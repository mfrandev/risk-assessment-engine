#include <risk/portfolio.hpp>

#include <risk/instrument.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace risk {

namespace {

constexpr std::size_t kPortfolioColumns = 11;

const char* kPortfolioHeader[kPortfolioColumns] = {
    "id",
    "type",
    "is_call",
    "qty",
    "current_price",
    "underlying_price",
    "underlying_index",
    "strike",
    "time_to_maturity",
    "implied_vol",
    "rate"
};

std::string trim(std::string_view input) {
    const auto begin = input.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) {
        return std::string{};
    }
    const auto end = input.find_last_not_of(" \t\r\n");
    return std::string(input.substr(begin, end - begin + 1));
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    std::istringstream stream(line);
    while (std::getline(stream, field, ',')) {
        fields.emplace_back(trim(field));
    }
    if (!line.empty() && line.back() == ',') {
        fields.emplace_back();
    }
    return fields;
}

bool parse_uint32(const std::string& token,
                  bool required,
                  std::uint32_t default_value,
                  std::uint32_t& value_out) {
    if (token.empty()) {
        if (required) {
            return false;
        }
        value_out = default_value;
        return true;
    }
    try {
        size_t idx = 0;
        unsigned long raw = std::stoul(token, &idx, 10);
        if (idx != token.size() || raw > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
        value_out = static_cast<std::uint32_t>(raw);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parse_double(const std::string& token,
                  bool required,
                  double default_value,
                  double& value_out) {
    if (token.empty()) {
        if (required) {
            return false;
        }
        value_out = default_value;
        return true;
    }
    try {
        size_t idx = 0;
        double value = std::stod(token, &idx);
        if (idx != token.size() || !std::isfinite(value)) {
            return false;
        }
        value_out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void clear_soa(InstrumentSoA& soa) {
    soa.id.clear();
    soa.type.clear();
    soa.is_call.clear();
    soa.qty.clear();
    soa.current_price.clear();
    soa.underlying_price.clear();
    soa.underlying_index.clear();
    soa.strike.clear();
    soa.time_to_maturity.clear();
    soa.implied_vol.clear();
    soa.rate.clear();
}

} // namespace

bool load_portfolio_csv(const std::string& path,
                        InstrumentSoA& soa,
                        std::size_t N) {
    clear_soa(soa);

    std::ifstream input(path);
    if (!input.is_open()) {
        spdlog::error("Failed to open portfolio CSV: {}", path);
        return false;
    }

    std::string line;
    if (!std::getline(input, line)) {
        spdlog::error("Portfolio CSV missing header row");
        return false;
    }

    const auto header = split_csv_line(line);
    if (header.size() != kPortfolioColumns) {
        spdlog::error("Unexpected portfolio header column count");
        return false;
    }
    for (std::size_t i = 0; i < kPortfolioColumns; ++i) {
        if (header[i] != kPortfolioHeader[i]) {
            spdlog::error("Portfolio header mismatch at column {}", i);
            return false;
        }
    }

    std::size_t row_index = 1;
    while (std::getline(input, line)) {
        ++row_index;
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        const auto fields = split_csv_line(line);
        if (fields.size() != kPortfolioColumns) {
            spdlog::error("Unexpected field count in portfolio row {}", row_index);
            clear_soa(soa);
            return false;
        }

        std::uint32_t id = 0;
        if (!parse_uint32(fields[0], true, 0, id) || id >= N) {
            spdlog::error("Invalid id in portfolio row {}", row_index);
            clear_soa(soa);
            return false;
        }

        std::uint32_t type_raw = 0;
        if (!parse_uint32(fields[1], true, 0, type_raw) || type_raw > 1U) {
            spdlog::error("Invalid type in portfolio row {}", row_index);
            clear_soa(soa);
            return false;
        }
        const std::uint8_t type = static_cast<std::uint8_t>(type_raw);

        std::uint32_t is_call_raw = 0;
        const bool is_call_required = type == static_cast<std::uint8_t>(InstrumentType::Option);
        if (!parse_uint32(fields[2], is_call_required, 0, is_call_raw) || is_call_raw > 1U) {
            spdlog::error("Invalid is_call in portfolio row {}", row_index);
            clear_soa(soa);
            return false;
        }
        const std::uint8_t is_call = static_cast<std::uint8_t>(is_call_raw);

        double qty = 0.0;
        if (!parse_double(fields[3], true, 0.0, qty)) {
            spdlog::error("Invalid qty in portfolio row {}", row_index);
            clear_soa(soa);
            return false;
        }

        double current_price = 0.0;
        if (!parse_double(fields[4], true, 0.0, current_price) || current_price <= 0.0) {
            spdlog::error("Invalid current_price in portfolio row {}", row_index);
            clear_soa(soa);
            return false;
        }

        double underlying_price = 0.0;
        const bool underlying_price_required = type == static_cast<std::uint8_t>(InstrumentType::Option);
        const double underlying_default = (underlying_price_required ? current_price : current_price);
        if (!parse_double(fields[5], underlying_price_required, underlying_default, underlying_price) ||
            underlying_price <= 0.0) {
            spdlog::error("Invalid underlying_price in portfolio row {}", row_index);
            clear_soa(soa);
            return false;
        }

        std::uint32_t underlying_index = id;
        const bool underlying_index_required = type == static_cast<std::uint8_t>(InstrumentType::Option);
        if (!parse_uint32(fields[6], underlying_index_required, id, underlying_index)) {
            spdlog::error("Invalid underlying_index in portfolio row {}", row_index);
            clear_soa(soa);
            return false;
        }
        if (type == static_cast<std::uint8_t>(InstrumentType::Equity) && underlying_index != id) {
            spdlog::error("Equity underlying_index must equal id (row {})", row_index);
            clear_soa(soa);
            return false;
        }
        if (underlying_index >= N) {
            spdlog::error("Underlying index out of bounds in portfolio row {}", row_index);
            clear_soa(soa);
            return false;
        }

        double strike = 0.0;
        if (type == static_cast<std::uint8_t>(InstrumentType::Option)) {
            if (!parse_double(fields[7], true, 0.0, strike) || strike <= 0.0) {
                spdlog::error("Invalid strike in portfolio row {}", row_index);
                clear_soa(soa);
                return false;
            }
        }

        double time_to_maturity = 0.0;
        if (!parse_double(fields[8], type == static_cast<std::uint8_t>(InstrumentType::Option), 0.0, time_to_maturity)) {
            spdlog::error("Invalid time_to_maturity in portfolio row {}", row_index);
            clear_soa(soa);
            return false;
        }
        time_to_maturity = std::max(time_to_maturity, 0.0);

        double implied_vol = 0.0;
        if (!parse_double(fields[9], type == static_cast<std::uint8_t>(InstrumentType::Option), 0.0, implied_vol)) {
            spdlog::error("Invalid implied_vol in portfolio row {}", row_index);
            clear_soa(soa);
            return false;
        }
        if (type == static_cast<std::uint8_t>(InstrumentType::Option)) {
            implied_vol = std::max(implied_vol, 1e-8);
        } else {
            implied_vol = 0.0;
        }

        double rate = 0.0;
        if (!parse_double(fields[10], false, 0.0, rate)) {
            spdlog::error("Invalid rate in portfolio row {}", row_index);
            clear_soa(soa);
            return false;
        }

        if (type == static_cast<std::uint8_t>(InstrumentType::Equity)) {
            soa.id.push_back(id);
            soa.type.push_back(type);
            soa.is_call.push_back(0);
            soa.qty.push_back(qty);
            soa.current_price.push_back(current_price);
            soa.underlying_price.push_back(current_price);
            soa.underlying_index.push_back(id);
            soa.strike.push_back(0.0);
            soa.time_to_maturity.push_back(0.0);
            soa.implied_vol.push_back(0.0);
            soa.rate.push_back(0.0);
        } else {
            soa.id.push_back(id);
            soa.type.push_back(type);
            soa.is_call.push_back(is_call);
            soa.qty.push_back(qty);
            soa.current_price.push_back(current_price);
            soa.underlying_price.push_back(underlying_price);
            soa.underlying_index.push_back(underlying_index);
            soa.strike.push_back(strike);
            soa.time_to_maturity.push_back(time_to_maturity);
            soa.implied_vol.push_back(implied_vol);
            soa.rate.push_back(rate);
        }
    }

    return true;
}

} // namespace risk
