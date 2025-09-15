
#include <stdio.h>

#pragma once

#define LEVEL_MESSAGE_OK 0
#define LEVEL_MESSAGE_FAIL 1
#define LEVEL_MESSAGE_WARN 2
#define LEVEL_MESSAGE_INFO 3

const char* level_messages[] = {
    [LEVEL_MESSAGE_OK] = "[   \x1b[38;2;0;255;0mOK\033[0m   ] ",
    [LEVEL_MESSAGE_FAIL] = "[ \x1b[38;2;255;0;0mFAILED\033[0m ] ",
    [LEVEL_MESSAGE_WARN] = "[  \x1b[38;2;255;165;0mWARN\033[0m  ] ",
    [LEVEL_MESSAGE_INFO] = "[  \x1b[38;2;0;191;255mINFO\033[0m  ] "
};

#define log(level, format,...) printf("%s" format "", level_messages[level], ##__VA_ARGS__)