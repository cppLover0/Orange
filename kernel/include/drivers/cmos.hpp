
#include <cstdint>
#include <drivers/io.hpp>

#pragma once

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71
#define CMOS_SECONDS 0x00
#define CMOS_MINUTES 0x02
#define CMOS_HOURS 0x04
#define CMOS_DAY 0x07
#define CMOS_MONTH 0x08
#define CMOS_YEAR 0x09

namespace drivers {
    class cmos {
    public:
        static inline std::uint8_t cmos_read(std::uint8_t reg) {
            drivers::io io;
            io.outb(CMOS_ADDRESS, reg);
            return io.inb(CMOS_DATA);
        }

        static inline std::uint8_t bcd_to_binary(std::uint8_t bcd) {
            return (bcd & 0x0F) + ((bcd / 16) * 10);
        }

        static std::uint8_t second() {
            uint8_t seconds = cmos_read(CMOS_SECONDS);
            return bcd_to_binary(seconds);
        }

        static std::uint8_t minute() {
            uint8_t minutes = cmos_read(CMOS_MINUTES);
            return bcd_to_binary(minutes);
        }

        static std::uint8_t hour() {
            uint8_t hours = cmos_read(CMOS_HOURS);
            return bcd_to_binary(hours);
        }

        static std::uint8_t day() {
            uint8_t day = cmos_read(CMOS_DAY);
            return bcd_to_binary(day);
        }

        static std::uint8_t month() {
            uint8_t month = cmos_read(CMOS_MONTH);
            return bcd_to_binary(month);
        }

        static std::uint16_t year() {
            uint8_t year = cmos_read(CMOS_YEAR);
            return 2000 + bcd_to_binary(year); 
        }
    };
};

inline bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

inline int daysInMonth(int month, int year) {
    int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2 && isLeapYear(year)) {
        return 29; 
    }
    return days[month - 1];
}

inline static uint64_t getUnixTime() {
    int year = drivers::cmos::year();
    int month = drivers::cmos::month();
    int day = drivers::cmos::day();
    int hour = drivers::cmos::hour();
    int minute = drivers::cmos::minute();
    int second = drivers::cmos::second();

    long long daysSinceEpoch = 0;
    for (int y = 1970; y < year; ++y) {
        daysSinceEpoch += isLeapYear(y) ? 366 : 365;
    }

    for (int m = 1; m < month; ++m) {
        daysSinceEpoch += daysInMonth(m, year);
    }

    daysSinceEpoch += (day - 1);

    long long secondsSinceEpoch = daysSinceEpoch * 86400;

    secondsSinceEpoch += hour * 3600;
    secondsSinceEpoch += minute * 60;
    secondsSinceEpoch += second;

    return secondsSinceEpoch;
}