#include <risk/market.hpp>

#include <risk/universe.hpp>

#include <spdlog/spdlog.h>

#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace risk {

namespace {

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

bool parse_double(const std::string& token, double& value) {
    if (token.empty()) {
        return false;
    }
    try {
        size_t idx = 0;
        value = std::stod(token, &idx);
        if (idx != token.size() || !std::isfinite(value)) {
            return false;
        }
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

} // namespace

bool load_closes_csv(const std::string& path,
                     std::vector<std::string>& dates,
                     std::vector<double>& prices_flat,
                     std::size_t& T,
                     std::size_t& N) {
    dates.clear();
    prices_flat.clear();
    T = 0;
    N = 0;

    std::ifstream input(path);
    if (!input.is_open()) {
        spdlog::error("Failed to open closes CSV: {}", path);
        return false;
    }

    std::string line;
    if (!std::getline(input, line)) {
        spdlog::error("Closes CSV missing header row");
        return false;
    }

    const auto header = split_csv_line(line);
    if (header.size() < 2) {
        spdlog::error("Unexpected column count in closes header");
        return false;
    }
    if (header.front() != "date") {
        spdlog::error("First header column must be 'date'");
        return false;
    }

    std::vector<std::string> tickers;
    tickers.reserve(header.size() - 1);
    for (std::size_t i = 1; i < header.size(); ++i) {
        if (header[i].empty()) {
            spdlog::error("Empty ticker symbol at header column {}", i);
            return false;
        }
        tickers.push_back(header[i]);
    }

    risk::set_universe(tickers);
    N = tickers.size();

    std::vector<double> row(N, 0.0);
    while (std::getline(input, line)) {
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }
        const auto fields = split_csv_line(line);
        if (fields.size() != N + 1) {
            spdlog::error("Unexpected field count in closes row");
            return false;
        }

        dates.emplace_back(fields.front());
        for (std::size_t i = 0; i < N; ++i) {
            double value = 0.0;
            if (!parse_double(fields[i + 1], value) || value <= 0.0) {
                spdlog::error("Invalid close for ticker '{}'", tickers[i]);
                return false;
            }
            row[i] = value;
        }
        prices_flat.insert(prices_flat.end(), row.begin(), row.end());
    }

    T = dates.size();
    if (T == 0) {
        spdlog::error("No data rows found in closes CSV");
        return false;
    }

    return true;
}

void compute_shocks(const std::vector<double>& prices_flat,
                    std::size_t T,
                    std::size_t N,
                    std::vector<double>& shocks_flat) {
    if (N == 0) {
        throw std::invalid_argument("compute_shocks requires positive dimension");
    }
    if (T < 2) {
        throw std::invalid_argument("compute_shocks requires at least two observations");
    }
    if (prices_flat.size() != T * N) {
        throw std::invalid_argument("price matrix size mismatch");
    }

    shocks_flat.assign((T - 1) * N, 0.0);
    for (std::size_t t = 1; t < T; ++t) {
        const std::size_t prev_offset = (t - 1) * N;
        const std::size_t curr_offset = t * N;
        const std::size_t shock_offset = (t - 1) * N;
        for (std::size_t i = 0; i < N; ++i) {
            const double base = prices_flat[prev_offset + i];
            const double current = prices_flat[curr_offset + i];
            if (base <= 0.0) {
                throw std::invalid_argument("encountered non-positive base price while computing shocks");
            }
            shocks_flat[shock_offset + i] = (current / base) - 1.0;
        }
    }
}

} // namespace risk
