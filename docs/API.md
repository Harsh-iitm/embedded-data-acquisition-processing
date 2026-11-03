# API Notes

All APIs use caller-owned state. No library function allocates memory. Headers provide the declarations; this page documents return values and lifetime rules.

## Ring buffer

```c
int daq_ring_buffer_init(daq_ring_buffer_t *rb, uint8_t *storage,
                         uint32_t capacity);
int daq_ring_buffer_push(daq_ring_buffer_t *rb, uint8_t value);
int daq_ring_buffer_pop(daq_ring_buffer_t *rb, uint8_t *value);
uint32_t daq_ring_buffer_count(const daq_ring_buffer_t *rb);
uint32_t daq_ring_buffer_free(const daq_ring_buffer_t *rb);
```

Initialization returns `0` on success and `-1` for invalid arguments. Push/pop return `1` when a byte is transferred, `0` when full/empty, and `-1` for invalid state or output pointer. The buffer is single-producer/single-consumer: one execution context owns `head`, one owns `tail`. The storage and ring object must outlive both contexts.

## Packet parser

```c
void daq_parser_init(daq_parser_t *parser,
                     daq_packet_callback_t callback, void *user);
void daq_parser_reset(daq_parser_t *parser);
void daq_parser_feed(daq_parser_t *parser, uint8_t byte);
void daq_parser_feed_bytes(daq_parser_t *parser,
                           const uint8_t *bytes, size_t length);
uint16_t daq_crc16_ccitt_false(const uint8_t *data, size_t length);
int daq_decode_imu(const daq_packet_t *packet,
                   daq_imu_sample_t *sample);
```

Initialize before feeding bytes. The callback is invoked only after a complete packet with a valid CRC. Its packet pointer refers to parser-owned storage and must not be retained without copying. Reset aborts only the current partial packet; cumulative counters remain intact. `daq_decode_imu` returns `0` for a 14-byte payload and `-1` otherwise. Invalid or oversized packets are counted and discarded; parser statistics are in `parser.stats`.

## Filters

```c
int daq_moving_average_init(daq_moving_average_t *filter, size_t window);
int32_t daq_moving_average_push(daq_moving_average_t *filter,
                                int32_t sample);
int daq_fir_init(daq_fir_t *filter, const float *coefficients, size_t taps);
float daq_fir_process(daq_fir_t *filter, float sample);
int daq_threshold_update(daq_threshold_t *detector, int32_t value);
```

Filter initializers return `0` on success, `-1` for unsupported sizes or null arguments. Moving-average windows are 1–64 samples; FIR length is 1–32 taps. FIR coefficient memory is owned by the caller and must stay valid while processing. Threshold update returns `+1` when the signal crosses the rising threshold, `-1` when it falls through the lower threshold, and `0` otherwise.
