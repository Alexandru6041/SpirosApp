#ifndef SCRAM_HEADER
#define SCRAM_HEADER

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char client_nonce[32];
    char client_first_base[64];
    char server_first_message[512];
    char combined_nonce[128];

    uint8_t salt[64];
    size_t salt_length;
    uint32_t iterations;
}SCRAM_State;

size_t scram_client_first(SCRAM_State *st, const char *username, char *out, size_t out_size);

int scram_parse_server_first(SCRAM_State *st, const char *server_first, size_t len);
int scram_client_final(SCRAM_State *st, const char *password, char *out, size_t out_size);
int scram_verify_server_final(SCRAM_State *st, const char *password, const char *server_final, size_t len);
int SCRAM_Authentication(int sock, const char *username, const char *password);


#endif