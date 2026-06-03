#include <generic/pmm.hpp>
#include <generic/vfs.hpp>
#include <generic/unix_sockets.hpp>
#include <generic/scheduling.hpp>
#include <generic/userspace/sockets.hpp>
#include <generic/lock/spinlock.hpp>
#include <klibc/string.hpp>
#include <klibc/stdio.hpp>
#include <utils/errno.hpp>
#include <utils/signal_ret.hpp>
#include <utils/signal.hpp>

unix_socket_node* head_unsock_node = nullptr;
locks::spinlock un_lock;

void insert_unix_socket(unix_socket_node* node) {
    un_lock.lock();
    node->next = head_unsock_node;
    head_unsock_node = node;
    un_lock.unlock();
}

unix_socket_node* unsock_find(sockaddr_un* path1) {
    sockaddr_un path_t = {};
    klibc::memcpy(path_t.sun_path, path1->sun_path, klibc::strlen(path1->sun_path));
    sockaddr_un* path = &path_t;
    unix_socket_node* current = head_unsock_node;
    while(current) {
        if(klibc::memcmp(&path->sun_path, &current->path.sun_path, sizeof(current->path.sun_path)) == 0)
            return current;
        current = current->next;
    }
    return nullptr;
}

bool unix_sockets::is_exists(sockaddr_un* path, bool is_internal) {
    (void)is_internal;
    return unsock_find(path) == nullptr ? false : true;
}

long long unix_sockets::bind(file_descriptor* file, sockaddr_un* path) {
    if(is_exists(path, true) == true)
        return -EEXIST;

    char buf[4096] = {};
    if(path->sun_path[0] == '/') {
        if(vfs::readlink(path->sun_path, buf, 4096) != -ENOENT)
            return -EEXIST; 
    }

    auto new_node = (unix_socket_node*)(pmm::freelist::alloc_4k() + etc::hhdm());
    new_node->path = *path;
    insert_unix_socket(new_node);

    new_node->link_counter++;
    new_node->open_counter = 1; 

    file->socket.socket_pointer = (void*)new_node;
    return 0;
}

long long unix_sockets::connect(thread* proc, file_descriptor* file, sockaddr_un* path) {
    auto node = unsock_find(path);
    if(node == nullptr)
        return -ENOENT;

    unix_socket_pending_connection* conn = nullptr;

    un_lock.lock();
    for(std::size_t i = 0;i < sizeof(node->pend_conns) / sizeof(unix_socket_pending_connection); i++) {
        if(node->pend_conns[i].is_used == false) {
            node->pend_conns[i].is_used = true;
            node->pend_conns[i].is_accepted.unlock();
            node->pend_conns[i].file = file;
            node->pend_conns[i].proc = proc;
            conn = &node->pend_conns[i];
            break;
        }
    }
    un_lock.unlock();

    klibc::debug_printf("trying to connect socket %s fd %d\n", path->sun_path, file->index);

    if(conn == nullptr)
        return -EAGAIN; // spec says must return eagain if there's no free space for connection

    node->conn_counter++;

    while(!conn->is_accepted.test()) {
        is_signal_ret(proc) {
            signal_ret(proc);
        }
        process::yield();
    }

    klibc::debug_printf("connected meow\n");

    conn->is_used = false;

    // meow meow
    
    return 0;
}

long long unix_sockets::accept(thread* proc, file_descriptor* file, sockaddr_un* path) {
    (void)file;
    auto node = unsock_find(path);
    if(node == nullptr)
        return -ENOENT;

    auto manager = (vfs::fdmanager*)proc->fd;
    file_descriptor* fd = manager->createlowest(2);

    while(true) {
        un_lock.lock();
        for(std::size_t i = 0;i < sizeof(node->pend_conns) / sizeof(unix_socket_pending_connection); i++) {
            unix_socket_pending_connection* current = &node->pend_conns[i];
            if(current->is_used == true && !current->is_accepted.test()) {
                file_descriptor* dest = (file_descriptor*)current->file;
                dest->socket.socket_side = 1;
                fd->socket.socket_side = 0;

                dest->type = file_descriptor_type::socket;
                fd->type = file_descriptor_type::socket;

                vfs::pipe* r = new vfs::pipe(0);
                vfs::pipe* w = new vfs::pipe(0);

                dest->socket.write_socket = w;
                dest->socket.read_socket = r;
                fd->socket.write_socket = w;
                fd->socket.read_socket = r;

                dest->socket.un.r_fd = new list;
                dest->socket.un.w_fd = new list;
                dest->socket.un.r_ucred = new list;
                dest->socket.un.w_ucred = new list;

                fd->socket.un.r_fd = dest->socket.un.r_fd;
                fd->socket.un.w_fd = dest->socket.un.w_fd;
                fd->socket.un.w_ucred = dest->socket.un.w_ucred;
                fd->socket.un.r_ucred = dest->socket.un.r_ucred;

                fd->socket.un.cred.gid = current->proc->gid;
                fd->socket.un.cred.pid = current->proc->id;
                fd->socket.un.cred.uid = current->proc->uid;

                dest->socket.un.cred = fd->socket.un.cred;
                dest->socket.socket_type = PF_UNIX;

                fd->socket.socket_type = PF_UNIX;
                fd->socket.socket_specific = dest->socket.socket_specific;

                fd->socket.socket_pointer = (void*)node;
                dest->socket.socket_pointer = (void*)node;

                node->conn_counter--;

                node->open_counter += 2;

                current->is_accepted.try_lock();
                current->is_used = false;

                un_lock.unlock();
                goto end;

            }
        }
        un_lock.unlock();
        process::yield();
    }

end:
    return fd->index;
}

void process_close(void* node1) {

    unix_socket_node* node = (unix_socket_node*)node1;

    node->open_counter--;
    if(node->open_counter == 0 && node->link_counter == 0) {
        un_lock.lock();
        unix_socket_node* parent = head_unsock_node;
        while(parent) {
            if(parent->next == node)
                break;
        }

        if(parent) {
            parent->next = node->next;
        }

        pmm::freelist::free((std::uint64_t)node - etc::hhdm());

        un_lock.unlock();
    }
}

unix_socket_node* unix_sockets::find(sockaddr_un* path) {
    return unsock_find(path);
}