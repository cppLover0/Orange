#include <cstdint>
#include <generic/scheduling.hpp>
#include <generic/elf.hpp>
#include <generic/pmm.hpp>
#include <generic/paging.hpp>
#include <generic/vmm.hpp>
#include <utils/gobject.hpp>
#include <klibc/stdio.hpp>
#include <klibc/string.hpp>
#include <klibc/stdlib.hpp>
#include <utils/errno.hpp> 
#include <utils/random.hpp>
#include <utils/stack.hpp>
#include <generic/vfs.hpp>
#include <generic/arch.hpp>
#include <generic/userspace/syscall_list.hpp>
#include <generic/heap.hpp>

uint64_t __elf_get_length(char** arr) {
    uint64_t counter = 0;

    while(arr[counter]) {
        counter++;
        char* t = (char*)(arr[counter - 1]);
        if(*t == '\0')
            return counter - 1; // invalid
    } 

    return counter;
}

bool elf::is_valid_elf(thread* proc, char* data) {

    if (data[0] == '#' && data[1] == '!') {
        log("a","a");
        char* path = &data[2];
        while (*path == ' ') path++; 

        char interp_path[256];
        int i = 0;
        while (path[i] && path[i] != '\n' && path[i] != '\r' && path[i] != ' ' && i < 255) {
            interp_path[i] = path[i];
            i++;
        }
        interp_path[i] = '\0';

        char zpath[4096] = {};
        process_path(proc->chroot, proc->cwd, interp_path, zpath);

        file_descriptor fd = {};
        int status = vfs::open(&fd, zpath, true, false);
        if (status != 0) {
            return false;
        }
        if (fd.vnode.close) fd.vnode.close(&fd);
        return true;
    }

    elfheader_t* head = (elfheader_t*)data;
    char ELF[4] = {0x7F, 'E', 'L', 'F'};
    if (klibc::strncmp((const char*)head->e_ident, ELF, 4) != 0) {
        log("a","ab %c%c%c%c x",head->e_ident[0],head->e_ident[1],head->e_ident[2],head->e_ident[3]);
        return false;
    }

    for (int i = 0; i < head->e_phnum; i++) {
        elfprogramheader_t* current_head = (elfprogramheader_t*)((uint64_t)data + head->e_phoff + head->e_phentsize * i);
        if (current_head->p_type == PT_INTERP) {
            file_descriptor fd = {};
            char path[4096] = {};
            process_path(proc->chroot, proc->cwd, (char*)((uint64_t)data + current_head->p_offset), path);
            int status = vfs::open(&fd, path, true, false);
            if (status) {log("elf", "no interp %s", path); return false;}
            if (fd.vnode.close) fd.vnode.close(&fd);
        }
    }

    return true;
}


