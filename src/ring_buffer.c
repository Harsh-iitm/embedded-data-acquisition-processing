#include "daq/ring_buffer.h"

#include <limits.h>

_Static_assert(UINT_MAX == UINT32_MAX, "ring buffer atomics must be 32-bit");

int daq_ring_buffer_init(daq_ring_buffer_t *rb, uint8_t *storage,
                         uint32_t capacity)
{
    if (rb == NULL || storage == NULL || capacity < 2u ||
        capacity > INT32_MAX) {
        return -1;
    }
    rb->storage = storage;
    rb->capacity = capacity;
    atomic_init(&rb->head, 0u);
    atomic_init(&rb->tail, 0u);
    return 0;
}

int daq_ring_buffer_push(daq_ring_buffer_t *rb, uint8_t value)
{
    if (rb == NULL || rb->storage == NULL || rb->capacity < 2u) {
        return -1;
    }
    unsigned int head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    unsigned int tail = atomic_load_explicit(&rb->tail, memory_order_acquire);
    if ((uint32_t)(head - tail) >= rb->capacity) {
        return 0;
    }
    rb->storage[head % rb->capacity] = value;
    atomic_store_explicit(&rb->head, head + 1u, memory_order_release);
    return 1;
}

int daq_ring_buffer_pop(daq_ring_buffer_t *rb, uint8_t *value)
{
    if (rb == NULL || rb->storage == NULL || rb->capacity < 2u ||
        value == NULL) {
        return -1;
    }
    unsigned int tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    unsigned int head = atomic_load_explicit(&rb->head, memory_order_acquire);
    if (tail == head) {
        return 0;
    }
    *value = rb->storage[tail % rb->capacity];
    atomic_store_explicit(&rb->tail, tail + 1u, memory_order_release);
    return 1;
}

uint32_t daq_ring_buffer_count(const daq_ring_buffer_t *rb)
{
    if (rb == NULL || rb->storage == NULL || rb->capacity < 2u) {
        return 0u;
    }
    unsigned int head = atomic_load_explicit(&rb->head, memory_order_acquire);
    unsigned int tail = atomic_load_explicit(&rb->tail, memory_order_acquire);
    uint32_t count = (uint32_t)(head - tail);
    return count > rb->capacity ? rb->capacity : count;
}

uint32_t daq_ring_buffer_free(const daq_ring_buffer_t *rb)
{
    if (rb == NULL || rb->storage == NULL || rb->capacity < 2u) {
        return 0u;
    }
    return rb->capacity - daq_ring_buffer_count(rb);
}
