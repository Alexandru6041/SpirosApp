#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "database_header.h"
#include "socket_helper.h"

static int result_AddRow(DatabaseResult *res, DatabaseValue *row) {
    if(res -> number_rows >= res -> row_capacity) {
        int new_capacity = (res -> row_capacity == 0) ? 16 : res -> row_capacity * 2;

        DatabaseValue **grown = realloc(res -> rows, (size_t) new_capacity * sizeof(DatabaseValue *));
        if(!grown) {
            return -1;
        }

        res -> rows = grown;
        res -> row_capacity = new_capacity;
    }

    res -> rows[res -> number_rows++] = row;

    return 0;
}

static int parse_row_description(const uint8_t *payload, DatabaseResult *res) {
    const uint8_t *p = payload;
    
    res -> number_columns = read_big_endian16(p);
    p += 2;

    res -> columns = calloc((size_t) res -> number_columns, sizeof(DatabaseColumn));
    if(!res -> columns)
        return -1;
    
    for(int i = 0; i < res -> number_columns; i++) {
        size_t name_length = strlen((const char *) p);

        res -> columns[i].name = malloc(name_length + 1);
        if(!res -> columns[i].name)
            return -1;
        
        memcpy(res -> columns[i].name, p, name_length + 1); ///including terminator
        p += name_length + 1;
        
        p += 4; //table oid
        p += 2; //column number

        res -> columns[i].type_oid = read_big_endian32(p);
        p += 4; 
        
        p += 2; ///type size;
        p += 4; /// type modifier
        p += 2; /// format code

    }

    return 0;
}

static int parse_data_row(const uint8_t *payload, DatabaseResult *res) {
    const uint8_t *p = payload;
    int cols = read_big_endian16(p);

    p += 2;
    DatabaseValue *row = calloc((size_t) cols, sizeof(DatabaseValue));
    if(!row)
        return -1;
    
    for(int i = 0; i < cols; i++) {
        int col_length = read_big_endian32(p);

        p += 4;
        if(col_length == -1) {
            row[i].data = NULL;
            row[i].length = -1;
        } else {
            row[i].data = malloc((size_t) col_length + 1);
            if(!row[i].data) {
                for(int j = 0; j < i; j++)
                    free(row[j].data);
                free(row);
                return -1;
            }

            memcpy(row[i].data, p, (size_t)col_length);
            row[i].data[col_length] = '\0';
            row[i].length = col_length;
            p += col_length;

        }
    }

    if(result_AddRow(res, row) != 0) {
        for(int i = 0; i < cols; i++) 
            free(row[i].data);
        free(row);
        return -1;
    }
    return 0;
}

static int send_query(int sock, const char *sql) {
    size_t sql_length = strlen(sql) + 1;
    size_t total = 1 + 4 + sql_length;

    uint8_t *out = malloc(total);
    if(!out)
        return -1;
    
    int pos = 0;
    out[pos++] = 'Q';
    pos += 4;
    memcpy(out + pos, sql, sql_length);
    pos += (int) sql_length;

    int lpos = 1;
    write_big_endian32(out, &lpos, (int) total, (unsigned int) (pos - 1));

    ssize_t sent = send(sock, out, (size_t) pos, 0);
    free(out);

    return (sent == pos) ? 0 : -1;
}

static char *parse_error_response(const uint8_t *payload, int payload_len) {
    const char *sqlstate = "";
    const char *message = "";

    const uint8_t *p = payload;
    const uint8_t *end = payload + payload_len;
    
    while(p < end && *p != 0) {
        char code = (char) *p;    
        p++;

        const char *value = (const char *) p;
        while(p < end && *p != 0)
            p++;
        if(p < end) ///skipping \0
            p++;
        
        if(code == 'C')
            sqlstate = value;
        else if(code == 'M')
            message = value;
    }

    size_t need = strlen(sqlstate) + 2 + strlen(message) + 1;
    char *out = malloc(need);
    if(!out)
        return NULL;
    
    snprintf(out, need, "%s: %s", sqlstate, message);
    return out;
}

int database_is_null(const DatabaseResult *res, int row, int col) {
    if(!res || row < 0 || row >= res -> number_rows || col < 0 || col >= res -> number_columns)
        return 1;

    return res -> rows[row][col].length == -1;
}

int wait_for_read(int sock) {
    while(1) {
        uint8_t type;
        if(read_exact(sock, &type, 1) < 0)
            return -1;
        
        uint8_t lenbuf[4];
        read_exact(sock, lenbuf, 4);

        int message_length = read_big_endian32(lenbuf);
        int payload_length = message_length - 4;

        uint8_t payload[4096];
        if(payload_length > 0 && payload_length <= (int) sizeof(payload))
            read_exact(sock, payload, payload_length);
        
        if(type == 'Z') {
            return 0;
        }

        if(type == 'E') {
            fprintf(stderr, "[QUERY]: Server Error\n");
            return -1;
        }
    }   
}

DatabaseResult *db_query(int sock, const char *sql) {
    (void) sock;
    (void) sql;

    return NULL;
}

DatabaseResult *database_query(int sock, const char *sql) {
    DatabaseResult *res = calloc(1, sizeof(DatabaseResult));
    if(!res)
        return NULL;
    
    if(send_query(sock, sql) != 0) {
        res -> error = strdup("[ERROR]: Failed to send query.\n");
        return res;
    }
    while(1) {
        uint8_t type;
        if(read_exact(sock, &type, 1) < 0) {
            res -> error = strdup("[ERROR]: Connection Lost");
            return res;
        }

        uint8_t lenbuf[4];
        read_exact(sock, lenbuf, 4);
        
        int message_length = read_big_endian32(lenbuf);
        int payload_length = message_length - 4;

        uint8_t *payload = NULL;
        if(payload_length > 0) {
            payload = malloc((size_t) payload_length);
            if(!payload) {
                res -> error = strdup("[ERROR]: Out of memory.\n");
                return res;
            }
            
            read_exact(sock, payload, (size_t) payload_length);
        }

        if(type == 'T') {
            parse_row_description(payload, res);
        } else if(type == 'D') {
            parse_data_row(payload, res);
        } else if(type == 'C') {
            ///pass, Querry Complete
        } else if(type == 'Z') {
            if(payload && payload_length >= 1)
                res -> status = (char) payload[0];
            free(payload);
            return res;
        }  else if(type == 'E') {
            if(payload)
                res -> error = parse_error_response(payload, payload_length);
        }

        free(payload);
    }
}


void database_result_free(DatabaseResult *res) {
    if(! res) {
        return ;
    }

    ///free value's data and each row's value array
    if(res -> rows) {
        for(int r = 0; r < res -> number_rows; r++) {
            if(res -> rows[r]) {
                for(int c = 0; c < res -> number_columns; c++) {
                    free(res -> rows[r][c].data);
                }
                free(res -> rows[r]);
            }
        }
        free(res -> rows);
    }
    
    ///freeing the column names and arrays
    if(res -> columns) {
        for(int c = 0; c < res -> number_columns; c++) {
            free(res -> columns[c].name);
        }

        free(res -> columns);
    }

    ///free error and struct
    free(res -> error);
    free(res);
}

const char *database_get_value(const DatabaseResult *res, int row, int col) {
    if(!res || row < 0 || row >= res -> number_rows || col < 0 || col >= res -> number_columns)
        return NULL;
    return res -> rows[row][col].data;
}


