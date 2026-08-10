#ifndef BASE64_HEADER
#define BASE64_HEADER

#include <stdint.h>
#include <stddef.h>

size_t base64_encode(const uint8_t *data, size_t length, char *out);

int base64_decode(const char *in, size_t in_len, uint8_t *out);

#endif