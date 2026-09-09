#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

static int require_true(int bCondition, const char *sMessage)
{
    if ( !bCondition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static int has_term(const xllm_memory_retrieval_debug_dump *pDump, const char *sTerm)
{
    size_t i;

    if ( !pDump || !sTerm ) {
        return 0;
    }
    for ( i = 0u; i < pDump->iTermCount; ++i ) {
        if ( pDump->pTerms[i].sTerm && strcmp(pDump->pTerms[i].sTerm, sTerm) == 0 ) {
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_options tIngestOptions;
    xllm_memory_retrieval_debug_options tDebugOptions;
    xllm_memory_retrieval_debug_dump tDump;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_options_init(&tIngestOptions);
    xllm_memory_retrieval_debug_options_init(&tDebugOptions);
    memset(&tDump, 0, sizeof(tDump));

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-retrieval-debug";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.bEnableHybridSearch = true;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tIngestOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngestOptions.sRecordId = "debug-alpha";
    tIngestOptions.sTitle = "debug alpha";
    tIngestOptions.sSourceUri = "memory://debug/alpha";
    tIngestOptions.sText = "retrieval debug alpha ranks fusion query terms";
    tIngestOptions.bReplaceExisting = true;
    iStatus = xllm_memory_ingest_text(pMemory, &tIngestOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "ingest alpha failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tIngestOptions.sRecordId = "debug-beta";
    tIngestOptions.sTitle = "debug beta";
    tIngestOptions.sSourceUri = "memory://debug/beta";
    tIngestOptions.sText = "unrelated beta storage record";
    iStatus = xllm_memory_ingest_text(pMemory, &tIngestOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "ingest beta failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tDebugOptions.tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tDebugOptions.tSearchOptions.sQuery = "retrieval fusion";
    tDebugOptions.tSearchOptions.uMaxHits = 2u;
    tDebugOptions.tSearchOptions.tMinScore.bSet = true;
    tDebugOptions.tSearchOptions.tMinScore.fValue = 0.0;
    tDebugOptions.uMaxCandidates = 8u;
    tDebugOptions.bIncludeBelowMinScore = true;
    iStatus = xllm_memory_search_debug(pMemory, &tDebugOptions, &tDump, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "search debug failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    if ( require_true(tDump.sQuery && strcmp(tDump.sQuery, "retrieval fusion") == 0, "expected debug query") != 0 ||
         require_true(tDump.eScheme == XLLM_MEMORY_SCHEME_BUILTIN_SPARSE, "expected sparse scheme") != 0 ||
         require_true(tDump.iTermCount >= 2u, "expected query terms") != 0 ||
         require_true(has_term(&tDump, "retrieval"), "expected retrieval term") != 0 ||
         require_true(has_term(&tDump, "fusion"), "expected fusion term") != 0 ||
         require_true(tDump.iFilteredRecordCount == 2u, "expected two filtered records") != 0 ||
         require_true(tDump.iScoredCandidateCount == 2u, "expected two scored candidates") != 0 ||
         require_true(tDump.iCandidateCount == 2u, "expected two debug candidates") != 0 ||
         require_true(tDump.tSearchResult.iHitCount == 2u, "expected two debug search hits") != 0 ||
         require_true(tDump.pCandidates[0].sRecordId != NULL, "expected candidate record id") != 0 ||
         require_true(tDump.pCandidates[0].bIncludedInResult, "expected first candidate included") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_retrieval_debug ok\n");
    iRc = 0;

cleanup:
    xllm_memory_retrieval_debug_dump_reset(&tDump);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
