#pragma once
#include "types.h"
#include "ring_buffer.h"

void gesture_reset(GestureContext* ctx);
bool gesture_update(GestureContext* ctx, const RingBuffer* rb, uint32_t now_ms, GestureEvent* out);