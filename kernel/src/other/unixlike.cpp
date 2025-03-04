
#include <drivers/cmos/cmos.hpp>
#include <other/unixlike.hpp>
#include <stdint.h>

bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int month, int year) {
    int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2 && isLeapYear(year)) {
        return 29; 
    }
    return days[month - 1];
}

uint64_t convertToUnixTime() {
    int year = CMOS::Year();
    int month = CMOS::Month();
    int day = CMOS::Day();
    int hour = CMOS::Hour();
    int minute = CMOS::Minute();
    int second = CMOS::Second();

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