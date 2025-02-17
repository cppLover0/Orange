
#define Assert(condition,msg) if(!condition) {\
    Log(msg);    \
    return; \
    }