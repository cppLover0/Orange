
#define STAR_MSR    0xC0000081 
#define LSTAR       0xC0000082 
#define STAR_MASK   0xC0000084
#define EFER        0xC0000080 

struct syscall_item {
    bool is_ctx_passed;
    long long num;
    void* sys;
};

namespace syscall {
    void init();
}