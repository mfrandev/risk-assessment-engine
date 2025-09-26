#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using Catch::Approx;

#include <risk/bs.hpp>
#include <risk/greeks.hpp>
#include <risk/instrument.hpp>
#include <risk/instrument_soa.hpp>

namespace {
constexpr double kTolerance = 1e-6;
}

TEST_CASE("compute_greeks matches analytic Black-Scholes outputs") {
    risk::Instrument equity{};
    equity.id = 0;
    equity.type = risk::InstrumentType::Equity;
    equity.qty = 100.0;
    equity.current_price = 50.0;
    equity.underlying_price = equity.current_price;
    equity.underlying_index = 0;

    risk::Instrument call{};
    call.id = 1;
    call.type = risk::InstrumentType::Option;
    call.is_call = true;
    call.qty = 10.0;
    call.current_price = risk::bs::price(true, 50.0, 55.0, 0.01, 0.30, 0.5);
    call.underlying_price = 50.0;
    call.underlying_index = 0;
    call.strike = 55.0;
    call.time_to_maturity = 0.5;
    call.implied_vol = 0.30;
    call.rate = 0.01;

    const std::vector<risk::Instrument> portfolio{equity, call};
    const auto soa = risk::to_struct_of_arrays(portfolio);

    std::vector<risk::bs::BSGreeks> per_contract;
    std::vector<risk::bs::BSGreeks> per_position;
    risk::GreeksSummary totals;
    risk::compute_greeks(soa, per_contract, per_position, totals);

    REQUIRE(per_contract.size() == 2);
    REQUIRE(per_position.size() == 2);

    // Equity leg behaves as delta-one
    REQUIRE(per_contract[0].price == Approx(50.0).margin(kTolerance));
    REQUIRE(per_contract[0].delta == Approx(1.0).margin(kTolerance));
    REQUIRE(per_position[0].delta == Approx(100.0).margin(kTolerance));

    const auto expected_call = risk::bs::call(50.0, 55.0, 0.01, 0.30, 0.5);
    REQUIRE(per_contract[1].price == Approx(expected_call.price).margin(kTolerance));
    REQUIRE(per_contract[1].delta == Approx(expected_call.delta).margin(kTolerance));
    REQUIRE(per_contract[1].gamma == Approx(expected_call.gamma).margin(kTolerance));
    REQUIRE(per_contract[1].vega == Approx(expected_call.vega).margin(kTolerance));
    REQUIRE(per_contract[1].theta == Approx(expected_call.theta).margin(kTolerance));
    REQUIRE(per_contract[1].rho == Approx(expected_call.rho).margin(kTolerance));

    REQUIRE(per_position[1].price == Approx(expected_call.price * call.qty).margin(kTolerance));
    REQUIRE(per_position[1].delta == Approx(expected_call.delta * call.qty).margin(kTolerance));
    REQUIRE(per_position[1].gamma == Approx(expected_call.gamma * call.qty).margin(kTolerance));
    REQUIRE(per_position[1].vega == Approx(expected_call.vega * call.qty).margin(kTolerance));
    REQUIRE(per_position[1].theta == Approx(expected_call.theta * call.qty).margin(kTolerance));
    REQUIRE(per_position[1].rho == Approx(expected_call.rho * call.qty).margin(kTolerance));

    REQUIRE(totals.price == Approx(per_position[0].price + per_position[1].price).margin(kTolerance));
    REQUIRE(totals.delta == Approx(per_position[0].delta + per_position[1].delta).margin(kTolerance));
    REQUIRE(totals.gamma == Approx(per_position[0].gamma + per_position[1].gamma).margin(kTolerance));
    REQUIRE(totals.vega == Approx(per_position[0].vega + per_position[1].vega).margin(kTolerance));
    REQUIRE(totals.theta == Approx(per_position[0].theta + per_position[1].theta).margin(kTolerance));
    REQUIRE(totals.rho == Approx(per_position[0].rho + per_position[1].rho).margin(kTolerance));
}
