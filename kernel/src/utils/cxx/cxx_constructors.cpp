
extern void (*__init_array[])();
extern void (*__init_array_end[])();

#include <utils/cxx/cxx_constructors.hpp>
#include <cstdint>

void utils::cxx::init_constructors() {
    for (std::size_t i = 0; &__init_array[i] != __init_array_end; i++) {
        __init_array[i]();
    }
}