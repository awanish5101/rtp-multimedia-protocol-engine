#include "rtp/multimedia_streamer.hpp"
#include <iostream>

namespace rtp {

MultimediaStreamer::MultimediaStreamer(StreamerConfig config)
    : config_(std::move(config)),
      jitter_buffer_(config_.jitter_buffer_delay),
      ring_buffer_(config_.ring_buffer_capacity) {}

MultimediaStreamer::~MultimediaStreamer() {
    stop();
}

bool MultimediaStreamer::start() {
    if (running_) return true;

    // Bind receiver transport
    if (!receiver_transport_.bind(config_.local_listen_port, "127.0.0.1")) {
        return false;
    }

    running_ = true;
    start_time_ = std::chrono::steady_clock::now();

    // Spawn background receiver and jitter buffer consumer thread
    receiver_thread_ = std::thread(&MultimediaStreamer::receiver_worker_loop, this);

    return true;
}

void MultimediaStreamer::stop() {
    if (!running_) return;

    running_ = false;
    receiver_transport_.close();
    ring_buffer_.shutdown();

    if (receiver_thread_.joinable()) {
        receiver_thread_.join();
    }
}

bool MultimediaStreamer::send_frame(const std::vector<uint8_t>& frame_data, bool is_marker) {
    if (!running_) return false;

    RtpHeader hdr;
    hdr.version = RTP_VERSION;
    hdr.padding = false;
    hdr.extension = false;
    hdr.csrc_count = 0;
    hdr.marker = is_marker;
    hdr.payload_type = config_.payload_type;
    hdr.sequence_number = next_seq_num_++;
    hdr.timestamp = current_timestamp_;
    hdr.ssrc = config_.ssrc;

    // Advance timestamp according to interval and sample rate
    // e.g. 20ms at 90kHz = 1800 units; 20ms at 8kHz = 160 units
    uint32_t ts_step = static_cast<uint32_t>((config_.sample_rate_hz * config_.packet_interval.count()) / 1000);
    current_timestamp_ += ts_step;

    RtpPacket pkt(hdr, frame_data);
    std::vector<uint8_t> serialized = pkt.serialize();

    bool sent = sender_transport_.send_to(serialized, config_.target_endpoint);
    if (sent) {
        packets_sent_count_++;
        total_bytes_sent_ += serialized.size();
    }
    return sent;
}

std::optional<RtpPacket> MultimediaStreamer::read_received_frame(std::chrono::milliseconds timeout) {
    return ring_buffer_.pop_timeout(timeout);
}

void MultimediaStreamer::receiver_worker_loop() {
    while (running_) {
        // Poll UDP socket for incoming RTP packets
        auto received = receiver_transport_.receive(std::chrono::milliseconds(5));
        if (received.has_value()) {
            auto opt_pkt = RtpPacket::deserialize(received->data);
            if (opt_pkt.has_value()) {
                jitter_buffer_.push(std::move(*opt_pkt), received->arrival_time);
            }
        }

        // Drain ready packets from JitterBuffer into RingBuffer
        while (auto ready_pkt = jitter_buffer_.pop()) {
            ring_buffer_.push(std::move(*ready_pkt));
        }
    }
}

StreamQoSReport MultimediaStreamer::get_qos_report() const {
    StreamQoSReport report;
    report.packets_sent = packets_sent_count_.load();

    auto jb_stats = jitter_buffer_.get_stats();
    report.packets_received = jb_stats.packets_received;
    report.packets_played = jb_stats.packets_played;
    report.packets_lost = jb_stats.packets_lost;
    report.jitter_ms = jb_stats.current_jitter_ms;

    uint64_t total_expected = report.packets_received + report.packets_lost;
    if (total_expected > 0) {
        report.loss_rate_percentage = (static_cast<double>(report.packets_lost) / total_expected) * 100.0;
    }

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_).count();
    if (elapsed_ms > 0) {
        report.throughput_kbps = (static_cast<double>(total_bytes_sent_.load() * 8) / elapsed_ms);
    }

    return report;
}

RtcpReceiverReport MultimediaStreamer::generate_rtcp_report() const {
    auto jb_stats = jitter_buffer_.get_stats();

    RtcpReceiverReport rr;
    rr.ssrc_source = 0x98765432; // Local receiver SSRC
    rr.ssrc_sender = config_.ssrc;

    uint64_t total_expected = jb_stats.packets_received + jb_stats.packets_lost;
    if (total_expected > 0) {
        double frac = static_cast<double>(jb_stats.packets_lost) / total_expected;
        rr.fraction_lost = static_cast<uint8_t>(frac * 256.0);
    }
    rr.cumulative_lost = static_cast<int32_t>(jb_stats.packets_lost);
    rr.highest_seq_received = static_cast<uint32_t>(next_seq_num_);
    rr.interarrival_jitter = static_cast<uint32_t>(jb_stats.current_jitter_ms * 90.0);
    rr.last_sr_timestamp = 0;
    rr.delay_since_last_sr = 0;

    return rr;
}

} // namespace rtp
