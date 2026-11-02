# Multi-stage Docker build for Real-Time Multimedia Protocol Engine
FROM alpine:3.20 AS builder

RUN apk add --no-cache clang make g++ musl-dev linux-headers

WORKDIR /app
COPY . .

RUN make clean && make test && make benchmark && make all

FROM alpine:3.20 AS runner
RUN apk add --no-cache libstdc++

WORKDIR /app
COPY --from=builder /app/bin/rtp_engine /app/bin/rtp_engine
COPY --from=builder /app/bin/test_runner /app/bin/test_runner
COPY --from=builder /app/bin/benchmark_throughput /app/bin/benchmark_throughput

EXPOSE 5004/udp

ENTRYPOINT ["/app/bin/rtp_engine"]
