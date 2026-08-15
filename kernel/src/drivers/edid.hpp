#pragma once

#include <cstdint>

namespace edid {

    inline static bool is_valid_monitor_desc(std::uint8_t* desc) {
        if(desc[0] == 0x00 && desc[1] == 0x00 && desc[2] == 0x00 && desc[3] == 0xFC && desc[4] == 0x00) 
            return true;
        return false;
    }

    struct __attribute__((packed)) raw_edid_info {
        std::uint64_t padding;
        std::uint16_t manufacter_id;
        std::uint16_t monitor_edid_id;
        std::uint32_t serial_number;
        std::uint8_t manufacture_week;
        std::uint8_t manufacture_year;
        std::uint8_t edid_version;
        std::uint8_t edid_revision;
        std::uint8_t video_input_type;
        std::uint8_t maximum_horizontal_size;
        std::uint8_t maximum_vertical_size;
        std::uint8_t gamma_factor;
        std::uint8_t dpms_flags;
        char chroma_information[10];
        std::uint8_t established_timings;
        std::uint8_t second_established_timings;
        std::uint8_t manufacter_timind;
        std::uint16_t standard_timings[8];
        char timing_desc[18];
        char timing_desc2[18];
        char timing_desc3[18];
        char timing_desc4[18];
        std::uint8_t unused;
        std::uint8_t checksum;
    };
    static_assert(sizeof(raw_edid_info) == 128, "mmmm");

    const char* get_monitor_name(void* edid); // should be freed later
    bool get_monitor_size(void* edid, std::uint8_t* width, std::uint8_t* height);

}