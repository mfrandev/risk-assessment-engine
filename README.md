# Risk Assessment Engine

Command-line portfolio risk analytics engine implemented in modern C++ with optional KDB+ data ingestion.

## Usage
- **Build**  
  ```bash
  ./buildit.sh
  ```
  Generates the Release binary under `build/`.
- **Configure options**  
  Edit `runit.sh` to include the desired command-line flags:  
  - `--portfolio/-p` and `--market/-m` must point to local CSV inputs.  
  - Optional KDB+ flags (`--kdb-host`, `--kdb-port`, `--kdb-auth`, `--connect-kdb`) should be set here as needed.  
  - `--connect-kdb` switches the engine to load market, portfolio, shocks, mean, and covariance from the locally running q instance via the `.api` functions in `scripts/load_data.q`. Ensure that q has sourced the script and exposes those endpoints.
- **Run locally**  
  ```bash
  ./runit.sh
  ```
  Assumes the binary was built already and uses the options defined inside the script.
- **Build and run**  
  ```bash
  ./buildandrunit.sh
  ```
  Rebuilds and executes the engine in one step.
- **Run unit tests**  
  ```bash
  ./testit.sh
  ```
  The script configures the test CMake project, builds, and executes the `risk_tests` binary.
- **KDB+ Integration**
  You must have a KDB+ instance running locally on your machine to make use of the `--connect-kdb` option successfully. Usage from the project <b>root directory</b> is as follows.
  ```bash
  <path-to-q> scripts/load_data.q -p 5000
  ```
  The program assumes the KDB+ server is running on port 5000 by default.

## Third-Party Libraries
- [CLI11](https://github.com/CLIUtils/CLI11) — command-line argument parsing.
- [spdlog](https://github.com/gabime/spdlog) — structured logging utilities.
- [Catch2](https://github.com/catchorg/Catch2) — unit testing framework.
- [Eigen](https://github.com/PX4/eigen) — linear algebra primitives (via local stub where Eigen is unavailable).
- [Kdb+ C API](https://github.com/kxcontrib/capi) — client connectivity to q via the vendor-supplied `c.o`.

## Design and Implementation Details
- **Data pipeline**: CSV loaders populate the universe, price history, and portfolio struct-of-arrays. When KDB+ is enabled, the loader module mirrors those structures by deserializing q tables returned by the functions explicitly named in`.api`.  
- **Risk calculations**: Historical VaR is computed directly from the shock matrix; Monte Carlo VaR uses sample mean/covariance feeding the pricing engine and option Greeks.  
- **Architecture**: Core components are split across `src` modules (market, portfolio, greeks, mcvar, hvar, etc.), with headers under `include/risk`. KDB connectivity uses the thin wrapper in `risk::kdb::Connection` and higher-level loading helpers in `risk::kdb::load_*`.

## Testing and Verification
- Automated unit tests cover pricing primitives, data parsing, and risk metric helpers via `risk_tests`.  
- Building in Release with assertions catches most data-shape mismatches; logging (via spdlog) surfaces invalid inputs at runtime.  
- KDB+ integration should be exercised against a staging q instance to verify table schemas, `.api` outputs, and connectivity parameters.
