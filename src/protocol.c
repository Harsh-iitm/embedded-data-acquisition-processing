#include "daq/protocol.h"

#include <string.h>

#define DAQ_CRC_INITIAL 0xFFFFu
#define DAQ_CRC_POLYNOMIAL 0x1021u

static uint16_t crc_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte << 8;
    for (unsigned bit = 0; bit < 8u; ++bit) {
        crc = (crc & 0x8000u) != 0u
                  ? (uint16_t)((crc << 1) ^ DAQ_CRC_POLYNOMIAL)
                  : (uint16_t)(crc << 1);
    }
    return crc;
}

uint16_t daq_crc16_ccitt_false(const uint8_t *data, size_t length)
{
    uint16_t crc = DAQ_CRC_INITIAL;
    if (data == NULL && length != 0u) {
        return 0u;
    }
    for (size_t i = 0; i < length; ++i) {
        crc = crc_update(crc, data[i]);
    }
    return crc;
}

void daq_parser_init(daq_parser_t *parser, daq_packet_callback_t callback,
                     void *user)
{
    if (parser == NULL) {
        return;
    }
    memset(parser, 0, sizeof(*parser));
    parser->callback = callback;
    parser->callback_user = user;
}

void daq_parser_reset(daq_parser_t *parser)
{
    if (parser != NULL) {
        parser->state = DAQ_PARSER_SYNC_0;
        parser->payload_index = 0u;
        parser->crc = DAQ_CRC_INITIAL;
    }
}

static void resync(daq_parser_t *parser, uint8_t byte)
{
    parser->state = byte == DAQ_PACKET_SYNC_0 ? DAQ_PARSER_SYNC_1
                                              : DAQ_PARSER_SYNC_0;
    parser->payload_index = 0u;
    parser->crc = DAQ_CRC_INITIAL;
}

void daq_parser_feed(daq_parser_t *parser, uint8_t byte)
{
    if (parser == NULL) {
        return;
    }
    switch (parser->state) {
    case DAQ_PARSER_SYNC_0:
        if (byte == DAQ_PACKET_SYNC_0) {
            parser->state = DAQ_PARSER_SYNC_1;
        } else {
            ++parser->stats.discarded_bytes;
        }
        break;
    case DAQ_PARSER_SYNC_1:
        if (byte == DAQ_PACKET_SYNC_1) {
            parser->state = DAQ_PARSER_LENGTH;
        } else {
            ++parser->stats.discarded_bytes;
            parser->state = byte == DAQ_PACKET_SYNC_0 ? DAQ_PARSER_SYNC_1
                                                      : DAQ_PARSER_SYNC_0;
        }
        break;
    case DAQ_PARSER_LENGTH:
        if (byte > DAQ_PACKET_MAX_PAYLOAD) {
            ++parser->stats.length_errors;
            resync(parser, byte);
        } else {
            parser->packet.length = byte;
            parser->payload_index = 0u;
            parser->crc = crc_update(DAQ_CRC_INITIAL, byte);
            parser->state = DAQ_PARSER_SEQUENCE;
        }
        break;
    case DAQ_PARSER_SEQUENCE:
        parser->packet.sequence = byte;
        parser->crc = crc_update(parser->crc, byte);
        parser->state = parser->packet.length == 0u ? DAQ_PARSER_CRC_LOW
                                                    : DAQ_PARSER_PAYLOAD;
        break;
    case DAQ_PARSER_PAYLOAD:
        parser->packet.payload[parser->payload_index++] = byte;
        parser->crc = crc_update(parser->crc, byte);
        if (parser->payload_index == parser->packet.length) {
            parser->state = DAQ_PARSER_CRC_LOW;
        }
        break;
    case DAQ_PARSER_CRC_LOW:
        parser->received_crc_low = byte;
        parser->state = DAQ_PARSER_CRC_HIGH;
        break;
    case DAQ_PARSER_CRC_HIGH: {
        uint16_t received = (uint16_t)parser->received_crc_low |
                            ((uint16_t)byte << 8);
        if (received == parser->crc) {
            ++parser->stats.packets_ok;
            if (parser->callback != NULL) {
                parser->callback(&parser->packet, parser->callback_user);
            }
            parser->state = DAQ_PARSER_SYNC_0;
        } else {
            ++parser->stats.crc_errors;
            resync(parser, byte);
        }
        break;
    }
    }
}

void daq_parser_feed_bytes(daq_parser_t *parser, const uint8_t *bytes,
                           size_t length)
{
    if (parser == NULL || (bytes == NULL && length != 0u)) {
        return;
    }
    for (size_t i = 0; i < length; ++i) {
        daq_parser_feed(parser, bytes[i]);
    }
}

static int16_t read_i16_le(const uint8_t *bytes)
{
    uint16_t raw = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    int32_t signed_value = (raw & 0x8000u) != 0u
                               ? (int32_t)raw - 0x10000L
                               : (int32_t)raw;
    return (int16_t)signed_value;
}

int daq_decode_imu(const daq_packet_t *packet, daq_imu_sample_t *sample)
{
    if (packet == NULL || sample == NULL ||
        packet->length != DAQ_IMU_PAYLOAD_SIZE) {
        return -1;
    }
    sample->sequence = packet->sequence;
    sample->accel_x = read_i16_le(&packet->payload[0]);
    sample->accel_y = read_i16_le(&packet->payload[2]);
    sample->accel_z = read_i16_le(&packet->payload[4]);
    sample->temperature = read_i16_le(&packet->payload[6]);
    sample->gyro_x = read_i16_le(&packet->payload[8]);
    sample->gyro_y = read_i16_le(&packet->payload[10]);
    sample->gyro_z = read_i16_le(&packet->payload[12]);
    return 0;
}
