
#pragma once

/*

    Orange status
    0 bit - is initializied
    1 bit - is something wrong
    2 bit - is not supported
    3 bit - is all ok 
    3-20 bits - status code (u16)
    20-31 not used

*/

#define ORANGE_STATUS_INITIALIZIED_STATE (1 << 0)
#define ORANGE_STATUS_SOMETHING_WRONG_STATE (1 << 1)
#define ORANGE_STATUS_NOT_SUPPORTED (1 << 2)
#define ORANGE_STATUS_IS_ALL_OK (1 << 3)

typedef unsigned int orange_status;