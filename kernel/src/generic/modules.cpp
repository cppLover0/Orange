#include <generic/bootloader/bootloader.hpp>
#include <generic/squashfs.hpp>
#include <generic/modules.hpp>
#include <generic/arch.hpp>
#include <generic/ustar.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>

#include <klibc/stdio.hpp>
#include <klibc/string.hpp>

#define KGZ_USE_OWN_MACROS
#define KGZ_MALLOC(size) (void*)(pmm::buddy::alloc(size).phys + etc::hhdm()) 
#define KGZ_FREE(ptr, sz) pmm::buddy::free((std::uint64_t)ptr - etc::hhdm())
#define KGZ_MEMCPY(dst, src, n) klibc::memcpy((dst), (src), (n))
#define KGZ_MEMSET(ptr, val, size) klibc::memset((ptr), (val), (size))
#define KGZ_PRINTF(...) log("kgz", __VA_ARGS__)

#define KGZ_IMPLEMENTATION
#include <utils/kgz_singleheader.h>
#include <cstdint>

bool is_there_initrd = false;
extern vfs::node* vfs_nodes;

void process_module(char* address, std::size_t size) {

    std::size_t out_size = 0;
    std::size_t buffer_size = 0;
    char* decomp = (char*)kgz_gzip_decompress(address, size, &out_size, &buffer_size);

    if(decomp != nullptr) {
        log("modules", "found .gz archive");
        process_module(decomp, out_size);
    }

    if(squashfs::is_valid((char*)address, size)) {
        squashfs::init((char*)address, size, &vfs_nodes[0]);
    } else if(ustar::is_valid_ustar((char*)address, size)) {
        ustar::load((char*)address, size);
        is_there_initrd = true;
    }
}

void modules::init() {
    limine_module_response* resp = bootloader::bootloader->get_modules();
    assert(resp, "no modules = no boot");

    for(std::uint64_t i = 0;i < resp->module_count;i++) {
        limine_file* current_module = resp->modules[i];
        process_module((char*)current_module->address, current_module->size);
    }

    assert(is_there_initrd, "no initrd = no boot ez");
}
