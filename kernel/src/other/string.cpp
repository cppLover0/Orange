
#pragma once

#include <stdint.h>
#include <other/string.hpp>
#include <cstdlib>

uint64_t String::strlen(char* str) {
    uint64_t len = 0; 
    while(str[len])
        len++;
    return len;
}

char* String::itoa(uint64_t value, char* str, int base ) {
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

// From meaty skeleton implementation
void* String::memset(void* bufptr, int value, uint64_t size) {
	unsigned char* buf = (unsigned char*) bufptr;
	for (uint64_t i = 0; i < size; i++)
		buf[i] = (unsigned char) value;
	return bufptr;
}

// From meaty skeleton implementation
void* String::memcpy(void* dstptr, const void* srcptr, uint64_t size) {
	unsigned char* dst = (unsigned char*) dstptr;
	const unsigned char* src = (const unsigned char*) srcptr;
	for (uint64_t i = 0; i < size; i++)
		dst[i] = src[i];
	return dstptr;
}

// From GNU Libc implementation
void* __rawmemchr(const char* s, int c_in)
{
  const unsigned char *char_ptr;
  const unsigned long int *longword_ptr;
  unsigned long int longword, magic_bits, charmask;
  unsigned char c;

  c = (unsigned char) c_in;
  for (char_ptr = (const unsigned char *) s;
       ((unsigned long int) char_ptr & (sizeof (longword) - 1)) != 0;
       ++char_ptr)
    if (*char_ptr == c)
      return (void*) char_ptr;

  longword_ptr = (unsigned long int *) char_ptr;

  if (sizeof (longword) != 4 && sizeof (longword) != 8)
    std::abort();

  magic_bits = 0x7efefeff;

  charmask = c | (c << 8);
  charmask |= charmask << 16;

  while (1)
    {

      longword = *longword_ptr++ ^ charmask;

      if ((((longword + magic_bits)

	    ^ ~longword)

	   & ~magic_bits) != 0)
	{

	  const unsigned char *cp = (const unsigned char *) (longword_ptr - 1);

	  if (cp[0] == c)
	    return (void*) cp;
	  if (cp[1] == c)
	    return (void*) &cp[1];
	  if (cp[2] == c)
	    return (void*) &cp[2];
	  if (cp[3] == c)
	    return (void*) &cp[3];
	}
    }
}

// From GNU Libc implementation
uint64_t strspn(const char *s, const char *accept)
{
  const char *p;
  const char *a;
  uint64_t count = 0;

  for (p = s; *p != '\0'; ++p)
    {
      for (a = accept; *a != '\0'; ++a)
	if (*p == *a)
	  break;
      if (*a == '\0')
	return count;
      else
	++count;
    }

  return count;
}

// From GNU Libc implementation
char* strpbrk(const char *s, const char *accept)
{
  while (*s != '\0')
    {
      const char *a = accept;
      while (*a != '\0')
	if (*a++ == *s)
	  return (char *) s;
      ++s;
    }

  return 0;
}

// From GNU Libc implementation
char* String::strtok(char *s, const char *delim) {
  char *token;
  static char* olds;

  if (s == 0)
    s = olds;

  s += strspn (s, delim);
  if (*s == '\0')
    {
      olds = s;
      return 0;
    }

  token = s;
  s = strpbrk (token, delim);
  if (s == 0)
    olds = (char*)__rawmemchr(token, '\0');
  else
    {
      *s = '\0';
      olds = s + 1;
    }
  return token;
}

// Yes, i'm lazy 