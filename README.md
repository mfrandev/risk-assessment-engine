# Risk Assessment Engine

[![CI](https://github.com/mfrandev/risk-assessment-engine/actions/workflows/ci.yml/badge.svg)](https://github.com/mfrandev/risk-assessment-engine/actions/workflows/ci.yml)

***Please note that this is an AI generated project summary***

A command-line portfolio risk engine written in C++20. It reads a market-close history and a portfolio of equities and European options, then writes a CSV report containing:

- **Historical Value-at-Risk (VaR)** from the observed daily shocks
- **Monte Carlo VaR** from a fitted multivariate normal model
- **Black-Scholes value and Greeks** (delta, gamma, vega, theta) for each option position and for the whole portfolio
s
Inputs come from CSV files by default. An optional, compile-time-gated KDB+ integration can instead load the market, portfolio and precomputed shocks, mean and covariance from a running q process.

## Contents
- [Quick start](#quick-start)
- [CSV schemas](#csv-schemas)
- [Calculations](#calculations)
- [Enabling KDB+](#enabling-kdb)
- [Project layout](#project-layout)
- [Testing](#testing)
- [Limitations](#limitations)
- [Third-party libraries](#third-party-libraries)

## Quick start

Requirements: CMake 3.20+, a C++20 compiler, and the git submodules (`git submodule update --init --recursive`). No KDB+ files or q server are needed for the default build.

```bash
./buildit.sh      # wipe build/, then configure + build (Release)
./runit.sh        # run on the bundled sample data (requires a prior build)
./buildandrunit.sh  # both in one step
./testit.sh       # wipe build_test/, then build and run the unit tests
```

`runit.sh` runs the binary from `build/` with:

```bash
./risk_assessment_engine \
  --portfolio ../data/portfolio/portfolio_scenario1.csv \
  --market    ../data/market/market_scenario1.csv \
  --output    ../data/output/risk_output.csv
```

| Option | Description |
|---|---|
| `-p, --portfolio` | Portfolio CSV (required) |
| `-m, --market` | Market closes CSV (required) |
| `-o, --output` | Report CSV path. Default `data/output/risk_output.csv`, relative to the working directory. Missing directories are created. |
| `--connect-kdb`, `--kdb-host`, `--kdb-port`, `--kdb-auth` | Only present in builds configured with `-DENABLE_KDB=ON`; see [Enabling KDB+](#enabling-kdb) |

Progress and diagnostics are logged with spdlog; results go only to the output file. The process exits non-zero if inputs are invalid or the report cannot be written. The report is git-ignored (`data/output/*.csv`), so it exists only after you run the engine.

## CSV schemas

Three CSV files are involved: two inputs and one output. `data/market/headers.csv` and `data/portfolio/headers.csv` hold only the header row of the respective input and can be copied as a starting template.

### Input: market closes (`data/market/*.csv`)

| Column | Type | Meaning |
|---|---|---|
| `date` | text | Row label, e.g. `10-11-2022`. Rows must be ordered oldest first. Not parsed beyond being a label. |
| `<TICKER>` ... | number > 0 | One column per ticker with its closing price. The header names are the universe; their order defines each ticker's zero-based id. At least one ticker is required. |

The first header must be exactly `date` (case-sensitive), and every row must have the same number of fields as the header; fields are split on plain commas, so quoting is not supported. At least two data rows are required (T rows give T-1 daily shock scenarios). An empty ticker name, a non-numeric value or a non-positive close is rejected. Duplicate ticker names are not detected by the engine.

```csv
date,SPY,QQQ,XOM,TSLA,AAPL,WMT
10-11-2022,453.32,100.00,92.43,1923.21,205.29,359.04
10-12-2022,458.38,102.01,91.27,1960.11,198.22,350.28
```

### Input: portfolio (`data/portfolio/*.csv`)
One instrument per row. The header must be exactly these 11 column names in this order, and the file must contain at least one instrument row:

| Column | Type | Equity | Option | Meaning |
|---|---|---|---|---|
| `id` | integer | required | required | Universe id (column position of a ticker in the market file, starting at 0); must be below the number of tickers. For equities this is the held ticker. For options it is only range-checked; the underlying comes from `underlying_index`. |
| `type` | `0`/`1` | `0` | `1` | `0` = equity, `1` = option |
| `is_call` | `0`/`1` | `0`, `1` or blank (ignored) | required | `1` call, `0` put |
| `qty` | number | required | required | Position size in units of the underlying (no 100x contract multiplier); negative for short |
| `current_price` | number > 0 | required | required | Quoted price per unit. Equity value and VaR use it. For options it is only a quote: options are valued with Black-Scholes. |
| `underlying_price` | number > 0 | optional, ignored (`current_price` is always used) | required | Spot of the underlying |
| `underlying_index` | integer | optional (must equal `id`) | required | Universe id of the option's underlying |
| `strike` | number > 0 | never read | required | Option strike |
| `time_to_maturity` | number | optional, discarded | required | Years to expiry (negative values are floored at 0) |
| `implied_vol` | number | optional, discarded | required | Annualised volatility, e.g. `0.25` (floored at 1e-8) |
| `rate` | number | optional, discarded | optional (default `0`) | Continuously compounded risk-free rate |

For equities, the discarded fields are stored as zero. `time_to_maturity`, `implied_vol`, `rate`, `underlying_price` and `is_call` may be blank, but if filled they must still be valid (finite numbers, and `underlying_price` > 0, `is_call` 0 or 1) or the row is rejected; only `strike` is never read.

```csv
id,type,is_call,qty,current_price,underlying_price,underlying_index,strike,time_to_maturity,implied_vol,rate
0,0,0,100,461.02,461.02,0,0,0,0,0
1,1,1,10,15.0,101.11,1,105.0,0.5,0.25,0.02
```

The sample file is 100 shares of SPY and 10 QQQ calls (strike 105, 6 months, 25% vol).

### Output: risk report (`data/output/risk_output.csv`)
One row per input row, in input order, followed by one final `portfolio` row. Cells that do not apply are left **empty** (never `0`). Numeric results use 6 decimal places; `strike`, `years_to_expiry` and `qty` are echoed as stored (a negative maturity is stored, and shown, as `0`).

| Column | Instrument rows | `portfolio` row | Meaning |
|---|---|---|---|
| `symbol` | underlying ticker | empty | e.g. `SPY`, `QQQ` |
| `instrument` | `equity`, `call` or `put` | `portfolio` | Row kind |
| `strike` | options only | empty | Option strike |
| `years_to_expiry` | options only | empty | Time to maturity in years |
| `qty` | always | empty | Position size in units of the underlying |
| `position_value` | always | total | `qty` x price in dollars. Options use the Black-Scholes price, equities the market price. |
| `position_delta` | options only | total | `qty` x per-contract delta, in units of the underlying. The total includes the delta-one equity exposure, although equity rows leave their delta blank. |
| `position_gamma` | options only | total | `qty` x per-contract gamma, per $² |
| `position_vega_per_1pct` | options only | total | Dollar change per 1 percentage-point move in implied volatility |
| `position_theta_per_day` | options only | total | Dollar change per trading day (annual theta / 252) |
| `hist_var_99_1d` | empty | value | Historical 99% one-day VaR in dollars: a loss is positive, and it is negative if even the 1% worst P&L is a gain |
| `mc_var_99_1d` | empty | value | Monte Carlo 99% one-day VaR in dollars, same sign convention |

All greeks are **position-level** (already multiplied by `qty`), so they are not bounded by [-1, 1]: 10 calls with a per-contract delta of 0.47 show a `position_delta` of 4.7. VaR is portfolio-wide, so it appears only on the `portfolio` row.

Sample (from `data/output/risk_output.csv`):

```csv
symbol,instrument,strike,years_to_expiry,qty,position_value,position_delta,position_gamma,position_vega_per_1pct,position_theta_per_day,hist_var_99_1d,mc_var_99_1d
SPY,equity,,,100,46102.000000,,,,,,
QQQ,call,105,0.5,10,58.875237,4.726553,0.222674,2.845558,-0.315553,,
,portfolio,,,,46160.875237,104.726553,0.222674,2.845558,-0.315553,286.108783,837.872073
```

Note that the sample option's quoted `current_price` (15.0) differs from its Black-Scholes value (5.89). The engine always values options with the model (see below), so the quote is not used for them.

## Calculations

All run parameters are named constants in [include/risk/config.hpp](include/risk/config.hpp): 99% confidence, 1-day horizon, 200,000 Monte Carlo paths, fixed seed, 252 trading days per year.

### Shocks
For consecutive rows of closes, the shock of each ticker is its simple one-day return, `P(t) / P(t-1) - 1`. A history of T rows gives T-1 scenarios.

### Revaluation under a shock
Every VaR scenario applies one row of shocks to the book and measures the P&L, `value(shocked) - value(today)`:

- **Equity**: price x (1 + shock of its ticker).
- **Option**: Black-Scholes price at spot x (1 + shock of its underlying), with strike, volatility, rate and time to maturity held fixed (no time decay, no volatility or rate shocks). Both the "today" and the "shocked" values are model prices, so a zero shock gives exactly zero P&L regardless of the market quote.

### Historical VaR (`hist_var_99_1d`)
Revalue the portfolio under each historical shock row and collect the P&Ls. VaR is the loss at the 1% quantile of that distribution, taken as an order statistic at rank `floor(q x (n-1))` without interpolation. VaR is reported as the negated quantile P&L (a loss is positive). The code also computes expected shortfall (the average P&L at or below that quantile) in `metrics_from_pnls`, but the report does not include it.

### Monte Carlo VaR (`mc_var_99_1d`)
1. Estimate the sample mean vector and the unbiased (n-1) sample covariance matrix of the shocks.
2. Scale both by the horizon (1 day), take a Cholesky factor of the covariance, and draw correlated normal returns with a seeded `std::mt19937_64` (the same seed always gives the same result).
3. Convert each draw to a price shock with `expm1` (the draw is treated as a log return), revalue the portfolio exactly as above, and compute VaR from the 200,000 simulated P&Ls with the same quantile definition as the historical method.

The Cholesky step tolerates rank-deficient covariance (more tickers than scenarios) but throws if the matrix is meaningfully non-positive-definite.

### Black-Scholes greeks
European options, no dividends, with `d1 = [ln(S/K) + (r + sigma^2/2) T] / (sigma sqrt(T))` and `d2 = d1 - sigma sqrt(T)`:

| Greek | Call | Put |
|---|---|---|
| Price | `S N(d1) - K e^(-rT) N(d2)` | `K e^(-rT) N(-d2) - S N(-d1)` |
| Delta | `N(d1)` | `-N(-d1)` |
| Gamma | `phi(d1) / (S sigma sqrt(T))` | same |
| Vega | `S phi(d1) sqrt(T)` | same |
| Theta (per year) | `-S phi(d1) sigma / (2 sqrt(T)) - r K e^(-rT) N(d2)` | `-S phi(d1) sigma / (2 sqrt(T)) + r K e^(-rT) N(-d2)` |
| Rho (computed, not in the report) | `K T e^(-rT) N(d2)` | `-K T e^(-rT) N(-d2)` |

Maturity and volatility are floored at 1e-8. At or below that floor, the greeks calculation returns the intrinsic value, a delta of 0 or +/-1, and zero for the other greeks; the pricing used for VaR revaluation simply evaluates the formula at the floored inputs. Equities are treated as delta-one with zero for the remaining greeks. Per-contract delta is bounded in [0, 1] for calls and [-1, 0] for puts; position greeks are quantity x the per-contract value.

## <a id="enabling-kdb"></a>Enabling KDB+

KDB+ support is **off by default**, so the project builds without any KDB+ files. The KDB code is compiled only when you configure with `ENABLE_KDB`:

```bash
./buildit.sh -DENABLE_KDB=ON
```

This adds the `--connect-kdb`, `--kdb-host` (default `localhost`), `--kdb-port` (default `5000`) and `--kdb-auth user:password` options and links the vendor C API.

**Requirements for an enabled build**
- The vendor-supplied KDB+ C API object for your platform inside the `lib/capi` submodule: `lib/capi/l64/c.o` (Linux), `lib/capi/m64/c.o` (Intel macOS) or `lib/capi/m1/c.o` (Apple Silicon). Download it from KX.
- A fresh checkout's copy can be an HTML download page rather than a binary. CMake checks the file's format and stops with an explicit error if it is not valid for your platform; replace the file and reconfigure.

**Running with a q server** (needed only at run time, and only with `--connect-kdb`)

From the project root, start q with the loader script:

```bash
<path-to-q> scripts/load_data.q -p 5000
```

The script loads `data/market/market_scenario1.csv` and `data/portfolio/portfolio_scenario1.csv`, computes the shock table and the sample mean/covariance (`scripts/compute_shocks.q`, `scripts/metrics.q`), and exposes `getMarketData`, `getPortfolioData`, `getShockData`, `getSampleMeanFromShocks` and `getSampleCovarianceFromShocks`. Then run the engine with `--connect-kdb` (for example `cd build && ./risk_assessment_engine -p ../data/portfolio/portfolio_scenario1.csv -m ../data/market/market_scenario1.csv -o ../data/output/risk_output.csv --connect-kdb`). In this mode all inputs (market, portfolio, shocks, mean and covariance) are read from q and the CSV files are not read. `--portfolio` and `--market` remain required because they are the fallback: if the connection succeeds but loading fails, the engine logs a warning and uses the CSV files. If the connection itself fails, the run aborts with an error.

## Project layout

`src/risk_assessment_engine.cpp` only parses the command line and runs the pipeline `load_inputs` -> `analyze` -> `write_risk_csv`. Headers are under `include/risk/`.

| Module | Role |
|---|---|
| `inputs` | Loads market, portfolio, shocks, mean and covariance from CSV (or KDB+) into `EngineInputs` |
| `analysis` | Runs historical VaR, Monte Carlo VaR and greeks on the inputs |
| `report` | Writes the CSV report |
| `market`, `portfolio` | CSV parsing and validation; shock computation |
| `csv` | Shared field splitting and trimming |
| `stats` | Sample mean and covariance |
| `utils` | Order-statistic quantile used by both VaR methods |
| `bs`, `greeks` | Black-Scholes pricing and greeks, per-position aggregation |
| `hvar`, `mcvar` | Scenario revaluation, historical and Monte Carlo VaR, shared `metrics_from_pnls` |
| `instrument`, `instrument_soa` | Portfolio data layout (struct-of-arrays) |
| `universe` | Ticker list and id lookup |
| `config` | Named run parameters |
| `kdb_connection`, `kdb_loader` | Optional KDB+ integration (only built with `ENABLE_KDB`) |
| `eigen_stub` | Minimal vector/matrix types used for the covariance math |

## Testing

```bash
./testit.sh
```

This builds `test/` and runs `risk_tests` (Catch2). GitHub Actions ([.github/workflows/ci.yml](.github/workflows/ci.yml)) builds the engine, runs it on the sample data and runs these tests on Ubuntu (GCC, Clang) and macOS (Clang) for every push to `main` and every pull request. The tests cover:

- Black-Scholes prices and greeks against known values, finite-difference bumps and put-call parity
- Delta bounds and position scaling
- Historical and Monte Carlo VaR, including the zero-shock P&L property and the shared VaR/ES helper (ES is computed but not reported)
- CSV parsing helpers, sample statistics, instrument layout, quantiles
- The CSV report layout (columns, per-contract vs position greeks, blank cells, portfolio row)

The KDB+ integration has no automated tests and needs a running q instance to verify.

## Limitations

- European options only; no dividends, no early exercise, one flat volatility and rate per option.
- Revaluation shocks only the spot: no volatility, rate or time-decay effects, and a fixed 1-day horizon at 99% confidence (change `config.hpp` to alter these).
- Historical VaR is only as good as the history provided; the quantile has no interpolation, so with a small sample (the bundled data has 3 scenarios) the 99% VaR is simply the worst scenario.
- Shocks are simple returns, while the Monte Carlo step treats sampled values as log returns; the difference is second order at daily horizons.
- Monte Carlo uses the historical mean as drift and assumes normal returns.

## Third-party libraries
- [CLI11](https://github.com/CLIUtils/CLI11) — command-line argument parsing.
- [spdlog](https://github.com/gabime/spdlog) — logging.
- [Catch2](https://github.com/catchorg/Catch2) — unit testing.
- [Eigen](https://github.com/PX4/eigen) — vendored as a submodule, but the code currently builds against the small in-repo `eigen_stub.hpp`.
- [KDB+ C API](https://github.com/kxcontrib/capi) — optional client connectivity to q via the platform-specific vendor-supplied `c.o`.
