#include <exception>
#include <iostream>
#include <vector>

#include <Eigen/Dense>

#include "risk/hvar.hpp"
#include "risk/instrument.hpp"
#include "risk/instrument_soa.hpp"
#include "risk/mcvar.hpp"

int main() {
    std::vector<risk::Instrument> portfolio;
    portfolio.reserve(2);

    // Equity leg driven by a single market factor (spot return)
    risk::Instrument equity{};
    equity.id = 0;
    equity.type = risk::InstrumentType::Equity;
    equity.qty = 100.0;
    equity.current_price = 200.0;
    equity.underlying_price = equity.current_price;
    equity.underlying_index = 0;
    portfolio.push_back(equity);

    // European call option hedged off the same underlying factor
    risk::Instrument option{};
    option.id = 1;
    option.type = risk::InstrumentType::Option;
    option.is_call = true;
    option.qty = 10.0;
    option.current_price = 15.0;  // observed option premium per contract
    option.underlying_price = 200.0; // spot of the underlying asset
    option.underlying_index = 0;     // map to the equity factor
    option.strike = 200.0;
    option.time_to_maturity = 0.5;
    option.implied_vol = 0.25;
    option.rate = 0.02;
    portfolio.push_back(option);

    const auto soa = risk::to_struct_of_arrays(portfolio);

    // Historical shocks: column 0 = underlying return, column 1 = vol shock
    const std::vector<std::vector<double>> shocks{
        {-0.015, -0.030},  // -1.5% return, vol down 3%
        {0.004, 0.015},    // +0.4% return, vol up 1.5%
        {-0.007, -0.012},  // -0.7% return, vol down 1.2%
        {0.011, 0.000}     // +1.1% return, flat vol
    };

    try {
        std::clog << "==================== HVaR ====================\n";
        const double var_99 = risk::historical_var(soa, shocks, 0.99);
        std::cout << "Sample 99% one-day HVaR: $" << var_99 << '\n';

        Eigen::VectorXd mu(1);
        mu << 0.0003;        // 0.03% daily drift
        Eigen::MatrixXd cov(1, 1);
        cov(0, 0) = 0.0001;  // (1% daily vol)^2

        const risk::MCParams mc_params{
            .paths = 20000,
            .seed = 123456789ULL,
            .use_cholesky = true,
            .threads = 2
        };

        std::clog << "==================== MCVaR ====================\n";
        const double mc_var_99 = risk::mc_var(soa, mu, cov, 1.0, 0.99, mc_params);
        std::cout << "Sample 99% one-day MCVaR: $" << mc_var_99 << '\n';
    } catch (const std::exception& ex) {
        std::cerr << "Failed to compute VaR: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
