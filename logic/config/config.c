#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int load_config(const char *path, DatabaseConfig *cfg) {
    FILE *current_file = fopen(path, "r");

    if(!current_file) {
        perror("[CONFIG] - File");
        return -1;
    }

    ///Default Values
    strcpy(cfg -> host, "127.0.0.1");
    cfg -> port = 5432;
    cfg -> user[0] = '\0';
    cfg -> dbname[0] = '\0';

    char line[512];
    while(fgets(line, sizeof(line), current_file)) {
        if(line[0] == '\n' || line[0] == '#') /// Commenting inside the config is done by #
            continue;
        
        char key[64], value[256];    
    
        if(sscanf(line, "%63[^= ] = %255[^\n]", key, value) == 2) {
            if(strcmp(key, "host") == 0)
                strncpy(cfg -> host, value, sizeof(cfg -> host) - 1);
            
            else if(strcmp(key, "port") == 0) {
                int p = atoi(value);
                if(p < 0 || p > 65535) {
                    fprintf(stderr, "[CONFIG]: port %d is out of range\n", p);                
                    fclose(current_file);
                    return -1;
                }

                cfg -> port = (unsigned short) p;
            }

            else if(strcmp(key, "user") == 0)
                strncpy(cfg -> user, value, sizeof(cfg -> user) - 1);

            else if(strcmp(key, "database") == 0)
                strncpy(cfg -> dbname, value, sizeof(cfg -> dbname) - 1);
        }
    }

    fclose(current_file);

    if(cfg -> user[0] == '\0' || cfg -> dbname[0] == '\0') {
        fprintf(stderr, "[CONFIG]: User and Database name are required.\n");
        return -1;
    }

    return 0;
}   