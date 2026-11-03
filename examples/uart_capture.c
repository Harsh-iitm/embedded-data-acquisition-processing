#define _DEFAULT_SOURCE
#include "daq/filters.h"
#include "daq/protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static volatile sig_atomic_t keep_running = 1;

static void stop_capture(int signal_number)
{
    (void)signal_number;
    keep_running = 0;
}

static speed_t baud_constant(long baud)
{
    switch (baud) {
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
#ifdef B230400
    case 230400: return B230400;
#endif
    default: return (speed_t)0;
    }
}

static void print_sample(const daq_packet_t *packet, void *user)
{
    daq_moving_average_t *z_average = user;
    daq_imu_sample_t sample;
    static daq_threshold_t event = {.rising_threshold = 12000,
                                    .falling_threshold = 10000,
                                    .active = 0u};
    if (daq_decode_imu(packet, &sample) != 0) {
        return;
    }
    int32_t filtered_z = daq_moving_average_push(z_average, sample.accel_z);
    int edge = daq_threshold_update(&event, filtered_z < 0 ? -filtered_z
                                                           : filtered_z);
    const char *label = edge > 0 ? "RISE" : edge < 0 ? "FALL" : "";
    printf("%u,%d,%d,%d,%d,%d,%d,%d,%d,%s\n", (unsigned)sample.sequence,
           sample.accel_x, sample.accel_y, sample.accel_z, sample.gyro_x,
           sample.gyro_y, sample.gyro_z, sample.temperature, filtered_z, label);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s /dev/ttyUSB0 [baud]\n", argv[0]);
        return EXIT_FAILURE;
    }
    long baud = argc == 3 ? strtol(argv[2], NULL, 10) : 115200L;
    speed_t speed = baud_constant(baud);
    if (speed == (speed_t)0) {
        fprintf(stderr, "Unsupported baud rate: %ld\n", baud);
        return EXIT_FAILURE;
    }

    int fd = open(argv[1], O_RDONLY | O_NOCTTY);
    if (fd < 0) {
        perror("open serial port");
        return EXIT_FAILURE;
    }
    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        perror("tcgetattr");
        close(fd);
        return EXIT_FAILURE;
    }
    cfmakeraw(&options);
    options.c_cflag &= (tcflag_t)~(PARENB | CSTOPB | CSIZE);
    options.c_cflag |= CS8 | CLOCAL | CREAD;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;
    if (cfsetispeed(&options, speed) != 0 ||
        cfsetospeed(&options, speed) != 0 || tcsetattr(fd, TCSANOW, &options) != 0) {
        perror("configure serial port");
        close(fd);
        return EXIT_FAILURE;
    }

    daq_parser_t parser;
    daq_moving_average_t z_average;
    if (daq_moving_average_init(&z_average, 8u) != 0) {
        close(fd);
        return EXIT_FAILURE;
    }
    daq_parser_init(&parser, print_sample, &z_average);
    (void)signal(SIGINT, stop_capture);
    (void)signal(SIGTERM, stop_capture);
    puts("sequence,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,temp,avg_accel_z,event");

    uint8_t bytes[128];
    while (keep_running != 0) {
        ssize_t received = read(fd, bytes, sizeof(bytes));
        if (received > 0) {
            daq_parser_feed_bytes(&parser, bytes, (size_t)received);
        } else if (received < 0 && errno != EINTR) {
            perror("read serial port");
            break;
        }
    }
    close(fd);
    fprintf(stderr, "packets=%u crc_errors=%u length_errors=%u discarded=%u\n",
            (unsigned)parser.stats.packets_ok,
            (unsigned)parser.stats.crc_errors,
            (unsigned)parser.stats.length_errors,
            (unsigned)parser.stats.discarded_bytes);
    return EXIT_SUCCESS;
}
