#include <generic/flock.hpp>
#include <generic/vfs.hpp>

flock::flock_struct* flock::create(void* fs1, int inode, off_t start, off_t len, short lock, short whence, off_t seek, std::size_t file_size, int pid) {
    
    filesystem* fs = (filesystem*)fs1;

    fs->flock_related.lock.lock();

    flock_high* list = (flock_high*)fs->flock_related.list;

    if(list == nullptr) {
        fs->flock_related.list = klibc::malloc(sizeof(flock_high));
        list = (flock_high*)fs->flock_related.list;
    }

    flock::flock_list* inode_list = nullptr;

    if(list->flocks != nullptr) {
        for(std::size_t i = 0;i < fs->flock_related.ptr; i++) {
            if(list->flocks[i].is_used == true && list->flocks[i].inode == inode) {
                inode_list = &list->flocks[i];
                break;
            }
        }
    }

    if(list->flocks != nullptr && inode_list == nullptr) {
        for(std::size_t i = 0;i < fs->flock_related.ptr; i++) {
            if(list->flocks[i].is_used == false) {
                inode_list = &list->flocks[i];
                inode_list->is_used = true;
                inode_list->ptr = 0;
                inode_list->lock = nullptr;
                inode_list->inode = inode;
                break;
            }
        }
    }

    

    if(inode_list == nullptr) {

        flock::flock_list* new_list = (flock::flock_list*)klibc::malloc(sizeof(flock::flock_list) * (fs->flock_related.ptr + 1));
        if(list->flocks != nullptr) {
            klibc::memcpy(new_list, list->flocks, sizeof(flock::flock_list) * (fs->flock_related.ptr));
            klibc::free(list->flocks);
        } 
        list->flocks = new_list;
        inode_list = &new_list[fs->flock_related.ptr];
        inode_list->is_used = true;
        inode_list->ptr = 0;
        inode_list->lock = nullptr;
        inode_list->inode = inode;
        fs->flock_related.ptr++;   
    }

    flock::flock_struct* found = nullptr;

    if (inode_list->lock != nullptr) {
        for (std::size_t i = 0; i < inode_list->ptr; i++) {

            if (inode_list->lock[i].l_type == F_UNLCK) {
                continue;
            }

            auto lock_start = inode_list->lock[i].l_start;
            if (inode_list->lock[i].l_whence == SEEK_CUR) {
                lock_start += seek; 
            } else if (inode_list->lock[i].l_whence == SEEK_END) {
                lock_start += file_size;  
            } 

            bool intersects = false;
            if (inode_list->lock[i].l_len == 0) {
                if (len == 0) { 
                    intersects = true;
                } else {
                    if (start + len > lock_start) intersects = true;
                }
            }
            else if (len == 0) {
                if (start < lock_start + inode_list->lock[i].l_len) intersects = true;
            }
            else {
                if (start < lock_start + inode_list->lock[i].l_len && start + len > lock_start)
                    intersects = true;
            }

            if (intersects) {
                if (inode_list->lock[i].l_pid == pid) {
                    found = &inode_list->lock[i];
                    break;
                } else {
                    fs->flock_related.lock.unlock();
                    return nullptr;
                }
            }

        }
    }

    if(found == nullptr) {
        
        if(inode_list->lock != nullptr) {
            for(std::size_t i = 0; i < inode_list->ptr; i++) {
                if(inode_list->lock[i].l_type == F_UNLCK) { // meow wmeowewoemwoewoemwmeowomowmowemo
                    found = &inode_list->lock[i];
                    break;
                }
            }
        }

        // allocate new
        if(found == nullptr) {
            flock::flock_struct* new_list = (flock::flock_struct*)klibc::malloc(sizeof(flock::flock_struct) * (inode_list->ptr + 1));
            if(inode_list->lock != nullptr) {
                klibc::memcpy(new_list, inode_list->lock, sizeof(flock::flock_struct) * inode_list->ptr);
                klibc::free(inode_list->lock);
            }
            inode_list->lock = new_list;
            found = &inode_list->lock[inode_list->ptr];
            inode_list->ptr++;
        }

    }

    found->l_len = len;
    found->l_pid = pid;
    found->l_start = start;
    found->l_type = lock;
    found->l_whence = whence;

    fs->flock_related.lock.unlock();
    return found;
}

flock::flock_struct* flock::search(void* fs1, int inode, off_t start, off_t len, short whence, off_t seek, std::size_t file_size) {
    (void)whence;

    filesystem* fs = (filesystem*)fs1;

    flock_high* list = (flock_high*)fs->flock_related.list;

    flock_list* inode_list = nullptr;
    if(list == nullptr) { 
        return nullptr;
    }

    if(list->flocks == nullptr) {
        return nullptr;
    }

    for(std::size_t i = 0; i < fs->flock_related.ptr; i++) {
        if(list->flocks[i].is_used == true && list->flocks[i].inode == inode) {
            inode_list = &list->flocks[i];
            break;
        }
    }

    if(inode_list == nullptr) {
        return nullptr;
    }

    if(inode_list->lock == nullptr) {
        return nullptr;
    }

    for(std::size_t i = 0; i < inode_list->ptr; i++) {
        if (inode_list->lock[i].l_type == F_UNLCK) {
            continue;
        }

        auto lock_start = inode_list->lock[i].l_start;
        if (inode_list->lock[i].l_whence == SEEK_CUR) {
            lock_start += seek; 
        } else if (inode_list->lock[i].l_whence == SEEK_END) {
            lock_start += file_size;  
        } 

        if (inode_list->lock[i].l_len == 0) {
            if (start + len > lock_start || len == 0) {
                return &inode_list->lock[i];
            }
            continue; 
        }

        if (len == 0) {
            if (start < lock_start + inode_list->lock[i].l_len) {
                return &inode_list->lock[i];
            }
            continue;
        }

        if ((start < lock_start + inode_list->lock[i].l_len) && 
            (start + len > lock_start)) {
            return &inode_list->lock[i];
        }
    }
    
    return nullptr;
}

flock::flock_list* flock::access_source(void* fs1, int inode) {

    filesystem* fs = (filesystem*)fs1;

    flock_high* list = (flock_high*)fs->flock_related.list;

    if(list != nullptr) {
        if(list->flocks != nullptr) {
            for(std::size_t i = 0;i < fs->flock_related.ptr; i++) {
                if(list->flocks[i].is_used == true && list->flocks[i].inode == inode) {
                    return &list->flocks[i];
                }
            }
        }
    }

    return nullptr;
}

void flock::remove(void* fs1, int inode) {

    filesystem* fs = (filesystem*)fs1;

    flock_high* list = (flock_high*)fs->flock_related.list;
    fs->flock_related.lock.lock();

    if(list != nullptr) {
        if(list->flocks != nullptr) {
            for(std::size_t i = 0; i < fs->flock_related.ptr; i++) {
                if(list->flocks[i].is_used == true && list->flocks[i].inode == inode) {
                    list->flocks[i].is_used = false;
                    if(list->flocks[i].lock)
                        delete[] list->flocks[i].lock;
                    list->flocks[i].ptr = 0;
                    list->flocks[i].inode = 0;
                    list->flocks[i].lock = nullptr;
                    fs->flock_related.lock.unlock();
                    return;
                }
            } 
        }
    }

    fs->flock_related.lock.unlock();
    return;
}

