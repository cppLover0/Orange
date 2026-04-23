#pragma once

#include <generic/scheduling.hpp>
#include <utils/errno.hpp>
#include <utils/gobject.hpp>
#include <generic/arch.hpp>

#if defined(__x86_64__)
#include <arch/x86_64/cpu/sse.hpp>
#endif

#define is_signal_ret(proc) if(proc->sig->is_not_empty_sigset(&proc->sigset))

inline static void temp_sigset(thread* proc, sigset_t* old) {
    proc->is_restore_sigset = true;
    proc->temp_sigset = *old;
}

inline static void signal_ret(thread* proc) {
    proc->ctx = proc->signal_ctx;
#if defined(__x86_64__)
    x86_64::sse::save(proc->sse_ctx);
    proc->ctx.rax = -EINTR;
#endif
    arch::enable_paging(gobject::kernel_root);
    proc->should_not_save_ctx = true;
    process::yield();
    assert(0, "bro what ???");
    __builtin_unreachable();
}