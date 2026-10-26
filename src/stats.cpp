#include <risk/stats.hpp>

#include <stdexcept>

namespace risk {

Eigen::VectorXd compute_sample_mean(const std::vector<double>& shocks,
                                    std::size_t scenarios,
                                    std::size_t factors) {
    if (scenarios == 0 || factors == 0) {
        throw std::invalid_argument("compute_sample_mean requires positive dimensions");
    }
    if (shocks.size() != scenarios * factors) {
        throw std::invalid_argument("shock matrix size mismatch for mean computation");
    }

    Eigen::VectorXd mean = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(factors));
    for (std::size_t t = 0; t < scenarios; ++t) {
        const std::size_t offset = t * factors;
        for (std::size_t i = 0; i < factors; ++i) {
            mean(static_cast<Eigen::Index>(i)) += shocks[offset + i];
        }
    }
    const double inv = 1.0 / static_cast<double>(scenarios);
    for (std::size_t i = 0; i < factors; ++i) {
        mean(static_cast<Eigen::Index>(i)) *= inv;
    }
    return mean;
}

Eigen::MatrixXd compute_sample_covariance(const std::vector<double>& shocks,
                                          const Eigen::VectorXd& mean,
                                          std::size_t scenarios,
                                          std::size_t factors) {
    if (factors == 0) {
        throw std::invalid_argument("compute_sample_covariance requires positive factors");
    }
    if (shocks.size() != scenarios * factors) {
        throw std::invalid_argument("shock matrix size mismatch for covariance computation");
    }
    if (mean.size() != static_cast<Eigen::Index>(factors)) {
        throw std::invalid_argument("mean vector dimension mismatch");
    }

    Eigen::MatrixXd cov = Eigen::MatrixXd::Zero(static_cast<Eigen::Index>(factors),
                                                static_cast<Eigen::Index>(factors));
    if (scenarios <= 1) {
        return cov;
    }

    std::vector<double> diff(factors, 0.0);
    for (std::size_t t = 0; t < scenarios; ++t) {
        const std::size_t offset = t * factors;
        for (std::size_t i = 0; i < factors; ++i) {
            diff[i] = shocks[offset + i] - mean(static_cast<Eigen::Index>(i));
        }
        for (std::size_t i = 0; i < factors; ++i) {
            for (std::size_t j = 0; j < factors; ++j) {
                const double increment = diff[i] * diff[j];
                cov(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) += increment;
            }
        }
    }

    const double inv = 1.0 / static_cast<double>(scenarios - 1);
    for (std::size_t i = 0; i < factors; ++i) {
        for (std::size_t j = 0; j < factors; ++j) {
            cov(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) *= inv;
        }
    }
    return cov;
}

} // namespace risk
