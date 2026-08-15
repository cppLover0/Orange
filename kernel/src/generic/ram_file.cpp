#include <generic/ram_file.hpp>
#include <generic/lock/spinlock.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>
#include <klibc/string.hpp>
#include <klibc/stdio.hpp>
#include <utils/assert.hpp>
#include <generic/vfs.hpp>

locks::spinlock ram_file_lock;
ram_file::content* ram_files = nullptr;

ram_file::content* ram_file_iget(std::int64_t inode, void* id) {
    auto current = ram_files;
    while(current) {
        if(current->ident == id && current->is_used == true && current->inode == inode)
            return current;
        current = current->next;
    }
    return nullptr;
}

ram_file::content* ram_file::create(std::int64_t inode, void* id) {
    auto file = ram_file_iget(inode, id);
    if(file) return file;

    auto free = ram_files;
    while(free) {
        if(free->is_used == false)
            break;
        free = free->next;
    }

    if(free == nullptr) {
        free = new ram_file::content;
        free->next = ram_files;
        ram_files = free;
    }

    free->is_used = true;
    free->ident = id;
    free->inode = inode;
    free->pages = nullptr;

    return free;
}

ram_file::content* ram_file::get(std::int64_t inode, void* id) {
    return ram_file_iget(inode, id);
}

ram_file::page* get_ipage(ram_file::content* node, std::size_t page_num) {
    auto found = node->pages;
    while(found) {
        if(found->num == page_num && found->p != nullptr) 
            return found;
        found = found->next;
    }
    return nullptr;
}

ram_file::page* ram_file::access_page(std::int64_t inode, void* id, std::size_t page_num, bool should_create, void* desc = nullptr, std::size_t original_mmap_off = 0) {
    content* found = ram_file_iget(inode, id);
    if(found == nullptr) return nullptr;

    page* pag = get_ipage(found, page_num);

    if(should_create == false || pag != nullptr)
        return pag;

    page* new_page = new page;

    new_page->num = page_num;
    new_page->p = (void*)(pmm::freelist::alloc_4k() + etc::hhdm());

    assert(desc != nullptr, "????");

    auto file = (file_descriptor*)desc;

    file_descriptor copy_file = *file;

    copy_file.offset = original_mmap_off + (page_num * PAGE_SIZE);

    assert(copy_file.vnode.read, "fuck you %s", copy_file.path);

    klibc::printf("MEOW %s off %lli, %lli, %lli count %lli\n", copy_file.path, original_mmap_off, page_num * PAGE_SIZE, copy_file.offset);
    std::size_t count = copy_file.vnode.read(&copy_file, new_page->p, PAGE_SIZE);
    klibc::printf("count %lli\n", count);
    
    assert(count != 0, "gamgesromssgfdkvbl %lli %lli count %lli", copy_file.offset, page_num, count); // debug assert

    new_page->next = found->pages;
    found->pages = new_page;

    return new_page;
}

void ram_file_remove_page(ram_file::content* file, ram_file::page* page, void* lock_protection) {
    return;
    auto prev = file->pages;
    while(prev != nullptr) {
        if(prev->next == page)
            break;
        prev = prev->next;
    }

    if(prev != nullptr) {
        prev->next = page->next;
    } else {
        file->pages = nullptr;
    }

    while(1) {
        thread* proc = process::_head_proc();
        while(proc) {
            proc->op_lock.lock();
            if(proc->vmem != nullptr && proc->status == PROCESS_LIVE) {

                bool state = false;

                if(proc->vmem != lock_protection)
                    state = proc->vmem->lock.lock();

                vmm_obj* current = proc->vmem->start;

                while(current) {

                    if(current == proc->vmem->end)
                        break;

                    if(current->base != 0) {
                        if(current->mmap_info.copied_file_desc != nullptr && current->mmap_info.file == file) {
                            paging::zero_range(proc->vmem->root, current->base + (page->num * PAGE_SIZE), PAGE_SIZE);
                        }
                    }

                    current = current->next;
                }

                if(proc->vmem != lock_protection)
                    proc->vmem->lock.unlock(state);

            }
            proc->op_lock.unlock();
            proc = proc->next;
        }
        break;
    }

    //pmm::freelist::free((std::uint64_t)page->p - etc::hhdm());
    //delete page;
}

// removes all pages that are higher than limit
void ram_file::small(std::int64_t inode, void* id, std::size_t page_limit, void* lock_protection = nullptr) {
    content* found = ram_file_iget(inode, id);
    if(found == nullptr) return;

    auto current = found->pages;
    while(current != nullptr) {
        if(current->num > page_limit) 
            ram_file_remove_page(found, current, lock_protection);
        current = current->next;
    }

    return;
}

// if counter is 0 ram file will be removed
void ram_file::dec(std::int64_t inode, void* id, void* lock_protection) {
    content* found = ram_file_iget(inode, id);
    if(found == nullptr) return;

    found->ref_count--;

    if(found->ref_count == 0) {
        auto current_page = found->pages;
        while(current_page) {
            ram_file_remove_page(found, current_page, lock_protection);
            current_page = current_page->next;
        }

        found->is_used = false;
    }
    
    return;
}

void ram_file::inc(std::int64_t inode, void* id) {
    content* found = ram_file_iget(inode, id);
    if(found == nullptr) return;

    found->ref_count++;

    return;
}