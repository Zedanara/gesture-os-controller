#include "com_port.h"
#include <cstdio>

bool com_open(ComPortState* port, const char* name, uint32_t baud) {
    char full_name[32];
    snprintf(full_name, sizeof(full_name), "\\\\.\\%s", name);

    port->handle = CreateFileA(full_name, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (port->handle == INVALID_HANDLE_VALUE) return false;

    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    GetCommState(port->handle, &dcb);
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    SetCommState(port->handle, &dcb);

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout         = 10;
    timeouts.ReadTotalTimeoutConstant    = 50;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    SetCommTimeouts(port->handle, &timeouts);

    port->is_open = true;
    return true;
}

bool com_read(ComPortState* port) {
    if (!port->is_open) return false;
    return ReadFile(port->handle, port->rx_buf, sizeof(port->rx_buf), &port->bytes_read, NULL) 
           && port->bytes_read > 0;
}

void com_close(ComPortState* port) {
    if (port->is_open) {
        CloseHandle(port->handle);
        port->is_open = false;
    }
}