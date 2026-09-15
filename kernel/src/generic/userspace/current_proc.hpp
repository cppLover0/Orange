#if defined(__x86_64__)
#include <arch/x86_64/cpu_local.hpp>
#define current_proc (CPU_LOCAL_READ(current_thread))
#else
#error "todo"
#endif