#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"
#include "xllm-memory.h"

static int require_true(int condition, const char *sMessage)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static int query_journal_mode(const char *sDbPath, char *sMode, size_t iModeSize)
{
    sqlite3 *pDb = NULL;
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    if ( sMode && iModeSize > 0u ) {
        sMode[0] = '\0';
    }
    if ( !sDbPath || !sMode || iModeSize == 0u ) {
        return 1;
    }

    iRc = sqlite3_open_v2(sDbPath, &pDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL);
    if ( iRc != SQLITE_OK ) {
        if ( pDb ) {
            sqlite3_close(pDb);
        }
        return 2;
    }

    iRc = sqlite3_prepare_v2(pDb, "PRAGMA journal_mode;", -1, &pStmt, NULL);
    if ( iRc != SQLITE_OK ) {
        sqlite3_close(pDb);
        return 3;
    }
    iRc = sqlite3_step(pStmt);
    if ( iRc == SQLITE_ROW ) {
        const unsigned char *sText = sqlite3_column_text(pStmt, 0);
        if ( sText ) {
            snprintf(sMode, iModeSize, "%s", (const char *)sText);
        }
        iRc = SQLITE_DONE;
    }

    sqlite3_finalize(pStmt);
    sqlite3_close(pDb);
    return iRc == SQLITE_DONE ? 0 : 4;
}

int main(void)
{
    const char *sDbPath = "build\\smoke_memory_sqlite_policy.db";
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_options tIngestOptions;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    xllm_memory_diagnostics tDiagnostics;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    char sJournalMode[32];
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_options_init(&tIngestOptions);
    xllm_memory_search_options_init(&tSearchOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_memory_diagnostics_init(&tDiagnostics);
    xllm_error_init(&tError);

    remove(sDbPath);
    if ( xllm_runtime_create(&tRuntimeOptions, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "runtime create failed\n");
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-sqlite-policy";
    tMemoryOptions.sSqlitePath = sDbPath;
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.uSqliteBusyTimeoutMs = 7000u;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    iStatus = xllm_memory_get_diagnostics(pMemory, &tDiagnostics, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "diagnostics failed: %d\n", iStatus);
        goto cleanup;
    }
    if ( require_true(tDiagnostics.bSqliteOpen, "expected sqlite open diagnostics") != 0 ||
         require_true(tDiagnostics.bSqliteWalRequested, "expected wal requested diagnostics") != 0 ||
         require_true(tDiagnostics.bSqliteWalEnabled, "expected wal enabled diagnostics") != 0 ||
         require_true(tDiagnostics.uSqliteBusyTimeoutMs == 7000u, "busy timeout diagnostics mismatch") != 0 ) {
        goto cleanup;
    }

    tIngestOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngestOptions.sRecordId = "sqlite-policy-record";
    tIngestOptions.sTitle = "SQLite policy record";
    tIngestOptions.sSourceUri = "memory://sqlite-policy";
    tIngestOptions.sText = "sqlite wal busy timeout transaction policy record";
    tIngestOptions.bReplaceExisting = true;
    tIngestOptions.uChunkChars = 1024u;
    iStatus = xllm_memory_ingest_text(pMemory, &tIngestOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_destroy(pMemory);
    pMemory = NULL;

    if ( query_journal_mode(sDbPath, sJournalMode, sizeof(sJournalMode)) != 0 ) {
        fprintf(stderr, "journal mode query failed\n");
        goto cleanup;
    }
    if ( require_true(strcmp(sJournalMode, "wal") == 0, "expected sqlite journal_mode=wal") != 0 ) {
        fprintf(stderr, "journal mode was %s\n", sJournalMode);
        goto cleanup;
    }

    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory reopen failed: %d\n", iStatus);
        goto cleanup;
    }
    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSearchOptions.sQuery = "transaction policy record";
    tSearchOptions.uMaxHits = 1u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "search after reopen failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount == 1u, "expected reopened sqlite search hit") != 0 ||
         require_true(strcmp(tSearchResult.pHits[0].sRecordId, "sqlite-policy-record") == 0, "unexpected reopened hit") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_sqlite_policy ok\n");
    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tSearchResult);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
