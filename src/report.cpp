#include <risk/report.hpp>

#include <risk/config.hpp>
#include <risk/instrument.hpp>

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <ostream>

namespace risk {

namespace {

// Units are part of the column names: vega is per 1% vol move, theta per trading day, VaR is
// a positive 99% one-day dollar loss.
constexpr const char* kHeader =
    "symbol,instrument,strike,days_to_expiry,qty,position_value,"
    "position_delta,position_gamma,position_vega_per_1pct,position_theta_per_day,"
    "hist_var_99_1d,mc_var_99_1d";

constexpr std::size_t kLeadingColumns = 2;   // symbol, instrument
constexpr std::size_t kContractColumns = 2;  // strike, days_to_expiry
constexpr std::size_t kGreekColumns = 4;     // delta, gamma, vega, theta

std::string number(double value) {
    return fmt::format("{:.6f}", value);
}

const char* instrument_label(const InstrumentSoA& pf, std::size_t i) {
    if (pf.type[i] == static_cast<std::uint8_t>(InstrumentType::Equity)) {
        return "equity";
    }
    return pf.is_call[i] != 0 ? "call" : "put";
}

// Greeks in reporting units: vega per 1% vol, theta per trading day.
std::vector<std::string> greek_cells(const bs::BSGreeks& g) {
    return {number(g.delta),
            number(g.gamma),
            number(g.vega / kPercentScale),
            number(g.theta / kTradingDaysPerYear)};
}

void append_blank(std::vector<std::string>& cells, std::size_t count) {
    cells.insert(cells.end(), count, std::string{});
}

void write_row(std::ostream& out, const std::vector<std::string>& cells) {
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        out << cells[i];
    }
    out << '\n';
}

} // namespace

void write_risk_csv(std::ostream& out, const RiskReport& report) {
    const InstrumentSoA& pf = report.portfolio;
    out << kHeader << '\n';

    for (std::size_t i = 0; i < pf.size(); ++i) {
        const auto& position = report.position_greeks[i];
        const bool is_option = pf.type[i] == static_cast<std::uint8_t>(InstrumentType::Option);

        std::vector<std::string> cells = {report.symbols.at(pf.underlying_index[i]),
                                          instrument_label(pf, i)};

        // Strike, expiry and greeks only apply to options; equities leave them empty.
        if (is_option) {
            cells.push_back(fmt::format("{}", pf.strike[i]));
            cells.push_back(fmt::format("{}", pf.days_to_maturity[i]));
        } else {
            append_blank(cells, kContractColumns);
        }
        cells.push_back(fmt::format("{}", pf.qty[i]));
        cells.push_back(number(position.price));
        if (is_option) {
            const auto greeks = greek_cells(position);
            cells.insert(cells.end(), greeks.begin(), greeks.end());
        } else {
            append_blank(cells, kGreekColumns);
        }

        // VaR only exists for the portfolio as a whole.
        append_blank(cells, 2);
        write_row(out, cells);
    }

    // Totals include the delta-one equity delta even though equity rows leave their delta blank.
    std::vector<std::string> total;
    append_blank(total, kLeadingColumns + kContractColumns + 1);  // ... through qty
    total[1] = "portfolio";
    total.push_back(number(report.totals.price));
    const auto greeks = greek_cells({report.totals.price,
                                     report.totals.delta,
                                     report.totals.gamma,
                                     report.totals.vega,
                                     report.totals.theta,
                                     report.totals.rho});
    total.insert(total.end(), greeks.begin(), greeks.end());
    total.push_back(number(report.historical.var));
    total.push_back(number(report.monte_carlo.var));
    write_row(out, total);
}

bool write_risk_csv(const std::string& path, const RiskReport& report) {
    const std::filesystem::path target(path);
    if (target.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(target.parent_path(), ec);
        if (ec) {
            spdlog::error("Failed to create output directory '{}': {}",
                          target.parent_path().string(),
                          ec.message());
            return false;
        }
    }

    std::ofstream out(target);
    if (!out.is_open()) {
        spdlog::error("Failed to open output CSV: {}", path);
        return false;
    }
    write_risk_csv(out, report);
    out.flush();
    if (!out) {
        spdlog::error("Failed writing output CSV: {}", path);
        return false;
    }
    return true;
}

} // namespace risk
