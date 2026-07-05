#include "gesture_engine.h"
#include <cstdio>

#define DIST_NEAR_MM        150
#define DIST_FAR_MM         350
#define DIST_HOLD_MM        200
#define TIME_HOLD_MIN_MS    800
#define TIME_HOLD_LONG_MS   2000
#define TIME_DOUBLE_GAP_MS  700
#define TIME_MAX_SWIPE_MS   300
#define TIME_MAX_WAVE_MS    600
#define TIME_COOLDOWN_MS    1200
#define TIME_THEREMIN_TIMEOUT_MS 4000

void gesture_reset(GestureContext* ctx) {
    ctx->current_state = GESTURE_IDLE;
    ctx->prev_state = GESTURE_IDLE;
    ctx->state_entry_ms = 0;
}

static void fsm_transition(GestureContext* ctx, GestureState s, uint32_t ts) {
    ctx->prev_state = ctx->current_state;
    ctx->current_state = s;
    ctx->state_entry_ms = ts;
}

bool gesture_update(GestureContext* ctx, const RingBuffer* rb, uint32_t now_ms, GestureEvent* out) {
    if (rb->count < 3) return false;

    uint16_t dist = rb_median3(rb);
    uint32_t time_in_state = now_ms - ctx->state_entry_ms;

    switch (ctx->current_state) {
        case GESTURE_IDLE:
            if (dist < DIST_NEAR_MM) fsm_transition(ctx, GESTURE_APPROACH, now_ms);
            break;

        case GESTURE_APPROACH:
            if (dist > DIST_FAR_MM) {
                if (time_in_state < TIME_MAX_SWIPE_MS) {
                    out->gesture_id = 0x30; // Swipe
                    out->confidence = 100;
                    fsm_transition(ctx, GESTURE_COOLDOWN, now_ms);
                    return true;
                } else if (time_in_state < TIME_MAX_WAVE_MS) {
                    fsm_transition(ctx, GESTURE_RETRACT_1, now_ms);
                } else {
                    fsm_transition(ctx, GESTURE_IDLE, now_ms);
                }
            } else if (time_in_state > TIME_HOLD_MIN_MS && dist < DIST_HOLD_MM) {
                fsm_transition(ctx, GESTURE_HOLD, now_ms);
            }
            break;

        case GESTURE_HOLD:
            if (dist > DIST_FAR_MM) {
                out->gesture_id = (time_in_state > TIME_HOLD_LONG_MS) ? 0x21 : 0x20;
                out->confidence = 100;
                fsm_transition(ctx, GESTURE_COOLDOWN, now_ms);
                return true;
            }
            break;

        case GESTURE_RETRACT_1:
            if (time_in_state > TIME_DOUBLE_GAP_MS) {
                fsm_transition(ctx, GESTURE_IDLE, now_ms);
            } else if (dist < DIST_NEAR_MM) {
                fsm_transition(ctx, GESTURE_APPROACH_2, now_ms);
            }
            break;

        case GESTURE_APPROACH_2:
            if (dist > DIST_FAR_MM) {
                if (time_in_state < TIME_MAX_WAVE_MS) {
                    printf("\n[FSM] === ENTERING VOLUME SLIDER MODE ===\n");
                    fsm_transition(ctx, GESTURE_THEREMIN, now_ms);
                } else {
                    fsm_transition(ctx, GESTURE_IDLE, now_ms);
                }
            }
            break;

        case GESTURE_THEREMIN:
            if (dist < 400) { 
                ctx->state_entry_ms = now_ms; 
                
                static uint32_t last_vol_time = 0;
                if (now_ms - last_vol_time > 150) {
                    last_vol_time = now_ms;
                    if (dist < 200) {
                        out->gesture_id = 0x40;
                        return true;
                    } else if (dist >= 250 && dist < 350) {
                        out->gesture_id = 0x41;
                        return true;
                    }
                }
            } else {
                if (time_in_state > TIME_THEREMIN_TIMEOUT_MS) {
                    printf("[FSM] === EXITING VOLUME SLIDER MODE ===\n\n");
                    fsm_transition(ctx, GESTURE_COOLDOWN, now_ms);
                }
            }
            break;

        case GESTURE_COOLDOWN:
            if (time_in_state > TIME_COOLDOWN_MS) fsm_transition(ctx, GESTURE_IDLE, now_ms);
            break;

        default:
            gesture_reset(ctx);
            break;
    }
    return false;
}