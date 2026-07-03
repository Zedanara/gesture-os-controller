#pragma once
#include "types.h"

void parser_init(ParserState* p);
bool parser_feed(ParserState* p, char byte, TofReading* out);