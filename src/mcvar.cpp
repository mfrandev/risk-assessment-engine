#include <risk/mcvar.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

#include <risk/hvar.hpp>
#include <risk/universe.hpp>
#include <risk/utils.hpp>

namespace risk {

namespace {

std::vector<double> compute_cholesky(std::span<const double> cov, int dim) {
    if (dim <= 0) {
        throw std::invalid_argument("covariance dimension must be positive");
    }
    const std::size_t expected = static_cast<std::size_t>(dim) * static_cast<std::size_t>(dim);
    if (cov.size() != expected) {
        throw std::invalid_argument("covariance matrix size mismatch");
    }

    std::vector<double> L(expected, 0.0);
    constexpr double kEpsilon = 1e-12;

    for (int i = 0; i < dim; ++i) {
        for (int j = 0; j <= i; ++j) {
            double sum = cov[static_cast<std::size_t>(i) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(j)];
            for (int k = 0; k < j; ++k) {
                sum -= L[static_cast<std::size_t>(i) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(k)] *
                       L[static_cast<std::size_t>(j) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(k)];
            }
            if (i == j) {
                if (sum < -kEpsilon) {
                    throw std::invalid_argument("covariance matrix is not positive definite");
                }
                const double value = sum <= kEpsilon ? 0.0 : std::sqrt(sum);
                L[static_cast<std::size_t>(i) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(j)] = value;
            } else {
                const double diag = L[static_cast<std::size_t>(j) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(j)];
                if (std::abs(diag) <= kEpsilon) {
                    L[static_cast<std::size_t>(i) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(j)] = 0.0;
                } else {
                    L[static_cast<std::size_t>(i) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(j)] = sum / diag;
                }
            }
        }
    }

    return L;
}

} // namespace

RiskMetrics compute_mcvar(const InstrumentSoA& soa,
                          const Eigen::VectorXd& mu,
                          const Eigen::MatrixXd& cov,
                          double horizon_days,
                          double alpha,
                          int paths,
                          std::uint64_t seed) {
    const std::size_t dim = static_cast<std::size_t>(mu.size());
    if (dim == 0) {
        throw std::invalid_argument("mu must have positive dimension");
    }
    if (cov.rows() != static_cast<Eigen::Index>(dim) ||
        cov.cols() != static_cast<Eigen::Index>(dim)) {
        throw std::invalid_argument("covariance matrix dimension mismatch");
    }
    if (dim != universe_size()) {
        throw std::invalid_argument("mu dimension must equal universe size");
    }
    if (!(alpha > 0.0 && alpha < 1.0)) {
        throw std::invalid_argument("alpha must be in (0,1)");
    }
    if (paths <= 0) {
        throw std::invalid_argument("paths must be positive");
    }
    if (horizon_days <= 0.0) {
        throw std::invalid_argument("horizon_days must be positive");
    }

    std::vector<double> drift(dim, 0.0);
    for (std::size_t i = 0; i < dim; ++i) {
        drift[i] = mu(static_cast<Eigen::Index>(i)) * horizon_days;
    }

    std::vector<double> cov_scaled(dim * dim, 0.0);
    for (std::size_t r = 0; r < dim; ++r) {
        for (std::size_t c = 0; c < dim; ++c) {
            cov_scaled[r * dim + c] = cov(static_cast<Eigen::Index>(r), static_cast<Eigen::Index>(c)) * horizon_days;
        }
    }

    const std::vector<double> sqrt_cov = compute_cholesky(std::span<const double>(cov_scaled.data(), cov_scaled.size()), static_cast<int>(dim));

    std::mt19937_64 rng(seed);
    std::normal_distribution<double> norm01(0.0, 1.0);

    std::vector<double> pnls(static_cast<std::size_t>(paths), 0.0);
    std::vector<double> z(dim, 0.0);
    std::vector<double> shocks(dim, 0.0);

    for (int path = 0; path < paths; ++path) {
        for (std::size_t i = 0; i < dim; ++i) {
            z[i] = norm01(rng);
        }
        for (std::size_t i = 0; i < dim; ++i) {
            double sum = 0.0;
            for (std::size_t k = 0; k < dim; ++k) {
                sum += sqrt_cov[i * dim + k] * z[k];
            }
            const double log_return = drift[i] + sum;
            shocks[i] = std::expm1(log_return);
        }
        pnls[static_cast<std::size_t>(path)] = hvarday(soa, shocks.data());
    }

    std::vector<double> pnls_copy = pnls;
    const double q = std::clamp(1.0 - alpha, 0.0, 1.0);
    const double var_quantile = quantile_inplace(pnls_copy, q);

    double tail_sum = 0.0;
    std::size_t tail_count = 0;
    for (double pnl : pnls) {
        if (pnl <= var_quantile) {
            tail_sum += pnl;
            ++tail_count;
        }
    }
    if (tail_count == 0) {
        tail_sum += var_quantile;
        tail_count = 1;
    }

    RiskMetrics metrics;
    metrics.var = -var_quantile;
    metrics.cvar = -(tail_sum / static_cast<double>(tail_count));
    return metrics;
}

} // namespace risk