elfloadresult_t elf::load_elf(thread* proc, char* data) {
    elfheader_t* head = (elfheader_t*)data;
    elfprogramheader_t* current_head = nullptr;
    std::uint64_t elf_base = UINT64_MAX;
    std::uint64_t size = 0;
    std::uint64_t phdr = 0;

    std::uint64_t highest_addr = 0;
    char interp[4096];
    klibc::memset(interp, 0, 4096);

    elfloadresult_t res = {};
    res.real_entry = head->e_entry;
    res.interp_entry = head->e_entry;
    res.phnum = head->e_phnum;
    res.phentsize = head->e_phentsize;

    if(head->e_type != ET_DYN) {
        for(int i = 0;i < head->e_phnum; i++) {
            current_head = (elfprogramheader_t*)((uint64_t)data + head->e_phoff + head->e_phentsize*i);
            if (current_head->p_type == PT_LOAD) {
                elf_base = MIN2(elf_base, ALIGNDOWN(current_head->p_vaddr, PAGE_SIZE));
                highest_addr = MAX2(highest_addr, ALIGNUP(current_head->p_vaddr + current_head->p_memsz, PAGE_SIZE));
            }
        }
    }

    for(int i = 0;i < head->e_phnum; i++) {
        uint64_t end = 0;
        current_head = (elfprogramheader_t*)((uint64_t)data + head->e_phoff + head->e_phentsize*i);

        if(current_head->p_type == PT_PHDR) {
            phdr = current_head->p_vaddr;
            res.phdr = phdr;
        } 

        if (current_head->p_type == PT_LOAD) {
            end = current_head->p_vaddr - elf_base + current_head->p_memsz;
            size = MAX2(size, end);
        }
    }

    void* elf_vmm;

    size = ALIGNPAGEUP(size);

    if(head->e_type != ET_DYN) {
        assert(!(proc->vmem->getlen(elf_base)),"memory 0x%p is not avaiable !",elf_base);
        elf_vmm = (void*)proc->vmem->alloc_memory(elf_base, size, true);
        assert((std::uint64_t)elf_vmm == elf_base,"0x%p is not equal 0x%p, elf size %lli",elf_vmm,elf_base, size);
        res.base = (uint64_t)elf_base;
    } else {
        elf_vmm = (void*)proc->vmem->alloc_memory(0, size, false);
        res.base = (uint64_t)elf_vmm;
    }

    proc->vmem->inv_lazy_alloc((std::uint64_t)elf_vmm, size);

    uint8_t* allocated_elf = (uint8_t*)elf_vmm;

    arch::enable_paging(proc->original_root);

    for(int i = 0;i < head->e_phnum; i++) {
        uint64_t dest = 0;
        current_head = (elfprogramheader_t*)((uint64_t)data + head->e_phoff + head->e_phentsize*i);
        
        if(current_head->p_type == PT_LOAD) {
            if(head->e_type != ET_DYN) {
                dest = ((current_head->p_vaddr) - elf_base) + (std::uint64_t)allocated_elf;
            } else {
                dest = (uint64_t)allocated_elf + current_head->p_vaddr;
            }

            klibc::memset((void*)dest, 0, current_head->p_memsz);
            klibc::memcpy((void*)dest, (void*)((uint64_t)data + current_head->p_offset), current_head->p_filesz);
        } else if(current_head->p_type == PT_INTERP) {
            stat interp_stat = {};
            file_descriptor fd = {};
            char path[4096] = {};
            process_path(proc->chroot, proc->cwd, (char*)((uint64_t)data + current_head->p_offset), path);

            int status = vfs::open(&fd, path, true, false);
            if(status) { arch::enable_paging(gobject::kernel_root);
                log("elf", "unknown interp path %s:%s (%d)",(char*)((uint64_t)data + current_head->p_offset), path, status);
                return {0,0,0,0,0,-ENOENT, 0, 0};
            }

            assert(fd.vnode.stat != nullptr, ":( stat elf");
            fd.vnode.stat(&fd, &interp_stat);

            char* inter = (char*)(pmm::buddy::alloc(interp_stat.st_size).phys + etc::hhdm());
            fd.vnode.read(&fd, inter, interp_stat.st_size);
            arch::enable_paging(gobject::kernel_root);
            elfloadresult_t interpload = load_elf(proc, (char*)inter);
            arch::enable_paging(proc->original_root);
            pmm::buddy::free((std::uint64_t)inter - etc::hhdm());
            if(fd.vnode.close) 
                fd.vnode.close(&fd);

            res.interp_base = interpload.base;
            res.interp_entry = interpload.real_entry;
        }
    }

    if(head->e_type != ET_DYN) {
        if(phdr) {
            res.phdr = phdr;
        }
    } else {
        if(phdr) {
            phdr += (uint64_t)elf_vmm;
            res.phdr = phdr;
        }
        res.real_entry = (uint64_t)elf_vmm + head->e_entry;
    }

//     if(proc->is_debug) {
//         uint64_t strtab_offset = 0;
//         uint64_t strtab_addr = 0;
//         uint64_t strtab_size = 0;

//         elfprogramheader_t* phdrz =0;

//         if(head->e_type != ET_DYN) {
//             phdrz = (elfprogramheader_t*)(phdr); 
//         } else 
//             phdrz = (elfprogramheader_t*)((phdr));  
//         elfprogramheader_t* dynamic_phdr = NULL;

//         if(phdrz) {
//             for (int i = 0; i < head->e_phnum; i++) {
//                 klibc::debug_printf("phdr type %d\n",phdrz[i].p_type);
//                 if (phdrz[i].p_type == PT_DYNAMIC) {
//                     dynamic_phdr = &phdrz[i];
//                     break;
//                 }
//             }
//         }

//         if (dynamic_phdr == NULL) {
//             klibc::debug_printf("DYNAMIC segment not found\n");
//             //goto end;
//         } else {
//             Elf64_Dyn* dyn;
//             if(head->e_type != ET_DYN) 
//                 dyn = (Elf64_Dyn *)((char *)allocated_elf + (dynamic_phdr->p_vaddr - elf_base));
//             else
//                 dyn = (Elf64_Dyn *)((char *)allocated_elf + (dynamic_phdr->p_vaddr));
//             uint64_t dyn_size = dynamic_phdr->p_filesz / sizeof(Elf64_Dyn);

//             for (uint64_t i = 0; i < dyn_size; i++) {
//                 if (dyn[i].d_tag == DT_NULL) {
//                     break;
//                 }
                
//                 if (dyn[i].d_tag == DT_STRTAB) {
//                     strtab_addr = dyn[i].d_un.d_ptr;
//                 } else if (dyn[i].d_tag == DT_STRSZ) {
//                     strtab_size = dyn[i].d_un.d_val;
//                 }
//             }

//             if (strtab_addr == 0) {
//                 klibc::debug_printf( ".dynstr string table not found\n");
//                 //goto end;
//             }

//             for (int i = 0; i < head->e_phnum; i++) {
//                 if (phdrz[i].p_type == PT_LOAD) {
//                     uint64_t vaddr = phdrz[i].p_vaddr;
//                     uint64_t memsz = phdrz[i].p_memsz;
//                     uint64_t offset = phdrz[i].p_offset;
                    
//                     if (strtab_addr >= vaddr && strtab_addr < vaddr + memsz) {
//                         strtab_offset = offset + (strtab_addr - vaddr);
//                         break;
//                     }
//                 }
//             }

//             if (strtab_offset == 0) {
//                 klibc::debug_printf("Could not find string table offset in file\n");
//                 //goto end;
//             }

//             char *strtab = (char *)allocated_elf + strtab_offset;
//             int dep_count = 0;

//             klibc::debug_printf("proc %d libraries\n", proc->id);

//             for (uint64_t i = 0; i < dyn_size; i++) {
//                 if (dyn[i].d_tag == DT_NULL) {
//                     break;
//                 }
                
//                 if (dyn[i].d_tag == DT_NEEDED) {
//                     uint64_t str_offset = dyn[i].d_un.d_val;
                    
//                     if (str_offset < strtab_size) {
//                         klibc::debug_printf("  %s\n", strtab + str_offset);
//                         dep_count++;
//                     } else {
//                         klibc::debug_printf("  [Error: string offset out of bounds]\n");
//                     }
//                 }
//             }

//             if (dep_count == 0) {
//                 klibc::debug_printf("  (no dependencies found)\n");
//             }
//         }
//    }

    return res;

}

