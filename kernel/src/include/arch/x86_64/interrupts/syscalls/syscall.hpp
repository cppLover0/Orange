
#include <stdint.h>

#define STAR_MSR 0xC0000081
#define LSTAR 0xC0000082
#define STAR_MASK 0xC0000084
#define EFER 0xC0000080

typedef struct {
    uint32_t num;
    void* func;
} syscall_t;

class Syscall {
public:
    static void Init();
};