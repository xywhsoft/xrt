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

static int query_posting_count(
    const char *sDbPath,
    const char *sNamespace,
    const char *sTerm,
    int *piTermCount
)
{
    sqlite3 *pDb = NULL;
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    if ( piTermCount ) {
        *piTermCount = 0;
    }
    if ( !sDbPath || !sNamespace || !sTerm || !piTermCount ) {
        return 1;
    }

    iRc = sqlite3_open(sDbPath, &pDb);
    if ( iRc != SQLITE_OK ) {
        if ( pDb ) {
            sqlite3_close(pDb);
        }
        return 2;
    }

    iRc = sqlite3_prepare_v2(
        pDb,
        "SELECT COALESCE(SUM(term_count), 0) "
        "FROM xllm_memory_sparse_posting "
        "WHERE namespace = ?1 AND term = ?2;",
        -1,
        &pStmt,
        NULL
    );
    if ( iRc != SQLITE_OK ) {
        sqlite3_close(pDb);
        return 3;
    }

    sqlite3_bind_text(pStmt, 1, sNamespace, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(pStmt, 2, sTerm, -1, SQLITE_TRANSIENT);
    iRc = sqlite3_step(pStmt);
    if ( iRc == SQLITE_ROW ) {
        *piTermCount = sqlite3_column_int(pStmt, 0);
        iRc = SQLITE_DONE;
    }

    sqlite3_finalize(pStmt);
    sqlite3_close(pDb);
    return iRc == SQLITE_DONE ? 0 : 4;
}

static int ingest_record(
    xllm_memory *pMemory,
    const char *sRecordId,
    const char *sTitle,
    const char *sSourceUri,
    const char *sText,
    xllm_error *pError
)
{
    xllm_memory_ingest_options tOptions;

    xllm_memory_ingest_options_init(&tOptions);
    tOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tOptions.sRecordId = sRecordId;
    tOptions.sTitle = sTitle;
    tOptions.sSourceUri = sSourceUri;
    tOptions.sText = sText;
    tOptions.bReplaceExisting = true;
    tOptions.uChunkChars = 4096u;
    return xllm_memory_ingest_text(pMemory, &tOptions, pError);
}

static int assert_top_hit(
    xllm_memory *pMemory,
    const char *sQuery,
    const char *sExpectedRecordId,
    size_t iExpectedMinHits,
    xllm_error *pError
)
{
    xllm_memory_search_options tOptions;
    xllm_memory_search_result tResult;
    int iStatus;
    int iRc = 1;

    xllm_memory_search_options_init(&tOptions);
    memset(&tResult, 0, sizeof(tResult));
    tOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tOptions.sQuery = sQuery;
    tOptions.uMaxHits = 3u;
    tOptions.uMaxCharsPerHit = 4096u;

    iStatus = xllm_memory_search(pMemory, &tOptions, &tResult, pError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "search '%s' failed: %s\n", sQuery, pError->sMessage ? pError->sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tResult.iHitCount >= iExpectedMinHits, "unexpected sparse hit count") != 0 ) {
        goto cleanup;
    }
    if ( sExpectedRecordId ) {
        if ( require_true(tResult.iHitCount > 0u, "expected sparse hit") != 0 ||
             require_true(tResult.pHits[0].sRecordId != NULL, "top hit record id missing") != 0 ||
             require_true(strcmp(tResult.pHits[0].sRecordId, sExpectedRecordId) == 0, "unexpected sparse top hit") != 0 ) {
            goto cleanup;
        }
    }
    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tResult);
    return iRc;
}

int main(void)
{
    const char *sDbPath = "build\\smoke_memory_builtin_sparse.db";
    const char *sNamespace = "smoke-memory-builtin-sparse";
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    int iPostingCount = 0;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_search_options_init(&tSearchOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_error_init(&tError);

    remove(sDbPath);
    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = sNamespace;
    tMemoryOptions.sSqlitePath = sDbPath;
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.bEnableHybridSearch = false;
    tMemoryOptions.uDefaultChunkChars = 4096u;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    if ( ingest_record(
            pMemory,
            "flux-runbook",
            "Flux Runbook",
            "workspace://docs/flux-runbook.md",
            "flux capacitor rollback checklist flux owner handoff",
            &tError
         ) != XRT_NET_OK ||
         ingest_record(
            pMemory,
            "generic-rollback",
            "Generic Rollback",
            "workspace://docs/generic-rollback.md",
            "rollback checklist checklist owner handoff procedure",
            &tError
         ) != XRT_NET_OK ||
         ingest_record(
            pMemory,
            "catalog-only",
            "Catalog Reference",
            "workspace://docs/catalog.md",
            "catalog catalog catalog reference",
            &tError
         ) != XRT_NET_OK ) {
        fprintf(stderr, "sparse ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    if ( assert_top_hit(pMemory, "flux rollback", "flux-runbook", 2u, &tError) != 0 ||
         assert_top_hit(pMemory, "runbook", "flux-runbook", 1u, &tError) != 0 ) {
        goto cleanup;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSearchOptions.sQuery = "cat";
    tSearchOptions.uMaxHits = 1u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "cat boundary search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount == 0u, "sparse token search should not match catalog substring") != 0 ) {
        goto cleanup;
    }
    xllm_memory_search_result_reset(&tSearchResult);

    xllm_memory_destroy(pMemory);
    pMemory = NULL;

    if ( query_posting_count(sDbPath, sNamespace, "flux", &iPostingCount) != 0 ||
         require_true(iPostingCount == 2, "expected persisted flux sparse postings") != 0 ) {
        goto cleanup;
    }

    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory reload failed: %d\n", iStatus);
        goto cleanup;
    }
    if ( assert_top_hit(pMemory, "flux rollback", "flux-runbook", 2u, &tError) != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_builtin_sparse ok\n");
    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tSearchResult);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_reset(&tError);
    return iRc;
}
