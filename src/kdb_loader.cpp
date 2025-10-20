#include <risk/kdb_loader.hpp>

#include <risk/instrument.hpp>
#include <risk/universe.hpp>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <type_traits>

#define KXVER 3
extern "C" {
#include "k.h"
}

namespace risk::kdb {
namespace {

[[nodiscard]] std::string format_date(I days_since_2000) {
    using namespace std::chrono;
    static constexpr sys_days base = sys_days{year{2000} / 1 / 1};
    const sys_days date = base + days{days_since_2000};
    const year_month_day ymd{date};
    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << static_cast<int>(ymd.year()) << "-"
        << std::setw(2) << static_cast<unsigned>(ymd.month()) << "-"
        << std::setw(2) << static_cast<unsigned>(ymd.day());
    return oss.str();
}

[[nodiscard]] K checked_call(int handle, std::string_view expression) {
    if (handle <= 0) {
        throw std::runtime_error("Invalid KDB+ handle");
    }
    K result = k(handle, const_cast<S>(expression.data()), static_cast<K>(nullptr));
    if (!result) {
        throw std::runtime_error("Failed to execute '" + std::string(expression) + "' on KDB+");
    }
    if (result->t == -128) {
        const char* raw = result->s;
        std::string message = raw ? std::string(raw) : "unknown KDB+ execution error";
        r0(result);
        throw std::runtime_error(message);
    }
    return result;
}

[[nodiscard]] std::vector<std::string> extract_column_names(K table) {
    if (!table || table->t != 98) {
        throw std::runtime_error("Expected KDB+ table");
    }
    K dict = table->k;
    if (!dict || dict->t != 99) {
        throw std::runtime_error("Malformed KDB+ table (dictionary expected)");
    }
    K keys = kK(dict)[0];
    if (!keys || keys->t != 11) {
        throw std::runtime_error("Malformed KDB+ table (column names expected)");
    }
    std::vector<std::string> names;
    names.reserve(keys->n);
    for (J i = 0; i < keys->n; ++i) {
        names.emplace_back(kS(keys)[i]);
    }
    return names;
}

[[nodiscard]] K column_data(K table, std::size_t index) {
    if (!table || table->t != 98) {
        throw std::runtime_error("Expected KDB+ table");
    }
    K dict = table->k;
    if (!dict || dict->t != 99) {
        throw std::runtime_error("Malformed KDB+ table (dictionary expected)");
    }
    K values = kK(dict)[1];
    if (!values || values->t != 0) {
        throw std::runtime_error("Malformed KDB+ table (column data expected)");
    }
    if (index >= static_cast<std::size_t>(values->n)) {
        throw std::runtime_error("Column index out of bounds");
    }
    return kK(values)[index];
}

void validate_column_count(const std::vector<std::string>& names, std::size_t expected) {
    if (names.size() != expected) {
        throw std::runtime_error("Unexpected column count (" + std::to_string(names.size()) + ")");
    }
}

void enforce_condition(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

MarketSnapshot load_market_data(int handle) {
    MarketSnapshot snapshot;
    K table = checked_call(handle, "getMarketData[]");
    auto guard = std::unique_ptr<std::remove_pointer_t<K>, decltype(&r0)>(table, &r0);

    if (table->t != 98) {
        throw std::runtime_error("getMarketData did not return a table");
    }

    const std::vector<std::string> names = extract_column_names(table);
    enforce_condition(!names.empty(), "Market data has no columns");
    enforce_condition(names.front() == "date", "First market column must be `date`");

    const std::size_t ticker_count = names.size() - 1;
    enforce_condition(ticker_count > 0, "Market data has no ticker columns");

    K date_col = column_data(table, 0);
    enforce_condition(date_col->t == 14, "Market `date` column must be type date");

    snapshot.dates.resize(date_col->n);
    for (J row = 0; row < date_col->n; ++row) {
        snapshot.dates[static_cast<std::size_t>(row)] = format_date(kI(date_col)[row]);
    }

    snapshot.tickers.reserve(ticker_count);
    snapshot.closes_flat.assign(snapshot.dates.size() * ticker_count, 0.0);
    for (std::size_t col = 0; col < ticker_count; ++col) {
        const std::string ticker = names[col + 1];
        snapshot.tickers.push_back(ticker);

        K col_data = column_data(table, col + 1);
        enforce_condition(col_data->t == 9, "Market column '" + ticker + "' must be float");
        enforce_condition(static_cast<std::size_t>(col_data->n) == snapshot.dates.size(),
                          "Market column '" + ticker + "' row count mismatch");

        const double* values = kF(col_data);
        for (std::size_t row = 0; row < snapshot.dates.size(); ++row) {
            snapshot.closes_flat[row * ticker_count + col] = values[row];
        }
    }

    set_universe(snapshot.tickers);
    return snapshot;
}

risk::InstrumentSoA load_portfolio_data(int handle, std::size_t universe_size) {
    K table = checked_call(handle, "getPortfolioData[]");
    auto guard = std::unique_ptr<std::remove_pointer_t<K>, decltype(&r0)>(table, &r0);

    if (table->t != 98) {
        throw std::runtime_error("getPortfolioData did not return a table");
    }

    const std::vector<std::string> names = extract_column_names(table);
    validate_column_count(names, 11);
    static const char* expected[] = {
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
        "rate"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        enforce_condition(names[i] == expected[i],
                          "Portfolio column mismatch at index " + std::to_string(i));
    }

    K id_col = column_data(table, 0);
    K type_col = column_data(table, 1);
    K call_col = column_data(table, 2);
    K qty_col = column_data(table, 3);
    K current_col = column_data(table, 4);
    K underlying_price_col = column_data(table, 5);
    K underlying_index_col = column_data(table, 6);
    K strike_col = column_data(table, 7);
    K ttm_col = column_data(table, 8);
    K iv_col = column_data(table, 9);
    K rate_col = column_data(table, 10);

    enforce_condition(id_col->t == 6, "`id` must be int");
    enforce_condition(type_col->t == 6, "`type` must be int");
    enforce_condition(call_col->t == 1, "`is_call` must be boolean");
    enforce_condition(qty_col->t == 9, "`qty` must be float");
    enforce_condition(current_col->t == 9, "`current_price` must be float");
    enforce_condition(underlying_price_col->t == 9, "`underlying_price` must be float");
    enforce_condition(underlying_index_col->t == 6, "`underlying_index` must be int");
    enforce_condition(strike_col->t == 9, "`strike` must be float");
    enforce_condition(ttm_col->t == 9, "`time_to_maturity` must be float");
    enforce_condition(iv_col->t == 9, "`implied_vol` must be float");
    enforce_condition(rate_col->t == 9, "`rate` must be float");

    const std::size_t rows = static_cast<std::size_t>(id_col->n);
    enforce_condition(static_cast<std::size_t>(type_col->n) == rows, "`type` row count mismatch");
    enforce_condition(static_cast<std::size_t>(call_col->n) == rows, "`is_call` row count mismatch");
    enforce_condition(static_cast<std::size_t>(qty_col->n) == rows, "`qty` row count mismatch");
    enforce_condition(static_cast<std::size_t>(current_col->n) == rows, "`current_price` row count mismatch");
    enforce_condition(static_cast<std::size_t>(underlying_price_col->n) == rows,
                      "`underlying_price` row count mismatch");
    enforce_condition(static_cast<std::size_t>(underlying_index_col->n) == rows,
                      "`underlying_index` row count mismatch");
    enforce_condition(static_cast<std::size_t>(strike_col->n) == rows, "`strike` row count mismatch");
    enforce_condition(static_cast<std::size_t>(ttm_col->n) == rows, "`time_to_maturity` row count mismatch");
    enforce_condition(static_cast<std::size_t>(iv_col->n) == rows, "`implied_vol` row count mismatch");
    enforce_condition(static_cast<std::size_t>(rate_col->n) == rows, "`rate` row count mismatch");

    risk::InstrumentSoA portfolio;
    portfolio.reserve(rows);

    const I* ids = kI(id_col);
    const I* types = kI(type_col);
    const G* calls = kG(call_col);
    const double* qtys = kF(qty_col);
    const double* current_prices = kF(current_col);
    const double* underlying_prices = kF(underlying_price_col);
    const I* underlying_indices = kI(underlying_index_col);
    const double* strikes = kF(strike_col);
    const double* ttms = kF(ttm_col);
    const double* ivs = kF(iv_col);
    const double* rates = kF(rate_col);

    for (std::size_t row = 0; row < rows; ++row) {
        const std::uint32_t id = static_cast<std::uint32_t>(ids[row]);
        enforce_condition(id < universe_size, "Portfolio id out of bounds at row " + std::to_string(row));

        const std::uint32_t type_raw = static_cast<std::uint32_t>(types[row]);
        enforce_condition(type_raw <= 1U, "Portfolio type invalid at row " + std::to_string(row));
        const std::uint8_t type = static_cast<std::uint8_t>(type_raw);

        const double qty = qtys[row];
        enforce_condition(std::isfinite(qty), "Portfolio qty invalid at row " + std::to_string(row));

        const double current_price = current_prices[row];
        enforce_condition(std::isfinite(current_price) && current_price > 0.0,
                          "Portfolio current_price invalid at row " + std::to_string(row));

        if (type == static_cast<std::uint8_t>(InstrumentType::Equity)) {
            portfolio.id.push_back(id);
            portfolio.type.push_back(type);
            portfolio.is_call.push_back(0);
            portfolio.qty.push_back(qty);
            portfolio.current_price.push_back(current_price);
            portfolio.underlying_price.push_back(current_price);
            portfolio.underlying_index.push_back(id);
            portfolio.strike.push_back(0.0);
            portfolio.time_to_maturity.push_back(0.0);
            portfolio.implied_vol.push_back(0.0);
            portfolio.rate.push_back(0.0);
            continue;
        }

        const std::uint8_t is_call = static_cast<std::uint8_t>(calls[row]);
        enforce_condition(is_call <= 1U, "Portfolio is_call invalid at row " + std::to_string(row));

        const double underlying_price = underlying_prices[row];
        enforce_condition(std::isfinite(underlying_price) && underlying_price > 0.0,
                          "Portfolio underlying_price invalid at row " + std::to_string(row));

        const std::uint32_t underlying_index = static_cast<std::uint32_t>(underlying_indices[row]);
        enforce_condition(underlying_index < universe_size,
                          "Portfolio underlying_index out of bounds at row " + std::to_string(row));

        const double strike = strikes[row];
        enforce_condition(std::isfinite(strike) && strike > 0.0,
                          "Portfolio strike invalid at row " + std::to_string(row));

        const double time_to_maturity = std::max(ttms[row], 0.0);
        enforce_condition(std::isfinite(time_to_maturity), "Portfolio time_to_maturity invalid at row " +
                                                           std::to_string(row));

        double implied_vol = ivs[row];
        enforce_condition(std::isfinite(implied_vol), "Portfolio implied_vol invalid at row " + std::to_string(row));
        implied_vol = std::max(implied_vol, 1e-8);

        const double rate = rates[row];
        enforce_condition(std::isfinite(rate), "Portfolio rate invalid at row " + std::to_string(row));

        portfolio.id.push_back(id);
        portfolio.type.push_back(type);
        portfolio.is_call.push_back(is_call);
        portfolio.qty.push_back(qty);
        portfolio.current_price.push_back(current_price);
        portfolio.underlying_price.push_back(underlying_price);
        portfolio.underlying_index.push_back(underlying_index);
        portfolio.strike.push_back(strike);
        portfolio.time_to_maturity.push_back(time_to_maturity);
        portfolio.implied_vol.push_back(implied_vol);
        portfolio.rate.push_back(rate);
    }

    return portfolio;
}

ShockSnapshot load_shock_data(int handle, std::size_t expected_factors) {
    ShockSnapshot snapshot;

    K table = checked_call(handle, "getShockData[]");
    auto guard = std::unique_ptr<std::remove_pointer_t<K>, decltype(&r0)>(table, &r0);

    if (table->t != 98) {
        throw std::runtime_error("getShockData did not return a table");
    }

    const std::vector<std::string> names = extract_column_names(table);
    enforce_condition(!names.empty(), "Shock data has no columns");
    enforce_condition(names.front() == "date", "First shock column must be `date`");
    enforce_condition(names.size() - 1 == expected_factors, "Shock factor count mismatch");

    K date_col = column_data(table, 0);
    enforce_condition(date_col->t == 14, "Shock `date` column must be type date");

    const std::size_t scenarios = static_cast<std::size_t>(date_col->n);
    snapshot.dates.resize(scenarios);
    for (std::size_t row = 0; row < scenarios; ++row) {
        snapshot.dates[row] = format_date(kI(date_col)[row]);
    }

    snapshot.shocks_flat.assign(scenarios * expected_factors, 0.0);
    for (std::size_t col = 0; col < expected_factors; ++col) {
        const std::string& name = names[col + 1];
        K col_data = column_data(table, col + 1);
        enforce_condition(col_data->t == 9, "Shock column '" + name + "' must be float");
        enforce_condition(static_cast<std::size_t>(col_data->n) == scenarios,
                          "Shock column '" + name + "' row count mismatch");
        const double* values = kF(col_data);
        for (std::size_t row = 0; row < scenarios; ++row) {
            snapshot.shocks_flat[row * expected_factors + col] = values[row];
        }
    }

    return snapshot;
}

Eigen::VectorXd load_sample_mean(int handle, std::size_t expected_factors) {
    K result = checked_call(handle, "getSampleMeanFromShocks[]");
    auto guard = std::unique_ptr<std::remove_pointer_t<K>, decltype(&r0)>(result, &r0);

    Eigen::VectorXd mean(static_cast<Eigen::Index>(expected_factors));

    if (result->t == 9) {
        enforce_condition(static_cast<std::size_t>(result->n) == expected_factors, "Mean vector length mismatch");
        const double* values = kF(result);
        for (std::size_t i = 0; i < expected_factors; ++i) {
            mean(static_cast<Eigen::Index>(i)) = values[i];
        }
        return mean;
    }

    if (result->t == 0) {
        enforce_condition(static_cast<std::size_t>(result->n) == expected_factors, "Mean vector length mismatch");
        for (std::size_t i = 0; i < expected_factors; ++i) {
            K element = kK(result)[i];
            enforce_condition(element && element->t == -9, "Mean entries must be float scalars");
            mean(static_cast<Eigen::Index>(i)) = element->f;
        }
        return mean;
    }

    throw std::runtime_error("Unexpected mean representation from KDB+");
}

Eigen::MatrixXd load_sample_covariance(int handle, std::size_t expected_factors) {
    K result = checked_call(handle, "getSampleCovarianceFromShocks[]");
    auto guard = std::unique_ptr<std::remove_pointer_t<K>, decltype(&r0)>(result, &r0);

    Eigen::MatrixXd covariance(static_cast<Eigen::Index>(expected_factors),
                               static_cast<Eigen::Index>(expected_factors));

    if (result->t != 0) {
        throw std::runtime_error("Covariance matrix must be a list of rows");
    }

    enforce_condition(static_cast<std::size_t>(result->n) == expected_factors, "Covariance row count mismatch");

    for (std::size_t row = 0; row < expected_factors; ++row) {
        K row_vector = kK(result)[row];
        enforce_condition(row_vector, "Covariance row missing");
        if (row_vector->t == 9) {
            enforce_condition(static_cast<std::size_t>(row_vector->n) == expected_factors,
                              "Covariance column count mismatch");
            const double* values = kF(row_vector);
            for (std::size_t col = 0; col < expected_factors; ++col) {
                covariance(static_cast<Eigen::Index>(row), static_cast<Eigen::Index>(col)) = values[col];
            }
            continue;
        }
        if (row_vector->t == 0) {
            enforce_condition(static_cast<std::size_t>(row_vector->n) == expected_factors,
                              "Covariance column count mismatch");
            for (std::size_t col = 0; col < expected_factors; ++col) {
                K element = kK(row_vector)[col];
                enforce_condition(element && element->t == -9, "Covariance entries must be float scalars");
                covariance(static_cast<Eigen::Index>(row), static_cast<Eigen::Index>(col)) = element->f;
            }
            continue;
        }
        throw std::runtime_error("Covariance row has unexpected representation");
    }

    return covariance;
}

} // namespace risk::kdb
