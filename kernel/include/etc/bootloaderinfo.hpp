
#pragma once

#include <limine.h>

namespace {

__attribute__((used, section(".limine_requests")))
volatile LIMINE_BASE_REVISION(3);

}

namespace {

__attribute__((used, section(".limine_requests")))
volatile limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = nullptr
};

}

namespace {

__attribute__((used, section(".limine_requests_start")))
volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
volatile LIMINE_REQUESTS_END_MARKER;

}

class BootloaderInfo {
public:
    static inline limine_framebuffer* AccessFramebuffer() {
        return framebuffer_request.response->framebuffers[0];
    }
};