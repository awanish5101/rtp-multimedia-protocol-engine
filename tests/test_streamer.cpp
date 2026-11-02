#include "rtp/multimedia_streamer.hpp"
#include <cassert>
#include <iostream>
#include <thread>

void test_multimedia_streamer_loopback() {
    std::cout << "[RUN] test_multimedia_streamer_loopback...\n";

    rtp::StreamerConfig config;
    config.local_listen_port = 17890;
    config.target_endpoint = {"127.0.0.1", 17890};
    config.packet_interval = std::chrono::milliseconds(2);
    config.jitter_buffer_delay = std::chrono::milliseconds(5);
    config.sample_rate_hz = 90000;

    rtp::MultimediaStreamer streamer(config);
    bool started = streamer.start();
    assert(started);

    constexpr int FRAME_COUNT = 30;
    for (int i = 0; i < FRAME_COUNT; ++i) {
        std::vector<uint8_t> payload = {
            static_cast<uint8_t>(i),
            0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
        };
        bool sent = streamer.send_frame(payload, (i == FRAME_COUNT - 1));
        assert(sent);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    // Read back received frames from the ring buffer
    int frames_received = 0;
    for (int i = 0; i < FRAME_COUNT; ++i) {
        auto pkt = streamer.read_received_frame(std::chrono::milliseconds(100));
        if (pkt.has_value()) {
            frames_received++;
            assert(pkt->payload().size() == 7);
            assert(pkt->payload()[0] == static_cast<uint8_t>(frames_received - 1));
        }
    }

    assert(frames_received == FRAME_COUNT);

    auto qos = streamer.get_qos_report();
    assert(qos.packets_sent == FRAME_COUNT);
    assert(qos.packets_received == FRAME_COUNT);

    auto rtcp = streamer.generate_rtcp_report();
    assert(rtcp.ssrc_sender == config.ssrc);

    streamer.stop();
    std::cout << "[PASS] test_multimedia_streamer_loopback (All " << frames_received << " frames received in order)\n";
}
