#include "rtp/jitter_buffer.hpp"
#include <cassert>
#include <iostream>

static rtp::RtpPacket make_packet(uint16_t seq, uint32_t ts) {
    rtp::RtpHeader h;
    h.version = 2;
    h.payload_type = 96;
    h.sequence_number = seq;
    h.timestamp = ts;
    h.ssrc = 0x1234;
    return rtp::RtpPacket(h, {static_cast<uint8_t>(seq & 0xFF)});
}

void test_jitter_buffer_reordering() {
    std::cout << "[RUN] test_jitter_buffer_reordering...\n";
    rtp::JitterBuffer jb(std::chrono::milliseconds(0)); // 0ms playout delay for immediate test popping

    auto now = std::chrono::steady_clock::now();

    // Push out of order: 100, 102, 101, 104, 103
    jb.push(make_packet(100, 1000), now);
    jb.push(make_packet(102, 3000), now);
    jb.push(make_packet(101, 2000), now);
    jb.push(make_packet(104, 5000), now);
    jb.push(make_packet(103, 4000), now);

    // Pop should produce strictly ordered sequence 100, 101, 102, 103, 104
    for (uint16_t expected_seq = 100; expected_seq <= 104; ++expected_seq) {
        auto pkt = jb.pop();
        assert(pkt.has_value());
        assert(pkt->header().sequence_number == expected_seq);
    }

    assert(!jb.pop().has_value());
    assert(jb.get_stats().packets_played == 5);
    std::cout << "[PASS] test_jitter_buffer_reordering\n";
}

void test_jitter_buffer_sequence_wraparound() {
    std::cout << "[RUN] test_jitter_buffer_sequence_wraparound...\n";
    rtp::JitterBuffer jb(std::chrono::milliseconds(0));

    auto now = std::chrono::steady_clock::now();

    // Test sequence 16-bit wrap around: 65534, 65535, 0, 1, 2
    // Push in scrambled order: 65535, 65534, 1, 0, 2
    jb.push(make_packet(65534, 10000), now);
    jb.push(make_packet(1, 40000), now);
    jb.push(make_packet(65535, 20000), now);
    jb.push(make_packet(2, 50000), now);
    jb.push(make_packet(0, 30000), now);

    std::vector<uint16_t> expected = {65534, 65535, 0, 1, 2};
    for (uint16_t exp_seq : expected) {
        auto pkt = jb.pop();
        assert(pkt.has_value());
        assert(pkt->header().sequence_number == exp_seq);
    }

    assert(!jb.pop().has_value());
    std::cout << "[PASS] test_jitter_buffer_sequence_wraparound\n";
}

void test_jitter_buffer_duplicates_and_late() {
    std::cout << "[RUN] test_jitter_buffer_duplicates_and_late...\n";
    rtp::JitterBuffer jb(std::chrono::milliseconds(0));

    auto now = std::chrono::steady_clock::now();
    assert(jb.push(make_packet(500, 1000), now));
    // Duplicate push of 500
    assert(!jb.push(make_packet(500, 1000), now));

    auto p1 = jb.pop();
    assert(p1.has_value() && p1->header().sequence_number == 500);

    // Now next expected is 501. Pushing 499 is late!
    assert(!jb.push(make_packet(499, 500), now));

    auto stats = jb.get_stats();
    assert(stats.packets_duplicate == 1);
    assert(stats.packets_dropped_late == 1);
    std::cout << "[PASS] test_jitter_buffer_duplicates_and_late\n";
}
