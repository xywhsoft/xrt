#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <windows.h>
#else
#include <time.h>
#endif

#include "xllm-memory.h"

typedef struct {
    const char *sRecordId;
    const char *sTitle;
    const char *sSourceUri;
    const char *sText;
    xllm_memory_scope eScope;
    int bConversationSummary;
} eval_record;

typedef struct {
    const char *sId;
    const char *sGroup;
    const char *sQuery;
    const char *sExpectedRecordId;
    xllm_memory_scope eScope;
} eval_query;

typedef struct {
    const eval_query *pQuery;
    size_t iHitCount;
    size_t iRank;
    double fLatencyMs;
    size_t iContextChars;
    char sTopRecordId[128];
    double fTopScore;
} eval_query_result;

static uint64 eval_now_ms(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return (uint64)GetTickCount64();
#else
    struct timespec tNow;
    if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) {
        return 0u;
    }
    return ((uint64)tNow.tv_sec * 1000u) + (uint64)(tNow.tv_nsec / 1000000u);
#endif
}

static int ingest_eval_record(
    xllm_memory *pMemory,
    const eval_record *pRecord,
    xllm_error *pError
)
{
    if ( pRecord->bConversationSummary ) {
        xllm_memory_ingest_turn_response_options tOptions;

        xllm_memory_ingest_turn_response_options_init(&tOptions);
        tOptions.eScope = pRecord->eScope;
        tOptions.eExtractionPolicy = XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY;
        tOptions.sRecordId = pRecord->sRecordId;
        tOptions.sTitle = pRecord->sTitle;
        tOptions.sSourceUri = pRecord->sSourceUri;
        tOptions.sConversationId = "eval-conversation";
        tOptions.sSummaryText = pRecord->sText;
        tOptions.uChunkChars = 4096u;
        return xllm_memory_ingest_turn_response(pMemory, &tOptions, pError);
    } else {
        xllm_memory_ingest_options tOptions;

        xllm_memory_ingest_options_init(&tOptions);
        tOptions.eScope = pRecord->eScope;
        tOptions.sRecordId = pRecord->sRecordId;
        tOptions.sTitle = pRecord->sTitle;
        tOptions.sSourceUri = pRecord->sSourceUri;
        tOptions.sText = pRecord->sText;
        tOptions.bReplaceExisting = true;
        tOptions.uChunkChars = 4096u;
        return xllm_memory_ingest_text(pMemory, &tOptions, pError);
    }
}

static int run_query(
    xllm_memory *pMemory,
    const eval_query *pQuery,
    eval_query_result *pResult,
    xllm_error *pError
)
{
    xllm_memory_search_options tOptions;
    xllm_memory_search_result tSearchResult;
    uint64 uStart;
    uint64 uEnd;
    size_t i;
    int iStatus;

    memset(pResult, 0, sizeof(*pResult));
    pResult->pQuery = pQuery;
    pResult->iRank = 0u;
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_memory_search_options_init(&tOptions);
    tOptions.eScope = pQuery->eScope;
    tOptions.sQuery = pQuery->sQuery;
    tOptions.uMaxHits = 5u;
    tOptions.uMaxCharsPerHit = 4096u;

    uStart = eval_now_ms();
    iStatus = xllm_memory_search(pMemory, &tOptions, &tSearchResult, pError);
    uEnd = eval_now_ms();
    if ( iStatus != XRT_NET_OK ) {
        xllm_memory_search_result_reset(&tSearchResult);
        return iStatus;
    }

    pResult->fLatencyMs = (double)(uEnd - uStart);
    pResult->iHitCount = tSearchResult.iHitCount;
    if ( tSearchResult.iHitCount > 0u ) {
        snprintf(
            pResult->sTopRecordId,
            sizeof(pResult->sTopRecordId),
            "%s",
            tSearchResult.pHits[0].sRecordId ? tSearchResult.pHits[0].sRecordId : ""
        );
        pResult->fTopScore = tSearchResult.pHits[0].fScore;
    }

    for ( i = 0u; i < tSearchResult.iHitCount; ++i ) {
        const xllm_memory_hit *pHit = &tSearchResult.pHits[i];
        if ( pHit->sText ) {
            pResult->iContextChars += strlen(pHit->sText);
        }
        if ( pHit->sRecordId &&
             strcmp(pHit->sRecordId, pQuery->sExpectedRecordId) == 0 &&
             pResult->iRank == 0u ) {
            pResult->iRank = i + 1u;
        }
    }

    xllm_memory_search_result_reset(&tSearchResult);
    return XRT_NET_OK;
}

