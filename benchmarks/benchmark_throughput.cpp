#include "rtp/packet.hpp"
#include "rtp/ring_buffer.hpp"
#include "rtp/jitter_buffer.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>

int main() {
    std::cout << "========================================================\n";
    std::cout << "RTP Protocol & Multimedia Engine Performance Benchmarks\n";
    std::cout << "========================================================\n";

    constexpr size_t ITERATIONS = 1000000;

    // 1. Packet Serialization Benchmark
    {
        rtp::RtpHeader header;
        header.version = 2;
        header.payload_type = 96;
        header.sequence_number = 100;
        header.timestamp = 90000;
        header.ssrc = 0xDEADBEEF;
        std::vector<uint8_t> payload(160, 0x55); // 160 bytes audio payload
        rtp::RtpPacket packet(header, payload);

        auto start = std::chrono::high_resolution_clock::now();
        size_t total_bytes = 0;
        for (size_t i = 0; i < ITERATIONS; ++i) {
            auto ser = packet.serialize();
            total_bytes += ser.size();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        double ops_per_sec = (static_cast<double>(ITERATIONS) * 1e9) / duration_ns;
        double ns_per_op = static_cast<double>(duration_ns) / ITERATIONS;
        double mb_per_sec = (static_cast<double>(total_bytes) / (1024 * 1024)) / (duration_ns / 1e9);

        std::cout << "1. RTP Packet Serialization:\n";
        std::cout << "   - Operations: " << ITERATIONS << "\n";
        std::cout << "   - Throughput: " << std::fixed << std::setprecision(2) << (ops_per_sec / 1e6) << " M ops/sec (" << mb_per_sec << " MB/sec)\n";
        std::cout << "   - Latency:    " << std::fixed << std::setprecision(1) << ns_per_op << " ns/op\n\n";
    }

    // 2. Packet Deserialization Benchmark
    {
        rtp::RtpHeader header;
        header.version = 2;
        header.payload_type = 96;
        header.sequence_number = 100;
        header.timestamp = 90000;
        header.ssrc = 0xDEADBEEF;
        std::vector<uint8_t> payload(160, 0x55);
        rtp::RtpPacket packet(header, payload);
        std::vector<uint8_t> ser = packet.serialize();

        auto start = std::chrono::high_resolution_clock::now();
        size_t successful_deser = 0;
        for (size_t i = 0; i < ITERATIONS; ++i) {
            auto res = rtp::RtpPacket::deserialize(ser.data(), ser.size());
            if (res) successful_deser++;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        double ops_per_sec = (static_cast<double>(ITERATIONS) * 1e9) / duration_ns;
        double ns_per_op = static_cast<double>(duration_ns) / ITERATIONS;
        double mb_per_sec = (static_cast<double>(ser.size() * ITERATIONS) / (1024 * 1024)) / (duration_ns / 1e9);

        std::cout << "2. RTP Packet Deserialization:\n";
        std::cout << "   - Operations: " << ITERATIONS << " (Verified: " << successful_deser << ")\n";
        std::cout << "   - Throughput: " << std::fixed << std::setprecision(2) << (ops_per_sec / 1e6) << " M ops/sec (" << mb_per_sec << " MB/sec)\n";
        std::cout << "   - Latency:    " << std::fixed << std::setprecision(1) << ns_per_op << " ns/op\n\n";
    }

    // 3. ThreadSafe RingBuffer Push/Pop Benchmark
    {
        rtp::ThreadSafeRingBuffer<int> ring_buffer(1024);
        constexpr size_t RB_ITERATIONS = 500000;
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < RB_ITERATIONS; ++i) {
            ring_buffer.push(static_cast<int>(i));
            ring_buffer.pop();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        double ops_per_sec = (static_cast<double>(RB_ITERATIONS * 2) * 1e9) / duration_ns;
        double ns_per_op = static_cast<double>(duration_ns) / (RB_ITERATIONS * 2);

        std::cout << "3. Thread-Safe Ring Buffer (Push + Pop):\n";
        std::cout << "   - Operations: " << (RB_ITERATIONS * 2) << "\n";
        std::cout << "   - Throughput: " << std::fixed << std::setprecision(2) << (ops_per_sec / 1e6) << " M ops/sec\n";
        std::cout << "   - Latency:    " << std::fixed << std::setprecision(1) << ns_per_op << " ns/op\n\n";
    }

    std::cout << "========================================================\n";
    std::cout << "Benchmark Suite Completed Successfully.\n";
    std::cout << "========================================================\n";
    return 0;
}
