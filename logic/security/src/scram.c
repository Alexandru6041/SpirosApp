#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "scram.h"
#include"base64.h"
#include "pbkdf2.h"
#include "hmac.h"
#include "sha256.h"
#include "socket_helper.h"

static void generate_nonce(char *out, size_t out_size) {
    uint8_t random_bytes[18];

    FILE *random = fopen("/dev/urandom", "rb"); /// using urandom instead of rand() for lower predictability
    if(random) {
        fread(random_bytes, 1, sizeof(random_bytes), random);
        fclose(random);
    }

    (void) out_size;
    base64_encode(random_bytes, sizeof(random_bytes), out);
}

size_t scram_client_first(SCRAM_State *st, const char *username, char *out, size_t out_size) {
    generate_nonce(st -> client_nonce, sizeof(st -> client_nonce));

    snprintf(st -> client_first_base, sizeof(st -> client_first_base), "n=%s,r=%s", username, st -> client_nonce);

    int n = snprintf(out, out_size, "n,,%s", st -> client_first_base);

    return (size_t) n;
}

int scram_parse_server_first(SCRAM_State *st, const char *server_first, size_t len) {
    if(len >= sizeof(st -> server_first_message))
        return -1;
    memcpy(st -> server_first_message, server_first, len);

    st -> server_first_message[len] = '\0';
    const char *p = st -> server_first_message;

    if(strncmp(p, "r=", 2) != 0)
        return -1;
    p += 2;

    const char *comma = strchr(p, ',');
    if(!comma)
        return -1;
    
    size_t nonce_length = comma - p;
    if(nonce_length >= sizeof(st -> combined_nonce))
        return -1;
    memcpy(st -> combined_nonce, p, nonce_length);
    st -> combined_nonce[nonce_length] = '\0';

    if(strncmp(st -> combined_nonce, st -> client_nonce, strlen(st -> client_nonce)) != 0)
        return -1; ///The combined nonce must start with the client nonce; else reject
    
    ///Base 64 salt
    p = comma + 1;
    if(strncmp(p, "s=", 2) != 0)
        return -1;
    p += 2;

    comma = strchr(p, ',');
    if(!comma)
        return -1;
    size_t salt_base64_length = comma - p; 
    char salt_b64[128];
    
    if(salt_base64_length >= sizeof(salt_b64))
        return -1;
    
    memcpy(salt_b64, p, salt_base64_length);
    salt_b64[salt_base64_length] = '\0';

    int decoded = base64_decode(salt_b64, salt_base64_length, st -> salt);
    if(decoded < 0) 
        return -1;
    st -> salt_length = (size_t) decoded;

    ///Iterations
    p = comma + 1;
    if(strncmp(p, "i=", 2) != 0) 
        return -1;
    p += 2;
    st -> iterations = (uint32_t) atoi(p);
    if(st -> iterations == 0)
        return -1;

    return 0;
}


int scram_client_final(SCRAM_State *st, const char *password, char *out, size_t out_size) {
    uint8_t salted_password[32];
    pbkdf2_sha256((const uint8_t *) password, strlen(password), st -> salt, st -> salt_length, st -> iterations, salted_password);

    ///Client key
    uint8_t client_key[32];
    hmac_sha256(salted_password, 32, (const uint8_t *) "Client Key", 10, client_key);

    ///Stored key
    uint8_t stored_key[32];
    sha256(client_key, 32, stored_key);

    ///client final without proof
    char client_final_no_proof[256];
    snprintf(client_final_no_proof, sizeof(client_final_no_proof), "c=biws,r=%s", st -> combined_nonce);

    ///Auth
    char auth_message[1024];
    int auth_length = snprintf(auth_message, sizeof(auth_message), "%s,%s,%s", st -> client_first_base, st -> server_first_message, client_final_no_proof);
    if(auth_length < 0 || auth_length >= (int) sizeof(auth_message))
        return -1;
    
    ///Client Signature HMAC
    uint8_t client_signature[32];
    hmac_sha256(stored_key, 32, (const uint8_t *) auth_message, (size_t) auth_length, client_signature);

    ///Client Proof(xor Signature)
    uint8_t client_proof[32];
    char proof_base64[64];
    for(int i = 0; i < 32; i++)
        client_proof[i] = client_key[i] ^ client_signature[i];
    //encode proof
    base64_encode(client_proof, 32, proof_base64);

    ///FINAL
    int n = snprintf(out, out_size, "%s,p=%s", client_final_no_proof, proof_base64);
    if(n < 0 || n >= (int)out_size)
        return -1;
    fprintf(stderr, "[DBG] AuthMessage='%s'\n", auth_message);
    return n;
}

int scram_verify_server_final(SCRAM_State *st, const char *password, const char *server_final, size_t len) {
    if(len < 2 || strncmp(server_final, "v=", 2) != 0) 
        return -1;

    // Extract server signature.
    const char *sig_b64 = server_final + 2;
    size_t sig_b64_len = len - 2;
    uint8_t server_sig_received[32];
    int decoded = base64_decode(sig_b64, sig_b64_len, server_sig_received);
    if (decoded != 32) 
        return -1;

    // salt password.
    uint8_t salted_password[32];
    pbkdf2_sha256((const uint8_t *)password, strlen(password), st->salt, st->salt_length, st->iterations, salted_password);

    uint8_t server_key[32];
    hmac_sha256(salted_password, 32, (const uint8_t *)"Server Key", 10, server_key);

    // rebuild auth
    char client_final_no_proof[256];
    snprintf(client_final_no_proof, sizeof(client_final_no_proof), "c=biws,r=%s", st->combined_nonce);

    char auth_message[1024];
    int auth_len = snprintf(auth_message, sizeof(auth_message), "%s,%s,%s", st->client_first_base, st->server_first_message, client_final_no_proof);
    
    if(auth_len < 0 || auth_len >= (int)sizeof(auth_message)) 
        return -1;

    ///signature
    uint8_t server_sig_computed[32];
    hmac_sha256(server_key, 32, (const uint8_t *)auth_message, (size_t)auth_len, server_sig_computed);

    if(memcmp(server_sig_received, server_sig_computed, 32) != 0)
        return -1;

    return 0;
}

