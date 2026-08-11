#ifndef DATABASE_HEADER
#define DATABASE_HEADER

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char *data;
    int length;
} DatabaseValue;

typedef struct {
    char *name;
    int type_oid;
} DatabaseColumn;

typedef struct {
    int status;

    int number_columns;
    DatabaseColumn *columns;

    int number_rows;
    int row_capacity;
    DatabaseValue **rows;

    char *error;
} DatabaseResult;

DatabaseResult *db_query(int sock, const char *sql);
DatabaseResult *database_query(int sock, const char *sql);

void database_result_free(DatabaseResult *res);

const char *database_get_value(const DatabaseResult *res, int row, int col);

int database_is_null(const DatabaseResult *res, int row, int col);
int wait_for_read(int sock);


#endif