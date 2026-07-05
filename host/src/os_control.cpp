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
        case 0x20: // Short Hold
            printf("[ACTION] SHORT HOLD -> PLAY / PAUSE\n");
            send_media_key(VK_MEDIA_PLAY_PAUSE);
            break;

        case 0x21: // Long Hold
            printf("[ACTION] LONG HOLD -> Locking Workstation\n");
            LockWorkStation();
            break;

        case 0x30: // Fast Swipe
            printf("[ACTION] SWIPE -> Next Track / Slide\n");
            send_media_key(VK_MEDIA_NEXT_TRACK);
            break;

        case 0x40: // Theremin UP
            printf("[ACTION] SLIDER -> Volume UP\n");
            send_media_key(VK_VOLUME_UP);
            break;

        case 0x41: // Theremin DOWN
            printf("[ACTION] SLIDER -> Volume DOWN\n");
            send_media_key(VK_VOLUME_DOWN);
            break;

        default:
            break;
    }
}