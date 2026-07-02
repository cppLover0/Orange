#include <generic/userspace/syscall_list.hpp>
#include <generic/pmm.hpp>
#include <generic/vfs.hpp>
#include <generic/vmm.hpp>
#include <generic/paging.hpp>
#include <generic/arch.hpp>
#include <utils/errno.hpp>
#include <generic/userspace/syscall_list.hpp>
#include <generic/userspace/safety.hpp>
#include <generic/time.hpp>
#include <generic/unix_sockets.hpp>
#include <generic/userspace/sockets.hpp>

long long sys_socket(int family, int type, int protocol) {
    (void)family;
    (void)type;
    (void)protocol;

    type = type & 0xF;
    if(type != SOCK_STREAM) {
        klibc::debug_printf("unimplemented socket type %d for family %d\n", type, family);
    }

    if(family != PF_UNIX) {
        klibc::debug_printf("unimplemented socket family %d for type %d\n", family, type);
    } 

    if(family == PF_NETLINK) {
        // wtf ???
        return -EINVAL;
    }

    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;

    file_descriptor* file = manager->createlowest(2);
    file->type = file_descriptor_type::socket;
    file->socket.socket_type = family;
    file->socket.socket_specific = type;

    if(current->is_debug) {
        klibc::debug_printf("creating socket family %d, type %d protocol %d fd %d\n", family, type, protocol, file->index);
    }

    return file->index;
}

long long sys_listen(int fd, int backlog) {
    (void)backlog;
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;

    if(current->is_debug) {
        klibc::debug_printf("listen fd %d backlog %d\n", fd, backlog);
    }

    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;
    
    if(file->type != file_descriptor_type::socket)
        return -EINVAL;

    if(file->socket.socket_type == PF_UNIX) {
        if(file->socket.write_socket || file->socket.read_socket)
            return -EINVAL;
    }

    file->socket.is_listen = true;
    return 0;
}

long long sys_bind(int fd, const struct sockaddr *addr_ptr, std::uint32_t addr_length) {
    (void)addr_length;

    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;

    if(!is_safe_to_rw(current, (std::uint64_t)addr_ptr, PAGE_SIZE))
        return -EFAULT;

    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;
    
    if(file->type != file_descriptor_type::socket)
        return -ENOTSOCK;

    if(file->socket.socket_type == PF_UNIX) {
        if(file->socket.write_socket || file->socket.read_socket)
            return -EINVAL;
        if(current->is_debug) {
            klibc::debug_printf("bind socket to %s fd %d\n", ((sockaddr_un*)addr_ptr)->sun_path, fd);
        }
        //og("sockets", "bind socket to %s", ((sockaddr_un*)addr_ptr)->sun_path);
        return unix_sockets::bind(file, (sockaddr_un*)addr_ptr);
    }

    assert(0, "SGDGSSGD");
    return -EFAULT;
}

static inline void print_ipv4_address(const void *sockaddr_ptr) {
    const struct sockaddr_in *addr = (const struct sockaddr_in *)sockaddr_ptr;

    uint32_t ip = addr->sin_addr.s_addr;
    uint8_t ip1 = (ip      ) & 0xFF;
    uint8_t ip2 = (ip >>  8) & 0xFF;
    uint8_t ip3 = (ip >> 16) & 0xFF;
    uint8_t ip4 = (ip >> 24) & 0xFF;

    uint16_t raw_port = addr->sin_port;
    uint16_t port = ((raw_port & 0xFF) << 8) | ((raw_port >> 8) & 0xFF);

    klibc::debug_printf("ip: %d.%d.%d.%d:%d\n", ip1, ip2, ip3, ip4, port);
}

long long sys_connect(int fd, const struct sockaddr *addr_ptr, std::uint32_t addr_length) {
    (void)addr_length;

    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;

    if(!is_safe_to_rw(current, (std::uint64_t)addr_ptr, PAGE_SIZE))
        return -EFAULT;

    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;
    
    if(file->type != file_descriptor_type::socket)
        return -ENOTSOCK;

    if(file->socket.socket_type == PF_UNIX) {
        if(file->socket.write_socket || file->socket.read_socket)
            return -EINVAL;
        if(current->is_debug) {
            klibc::debug_printf("trying to connect to %s fd %d\n", ((sockaddr_un*)addr_ptr)->sun_path, fd);
        }
        //log("sockets", "trying to connect to %s", ((sockaddr_un*)addr_ptr)->sun_path);
        long long ret = unix_sockets::connect(current, file, (sockaddr_un*)addr_ptr);

        if(current->is_debug) {
            klibc::debug_printf("ret %d\n", ret);
        }
        //log("sockets", "ret is %d", ret);
        return ret;
    } else if(file->socket.socket_type == PF_INET) {
        klibc::debug_printf("unimplemented ipv4 connect\n");
        print_ipv4_address((const void*)addr_ptr);
        return -EHOSTUNREACH;
    }

    assert(0, "SGDGcXXXXXSSGD type %d", file->socket.socket_type);
    return -EFAULT;
}

