# Real-Time RTP/UDP Multimedia Protocol Engine & Jitter Buffer

A high-performance, real-time multimedia packet streaming and transport protocol engine written in modern **C++17**, implementing **RFC 3550 Real-Time Transport Protocol (RTP)** and **RTP Control Protocol (RTCP)** standards. Engineered for low-latency audio/video streaming, embedded network subsystems, and mobile chipset multimedia processing pipelines.

[![C++ CI / Build & Test](https://github.com/awanish5101/rtp-multimedia-protocol-engine/actions/workflows/ci.yml/badge.svg)](https://github.com/awanish5101/rtp-multimedia-protocol-engine/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

---

## Architecture Overview

```
+-----------------------------------------------------------------------------------+
|                           MULTIMEDIA STREAMING ENGINE                             |
+-----------------------------------------------------------------------------------+
       |                                                                ^
  send_frame()                                                 read_received_frame()
       |                                                                |
       v                                                                |
+----------------+                                             +--------------------+
|  RTP Packetizer| (RFC 3550 Header + Payload)                 | Thread-Safe Queue  |
+----------------+                                             | Circular RingBuffer|
       |                                                       +--------------------+
       v                                                                ^
+----------------+                                                      |
| POSIX UDP      |                                             +--------------------+
| Socket (Send)  |                                             | Playout Scheduler  |
+----------------+                                             +--------------------+
       |                                                                ^
   UDP/IP Network                                                       |
  (Jitter / Drops)                                             +--------------------+
       |                                                       | Adaptive Jitter    |
       v                                                       | Buffer (RFC 3550)  |
+----------------+                                             +--------------------+
| POSIX UDP      | (Non-blocking I/O + poll)                            ^
| Socket (Recv)  |------------------------------------------------------+
+----------------+
       |
       v
+----------------+
| RTCP Receiver  | (Loss fraction, cumulative drops, interarrival jitter telemetry)
| Reports (RR)   |
+----------------+
```

---

## Core Engineering Features

1. **RFC 3550 RTP/RTCP Specification Compliance:**
   - Bit-level fixed header serialization & parsing (Version 2, Marker bit, CSRC list, 7-bit Payload Type, 16-bit Sequence Number, 32-bit Timestamp, 32-bit SSRC).
   - Strict network byte order (`htons`, `ntohs`, `htonl`, `ntohl`) and memory bounds safety.
   - Generation and parsing of RTCP Receiver Reports (RR, PT=201) measuring packet loss fraction, cumulative packet drops, extended highest sequence number, and interarrival jitter.

2. **Adaptive Jitter Buffer & Packet Reordering:**
   - Sequence number reordering dealing with out-of-order UDP packet arrivals and duplicate packet suppression.
   - 16-bit modular sequence number arithmetic handling wrap-around transitions (`65535 -> 0`).
   - Playout deadline scheduling with late-packet drop detection.
   - Real-time RFC 3550 statistical interarrival jitter estimation:
     $$D(i, j) = (R_j - S_j) - (R_i - S_i)$$
     $$J(i) = J(i-1) + \frac{|D(i-1, i)| - J(i-1)}{16}$$

3. **High-Throughput Thread-Safe Circular Ring Buffer:**
   - Zero-copy circular buffer template for inter-thread streaming between network receiver thread and video/audio decoder thread.
   - Mutex and condition-variable synchronization with bounded queue depth and overflow telemetry.

4. **POSIX Networking & Platform I/O:**
   - Non-blocking UDP socket transport using `fcntl(O_NONBLOCK)` and `poll()`.
   - Socket buffer tuning (`SO_RCVBUF`, `SO_SNDBUF` set to 512 KB) for high-framerate packet streams.
   - Emulated packet loss injection for deterministic stress testing and recovery validation.

---

## Benchmark Results

Benchmarked on Apple Clang 17 / ARM64 with `-O3` optimization across 1,000,000 iterations:

| Metric | Throughput | Latency |
| :--- | :--- | :--- |
| **RTP Packet Serialization** | **17.58 M ops/sec** (2,883 MB/sec) | **56.9 ns/op** |
| **RTP Packet Deserialization** | **22.58 M ops/sec** (3,703 MB/sec) | **44.3 ns/op** |
| **Thread-Safe Ring Buffer (Push/Pop)** | **62.93 M ops/sec** | **15.9 ns/op** |

---

## Test Suite Coverage

The automated test runner validates all 9 test suites:
- `test_rtp_packet_serialization`: Validates RFC 3550 header fields, CSRC list, payload integrity.
- `test_rtcp_receiver_report`: Validates RTCP RR report serialization, loss calculation, jitter formatting.
- `test_invalid_packet_handling`: Confirms rejection of malformed packets, invalid lengths, version mismatches.
- `test_ring_buffer_basic`: Validates FIFO behavior, boundary enforcement, overflow handling.
- `test_ring_buffer_multithreaded`: Concurrency test with 1,000 items streamed between distinct threads.
- `test_jitter_buffer_reordering`: Scrambled packet order verification (100, 102, 101, 104, 103).
- `test_jitter_buffer_sequence_wraparound`: Verifies 16-bit sequence overflow handling (`65534 -> 65535 -> 0 -> 1 -> 2`).
- `test_jitter_buffer_duplicates_and_late`: Verifies duplicate suppression and late-packet rejection.
- `test_multimedia_streamer_loopback`: End-to-end loopback streaming over real POSIX UDP sockets.

---

## Build & Run

### Prerequisites
- Clang++ or G++ supporting C++17
- Make
- POSIX-compliant environment (Linux / macOS)

### Commands
```bash
# Build all targets (engine, test runner, benchmarks)
make all

# Run unit tests
make test

# Run performance benchmarks
make benchmark

# Run real-time streaming demo
make run
```

### Docker
```bash
docker build -t rtp-multimedia-protocol-engine .
docker run --rm -it rtp-multimedia-protocol-engine
```

---

## Author
**Awanish Chaurasiya**  
GitHub: [@awanish5101](https://github.com/awanish5101)  
LinkedIn: [awanish-chaurasiya](https://linkedin.com/in/awanish-chaurasiya)
