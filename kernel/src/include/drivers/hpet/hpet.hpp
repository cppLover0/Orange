
#pragma once

#include <stdint.h>

class HPET {
public:
    static void Init();
    static void Sleep(uint64_t usec);
    static uint64_t NanoCurrent();
};