long long sys_accept(int fd, struct sockaddr *addr_ptr, std::uint32_t *addr_length, int flags) {
    (void)flags;

    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;

    if(!is_safe_to_rw(current, (std::uint64_t)addr_ptr, PAGE_SIZE))
        return -EFAULT;

    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;
    
    if(file->type != file_descriptor_type::socket)
        return -EINVAL;

    if(file->socket.socket_type == PF_UNIX) {
        if(file->socket.write_socket || file->socket.read_socket)
            return -EINVAL;
        
        unix_socket_node* node = (unix_socket_node*)file->socket.socket_pointer;
        if(node == nullptr)
            return -EINVAL;

        long long ret = unix_sockets::accept(current, file, &node->path);
        if(addr_ptr) {
            klibc::memcpy(addr_ptr, &node->path, sizeof(node->path));
            if(addr_length)
                *addr_length = sizeof(sockaddr_un);
            addr_ptr->sa_family = AF_UNIX;
        }
        return ret;
    
    }

    assert(0, "SGDGSSGD");
    return -EFAULT;
}

long long sys_recvfrom(int fd, void *buffer, size_t size, int flags, struct sockaddr *sock_addr, std::uint32_t *addr_length) {
    (void)flags;
    (void)sock_addr;
    (void)addr_length;
    // i have unix sockets sock stream now so no sock_addr 

    if(flags != 0) {
        klibc::debug_printf("MEOWW !! MEOW RECV !!! %d\n", flags);
    }


    return sys_read(fd, (char*)buffer, size);
}

long long sys_sendto(int fd, const void *buffer, size_t size, int flags, const struct sockaddr *sock_addr, std::uint32_t addr_length) {
    (void)flags;
    (void)sock_addr;
    (void)addr_length;

    if(flags != 0) {
        klibc::debug_printf("MEOWW !! MEOW !!! %d\n", flags);
    }

    return sys_write(fd, (char*)buffer, size);
}