static int write_reports(
    const char *sOutputDir,
    const eval_query_result *pResults,
    size_t iResultCount
)
{
    char sJsonPath[1024];
    char sTxtPath[1024];
    FILE *pJson;
    FILE *pTxt;
    size_t i;
    size_t iHitAt1 = 0u;
    size_t iHitAt5 = 0u;
    double fMrr = 0.0;
    double fPrecisionAt1 = 0.0;
    double fPrecisionAt5 = 0.0;
    double fTotalLatencyMs = 0.0;
    size_t iTotalContextChars = 0u;

    if ( !sOutputDir || !sOutputDir[0] ) {
        sOutputDir = "build\\memory_eval";
    }

    snprintf(sJsonPath, sizeof(sJsonPath), "%s\\memory_eval_report.json", sOutputDir);
    snprintf(sTxtPath, sizeof(sTxtPath), "%s\\memory_eval_report.txt", sOutputDir);

    pJson = fopen(sJsonPath, "wb");
    pTxt = fopen(sTxtPath, "wb");
    if ( !pJson || !pTxt ) {
        if ( pJson ) fclose(pJson);
        if ( pTxt ) fclose(pTxt);
        return 1;
    }

    for ( i = 0u; i < iResultCount; ++i ) {
        const eval_query_result *pResult = &pResults[i];
        if ( pResult->iRank == 1u ) {
            ++iHitAt1;
        }
        if ( pResult->iRank > 0u && pResult->iRank <= 5u ) {
            ++iHitAt5;
            fMrr += 1.0 / (double)pResult->iRank;
            fPrecisionAt5 += 1.0 / 5.0;
        }
        if ( pResult->iRank == 1u ) {
            fPrecisionAt1 += 1.0;
        }
        fTotalLatencyMs += pResult->fLatencyMs;
        iTotalContextChars += pResult->iContextChars;
    }

    if ( iResultCount > 0u ) {
        fMrr /= (double)iResultCount;
        fPrecisionAt1 /= (double)iResultCount;
        fPrecisionAt5 /= (double)iResultCount;
    }

    fprintf(pJson, "{\n");
    fprintf(pJson, "  \"dataset\": \"memory_eval_v1\",\n");
    fprintf(pJson, "  \"query_count\": %u,\n", (unsigned)iResultCount);
    fprintf(pJson, "  \"metrics\": {\n");
    fprintf(pJson, "    \"recall_at_1\": %.6f,\n", iResultCount ? ((double)iHitAt1 / (double)iResultCount) : 0.0);
    fprintf(pJson, "    \"recall_at_5\": %.6f,\n", iResultCount ? ((double)iHitAt5 / (double)iResultCount) : 0.0);
    fprintf(pJson, "    \"mrr\": %.6f,\n", fMrr);
    fprintf(pJson, "    \"precision_at_1\": %.6f,\n", fPrecisionAt1);
    fprintf(pJson, "    \"precision_at_5\": %.6f,\n", fPrecisionAt5);
    fprintf(pJson, "    \"avg_latency_ms\": %.3f,\n", iResultCount ? (fTotalLatencyMs / (double)iResultCount) : 0.0);
    fprintf(pJson, "    \"avg_context_chars\": %.3f\n", iResultCount ? ((double)iTotalContextChars / (double)iResultCount) : 0.0);
    fprintf(pJson, "  },\n");
    fprintf(pJson, "  \"queries\": [\n");
    for ( i = 0u; i < iResultCount; ++i ) {
        const eval_query_result *pResult = &pResults[i];
        fprintf(pJson, "    {\n");
        fprintf(pJson, "      \"id\": \"%s\",\n", pResult->pQuery->sId);
        fprintf(pJson, "      \"group\": \"%s\",\n", pResult->pQuery->sGroup);
        fprintf(pJson, "      \"expected_record_id\": \"%s\",\n", pResult->pQuery->sExpectedRecordId);
        fprintf(pJson, "      \"top_record_id\": \"%s\",\n", pResult->sTopRecordId);
        fprintf(pJson, "      \"rank\": %u,\n", (unsigned)pResult->iRank);
        fprintf(pJson, "      \"hit_count\": %u,\n", (unsigned)pResult->iHitCount);
        fprintf(pJson, "      \"top_score\": %.6f,\n", pResult->fTopScore);
        fprintf(pJson, "      \"latency_ms\": %.3f,\n", pResult->fLatencyMs);
        fprintf(pJson, "      \"context_chars\": %u\n", (unsigned)pResult->iContextChars);
        fprintf(pJson, "    }%s\n", (i + 1u < iResultCount) ? "," : "");
    }
    fprintf(pJson, "  ]\n");
    fprintf(pJson, "}\n");

    fprintf(pTxt, "dataset: memory_eval_v1\n");
    fprintf(pTxt, "query_count: %u\n", (unsigned)iResultCount);
    fprintf(pTxt, "recall_at_1: %.6f\n", iResultCount ? ((double)iHitAt1 / (double)iResultCount) : 0.0);
    fprintf(pTxt, "recall_at_5: %.6f\n", iResultCount ? ((double)iHitAt5 / (double)iResultCount) : 0.0);
    fprintf(pTxt, "mrr: %.6f\n", fMrr);
    fprintf(pTxt, "precision_at_1: %.6f\n", fPrecisionAt1);
    fprintf(pTxt, "precision_at_5: %.6f\n", fPrecisionAt5);
    fprintf(pTxt, "avg_latency_ms: %.3f\n", iResultCount ? (fTotalLatencyMs / (double)iResultCount) : 0.0);
    fprintf(pTxt, "avg_context_chars: %.3f\n", iResultCount ? ((double)iTotalContextChars / (double)iResultCount) : 0.0);
    fprintf(pTxt, "\n");
    for ( i = 0u; i < iResultCount; ++i ) {
        const eval_query_result *pResult = &pResults[i];
        fprintf(
            pTxt,
            "%s [%s] rank=%u hits=%u top=%s expected=%s latency_ms=%.3f context_chars=%u\n",
            pResult->pQuery->sId,
            pResult->pQuery->sGroup,
            (unsigned)pResult->iRank,
            (unsigned)pResult->iHitCount,
            pResult->sTopRecordId,
            pResult->pQuery->sExpectedRecordId,
            pResult->fLatencyMs,
            (unsigned)pResult->iContextChars
        );
    }

    fclose(pJson);
    fclose(pTxt);
    printf("memory eval report json: %s\n", sJsonPath);
    printf("memory eval report txt:  %s\n", sTxtPath);
    return 0;
}

