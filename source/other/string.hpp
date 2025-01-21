
#pragma once

#include <stdint.h>

class String {
public:

    static uint64_t strlen(char* str) {
        uint64_t len = 0; 
        while(str[len])
            len++;
        return len;
    }

    static char memcmp(const void* f,const void* s,uint64_t size) {
        const uint8_t* a = (const uint8_t*)f;
        const uint8_t* b = (const uint8_t*)s;

        for(uint64_t i = 0;i < size;i++) {
            if(a[i] < b[i])
                return -1;
            else if(b[i] < a[i])
                return 1;
        }

        return 0;

    }

    static void memset(void* b,char v,uint64_t size) {
        for(uint64_t i = 0;i < size;i++)
            ((uint8_t*)b)[i] = v;
    }

    static void memset16(void* b,short v,uint64_t size) {
        for(uint64_t i = 0;i < size;i += 2) 
            ((uint16_t*)b)[i] = v;
    }

    static void memset32(void* b,int v,uint64_t size) {
        for(uint64_t i = 0;i < size;i += 4) 
            ((uint32_t*)b)[i] = v;
    }

    static void memset64(void* b,long v,uint64_t size) {
        for(uint64_t i = 0;i < size;i += 8)
            ((uint64_t*)b)[i] = v;
    }

    static char* itoa(uint64_t value, char* str, int base ) {
        char * rc;
        char * ptr;
        char * low;
        if ( base < 2 || base > 36 )
        {
            *str = '\0';
            return str;
        }
        rc = ptr = str;
        if ( value < 0 && base == 10 )
        {
            *ptr++ = '-';
        }
        low = ptr;
        do
        {
            *ptr++ = "ZYXWVUTSRQPONMLKJIHGFEDCBA9876543210123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[35 + value % base];
            value /= base;
        } while ( value );
        *ptr-- = '\0';
        while ( low < ptr )
        {
            char tmp = *low;
            *low++ = *ptr;
            *ptr-- = tmp;
        }
        return rc;
    }

};