long long sys_msg_send(int fd, struct msghdr* hdr, int flags) {
    thread* proc = current_proc;
    auto manager = (vfs::fdmanager*)proc->fd;

    struct msghdr* msg = hdr;
    if(!is_safe_to_rw(proc, (std::uint64_t)hdr, 4096))
        return -EFAULT;

    file_descriptor* fd_s = manager->search(fd);
    if(!fd_s)
        return -EBADF;

    std::uint64_t total_size = 0;
    for (int i = 0; i < hdr->msg_iovlen; i++) {
        if(!is_safe_to_rw(proc, (std::uint64_t)hdr->msg_iov[i].iov_base, hdr->msg_iov[i].iov_len))
            return -EFAULT;
        total_size += hdr->msg_iov[i].iov_len;
    }

    if(fd_s->socket.socket_type == PF_UNIX && fd_s->type == file_descriptor_type::socket) {
        vfs::pipe* target_pipe = fd_s->socket.socket_side == 1 ? fd_s->socket.write_socket : fd_s->socket.read_socket;
        if(!target_pipe)
            return -EBADF;

        if(total_size > target_pipe->total_size)
            return -EMSGSIZE;

        bool state = target_pipe->lock.lock();
        std::uint64_t space_left = target_pipe->total_size - target_pipe->size;
        target_pipe->lock.unlock(state);
        while(space_left < total_size) {
            process::yield();
            state = target_pipe->lock.lock();
            space_left = target_pipe->total_size - target_pipe->size;
            target_pipe->lock.unlock(state);
        }

        state = target_pipe->lock.lock();

        struct cmsghdr *cmsg = 0;


        for (cmsg = CMSG_FIRSTHDR(msg);
            cmsg != NULL;
            cmsg = CMSG_NXTHDR(msg, cmsg)) {
            
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                int new_fd = 0;
                klibc::memcpy(&new_fd, CMSG_DATA(cmsg), sizeof(int));

                file_descriptor* fd_s1 = manager->search(new_fd);

                if(!fd_s1) {
                    break;
                }

                klibc::debug_printf("SCM_RIGHTS\n");

                if(fd_s->socket.socket_side == 1)
                    fd_s->socket.un.r_fd->push((uint64_t)fd_s1);
                else 
                    fd_s->socket.un.w_fd->push((uint64_t)fd_s1);
            
            } else if(cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_CREDENTIALS) {
                ucred* tm = (struct ucred*)CMSG_DATA(cmsg);
                ucred* tmp_ucred = new ucred;
                *tmp_ucred = *tm;

                klibc::debug_printf("SCM_CREDENTIALS\n");

                if(fd_s->socket.socket_side == 1)
                    fd_s->socket.un.r_ucred->push((uint64_t)tmp_ucred);
                else 
                    fd_s->socket.un.w_ucred->push((uint64_t)tmp_ucred);
            }
        }

        std::int64_t total_written = 0;

        for (int i = 0; i < hdr->msg_iovlen; i++) {
            std::int64_t sent_bytes = target_pipe->nolock_write((const char*)hdr->msg_iov[i].iov_base,hdr->msg_iov[i].iov_len);
            if(sent_bytes < 0) {
                target_pipe->lock.unlock(state);
                return sent_bytes;
            }
            total_written += sent_bytes;
            if ((std::uint64_t)sent_bytes < hdr->msg_iov[i].iov_len) {
                break; 
            }
        }

        klibc::debug_printf("msg_send fd %d total_written %lli flags %d from proc %d controlern %d\n",fd,total_written,flags,proc->id, hdr->msg_controllen);

        target_pipe->lock.unlock(state);
        return total_written;
    } else if(fd_s->type == file_descriptor_type::socketpair) {
        vfs::pipe* target_pipe = fd_s->socketpair.is_slave ? fd_s->socketpair.write_socket : fd_s->socketpair.read_socket;
        if(!target_pipe)
            return -EBADF;

        if(total_size > target_pipe->total_size)
            return -EMSGSIZE;

        bool state = target_pipe->lock.lock();
        std::uint64_t space_left = target_pipe->total_size - target_pipe->size;
        target_pipe->lock.unlock(state);
        while(space_left < total_size) {
            process::yield();
            state = target_pipe->lock.lock();
            space_left = target_pipe->total_size - target_pipe->size;
            target_pipe->lock.unlock(state);
        }

        state = target_pipe->lock.lock();

        std::int64_t total_written = 0;

        for (int i = 0; i < hdr->msg_iovlen; i++) {
            std::int64_t sent_bytes = target_pipe->nolock_write((const char*)hdr->msg_iov[i].iov_base,hdr->msg_iov[i].iov_len);
            if(sent_bytes < 0) {
                target_pipe->lock.unlock(state);
                return sent_bytes;
            }
            total_written += sent_bytes;
            if ((std::uint64_t)sent_bytes < hdr->msg_iov[i].iov_len) {
                break; 
            }
        }

        if(fd_s->socketpair.is_slave) {
            fd_s->socketpair.read_socket->sock_ucred.pid = proc->pid;
            fd_s->socketpair.read_socket->sock_ucred.gid = proc->gid;
            fd_s->socketpair.read_socket->sock_ucred.uid = proc->uid;
        } else {
            fd_s->socketpair.write_socket->sock_ucred.pid = proc->pid;
            fd_s->socketpair.write_socket->sock_ucred.gid = proc->gid;
            fd_s->socketpair.write_socket->sock_ucred.uid = proc->uid;
        }

        klibc::debug_printf("msg_send fd %d total_written %lli flags %d from proc %d controllen %d\n",fd,total_written,flags,proc->id, hdr->msg_controllen);

        target_pipe->lock.unlock(state);
        return total_written;
    }

    assert(0,"f");
    return -EFAULT;
}

