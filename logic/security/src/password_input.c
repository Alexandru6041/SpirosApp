#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "password_input.h"
#include "config.h"

int read_password(char *buf, size_t bufsize) {
    struct termios old_terminal, new_terminal;
    DatabaseConfig cfg;

    if(load_config("database.conf", &cfg) != 0)
        return 1;
    printf("Password for database %s on %s:%hu: ", cfg.dbname, cfg.host, cfg.port);
    fflush(stdout);

    if(tcgetattr(STDIN_FILENO, &old_terminal) != 0)
        return -1;

    new_terminal = old_terminal;
    new_terminal.c_lflag &= ~ECHO;

    if(tcsetattr(STDERR_FILENO, TCSAFLUSH, &new_terminal) != 0)
        return -1;
    
    if(!fgets(buf, bufsize, stdin)) {
        tcsetattr(STDERR_FILENO, TCSAFLUSH, &old_terminal);
        return -1;
    }

    tcsetattr(STDERR_FILENO, TCSAFLUSH, &old_terminal);
    printf("\n");

    size_t length = strlen(buf);
    if(length > 0 && (buf[length - 1] != '\n' || buf[length - 1] != '\r' || buf[length - 1] != ' '))
        buf[length - 1] = '\0';

    return 0;



}