#include <iostream>

// Forward declarations of test functions
void test_rtp_packet_serialization();
void test_rtcp_receiver_report();
void test_invalid_packet_handling();

void test_ring_buffer_basic();
void test_ring_buffer_multithreaded();

void test_jitter_buffer_reordering();
void test_jitter_buffer_sequence_wraparound();
void test_jitter_buffer_duplicates_and_late();

void test_multimedia_streamer_loopback();

int main() {
    std::cout << "========================================\n";
    std::cout << "Running RTP Multimedia Engine Test Suite\n";
    std::cout << "========================================\n";

    try {
        test_rtp_packet_serialization();
        test_rtcp_receiver_report();
        test_invalid_packet_handling();

        test_ring_buffer_basic();
        test_ring_buffer_multithreaded();

        test_jitter_buffer_reordering();
        test_jitter_buffer_sequence_wraparound();
        test_jitter_buffer_duplicates_and_late();

        test_multimedia_streamer_loopback();

        std::cout << "========================================\n";
        std::cout << "ALL 9 TEST SUITES PASSED SUCCESSFULLY!\n";
        std::cout << "========================================\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
