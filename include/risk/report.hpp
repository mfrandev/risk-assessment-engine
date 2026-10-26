#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include <risk/bs.hpp>
#include <risk/greeks.hpp>
#include <risk/hvar.hpp>
#include <risk/instrument_soa.hpp>

namespace risk {

// Everything the CSV report needs; all values are already computed.
struct RiskReport {
    const InstrumentSoA& portfolio;
    const std::vector<std::string>& symbols;           // indexed by underlying_index
    const std::vector<bs::BSGreeks>& position_greeks;  // one per instrument, quantity-scaled
    const GreeksSummary& totals;
    RiskMetrics historical;
    RiskMetrics monte_carlo;
};

// One row per instrument (identity, position value and quantity-scaled option greeks), followed
// by a single "portfolio" row with totals and the 99% one-day historical and Monte Carlo VaR.
// Cells that do not apply are left empty.
void write_risk_csv(std::ostream& out, const RiskReport& report);

// Creates missing parent directories. Returns false if the file cannot be written.
bool write_risk_csv(const std::string& path, const RiskReport& report);

} // namespace risk