long long sys_msg_recv(int fd, struct msghdr *hdr, int flags) {
    struct msghdr* msg = hdr;
    thread* proc = current_proc;
    auto manager = (vfs::fdmanager*)proc->fd;

    if(!is_safe_to_rw(proc, (std::uint64_t)msg, 4096))
        return -EFAULT;

    file_descriptor* fd_s = manager->search(fd);
    if(!fd_s)
        return -EBADF;

    if(fd_s->socket.socket_type == PF_UNIX && fd_s->type == file_descriptor_type::socket) {
        vfs::pipe* target_pipe = fd_s->socket.socket_side == 1 ? fd_s->socket.read_socket : fd_s->socket.write_socket;
        if(!target_pipe)
            return -EBADF;

        std::uint64_t total_size = 0;
        for (int i = 0; i < hdr->msg_iovlen; i++) {
            if(!is_safe_to_rw(proc, (std::uint64_t)hdr->msg_iov[i].iov_base,hdr->msg_iov[i].iov_len))
                return -EFAULT;
            total_size += hdr->msg_iov[i].iov_len;
        }

        (void)total_size;

        std::int64_t total_read = 0;

        for (int i = 0; i < hdr->msg_iovlen; i++) {
            std::int64_t recv_bytes = 0;
            recv_bytes = target_pipe->read((char*)hdr->msg_iov[i].iov_base,hdr->msg_iov[i].iov_len,(((fd_s->flags & O_NONBLOCK) ? 1 : 0)) | ((flags & MSG_DONTWAIT) ? 1 : 0));
            if(recv_bytes == -EAGAIN) {
                klibc::debug_printf("EAGAIN fd %d\n", fd);
                return -EAGAIN;
            } 
            total_read += recv_bytes;
        }

        std::uint32_t new_msglen = 0;
        std::uint32_t src_msglen = hdr->msg_controllen;

        struct cmsghdr* cmsg = 0;

        bool state = target_pipe->lock.lock();

        for (cmsg = CMSG_FIRSTHDR(msg);
            cmsg != 0;
            cmsg = CMSG_NXTHDR(msg, cmsg)) {
            if (1) {
                if(1) {
                    file_descriptor* src = (file_descriptor*)(fd_s->socket.socket_side == 1 ? fd_s->socket.un.w_fd->pop() : fd_s->socket.un.r_fd->pop());
                    if(src != nullptr) {
                        cmsg->cmsg_level = SOL_SOCKET;
                        cmsg->cmsg_type = SCM_RIGHTS;
                        cmsg->cmsg_len = 24;
                        new_msglen += 24;
                        file_descriptor* new_fd = manager->createlowest(2);
                        manager->do_dup(src, new_fd);
                        klibc::memcpy(CMSG_DATA(cmsg), &new_fd, sizeof(int));
                    } else {
                        ucred* cr = (ucred*)(fd_s->socket.socket_side == 1 ? fd_s->socket.un.w_ucred->pop() : fd_s->socket.un.r_ucred->pop());
                        if(cr != nullptr) {
                            *(struct ucred*)CMSG_DATA(cmsg) = *cr;
                            cmsg->cmsg_level = SOL_SOCKET;
                            cmsg->cmsg_type = SCM_CREDENTIALS;
                            cmsg->cmsg_len = 28;
                            new_msglen += 28;
                            delete cr;
                        }
                    }
                } 
            }
        }

        if(new_msglen != 0) {
                target_pipe->fd_pass = 0; // clear
                target_pipe->lock.unlock(state);
            } else
                target_pipe->lock.unlock(state);

            target_pipe->lock.unlock(state);
            hdr->msg_controllen = new_msglen;

            klibc::debug_printf("msg_recv fd %d total_read %lli flags %d from proc %d, controllen %d srclen %d\n",fd,total_read,flags,proc->id,hdr->msg_controllen,src_msglen);
            return total_read;
    } else if(fd_s->type == file_descriptor_type::socketpair) {

        vfs::pipe* target_pipe = fd_s->socketpair.is_slave ? fd_s->socketpair.read_socket : fd_s->socketpair.write_socket;

        std::uint64_t total_size = 0;
        for (int i = 0; i < hdr->msg_iovlen; i++) {
            if(!is_safe_to_rw(proc, (std::uint64_t)hdr->msg_iov[i].iov_base,hdr->msg_iov[i].iov_len))
                return -EFAULT;
            total_size += hdr->msg_iov[i].iov_len;
        }

        (void)total_size;

        std::int64_t total_read = 0;

        for (int i = 0; i < hdr->msg_iovlen; i++) {
            std::int64_t recv_bytes = 0;
            recv_bytes = target_pipe->read((char*)hdr->msg_iov[i].iov_base,hdr->msg_iov[i].iov_len,((fd_s->flags & O_NONBLOCK) ? 1 : 0));
            if(recv_bytes == -EAGAIN) {
                klibc::debug_printf("EAGAIN fd %d\n", fd);
                return -EAGAIN;
            } 
            total_read += recv_bytes;
        }

        hdr->msg_controllen = 0;

        klibc::debug_printf("msg_recv pair fd %d total_read %lli flags %d from proc %d, controllen %d srclen %d\n",fd,total_read,flags,proc->id,hdr->msg_controllen,0);
        return total_read;

    }
    assert(0, "Wwah awah");
    return -EFAULT;
}

