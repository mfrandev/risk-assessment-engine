#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <risk/instrument.hpp>
#include <risk/instrument_soa.hpp>

using Catch::Approx;

TEST_CASE("to_struct_of_arrays preserves instrument fields") {
    risk::Instrument equity{};
    equity.id = 10;
    equity.type = risk::InstrumentType::Equity;
    equity.qty = 50.0;
    equity.current_price = 20.0;
    equity.underlying_price = 20.0;
    equity.underlying_index = 0;
    equity.rate = 0.01;

    risk::Instrument option{};
    option.id = 11;
    option.type = risk::InstrumentType::Option;
    option.is_call = true;
    option.qty = 5.0;
    option.current_price = 4.0;
    option.underlying_price = 20.0;
    option.underlying_index = 0;
    option.strike = 25.0;
    option.time_to_maturity = 0.75;
    option.implied_vol = 0.35;
    option.rate = 0.02;

    const risk::InstrumentSoA soa = risk::to_struct_of_arrays({equity, option});

    REQUIRE(soa.size() == 2);
    REQUIRE(soa.id[0] == equity.id);
    REQUIRE(soa.id[1] == option.id);

    REQUIRE(soa.type[0] == static_cast<std::uint8_t>(risk::InstrumentType::Equity));
    REQUIRE(soa.type[1] == static_cast<std::uint8_t>(risk::InstrumentType::Option));

    REQUIRE(soa.is_call[0] == 0);
    REQUIRE(soa.is_call[1] == 1);

    REQUIRE(soa.qty[0] == Approx(equity.qty));
    REQUIRE(soa.qty[1] == Approx(option.qty));
    REQUIRE(soa.strike[1] == Approx(option.strike));
    REQUIRE(soa.implied_vol[1] == Approx(option.implied_vol));
    REQUIRE(soa.time_to_maturity[1] == Approx(option.time_to_maturity));
    REQUIRE(soa.rate[1] == Approx(option.rate));
}
