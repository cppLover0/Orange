
#include <stdint.h>

#pragma once

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71
#define CMOS_SECONDS 0x00
#define CMOS_MINUTES 0x02
#define CMOS_HOURS 0x04
#define CMOS_DAY 0x07
#define CMOS_MONTH 0x08
#define CMOS_YEAR 0x09

class CMOS {
public:
    static uint8_t Second();

    static uint8_t Minute();

    static uint8_t Hour();

    static uint8_t Day();

    static uint8_t Month();

    static uint16_t Year();

};