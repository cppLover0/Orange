
#include <stdint.h>

typedef struct {
    char file_name[100];
    char file_mode[8];
    char owner_id[8];
    char group_id[8];
    char file_size[12];
    char last_modific[12];
    char checksum[8];
    char type;
    char name_linked[100];
    char ustar[6];
    char ustar_ver[2];
    char owner_name[32];
    char group_name[32];
    char dev_major_num[8];
    char dev_minor_num[8];
    char filename_prefix[155];
} __attribute__((packed)) ustar_t;

class USTAR {
public:
    static void ParseAndCopy();
};