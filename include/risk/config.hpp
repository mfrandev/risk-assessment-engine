#pragma once

#include <cstdint>

namespace risk {

// Risk-run parameters shared by the engine and the report units.
inline constexpr double kVarConfidence = 0.99;
inline constexpr double kVarHorizonDays = 1.0;
inline constexpr int kMonteCarloPaths = 200000;
inline constexpr std::uint64_t kMonteCarloSeed = 123456789ULL;

// Option expiries are quoted in calendar days; Black-Scholes needs years (actual/365).
inline constexpr double kCalendarDaysPerYear = 365.0;
inline constexpr double years_from_days(double days) { return days / kCalendarDaysPerYear; }

// Reporting conventions: theta is quoted per trading day, vega and rho per 1% move.
inline constexpr double kTradingDaysPerYear = 252.0;
inline constexpr double kPercentScale = 100.0;

} // namespace risk
