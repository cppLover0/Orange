
#include <arch/x86_64/interrupts/idt.hpp>
#include <cstdint>

#include <etc/list.hpp>

#pragma once

void signal_ret();

typedef long clock_t;

union sigval {
	int sival_int;
	void *sival_ptr;
};

typedef uint32_t __uint32_t;
typedef uint64_t __uint64_t;
typedef uint16_t __uint16_t;
typedef int pid_t;
typedef int uid_t;




#define _SIGSET_NWORDS (1024 / (8 * sizeof (unsigned long int)))
typedef struct
{
  unsigned long long __val;
} __sigset_t;

typedef __sigset_t sigset_t;

void signal_ret_sigmask(sigset_t* sigmask);

#define __sigmask(sig) \
  (1UL << (((sig) - 1) % 64))

/* Return the word index for SIG.  */
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


struct siginfo_t {
    int      si_signo;  /* Номер сигнала */
    int      si_errno;  /* Значение errno */
    int      si_code;   /* Код сигнала */
    pid_t    si_pid;    /* Идентификатор процесса, пославшего сигнал */
    uid_t    si_uid;    /* Реальный идентификатор пользователя процесса, пославшего сигнал */
    int      si_status; /* Выходное значение или номер сигнала */
    clock_t  si_utime;  /* Занятое пользователем время */
    clock_t  si_stime;  /* Использованное системное время */
    sigval si_value;  /* Значение сигнала */
    int      si_int;    /* Сигнал  POSIX.1b */
    void *   si_ptr;    /* Сигнал  POSIX.1b */
    void *   si_addr;   /* Адрес в памяти, приводящий к ошибке */
    int      si_band;   /* Общее событие */
    int      si_fd;     /* Описатель файла */
};

# define __ctx(fld) fld

struct _libc_fpxreg
{
  unsigned short int __ctx(significand)[4];
  unsigned short int __ctx(exponent);
  unsigned short int __glibc_reserved1[3];
};

typedef struct {
               void  *ss_sp;     /* Base address of stack */
               int    ss_flags;  /* Flags */
               size_t ss_size;   /* Number of bytes in stack */
           } stack_t;


struct _libc_xmmreg
{
  __uint32_t	__ctx(element)[4];
};

struct _libc_fpstate
{
  /* 64-bit FXSAVE format.  */
  __uint16_t		__ctx(cwd);
  __uint16_t		__ctx(swd);
  __uint16_t		__ctx(ftw);
  __uint16_t		__ctx(fop);
  __uint64_t		__ctx(rip);
  __uint64_t		__ctx(rdp);
  __uint32_t		__ctx(mxcsr);
  __uint32_t		__ctx(mxcr_mask);
  struct _libc_fpxreg	_st[8];
  struct _libc_xmmreg	_xmm[16];
  __uint32_t		__glibc_reserved1[24];
};

#define __NGREG 23

#define __pollution(n) n

struct _fpxreg {
	unsigned short __pollution(significand)[4];
	unsigned short __pollution(exponent);
	unsigned short __padding[3];
};

struct _xmmreg {
	uint32_t __pollution(element)[4];
};

struct _fpstate {
	uint16_t __pollution(cwd);
	uint16_t __pollution(swd);
	uint16_t __pollution(ftw);
	uint16_t __pollution(fop);
	uint64_t __pollution(rip);
	uint64_t __pollution(rdp);
	uint32_t __pollution(mxcsr);
	uint32_t __pollution(mxcr_mask);
	struct _fpxreg _st[8];
	struct _xmmreg _xmm[16];
	uint32_t __padding[24];
};

typedef struct {
	unsigned long __pollution(gregs)[__NGREG];
	struct _fpstate *__pollution(fpregs);
	unsigned long __reserved1[8];
} mcontext_t;

/* Structure to describe FPU registers.  */
typedef struct _libc_fpstate *fpregset_t;

struct sigtrace {
    mcontext_t* mctx;
    sigset_t sigset;
    void* fpu_state;
    struct sigtrace* next;
};

struct sigaction
  {
    /* Signal handler.  */
#if 1
    union
      {
	/* Used if SA_SIGINFO is not set.  */
	void (*sa_handler)(int);
	/* Used if SA_SIGINFO is set.  */
	void (*sa_sigaction) (int, siginfo_t *, void *);
      }
    __sigaction_handler;
# define sa_handler	__sigaction_handler.sa_handler
# define sa_sigaction	__sigaction_handler.sa_sigaction
#else
    __sighandler_t sa_handler;
#endif

    /* Additional set of signals to be blocked.  */
    sigset_t sa_mask;

    /* Special flags.  */
    int sa_flags;

    /* Restore handler.  */
    void (*sa_restorer) (void);
  };

typedef struct {
    std::uint16_t sig;
} pending_signal_t;

class signalmanager {
private:
    pending_signal_t* sigs = 0;
    Lists::Bitmap* bitmap = 0;
    int size = 0;
    int total_size = 0;
    locks::spinlock lock;

    int allocate() {
        for(int i = 0; i < 128; i++) {
            if(!this->bitmap->test(i))
                return i;
        }
        return -1;
    }

    int find_free() {
        for(int i = 0; i < 128; i++) {
            if(this->bitmap->test(i))
                return i;
        }
        return -1;
    }

public:

    signalmanager() {
        this->total_size = 128;
        sigs = new pending_signal_t[this->total_size];
        memset(sigs,0,sizeof(pending_signal_t) * this->total_size);
        bitmap = new Lists::Bitmap(this->total_size);
    }

    ~signalmanager() {
        delete (void*)this->sigs;
        delete (void*)this->bitmap;
    }

    int pop(pending_signal_t* out) {
        this->lock.lock();
        int idx = find_free();
        if(idx == -1) { this->lock.unlock();
            return -1; } 
        memcpy(out,&sigs[idx],sizeof(pending_signal_t));
        this->bitmap->clear(idx);
        this->lock.unlock();
        return 0;
    }

    int push(pending_signal_t* src) {
        this->lock.lock();
        int idx = allocate();
        if(idx == -1) { this->lock.unlock();
            return -1; }
        memcpy(&sigs[idx],src,sizeof(pending_signal_t));
        this->bitmap->set(idx);
        this->lock.unlock();
        return 0;
    }

    int is_not_empty_sigset(sigset_t* sigsetz) {
        this->lock.lock();

        // ban sigset signals :p
        for(int i = 0; i < 128; i++) {
            if(this->bitmap->test(i) && !__sigismember(sigsetz,sigs[i].sig)) {
                this->lock.unlock(); // great there's unbanned pending signal
                return 1;
            }
        }

        this->lock.unlock();
        return 0;
    }

    int popsigset(pending_signal_t* out, sigset_t* sigsetz) {
        this->lock.lock();
        int idx = -1;
        for(int i = 0; i < 128; i++) {
            if(this->bitmap->test(i) && (sigs[i].sig == 9 || !__sigismember(sigsetz,sigs[i].sig))) {
                idx = i;
                break;
            }
        }
        if(idx == -1) {
            this->lock.unlock();
            return -1;
        }
            
        memcpy(out,&sigs[idx],sizeof(pending_signal_t));
        this->bitmap->clear(idx);
        this->lock.unlock();
        return 0;
    }

    int is_not_empty() {
        this->lock.lock();
        int idx = find_free();
        if(idx == -1) { this->lock.unlock();
            return 0; }
        this->lock.unlock();
        return 1;
    }

};