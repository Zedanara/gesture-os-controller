#pragma once
#include "types.h"

void rb_init(RingBuffer* rb);
void rb_push(RingBuffer* rb, const TofReading* r);
const TofReading* rb_peek(const RingBuffer* rb, uint8_t idx_from_newest);
uint16_t rb_median3(const RingBuffer* rb);
int32_t rb_velocity(const RingBuffer* rb);