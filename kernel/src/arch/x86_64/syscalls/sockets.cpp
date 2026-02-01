
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
        if(!memcmp(current->path,path->sun_path,sizeof(path->sun_path))) {
            return current;
        }
        current = current->next;
    }
    return 0;
}

socket_node_t* sockets::find(char* path) {
    socket_node_t* current = head;
    int is_abstract = 0;
    if(path[0] == '\0') {// abstract path
        path += 1; 
        is_abstract = 1;
    }
    while(current) {
        if(!strcmp(is_abstract == 1 ? current->path + 1 : current->path,path)) {
            return current;
        }
        current = current->next;
    }
    return 0;
}

socket_node_t* find_node_str(char* path) {
    return sockets::find(path);
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
        return -EINVAL;

    if(path->sun_family != AF_UNIX)
        return -ENOSYS;

    memset(fd->path,0,2048);
    memcpy(fd->path,path->sun_path,sizeof(path->sun_path));

    vfs::stat_t stat;
    if(vfs::vfs::stat(fd,&stat) == 0) /* Check is there vfs object with some name */
        return -EEXIST; 

    socket_spinlock.lock();

    if(find_node(path)) { socket_spinlock.unlock();
        return -EEXIST; }

    socket_node_t* new_node = new socket_node_t;
    memset(new_node->path,0,128);
    memcpy(new_node->path,path->sun_path,108);
    new_node->is_used = 1;
    new_node->socket_counter = 0;
    new_node->next = head;

    fd->binded_socket = (void*)new_node;
    head = new_node;
    socket_spinlock.unlock();

    return 0;

}

int sockets::connect(userspace_fd_t* fd, struct sockaddr_un* path) {

    if(!fd || !path)
        return -EINVAL;

    if(path->sun_family != AF_UNIX)
        return -ENOSYS;

    //socket_spinlock.lock();

    socket_node_t* node = find_node(path);
    if(!node) { //socket_spinlock.unlock();
        return -ENOENT; }

    socket_pending_obj_t* pending = new socket_pending_obj_t;
    pending->son = fd;
    pending->next = node->pending_list;
    pending->is_accepted.unlock();
    node->pending_list = pending;

    memcpy(fd->path,node->path,sizeof(node->path));

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

                new_fd_s->read_socket_pipe->ucred_pass = new vfs::ucred_manager;
                new_fd_s->write_socket_pipe->ucred_pass = new vfs::ucred_manager;

                new_fd_s->socket_pid = pending_connections->son->pid;
                new_fd_s->socket_uid = pending_connections->son->uid;
                
                pending_connections->son->read_socket_pipe = new_fd_s->read_socket_pipe;
                pending_connections->son->write_socket_pipe = new_fd_s->write_socket_pipe;

                //Log::SerialDisplay(LEVEL_MESSAGE_INFO,"together fd %d fd2 %d\n",pending_connections->son->index,new_fd_s->index);

                if(path)
                    memcpy(path->sun_path,node->path,sizeof(path->sun_path));

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

long long sys_connect(int fd, struct sockaddr_un* path, int len) {

    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    
    if(!fd_s)
        return -EBADF;

    if(!path)
        return -EINVAL;

    DEBUG(1,"Trying to connect to socket %s (fd %d) from proc %d",path->sun_path,fd,proc->id);

    int status = sockets::connect(fd_s,path);

    DEBUG(proc->is_debug,"Socket is connected %s from proc %d, status %d",path->sun_path,proc->id,status);

    return status;
}

long long sys_bind(int fd, struct sockaddr_un* path, int len) {

    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!path)
        return -EINVAL;

    if(!fd_s)
        return -EBADF;

    struct sockaddr_un spath = *path;

    DEBUG(proc->is_debug,"Binding socket from fd %d to %s from proc %d",fd,spath.sun_path + 1,proc->id);

    int status = sockets::bind(fd_s,&spath);

    memcpy(fd_s->path,spath.sun_path,sizeof(spath.sun_path));

    return status;
}

long long sys_accept(int fd, struct sockaddr_un* path, int* zlen) {

    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!zlen)
        return -EINVAL;

    SYSCALL_IS_SAFEZ(zlen,4096);

    int len = sizeof(sockaddr_un);

    struct sockaddr_un spath;

    if(path) {
        memset(&spath,0,sizeof(spath));
        copy_in_userspace(proc,&spath,path,len > sizeof(spath) ? sizeof(spath) : len);
    }

    if(!fd_s)
        return -EBADF;


    DEBUG(proc->is_debug,"Accepting socket %s on fd %d from proc %d, path %s",fd_s->path + 1,fd,proc->id,spath.sun_path);

    int status = sockets::accept(fd_s,path != 0 ? &spath : 0);

    if(path)
        copy_in_userspace(proc,path,&spath,len > sizeof(spath) ? sizeof(spath) : len);

    return status;
}