long long sys_getsockopt(int fd, int layer, int number, void* buffer, unsigned* size) {

    thread* proc = current_proc;
    auto manager = (vfs::fdmanager*)proc->fd;

    auto file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;
    
    if(!is_safe_to_rw(proc, (std::uint64_t)buffer, 4096))
        return -EFAULT;

    switch(layer) {
    case 1: { // SOL_SOCKET
        switch(number) {
        case 17: // SO_CREDENTIALS

            if(file->type == file_descriptor_type::socketpair) {
                *(un_ucred*)buffer = file->socketpair.is_slave ? file->socketpair.write_socket->sock_ucred : file->socketpair.read_socket->sock_ucred;
                *size = sizeof(un_ucred);
                return 0;
            }

            klibc::debug_printf("fd %d pid %d gid %d uid %d gesockopt ucred\n", fd, file->socket.un.cred.pid, file->socket.un.cred.gid, file->socket.un.cred.uid);
            *(un_ucred*)buffer = file->socket.un.cred;
            *size = sizeof(un_ucred);
            return 0;

        case 7: // SO_SNDBUF
            *(int*)buffer = 65536;
            *size = 4;
            return 0;

        case 3: // SO_TYPE

            if(file->type == file_descriptor_type::socketpair) {
                *(int*)buffer = SOCK_STREAM;
                *size = 4;
                return 0;
            }

            *(int*)buffer = file->socket.socket_specific;
            *size = 4;
            return 0;
        }

    };
    }
    assert(0, "getsockopt fd %d layer %d number %d buffer 0x%p size 0x%p", fd, layer, number, buffer, size);
    return -EFAULT;
}

long long sys_setsockopt(int fd, int layer, int number, const void *buffer, std::uint32_t size) {

    if(layer == 6)
        if(number == 1)
            return -EOPNOTSUPP; 

    klibc::debug_printf("setsockopt %d %d %d 0x%p %d", fd, layer, number, buffer, size);
    return 0;
}

long long sys_getsockname(int fd, struct sockaddr *addr_ptr, std::uint32_t max_addr_length) {
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;

    if(!is_safe_to_rw(current, (std::uint64_t)addr_ptr, PAGE_SIZE))
        return -EFAULT;

    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;
    
    if(file->type != file_descriptor_type::socket)
        return -EINVAL;

    if(!is_safe_to_rw(current, (std::uint64_t)addr_ptr, max_addr_length + PAGE_SIZE))
        return -EFAULT;

    if(file->socket.socket_type == PF_UNIX) {
        if(file->socket.socket_pointer == nullptr)
            return -ENOTCONN;

        auto node = (unix_socket_node*)file->socket.socket_pointer;

        klibc::memset(addr_ptr, 0, max_addr_length);
        klibc::memcpy(addr_ptr, &node->path, sizeof(node->path) > max_addr_length ? max_addr_length : sizeof(node->path));
        addr_ptr->sa_family = AF_UNIX;

        return sizeof(node->path) > max_addr_length ? max_addr_length : sizeof(node->path);
    }

    assert(0,"c");
    return -EFAULT;
}

long long sys_socketpair(int* fds, int flags) {
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;
    if(!is_safe_to_rw(current, (std::uint64_t)fds, PAGE_SIZE))
        return -EFAULT;

    if(fds == nullptr)
        return -EINVAL;

    file_descriptor* fd0 = manager->createlowest(2);
    file_descriptor* fd1 = manager->createlowest(2);
    fd0->type = file_descriptor_type::socketpair;
    fd1->type = file_descriptor_type::socketpair;
    fd0->socketpair.read_socket = new vfs::pipe(0);
    fd0->socketpair.write_socket = new vfs::pipe(0);
    fd1->socketpair.read_socket = fd0->socketpair.read_socket;
    fd1->socketpair.write_socket = fd0->socketpair.write_socket;
    fd0->socketpair.is_slave = true;
    fd1->socketpair.is_slave = false;
    fds[0] = fd0->index;
    fds[1] = fd1->index;
    fd0->other.is_cloexec = (flags & SOCK_CLOEXEC) ? true : false; 
    fd1->other.is_cloexec = (flags & SOCK_CLOEXEC) ? true : false; 

    fd0->flags = (flags & SOCK_NONBLOCK) ? O_NONBLOCK : 0;
    fd1->flags = (flags & SOCK_NONBLOCK) ? O_NONBLOCK : 0;

    if(current->is_debug) {
        klibc::debug_printf("socketpair fd0 %d fd1 %d flags %d\n", fds[0], fds[1], flags);
    }

    return 0;
}

long long sys_shutdown(int sockfd, int how) {
    (void)how;
    return sys_close(sockfd);
}