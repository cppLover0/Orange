
extern "C" void Panic(const char* msg);

#define Assert(condition,msg) if(!condition) {\
        Log(msg);    \
        return; \
    }

#define pAssert(condition,msg) if(!condition) {\
        Panic(msg); \
        __builtin_unreachable(); \
    }