#include "os_control.h"
#include <cstdio>
#include <windows.h>

static void send_keyboard_key(WORD vk_code) {
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
        case 0x20:
            printf("[ACTION] SHORT HOLD -> PLAY / PAUSE\n");
            send_keyboard_key(VK_MEDIA_PLAY_PAUSE);
            break;

        case 0x21:
            printf("[ACTION] LONG HOLD -> Locking Workstation\n");
            LockWorkStation();
            break;

        case 0x30:
            printf("[ACTION] SWIPE IN -> Next Slide (KEY RIGHT)\n");
            send_keyboard_key(VK_RIGHT);
            break;

        case 0x31:
            printf("[ACTION] SWIPE OUT -> Previous Slide (KEY LEFT)\n");
            send_keyboard_key(VK_LEFT);
            break;

        case 0x40:
            printf("[ACTION] SLIDER -> Volume UP\n");
            send_keyboard_key(VK_VOLUME_UP);
            break;

        case 0x41:
            printf("[ACTION] SLIDER -> Volume DOWN\n");
            send_keyboard_key(VK_VOLUME_DOWN);
            break;

        default:
            break;
    }
}