#include "rtp/multimedia_streamer.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

int main() {
    std::cout << "===============================================================\n";
    std::cout << "   Qualcomm-Oriented Real-Time RTP/UDP Multimedia Protocol Engine\n";
    std::cout << "   RFC 3550 Real-Time Transport Protocol & Adaptive Jitter Buffer\n";
    std::cout << "===============================================================\n\n";

    rtp::StreamerConfig config;
    config.local_listen_port = 5004;
    config.target_endpoint = {"127.0.0.1", 5004};
    config.payload_type = static_cast<uint8_t>(rtp::PayloadType::DYNAMIC_H264);
    config.ssrc = 0x5101ABCD;
    config.packet_interval = std::chrono::milliseconds(10);
    config.jitter_buffer_delay = std::chrono::milliseconds(30);

    rtp::MultimediaStreamer streamer(config);

    std::cout << "[INIT] Binding POSIX UDP socket on 127.0.0.1:" << config.local_listen_port << "...\n";
    if (!streamer.start()) {
        std::cerr << "[ERROR] Failed to start multimedia streaming pipeline.\n";
        return 1;
    }
    std::cout << "[INIT] Streaming pipeline initialized (Worker thread active).\n\n";

    constexpr int TOTAL_FRAMES = 50;
    std::cout << "[STREAM] Transmitting " << TOTAL_FRAMES << " real-time H.264 video payload frames...\n";

    for (int i = 0; i < TOTAL_FRAMES; ++i) {
        // Simulated H.264 NAL Unit payload (e.g. 512 bytes per packet)
        std::vector<uint8_t> frame_data(256, static_cast<uint8_t>((i * 7) & 0xFF));
        frame_data[0] = 0x00; frame_data[1] = 0x00; frame_data[2] = 0x00; frame_data[3] = 0x01;
        frame_data[4] = 0x65; // IDR Slice header

        bool is_keyframe = (i % 25 == 0);
        streamer.send_frame(frame_data, is_keyframe);

        // Sleep to simulate 100 packets/sec streaming cadence
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[STREAM] Reading de-jittered, ordered frames from RingBuffer...\n";
    int received_count = 0;
    while (auto frame = streamer.read_received_frame(std::chrono::milliseconds(50))) {
        received_count++;
    }
    std::cout << "[STREAM] Successfully decoded " << received_count << " media frames.\n";

    auto qos = streamer.get_qos_report();
    auto rtcp = streamer.generate_rtcp_report();

    std::cout << "\n---------------------------------------------------------------\n";
    std::cout << "                 REAL-TIME STREAMING TELEMETRY (QoS)           \n";
    std::cout << "---------------------------------------------------------------\n";
    std::cout << "  Packets Sent:         " << qos.packets_sent << "\n";
    std::cout << "  Packets Received:     " << qos.packets_received << "\n";
    std::cout << "  Packets Played Out:   " << qos.packets_played << "\n";
    std::cout << "  Packets Lost:         " << qos.packets_lost << " (" << std::fixed << std::setprecision(2) << qos.loss_rate_percentage << "%)\n";
    std::cout << "  Interarrival Jitter:  " << std::fixed << std::setprecision(3) << qos.jitter_ms << " ms\n";
    std::cout << "  Throughput:           " << std::fixed << std::setprecision(2) << qos.throughput_kbps << " kbps\n";
    std::cout << "---------------------------------------------------------------\n";
    std::cout << "                 RTCP RECEIVER REPORT TELEMETRY                \n";
    std::cout << "---------------------------------------------------------------\n";
    std::cout << "  Sender SSRC:          0x" << std::hex << rtcp.ssrc_sender << std::dec << "\n";
    std::cout << "  Source SSRC:          0x" << std::hex << rtcp.ssrc_source << std::dec << "\n";
    std::cout << "  Fraction Lost:        " << static_cast<int>(rtcp.fraction_lost) << " / 256\n";
    std::cout << "  Cumulative Lost:      " << rtcp.cumulative_lost << "\n";
    std::cout << "  Highest Seq Received: " << rtcp.highest_seq_received << "\n";
    std::cout << "  Interarrival Jitter:  " << rtcp.interarrival_jitter << " (90kHz clock units)\n";
    std::cout << "===============================================================\n";

    streamer.stop();
    return 0;
}
