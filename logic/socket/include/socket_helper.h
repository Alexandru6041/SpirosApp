#ifndef SOCKET_HELPER_H
#define SOCKET_HELPER_H
#include <stdlib.h>

int read_exact(int sock, void *buf, size_t n);
int put_str(unsigned char *buf, int *pos, size_t bufsize, const char *s);

int read_big_endian32(const unsigned char *p);
int write_big_endian32(unsigned char *buf, int *pos, size_t bufsize, unsigned int value);
unsigned short read_big_endian16(const unsigned char *p);

#endif