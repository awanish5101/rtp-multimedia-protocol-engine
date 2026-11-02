#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <stdexcept>

namespace rtp {

// RFC 3550 RTP Fixed Header (12 octets)
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|X|  CC   |M|     PT      |       sequence number         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                           timestamp                           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           synchronization source (SSRC) identifier            |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |            contributing source (CSRC) identifiers             |
// |                             ....                              |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

constexpr uint8_t RTP_VERSION = 2;
constexpr size_t RTP_FIXED_HEADER_SIZE = 12;

enum class PayloadType : uint8_t {
    PCMU = 0,         // G.711 mu-law Audio (8 kHz)
    GSM = 3,          // GSM Audio (8 kHz)
    G723 = 4,         // G.723 Audio (8 kHz)
    L16_2CH = 10,     // 16-bit Linear PCM Stereo (44.1 kHz)
    PCMA = 8,         // G.711 A-law Audio (8 kHz)
    JPEG = 26,        // JPEG Video (90 kHz)
    H261 = 31,        // H.261 Video (90 kHz)
    MPV = 32,         // MPEG-1/2 Video (90 kHz)
    H263 = 34,        // H.263 Video (90 kHz)
    DYNAMIC_H264 = 96,// Dynamic H.264 Video (90 kHz)
    DYNAMIC_OPUS = 97 // Dynamic Opus Audio (48 kHz)
};

struct RtpHeader {
    uint8_t version{RTP_VERSION};   // 2 bits
    bool padding{false};            // 1 bit
    bool extension{false};          // 1 bit
    uint8_t csrc_count{0};          // 4 bits (CC)
    bool marker{false};             // 1 bit (M)
    uint8_t payload_type{0};        // 7 bits (PT)
    uint16_t sequence_number{0};    // 16 bits
    uint32_t timestamp{0};          // 32 bits
    uint32_t ssrc{0};               // 32 bits
    std::vector<uint32_t> csrc_list;// CC * 4 octets
};

class RtpPacket {
public:
    RtpPacket() = default;
    RtpPacket(const RtpHeader& header, std::vector<uint8_t> payload);

    // Serialization & Deserialization
    [[nodiscard]] std::vector<uint8_t> serialize() const;
    static std::optional<RtpPacket> deserialize(const uint8_t* data, size_t length);
    static std::optional<RtpPacket> deserialize(const std::vector<uint8_t>& buffer);

    // Accessors
    [[nodiscard]] const RtpHeader& header() const noexcept { return header_; }
    [[nodiscard]] RtpHeader& header() noexcept { return header_; }
    [[nodiscard]] const std::vector<uint8_t>& payload() const noexcept { return payload_; }
    [[nodiscard]] std::vector<uint8_t>& payload() noexcept { return payload_; }
    [[nodiscard]] size_t total_size() const noexcept;

    // Mutators
    void set_sequence_number(uint16_t seq) noexcept { header_.sequence_number = seq; }
    void set_timestamp(uint32_t ts) noexcept { header_.timestamp = ts; }
    void set_marker(bool m) noexcept { header_.marker = m; }
    void set_payload(std::vector<uint8_t> p) { payload_ = std::move(p); }

private:
    RtpHeader header_{};
    std::vector<uint8_t> payload_{};
};

// RTCP Receiver Report (RFC 3550 Section 6.4.2)
struct RtcpReceiverReport {
    uint32_t ssrc_source{0};        // Receiver SSRC
    uint32_t ssrc_sender{0};        // Sender SSRC being reported
    uint8_t fraction_lost{0};       // Fraction of packets lost since last report (8 bits)
    int32_t cumulative_lost{0};     // Cumulative packets lost (24 bits signed)
    uint32_t highest_seq_received{0};// Extended highest sequence number received
    uint32_t interarrival_jitter{0};// Statistical variance of packet inter-arrival time
    uint32_t last_sr_timestamp{0};  // Middle 32 bits of NTP timestamp of last SR
    uint32_t delay_since_last_sr{0};// Delay between receiving last SR and sending this RR

    [[nodiscard]] std::vector<uint8_t> serialize() const;
    static std::optional<RtcpReceiverReport> deserialize(const uint8_t* data, size_t length);
};

} // namespace rtp
