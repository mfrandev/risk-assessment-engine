#include "risk/mcvar.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <functional>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

#include "risk/bs.hpp"
#include "risk/utils.hpp"

namespace risk {

namespace {

bool is_option(std::uint8_t type_flag) {
    return type_flag == static_cast<std::uint8_t>(InstrumentType::Option);
}

std::vector<double> compute_sqrt_covariance(std::span<const double> cov,
                                            int dim,
                                            bool use_cholesky) {
    if (dim <= 0) {
        throw std::invalid_argument("covariance dimension must be positive");
    }
    if (cov.size() != static_cast<std::size_t>(dim * dim)) {
        throw std::invalid_argument("covariance matrix size mismatch");
    }

    if (!use_cholesky) {
        return std::vector<double>(cov.begin(), cov.end());
    }

    std::vector<double> L(static_cast<std::size_t>(dim * dim), 0.0);
    for (int i = 0; i < dim; ++i) {
        for (int j = 0; j <= i; ++j) {
            double sum = cov[static_cast<std::size_t>(i * dim + j)];
            for (int k = 0; k < j; ++k) {
                sum -= L[static_cast<std::size_t>(i * dim + k)] *
                        L[static_cast<std::size_t>(j * dim + k)];
            }
            if (i == j) {
                if (sum <= 0.0) {
                    throw std::invalid_argument("covariance matrix is not positive definite");
                }
                L[static_cast<std::size_t>(i * dim + j)] = std::sqrt(sum);
            } else {
                const double diag = L[static_cast<std::size_t>(j * dim + j)];
                if (diag == 0.0) {
                    throw std::invalid_argument("encountered zero diagonal during Cholesky");
                }
                L[static_cast<std::size_t>(i * dim + j)] = sum / diag;
            }
        }
    }
    return L;
}

double revalue_once(const InstrumentSoA& instruments,
                    std::span<const double> factor_shocks) {
    const std::size_t n = instruments.size();
    const std::size_t shock_dim = factor_shocks.size();
    double value_today = 0.0;
    double value_shocked = 0.0;

    for (std::size_t i = 0; i < n; ++i) {
        const double qty = instruments.qty[i];
        const double price_today = instruments.current_price[i];
        const double value_today_instrument = price_today * qty;
        value_today += value_today_instrument;

        if (qty == 0.0) {
            continue;
        }

        if (is_option(instruments.type[i])) {
            const std::uint32_t factor_idx = instruments.underlying_index[i];
            if (static_cast<std::size_t>(factor_idx) >= shock_dim) {
                throw std::out_of_range("option references factor index beyond shock vector");
            }

            const double underlying_today = instruments.underlying_price[i] > 0.0
                                                 ? instruments.underlying_price[i]
                                                 : price_today;
            const double underlying_shocked = underlying_today *
                                               std::exp(factor_shocks[static_cast<std::size_t>(factor_idx)]);

            const double option_price = bs::price(instruments.is_call[i] != 0,
                                                  underlying_shocked,
                                                  instruments.strike[i],
                                                  instruments.rate[i],
                                                  instruments.implied_vol[i],
                                                  instruments.time_to_maturity[i]);

            value_shocked += option_price * qty;
        } else {
            const std::uint32_t factor_idx = instruments.underlying_index[i];
            if (static_cast<std::size_t>(factor_idx) >= shock_dim) {
                throw std::out_of_range("equity references factor index beyond shock vector");
            }
            const double shocked_price = price_today *
                                         std::exp(factor_shocks[static_cast<std::size_t>(factor_idx)]);
            value_shocked += shocked_price * qty;
        }
    }

    return value_shocked - value_today;
}

} // namespace

RiskMetrics mc_var(const InstrumentSoA& instruments,
                   std::span<const double> mu,
                   std::span<const double> cov,
                   double horizon_days,
                   double alpha,
                   const MCParams& params) {
    if (mu.empty()) {
        throw std::invalid_argument("mu vector must be non-empty");
    }
    const std::size_t dim_size = mu.size();
    if (cov.size() != dim_size * dim_size) {
        throw std::invalid_argument("mu dimension must match covariance size");
    }
    if (alpha <= 0.0 || alpha >= 1.0) {
        throw std::invalid_argument("alpha must be in (0,1)");
    }
    if (params.paths <= 0) {
        throw std::invalid_argument("paths must be positive");
    }
    if (horizon_days <= 0.0) {
        throw std::invalid_argument("horizon_days must be positive");
    }

    const int dim = static_cast<int>(dim_size);
    const std::vector<double> sqrt_cov = compute_sqrt_covariance(cov, dim, params.use_cholesky);

    const unsigned int hw_threads = std::thread::hardware_concurrency();
    const int thread_count = params.threads > 0
                                 ? params.threads
                                 : (hw_threads == 0 ? 1 : static_cast<int>(hw_threads));

    std::vector<double> pnl(static_cast<std::size_t>(params.paths), 0.0);
    std::atomic<int> next{0};

    std::vector<double> drift(mu.begin(), mu.end());
    for (double& v : drift) {
        v *= horizon_days;
    }
    const double sqrt_h = std::sqrt(horizon_days);

    auto worker = [&](int worker_id) {
        std::mt19937_64 rng(params.seed + static_cast<std::uint64_t>(worker_id));
        std::normal_distribution<double> norm01(0.0, 1.0);
        std::vector<double> z(static_cast<std::size_t>(dim), 0.0);
        std::vector<double> shock(static_cast<std::size_t>(dim), 0.0);
        while (true) {
            const int path_index = next.fetch_add(1);
            if (path_index >= params.paths) {
                break;
            }
            for (int k = 0; k < dim; ++k) {
                z[static_cast<std::size_t>(k)] = norm01(rng);
            }
            for (int i = 0; i < dim; ++i) {
                double sum = 0.0;
                for (int k = 0; k < dim; ++k) {
                    sum += sqrt_cov[static_cast<std::size_t>(i * dim + k)] *
                           z[static_cast<std::size_t>(k)];
                }
                shock[static_cast<std::size_t>(i)] =
                    drift[static_cast<std::size_t>(i)] + sum * sqrt_h;
            }
            pnl[static_cast<std::size_t>(path_index)] = revalue_once(instruments, shock);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(thread_count));
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back(worker, t);
    }
    for (auto& th : threads) {
        th.join();
    }

    const double q = std::clamp(1.0 - alpha, 0.0, 1.0);
    const double quantile = quantile_inplace(pnl, q);

    double tail_sum = 0.0;
    std::size_t tail_count = 0;
    for (double x : pnl) {
        if (x <= quantile) {
            tail_sum += x;
            ++tail_count;
        }
    }
    if (tail_count == 0) {
        tail_sum += quantile;
        tail_count = 1;
    }
    const double cvar = -(tail_sum / static_cast<double>(tail_count));

    return RiskMetrics{.var = -quantile, .cvar = cvar};
}

} // namespace risk

