#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <risk/utils.hpp>

#include <vector>

using Catch::Approx;

TEST_CASE("quantile_inplace returns expected order statistics") {
    std::vector<double> data{1.0, 2.0, 3.0, 4.0, 5.0};

    REQUIRE(risk::quantile_inplace(data, 0.25) == Approx(2.0));
    REQUIRE(risk::quantile_inplace(data, 0.50) == Approx(3.0));
    REQUIRE(risk::quantile_inplace(data, 0.99) == Approx(4.0));
}

TEST_CASE("quantile_inplace clamps extreme quantiles") {
    std::vector<double> data{10.0, 20.0, 30.0};

    REQUIRE(risk::quantile_inplace(data, -0.5) == Approx(10.0));
    REQUIRE(risk::quantile_inplace(data, 1.5) == Approx(30.0));
}
