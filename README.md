# Embedded Data Acquisition and Processing Library

A fixed-memory C library for receiving sensor packets from an STM32 over UART, validating and decoding them on a Linux host, and processing live IMU samples. The same parser, filters, and ring buffer can be used in an STM32 firmware build with a C11 toolchain.

## Features

- Single-producer/single-consumer byte ring buffer using always-lock-free 32-bit C11 atomics.
- Incremental packet parser with sync recovery, payload bounds checking, sequence field, and CRC-16/CCITT-FALSE validation.
- IMU payload decoder for three accelerometer axes, temperature, and three gyroscope axes.
- Fixed-size moving average and FIR filters plus a threshold detector with hysteresis.
- Linux UART capture program that parses packets, filters Z-axis acceleration, reports threshold transitions, and prints parser counters.
- Unit tests and a Makefile with test, host, sanitizer, debug, and Valgrind targets.

No heap allocation is used by the ring buffer, parser, filters, or processing path. Callbacks receive a pointer to parser-owned packet storage, which is valid until the next completed packet. Copy the packet in the callback if it must be retained.

## UART packet format

All multibyte fields are little-endian. CRC is CRC-16/CCITT-FALSE (polynomial `0x1021`, initial value `0xFFFF`, no final XOR) over the length, sequence, and payload bytes.

| Field | Size | Description |
| --- | ---: | --- |
| Sync | 2 bytes | `A5 5A` |
| Payload length | 1 byte | `0` through `64` |
| Sequence | 1 byte | Wrapping packet sequence number |
| Payload | Variable | Application data |
| CRC | 2 bytes | Low byte first |

The supplied IMU decoder expects a 14-byte payload containing signed 16-bit values in this order: accelerometer X/Y/Z, temperature, gyroscope X/Y/Z. Values remain in sensor-native raw units; apply sensor-specific scale factors in the application.

## Build and run on Linux

Install a C11 compiler and GNU Make. Build the unit-test executable and run it with:

```sh
make test
```

Build the UART capture utility with:

```sh
make host
./build/uart_capture /dev/ttyUSB0 115200
```

The port uses 8-N-1, no flow control, and defaults to 115200 baud. Supported rates are 9600, 19200, 38400, 57600, 115200, and 230400 when provided by the platform. The capture output is CSV on stdout; parser counters are printed to stderr when the program exits. Stop it with Ctrl+C.

## STM32 integration

Provide caller-owned storage and initialize the SPSC ring buffer once. The UART RX ISR is the producer and pushes each received byte; a task or main loop is the consumer and feeds bytes into the parser. Do not call `push` from multiple producers or `pop` from multiple consumers. The ring buffer compile-time check requires the target compiler to implement always-lock-free unsigned integer atomics.

```c
static uint8_t uart_rx_storage[256];
static daq_ring_buffer_t uart_rx;
static daq_parser_t parser;

void app_init(void)
{
    (void)daq_ring_buffer_init(&uart_rx, uart_rx_storage,
                               sizeof uart_rx_storage);
    daq_parser_init(&parser, on_packet, NULL);
}

/* Called by the UART receive ISR. */
void uart_rx_byte_isr(uint8_t byte)
{
    if (!daq_ring_buffer_push(&uart_rx, byte)) {
        /* Count or signal an overrun; never block in the ISR. */
    }
}

/* Called by the foreground task. */
void process_uart_bytes(void)
{
    uint8_t byte;
    while (daq_ring_buffer_pop(&uart_rx, &byte)) {
        daq_parser_feed(&parser, byte);
    }
}
```

On UART idle timeout or a known transport reset, call `daq_parser_reset` to discard a partial frame while preserving parser statistics. A completed frame with an invalid CRC is rejected and the parser continues scanning for the next sync sequence.

## Filters

- Moving average: window size 1–64; uses a rolling sum and fixed storage.
- FIR: up to 32 caller-supplied coefficients; coefficient storage remains owned by the caller.
- Threshold detector: rising threshold activates an event; a lower falling threshold clears it to provide hysteresis.

All filter state is caller-owned and allocation-free. Validate return codes from each initializer before using its state.

## Verification

- Unit test suite: passed, including ring-buffer wrap/full/empty cases, the CRC reference vector, valid/corrupt/oversized packet handling, IMU decoding, filter outputs, and threshold hysteresis.
- Valgrind: zero memory leaks reported.
- GDB: used to debug packet-parser edge cases, including corrupted and dropped serial data.

The parser rejects corrupt frames and resumes synchronization without crashing. For a transport gap that leaves a partial frame, call `daq_parser_reset` when the UART idle timeout or link reset is detected.

Re-run the checks with:

```sh
make test
make sanitize
make debug       # Then: gdb ./build/test_daq
make valgrind
```

## Public API

Headers are under `include/daq/`:

- `ring_buffer.h`
- `protocol.h`
- `filters.h`

## License

MIT. See [LICENSE](LICENSE).
