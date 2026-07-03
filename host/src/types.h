#pragma once
#include <cstdint>
#include <stdbool.h>
#include <windows.h>

struct TofReading {
    uint16_t dist_mm;
    uint32_t timestamp_ms;
    bool     valid;
};

enum ParserFsmState {
    PARSE_WAIT_Y1 = 0,
    PARSE_WAIT_Y2,
    PARSE_READ_DIST,
    PARSE_READ_TIME
};

struct ParserState {
    ParserFsmState fsm;
    char           dist_buf[8];
    char           time_buf[12];
    uint8_t        dist_pos;
    uint8_t        time_pos;
};

struct ComPortState {
    HANDLE  handle;
    bool    is_open;
    uint8_t rx_buf[512];
    DWORD   bytes_read;
};


#define RING_BUF_SIZE 64

struct RingBuffer {
    TofReading data[RING_BUF_SIZE];
    uint8_t    head;
    uint8_t    count;
};

enum GestureState {
    GESTURE_IDLE = 0,
    GESTURE_APPROACH,
    GESTURE_HOLD,
    GESTURE_RETRACT_1,
    GESTURE_APPROACH_2,
    GESTURE_COOLDOWN
};

struct GestureContext {
    GestureState current_state;
    GestureState prev_state;
    uint32_t     state_entry_ms;
};

struct GestureEvent {
    uint8_t  gesture_id;
    uint16_t duration_ms;
    uint8_t  confidence;
};