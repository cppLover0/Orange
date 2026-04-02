#include <cstdint>
#include <utils/linux.hpp>

namespace fbdev {

    struct fbdev_arg {
        fb_fix_screeninfo fix;
        fb_var_screeninfo var;
    };

    void init();
}