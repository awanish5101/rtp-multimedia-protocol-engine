#pragma once

#include "rtp/packet.hpp"
#include "rtp/ring_buffer.hpp"
#include "rtp/jitter_buffer.hpp"
#include "rtp/socket_transport.hpp"
#include <thread>
#include <atomic>
#include <memory>
#include <functional>

namespace rtp {

struct StreamerConfig {
    NetworkEndpoint target_endpoint{"127.0.0.1", 5004};
    uint16_t local_listen_port{5004};
    uint8_t payload_type{static_cast<uint8_t>(PayloadType::DYNAMIC_H264)};
    uint32_t ssrc{0x1A2B3C4D};
    uint32_t sample_rate_hz{90000}; // 90kHz for video, 8000 for PCMU
    std::chrono::milliseconds packet_interval{20}; // 20ms packet cadence (50 fps / audio frame)
    std::chrono::milliseconds jitter_buffer_delay{40};
    size_t ring_buffer_capacity{256};
};

struct StreamQoSReport {
    uint64_t packets_sent{0};
    uint64_t packets_received{0};
    uint64_t packets_played{0};
    uint64_t packets_lost{0};
    double loss_rate_percentage{0.0};
    double jitter_ms{0.0};
    double throughput_kbps{0.0};
};

class MultimediaStreamer {
public:
    explicit MultimediaStreamer(StreamerConfig config);
    ~MultimediaStreamer();

    // Start streaming pipeline
    bool start();

    // Stop streaming pipeline
    void stop();

    // Send a raw media frame (audio or video payload)
    bool send_frame(const std::vector<uint8_t>& frame_data, bool is_marker = false);

    // Read a reordered, de-jittered media frame from the ring buffer
    std::optional<RtpPacket> read_received_frame(std::chrono::milliseconds timeout = std::chrono::milliseconds(50));

    // Get live QoS metrics and RTCP receiver report
    [[nodiscard]] StreamQoSReport get_qos_report() const;
    [[nodiscard]] RtcpReceiverReport generate_rtcp_report() const;

    [[nodiscard]] bool is_running() const noexcept { return running_; }

private:
    void receiver_worker_loop();

    StreamerConfig config_;
    UdpSocketTransport sender_transport_;
    UdpSocketTransport receiver_transport_;

    JitterBuffer jitter_buffer_;
    ThreadSafeRingBuffer<RtpPacket> ring_buffer_;

    std::thread receiver_thread_;
    std::atomic<bool> running_{false};

    // State
    uint16_t next_seq_num_{1000};
    uint32_t current_timestamp_{0};
    std::atomic<uint64_t> total_bytes_sent_{0};
    std::atomic<uint64_t> packets_sent_count_{0};
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace rtp
