
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

uint64_t String::strncmp(const char* str1, const char* str2, uint64_t n) {
  uint64_t i = 0;

  while (i < n && str1[i] != '\0' && str2[i] != '\0') {
    if (str1[i] != str2[i]) {
      return (unsigned char)str1[i] - (unsigned char)str2[i];
    }
    i++;
  }

  if (i == n)
    return 0;

  return (unsigned char)str1[i] - (unsigned char)str2[i];
}


char * String::strchr(const char *str, int ch) {
  while (*str) {
      if (*str == (char)ch) {
          return (char *)str;
      }
      str++;
  }

  if (ch == '\0') {
      return (char *)str;
  }

  return NULL;
}

char* String::strtok(char *str, const char *delim, char** saveptr) {
  char *start;
  char *delim_pos;

  if (str == NULL) {
      str = *saveptr;
  }

  while (*str && strchr(delim, *str)) {
      str++;
  }

  if (*str == '\0') {
      *saveptr = str;
      return NULL;
  }

  start = str;

  while (*str && (delim_pos = strchr(delim, *str)) == NULL) {
      str++;
  }

  if (*str) {
      *str = '\0';
      *saveptr = str + 1;
  } else {
      *saveptr = str;
  }

  return start;
}

int String::strcmp(const char *str1, const char *str2) {
  while (*str1 && (*str1 == *str2)) {
      str1++;
      str2++;
  }
  return *(unsigned char *)str1 - *(unsigned char *)str2;
}


char* String::strncpy(char* dest, const char* src, uint64_t n) {
  uint64_t i;
  for (i = 0; i < n && src[i] != '\0'; i++) {
      dest[i] = src[i];
  }
  for (; i < n; i++) {
      dest[i] = '\0';
  }
  return dest;
}

char* String::strncat(char* dest, const char* src, uint64_t n) {
  uint64_t dest_len = 0;
  while (dest[dest_len] != '\0') {
      dest_len++;
  }
  uint64_t i;
  for (i = 0; i < n && src[i] != '\0'; i++) {
      dest[dest_len + i] = src[i];
  }
  dest[dest_len + i] = '\0';

  return dest;
}

// Yes, i'm lazy 