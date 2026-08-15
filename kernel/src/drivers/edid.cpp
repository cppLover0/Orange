#include <drivers/edid.hpp>
#include <generic/heap.hpp>
#include <klibc/string.hpp>

const char* edid::get_monitor_name(void* edid) {
    if(edid == nullptr)
        return nullptr;

    auto info = (edid::raw_edid_info*)edid;

    const char* start = nullptr;

    if(edid::is_valid_monitor_desc((std::uint8_t*)info->timing_desc) == true) {
        start = &info->timing_desc[5];
        goto found;
    }
    
    if(edid::is_valid_monitor_desc((std::uint8_t*)info->timing_desc2) == true) {
        start = &info->timing_desc2[5];
        goto found;
    }

    if(edid::is_valid_monitor_desc((std::uint8_t*)info->timing_desc3) == true) {
        start = &info->timing_desc3[5];
        goto found;
    }

    if(edid::is_valid_monitor_desc((std::uint8_t*)info->timing_desc4) == true) {
        start = &info->timing_desc4[5];
        goto found;
    }
    
found:

    if(start == nullptr)
        return nullptr;

    char* str = new char[14];
    int length = 0;
    for (int i = 0; i < 13; i++) {
        char c = static_cast<char>(start[i]);
        
        if (c == 0x0A) {
            break;
        }
        str[length++] = c;
    }
    
    while (length > 0 && str[length - 1] == ' ') {
        length--;
    }

    str[length] = '\0';
    return str;
}

bool edid::get_monitor_size(void* edid, std::uint8_t* width, std::uint8_t* height) {
    if(edid == nullptr)
        return false;

    auto info = (edid::raw_edid_info*)edid;

    *width = info->maximum_horizontal_size;
    *height = info->maximum_vertical_size;

    return true;
}