void elf::exec(thread* proc, const char* path, char** argv, char** envp) {
    stat elf_stat = {};
    file_descriptor fd = {};

    int status = vfs::open(&fd, (char*)path, true, false);
    assert(status == 0, ":( %d dskvs", status);
    assert(fd.vnode.stat, "FAOFSAPFJSAD}F");
    assert(fd.vnode.read, "help meeee");

    status = fd.vnode.stat(&fd, &elf_stat);
    assert(status == 0, "ok");

    char header[128];
    klibc::memset(header, 0, 128);
    fd.vnode.read(&fd, header, 127);

    if (header[0] == '#' && header[1] == '!') {
        char* interpreter_path = &header[2];
        char* end = interpreter_path;
        while (*end && *end != '\n' && *end != '\r' && *end != ' ') end++;
        *end = '\0';

        uint64_t old_argv_len = __elf_get_length(argv);
        char** new_argv = (char**)kheap::malloc(8 * (old_argv_len + 2));
        
        new_argv[0] = interpreter_path;
        new_argv[1] = (char*)path;
        for (uint64_t i = 1; i < old_argv_len; i++) {
            new_argv[i+1] = argv[i];
        }
        new_argv[old_argv_len + 1] = nullptr;

        elf::exec(proc, interpreter_path, new_argv, envp);
        
        kheap::free(new_argv);
        return; 
    }

    fd.offset = 0; 

    char* file = (char*)(pmm::buddy::alloc(elf_stat.st_size).phys + etc::hhdm());
    status = fd.vnode.read(&fd, file, elf_stat.st_size);
    assert(status >= 0, "m");

    elfloadresult_t elfload = elf::load_elf(proc, file);
    pmm::buddy::free((std::uint64_t)file - etc::hhdm());

    assert(elfload.status == 0, "shit fuck elf %d", elfload.status);

#if defined(__x86_64__)
    proc->ctx.rsp = proc->vmem->alloc_memory(0, 4 * 1024 * 1024, false);
    proc->vmem->inv_lazy_alloc(proc->ctx.rsp,(4 * 1024 * 1024));
    proc->ctx.rsp += (4 * 1024 * 1024) - PAGE_SIZE;
    std::uint64_t* _stack = (std::uint64_t*)proc->ctx.rsp;

    std::uint64_t random_data_aux = (std::uint64_t)proc->vmem->alloc_memory(0, 4096, false);
    proc->vmem->inv_lazy_alloc(random_data_aux, 4096);

    std::uint64_t auxv_stack[] = {(std::uint64_t)elfload.real_entry,AT_ENTRY,elfload.phdr,AT_PHDR,elfload.phentsize,AT_PHENT,elfload.phnum,AT_PHNUM,4096,AT_PAGESZ,0,AT_SECURE,random_data_aux,AT_RANDOM,4096,51,elfload.interp_base,AT_BASE, (std::uint64_t)proc->uid, AT_UID, (std::uint64_t)proc->uid, AT_EUID, (std::uint64_t)proc->gid, AT_GID};
    std::uint64_t argv_length = __elf_get_length(argv);
    std::uint64_t envp_length = __elf_get_length(envp);

    arch::enable_paging(proc->original_root);

    std::uint64_t* randaux = (std::uint64_t*)random_data_aux;
    randaux[0] = random::random();
    randaux[1] = random::random();

    char** stack_argv = (char**)kheap::malloc(8 * (argv_length + 1));
    char** stack_envp = (char**)kheap::malloc(8 * (envp_length + 1));

    klibc::memset(stack_argv,0,8 * (argv_length + 1));
    klibc::memset(stack_envp,0,8 * (envp_length + 1));

    _stack = stackmgr::copy_to_stack(argv,_stack,stack_argv,argv_length);

    for(std::uint64_t i = 0; i < argv_length / 2; ++i) {
        char* tmp = stack_argv[i];
        stack_argv[i] = stack_argv[argv_length - 1 - i];
        stack_argv[argv_length - 1 - i] = tmp;
    }

    _stack = stackmgr::copy_to_stack(envp,_stack,stack_envp,envp_length);

    PUT_STACK(_stack,0);

    _stack = stackmgr::copy_to_stack_no_out(auxv_stack,_stack,sizeof(auxv_stack) / 8);
    PUT_STACK(_stack,0);

    _stack = stackmgr::copy_to_stack_no_out((uint64_t*)stack_envp,_stack,envp_length);
    PUT_STACK(_stack,0);

    _stack = stackmgr::copy_to_stack_no_out((uint64_t*)stack_argv,_stack,argv_length);
    PUT_STACK(_stack,argv_length);

    arch::enable_paging(gobject::kernel_root);

    proc->ctx.rsp = (std::uint64_t)_stack;
    proc->ctx.rip = elfload.interp_entry;

    kheap::free(stack_argv);
    kheap::free(stack_envp);
#endif

}