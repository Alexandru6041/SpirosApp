#include <string.h>
#include <stdlib.h>

#include "hmac.h"
#include "pbkdf2.h"

void pbkdf2_sha256(const uint8_t *password, size_t password_length, const uint8_t *salt, size_t salt_length, uint32_t iterations, uint8_t out[32]) {
    size_t salt_block_length = salt_length + 4;
    uint8_t *salt_block = malloc(salt_block_length);
    if(!salt_block_length) {
        return ;
    }


    memcpy(salt_block, salt, salt_length);
    salt_block[salt_length] = 0x00; /// the salt;
    salt_block[salt_length + 1] = 0x00;
    salt_block[salt_length + 2] = 0x00;
    salt_block[salt_length + 3] = 0x01; /// converting to big-endian

    uint8_t u[32];
    uint8_t result[32];
    
    hmac_sha256(password, password_length, salt_block, salt_block_length, u);
    free(salt_block);

    memcpy(result, u, 32);

    for(uint32_t i = 1; i < iterations; i++) {
        hmac_sha256(password, password_length, u, 32, u);

        for(int  j = 0; j < 32; j++) {
            result[j] ^= u[j];
        }
    }

    memcpy(out, result, 32);
}