#pragma once

#include "rtp/packet.hpp"
#include <map>
#include <mutex>
#include <chrono>
#include <cstdint>
#include <optional>

namespace rtp {

struct JitterBufferStats {
    uint64_t packets_received{0};
    uint64_t packets_played{0};
    uint64_t packets_dropped_late{0};
    uint64_t packets_duplicate{0};
    uint64_t packets_lost{0};
    double current_jitter_ms{0.0};
};

class JitterBuffer {
public:
    // Playout delay in milliseconds (e.g. 40ms to 100ms)
    explicit JitterBuffer(std::chrono::milliseconds playout_delay = std::chrono::milliseconds(50));

    // Ingest a packet from UDP receiver
    bool push(RtpPacket packet, std::chrono::steady_clock::time_point arrival_time);

    // Pop the next sequential packet if ready for playout
    std::optional<RtpPacket> pop();

    // Reset buffer state
    void reset();

    // Stats
    [[nodiscard]] JitterBufferStats get_stats() const;
    [[nodiscard]] size_t size() const;
    [[nodiscard]] double interarrival_jitter_ms() const;

    // Helper: RFC 3550 Sequence Number comparison (handles 16-bit modular wrap-around)
    static bool seq_less_than(uint16_t s1, uint16_t s2) noexcept {
        return static_cast<int16_t>(s1 - s2) < 0;
    }

private:
    struct BufferedPacket {
        RtpPacket packet;
        std::chrono::steady_clock::time_point arrival_time;
        std::chrono::steady_clock::time_point playout_time;
    };

    // Custom comparator for 16-bit sequence numbers handling wrap-around
    struct SeqComparator {
        bool operator()(uint16_t a, uint16_t b) const {
            return seq_less_than(a, b);
        }
    };

    const std::chrono::milliseconds playout_delay_;
    std::map<uint16_t, BufferedPacket, SeqComparator> buffer_;
    mutable std::mutex mutex_;

    bool initialized_{false};
    uint16_t next_playout_seq_{0};
    uint16_t max_seq_seen_{0};

    // RFC 3550 Jitter estimation
    // J = J + (|D(i-1, i)| - J) / 16
    double jitter_estimate_samples_{0.0};
    uint32_t last_transit_time_{0};
    bool first_packet_for_jitter_{true};

    JitterBufferStats stats_{};
};

} // namespace rtp
