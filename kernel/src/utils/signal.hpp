#pragma once
#include <cstdint>
#include <utils/bitmap.hpp>
#include <generic/lock/spinlock.hpp>

#define SIGHUP    1
#define SIGQUIT   3
#define SIGTRAP   5
#define SIGIOT    SIGABRT
#define SIGBUS    7
#define SIGKILL   9
#define SIGUSR1   10
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGABRT 6
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF         27
#define SIGWINCH  28
#define SIGPOLL   29
#define SIGSYS    31
#define SIGUNUSED SIGSYS
#define SIGCANCEL 32
#define SIGTIMER  33

typedef struct
{
  unsigned long long __val;
} __sigset_t;

typedef __sigset_t sigset_t;

void signal_ret_sigmask(sigset_t* sigmask);

#define __sigmask(sig) \
  (1UL << (((sig) - 1) % 64))

static inline int
__sigword (int sig)
{
  return (sig - 1) / 64;
}

static inline int
__sigismember (sigset_t *set, int sig)
{
  return (set->__val & (1 << (sig - 1))) ? 1 : 0;
}

struct sig_stack {
    void  *ss_sp;     /* Base address of stack */
    int    ss_flags;  /* Flags */
    std::size_t ss_size;   /* Number of bytes in stack */
};

class signal_manager {
private:
    locks::spinlock lock;
    utils::bitmap* bitmap;
    std::uint8_t* sigs;
public:
    signal_manager() {
        bitmap = new utils::bitmap(256);
        sigs = new std::uint8_t[256];
    }

    std::int8_t pop(sigset_t* sigset) {
        this->lock.lock();
        for(int i = 0;i < 256; i++) {
            if(this->bitmap->test(i) && (this->sigs[i] == 9 || !__sigismember(sigset,this->sigs[i]))) {
                std::uint8_t saved_sig = this->sigs[i];
                this->bitmap->clear(i);
                this->lock.unlock();
                return (std::int8_t)saved_sig;
            }
        }
        this->lock.unlock();
        return -1;
    }

    void push(std::uint8_t sig) {
        this->lock.lock();
        for(int i = 0;i < 256; i++) {
            if(!this->bitmap->test(i)) {
                this->bitmap->set(i);
                this->sigs[i] = sig;
                this->lock.unlock();
                return;
            }
        }
        this->lock.unlock(); 
    }

    bool is_not_empty_sigset(sigset_t* sigsetz) {
        this->lock.lock();

        // ban sigset signals :p
        for(int i = 0; i < 128; i++) {
            if(this->bitmap->test(i) && !__sigismember(sigsetz,this->sigs[i])) {
                this->lock.unlock(); // great there's unbanned pending signal
                return 1;
            }
        }

        this->lock.unlock();
        return 0;
    }

};
