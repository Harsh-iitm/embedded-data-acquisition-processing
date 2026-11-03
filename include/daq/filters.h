#ifndef DAQ_FILTERS_H
#define DAQ_FILTERS_H

#include <stddef.h>
#include <stdint.h>

#define DAQ_MOVING_AVERAGE_MAX_WINDOW 64u
#define DAQ_FIR_MAX_TAPS 32u

typedef struct {
    int32_t samples[DAQ_MOVING_AVERAGE_MAX_WINDOW];
    int64_t sum;
    size_t window;
    size_t count;
    size_t cursor;
} daq_moving_average_t;

int daq_moving_average_init(daq_moving_average_t *filter, size_t window);
int32_t daq_moving_average_push(daq_moving_average_t *filter, int32_t sample);

typedef struct {
    float delay[DAQ_FIR_MAX_TAPS];
    const float *coefficients;
    size_t tap_count;
    size_t cursor;
} daq_fir_t;

/* Coefficients are caller-owned and must outlive the filter. */
int daq_fir_init(daq_fir_t *filter, const float *coefficients, size_t taps);
float daq_fir_process(daq_fir_t *filter, float sample);

typedef struct {
    int32_t rising_threshold;
    int32_t falling_threshold;
    uint8_t active;
} daq_threshold_t;

/* Returns +1 on rising event, -1 on falling event, and 0 otherwise. */
int daq_threshold_update(daq_threshold_t *detector, int32_t value);

#endif
