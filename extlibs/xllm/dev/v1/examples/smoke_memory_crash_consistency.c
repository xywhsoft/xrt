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

static int simulate_uncommitted_write_interruption(const char *sDbPath)
{
    sqlite3 *pDb = NULL;
    int iRc;

    iRc = sqlite3_open_v2(sDbPath, &pDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL);
    if ( iRc != SQLITE_OK ) {
        if ( pDb ) {
            sqlite3_close(pDb);
        }
        return 1;
    }
    if ( sqlite3_exec(pDb, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL) != SQLITE_OK ) {
        sqlite3_close(pDb);
        return 2;
    }
    if ( sqlite3_exec(pDb, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, NULL) != SQLITE_OK ) {
        sqlite3_close(pDb);
        return 3;
    }
    if ( sqlite3_exec(
            pDb,
            "INSERT INTO xllm_memory_record("
            "namespace, scope, record_id, title, source_uri, text_body, "
            "memory_profile_id, retrieval_profile_id, embed_profile_id, index_profile_id, profile_version"
            ") VALUES("
            "'smoke-memory-crash-consistency', 2, 'interrupted-record', 'interrupted', "
            "'memory://crash/interrupted', 'uncommitted interrupted record text', "
            "'builtin_sparse:v1', 'builtin_sparse:v1', 'none:v1', 'builtin_sparse:v1', 1"
            ");",
            NULL,
            NULL,
            NULL
         ) != SQLITE_OK ) {
        sqlite3_close(pDb);
        return 4;
    }
    if ( sqlite3_exec(
            pDb,
            "INSERT INTO xllm_memory_chunk("
            "namespace, scope, record_id, chunk_index, chunk_id, text_body, "
            "memory_profile_id, chunk_profile_id, retrieval_profile_id, embed_profile_id, index_profile_id, "
            "profile_version, content_hash, start_byte, end_byte"
            ") VALUES("
            "'smoke-memory-crash-consistency', 2, 'interrupted-record', 0, 'interrupted-record#0', "
            "'uncommitted interrupted chunk text', "
            "'builtin_sparse:v1', 'chunk:fixed:v1', 'builtin_sparse:v1', 'none:v1', 'builtin_sparse:v1', "
            "1, 12345, 0, 35"
            ");",
            NULL,
            NULL,
            NULL
         ) != SQLITE_OK ) {
        sqlite3_close(pDb);
        return 5;
    }

    /* Deliberately close without COMMIT/ROLLBACK to emulate process loss before commit. */
    if ( sqlite3_close(pDb) != SQLITE_OK ) {
        return 6;
    }
    return 0;
}

int main(void)
{
    const char *sDbPath = "build\\smoke_memory_crash_consistency.db";
    xllm_runtime *pRuntime = NULL;
    xllm_memory *pMemory = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_options tIngestOptions;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecordList;
    xllm_memory_health_check tHealth;
    xllm_error tError;
    int iStatus;
    int iRc = 1;

    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_options_init(&tIngestOptions);
    xllm_memory_list_options_init(&tListOptions);
    xllm_memory_health_check_init(&tHealth);
    memset(&tRecordList, 0, sizeof(tRecordList));
    remove_db_files(sDbPath);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "runtime create failed\n");
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-memory-crash-consistency";
    tMemoryOptions.sSqlitePath = sDbPath;
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tIngestOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngestOptions.sRecordId = "committed-record";
    tIngestOptions.sTitle = "committed";
    tIngestOptions.sSourceUri = "memory://crash/committed";
    tIngestOptions.sText = "committed crash consistency record survives reopen";
    tIngestOptions.bReplaceExisting = true;
    iStatus = xllm_memory_ingest_text(pMemory, &tIngestOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "committed ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_destroy(pMemory);
    pMemory = NULL;

    if ( simulate_uncommitted_write_interruption(sDbPath) != 0 ) {
        fprintf(stderr, "failed to simulate uncommitted write interruption\n");
        goto cleanup;
    }

    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory reopen failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    iStatus = xllm_memory_check_health(pMemory, NULL, &tHealth, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "health check failed after reopen: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tHealth.bOk, "expected healthy db after interrupted write rollback") != 0 ||
         require_true(tHealth.iRecordCount == 1u, "expected only committed record after reopen") != 0 ||
         require_true(tHealth.iSqliteRecordCount == 1u, "expected only committed sqlite record after reopen") != 0 ||
         require_true(tHealth.iOrphanChunkCount == 0u, "expected no orphan chunks after interrupted write") != 0 ) {
        goto cleanup;
    }

    tListOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tListOptions.sRecordId = "committed-record";
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecordList, &tError);
    if ( iStatus != XRT_NET_OK ||
         require_true(tRecordList.iRecordCount == 1u, "expected committed record to remain listed") != 0 ) {
        fprintf(stderr, "list committed record failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecordList);

    tListOptions.sRecordId = "interrupted-record";
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecordList, &tError);
    if ( iStatus != XRT_NET_OK ||
         require_true(tRecordList.iRecordCount == 0u, "expected interrupted record to be rolled back") != 0 ) {
        fprintf(stderr, "list interrupted record failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    printf("smoke_memory_crash_consistency ok\n");
    iRc = 0;

cleanup:
    xllm_memory_record_list_result_reset(&tRecordList);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    remove_db_files(sDbPath);
    return iRc;
}
