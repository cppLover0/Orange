#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sqlite3.h>

void execute_pragma(sqlite3 *db, const char *sql) {
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("[PRAGMA] %s -> %s\n", sql, sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    } else {
        fprintf(stderr, "Execution error: %s\n", sqlite3_errmsg(db));
    }
}

int main() {
    sqlite3 *db;
    char *err_msg = 0;
    const char *db_name = "test_shm.db";

    int rc = sqlite3_open(db_name, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    printf("1. Database %s opened successfully.\n", db_name);

    execute_pragma(db, "PRAGMA journal_mode=WAL;");
    execute_pragma(db, "PRAGMA synchronous=NORMAL;");

    const char *sql_create = "CREATE TABLE IF NOT EXISTS Users(Id INT, Name TEXT);";
    rc = sqlite3_exec(db, sql_create, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }

    printf("2. Table created. Waiting for observation...\n");
    printf("   Open another terminal and check files via: ls -la test_shm.db*\n\n");

    for (int i = 0; i < 5; i++) {
        printf("[Process] Executing transaction %d/5...\n", i + 1);
        
        rc = sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, NULL);
        
        char insert_query[128];
        snprintf(insert_query, sizeof(insert_query), "INSERT INTO Users VALUES(%d, 'User_%d');", i, i);
        sqlite3_exec(db, insert_query, 0, 0, NULL);
        
        rc = sqlite3_exec(db, "COMMIT;", 0, 0, NULL);
        
        sleep(3); 
    }

    printf("\n3. Closing database...\n");
    sqlite3_close(db);
    printf("4. Database closed. SHM file should be removed.\n");

    return 0;
}
