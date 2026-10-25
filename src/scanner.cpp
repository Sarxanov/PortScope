#include "scanner.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

namespace portscope {
namespace {

class UniqueFd {
public:
    explicit UniqueFd(int fd = -1) : fd_(fd) {}

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            Reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    ~UniqueFd() {
        Reset();
    }

    int Get() const {
        return fd_;
    }

    void Reset() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_;
};

bool SetNonBlocking(int fd, std::string& error) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        error = std::string("fcntl(F_GETFL) failed: ") + std::strerror(errno);
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        error = std::string("fcntl(F_SETFL) failed: ") + std::strerror(errno);
        return false;
    }

    return true;
}

bool ResolveIPv4(const std::string& host, sockaddr_in& address, std::string& numeric_address, std::string& error) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    const int rc = getaddrinfo(host.c_str(), nullptr, &hints, &result);
    if (rc != 0) {
        error = std::string("Host resolution failed: ") + gai_strerror(rc);
        return false;
    }

    bool success = false;
    for (const addrinfo* current = result; current != nullptr; current = current->ai_next) {
        if (current->ai_family != AF_INET || current->ai_addrlen < static_cast<socklen_t>(sizeof(sockaddr_in))) {
            continue;
        }

        address = *reinterpret_cast<const sockaddr_in*>(current->ai_addr);
        address.sin_port = 0;

        char buffer[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &address.sin_addr, buffer, sizeof(buffer)) != nullptr) {
            numeric_address = buffer;
            success = true;
            break;
        }
    }

    freeaddrinfo(result);

    if (!success) {
        error = "No usable IPv4 address found for host";
    }

    return success;
}

int RemainingMillis(const std::chrono::steady_clock::time_point deadline) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
        return 0;
    }

    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
    if (remaining <= 0) {
        return 1;
    }

    if (remaining > static_cast<long long>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }

    return static_cast<int>(remaining);
}

PortScanResult ScanSinglePort(const sockaddr_in& base_address, std::uint16_t port, int timeout_ms) {
    PortScanResult result;
    result.port = port;

    UniqueFd fd(socket(AF_INET, SOCK_STREAM, 0));
    if (fd.Get() < 0) {
        result.state = PortState::Filtered;
        result.error_code = errno;
        return result;
    }

    std::string set_nonblocking_error;
    if (!SetNonBlocking(fd.Get(), set_nonblocking_error)) {
        result.state = PortState::Filtered;
        result.error_code = errno;
        return result;
    }

    sockaddr_in destination = base_address;
    destination.sin_port = htons(port);

    while (true) {
        if (connect(fd.Get(), reinterpret_cast<const sockaddr*>(&destination), sizeof(destination)) == 0) {
            result.state = PortState::Open;
            result.error_code = 0;
            return result;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno != EINPROGRESS && errno != EALREADY) {
            const int socket_error = errno;
            result.state = ClassifySocketError(socket_error);
            result.error_code = socket_error;
            return result;
        }

        break;
    }

    pollfd poll_descriptor{};
    poll_descriptor.fd = fd.Get();
    poll_descriptor.events = POLLOUT;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (true) {
        const int remaining = RemainingMillis(deadline);
        if (remaining <= 0) {
            result.state = PortState::Filtered;
            result.error_code = ETIMEDOUT;
            return result;
        }

        const int poll_result = poll(&poll_descriptor, 1, remaining);
        if (poll_result == 0) {
            result.state = PortState::Filtered;
            result.error_code = ETIMEDOUT;
            return result;
        }

        if (poll_result < 0) {
            if (errno == EINTR) {
                continue;
            }

            result.state = PortState::Filtered;
            result.error_code = errno;
            return result;
        }

        int socket_error = 0;
        socklen_t option_length = sizeof(socket_error);
        if (getsockopt(fd.Get(), SOL_SOCKET, SO_ERROR, &socket_error, &option_length) < 0) {
            result.state = PortState::Filtered;
            result.error_code = errno;
            return result;
        }

        result.state = ClassifySocketError(socket_error);
        result.error_code = socket_error;
        return result;
    }
}

}  // namespace

PortState ClassifySocketError(const int socket_error) {
    if (socket_error == 0) {
        return PortState::Open;
    }

    if (socket_error == ECONNREFUSED) {
        return PortState::Closed;
    }

    return PortState::Filtered;
}

bool ScanPorts(const ScanConfig& config, ScanOutput& output, std::string& error) {
    output = ScanOutput{};

    if (config.host.empty()) {
        error = "Host cannot be empty";
        return false;
    }

    if (config.ports.empty()) {
        error = "Port list cannot be empty";
        return false;
    }

    if (config.timeout_ms <= 0) {
        error = "Timeout must be greater than zero";
        return false;
    }

    if (config.concurrency <= 0) {
        error = "Concurrency must be greater than zero";
        return false;
    }

    sockaddr_in base_address{};
    std::string numeric_address;
    if (!ResolveIPv4(config.host, base_address, numeric_address, error)) {
        return false;
    }

    output.resolved_address = numeric_address;
    output.results.resize(config.ports.size());

    std::atomic<std::size_t> next_index{0};
    const std::size_t worker_count = std::min<std::size_t>(config.ports.size(), static_cast<std::size_t>(config.concurrency));

    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    for (std::size_t worker = 0; worker < worker_count; ++worker) {
        workers.emplace_back([&config, &base_address, &output, &next_index]() {
            while (true) {
                const std::size_t index = next_index.fetch_add(1);
                if (index >= config.ports.size()) {
                    break;
                }

                output.results[index] = ScanSinglePort(base_address, config.ports[index], config.timeout_ms);
            }
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    for (const PortScanResult& result : output.results) {
        if (result.state == PortState::Open) {
            ++output.summary.open_count;
        } else if (result.state == PortState::Closed) {
            ++output.summary.closed_count;
        } else {
            ++output.summary.filtered_count;
        }
    }

    return true;
}

}  // namespace portscope
