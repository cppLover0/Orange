#include <generic/bootloader/bootloader.hpp>
#include <generic/squashfs.hpp>
#include <generic/modules.hpp>
#include <generic/arch.hpp>
#include <generic/ustar.hpp>
#include <cstdint>

bool is_there_initrd = false;
extern vfs::node* vfs_nodes;

void modules::init() {
    limine_module_response* resp = bootloader::bootloader->get_modules();
    assert(resp, "no modules = no boot");

    for(std::uint64_t i = 0;i < resp->module_count;i++) {
        limine_file* current_module = resp->modules[i];
        if(squashfs::is_valid((char*)current_module->address, current_module->size)) {
            squashfs::init((char*)current_module->address, current_module->size, &vfs_nodes[0]);
        } else if(ustar::is_valid_ustar((char*)current_module->address, current_module->size)) {
            ustar::load((char*)current_module->address, current_module->size);
            is_there_initrd = true;
        }
    }

    assert(is_there_initrd, "no initrd = no boot ez");
}
