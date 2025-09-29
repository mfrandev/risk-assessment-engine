#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include <risk/eigen_stub.hpp>

#include <risk/instrument.hpp>
#include <risk/instrument_soa.hpp>
#include <risk/mcvar.hpp>
#include <risk/universe.hpp>

using Catch::Approx;

namespace {

risk::InstrumentSoA make_single_equity(double price, double qty = 1.0) {
    risk::set_universe({"SPY", "QQQ", "XOM", "TSLA", "AAPL", "WMT"});
    risk::Instrument inst{};
    inst.id = 0;
    inst.type = risk::InstrumentType::Equity;
    inst.qty = qty;
    inst.current_price = price;
    inst.underlying_price = price;
    inst.underlying_index = 0;
    return risk::to_struct_of_arrays({inst});
}

} // namespace

TEST_CASE("compute_mcvar returns zero risk when drift and covariance are zero") {
    const auto soa = make_single_equity(100.0);
    const std::size_t universe_size = risk::universe_size();

    Eigen::VectorXd mu = Eigen::VectorXd::Zero(universe_size);
    Eigen::MatrixXd cov = Eigen::MatrixXd::Zero(universe_size, universe_size);

    const auto metrics = risk::compute_mcvar(soa,
                                             mu,
                                             cov,
                                             /*horizon_days=*/1.0,
                                             /*alpha=*/0.99,
                                             /*paths=*/64,
                                             /*seed=*/42ULL);
    REQUIRE(metrics.var == Approx(0.0).margin(1e-9));
    REQUIRE(metrics.cvar == Approx(0.0).margin(1e-9));
}

TEST_CASE("compute_mcvar matches deterministic drift-only scenario") {
    const auto soa = make_single_equity(100.0);
    const std::size_t universe_size = risk::universe_size();

    Eigen::VectorXd mu = Eigen::VectorXd::Zero(universe_size);
    mu(0) = -0.02; // -2% expected return over horizon

    Eigen::MatrixXd cov = Eigen::MatrixXd::Zero(universe_size, universe_size);

    const auto metrics = risk::compute_mcvar(soa,
                                             mu,
                                             cov,
                                             /*horizon_days=*/1.0,
                                             /*alpha=*/0.99,
                                             /*paths=*/16,
                                             /*seed=*/7ULL);

    const double expected_loss = 100.0 - 100.0 * std::exp(mu(0));
    REQUIRE(metrics.var == Approx(expected_loss).margin(1e-6));
    REQUIRE(metrics.cvar == Approx(expected_loss).margin(1e-6));
}
