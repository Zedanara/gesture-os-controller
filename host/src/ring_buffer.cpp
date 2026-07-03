#include "ring_buffer.h"
#include <cstring>

void rb_init(RingBuffer* rb) {
    memset(rb, 0, sizeof(RingBuffer));
}

void rb_push(RingBuffer* rb, const TofReading* r) {
    rb->data[rb->head] = *r;
    rb->head = (rb->head + 1) & (RING_BUF_SIZE - 1); 
    if (rb->count < RING_BUF_SIZE) rb->count++;
}

const TofReading* rb_peek(const RingBuffer* rb, uint8_t idx_from_newest) {
    if (idx_from_newest >= rb->count) return nullptr;
    uint8_t actual_pos = (rb->head - 1 - idx_from_newest + RING_BUF_SIZE) & (RING_BUF_SIZE - 1);
    return &rb->data[actual_pos];
}

uint16_t rb_median3(const RingBuffer* rb) {
    if (rb->count < 3) return rb->count > 0 ? rb_peek(rb, 0)->dist_mm : 0;
    uint16_t a = rb_peek(rb, 0)->dist_mm;
    uint16_t b = rb_peek(rb, 1)->dist_mm;
    uint16_t c = rb_peek(rb, 2)->dist_mm;
    
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) { uint16_t t = b; b = c; c = t; }
    if (a > b) { uint16_t t = a; a = b; b = t; }
    return b; 
}

int32_t rb_velocity(const RingBuffer* rb) {
    if (rb->count < 2) return 0;
    const TofReading* newest = rb_peek(rb, 0);
    const TofReading* older = rb_peek(rb, 1);
    
    if (newest->timestamp_ms <= older->timestamp_ms) return 0;
    
    int32_t dt = newest->timestamp_ms - older->timestamp_ms;
    int32_t dd = (int32_t)newest->dist_mm - (int32_t)older->dist_mm;
    return (dd * 1000) / dt; 
}