
#pragma once

void Log(char* format, ...);
void NLog(char* format, ...);
void LogUnlock();
void LogBuffer(char* buffer,uint64_t size);

void LogInit(char* ctx);