#include "rtp/ring_buffer.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

void test_ring_buffer_basic() {
    std::cout << "[RUN] test_ring_buffer_basic...\n";
    rtp::ThreadSafeRingBuffer<int> rb(4);
    assert(rb.empty());
    assert(rb.size() == 0);
    assert(rb.capacity() == 4);

    assert(rb.push(10));
    assert(rb.push(20));
    assert(rb.push(30));
    assert(rb.push(40));
    assert(rb.size() == 4);

    // Should reject 5th element because capacity is 4
    assert(!rb.push(50));
    assert(rb.overflow_count() == 1);

    auto val1 = rb.pop();
    assert(val1.has_value() && *val1 == 10);
    auto val2 = rb.pop();
    assert(val2.has_value() && *val2 == 20);

    assert(rb.push(50));
    assert(rb.size() == 3);

    auto val3 = rb.pop();
    assert(val3.has_value() && *val3 == 30);
    auto val4 = rb.pop();
    assert(val4.has_value() && *val4 == 40);
    auto val5 = rb.pop();
    assert(val5.has_value() && *val5 == 50);

    assert(rb.empty());
    std::cout << "[PASS] test_ring_buffer_basic\n";
}

void test_ring_buffer_multithreaded() {
    std::cout << "[RUN] test_ring_buffer_multithreaded...\n";
    rtp::ThreadSafeRingBuffer<int> rb(64);
    constexpr int ITEM_COUNT = 1000;

    std::thread producer([&]() {
        for (int i = 0; i < ITEM_COUNT; ++i) {
            while (!rb.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::vector<int> consumed;
    consumed.reserve(ITEM_COUNT);

    std::thread consumer([&]() {
        for (int i = 0; i < ITEM_COUNT; ++i) {
            auto val = rb.pop();
            if (val.has_value()) {
                consumed.push_back(*val);
            }
        }
    });

    producer.join();
    consumer.join();

    assert(consumed.size() == ITEM_COUNT);
    for (int i = 0; i < ITEM_COUNT; ++i) {
        assert(consumed[i] == i);
    }

    std::cout << "[PASS] test_ring_buffer_multithreaded\n";
}