///FINALLY!!!
int SCRAM_Authentication(int sock, const char *username, const char *password) {
    SCRAM_State st;
    char buffer[2048];

    ///Reading R
    uint8_t type;
    if(read_exact(sock, &type, 1) < 0)
        return -1;
    uint8_t lenbuf[4];
    read_exact(sock, lenbuf, 4);
    int message_length = read_big_endian32(lenbuf);
    int payload_length = message_length - 4;

    uint8_t payload[2048];
    if(payload_length > 0 && payload_length <= (int) sizeof(payload))
        read_exact(sock, payload, payload_length);
    
    if(type != 'R') {fprintf(stderr, "[DBG] not R (type=%c)\n", type);
        return -1;
    
    }
     
    int auth_code = read_big_endian32(payload);
    if(auth_code != 10){ 
        fprintf(stderr, "[DBG] not SASL (code=%d)\n", auth_code);
        return -1; 
    }
    
    char client_first[512];
    size_t cf_len = scram_client_first(&st, username, client_first, sizeof(client_first));

    {
        const char *mechanism = "SCRAM-SHA-256";
        int pos = 0;
        uint8_t out[1024];

        out[pos++] = 'p';
        pos += 4;

        size_t mlen = strlen(mechanism) + 1;    
        memcpy(out + pos, mechanism, mlen);
        pos += mlen;

        write_big_endian32(out, &pos, sizeof(out), (unsigned int) cf_len);
        memcpy(out + pos, client_first, cf_len);
        pos += cf_len;

        int saved = pos;
        int lpos = 1;
        write_big_endian32(out, &lpos, sizeof(out), (unsigned int) (saved - 1));
        send(sock, out, saved, 0);
    }

    ///Server first 'R'
        
    read_exact(sock, &type, 1);
    read_exact(sock, lenbuf, 4);
    message_length = read_big_endian32(lenbuf);
    payload_length = message_length - 4;
   
    read_exact(sock, payload, payload_length);
    fprintf(stderr, "[DBG] auth_code=%d msg_len=%d payload_len=%d\n", auth_code, message_length, payload_length);

   
    if(type != 'R') {fprintf(stderr, "[DBG] not R (type=%c)\n", type);
        return -1;
    
    }
    
    auth_code = read_big_endian32(payload);
    if(auth_code != 11) 
        return -1;

    const char *server_first = (const char *)(payload + 4);
    int sf_len = payload_length - 4;
    
    if(scram_parse_server_first(&st, server_first, sf_len) != 0) 
        return -1;

    ///Client final 'p'
    char client_final[512];
    int cfin_len = scram_client_final(&st, password, client_final, sizeof(client_final));

    if(cfin_len < 0) 
        return -1;
    {
        int pos = 0;
        uint8_t out[1024];
        out[pos++] = 'p';
        pos += 4;
        memcpy(out + pos, client_final, cfin_len); pos += cfin_len;
        int saved = pos;
        int lpos = 1;
        write_big_endian32(out, &lpos, sizeof(out), (unsigned int)(saved - 1));
        send(sock, out, saved, 0);
    }

    read_exact(sock, &type, 1);
    read_exact(sock, lenbuf, 4);
    
    message_length = read_big_endian32(lenbuf);
    payload_length = message_length - 4;
    read_exact(sock, payload, payload_length);

    fprintf(stderr, "[DBG] auth_code=%d msg_len=%d payload_len=%d\n", auth_code, message_length, payload_length);


    if (type == 'E') {
        fprintf(stderr, "[DBG] SERVER ERROR: ");
        
        for(int i = 1; i < payload_length; i++)
            fputc(payload[i] == '\0' ? ' ' : payload[i], stderr);
        
        fprintf(stderr, "\n");
        return -1;
    }
    if (type != 'R') { fprintf(stderr, "[DBG] not R (type=%c)\n", type); return -1; }

    auth_code = read_big_endian32(payload);
    if (auth_code != 12) 
        return -1;

    const char *server_final = (const char *)(payload + 4);
    int svf_len = payload_length - 4;

    if(scram_verify_server_final(&st, password, server_final, svf_len) != 0) {
        fprintf(stderr, "[SCRAM]: server verification failed!\n");
        return -1;
    }

    read_exact(sock, &type, 1);
    read_exact(sock, lenbuf, 4);
    message_length = read_big_endian32(lenbuf);
    payload_length = message_length - 4;
    read_exact(sock, payload, payload_length);

    fprintf(stderr, "[DBG] auth_code=%d msg_len=%d payload_len=%d\n", auth_code, message_length, payload_length);

    if(type != 'R') 
        { fprintf(stderr, "[DBG] not R (type=%c)\n", type); return -1; }
    auth_code = read_big_endian32(payload);
    if(auth_code != 0)
        return -1;
    
    if (scram_verify_server_final(&st, password, server_final, svf_len) != 0) {
        fprintf(stderr, "[DBG] verify_server_final FAILED\n");
        return -1;
    }
    read_exact(sock, payload, payload_length);
    if (type == 'E') {
        fprintf(stderr, "[DBG] server error: %.*s\n", payload_length, payload + 1);
        return -1;
    }
    return 0;
}