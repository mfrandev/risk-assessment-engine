#include <exception>
#include <iostream>
#include <vector>

#include <risk/hvar.hpp>
#include <risk/instrument.hpp>
#include <risk/instrument_soa.hpp>

int main() {
    std::vector<risk::Instrument> portfolio;
    portfolio.reserve(2);

    risk::Instrument equity{};
    equity.id = 0;
    equity.type = risk::InstrumentType::Equity;
    equity.qty = 120.0;
    equity.current_price = 163.0;
    equity.underlying_price = equity.current_price;
    equity.underlying_index = 0;
    portfolio.push_back(equity);

    risk::Instrument option{};
    option.id = 1;
    option.type = risk::InstrumentType::Option;
    option.is_call = true;
    option.qty = 11.0;
    option.current_price = 9.0;  // observed option premium per contract
    option.underlying_price = 1923.0; // spot of the underlying asset
    option.underlying_index = 0; // index of the equity instrument
    option.strike = 1900.0;
    option.time_to_maturity = 0.5;
    option.implied_vol = 0.3;
    option.rate = 0.02;
    portfolio.push_back(option);

    const auto soa = risk::to_struct_of_arrays(portfolio);

    const std::vector<std::vector<double>> shocks{
        {-0.36, -0.03},
        {0.03, -0.015},
        {0.025, 0.0},
        {0.021, 0.012}
    };

    try {
        const double var_99 = risk::historical_var(soa, shocks, 0.99);
        std::cout << "Sample 99% one-day HVaR: $" << var_99 << '\n';
    } catch (const std::exception& ex) {
        std::cerr << "Failed to compute HVaR: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
