#include "rtp/jitter_buffer.hpp"
#include <cmath>

namespace rtp {

JitterBuffer::JitterBuffer(std::chrono::milliseconds playout_delay)
    : playout_delay_(playout_delay) {}

bool JitterBuffer::push(RtpPacket packet, std::chrono::steady_clock::time_point arrival_time) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.packets_received++;

    uint16_t seq = packet.header().sequence_number;

    if (!initialized_) {
        initialized_ = true;
        next_playout_seq_ = seq;
        max_seq_seen_ = seq;
    } else {
        // Check if duplicate or late
        if (seq == next_playout_seq_ && buffer_.find(seq) != buffer_.end()) {
            stats_.packets_duplicate++;
            return false;
        }

        if (seq_less_than(seq, next_playout_seq_)) {
            // Arrived after playout window
            stats_.packets_dropped_late++;
            return false;
        }

        if (buffer_.find(seq) != buffer_.end()) {
            stats_.packets_duplicate++;
            return false;
        }

        // Check if there was a sequence jump indicating packet loss
        if (seq_less_than(max_seq_seen_, seq)) {
            uint16_t diff = seq - max_seq_seen_;
            if (diff > 1) {
                stats_.packets_lost += (diff - 1);
            }
            max_seq_seen_ = seq;
        }
    }

    // RFC 3550 Section 6.4.1 Interarrival Jitter Calculation
    // D(i, j) = (R_j - R_i) - (S_j - S_i) = (R_j - S_j) - (R_i - S_i)
    // Convert arrival time to 90kHz timestamp units (approximate for calculation)
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(arrival_time.time_since_epoch()).count();
    uint32_t arrival_ts_units = static_cast<uint32_t>((now_us * 90) / 1000); // 90kHz clock
    uint32_t transit = arrival_ts_units - packet.header().timestamp;

    if (first_packet_for_jitter_) {
        last_transit_time_ = transit;
        first_packet_for_jitter_ = false;
    } else {
        int32_t d = static_cast<int32_t>(transit - last_transit_time_);
        if (d < 0) d = -d;
        last_transit_time_ = transit;
        // J = J + (|D| - J) / 16.0
        jitter_estimate_samples_ += (d - jitter_estimate_samples_) / 16.0;
        stats_.current_jitter_ms = (jitter_estimate_samples_ / 90.0); // 90 units per ms
    }

    BufferedPacket bp;
    bp.packet = std::move(packet);
    bp.arrival_time = arrival_time;
    bp.playout_time = arrival_time + playout_delay_;

    buffer_.emplace(seq, std::move(bp));
    return true;
}

std::optional<RtpPacket> JitterBuffer::pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffer_.empty()) return std::nullopt;

    auto it = buffer_.find(next_playout_seq_);
    if (it != buffer_.end()) {
        auto now = std::chrono::steady_clock::now();
        // If playout time reached or buffer has enough queued frames to prevent starvation
        if (now >= it->second.playout_time || buffer_.size() > 5) {
            RtpPacket pkt = std::move(it->second.packet);
            buffer_.erase(it);
            next_playout_seq_++;
            stats_.packets_played++;
            return pkt;
        }
    } else {
        // If next_playout_seq_ is missing, check if oldest buffered packet is way ahead
        // (indicates packet loss, skip the lost sequence number after timeout)
        auto oldest_it = buffer_.begin();
        auto now = std::chrono::steady_clock::now();
        if (now >= oldest_it->second.playout_time + playout_delay_) {
            // Playout deadline exceeded for missing packets; skip forward
            next_playout_seq_ = oldest_it->first;
            RtpPacket pkt = std::move(oldest_it->second.packet);
            buffer_.erase(oldest_it);
            next_playout_seq_++;
            stats_.packets_played++;
            return pkt;
        }
    }

    return std::nullopt;
}

void JitterBuffer::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
    initialized_ = false;
    next_playout_seq_ = 0;
    max_seq_seen_ = 0;
    jitter_estimate_samples_ = 0.0;
    first_packet_for_jitter_ = true;
    stats_ = JitterBufferStats{};
}

JitterBufferStats JitterBuffer::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

size_t JitterBuffer::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size();
}

double JitterBuffer::interarrival_jitter_ms() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_.current_jitter_ms;
}

} // namespace rtp
