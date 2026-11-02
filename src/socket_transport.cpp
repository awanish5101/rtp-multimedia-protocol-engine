#include "rtp/socket_transport.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <random>

namespace rtp {

UdpSocketTransport::UdpSocketTransport() {
    socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ >= 0) {
        // Set SO_REUSEADDR
        int opt = 1;
        ::setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        // Set buffer sizes to 512KB for real-time multimedia
        int buf_size = 512 * 1024;
        ::setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));
        ::setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));
    }
}

UdpSocketTransport::~UdpSocketTransport() {
    close();
}

UdpSocketTransport::UdpSocketTransport(UdpSocketTransport&& other) noexcept
    : socket_fd_(other.socket_fd_), bound_port_(other.bound_port_), simulated_loss_rate_(other.simulated_loss_rate_) {
    other.socket_fd_ = -1;
    other.bound_port_ = 0;
}

UdpSocketTransport& UdpSocketTransport::operator=(UdpSocketTransport&& other) noexcept {
    if (this != &other) {
        close();
        socket_fd_ = other.socket_fd_;
        bound_port_ = other.bound_port_;
        simulated_loss_rate_ = other.simulated_loss_rate_;
        other.socket_fd_ = -1;
        other.bound_port_ = 0;
    }
    return *this;
}

bool UdpSocketTransport::bind(uint16_t port, const std::string& ip) {
    if (socket_fd_ < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (ip == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
            return false;
        }
    }

    if (::bind(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        return false;
    }

    // Retrieve actual bound port if port 0 was passed
    if (port == 0) {
        socklen_t len = sizeof(addr);
        if (::getsockname(socket_fd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
            bound_port_ = ntohs(addr.sin_port);
        }
    } else {
        bound_port_ = port;
    }

    return true;
}

bool UdpSocketTransport::send_to(const uint8_t* data, size_t length, const NetworkEndpoint& dest) {
    if (socket_fd_ < 0 || !data || length == 0) return false;

    // Simulated packet loss hook
    if (simulated_loss_rate_ > 0.0) {
        static thread_local std::mt19937 gen(1337);
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        if (dist(gen) < simulated_loss_rate_) {
            return true; // Dropped silently
        }
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest.port);
    if (::inet_pton(AF_INET, dest.ip.c_str(), &addr.sin_addr) <= 0) {
        return false;
    }

    ssize_t sent = ::sendto(socket_fd_, data, length, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return sent == static_cast<ssize_t>(length);
}

bool UdpSocketTransport::send_to(const std::vector<uint8_t>& data, const NetworkEndpoint& dest) {
    return send_to(data.data(), data.size(), dest);
}

std::optional<UdpSocketTransport::ReceivedPacket> UdpSocketTransport::receive(std::chrono::milliseconds timeout) {
    if (socket_fd_ < 0) return std::nullopt;

    pollfd pfd{};
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;

    int poll_ret = ::poll(&pfd, 1, static_cast<int>(timeout.count()));
    if (poll_ret <= 0 || !(pfd.revents & POLLIN)) {
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(65535); // Max UDP packet size
    sockaddr_in src_addr{};
    socklen_t addr_len = sizeof(src_addr);

    ssize_t received = ::recvfrom(socket_fd_, buffer.data(), buffer.size(), 0,
                                  reinterpret_cast<sockaddr*>(&src_addr), &addr_len);
    auto arrival = std::chrono::steady_clock::now();

    if (received <= 0) return std::nullopt;

    buffer.resize(static_cast<size_t>(received));

    char ip_str[INET_ADDRSTRLEN];
    ::inet_ntop(AF_INET, &(src_addr.sin_addr), ip_str, INET_ADDRSTRLEN);

    ReceivedPacket pkt;
    pkt.data = std::move(buffer);
    pkt.sender.ip = std::string(ip_str);
    pkt.sender.port = ntohs(src_addr.sin_port);
    pkt.arrival_time = arrival;

    return pkt;
}

void UdpSocketTransport::close() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
        bound_port_ = 0;
    }
}

} // namespace rtp
