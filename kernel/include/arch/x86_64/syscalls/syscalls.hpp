
#include <cstdint>

#pragma once

#include <generic/mm/paging.hpp>

#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/scheduling.hpp>
#include <etc/bootloaderinfo.hpp>
#include <etc/libc.hpp>

#include <generic/mm/vmm.hpp>
#include <etc/etc.hpp>

#include <arch/x86_64/cpu/data.hpp>

inline void copy_in_userspace(arch::x86_64::process_t* proc,void* dest, void* src, std::uint64_t size) {

    void* ddest = 0;
    void* ssrc = 0;

    if((std::uint64_t)dest > BootloaderInfo::AccessHHDM()) {
        ddest = dest;
    }

    if((std::uint64_t)src > BootloaderInfo::AccessHHDM()) {
        ssrc = src;
    }

    if((std::uint64_t)dest < BootloaderInfo::AccessHHDM()) {
        vmm_obj_t* vmm_object = memory::vmm::getlen(proc,(std::uint64_t)dest);

        if(!vmm_object)
            return;

        uint64_t need_phys = vmm_object->phys + ((std::uint64_t)dest - vmm_object->base);
        ddest = Other::toVirt(need_phys);
    }

    if((std::uint64_t)src < BootloaderInfo::AccessHHDM()) {
        vmm_obj_t* vmm_object = memory::vmm::getlen(proc,(std::uint64_t)src);

        if(!vmm_object)
            return;

        uint64_t need_phys = vmm_object->phys + ((std::uint64_t)src - vmm_object->base);
        ssrc = Other::toVirt(need_phys);
    }

    memcpy(ddest,ssrc,size);
}

inline void copy_in_userspace_string(arch::x86_64::process_t* proc,void* dest, void* src, std::uint64_t size) {

    void* ddest = 0;
    void* ssrc = 0;

    if((std::uint64_t)dest > BootloaderInfo::AccessHHDM()) {
        ddest = dest;
    }

    if((std::uint64_t)src > BootloaderInfo::AccessHHDM()) {
        ssrc = src;
    }

    if((std::uint64_t)dest < BootloaderInfo::AccessHHDM()) {
        vmm_obj_t* vmm_object = memory::vmm::getlen(proc,(std::uint64_t)dest);

        if(!vmm_object)
            return;

        uint64_t need_phys = vmm_object->phys + ((std::uint64_t)dest - vmm_object->base);
        ddest = Other::toVirt(need_phys);
    }

    if((std::uint64_t)src < BootloaderInfo::AccessHHDM()) {
        vmm_obj_t* vmm_object = memory::vmm::getlen(proc,(std::uint64_t)src);

        if(!vmm_object)
            return;

        uint64_t need_phys = vmm_object->phys + ((std::uint64_t)src - vmm_object->base);
        ssrc = Other::toVirt(need_phys);
    }

    memcpy(ddest,ssrc,strlen((char*)ssrc) > size ? size : strlen((char*)ssrc));
}

inline void zero_in_userspace(arch::x86_64::process_t* proc,void* buf, std::uint64_t size) {
    
    void* bbuf;

    if((std::uint64_t)buf > BootloaderInfo::AccessHHDM()) {
        bbuf = buf;
    }

    if((std::uint64_t)buf < BootloaderInfo::AccessHHDM()) {
        vmm_obj_t* vmm_object = memory::vmm::getlen(proc,(std::uint64_t)buf);

        if(!vmm_object)
            return;

        uint64_t need_phys = vmm_object->phys + ((std::uint64_t)buf - vmm_object->base);
        bbuf = Other::toVirt(need_phys);
    }

    memset(bbuf,0,size);
}

class syscall_safe {
public:

    static inline int is_safe(std::uint64_t ptr) {
        if(ptr >= BootloaderInfo::AccessHHDM())
            return 0;
        return 1;
    }

    static inline int is_safe(void* ptr) {
        if((std::uint64_t)ptr >= BootloaderInfo::AccessHHDM())
            return 0;
        return 1;
    }

    static inline int is_safe(std::uint64_t ptr,std::uint64_t size) {
        if(ptr + size >= BootloaderInfo::AccessHHDM())
            return 0;
        return 1;
    }

    static inline int is_safe(void* ptr,std::uint64_t size) {
        if((std::uint64_t)ptr + size >= BootloaderInfo::AccessHHDM())
            return 0;
        return 1;
    }

};

#define SYSCALL_IS_SAFEA(x,sz) \
    if(!syscall_safe::is_safe(x,sz)) \
        return {0,EFAULT,0};

#define SYSCALL_IS_SAFEB(x,sz) \
    if(!syscall_safe::is_safe(x,sz)) \
        return {0,EFAULT,0};

inline arch::x86_64::process_t* __proc_get() {
    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc;
    return proc;
}

#define CURRENT_PROC __proc_get()
#define FIND_FD(x) vfs::fdmanager::search(proc,x)

#define SYSCALL_DISABLE_PREEMPT() asm volatile("cli");
#define SYSCALL_ENABLE_PREEMPT() asm volatile("sti");

typedef struct {
    std::int8_t is_rdx_ret;
    std::int32_t ret;
    std::int64_t ret_val;
} syscall_ret_t;

