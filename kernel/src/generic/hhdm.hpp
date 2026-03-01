#pragma once
#include <generic/bootloader/bootloader.hpp>

namespace etc {
    inline static std::uintptr_t hhdm() {
        return bootloader::bootloader->get_hhdm();
    }
}