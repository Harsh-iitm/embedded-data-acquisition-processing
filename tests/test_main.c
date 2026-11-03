#include "daq/filters.h"
#include "daq/protocol.h"
#include "daq/ring_buffer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned calls;
    daq_packet_t last;
} packet_capture_t;

static void capture_packet(const daq_packet_t *packet, void *user)
{
    packet_capture_t *capture = user;
    ++capture->calls;
    capture->last = *packet;
}

static size_t make_packet(uint8_t *out, uint8_t sequence,
                          const uint8_t *payload, uint8_t length)
{
    out[0] = DAQ_PACKET_SYNC_0;
    out[1] = DAQ_PACKET_SYNC_1;
    out[2] = length;
    out[3] = sequence;
    if (length != 0u) {
        memcpy(&out[4], payload, length);
    }
    uint16_t crc = daq_crc16_ccitt_false(&out[2], (size_t)length + 2u);
    out[4u + length] = (uint8_t)(crc & 0xFFu);
    out[5u + length] = (uint8_t)(crc >> 8);
    return (size_t)length + 6u;
}

static void test_ring_buffer(void)
{
    uint8_t storage[3];
    uint8_t value = 0u;
    daq_ring_buffer_t rb;
    assert(daq_ring_buffer_init(&rb, storage, sizeof(storage)) == 0);
    assert(daq_ring_buffer_count(&rb) == 0u);
    assert(daq_ring_buffer_free(&rb) == sizeof(storage));
    assert(daq_ring_buffer_pop(&rb, &value) == 0);
    assert(daq_ring_buffer_push(&rb, 10u) == 1);
    assert(daq_ring_buffer_push(&rb, 11u) == 1);
    assert(daq_ring_buffer_push(&rb, 12u) == 1);
    assert(daq_ring_buffer_push(&rb, 13u) == 0);
    assert(daq_ring_buffer_pop(&rb, &value) == 1 && value == 10u);
    assert(daq_ring_buffer_push(&rb, 13u) == 1);
    assert(daq_ring_buffer_pop(&rb, &value) == 1 && value == 11u);
    assert(daq_ring_buffer_pop(&rb, &value) == 1 && value == 12u);
    assert(daq_ring_buffer_pop(&rb, &value) == 1 && value == 13u);
    assert(daq_ring_buffer_pop(&rb, &value) == 0);
    assert(daq_ring_buffer_init(&rb, storage, 1u) == -1);
}

static void test_parser_and_crc(void)
{
    static const uint8_t check[] = "123456789";
    const uint8_t payload[] = {0x01u, 0xA5u, 0x5Au, 0x7Fu};
    uint8_t frame[DAQ_PACKET_MAX_PAYLOAD + 6u];
    uint8_t stream[2u * (sizeof(payload) + 6u) + 4u];
    packet_capture_t capture = {0};
    daq_parser_t parser;
    size_t frame_size = make_packet(frame, 42u, payload, sizeof(payload));

    assert(daq_crc16_ccitt_false(check, sizeof(check) - 1u) == 0x29B1u);
    daq_parser_init(&parser, capture_packet, &capture);
    stream[0] = 0x00u;
    stream[1] = 0xFFu;
    memcpy(&stream[2], frame, frame_size);
    daq_parser_feed_bytes(&parser, stream, frame_size + 2u);
    assert(capture.calls == 1u);
    assert(capture.last.sequence == 42u);
    assert(capture.last.length == sizeof(payload));
    assert(memcmp(capture.last.payload, payload, sizeof(payload)) == 0);
    assert(parser.stats.packets_ok == 1u);
    assert(parser.stats.discarded_bytes == 2u);

    memcpy(stream, frame, frame_size);
    stream[frame_size - 1u] ^= 0x80u;
    memcpy(&stream[frame_size], frame, frame_size);
    daq_parser_feed_bytes(&parser, stream, frame_size * 2u);
    assert(capture.calls == 2u);
    assert(parser.stats.crc_errors == 1u);
    assert(parser.stats.packets_ok == 2u);

    /* An oversized frame is rejected and the next valid sync is found. */
    stream[0] = DAQ_PACKET_SYNC_0;
    stream[1] = DAQ_PACKET_SYNC_1;
    stream[2] = DAQ_PACKET_MAX_PAYLOAD + 1u;
    memcpy(&stream[3], frame, frame_size);
    daq_parser_feed_bytes(&parser, stream, frame_size + 3u);
    assert(capture.calls == 3u);
    assert(parser.stats.length_errors == 1u);
}

static void test_imu_decode(void)
{
    daq_packet_t packet = {.sequence = 7u, .length = DAQ_IMU_PAYLOAD_SIZE};
    daq_imu_sample_t sample;
    const uint16_t words[7] = {0x1234u, 0xFFFEu, 0x8000u, 0x0001u,
                               0x7FFFu, 0xABCDu, 0x0000u};
    for (size_t i = 0; i < 7u; ++i) {
        packet.payload[i * 2u] = (uint8_t)words[i];
        packet.payload[i * 2u + 1u] = (uint8_t)(words[i] >> 8);
    }
    assert(daq_decode_imu(&packet, &sample) == 0);
    assert(sample.sequence == 7u);
    assert(sample.accel_x == 0x1234);
    assert(sample.accel_y == -2);
    assert(sample.accel_z == INT16_MIN);
    assert(sample.temperature == 1);
    assert(sample.gyro_x == INT16_MAX);
    assert(sample.gyro_y == (int16_t)0xABCDu);
    packet.length = 13u;
    assert(daq_decode_imu(&packet, &sample) == -1);
}

static void test_filters(void)
{
    daq_moving_average_t average;
    const float coefficients[] = {0.5f, 0.5f};
    daq_fir_t fir;
    daq_threshold_t detector = {.rising_threshold = 10,
                                .falling_threshold = 5,
                                .active = 0u};
    assert(daq_moving_average_init(&average, 3u) == 0);
    assert(daq_moving_average_push(&average, 1) == 1);
    assert(daq_moving_average_push(&average, 3) == 2);
    assert(daq_moving_average_push(&average, 5) == 3);
    assert(daq_moving_average_push(&average, 7) == 5);
    assert(daq_moving_average_init(&average, 0u) == -1);

    assert(daq_fir_init(&fir, coefficients, 2u) == 0);
    assert(daq_fir_process(&fir, 2.0f) == 1.0f);
    assert(daq_fir_process(&fir, 4.0f) == 3.0f);
    assert(daq_fir_process(&fir, 6.0f) == 5.0f);
    assert(daq_fir_init(&fir, coefficients, DAQ_FIR_MAX_TAPS + 1u) == -1);

    assert(daq_threshold_update(&detector, 9) == 0);
    assert(daq_threshold_update(&detector, 10) == 1);
    assert(daq_threshold_update(&detector, 7) == 0);
    assert(daq_threshold_update(&detector, 5) == -1);
}

int main(void)
{
    test_ring_buffer();
    test_parser_and_crc();
    test_imu_decode();
    test_filters();
    puts("All DAQ library tests passed.");
    return 0;
}
