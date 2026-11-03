#ifndef DAQ_PROTOCOL_H
#define DAQ_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define DAQ_PACKET_SYNC_0 0xA5u
#define DAQ_PACKET_SYNC_1 0x5Au
#define DAQ_PACKET_MAX_PAYLOAD 64u
#define DAQ_IMU_PAYLOAD_SIZE 14u

typedef struct {
    uint8_t sequence;
    uint8_t length;
    uint8_t payload[DAQ_PACKET_MAX_PAYLOAD];
} daq_packet_t;

typedef struct {
    uint32_t packets_ok;
    uint32_t crc_errors;
    uint32_t length_errors;
    uint32_t discarded_bytes;
} daq_parser_stats_t;

typedef void (*daq_packet_callback_t)(const daq_packet_t *packet, void *user);

typedef enum {
    DAQ_PARSER_SYNC_0,
    DAQ_PARSER_SYNC_1,
    DAQ_PARSER_LENGTH,
    DAQ_PARSER_SEQUENCE,
    DAQ_PARSER_PAYLOAD,
    DAQ_PARSER_CRC_LOW,
    DAQ_PARSER_CRC_HIGH
} daq_parser_state_t;

typedef struct {
    daq_parser_state_t state;
    daq_packet_t packet;
    uint8_t payload_index;
    uint16_t crc;
    uint8_t received_crc_low;
    daq_parser_stats_t stats;
    daq_packet_callback_t callback;
    void *callback_user;
} daq_parser_t;

void daq_parser_init(daq_parser_t *parser, daq_packet_callback_t callback,
                     void *user);
void daq_parser_reset(daq_parser_t *parser);
void daq_parser_feed(daq_parser_t *parser, uint8_t byte);
void daq_parser_feed_bytes(daq_parser_t *parser, const uint8_t *bytes,
                           size_t length);
uint16_t daq_crc16_ccitt_false(const uint8_t *data, size_t length);

typedef struct {
    uint8_t sequence;
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temperature;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} daq_imu_sample_t;

/* Decodes the 14-byte payload layout: ax, ay, az, temperature, gx, gy, gz. */
int daq_decode_imu(const daq_packet_t *packet, daq_imu_sample_t *sample);

#endif
