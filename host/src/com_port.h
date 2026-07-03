#pragma once
#include "types.h"

bool com_open(ComPortState* port, const char* name, uint32_t baud);
bool com_read(ComPortState* port);
void com_close(ComPortState* port);