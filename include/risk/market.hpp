#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace risk {

bool load_closes_csv(const std::string& path,
                     std::vector<std::string>& dates,
                     std::vector<double>& prices_flat,
                     std::size_t& T,
                     std::size_t& N);

void compute_shocks(const std::vector<double>& prices_flat,
                    std::size_t T,
                    std::size_t N,
                    std::vector<double>& shocks_flat);

} // namespace risk

