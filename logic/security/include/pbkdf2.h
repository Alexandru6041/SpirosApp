#ifndef PBKDF_HEADER
#define PBKDF_HEADER

#include<stdint.h>
#include <stddef.h>

void pbkdf2_sha256(const uint8_t *password, size_t password_length, const uint8_t *salt, size_t salt_length, uint32_t iterations, uint8_t out[32]);

#endif