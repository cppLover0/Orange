
// simple libc basic memory implementations

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <generic/mm/heap.hpp>

static inline void* memcpy(void* dest, const void* src, size_t n) {
    if (n == 0) return dest;
    
    void* ret = dest;
    
    __asm__ volatile (
        "cld\n"               
        "rep movsb\n"      
        : "+D" (dest), "+S" (src), "+c" (n)
        : 
        : "memory"
    );
    
    return ret;
}

// used from glibc
typedef unsigned long long op_t;
#define OPSIZ 8
typedef char byte;

inline void * __attribute__ ((__optimize__ ("-fno-tree-loop-distribute-patterns")))  memset (void *dstpp, int c, size_t len)
{
  long int dstp = (long int) dstpp;

  if (len >= 8)
    {
      size_t xlen;
      op_t cccc;

      cccc = (unsigned char) c;
      cccc |= cccc << 8;
      cccc |= cccc << 16;
      if (OPSIZ > 4)
	/* Do the shift in two steps to avoid warning if long has 32 bits.  */
	cccc |= (cccc << 16) << 16;

      /* There are at least some bytes to set.
	 No need to test for LEN == 0 in this alignment loop.  */
      while (dstp % OPSIZ != 0)
	{
	  ((byte *) dstp)[0] = c;
	  dstp += 1;
	  len -= 1;
	}

      /* Write 8 `op_t' per iteration until less than 8 `op_t' remain.  */
      xlen = len / (OPSIZ * 8);
      while (xlen > 0)
	{
	  ((op_t *) dstp)[0] = cccc;
	  ((op_t *) dstp)[1] = cccc;
	  ((op_t *) dstp)[2] = cccc;
	  ((op_t *) dstp)[3] = cccc;
	  ((op_t *) dstp)[4] = cccc;
	  ((op_t *) dstp)[5] = cccc;
	  ((op_t *) dstp)[6] = cccc;
	  ((op_t *) dstp)[7] = cccc;
	  dstp += 8 * OPSIZ;
	  xlen -= 1;
	}
      len %= OPSIZ * 8;

      /* Write 1 `op_t' per iteration until less than OPSIZ bytes remain.  */
      xlen = len / OPSIZ;
      while (xlen > 0)
	{
	  ((op_t *) dstp)[0] = cccc;
	  dstp += OPSIZ;
	  xlen -= 1;
	}
      len %= OPSIZ;
    }

  /* Write the last few bytes.  */
  while (len > 0)
    {
      ((byte *) dstp)[0] = c;
      dstp += 1;
      len -= 1;
    }

  return dstpp;
}

#define zeromem(x) memset(x,0,sizeof(*x))

inline void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = static_cast<uint8_t *>(dest);
    const uint8_t *psrc = static_cast<const uint8_t *>(src);

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

inline int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = static_cast<const uint8_t *>(s1);
    const uint8_t *p2 = static_cast<const uint8_t *>(s2);

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

inline char* strcat(char* dest, const char* src) {

    char* ptr = dest;
    while(*ptr != '\0') {
        ptr++;
    }
    
    while(*src != '\0') {
        *ptr = *src;
        ptr++;
        src++;
    }
    
    *ptr = '\0';
    
    return dest;
}

inline int strlen (const char *str)
{
  const char *char_ptr;
  const unsigned long int *longword_ptr;
  unsigned long int longword, himagic, lomagic;

  /* Handle the first few characters by reading one character at a time.
     Do this until CHAR_PTR is aligned on a longword boundary.  */
  for (char_ptr = str; ((unsigned long int) char_ptr
			& (sizeof (longword) - 1)) != 0;
       ++char_ptr)
    if (*char_ptr == '\0')
      return char_ptr - str;

  /* All these elucidatory comments refer to 4-byte longwords,
     but the theory applies equally well to 8-byte longwords.  */

  longword_ptr = (unsigned long int *) char_ptr;

  /* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
     the "holes."  Note that there is a hole just to the left of
     each byte, with an extra at the end:

     bits:  01111110 11111110 11111110 11111111
     bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD

     The 1-bits make sure that carries propagate to the next 0-bit.
     The 0-bits provide holes for carries to fall into.  */
  himagic = 0x80808080L;
  lomagic = 0x01010101L;
  if (sizeof (longword) > 4)
    {
      /* 64-bit version of the magic.  */
      /* Do the shift in two steps to avoid a warning if long has 32 bits.  */
      himagic = ((himagic << 16) << 16) | himagic;
      lomagic = ((lomagic << 16) << 16) | lomagic;
    }

  /* Instead of the traditional loop which tests each character,
     we will test a longword at a time.  The tricky part is testing
     if *any of the four* bytes in the longword in question are zero.  */
  for (;;)
    {
      longword = *longword_ptr++;

      if (((longword - lomagic) & ~longword & himagic) != 0)
	{
	  /* Which of the bytes was the zero?  If none of them were, it was
	     a misfire; continue the search.  */

	  const char *cp = (const char *) (longword_ptr - 1);

	  if (cp[0] == 0)
	    return cp - str;
	  if (cp[1] == 0)
	    return cp - str + 1;
	  if (cp[2] == 0)
	    return cp - str + 2;
	  if (cp[3] == 0)
	    return cp - str + 3;
	  if (sizeof (longword) > 4)
	    {
	      if (cp[4] == 0)
		return cp - str + 4;
	      if (cp[5] == 0)
		return cp - str + 5;
	      if (cp[6] == 0)
		return cp - str + 6;
	      if (cp[7] == 0)
		return cp - str + 7;
	    }
	}
    }
}
inline void* malloc(size_t size) {
    return memory::heap::malloc(size);
}

inline void free(void* p) {
    memory::heap::free(p);
}

#define strcpy(dest,src) memcpy(dest,src,strlen(src) + 1)

inline int strcmp(const char *s1, const char *s2) {
    const unsigned long *w1 = (const unsigned long *)s1;
    const unsigned long *w2 = (const unsigned long *)s2;
    const unsigned long himagic = 0x8080808080808080UL;
    const unsigned long lomagic = 0x0101010101010101UL;
    
    while (1) {
        unsigned long word1 = *w1++;
        unsigned long word2 = *w2++;
        
        unsigned long diff = word1 ^ word2;
        unsigned long zeros = (word1 - lomagic) & ~word1 & himagic;
        
        if (diff != 0 || zeros != 0) {
            const char *c1 = (const char *)(w1 - 1);
            const char *c2 = (const char *)(w2 - 1);
            
            do {
                unsigned char ch1 = *c1++;
                unsigned char ch2 = *c2++;
                if (ch1 != ch2) return ch1 - ch2;
                if (ch1 == 0) break;
            } while (1);
            
            return 0;
        }
    }
}
inline int strncmp(const char *s1, const char *s2, size_t n) {
    size_t i = 0;
    while (i < n && s1[i] && (s1[i] == s2[i])) {
        i++;
    }
    if (i == n) return 0;
    return (unsigned char)s1[i] - (unsigned char)s2[i];
}

inline char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return NULL;
}

char *strtok(char *str, const char *delim);

inline char* strdup(const char *s) {
    size_t len = 0;
    while (s[len]) len++;

    char *copy = (char *)malloc(len + 1);
    if (!copy) return NULL;

    for (size_t i = 0; i <= len; i++) {
        copy[i] = s[i];
    }
    return copy;
}

inline char* strrchr(const char* str, int ch) {
    char* last_occurrence = 0;

    while (*str) {
        if (*str == ch) {
            last_occurrence = (char*)str; 
        }
        str++;
    }

    return last_occurrence;
}