
#include <stdint.h>

#ifndef ORANGETTY_CONFIG
#define ORANGETTY_CONFIG

#define DEFAULT_BG 0
#define DEFAULT_FG 0xDDDDDDDD
#define DEFAULT_FG_BRIGHT 0xFFFFFFFF

#define TTX_CREATE 0xFFFF0002

class OrangeTTY_Config {
private:
    uint32_t bg;
    uint32_t fg;
    uint32_t fg_bright;
public:

    uint32_t* BackGround() {
        return &bg;
    }

    uint32_t* ForeGround() {
        return &fg;
    }

    uint32_t* ForeGroundBright() {
        return &fg_bright;
    }

    OrangeTTY_Config() {
        bg = DEFAULT_BG;
        fg = DEFAULT_FG;
        fg_bright = DEFAULT_FG_BRIGHT;
    }

};

#endif