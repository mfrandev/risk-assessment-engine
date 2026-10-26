#include <CLI/CLI.hpp>

#include <spdlog/spdlog.h>

#include <exception>
#include <string>

#include <risk/analysis.hpp>
#include <risk/inputs.hpp>
#include <risk/report.hpp>
#include <risk/universe.hpp>

int main(int argc, char** argv) {
    CLI::App app{"risk_assessment_engine"};

    risk::InputOptions options;
    std::string output_path = "data/output/risk_output.csv";

    app.add_option("-p,--portfolio", options.portfolio_path, "Portfolio CSV path")->required();
    app.add_option("-m,--market", options.market_path, "Market closes CSV path")->required();
    app.add_option("-o,--output", output_path, "Risk report CSV path")->default_val(output_path);
#ifdef ENABLE_KDB
    app.add_option("--kdb-host", options.kdb_host, "KDB+ host to connect to")->default_val(options.kdb_host);
    app.add_option("--kdb-port", options.kdb_port, "KDB+ port number")->default_val(options.kdb_port);
    app.add_option("--kdb-auth", options.kdb_credentials, "KDB+ credentials in user:password form");
    app.add_flag("--connect-kdb", options.connect_to_kdb, "Connect to the configured KDB+ instance before processing");
#endif

    try {
        CLI11_PARSE(app, argc, argv);

        spdlog::set_level(spdlog::level::debug);

        const auto inputs = risk::load_inputs(options);
        if (!inputs) {
            return 1;
        }

        const risk::RiskResults results = risk::analyze(*inputs);

        const risk::RiskReport report{inputs->portfolio,
                                      risk::universe_symbols(),
                                      results.position_greeks,
                                      results.totals,
                                      results.historical,
                                      results.monte_carlo};
        if (!risk::write_risk_csv(output_path, report)) {
            return 1;
        }
        spdlog::info("Wrote risk report to '{}'.", output_path);
    } catch (const CLI::ParseError& parse_error) {
        return app.exit(parse_error);
    } catch (const std::exception& ex) {
        spdlog::error("Failed to compute risk metrics: {}", ex.what());
        return 1;
    }

    return 0;
}
