#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <stdexcept>
#include <vector>

#include <risk/stats.hpp>

using Catch::Approx;

namespace {
// 3 scenarios x 2 factors, row-major
const std::vector<double> kShocks{1.0, 2.0,
                                  2.0, 4.0,
                                  3.0, 9.0};
}

TEST_CASE("compute_sample_mean averages each factor") {
    const auto mean = risk::compute_sample_mean(kShocks, 3, 2);
    REQUIRE(mean(0) == Approx(2.0));
    REQUIRE(mean(1) == Approx(5.0));
}

TEST_CASE("compute_sample_covariance is symmetric and unbiased") {
    const auto mean = risk::compute_sample_mean(kShocks, 3, 2);
    const auto cov = risk::compute_sample_covariance(kShocks, mean, 3, 2);
    REQUIRE(cov(0, 0) == Approx(1.0));
    REQUIRE(cov(1, 1) == Approx(13.0));
    REQUIRE(cov(0, 1) == Approx(3.5));
    REQUIRE(cov(1, 0) == Approx(cov(0, 1)));
}

TEST_CASE("compute_sample_covariance returns zeros for a single scenario") {
    const std::vector<double> one{1.0, 2.0};
    const auto mean = risk::compute_sample_mean(one, 1, 2);
    const auto cov = risk::compute_sample_covariance(one, mean, 1, 2);
    REQUIRE(cov(0, 0) == 0.0);
    REQUIRE(cov(1, 1) == 0.0);
}

TEST_CASE("sample statistics reject mismatched dimensions") {
    REQUIRE_THROWS_AS(risk::compute_sample_mean(kShocks, 2, 2), std::invalid_argument);
    REQUIRE_THROWS_AS(risk::compute_sample_mean(kShocks, 0, 2), std::invalid_argument);
    const auto mean = risk::compute_sample_mean(kShocks, 3, 2);
    REQUIRE_THROWS_AS(risk::compute_sample_covariance(kShocks, mean, 3, 3), std::invalid_argument);
}
