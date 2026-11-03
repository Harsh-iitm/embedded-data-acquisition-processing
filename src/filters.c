#include "daq/filters.h"

#include <string.h>

int daq_moving_average_init(daq_moving_average_t *filter, size_t window)
{
    if (filter == NULL || window == 0u ||
        window > DAQ_MOVING_AVERAGE_MAX_WINDOW) {
        return -1;
    }
    memset(filter, 0, sizeof(*filter));
    filter->window = window;
    return 0;
}

int32_t daq_moving_average_push(daq_moving_average_t *filter, int32_t sample)
{
    if (filter == NULL || filter->window == 0u) {
        return 0;
    }
    if (filter->count == filter->window) {
        filter->sum -= filter->samples[filter->cursor];
    } else {
        ++filter->count;
    }
    filter->samples[filter->cursor] = sample;
    filter->sum += sample;
    filter->cursor = (filter->cursor + 1u) % filter->window;
    return (int32_t)(filter->sum / (int64_t)filter->count);
}

int daq_fir_init(daq_fir_t *filter, const float *coefficients, size_t taps)
{
    if (filter == NULL || coefficients == NULL || taps == 0u ||
        taps > DAQ_FIR_MAX_TAPS) {
        return -1;
    }
    memset(filter, 0, sizeof(*filter));
    filter->coefficients = coefficients;
    filter->tap_count = taps;
    return 0;
}

float daq_fir_process(daq_fir_t *filter, float sample)
{
    if (filter == NULL || filter->coefficients == NULL ||
        filter->tap_count == 0u) {
        return 0.0f;
    }
    filter->delay[filter->cursor] = sample;
    float result = 0.0f;
    size_t index = filter->cursor;
    for (size_t tap = 0; tap < filter->tap_count; ++tap) {
        result += filter->coefficients[tap] * filter->delay[index];
        index = index == 0u ? filter->tap_count - 1u : index - 1u;
    }
    filter->cursor = (filter->cursor + 1u) % filter->tap_count;
    return result;
}

int daq_threshold_update(daq_threshold_t *detector, int32_t value)
{
    if (detector == NULL || detector->falling_threshold >
                                detector->rising_threshold) {
        return 0;
    }
    if (detector->active == 0u && value >= detector->rising_threshold) {
        detector->active = 1u;
        return 1;
    }
    if (detector->active != 0u && value <= detector->falling_threshold) {
        detector->active = 0u;
        return -1;
    }
    return 0;
}
