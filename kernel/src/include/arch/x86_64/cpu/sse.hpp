
#include <stdint.h>

#pragma once

#define DEFAULT_SSE_FLAGS ((1 << 9) | (1 << 10) | (1 << 1))
#define SSE_DISABLE_EMUL ~(1 << 2)
#define SSE_XSAVE_SUPPORT (1 << 26)
#define SSE_XSAVE_CR4 (1 << 18)

#define SSE_CONTROL_DEFAULT ((1 << 0) | (1 << 1))

#define SSE_CHECK_AND_SET(bit) \
if(a & bit) \
    sse_control |= bit;

class SSE {
public:
    static void Init();
    static uint64_t Size();
    static void Save(uint8_t* buf);
    static void Load(uint8_t* buf);
};