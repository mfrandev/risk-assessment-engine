#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>

using Catch::Approx;

#include <risk/hvar.hpp>
#include <risk/instrument.hpp>
#include <risk/instrument_soa.hpp>
#include <risk/universe.hpp>

TEST_CASE("compute_hvar computes VaR for simple equity book") {
    risk::set_universe({"SPY", "QQQ", "XOM", "TSLA", "AAPL", "WMT"});
    const std::size_t universe_size = risk::universe_size();

    risk::Instrument equity{};
    equity.id = 0;
    equity.type = risk::InstrumentType::Equity;
    equity.qty = 1.0;
    equity.current_price = 100.0;
    equity.underlying_price = equity.current_price;
    equity.underlying_index = 0;

    const auto soa = risk::to_struct_of_arrays({equity});

    std::vector<double> shocks(4 * universe_size, 0.0);
    shocks[0 * universe_size + 0] = -0.10;
    shocks[1 * universe_size + 0] = -0.05;
    shocks[2 * universe_size + 0] = 0.01;
    shocks[3 * universe_size + 0] = 0.02;

    const auto metrics = risk::compute_hvar(soa,
                                            shocks,
                                            /*Tm1=*/4,
                                            universe_size,
                                            0.95);

    REQUIRE(metrics.var == Approx(10.0).margin(1e-9));
    REQUIRE(metrics.cvar == Approx(10.0).margin(1e-9));
}

TEST_CASE("compute_hvar validates shock dimensions") {
    risk::set_universe({"SPY", "QQQ", "XOM", "TSLA", "AAPL", "WMT"});
    const std::size_t universe_size = risk::universe_size();

    risk::Instrument equity{};
    equity.id = 0;
    equity.type = risk::InstrumentType::Equity;
    equity.qty = 1.0;
    equity.current_price = 100.0;
    equity.underlying_price = 100.0;
    equity.underlying_index = 0;

    const auto soa = risk::to_struct_of_arrays({equity});
    const std::vector<double> bad_shocks(universe_size - 1, -0.01);

    REQUIRE_THROWS_AS(risk::compute_hvar(soa,
                                         bad_shocks,
                                         /*Tm1=*/1,
                                         universe_size,
                                         0.95),
                      std::invalid_argument);
}
