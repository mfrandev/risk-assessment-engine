#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using Catch::Approx;

#include <risk/hvar.hpp>
#include <risk/instrument.hpp>
#include <risk/instrument_soa.hpp>

TEST_CASE("historical_var computes VaR and CVaR for simple equity book") {
    risk::Instrument equity{};
    equity.id = 0;
    equity.type = risk::InstrumentType::Equity;
    equity.qty = 1.0;
    equity.current_price = 100.0;
    equity.underlying_price = equity.current_price;
    equity.underlying_index = 0;

    const auto soa = risk::to_struct_of_arrays({equity});

    const std::vector<std::vector<double>> shocks{{-0.10}, {-0.05}, {0.01}, {0.02}};

    const auto metrics = risk::historical_var(soa, shocks, 0.95);

    REQUIRE(metrics.var == Approx(10.0).margin(1e-9));
    REQUIRE(metrics.cvar == Approx(10.0).margin(1e-9));
}

TEST_CASE("historical_var validates shock dimensions") {
    risk::Instrument equity{};
    equity.id = 0;
    equity.type = risk::InstrumentType::Equity;
    equity.qty = 1.0;
    equity.current_price = 100.0;
    equity.underlying_price = 100.0;
    equity.underlying_index = 0;

    const auto soa = risk::to_struct_of_arrays({equity});
    const std::vector<std::vector<double>> bad_shocks{{-0.01, 0.0}}; // wrong width

    REQUIRE_THROWS_AS(risk::historical_var(soa, bad_shocks, 0.95), std::invalid_argument);
}
