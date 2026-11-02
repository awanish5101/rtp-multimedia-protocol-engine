#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <chrono>

namespace rtp {

struct NetworkEndpoint {
    std::string ip{"127.0.0.1"};
    uint16_t port{5004};
};

class UdpSocketTransport {
public:
    UdpSocketTransport();
    ~UdpSocketTransport();

    // Prevent copy, allow move
    UdpSocketTransport(const UdpSocketTransport&) = delete;
    UdpSocketTransport& operator=(const UdpSocketTransport&) = delete;
    UdpSocketTransport(UdpSocketTransport&& other) noexcept;
    UdpSocketTransport& operator=(UdpSocketTransport&& other) noexcept;

    // Bind socket to local port
    bool bind(uint16_t port, const std::string& ip = "0.0.0.0");

    // Send packet to destination
    bool send_to(const uint8_t* data, size_t length, const NetworkEndpoint& dest);
    bool send_to(const std::vector<uint8_t>& data, const NetworkEndpoint& dest);

    // Receive packet (non-blocking with optional timeout)
    // Returns pair of payload and sender endpoint
    struct ReceivedPacket {
        std::vector<uint8_t> data;
        NetworkEndpoint sender;
        std::chrono::steady_clock::time_point arrival_time;
    };
    std::optional<ReceivedPacket> receive(std::chrono::milliseconds timeout = std::chrono::milliseconds(10));

    // Socket controls
    void close();
    [[nodiscard]] bool is_open() const noexcept { return socket_fd_ >= 0; }
    [[nodiscard]] uint16_t bound_port() const noexcept { return bound_port_; }

    // Simulation hooks for automated testing
    void set_simulated_loss_rate(double loss_rate) { simulated_loss_rate_ = loss_rate; }

private:
    int socket_fd_{-1};
    uint16_t bound_port_{0};
    double simulated_loss_rate_{0.0};
};

} // namespace rtp
