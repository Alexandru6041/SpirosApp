#ifndef HMAC_HEADER
#define HMAC_HEADER

#include <stdint.h>
#include <stddef.h>

void hmac_sha256(const uint8_t *key, size_t key_length, const uint8_t *message, size_t message_length, uint8_t out[32]);

#endif