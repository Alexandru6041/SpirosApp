#include "base64.h"

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t base64_encode(const uint8_t *data, size_t length, char *out) {
    size_t i = 0, j = 0;

    ///processing in groups of 3 bytes
    while(i + 3 <= length) {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | (data[i + 2]);

        out[j++] = B64[(n >> 18) & 0x3F];
        out[j++] = B64[(n >> 12) & 0x3F];
        out[j++] = B64[(n >> 6) & 0x3F];
        out[j++] = B64[n & 0x3F];
        
        i += 3;
    }   

    size_t remaining = length - i;
    if(remaining == 1) {
        uint32_t n = data[i] << 16;
        out[j++] = B64[(n >> 18) & 0x3F];    
        out[j++] = B64[(n >> 12) & 0x3F];    
        out[j++] = '=';    
        out[j++] = '=';     
    }
    else if(remaining == 2){
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
        out[j++] = B64[(n >> 18) & 0x3F];    
        out[j++] = B64[(n >> 12) & 0x3F];    
        out[j++] = B64[(n >> 6) & 0x3F];    
        out[j++] = '='; 
    }

    out[j] = '\0';

    return j;
}

static int b64_val(char c) {
    if(c >= 'A' && c <= 'Z') 
        return c - 'A';
    if(c >= 'a' && c <= 'z') 
        return c - 'a' + 26;
    if(c >= '0' && c <= '9') 
        return c - '0' + 52;
    
    if(c == '+') 
        return 62;
    if(c == '/') 
        return 63;
    return -1;                 
}

int base64_decode(const char *in, size_t in_len, uint8_t *out) {
    if(in_len % 4 != 0) {
        return -1;
    }

    size_t o = 0;
    for(size_t i = 0; i < in_len; i += 4) {
        int v0 = b64_val(in[i]);
        int v1 = b64_val(in[i + 1]);

        int pad = 0, v2, v3;

        if(in[i + 2] == '=') {
            v2 = 0;
            pad++;
        }
        else {
            v2 = b64_val(in[i + 2]);
        }

        if(in[i + 3] == '=') {
            v3 = 0;
            pad++;
        }
        else {
            v3 = b64_val(in[i + 3]);
        }

        if(v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0)
            return -1;
        
        uint32_t n = (v0 << 18) | (v1 << 12) | (v2 << 6) | v3;
        
        out[o++] = (n >> 16) & 0xFF;
        if(pad < 2)
            out[o++] = (n >> 8) & 0xFF;
        if(pad < 1)
            out[o++] = n & 0xFF;
    }

    return (int) o;
}