#define STAR_MSR 0xC0000081
#define LSTAR 0xC0000082
#define STAR_MASK 0xC0000084
#define EFER 0xC0000080

#define MAP_FAILED ((void *)(-1))
#define MAP_FILE    0x00
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANON      0x20
#define MAP_ANONYMOUS 0x20

#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4

extern "C" void syscall_handler();

/* File,Pipes and etc. */
syscall_ret_t sys_openat(int dirfd, const char* path, int flags, int_frame_t* ctx);
syscall_ret_t sys_ioctl(int fd, unsigned long request, void *arg);
syscall_ret_t sys_write(int fd, const void *buf, size_t count);
syscall_ret_t sys_read(int fd, void *buf, size_t count);
syscall_ret_t sys_seek(int fd, long offset, int whence);
syscall_ret_t sys_dup2(int fd, int flags, int newfd);
syscall_ret_t sys_stat(int fd, void* out, int flags);
syscall_ret_t sys_dup(int fd, int flags);
syscall_ret_t sys_pipe(int flags);
syscall_ret_t sys_close(int fd);

syscall_ret_t sys_create_dev(std::uint64_t requestandnum, char* slave_path, char* master_path);
syscall_ret_t sys_create_ioctl(char* path, std::uint64_t write_and_read_req, std::uint32_t size);
syscall_ret_t sys_setupmmap(char* path, std::uint64_t addr, std::uint64_t size, int_frame_t* ctx);

syscall_ret_t sys_setup_tty(char* path);
syscall_ret_t sys_isatty(int fd);

syscall_ret_t sys_ptsname(int fd, void* out, int max_size);

syscall_ret_t sys_setup_ring_bytelen(char* path, int bytelen);
syscall_ret_t sys_read_dir(int fd, void* buffer);

syscall_ret_t sys_fcntl(int fd, int request, std::uint64_t arg);

syscall_ret_t sys_fchdir(int fd);

syscall_ret_t sys_poll(struct pollfd *fds, int count, int timeout);
syscall_ret_t sys_readlinkat(int dirfd, const char* path, void* buffer, int_frame_t* ctx);

syscall_ret_t sys_link(char* old_path, char* new_path);
syscall_ret_t sys_mkdirat(int dirfd, char* path, int mode);
syscall_ret_t sys_chmod(char* path, int mode);

syscall_ret_t sys_ttyname(int fd, char *buf, size_t size);
syscall_ret_t sys_rename(char* old, char* newp);

/* Process */
syscall_ret_t sys_mmap(std::uint64_t hint, std::uint64_t size, int fd0, int_frame_t* ctx);
syscall_ret_t sys_free(void *pointer, size_t size);
syscall_ret_t sys_libc_log(const char* msg);
syscall_ret_t sys_tcb_set(std::uint64_t fs);
syscall_ret_t sys_exit(int status);

syscall_ret_t sys_iopl(int a, int b ,int c , int_frame_t* ctx);

syscall_ret_t sys_fork(int D, int S, int d, int_frame_t* ctx);

syscall_ret_t sys_access_framebuffer(void* out);

syscall_ret_t sys_exec(char* path, char** argv, char** envp, int_frame_t* ctx);

syscall_ret_t sys_getpid();
syscall_ret_t sys_getppid();

syscall_ret_t sys_gethostname(void* buffer, std::uint64_t bufsize);
syscall_ret_t sys_getcwd(void* buffer, std::uint64_t bufsize);

syscall_ret_t sys_waitpid(int pid, int flags);

syscall_ret_t sys_sleep(long us);

syscall_ret_t sys_alloc_dma(std::uint64_t size);
syscall_ret_t sys_free_dma(std::uint64_t phys);

syscall_ret_t sys_map_phys(std::uint64_t phys, std::uint64_t flags, std::uint64_t size);

syscall_ret_t sys_timestamp();

syscall_ret_t sys_mkfifoat(int dirfd, const char *path, int mode);

syscall_ret_t sys_enabledebugmode();

syscall_ret_t sys_clone(std::uint64_t stack, std::uint64_t rip, int c, int_frame_t* ctx);
syscall_ret_t sys_breakpoint(int num);

syscall_ret_t sys_getpriority(int which, int who);
syscall_ret_t sys_setpriority(int which, int who, int prio);

syscall_ret_t sys_yield();

/* Futex */
syscall_ret_t sys_futex_wait(int* pointer, int excepted);
syscall_ret_t sys_futex_wake(int* pointer);

/* Socket */
syscall_ret_t sys_socket(int family, int type, int protocol);

syscall_ret_t sys_bind(int fd, struct sockaddr_un* path, int len);
syscall_ret_t sys_accept(int fd, struct sockaddr_un* path, int len);
syscall_ret_t sys_connect(int fd, struct sockaddr_un* path, int len);

syscall_ret_t sys_listen(int fd, int backlog);

syscall_ret_t sys_copymemory(void* src, void* dest, int len);

struct pollfd {
    int fd;          
    short events;   
    short revents;  
};

namespace arch {
    namespace x86_64 {
        typedef struct {
            int syscall_num;
            void* syscall_func;
        } syscall_item_t;
        class syscall {
        public:
            static void init();
        };
    }
}
