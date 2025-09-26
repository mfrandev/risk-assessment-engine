#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using Catch::Approx;

#include <risk/bs.hpp>

namespace {
constexpr double kTolerance = 1e-6;
}

TEST_CASE("Black-Scholes call price and greeks match known values") {
    const double spot = 100.0;
    const double strike = 100.0;
    const double rate = 0.05;
    const double vol = 0.20;
    const double maturity = 1.0;

    const auto greeks = risk::bs::call(spot, strike, rate, vol, maturity);

    REQUIRE(greeks.price == Approx(10.4505835721856).margin(kTolerance));
    REQUIRE(greeks.delta == Approx(0.636830651175619).margin(kTolerance));
    REQUIRE(greeks.gamma == Approx(0.0187620173458469).margin(kTolerance));
    REQUIRE(greeks.vega == Approx(37.5240346916938).margin(kTolerance));
    REQUIRE(greeks.theta == Approx(-6.4140275464382).margin(kTolerance));
    REQUIRE(greeks.rho == Approx(53.2324815453763).margin(kTolerance));
}

TEST_CASE("Black-Scholes put price and greeks match known values") {
    const double spot = 100.0;
    const double strike = 100.0;
    const double rate = 0.05;
    const double vol = 0.20;
    const double maturity = 1.0;

    const auto greeks = risk::bs::put(spot, strike, rate, vol, maturity);

    REQUIRE(greeks.price == Approx(5.57352602225697).margin(kTolerance));
    REQUIRE(greeks.delta == Approx(-0.363169348824381).margin(kTolerance));
    REQUIRE(greeks.gamma == Approx(0.0187620173458469).margin(kTolerance));
    REQUIRE(greeks.vega == Approx(37.5240346916938).margin(kTolerance));
    REQUIRE(greeks.theta == Approx(-1.65788042393463).margin(kTolerance));
    REQUIRE(greeks.rho == Approx(-41.8904609046951).margin(kTolerance));
}

TEST_CASE("Black-Scholes handles near-zero time or volatility with intrinsic value") {
    const double spot = 110.0;
    const double strike = 100.0;

    const double call_price = risk::bs::price(true, spot, strike, 0.01, 1e-8, 1e-8);
    const double put_price = risk::bs::price(false, spot, strike, 0.01, 1e-8, 1e-8);

    REQUIRE(call_price == Approx(10.0).margin(kTolerance));
    REQUIRE(put_price == Approx(0.0).margin(kTolerance));
}
