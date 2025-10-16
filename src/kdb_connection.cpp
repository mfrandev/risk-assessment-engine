#include "risk/kdb_connection.hpp"

#include <stdexcept>
#include <utility>

#define KXVER 3
extern "C" {
#include "k.h"
}

namespace {

bool handle_ok(int handle) {
    return handle > 0;
}

std::string describe_handle_error(int handle) {
    switch (handle) {
    case 0:
        return "authentication failed";
    case -1:
        return "connection error";
    case -2:
        return "timeout";
    default:
        return "unknown error (" + std::to_string(handle) + ")";
    }
}

} // namespace

namespace risk::kdb {

struct Connection::Impl {
    std::string host;
    int port;
    std::string credentials;
    int handle{-1};
};

Connection::Connection(std::string host, int port, std::string credentials)
    : impl_(std::make_unique<Impl>(Impl{std::move(host), port, std::move(credentials), -1})) {
    open();
}

Connection::~Connection() {
    close();
}

Connection::Connection(Connection&& other) noexcept = default;
Connection& Connection::operator=(Connection&& other) noexcept = default;

bool Connection::is_connected() const noexcept {
    return impl_ && handle_ok(impl_->handle);
}

int Connection::handle() const noexcept {
    return impl_ ? impl_->handle : -1;
}

void Connection::close() {
    if (!impl_ || impl_->handle <= 0) {
        return;
    }
    kclose(impl_->handle);
    impl_->handle = -1;
    m9();
}

void Connection::open() {
    if (!impl_) {
        throw std::runtime_error("connection implementation not initialized");
    }
    const bool use_credentials = !impl_->credentials.empty();
    const int handle = use_credentials
                           ? khpu(const_cast<S>(impl_->host.c_str()),
                                  impl_->port,
                                  const_cast<S>(impl_->credentials.c_str()))
                           : khp(const_cast<S>(impl_->host.c_str()), impl_->port);

    if (!handle_ok(handle)) {
        throw std::runtime_error(
            "Failed to connect to KDB+ at " + impl_->host + ":" + std::to_string(impl_->port) + " - " +
            describe_handle_error(handle));
    }

    impl_->handle = handle;
}

} // namespace risk::kdb