int main(int argc, char **argv)
{
    static const eval_record pRecords[] = {
        {
            "code.git_safety",
            "Git Safety Rules",
            "workspace://src/git_safety.c",
            "dirty worktree protection uses preserve user changes and non destructive git operations before branch checkout or reset",
            XLLM_MEMORY_SCOPE_KNOWLEDGE,
            0
        },
        {
            "code.watcher_debounce",
            "Watcher Debounce Worker",
            "workspace://src/watcher_debounce.c",
            "filesystem watcher debounce coalesces rapid file events before indexing workspace changes in the memory watcher worker",
            XLLM_MEMORY_SCOPE_KNOWLEDGE,
            0
        },
        {
            "docs.memory_bridge",
            "Memory Bridge Defaults",
            "workspace://docs/memory_bridge.md",
            "memory bridge defaults to search before chat and does not automatically ingest after chat unless host explicitly enables writes",
            XLLM_MEMORY_SCOPE_KNOWLEDGE,
            0
        },
        {
            "docs.workspace_security",
            "Workspace Security Defaults",
            "workspace://docs/workspace_security.md",
            "workspace defaults skip env files token secret credential names private keys certificates binaries large files and ignored directories",
            XLLM_MEMORY_SCOPE_KNOWLEDGE,
            0
        },
        {
            "conv.summary_style",
            "Conversation Summary Preference",
            "conversation://eval/summary-style",
            "User prefers concise progress summaries with direct unfinished task lists and concrete verification results.",
            XLLM_MEMORY_SCOPE_MEMORY,
            1
        },
        {
            "conv.production_goal",
            "Production Goal",
            "conversation://eval/production-goal",
            "The core product goal is using xllm as a reusable agent development library for AI IDE and claw infrastructure.",
            XLLM_MEMORY_SCOPE_MEMORY,
            1
        }
    };
    static const eval_query pQueries[] = {
        {
            "code.git_safety.lookup",
            "code",
            "dirty worktree protection non destructive git reset checkout",
            "code.git_safety",
            XLLM_MEMORY_SCOPE_KNOWLEDGE
        },
        {
            "code.watcher_debounce.lookup",
            "code",
            "debounce rapid filesystem watcher events before indexing",
            "code.watcher_debounce",
            XLLM_MEMORY_SCOPE_KNOWLEDGE
        },
        {
            "docs.memory_bridge.lookup",
            "docs",
            "memory bridge search before chat not automatically ingest after chat",
            "docs.memory_bridge",
            XLLM_MEMORY_SCOPE_KNOWLEDGE
        },
        {
            "docs.workspace_security.lookup",
            "docs",
            "workspace defaults skip env token secret credential private key certificate",
            "docs.workspace_security",
            XLLM_MEMORY_SCOPE_KNOWLEDGE
        },
        {
            "conversation.summary_style.lookup",
            "conversation",
            "user prefers concise progress summaries unfinished task list verification",
            "conv.summary_style",
            XLLM_MEMORY_SCOPE_MEMORY
        },
        {
            "conversation.production_goal.lookup",
            "conversation",
            "xllm reusable agent development library AI IDE claw infrastructure",
            "conv.production_goal",
            XLLM_MEMORY_SCOPE_MEMORY
        }
    };
    const char *sOutputDir = (argc > 1 && argv[1] && argv[1][0]) ? argv[1] : "build\\memory_eval";
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory *pMemory = NULL;
    xllm_error tError;
    eval_query_result pResults[sizeof(pQueries) / sizeof(pQueries[0])];
    size_t i;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_error_init(&tError);
    memset(pResults, 0, sizeof(pResults));

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "memory-eval-v1";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.bEnableHybridSearch = false;
    tMemoryOptions.uDefaultChunkChars = 4096u;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    for ( i = 0u; i < sizeof(pRecords) / sizeof(pRecords[0]); ++i ) {
        iStatus = ingest_eval_record(pMemory, &pRecords[i], &tError);
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "ingest %s failed: %s\n", pRecords[i].sRecordId, tError.sMessage ? tError.sMessage : "(null)");
            goto cleanup;
        }
    }

    for ( i = 0u; i < sizeof(pQueries) / sizeof(pQueries[0]); ++i ) {
        iStatus = run_query(pMemory, &pQueries[i], &pResults[i], &tError);
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "query %s failed: %s\n", pQueries[i].sId, tError.sMessage ? tError.sMessage : "(null)");
            goto cleanup;
        }
    }

    if ( write_reports(sOutputDir, pResults, sizeof(pQueries) / sizeof(pQueries[0])) != 0 ) {
        fprintf(stderr, "failed to write eval reports\n");
        goto cleanup;
    }

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
