#pragma once

#include <generic/vmm.hpp>

namespace mmap_syncer {
    void init();
    void sync(void* vmem, void* mem);
}