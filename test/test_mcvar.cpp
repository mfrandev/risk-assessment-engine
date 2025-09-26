#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <vector>

#include <risk/instrument.hpp>
#include <risk/instrument_soa.hpp>
#include <risk/mcvar.hpp>

using Catch::Approx;

namespace {
risk::InstrumentSoA make_single_equity(double price, double qty = 1.0) {
    risk::Instrument inst{};
    inst.id = 0;
    inst.type = risk::InstrumentType::Equity;
    inst.qty = qty;
    inst.current_price = price;
    inst.underlying_price = price;
    inst.underlying_index = 0;
    return risk::to_struct_of_arrays({inst});
}
}

TEST_CASE("mc_var returns zero risk when drift and covariance are zero") {
    const auto soa = make_single_equity(100.0);

    const std::vector<double> mu{0.0};
    const std::vector<double> cov{0.0};

    const risk::MCParams params{
        .paths = 64,
        .seed = 42,
        .use_cholesky = false,
        .threads = 1
    };

    const auto metrics = risk::mc_var(soa, mu, cov, 1.0, 0.99, params);
    REQUIRE(metrics.var == Approx(0.0).margin(1e-9));
    REQUIRE(metrics.cvar == Approx(0.0).margin(1e-9));
}

TEST_CASE("mc_var matches deterministic drift-only scenario") {
    const auto soa = make_single_equity(100.0);

    const std::vector<double> mu{-0.02}; // -2% expected log-return over horizon
    const std::vector<double> cov{0.0};

    const risk::MCParams params{
        .paths = 16,
        .seed = 7,
        .use_cholesky = false,
        .threads = 1
    };

    const auto metrics = risk::mc_var(soa, mu, cov, 1.0, 0.99, params);

    const double expected_loss = 100.0 - 100.0 * std::exp(-0.02);
    REQUIRE(metrics.var == Approx(expected_loss).margin(1e-6));
    REQUIRE(metrics.cvar == Approx(expected_loss).margin(1e-6));
}
