
#include <arch/x86_64/syscalls/syscalls.hpp>
#include <arch/x86_64/syscalls/sockets.hpp>

#include <generic/vfs/vfs.hpp>

#include <generic/vfs/fd.hpp>

#include <generic/locks/spinlock.hpp>

#include <etc/errno.hpp>

#include <cstdint>

locks::spinlock socket_spinlock;
socket_node_t* head = 0;

char is_socket_init = 0;

socket_node_t* find_node(struct sockaddr_un* path) {
    socket_node_t* current = head;
    while(current) {
        if(!strcmp(current->path,path->sun_path)) {
            return current;
        }
        current = current->next;
    }
    return 0;
}

socket_node_t* sockets::find(char* path) {
    socket_node_t* current = head;
    while(current) {
        if(!strcmp(current->path,path)) {
            return current;
        }
        current = current->next;
    }
    return 0;
}

socket_node_t* find_node_str(char* path) {
    socket_node_t* current = head;
    while(current) {
        if(!strcmp(current->path,path)) {
            return current;
        }
        current = current->next;
    }
    return 0;
}

char sockets::is_exists(char* path) {

    if(!is_socket_init)
        return 0;

    socket_node_t* node = find_node_str(path);
    if(!node)
        return 0;
    return 1;
}

int sockets::bind(userspace_fd_t* fd, struct sockaddr_un* path) {
    
    if(!fd || !path)
        return EINVAL;

    if(path->sun_family != AF_UNIX)
        return ENOSYS;

    memset(fd->path,0,2048);
    memcpy(fd->path,path->sun_path,strlen(path->sun_path));

    vfs::stat_t stat;
    if(vfs::vfs::stat(fd,&stat) == 0) /* Check is there vfs object with some name */
        return EEXIST; 

    socket_spinlock.lock();

    if(find_node(path)) { socket_spinlock.unlock();
        return EEXIST; }

    socket_node_t* new_node = new socket_node_t;
    memset(new_node->path,0,128);
    memcpy(new_node->path,path->sun_path,108);
    new_node->is_used = 1;
    new_node->socket_counter = 0;
    new_node->next = head;
    head = new_node;
    socket_spinlock.unlock();

    return 0;

}

int sockets::connect(userspace_fd_t* fd, struct sockaddr_un* path) {

    if(!fd || !path)
        return EINVAL;

    if(path->sun_family != AF_UNIX)
        return ENOSYS;

    //socket_spinlock.lock();

    socket_node_t* node = find_node(path);
    if(!node) { //socket_spinlock.unlock();
        return ENOENT; }

    socket_pending_obj_t* pending = new socket_pending_obj_t;
    pending->son = fd;
    pending->next = node->pending_list;
    pending->is_accepted.unlock();
    node->pending_list = pending;

    node->socket_counter++;

    while(!pending->is_accepted.test()) {  yield();  }

    //socket_spinlock.unlock();
    return 0;
}

int sockets::accept(userspace_fd_t* fd, struct sockaddr_un* path) {
    if(!fd)
        return -EINVAL;

    arch::x86_64::process_t* proc = CURRENT_PROC;

    socket_spinlock.lock();

    socket_node_t* node = find_node_str(fd->path);
    if(!node) { socket_spinlock.unlock();
        return -ENOENT; }

    socket_pending_obj_t* pending_connections = node->pending_list;
    while(1) {
        while(pending_connections) {
            if(!pending_connections->is_accepted.test()) {
                int new_fd = vfs::fdmanager::create(proc);

                userspace_fd_t* new_fd_s = vfs::fdmanager::search(proc,new_fd);
                memset(new_fd_s->path,0,2048);
                memcpy(new_fd_s->path,fd->path,strlen(fd->path));

                new_fd_s->offset = 0;
                new_fd_s->queue = 0;
                new_fd_s->pipe = 0;
                new_fd_s->pipe_side = 0;
                new_fd_s->cycle = 0;
                new_fd_s->is_a_tty = 0;
                new_fd_s->is_cached_path = 0;

                new_fd_s->state = USERSPACE_FD_STATE_SOCKET;
                new_fd_s->other_state = USERSPACE_FD_OTHERSTATE_MASTER; // master - writes to read pipe and reads from write pipe 

                pending_connections->son->other_state = USERSPACE_FD_OTHERSTATE_SLAVE;

                new_fd_s->read_socket_pipe = new vfs::pipe(0);
                new_fd_s->write_socket_pipe = new vfs::pipe(0);

                new_fd_s->read_socket_pipe->create(PIPE_SIDE_READ);
                new_fd_s->read_socket_pipe->create(PIPE_SIDE_WRITE);    
                new_fd_s->write_socket_pipe->create(PIPE_SIDE_READ);   
                new_fd_s->write_socket_pipe->create(PIPE_SIDE_WRITE);
                
                pending_connections->son->read_socket_pipe = new_fd_s->read_socket_pipe;
                pending_connections->son->write_socket_pipe = new_fd_s->write_socket_pipe;

                //Log::SerialDisplay(LEVEL_MESSAGE_INFO,"together fd %d fd2 %d\n",pending_connections->son->index,new_fd_s->index);

                if(path)
                    memcpy(path->sun_path,node->path,strlen(node->path));

                pending_connections->is_accepted.test_and_set();
                
                socket_spinlock.unlock();
                return new_fd_s->index;

            }
            pending_connections = pending_connections->next;
        }
        socket_spinlock.unlock();
        yield();
        socket_spinlock.lock();
        pending_connections = node->pending_list;
    }

    return -EFAULT;
}

