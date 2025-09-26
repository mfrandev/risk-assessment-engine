#include "risk/mcvar.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <functional>
#include <random>
#include <stdexcept>
#include <thread>

#include <Eigen/Dense>

#include "risk/bs.hpp"
#include "risk/utils.hpp"

namespace risk {

namespace {

Eigen::MatrixXd compute_sqrt_covariance(const Eigen::MatrixXd& cov,
                                        bool use_cholesky) {
    const auto rows = cov.rows();
    const auto cols = cov.cols();
    if (rows == 0 || cols == 0) {
        throw std::invalid_argument("covariance matrix must be non-empty");
    }
    if (rows != cols) {
        throw std::invalid_argument("covariance matrix must be square");
    }

    if (!use_cholesky) {
        return cov;
    }

    Eigen::MatrixXd L = Eigen::MatrixXd::Zero(rows, cols);
    for (Eigen::Index i = 0; i < rows; ++i) {
        for (Eigen::Index j = 0; j <= i; ++j) {
            double sum = cov(i, j);
            for (Eigen::Index k = 0; k < j; ++k) {
                sum -= L(i, k) * L(j, k);
            }
            if (i == j) {
                if (sum <= 0.0) {
                    throw std::invalid_argument("covariance matrix is not positive definite");
                }
                L(i, j) = std::sqrt(sum);
            } else {
                L(i, j) = sum / L(j, j);
            }
        }
    }
    return L;
}

bool is_option(std::uint8_t type_flag) {
    return type_flag == static_cast<std::uint8_t>(InstrumentType::Option);
}

double revalue_once(const InstrumentSoA& instruments,
                    const Eigen::VectorXd& factor_shocks) {
    const std::size_t n = instruments.size();
    const Eigen::Index shock_dim = factor_shocks.size();
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
            if (static_cast<Eigen::Index>(factor_idx) >= shock_dim) {
                throw std::out_of_range("option references factor index beyond shock vector");
            }

            const double underlying_today = instruments.underlying_price[i] > 0.0
                                                 ? instruments.underlying_price[i]
                                                 : price_today;
            const double underlying_shocked = underlying_today * std::exp(factor_shocks(static_cast<Eigen::Index>(factor_idx)));

            const double option_price = bs::price(instruments.is_call[i] != 0,
                                                  underlying_shocked,
                                                  instruments.strike[i],
                                                  instruments.rate[i],
                                                  instruments.implied_vol[i],
                                                  instruments.time_to_maturity[i]);

            value_shocked += option_price * qty;
        } else {
            const std::uint32_t factor_idx = instruments.underlying_index[i];
            if (static_cast<Eigen::Index>(factor_idx) >= shock_dim) {
                throw std::out_of_range("equity references factor index beyond shock vector");
            }
            const double shocked_price = price_today * std::exp(factor_shocks(static_cast<Eigen::Index>(factor_idx)));
            value_shocked += shocked_price * qty;
        }
    }

    return value_shocked - value_today;
}

} // namespace

double mc_var(const InstrumentSoA& instruments,
              const Eigen::VectorXd& mu,
              const Eigen::MatrixXd& cov,
              double horizon_days,
              double alpha,
              const MCParams& params) {
    if (mu.size() == 0) {
        throw std::invalid_argument("mu vector must be non-empty");
    }
    if (mu.size() != cov.rows() || cov.rows() != cov.cols()) {
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

    const Eigen::MatrixXd sqrt_cov = compute_sqrt_covariance(cov, params.use_cholesky);
    const Eigen::Index dim = mu.size();

    const unsigned int hw_threads = std::thread::hardware_concurrency();
    const int thread_count = params.threads > 0
                                 ? params.threads
                                 : (hw_threads == 0 ? 1 : static_cast<int>(hw_threads));

    std::vector<double> pnl(params.paths, 0.0);
    std::atomic<int> next{0};

    Eigen::VectorXd drift = mu;
    for (Eigen::Index i = 0; i < drift.size(); ++i) {
        drift[i] *= horizon_days;
    }
    const double sqrt_h = std::sqrt(horizon_days);

    auto worker = [&](int worker_id) {
        std::mt19937_64 rng(params.seed + static_cast<std::uint64_t>(worker_id));
        std::normal_distribution<double> norm01(0.0, 1.0);
        Eigen::VectorXd z(dim);
        Eigen::VectorXd shock(dim);
        while (true) {
            const int path_index = next.fetch_add(1);
            if (path_index >= params.paths) {
                break;
            }
            for (Eigen::Index k = 0; k < dim; ++k) {
                z[k] = norm01(rng);
            }
            for (Eigen::Index i = 0; i < dim; ++i) {
                double sum = 0.0;
                for (Eigen::Index k = 0; k <= i; ++k) {
                    sum += sqrt_cov(i, k) * z[k];
                }
                shock[i] = drift[i] + sum * sqrt_h;
            }
            pnl[path_index] = revalue_once(instruments, shock);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back(worker, t);
    }
    for (auto& th : threads) {
        th.join();
    }

    const double quantile = quantile_inplace(pnl, std::clamp(1.0 - alpha, 0.0, 1.0));
    return -quantile;
}

} // namespace risk
