#include <risk/universe.hpp>

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace risk {

namespace {

std::vector<std::string> g_symbols;
std::unordered_map<std::string, std::size_t> g_index;
std::mutex g_mutex;

void rebuild_index_locked() {
    g_index.clear();
    for (std::size_t i = 0; i < g_symbols.size(); ++i) {
        g_index.emplace(g_symbols[i], i);
    }
}

} // namespace

void set_universe(std::vector<std::string> symbols) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_symbols = std::move(symbols);
    rebuild_index_locked();
}

const std::vector<std::string>& universe_symbols() {
    return g_symbols;
}

std::size_t universe_size() {
    return g_symbols.size();
}

std::optional<std::size_t> ticker_to_id(std::string_view ticker) {
    const auto it = g_index.find(std::string(ticker));
    if (it == g_index.end()) {
        return std::nullopt;
    }
    return it->second;
}

} // namespace risk
