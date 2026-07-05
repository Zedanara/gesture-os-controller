#include "os_control.h"
#include <cstdio>
#include <windows.h>

static void send_media_key(WORD vk_code) {
    INPUT inputs[2] = {0};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk_code;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk_code;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void os_execute_action(const GestureEvent* event) {
    switch (event->gesture_id) {
        case 0x10:
            printf("[ACTION] DOUBLE WAVE -> Volume DOWN\n");
            send_media_key(VK_VOLUME_DOWN);
            send_media_key(VK_VOLUME_DOWN);
            break;

        case 0x20:
            printf("[ACTION] SHORT HOLD -> PLAY / PAUSE\n");
            send_media_key(VK_MEDIA_PLAY_PAUSE);
            break;

        case 0x21:
            printf("[ACTION] LONG HOLD -> Locking Workstation\n");
            LockWorkStation();
            break;

        case 0x30:
            printf("[ACTION] SWIPE -> Next Track / Slide\n");
            send_media_key(VK_MEDIA_NEXT_TRACK);
            break;

        default:
            break;
    }
}