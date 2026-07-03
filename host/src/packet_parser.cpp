#include "packet_parser.h"
#include <cstdlib>
#include <cstring>

void parser_init(ParserState* p) {
    memset(p, 0, sizeof(ParserState));
    p->fsm = PARSE_WAIT_Y1;
}

bool parser_feed(ParserState* p, char byte, TofReading* out) {
    switch (p->fsm) {
        case PARSE_WAIT_Y1:
            if (byte == 'Y') p->fsm = PARSE_WAIT_Y2;
            break;

        case PARSE_WAIT_Y2:
            if (byte == 'Y') {
                memset(p->dist_buf, 0, sizeof(p->dist_buf));
                memset(p->time_buf, 0, sizeof(p->time_buf));
                p->dist_pos = 0;
                p->time_pos = 0;
                p->fsm = PARSE_READ_DIST;
            } else {
                p->fsm = PARSE_WAIT_Y1;
            }
            break;

        case PARSE_READ_DIST:
            if (byte >= '0' && byte <= '9') {
                if (p->dist_pos < sizeof(p->dist_buf) - 1) p->dist_buf[p->dist_pos++] = byte;
            } else if (byte == 'T') {
                p->fsm = PARSE_READ_TIME;
            } else {
                p->fsm = PARSE_WAIT_Y1;
            }
            break;

        case PARSE_READ_TIME:
            if (byte >= '0' && byte <= '9') {
                if (p->time_pos < sizeof(p->time_buf) - 1) p->time_buf[p->time_pos++] = byte;
            } else if (byte == 'E') {
                p->fsm = PARSE_WAIT_Y1; 
                
                if (p->dist_pos == 0 || p->time_pos == 0) return false;

                uint32_t dist = (uint32_t)atol(p->dist_buf);
                uint32_t ts   = (uint32_t)atol(p->time_buf);

                if (dist == 0 || dist > 3000) return false;

                out->dist_mm      = (uint16_t)dist;
                out->timestamp_ms = ts;
                out->valid        = true;
                return true;
            } else {
                p->fsm = PARSE_WAIT_Y1;
            }
            break;
    }
    return false;
}