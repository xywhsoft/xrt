#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

static int exec_sql(sqlite3 *pDb, const char *sSql)
{
    char *sErr = NULL;
    int iRc = sqlite3_exec(pDb, sSql, NULL, NULL, &sErr);
    if ( iRc != SQLITE_OK ) {
        fprintf(stderr, "sql failed rc=%d sql=%s\n", iRc, sSql ? sSql : "(null)");
        if ( sErr ) {
            fprintf(stderr, "sqlite exec error: %s\n", sErr);
            sqlite3_free(sErr);
        } else {
            fprintf(stderr, "sqlite errmsg: %s\n", sqlite3_errmsg(pDb));
        }
    }
    return iRc;
}

int main(void)
{
    sqlite3 *pDb = NULL;
    sqlite3_stmt *pStmt = NULL;
    char *sLoadErr = NULL;
    int iRc;
    const char *sDbPath = "build\\smoke_sqlite_vec_probe.db";
    const char *sExtPath = "lib\\sqlite-vec\\sqlite-vec.dll";

    remove(sDbPath);
    iRc = sqlite3_open_v2(
        sDbPath,
        &pDb,
        SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX,
        NULL
    );
    if ( iRc != SQLITE_OK || !pDb ) {
        fprintf(stderr, "sqlite open failed rc=%d msg=%s\n", iRc, pDb ? sqlite3_errmsg(pDb) : "(null)");
        if ( pDb ) {
            sqlite3_close(pDb);
        }
        return 1;
    }

    iRc = sqlite3_enable_load_extension(pDb, 1);
    if ( iRc != SQLITE_OK ) {
        fprintf(stderr, "enable load extension failed rc=%d msg=%s\n", iRc, sqlite3_errmsg(pDb));
        sqlite3_close(pDb);
        return 2;
    }
    iRc = sqlite3_load_extension(pDb, sExtPath, NULL, &sLoadErr);
    if ( iRc != SQLITE_OK ) {
        if ( sLoadErr ) {
            sqlite3_free(sLoadErr);
            sLoadErr = NULL;
        }
        iRc = sqlite3_load_extension(pDb, sExtPath, "sqlite3_vec_init", &sLoadErr);
    }
    sqlite3_enable_load_extension(pDb, 0);
    if ( iRc != SQLITE_OK ) {
        fprintf(stderr, "load extension failed rc=%d msg=%s ext=%s\n", iRc, sLoadErr ? sLoadErr : sqlite3_errmsg(pDb), sExtPath);
        if ( sLoadErr ) {
            sqlite3_free(sLoadErr);
        }
        sqlite3_close(pDb);
        return 3;
    }
    if ( sLoadErr ) {
        sqlite3_free(sLoadErr);
    }

    if ( exec_sql(pDb, "CREATE VIRTUAL TABLE vec_probe USING vec0(embedding float[4] distance_metric=cosine);") != SQLITE_OK ) {
        sqlite3_close(pDb);
        return 4;
    }

    if ( exec_sql(pDb, "INSERT INTO vec_probe(rowid, embedding) VALUES(1, '[1,0,0,0]');") != SQLITE_OK ) {
        sqlite3_close(pDb);
        return 5;
    }

    iRc = sqlite3_prepare_v2(
        pDb,
        "SELECT rowid, distance FROM vec_probe WHERE embedding MATCH ?1 AND k = 1 ORDER BY distance ASC;",
        -1,
        &pStmt,
        NULL
    );
    if ( iRc != SQLITE_OK ) {
        fprintf(stderr, "prepare query failed rc=%d msg=%s\n", iRc, sqlite3_errmsg(pDb));
        sqlite3_close(pDb);
        return 6;
    }
    sqlite3_bind_text(pStmt, 1, "[1,0,0,0]", -1, SQLITE_STATIC);
    iRc = sqlite3_step(pStmt);
    if ( iRc != SQLITE_ROW ) {
        fprintf(stderr, "query step failed rc=%d msg=%s\n", iRc, sqlite3_errmsg(pDb));
        sqlite3_finalize(pStmt);
        sqlite3_close(pDb);
        return 7;
    }

    printf(
        "smoke_sqlite_vec_probe ok rowid=%lld distance=%g\n",
        (long long)sqlite3_column_int64(pStmt, 0),
        sqlite3_column_double(pStmt, 1)
    );

    sqlite3_finalize(pStmt);
    sqlite3_close(pDb);
    return 0;
}
