
#include <cstdint>

namespace arch {
    namespace x86_64 {
        namespace cpu {
            class mp {
            public:
                static void init();
                static void sync(std::uint8_t id);
            };
        }
    }
}