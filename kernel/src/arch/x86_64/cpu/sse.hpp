#pragma once
#include <cstdint>

namespace x86_64 {
    namespace sse {

        typedef struct {
            std::uint16_t dumb0;
            std::uint32_t dumb1;
            std::uint16_t dumb2;
            std::uint64_t dumb3;
            std::uint64_t dumb4;
            std::uint32_t dumb5;
        } __attribute__((packed)) fpu_head_t;

        #define DEFAULT_SSE_FLAGS ((1 << 9) | (1 << 10) | (1 << 1))
        #define SSE_XSAVE_SUPPORT (1 << 26)
        #define SSE_XSAVE_CR4 (1 << 18)

        #define SSE_CONTROL_DEFAULT ((1 << 0) | (1 << 1))

        #define SSE_CHECK_AND_SET(bit) \
        if(a & bit) \
            sse_control |= bit;

        void init();
        std::uint64_t size();
        void save(std::uint8_t* buf);
        void load(std::uint8_t* buf);
        void print_sse_features();
        inline static void setup_headers(fpu_head_t* head) { head->dumb5 = 0b0001111110000000; }
    };
}