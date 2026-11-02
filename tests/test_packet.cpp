#include "rtp/packet.hpp"
#include <cassert>
#include <iostream>

void test_rtp_packet_serialization() {
    std::cout << "[RUN] test_rtp_packet_serialization...\n";
    rtp::RtpHeader header;
    header.version = 2;
    header.padding = false;
    header.extension = false;
    header.csrc_count = 1;
    header.marker = true;
    header.payload_type = static_cast<uint8_t>(rtp::PayloadType::DYNAMIC_H264);
    header.sequence_number = 12345;
    header.timestamp = 987654321;
    header.ssrc = 0xAABBCCDD;
    header.csrc_list = {0x11223344};

    std::vector<uint8_t> payload = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1E}; // H.264 SPS NAL
    rtp::RtpPacket packet(header, payload);

    std::vector<uint8_t> serialized = packet.serialize();
    assert(serialized.size() == rtp::RTP_FIXED_HEADER_SIZE + 4 + payload.size());

    auto deserialized_opt = rtp::RtpPacket::deserialize(serialized);
    assert(deserialized_opt.has_value());

    const auto& deserialized = *deserialized_opt;
    assert(deserialized.header().version == 2);
    assert(deserialized.header().marker == true);
    assert(deserialized.header().payload_type == static_cast<uint8_t>(rtp::PayloadType::DYNAMIC_H264));
    assert(deserialized.header().sequence_number == 12345);
    assert(deserialized.header().timestamp == 987654321);
    assert(deserialized.header().ssrc == 0xAABBCCDD);
    assert(deserialized.header().csrc_list.size() == 1);
    assert(deserialized.header().csrc_list[0] == 0x11223344);
    assert(deserialized.payload() == payload);

    std::cout << "[PASS] test_rtp_packet_serialization\n";
}

void test_rtcp_receiver_report() {
    std::cout << "[RUN] test_rtcp_receiver_report...\n";
    rtp::RtcpReceiverReport rr;
    rr.ssrc_source = 0x12345678;
    rr.ssrc_sender = 0x87654321;
    rr.fraction_lost = 5;
    rr.cumulative_lost = 120;
    rr.highest_seq_received = 65500;
    rr.interarrival_jitter = 350;
    rr.last_sr_timestamp = 0xABCD1234;
    rr.delay_since_last_sr = 250;

    auto serialized = rr.serialize();
    assert(serialized.size() == 32);

    auto deserialized_opt = rtp::RtcpReceiverReport::deserialize(serialized.data(), serialized.size());
    assert(deserialized_opt.has_value());

    const auto& d = *deserialized_opt;
    assert(d.ssrc_source == 0x12345678);
    assert(d.ssrc_sender == 0x87654321);
    assert(d.fraction_lost == 5);
    assert(d.cumulative_lost == 120);
    assert(d.highest_seq_received == 65500);
    assert(d.interarrival_jitter == 350);
    assert(d.last_sr_timestamp == 0xABCD1234);
    assert(d.delay_since_last_sr == 250);

    std::cout << "[PASS] test_rtcp_receiver_report\n";
}

void test_invalid_packet_handling() {
    std::cout << "[RUN] test_invalid_packet_handling...\n";
    std::vector<uint8_t> corrupted_short = {0x80, 0x60}; // Only 2 bytes (needs >= 12)
    auto res1 = rtp::RtpPacket::deserialize(corrupted_short);
    assert(!res1.has_value());

    std::vector<uint8_t> wrong_version(16, 0);
    wrong_version[0] = 0x40; // Version 1 instead of 2
    auto res2 = rtp::RtpPacket::deserialize(wrong_version);
    assert(!res2.has_value());

    std::cout << "[PASS] test_invalid_packet_handling\n";
}