#define SOCK_NONBLOCK  04000
#define SOCK_CLOEXEC   02000000
#define SOCK_STREAM    1
#define SOCK_DGRAM     2

long long sys_socket(int family, int type, int protocol) {
    arch::x86_64::process_t* proc = CURRENT_PROC;

    if(family != AF_UNIX)
        return -EINVAL; // only unix socekts fo rnow

    std::uint8_t socket_type = type & 0xFF;

    if(socket_type != SOCK_STREAM) {
        Log::SerialDisplay(LEVEL_MESSAGE_WARN,"Tried to open non SOCK_STREAM socket which not implemented (socket type %d)\n",socket_type);
        //return {1,ENOSYS,0}; 
    }
        
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

    return new_fd;
}

long long sys_listen(int fd, int backlog) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!fd_s)
        return -EBADF;

    if(fd_s->state == USERSPACE_FD_STATE_SOCKET) {
        if(!fd_s->read_socket_pipe) { // not connected
            fd_s->is_listen = 1;
        }
    } else
        return -ENOTSOCK;
    
    return 0;
}

syscall_ret_t sys_socketpair(int domain, int type_and_flags, int proto) {

    arch::x86_64::process_t* proc = CURRENT_PROC;
    int read_fd = vfs::fdmanager::create(proc);
    int write_fd = vfs::fdmanager::create(proc);
    userspace_fd_t* fd1 = vfs::fdmanager::search(proc,read_fd);
    userspace_fd_t* fd2 = vfs::fdmanager::search(proc,write_fd);

    vfs::pipe* first = new vfs::pipe(0);
    vfs::pipe* second = new vfs::pipe(0);

    fd1->read_socket_pipe = first;
    fd2->write_socket_pipe = first;
    fd1->write_socket_pipe = second;
    fd2->read_socket_pipe = second;

    fd1->other_state = USERSPACE_FD_OTHERSTATE_MASTER;
    fd2->other_state = USERSPACE_FD_OTHERSTATE_MASTER;

    fd1->read_socket_pipe->create(PIPE_SIDE_READ);
    fd1->read_socket_pipe->create(PIPE_SIDE_WRITE);    
    fd1->write_socket_pipe->create(PIPE_SIDE_READ);   
    fd1->write_socket_pipe->create(PIPE_SIDE_WRITE);

    fd1->state = USERSPACE_FD_STATE_SOCKET;
    fd2->state = USERSPACE_FD_STATE_SOCKET;

    DEBUG(proc->is_debug,"Creating socketpair %d:%d from proc %d",read_fd,write_fd,proc->id);

    return {1,read_fd,write_fd};
}

long long sys_getsockname(int fd, struct sockaddr_un* path, int* len) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!fd_s)
        return -EBADF;

    if(path) {
        path->sun_family = AF_UNIX;
        memcpy(path->sun_path,fd_s->path,strlen(fd_s->path) + 1);
        *len = strlen(fd_s->path);
        return 0;
    } else 
        return -EINVAL;
    return 0;
}

#define SO_PEERCRED     17

long long sys_getsockopt(int fd, int layer, int number, int_frame_t* ctx) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!fd_s)
        return -EBADF;

    void* buffer = (void*)ctx->r10;

    int* optlen = (int*)ctx->r8;

    SYSCALL_IS_SAFEZ(buffer,4096);
    SYSCALL_IS_SAFEZ(optlen,4096);

    if(!buffer)
        return -EINVAL;

    if(fd_s->state != USERSPACE_FD_STATE_SOCKET || !fd_s->read_socket_pipe)
        return -EBADF;

    switch(number) {
        
        case SO_PEERCRED: {
            struct ucred* cred = (struct ucred*)buffer;
            cred->pid = fd_s->socket_pid;
            cred->uid = fd_s->socket_uid;
            cred->gid = 0; // not implemented

            if(optlen)
                *optlen = sizeof(struct ucred);

            return 0;
        };

        default: {
            return -ENOSYS;
        };
    
    }

    return 0;

}

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

long long sys_shutdown(int sockfd, int how) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,sockfd);

    if(!fd_s)
        return -EBADF;

    if(fd_s->state != USERSPACE_FD_STATE_SOCKET)
        return -ENOTSOCK;

    if(fd_s->is_listen || fd_s->read_socket_pipe == 0)
        return -ENOTCONN;

    switch(how) {
    case SHUT_RD:
    case SHUT_RDWR:
        fd_s->read_socket_pipe->lock.lock();
        fd_s->write_socket_pipe->lock.lock();
        fd_s->read_socket_pipe->is_closed.test_and_set();
        fd_s->write_socket_pipe->is_closed.test_and_set();
        fd_s->read_socket_pipe->lock.unlock();
        fd_s->write_socket_pipe->lock.unlock();
        DEBUG(proc->is_debug,"shutdown %d from proc %d",sockfd,proc->id);
        break;
    case SHUT_WR:
        return -ENOSYS;
    default:
        return -EINVAL;
    }

    return 0;
}
