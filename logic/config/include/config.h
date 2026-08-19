#ifndef CONFIG_H
#define CONFIG_H

#ifndef CONFNIG_PATH
#define CONFIG_PATH "" ///include config path here
#endif

typedef struct {
    char host[256];
    unsigned short port;
    char user[64];
    char dbname[64];
} DatabaseConfig;

int load_config(const char *path, DatabaseConfig *cfg);


#endif