#include <cstdio>
#include <windows.h>
#include "types.h"
#include "com_port.h"
#include "packet_parser.h"

static ComPortState g_com = {0};
BOOL WINAPI ctrl_handler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_CLOSE_EVENT) {
        com_close(&g_com);
        printf("\n[INFO] COM port closed safely. Exiting...\n");
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char* argv[]) {
    const char* port_name = (argc > 1) ? argv[1] : "COM5"; 

    ParserState parser;
    parser_init(&parser);
    
    RingBuffer ring_buf;
    rb_init(&ring_buf);
    
    GestureContext gesture;
    gesture_reset(&gesture);
    
    TofReading reading = {0};
    GestureEvent event = {0};

    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    if (!com_open(&g_com, port_name, 115200)) {
        printf("[ERROR] Cannot open %s\n", port_name);
        return 1;
    }

    printf("[INFO] Connected to %s. Waiting for gestures...\n", port_name);

    while (true) {
        if (com_read(&g_com)) {
            for (DWORD i = 0; i < g_com.bytes_read; i++) {
                char byte = (char)g_com.rx_buf[i];
                if (parser_feed(&parser, byte, &reading)) {
                    
                    rb_push(&ring_buf, &reading);
                    
                    if (gesture_update(&gesture, &ring_buf, reading.timestamp_ms, &event)) {
                        if (event.gesture_id == 0x10) printf("\n>>> GESTURE DETECTED: DOUBLE WAVE!\n\n");
                        if (event.gesture_id == 0x20) printf("\n>>> GESTURE DETECTED: SHORT HOLD (Point)\n\n");
                        if (event.gesture_id == 0x21) printf("\n>>> GESTURE DETECTED: LONG HOLD (Dash)\n\n");
                    }
                }
            }
        }
        Sleep(5);
    }

    com_close(&g_com);
    return 0;
}