#include "socket_helper.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define POSTGRES_PROTOCOL_VERSION 0x00030000 /// Postgres protocol version 3.0 


int main(void) {
    
    ///Loading configuration
    DatabaseConfig cfg;
    if(load_config("database.conf", &cfg) != 0)
        return 1;

    printf("[SUCCESS]: Config loaded:\n host = %s \n port = %u \n user = %s \n database = %s \n", 
            cfg.host, cfg.port, cfg.user, cfg.dbname);
    

    ///Socket connection
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sock < 0) {
        perror("[SOCKET]");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr)); 
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg.port);///big-endian to little-endian
    if(inet_pton(AF_INET, cfg.host, &addr.sin_addr) != 1) {
        fprintf(stderr, "[HOST]: Invalid address %s\n", cfg.host);
        return 1;
    }

    if(connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("[CONNECTION]");
        return 1;
    }

    printf("[SUCCESS] Connected to %s:%u\n", cfg.host, cfg.port);


    ///Building the payload for the socket
    unsigned char buf[1024];
    int pos = 4; ///Reserving bytes 0-3 for the total length of the message(required by Postgres format)

    if(write_big_endian32(buf, &pos, sizeof(buf), POSTGRES_PROTOCOL_VERSION) != 0) {
        fprintf(stderr, "[OVERFLOW] The total length of the Postgres initial startup message overflowed the 1024 byte buffer.\n");
        close(sock);
        return 1;
    } 

    if(put_str(buf, &pos, sizeof(buf), "user") != 0 ||
       put_str(buf, &pos, sizeof(buf), cfg.user) != 0 ||
       put_str(buf, &pos, sizeof(buf), "database") != 0 ||
       put_str(buf, &pos, sizeof(buf), cfg.dbname) != 0
    ) {
        fprintf(stderr, "[OVERFLOW]: The total length of the Postgres initial startup message overflowed the 1024 byte buffer.\n");
        close(sock);
        return 1;
    }

    if(pos + 1 > (int) sizeof(buf)) {
        fprintf(stderr, "[OVERFLOW]: The total length of the Postgres initial startup message overflowed the 1024 byte buffer.\n");
        close(sock);
        return 1;
    }
    buf[pos++] = 0;

    int saved = pos;
    pos = 0;

    if(write_big_endian32(buf, &pos, sizeof(buf), (unsigned int) saved) != 0) {
        fprintf(stderr, "[OVERFLOW] The total length of the Postgres initial startup message overflowed the 1024 byte buffer.\n");
        close(sock);
        return 1;
    }

    pos = saved;

    ///Sending the buffer
    ssize_t sent = send(sock, buf, (size_t) saved, 0);

    if(sent != saved) {
        fprintf(stderr, "[CONNECTION]: Transmitting data process was corrupted. Not all bytes have been sent to %s via %u\n", cfg.host, cfg.port);
        close(sock);
        return 1;
    }
    
    printf("[SUCCESS] Startup message sent.\n");
    close(sock);

}