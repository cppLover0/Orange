#pragma once
#define is_power_of_two(n) ((n) > 0 && ((n) & ((n) - 1)) == 0)
#define div_remain(value, full) (value % full)