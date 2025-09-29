#pragma once

#include <cstddef>
#include <vector>

namespace Eigen {

using Index = std::ptrdiff_t;

class VectorXd {
public:
    VectorXd() = default;
    explicit VectorXd(Index n)
        : data_(static_cast<std::size_t>(n), 0.0) {}

    static VectorXd Zero(Index n) {
        return VectorXd(n);
    }

    Index size() const {
        return static_cast<Index>(data_.size());
    }

    double& operator()(Index idx) {
        return data_.at(static_cast<std::size_t>(idx));
    }

    double operator()(Index idx) const {
        return data_.at(static_cast<std::size_t>(idx));
    }

    const double* data() const {
        return data_.data();
    }

private:
    std::vector<double> data_{};
};

class MatrixXd {
public:
    MatrixXd() = default;
    MatrixXd(Index rows, Index cols)
        : rows_(rows), cols_(cols), data_(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols), 0.0) {}

    static MatrixXd Zero(Index rows, Index cols) {
        return MatrixXd(rows, cols);
    }

    Index rows() const {
        return rows_;
    }

    Index cols() const {
        return cols_;
    }

    double& operator()(Index row, Index col) {
        return data_.at(linear_index(row, col));
    }

    double operator()(Index row, Index col) const {
        return data_.at(linear_index(row, col));
    }

private:
    Index rows_ = 0;
    Index cols_ = 0;
    std::vector<double> data_{};

    std::size_t linear_index(Index row, Index col) const {
        return static_cast<std::size_t>(row) * static_cast<std::size_t>(cols_) +
               static_cast<std::size_t>(col);
    }
};

} // namespace Eigen

