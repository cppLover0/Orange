
#pragma once

#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR 3

void Log(int level,char* format, ...);
void NLog(char* format, ...);
void LogUnlock();
void LogBuffer(char* buffer,uint64_t size);

void LogInit(char* ctx);