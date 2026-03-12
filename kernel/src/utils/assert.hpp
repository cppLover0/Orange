#pragma once

#include <generic/arch.hpp>
#include <klibc/stdio.hpp>

#define assert(cond, msg,...) if(!(cond)) { klibc::printf("Failed assert at %s:%s:%d \"" msg "\"\n" , __FILE__ ,__FUNCTION__, __LINE__ , ##__VA_ARGS__); arch::panic((char*)"Failed assert"); }