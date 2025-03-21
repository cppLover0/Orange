
#include <lib/flanterm/flanterm.h>

#pragma once

void Log(char* format, ...);
void NLog(char* format, ...);

void LogInit(flanterm_context* ctx);