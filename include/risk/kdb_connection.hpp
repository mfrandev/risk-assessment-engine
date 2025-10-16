#pragma once

#include <memory>
#include <string>

namespace risk::kdb {

class Connection {
public:
    Connection(std::string host, int port, std::string credentials = {});
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;

    bool is_connected() const noexcept;
    int handle() const noexcept;
    void close();

private:
    void open();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace risk::kdb

