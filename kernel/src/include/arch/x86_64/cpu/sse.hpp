
#include <stdint.h>

#pragma once

#define DEFAULT_SSE_FLAGS ((1 << 9) | (1 << 10) | (1 << 1))
#define SSE_XSAVE_SUPPORT (1 << 26)
#define SSE_XSAVE_CR4 (1 << 18)

#define SSE_CONTROL_DEFAULT ((1 << 0) | (1 << 1))

#define SSE_CHECK_AND_SET(bit) \
if(a & bit) \
    sse_control |= bit;

typedef struct {
    uint16_t dumb0;
    uint32_t dumb1;
    uint16_t dumb2;
    uint64_t dumb3;
    uint64_t dumb4;
    uint32_t dumb5;
} __attribute__((packed)) fpu_head_t;

class SSE {
public:
    static void Init();
    static uint64_t Size();
    static void Save(uint8_t* buf);
    static void Load(uint8_t* buf);
};