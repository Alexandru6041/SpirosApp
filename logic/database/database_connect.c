#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "database_header.h"
#include "scram.h"
#include "socket_helper.h"


#define POSTGRES_PROTOCOL_VERSION 0x00030000 /// Postgres protocol version 3.0 


static int send_startup_message(int sock, char *user, char *database_name, char *host, unsigned int port) {
        ///Building the payload for the socket
    unsigned char buf[1024];
    int pos = 4; ///Reserving bytes 0-3 for the total length of the message(required by Postgres format)

    if(write_big_endian32(buf, &pos, sizeof(buf), POSTGRES_PROTOCOL_VERSION) != 0) {
        fprintf(stderr, "[OVERFLOW] The total length of the Postgres initial startup message overflowed the 1024 byte buffer.\n");
        close(sock);
        return 1;
    } 

    if(put_str(buf, &pos, sizeof(buf), "user") != 0 ||
    put_str(buf, &pos, sizeof(buf), user) != 0 ||
    put_str(buf, &pos, sizeof(buf), "database") != 0 ||
    put_str(buf, &pos, sizeof(buf), database_name) != 0
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
        fprintf(stderr, "[CONNECTION]: Transmitting data process was corrupted. Not all bytes have been sent to %s via %u\n", host, port);
        close(sock);
        return 1;
    }
    
    printf("[SUCCESS] Startup message sent.\n");
    return 0;
}

DatabaseConnection *db_connect(const char *config_path, const char *password) {
    DatabaseConfig cfg;
    if(load_config(config_path, &cfg) != 0) {
        fprintf(stderr, "[CONFIG]: Failed to load config!\n");
        return NULL;

    }

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sock < 0) {
        fprintf(stderr, "[CONNECTION]: socket() failed!\n");
        return NULL;
    }


    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(cfg.port);/// big-endian to little-endian
    if(inet_pton(AF_INET, cfg.host, &server.sin_addr) <= 0) {
        fprintf(stderr, "[CONNECTION]: Invalid Host address! Change the host inside .cfg file.\n");
        close(sock);
        return NULL;
    }

    if(connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        fprintf(stderr, "[CONNECTION]: Database connection failed on attempt.\n");
        close(sock);
        return NULL;
    }

    if(send_startup_message(sock, cfg.user, cfg.dbname, cfg.host, cfg.port) != 0) {
        fprintf(stderr, "[DATABASE CONNECTION]: Startup message failed.\n");
        close(sock);
        return NULL;
    }
    

    if(SCRAM_Authentication(sock, cfg.user, password) != 0) {
        fprintf(stderr, "[AUTH]: SCRAM authentication failed\n");
        close(sock);
        return NULL;
    }
    printf("[SUCCESS] Authenticated to PostgreSQL!\n");


    if(wait_for_read(sock) != 0) {
        fprintf(stderr, "[ERROR] Could not read query. Have this checked by the administrator");
        close(sock);
        return NULL;
    }

    DatabaseConnection *conn = calloc(1, sizeof(DatabaseConnection));
    if(!conn) {
        close(sock);
        return NULL;
    }

    conn -> sock = sock;
    conn -> connected = 1;
    conn -> status = 'I'; /// idle
    return conn;
}

void db_disconnect(DatabaseConnection *conn) {
    if(!conn)
        return ;
    if(conn -> connected) {
        uint8_t term[5] = {'X', 0, 0, 0, 4}; ///Sending 'X' terminate signal to POSTGRES.
        send(conn -> sock, term, 5, 0);
        close(conn -> sock); 
    }
    free(conn);
}