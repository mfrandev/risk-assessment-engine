#pragma once

#include <cstddef>
#include <vector>

#include <risk/eigen_stub.hpp>

namespace risk {

// Per-factor mean of a row-major scenarios x factors shock matrix.
Eigen::VectorXd compute_sample_mean(const std::vector<double>& shocks,
                                    std::size_t scenarios,
                                    std::size_t factors);

// Unbiased (n - 1) sample covariance; returns a zero matrix when scenarios <= 1.
Eigen::MatrixXd compute_sample_covariance(const std::vector<double>& shocks,
                                          const Eigen::VectorXd& mean,
                                          std::size_t scenarios,
                                          std::size_t factors);

} // namespace risk
