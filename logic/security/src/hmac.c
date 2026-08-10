#include <string.h>
#include <stdlib.h>

#include "hmac.h"
#include "sha256.h"

#define BLOCK_SIZE 64 ///bytes

void hmac_sha256(const uint8_t *key, size_t key_length, const uint8_t *message, size_t message_length, uint8_t out[32]) {
    uint8_t k[BLOCK_SIZE];
    memset(k, 0, BLOCK_SIZE);

    if(key_length > BLOCK_SIZE)
        sha256(key, key_length, k); /// hashing down to 32 bytes in case the key is too long
    else
        memcpy(k, key, key_length);

    uint8_t inner_key[BLOCK_SIZE];
    uint8_t outer_key[BLOCK_SIZE];

    for(int i = 0; i < BLOCK_SIZE; i++) {
        inner_key[i] = k[i] ^ 0x36; ///inner padding
        outer_key[i] = k[i] ^ 0x5c; /// outer padding
    }
    size_t inner_input_length = BLOCK_SIZE + message_length;
    uint8_t *inner_input = malloc(inner_input_length);
    if(!inner_input)
        return ;

    /// inner hash
    memcpy(inner_input, inner_key, BLOCK_SIZE);
    memcpy(inner_input + BLOCK_SIZE, message, message_length);

    uint8_t inner_hash[32];

    sha256(inner_input, inner_input_length, inner_hash);
    free(inner_input);

    /// outer hash
    uint8_t outer_input[BLOCK_SIZE + 32];

    memcpy(outer_input, outer_key, BLOCK_SIZE);
    memcpy(outer_input + BLOCK_SIZE, inner_hash, 32);

    sha256(outer_input, BLOCK_SIZE + 32, out);

    
}