
#include <stdint.h>
#include <drivers/cmos/cmos.hpp>
#include <drivers/io/io.hpp>

static inline uint8_t cmos_read(uint8_t reg) {
    IO::OUT(CMOS_ADDRESS, reg,1);
    return IO::IN(CMOS_DATA,1);
}

static inline uint8_t bcd_to_binary(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd / 16) * 10);
}

uint8_t CMOS::Second() {
    uint8_t seconds = cmos_read(CMOS_SECONDS);
    return bcd_to_binary(seconds);
}

uint8_t CMOS::Minute() {
    uint8_t minutes = cmos_read(CMOS_MINUTES);
    return bcd_to_binary(minutes);
}

uint8_t CMOS::Hour() {
    uint8_t hours = cmos_read(CMOS_HOURS);
    return bcd_to_binary(hours);
}

uint8_t CMOS::Day() {
    uint8_t day = cmos_read(CMOS_DAY);
    return bcd_to_binary(day);
}

uint8_t CMOS::Month() {
    uint8_t month = cmos_read(CMOS_MONTH);
    return bcd_to_binary(month);
}

uint16_t CMOS::Year() {
    uint8_t year = cmos_read(CMOS_YEAR);
    return 2000 + bcd_to_binary(year); 
}