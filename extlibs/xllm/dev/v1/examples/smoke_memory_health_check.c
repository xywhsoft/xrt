#include <stdio.h>
#include <string.h>

#include "sqlite3.h"
#include "xllm-memory.h"

static int require_true(int bCondition, const char *sMessage)
{
    if ( !bCondition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static void remove_db_files(const char *sDbPath)
{
    char sWalPath[512];
    char sShmPath[512];

    remove(sDbPath);
    snprintf(sWalPath, sizeof(sWalPath), "%s-wal", sDbPath);
    snprintf(sShmPath, sizeof(sShmPath), "%s-shm", sDbPath);
    remove(sWalPath);
    remove(sShmPath);
}

static int insert_orphan_chunk(const char *sDbPath)
{
    sqlite3 *pDb = NULL;
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    iRc = sqlite3_open_v2(sDbPath, &pDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL);
    if ( iRc != SQLITE_OK ) {
        if ( pDb ) {
            sqlite3_close(pDb);
        }
        return 1;
    }
    iRc = sqlite3_exec(pDb, "PRAGMA foreign_keys = OFF;", NULL, NULL, NULL);
    if ( iRc != SQLITE_OK ) {
        sqlite3_close(pDb);
        return 2;
    }
    iRc = sqlite3_prepare_v2(
        pDb,
        "INSERT INTO xllm_memory_chunk(namespace, scope, record_id, chunk_index, chunk_id, text_body, profile_version) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, 1);",
        -1,
        &pStmt,
        NULL
    );
    if ( iRc != SQLITE_OK ) {
        sqlite3_close(pDb);
        return 3;
    }
    sqlite3_bind_text(pStmt, 1, "smoke-memory-health-check", -1, SQLITE_STATIC);
    sqlite3_bind_int(pStmt, 2, (int)XLLM_MEMORY_SCOPE_KNOWLEDGE);
    sqlite3_bind_text(pStmt, 3, "missing-record", -1, SQLITE_STATIC);
    sqlite3_bind_int(pStmt, 4, 0);
    sqlite3_bind_text(pStmt, 5, "orphan-chunk", -1, SQLITE_STATIC);
    sqlite3_bind_text(pStmt, 6, "orphan chunk without parent record", -1, SQLITE_STATIC);

    iRc = sqlite3_step(pStmt);
    sqlite3_finalize(pStmt);
    sqlite3_close(pDb);
    return iRc == SQLITE_DONE ? 0 : 4;
}

int main(void)
{
    const char *sDbPath = "build\\smoke_memory_health_check.db";
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_options tIngestOptions;
    xllm_memory_health_check tHealth;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_options_init(&tIngestOptions);
    xllm_memory_health_check_init(&tHealth);
    remove_db_files(sDbPath);

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-memory-health-check";
    tMemoryOptions.sSqlitePath = sDbPath;
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tIngestOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngestOptions.sRecordId = "health-record";
    tIngestOptions.sTitle = "health record";
    tIngestOptions.sSourceUri = "memory://health-check";
    tIngestOptions.sText = "health check sqlite schema profile sparse postings consistency record";
    tIngestOptions.bReplaceExisting = true;
    iStatus = xllm_memory_ingest_text(pMemory, &tIngestOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    iStatus = xllm_memory_check_health(pMemory, NULL, &tHealth, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "health check failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tHealth.bOk, "expected healthy memory db") != 0 ||
         require_true(tHealth.bSqliteOpen, "expected sqlite open") != 0 ||
         require_true(tHealth.bSchemaOk, "expected schema ok") != 0 ||
         require_true(tHealth.bProfileOk, "expected profile ok") != 0 ||
         require_true(tHealth.bSparsePostingsOk, "expected sparse postings ok") != 0 ||
         require_true(tHealth.iRecordCount == 1u, "expected one in-memory record") != 0 ||
         require_true(tHealth.iSqliteRecordCount == 1u, "expected one sqlite record") != 0 ||
         require_true(tHealth.iChunkCount == tHealth.iSqliteChunkCount, "expected chunk count parity") != 0 ||
         require_true(tHealth.iOrphanChunkCount == 0u, "expected no orphan chunks") != 0 ||
         require_true(tHealth.iSparseMissingChunkCount == 0u, "expected no missing sparse postings") != 0 ) {
        goto cleanup;
    }

    xllm_memory_destroy(pMemory);
    pMemory = NULL;
    if ( insert_orphan_chunk(sDbPath) != 0 ) {
        fprintf(stderr, "failed to insert orphan chunk\n");
        goto cleanup;
    }

    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory reopen failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    iStatus = xllm_memory_check_health(pMemory, NULL, &tHealth, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "health check after orphan failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(!tHealth.bOk, "expected unhealthy memory db after orphan insert") != 0 ||
         require_true(tHealth.iOrphanChunkCount == 1u, "expected one orphan chunk") != 0 ||
         require_true(tHealth.iSqliteChunkCount == tHealth.iChunkCount + 1u, "expected sqlite chunk count to include orphan") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_health_check ok\n");
    iRc = 0;

cleanup:
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
