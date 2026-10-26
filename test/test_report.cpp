#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <vector>

#include <risk/csv.hpp>
#include <risk/greeks.hpp>
#include <risk/instrument.hpp>
#include <risk/instrument_soa.hpp>
#include <risk/report.hpp>

namespace {

std::vector<std::string> output_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

// Splits keeping empty fields; risk::split_csv_line already does so (including a trailing one).
std::vector<std::string> cells(const std::string& line) {
    return risk::split_csv_line(line);
}

} // namespace

TEST_CASE("write_risk_csv emits input rows then a portfolio row with blanks where not applicable") {
    risk::Instrument equity{};
    equity.id = 0;
    equity.type = risk::InstrumentType::Equity;
    equity.qty = 100.0;
    equity.current_price = 461.02;
    equity.underlying_price = 461.02;
    equity.underlying_index = 0;

    risk::Instrument call{};
    call.id = 1;
    call.type = risk::InstrumentType::Option;
    call.is_call = true;
    call.qty = 10.0;
    call.current_price = 15.0;
    call.underlying_price = 101.11;
    call.underlying_index = 1;
    call.strike = 105.0;
    call.days_to_maturity = 182.5;
    call.implied_vol = 0.25;
    call.rate = 0.02;

    const auto soa = risk::to_struct_of_arrays({equity, call});
    const std::vector<std::string> symbols{"SPY", "QQQ"};

    std::vector<risk::bs::BSGreeks> per_contract;
    std::vector<risk::bs::BSGreeks> position;
    risk::GreeksSummary totals;
    risk::compute_greeks(soa, per_contract, position, totals);

    const risk::RiskReport report{soa, symbols, position, totals, {286.5, 290.25}, {800.0, 900.0}};
    std::ostringstream out;
    risk::write_risk_csv(out, report);

    const auto lines = output_lines(out.str());
    REQUIRE(lines.size() == 4); // header + 2 instruments + portfolio

    REQUIRE(lines[0] ==
            "symbol,instrument,strike,days_to_expiry,qty,position_value,"
            "position_delta,position_gamma,position_vega_per_1pct,position_theta_per_day,"
            "hist_var_99_1d,mc_var_99_1d");
    const auto header = cells(lines[0]);
    REQUIRE(header.size() == 12);
    for (std::size_t i = 1; i < lines.size(); ++i) {
        REQUIRE(cells(lines[i]).size() == header.size());
    }

    const auto eq = cells(lines[1]);
    REQUIRE(eq[0] == "SPY");
    REQUIRE(eq[1] == "equity");
    REQUIRE(eq[4] == "100");
    REQUIRE(eq[5] == "46102.000000");
    for (std::size_t c : {2, 3, 6, 7, 8, 9, 10, 11}) {
        REQUIRE(eq[c].empty()); // strike, expiry, greeks and VaR
    }

    const auto opt = cells(lines[2]);
    REQUIRE(opt[0] == "QQQ");
    REQUIRE(opt[1] == "call");
    REQUIRE(opt[2] == "105");
    REQUIRE(opt[3] == "182.5");
    REQUIRE(opt[4] == "10");
    REQUIRE(opt[6] == "4.726553");            // position delta = qty x per-contract delta
    for (std::size_t c = 6; c < 10; ++c) {
        REQUIRE_FALSE(opt[c].empty());
    }
    REQUIRE(opt[10].empty());                 // VaR is portfolio-wide
    REQUIRE(opt[11].empty());

    const auto total = cells(lines[3]);
    REQUIRE(total[0].empty());
    REQUIRE(total[1] == "portfolio");
    for (std::size_t c : {2, 3, 4}) {
        REQUIRE(total[c].empty());
    }
    REQUIRE(total[5] == "46160.875237");
    REQUIRE(total[6] == "104.726553");        // includes equity delta
    REQUIRE(total[10] == "286.500000");
    REQUIRE(total[11] == "800.000000");
}
