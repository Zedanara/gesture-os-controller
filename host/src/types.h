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