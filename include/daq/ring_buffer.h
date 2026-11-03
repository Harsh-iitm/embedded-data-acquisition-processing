#ifndef DAQ_RING_BUFFER_H
#define DAQ_RING_BUFFER_H

#include <stdatomic.h>
#include <stdint.h>

#if ATOMIC_INT_LOCK_FREE != 2
#error "DAQ ring buffer requires always-lock-free unsigned int atomics"
#endif

typedef struct {
    uint8_t *storage;
    uint32_t capacity;
    _Atomic unsigned int head;
    _Atomic unsigned int tail;
} daq_ring_buffer_t;

/* Single producer / single consumer. Capacity must be in [2, 2^31 - 1]. */
int daq_ring_buffer_init(daq_ring_buffer_t *rb, uint8_t *storage,
                         uint32_t capacity);
int daq_ring_buffer_push(daq_ring_buffer_t *rb, uint8_t value);
int daq_ring_buffer_pop(daq_ring_buffer_t *rb, uint8_t *value);
uint32_t daq_ring_buffer_count(const daq_ring_buffer_t *rb);
uint32_t daq_ring_buffer_free(const daq_ring_buffer_t *rb);

#endif
