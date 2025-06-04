
#pragma once

#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_DEBUG 4

void Log(int level,char* format, ...);

#define WARN(x,...) Log(LOG_LEVEL_WARNING, "%s(): " x, __func__, ##__VA_ARGS__)
#define INFO(x,...) Log(LOG_LEVEL_INFO, "%s(): " x, __func__, ##__VA_ARGS__)
#define DEBUG(x,...) Log(LOG_LEVEL_DEBUG, "%s(): " x, __func__, ##__VA_ARGS__)
#define ERROR(x,...) Log(LOG_LEVEL_ERROR, "%s(): " x, __func__, ##__VA_ARGS__)

void SLog(int level,char* format, ...);

#define SWARN(x,...) SLog(LOG_LEVEL_WARNING, "%s(): " x, __func__, ##__VA_ARGS__)
#define SINFO(x,...) SLog(LOG_LEVEL_INFO, "%s(): " x, __func__, ##__VA_ARGS__)
#define SDEBUG(x,...) SLog(LOG_LEVEL_DEBUG, "%s(): " x, __func__, ##__VA_ARGS__)
#define SERROR(x,...) SLog(LOG_LEVEL_ERROR, "%s(): " x, __func__, ##__VA_ARGS__)

void DLog(char* format, ...);
void NLog(char* format, ...);
void LogUnlock();
void LogBuffer(char* buffer,uint64_t size);

void LogInit(char* ctx);