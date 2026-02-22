
namespace drivers {

    #define PS2_DATA 0x60
    #define PS2_STATUS 0x64
    #define PS2_CMD 0x64
    #define CMD_DISABLE_MOUSE     0xA7
    #define CMD_DISABLE_KEYBOARD  0xAD
    #define CMD_READ_CONFIG       0x20
    #define CMD_WRITE_CONFIG      0x60
    #define CMD_SELF_TEST         0xAA
    #define CMD_ENABLE_MOUSE      0xA8
    #define CMD_ENABLE_KEYBOARD   0xAE

    class ps2 {
    public:
        static void init();
    };
};