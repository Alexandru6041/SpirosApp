#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "socket_helper.h"
#include "scram.h"
#include "password_input.h"
#include "database_header.h"
#include "config.h"

#define POSTGRES_PROTOCOL_VERSION 0x00030000 /// Postgres protocol version 3.0 


int main(void) {
    DatabaseConfig cfg;
    if(load_config(CONFIG_PATH, &cfg) != 0) {
        fprintf(stderr, "[CONFIG]: Failed to load config.\n");
        return 1;
    }


    char password[256];
    memset(password, 0, sizeof(password));
    if(read_password(password, sizeof(password)) != 0) {
        fprintf(stderr, "Failed to read password.\n");
        return 1;
    }

    DatabaseConnection *conn = db_connect(CONFIG_PATH, password);
    memset(password, 0, sizeof(password));
    if(!conn) {
        fprintf(stderr, "[CONNECTION]: Connection to the database failed.\n");
        return 1;
    }

    const char *parameters[] = {
        "",
    };

    DatabaseResult *res = database_query_params(conn -> sock, "", parameters, 0);//  inserting querry here for testing
    if(res -> error) {
        fprintf(stderr, "[QUERY] %s\n", res -> error);
    } else {
        for(int index_columns = 0; index_columns < res -> number_columns; index_columns++) {
            printf("%s%s", res -> columns[index_columns].name, index_columns < res -> number_columns - 1 ? " | " : "\n");
        }

        for(int index_rows = 0; index_rows < res -> number_rows; index_rows++) {
            for(int index_columns = 0; index_columns < res -> number_columns; index_columns++) {
                const char *v = database_get_value(res, index_rows, index_columns);
                printf("%s%s", v ? v : "(null)", index_columns < res -> number_columns - 1 ? " | " : "\n");
            }
        }
    }
    database_result_free(res);

    close(conn -> sock);

}