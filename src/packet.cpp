#include "rtp/packet.hpp"
#include <arpa/inet.h>
#include <cstring>

namespace rtp {

RtpPacket::RtpPacket(const RtpHeader& header, std::vector<uint8_t> payload)
    : header_(header), payload_(std::move(payload)) {}

size_t RtpPacket::total_size() const noexcept {
    return RTP_FIXED_HEADER_SIZE + (header_.csrc_list.size() * sizeof(uint32_t)) + payload_.size();
}

std::vector<uint8_t> RtpPacket::serialize() const {
    const size_t csrc_bytes = header_.csrc_list.size() * sizeof(uint32_t);
    const size_t total_len = RTP_FIXED_HEADER_SIZE + csrc_bytes + payload_.size();
    std::vector<uint8_t> buffer(total_len);

    // Byte 0: V(2), P(1), X(1), CC(4)
    buffer[0] = static_cast<uint8_t>((header_.version & 0x03) << 6);
    if (header_.padding)   buffer[0] |= 0x20;
    if (header_.extension) buffer[0] |= 0x10;
    buffer[0] |= static_cast<uint8_t>(header_.csrc_list.size() & 0x0F);

    // Byte 1: M(1), PT(7)
    buffer[1] = static_cast<uint8_t>(header_.payload_type & 0x7F);
    if (header_.marker) buffer[1] |= 0x80;

    // Bytes 2-3: Sequence Number (Network Byte Order)
    uint16_t net_seq = htons(header_.sequence_number);
    std::memcpy(&buffer[2], &net_seq, sizeof(net_seq));

    // Bytes 4-7: Timestamp (Network Byte Order)
    uint32_t net_ts = htonl(header_.timestamp);
    std::memcpy(&buffer[4], &net_ts, sizeof(net_ts));

    // Bytes 8-11: SSRC (Network Byte Order)
    uint32_t net_ssrc = htonl(header_.ssrc);
    std::memcpy(&buffer[8], &net_ssrc, sizeof(net_ssrc));

    // CSRC List
    size_t offset = RTP_FIXED_HEADER_SIZE;
    for (uint32_t csrc : header_.csrc_list) {
        uint32_t net_csrc = htonl(csrc);
        std::memcpy(&buffer[offset], &net_csrc, sizeof(net_csrc));
        offset += sizeof(net_csrc);
    }

    // Payload
    if (!payload_.empty()) {
        std::memcpy(&buffer[offset], payload_.data(), payload_.size());
    }

    return buffer;
}

std::optional<RtpPacket> RtpPacket::deserialize(const uint8_t* data, size_t length) {
    if (!data || length < RTP_FIXED_HEADER_SIZE) {
        return std::nullopt;
    }

    RtpHeader header;
    header.version = (data[0] >> 6) & 0x03;
    if (header.version != RTP_VERSION) {
        return std::nullopt; // RFC 3550 requires version 2
    }

    header.padding = (data[0] & 0x20) != 0;
    header.extension = (data[0] & 0x10) != 0;
    header.csrc_count = data[0] & 0x0F;

    header.marker = (data[1] & 0x80) != 0;
    header.payload_type = data[1] & 0x7F;

    uint16_t net_seq;
    std::memcpy(&net_seq, &data[2], sizeof(net_seq));
    header.sequence_number = ntohs(net_seq);

    uint32_t net_ts;
    std::memcpy(&net_ts, &data[4], sizeof(net_ts));
    header.timestamp = ntohl(net_ts);

    uint32_t net_ssrc;
    std::memcpy(&net_ssrc, &data[8], sizeof(net_ssrc));
    header.ssrc = ntohl(net_ssrc);

    size_t expected_header_len = RTP_FIXED_HEADER_SIZE + (header.csrc_count * sizeof(uint32_t));
    if (length < expected_header_len) {
        return std::nullopt;
    }

    header.csrc_list.reserve(header.csrc_count);
    size_t offset = RTP_FIXED_HEADER_SIZE;
    for (size_t i = 0; i < header.csrc_count; ++i) {
        uint32_t net_csrc;
        std::memcpy(&net_csrc, &data[offset], sizeof(net_csrc));
        header.csrc_list.push_back(ntohl(net_csrc));
        offset += sizeof(net_csrc);
    }

    size_t payload_len = length - offset;
    std::vector<uint8_t> payload;
    if (payload_len > 0) {
        payload.assign(data + offset, data + length);
    }

    return RtpPacket(header, std::move(payload));
}

std::optional<RtpPacket> RtpPacket::deserialize(const std::vector<uint8_t>& buffer) {
    return deserialize(buffer.data(), buffer.size());
}

// RTCP Receiver Report Serialization
// RTCP Header: V=2, P=0, RC=1 (report count), PT=201 (RR), length=7 (32-bit words - 1)
constexpr uint8_t RTCP_RR_PT = 201;

std::vector<uint8_t> RtcpReceiverReport::serialize() const {
    // 4 bytes header + 4 bytes ssrc_source + 24 bytes report block = 32 bytes (8 words)
    std::vector<uint8_t> buffer(32, 0);

    // Header: V=2, P=0, RC=1
    buffer[0] = 0x81; // V=2, P=0, RC=1
    buffer[1] = RTCP_RR_PT; // PT=201
    uint16_t length_words = htons(7); // 8 32-bit words - 1
    std::memcpy(&buffer[2], &length_words, 2);

    // SSRC of packet sender
    uint32_t net_src = htonl(ssrc_source);
    std::memcpy(&buffer[4], &net_src, 4);

    // Report Block
    // SSRC_1 (SSRC of source being reported)
    uint32_t net_sender = htonl(ssrc_sender);
    std::memcpy(&buffer[8], &net_sender, 4);

    // Fraction lost (8 bits) and Cumulative lost (24 bits)
    buffer[12] = fraction_lost;
    int32_t clamped_cum = cumulative_lost & 0x00FFFFFF;
    buffer[13] = static_cast<uint8_t>((clamped_cum >> 16) & 0xFF);
    buffer[14] = static_cast<uint8_t>((clamped_cum >> 8) & 0xFF);
    buffer[15] = static_cast<uint8_t>(clamped_cum & 0xFF);

    // Extended highest sequence number received
    uint32_t net_hseq = htonl(highest_seq_received);
    std::memcpy(&buffer[16], &net_hseq, 4);

    // Interarrival jitter
    uint32_t net_jitter = htonl(interarrival_jitter);
    std::memcpy(&buffer[20], &net_jitter, 4);

    // Last SR timestamp
    uint32_t net_lsr = htonl(last_sr_timestamp);
    std::memcpy(&buffer[24], &net_lsr, 4);

    // Delay since last SR (DLSR)
    uint32_t net_dlsr = htonl(delay_since_last_sr);
    std::memcpy(&buffer[28], &net_dlsr, 4);

    return buffer;
}

std::optional<RtcpReceiverReport> RtcpReceiverReport::deserialize(const uint8_t* data, size_t length) {
    if (!data || length < 32) return std::nullopt;

    uint8_t version = (data[0] >> 6) & 0x03;
    uint8_t pt = data[1];
    if (version != 2 || pt != RTCP_RR_PT) return std::nullopt;

    RtcpReceiverReport rr;
    uint32_t net_val;

    std::memcpy(&net_val, &data[4], 4);
    rr.ssrc_source = ntohl(net_val);

    std::memcpy(&net_val, &data[8], 4);
    rr.ssrc_sender = ntohl(net_val);

    rr.fraction_lost = data[12];
    int32_t cum = (static_cast<int32_t>(data[13]) << 16) |
                  (static_cast<int32_t>(data[14]) << 8) |
                  static_cast<int32_t>(data[15]);
    if (cum & 0x00800000) cum |= 0xFF000000; // Sign extend 24-bit to 32-bit
    rr.cumulative_lost = cum;

    std::memcpy(&net_val, &data[16], 4);
    rr.highest_seq_received = ntohl(net_val);

    std::memcpy(&net_val, &data[20], 4);
    rr.interarrival_jitter = ntohl(net_val);

    std::memcpy(&net_val, &data[24], 4);
    rr.last_sr_timestamp = ntohl(net_val);

    std::memcpy(&net_val, &data[28], 4);
    rr.delay_since_last_sr = ntohl(net_val);

    return rr;
}

} // namespace rtp