void sockets::init() {
    is_socket_init = 1;
    socket_spinlock.unlock();
}

syscall_ret_t sys_connect(int fd, struct sockaddr_un* path, int len) {

    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    
    if(!fd_s)
        return {0,EBADF,0};

    if(!path)
        return {0,EINVAL,0};

    struct sockaddr_un spath;

    memset(&spath,0,sizeof(spath));
    copy_in_userspace(proc,&spath,path,len > sizeof(spath) ? sizeof(spath) : len);

    DEBUG(proc->is_debug,"Trying to connect to socket %s from proc %d",spath.sun_path,proc->id);

    int status = sockets::connect(fd_s,&spath);

    DEBUG(proc->is_debug,"Socket is connected %s from proc %d",spath.sun_path,proc->id);

    return {0,status,0};
}

syscall_ret_t sys_bind(int fd, struct sockaddr_un* path, int len) {

    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!path)
        return {0,EINVAL,0};

    if(!fd_s)
        return {0,EBADF,0};

    struct sockaddr_un spath;

    memset(&spath,0,sizeof(spath));
    copy_in_userspace(proc,&spath,path,len > sizeof(spath) ? sizeof(spath) : len);

    DEBUG(proc->is_debug,"Binding socket from fd %d to %s from proc %d",fd,spath.sun_path,proc->id);

    int status = sockets::bind(fd_s,&spath);

    return {0,status,0};
}

syscall_ret_t sys_accept(int fd, struct sockaddr_un* path, int len) {

    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    struct sockaddr_un spath;

    if(path) {
        memset(&spath,0,sizeof(spath));
        copy_in_userspace(proc,&spath,path,len > sizeof(spath) ? sizeof(spath) : len);
    }

    if(!fd_s)
        return {1,EBADF,0};

    int status = sockets::accept(fd_s,path != 0 ? &spath : 0);

    DEBUG(proc->is_debug,"Accepting socket %s on fd %d from proc %d",fd_s->path,fd,proc->id);

    if(path)
        copy_in_userspace(proc,path,&spath,len > sizeof(spath) ? sizeof(spath) : len);

    return {1,status < 0 ? +status : 0,status};
}

#define SOCK_NONBLOCK  04000
#define SOCK_CLOEXEC   02000000

syscall_ret_t sys_socket(int family, int type, int protocol) {
    arch::x86_64::process_t* proc = CURRENT_PROC;

    if(family != AF_UNIX)
        return {0,ENOSYS,0};

    int new_fd = vfs::fdmanager::create(proc);

    userspace_fd_t* new_fd_s = vfs::fdmanager::search(proc,new_fd);
    memset(new_fd_s->path,0,2048);

    new_fd_s->offset = 0;
    new_fd_s->queue = 0;
    new_fd_s->pipe = 0;
    new_fd_s->pipe_side = 0;
    new_fd_s->cycle = 0;
    new_fd_s->is_a_tty = 0;
    new_fd_s->is_listen = 0;
    new_fd_s->flags = (type & SOCK_NONBLOCK) ? O_NONBLOCK : 0;

    new_fd_s->state = USERSPACE_FD_STATE_SOCKET;

    DEBUG(proc->is_debug,"Creating socket on fd %d from proc %d",new_fd,proc->id);

    return {1,0,new_fd};
}

syscall_ret_t sys_listen(int fd, int backlog) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!fd_s)
        return {0,EBADF,0};

    if(fd_s->state == USERSPACE_FD_STATE_SOCKET) {
        if(!fd_s->read_socket_pipe) { // not connected
            fd_s->is_listen = 1;
        }
    } else
        return {0,ENOTSOCK,0};
    
    return {0,0,0};
}
