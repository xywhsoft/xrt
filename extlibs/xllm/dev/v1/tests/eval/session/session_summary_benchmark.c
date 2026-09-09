#include "xllm-session.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <time.h>
#endif

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *sId;
    const char *sGroup;
    const char *pTurns[6];
    size_t iTurnCount;
    const char *pExpected[6];
    size_t iExpectedCount;
} bench_case;

typedef struct {
    const bench_case *pCase;
    int iStatus;
    bool bCompacted;
    bool bSummarized;
    uint32 uInputBefore;
    uint32 uInputAfter;
    size_t iSummaryChars;
    size_t iRetainedKeywords;
    double fLatencyMs;
} bench_result;

static uint64 bench_now_ms(void)
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

static char *bench_dupstr(const char *sText)
{
    size_t iLen;
    char *sCopy;

    if ( !sText ) {
        return NULL;
    }
    iLen = strlen(sText);
    sCopy = (char *)xrtCalloc(iLen + 1u, sizeof(char));
    if ( !sCopy ) {
        return NULL;
    }
    memcpy(sCopy, sText, iLen);
    sCopy[iLen] = '\0';
    return sCopy;
}

static bool bench_request_contains_text(const xllm_request *pRequest, const char *sNeedle)
{
    size_t i;

    if ( !pRequest || !sNeedle ) {
        return false;
    }
    for ( i = 0; i < pRequest->iMessageCount; ++i ) {
        const xllm_message *pMessage = &pRequest->pMessages[i];
        size_t j;
        for ( j = 0; j < pMessage->iPartCount; ++j ) {
            const xllm_content_part *pPart = &pMessage->pParts[j];
            const char *sText;
            if ( pPart->eKind != XLLM_PART_TEXT ||
                 pPart->as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
                continue;
            }
            sText = pPart->as.tSource.as.sText;
            if ( sText && strstr(sText, sNeedle) ) {
                return true;
            }
        }
    }
    return false;
}

static int bench_make_text_response(
    const xllm_profile *pProfile,
    const char *sId,
    const char *sText,
    xllm_response **ppResponse
)
{
    xllm_response *pResponse;

    if ( !pProfile || !sId || !sText || !ppResponse ) {
        return XRT_NET_ERROR;
    }
    pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        return XRT_NET_ERROR;
    }
    pResponse->sId = bench_dupstr(sId);
    pResponse->sProvider = bench_dupstr(pProfile->sProvider ? pProfile->sProvider : "mock");
    pResponse->sProfileId = bench_dupstr(pProfile->sId);
    pResponse->sModel = bench_dupstr(pProfile->tModels.tText.sModelId ? pProfile->tModels.tText.sModelId : "mock-text");
    pResponse->eStatus = XLLM_STATUS_COMPLETED;
    pResponse->sFinishReason = bench_dupstr("stop");
    pResponse->sVisibleText = bench_dupstr(sText);
    pResponse->iOutputCount = 1u;
    pResponse->pOutputs = (xllm_output_item *)xrtCalloc(1u, sizeof(xllm_output_item));
    if ( !pResponse->pOutputs ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }
    pResponse->pOutputs[0].eKind = XLLM_OUTPUT_MESSAGE;
    pResponse->pOutputs[0].as.tMessage.iPartCount = 1u;
    pResponse->pOutputs[0].as.tMessage.pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pResponse->pOutputs[0].as.tMessage.pParts ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }
    pResponse->pOutputs[0].as.tMessage.pParts[0].eKind = XLLM_PART_TEXT;
    pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.sMimeType = bench_dupstr("text/plain");
    pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.as.sText = bench_dupstr(sText);
    if ( !pResponse->sId || !pResponse->sProvider || !pResponse->sProfileId ||
         !pResponse->sModel || !pResponse->sFinishReason || !pResponse->sVisibleText ||
         !pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.sMimeType ||
         !pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.as.sText ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    *ppResponse = pResponse;
    return XRT_NET_OK;
}

static const char *bench_summary_for_request(const xllm_request *pRequest)
{
    if ( bench_request_contains_text(pRequest, "alpha-api") ) {
        return "alpha-api alice qa-freeze beta cleanup";
    }
    if ( bench_request_contains_text(pRequest, "weather-tool") ) {
        return "weather-tool sunny cache ttl 15m call-weather-1";
    }
    if ( bench_request_contains_text(pRequest, "prefers-terse") ) {
        return "prefers-terse tradeoffs no emojis";
    }
    return "generic summary";
}

static int32 bench_chat(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    (void)pCtx;
    (void)pOptions;
    (void)pError;

    if ( bench_request_contains_text(pRequest, "Write the updated summary now.") ) {
        return bench_make_text_response(pProfile, "bench-summary", bench_summary_for_request(pRequest), ppResponse);
    }
    return bench_make_text_response(pProfile, "bench-chat", "ack", ppResponse);
}

static size_t bench_count_retained_keywords(const char *sSummary, const bench_case *pCase)
{
    size_t i;
    size_t iCount = 0u;

    if ( !sSummary || !pCase ) {
        return 0u;
    }
    for ( i = 0u; i < pCase->iExpectedCount; ++i ) {
        if ( pCase->pExpected[i] && strstr(sSummary, pCase->pExpected[i]) ) {
            ++iCount;
        }
    }
    return iCount;
}

static const char *bench_state_summary_text(xvalue tStateValue)
{
    if ( !tStateValue ) {
        return NULL;
    }
    return (const char *)xvoTableGetText(tStateValue, (str)"session_summary", 0u);
}

static int bench_run_case(
    xllm_runtime *pRuntime,
    const bench_case *pCase,
    bench_result *pResult
)
{
    xllm_session *pSession = NULL;
    xllm_response *pResponse = NULL;
    xllm_session_state *pState = NULL;
    xvalue tStateValue = NULL;
    xllm_session_options tSessionOptions;
    xllm_compact_result tCompactResult;
    xllm_turn tTurn;
    const char *sSummary;
    uint64 uStart;
    uint64 uEnd;
    size_t i;
    int iStatus = XRT_NET_OK;

    memset(pResult, 0, sizeof(*pResult));
    pResult->pCase = pCase;
    memset(&tCompactResult, 0, sizeof(tCompactResult));

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "session-summary-bench";
    tSessionOptions.sSystemPrompt = "session summary benchmark";
    tSessionOptions.bEnableAutoCompact = false;
    tSessionOptions.uKeepRecentTurns = 1u;
    tSessionOptions.eCompactStrategy = XLLM_COMPACT_SUMMARIZE;
    tSessionOptions.sSummarizerProfileId = "session-summary-bench";
    tSessionOptions.eCompactStrategy = XLLM_COMPACT_SUMMARIZE;

    iStatus = xllm_session_create(pRuntime, &tSessionOptions, &pSession);
    if ( iStatus != XRT_NET_OK || !pSession ) {
        pResult->iStatus = iStatus;
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < pCase->iTurnCount; ++i ) {
        xllm_turn_init(&tTurn);
        if ( xllm_turn_add_user_text(&tTurn, pCase->pTurns[i]) != XRT_NET_OK ) {
            iStatus = XRT_NET_ERROR;
            xllm_turn_reset(&tTurn);
            break;
        }
        iStatus = xllm_session_chat(pSession, &tTurn, NULL, &pResponse);
        xllm_turn_reset(&tTurn);
        if ( iStatus != XRT_NET_OK ) {
            break;
        }
        xllm_response_free(pResponse);
        pResponse = NULL;
    }
    if ( iStatus == XRT_NET_OK ) {
        uStart = bench_now_ms();
        iStatus = xllm_session_compact(pSession, NULL, &tCompactResult);
        uEnd = bench_now_ms();
        pResult->fLatencyMs = (double)(uEnd - uStart);
    }
    if ( iStatus == XRT_NET_OK ) {
        pResult->bCompacted = tCompactResult.bCompacted;
        pResult->bSummarized = tCompactResult.bSummarized;
        pResult->uInputBefore = tCompactResult.uInputTokensBefore;
        pResult->uInputAfter = tCompactResult.uInputTokensAfter;

        iStatus = xllm_session_export_state(pSession, &pState);
    }
    if ( iStatus == XRT_NET_OK ) {
        iStatus = xllm_session_state_to_xvalue(pState, &tStateValue);
    }
    if ( iStatus == XRT_NET_OK ) {
        sSummary = bench_state_summary_text(tStateValue);
        pResult->iSummaryChars = sSummary ? strlen(sSummary) : 0u;
        pResult->iRetainedKeywords = bench_count_retained_keywords(sSummary, pCase);
    }

    pResult->iStatus = iStatus;
    if ( tStateValue ) {
        xvoUnref(tStateValue);
    }
    xllm_session_state_free(pState);
    xllm_response_free(pResponse);
    xllm_session_destroy(pSession);
    return iStatus;
}

static int bench_write_reports(const char *sOutputDir, const bench_result *pResults, size_t iResultCount)
{
    char sJsonPath[1024];
    char sTxtPath[1024];
    FILE *pJson;
    FILE *pTxt;
    size_t i;
    double fTotalQuality = 0.0;
    double fTotalCompression = 0.0;
    double fTotalLatency = 0.0;
    double fTotalSummaryChars = 0.0;

    if ( !sOutputDir || !sOutputDir[0] ) {
        sOutputDir = "build\\session_summary_benchmark";
    }

    snprintf(sJsonPath, sizeof(sJsonPath), "%s\\session_summary_benchmark_report.json", sOutputDir);
    snprintf(sTxtPath, sizeof(sTxtPath), "%s\\session_summary_benchmark_report.txt", sOutputDir);
    pJson = fopen(sJsonPath, "wb");
    pTxt = fopen(sTxtPath, "wb");
    if ( !pJson || !pTxt ) {
        if ( pJson ) fclose(pJson);
        if ( pTxt ) fclose(pTxt);
        return 1;
    }

    for ( i = 0u; i < iResultCount; ++i ) {
        const bench_result *pResult = &pResults[i];
        double fQuality = pResult->pCase->iExpectedCount ?
            ((double)pResult->iRetainedKeywords / (double)pResult->pCase->iExpectedCount) : 0.0;
        double fCompression = pResult->uInputBefore ?
            (1.0 - ((double)pResult->uInputAfter / (double)pResult->uInputBefore)) : 0.0;
        fTotalQuality += fQuality;
        fTotalCompression += fCompression;
        fTotalLatency += pResult->fLatencyMs;
        fTotalSummaryChars += (double)pResult->iSummaryChars;
    }

    fprintf(pJson, "{\n");
    fprintf(pJson, "  \"dataset\": \"session_summary_benchmark_v1\",\n");
    fprintf(pJson, "  \"case_count\": %u,\n", (unsigned)iResultCount);
    fprintf(pJson, "  \"metrics\": {\n");
    fprintf(pJson, "    \"avg_keyword_retention\": %.6f,\n", iResultCount ? fTotalQuality / (double)iResultCount : 0.0);
    fprintf(pJson, "    \"avg_token_compression\": %.6f,\n", iResultCount ? fTotalCompression / (double)iResultCount : 0.0);
    fprintf(pJson, "    \"avg_latency_ms\": %.3f,\n", iResultCount ? fTotalLatency / (double)iResultCount : 0.0);
    fprintf(pJson, "    \"avg_summary_chars\": %.3f\n", iResultCount ? fTotalSummaryChars / (double)iResultCount : 0.0);
    fprintf(pJson, "  },\n");
    fprintf(pJson, "  \"cases\": [\n");
    for ( i = 0u; i < iResultCount; ++i ) {
        const bench_result *pResult = &pResults[i];
        double fQuality = pResult->pCase->iExpectedCount ?
            ((double)pResult->iRetainedKeywords / (double)pResult->pCase->iExpectedCount) : 0.0;
        double fCompression = pResult->uInputBefore ?
            (1.0 - ((double)pResult->uInputAfter / (double)pResult->uInputBefore)) : 0.0;
        fprintf(pJson, "    {\n");
        fprintf(pJson, "      \"id\": \"%s\",\n", pResult->pCase->sId);
        fprintf(pJson, "      \"group\": \"%s\",\n", pResult->pCase->sGroup);
        fprintf(pJson, "      \"status\": %d,\n", pResult->iStatus);
        fprintf(pJson, "      \"compacted\": %s,\n", pResult->bCompacted ? "true" : "false");
        fprintf(pJson, "      \"summarized\": %s,\n", pResult->bSummarized ? "true" : "false");
        fprintf(pJson, "      \"keyword_retention\": %.6f,\n", fQuality);
        fprintf(pJson, "      \"retained_keywords\": %u,\n", (unsigned)pResult->iRetainedKeywords);
        fprintf(pJson, "      \"expected_keywords\": %u,\n", (unsigned)pResult->pCase->iExpectedCount);
        fprintf(pJson, "      \"input_tokens_before\": %u,\n", (unsigned)pResult->uInputBefore);
        fprintf(pJson, "      \"input_tokens_after\": %u,\n", (unsigned)pResult->uInputAfter);
        fprintf(pJson, "      \"token_compression\": %.6f,\n", fCompression);
        fprintf(pJson, "      \"summary_chars\": %u,\n", (unsigned)pResult->iSummaryChars);
        fprintf(pJson, "      \"latency_ms\": %.3f\n", pResult->fLatencyMs);
        fprintf(pJson, "    }%s\n", (i + 1u < iResultCount) ? "," : "");
    }
    fprintf(pJson, "  ]\n");
    fprintf(pJson, "}\n");

    fprintf(pTxt, "dataset: session_summary_benchmark_v1\n");
    fprintf(pTxt, "case_count: %u\n", (unsigned)iResultCount);
    fprintf(pTxt, "avg_keyword_retention: %.6f\n", iResultCount ? fTotalQuality / (double)iResultCount : 0.0);
    fprintf(pTxt, "avg_token_compression: %.6f\n", iResultCount ? fTotalCompression / (double)iResultCount : 0.0);
    fprintf(pTxt, "avg_latency_ms: %.3f\n", iResultCount ? fTotalLatency / (double)iResultCount : 0.0);
    fprintf(pTxt, "avg_summary_chars: %.3f\n\n", iResultCount ? fTotalSummaryChars / (double)iResultCount : 0.0);
    for ( i = 0u; i < iResultCount; ++i ) {
        const bench_result *pResult = &pResults[i];
        double fQuality = pResult->pCase->iExpectedCount ?
            ((double)pResult->iRetainedKeywords / (double)pResult->pCase->iExpectedCount) : 0.0;
        double fCompression = pResult->uInputBefore ?
            (1.0 - ((double)pResult->uInputAfter / (double)pResult->uInputBefore)) : 0.0;
        fprintf(
            pTxt,
            "%s [%s] quality=%.6f compacted=%s summarized=%s before=%u after=%u compression=%.6f summary_chars=%u latency_ms=%.3f\n",
            pResult->pCase->sId,
            pResult->pCase->sGroup,
            fQuality,
            pResult->bCompacted ? "true" : "false",
            pResult->bSummarized ? "true" : "false",
            (unsigned)pResult->uInputBefore,
            (unsigned)pResult->uInputAfter,
            fCompression,
            (unsigned)pResult->iSummaryChars,
            pResult->fLatencyMs
        );
    }

    fclose(pJson);
    fclose(pTxt);
    printf("session summary benchmark json: %s\n", sJsonPath);
    printf("session summary benchmark txt:  %s\n", sTxtPath);
    return 0;
}

int main(int argc, char **argv)
{
    const char *sOutputDir = (argc > 1 && argv[1]) ? argv[1] : "build\\session_summary_benchmark";
    const bench_case pCases[] = {
        {
            "planning.alpha_api",
            "planning",
            {
                "alpha-api migration is P0 and owner alice will drive it",
                "qa-freeze must happen before release",
                "beta cleanup finished and can be kept as recent context"
            },
            3u,
            { "alpha-api", "alice", "qa-freeze", "beta cleanup" },
            4u
        },
        {
            "tool.weather_handoff",
            "tool",
            {
                "weather-tool should call app.weather.get_current",
                "weather-tool returned sunny for Shanghai",
                "cache ttl 15m and call-weather-1 completed"
            },
            3u,
            { "weather-tool", "sunny", "cache ttl 15m", "call-weather-1" },
            4u
        },
        {
            "preference.terse",
            "preference",
            {
                "user prefers-terse answers for status updates",
                "explain tradeoffs only when changing architecture",
                "no emojis should be used in engineering updates"
            },
            3u,
            { "prefers-terse", "tradeoffs", "no emojis" },
            3u
        }
    };
    xllm_runtime *pRuntime = NULL;
    xllm_adapter tAdapter;
    xllm_profile tProfile;
    bench_result pResults[sizeof(pCases) / sizeof(pCases[0])];
    size_t i;
    int iRc = 1;

    memset(&tAdapter, 0, sizeof(tAdapter));
    memset(&tProfile, 0, sizeof(tProfile));
    memset(pResults, 0, sizeof(pResults));

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "runtime create failed\n");
        return 1;
    }

    tAdapter.sName = "session_summary_bench_adapter";
    tAdapter.pfnChat = bench_chat;
    if ( xllm_register_adapter(pRuntime, &tAdapter) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        goto cleanup;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "session-summary-bench";
    tProfile.sProvider = "mock";
    tProfile.sAdapter = "session_summary_bench_adapter";
    tProfile.tModels.tText.sModelId = "mock-summary-model";
    tProfile.tModels.tText.tCaps.uMaxInputTokens = 128u;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        goto cleanup;
    }

    for ( i = 0u; i < sizeof(pCases) / sizeof(pCases[0]); ++i ) {
        if ( bench_run_case(pRuntime, &pCases[i], &pResults[i]) != XRT_NET_OK ) {
            fprintf(stderr, "benchmark case failed: %s status=%d\n", pCases[i].sId, pResults[i].iStatus);
            goto cleanup;
        }
        if ( !pResults[i].bCompacted || !pResults[i].bSummarized ||
             pResults[i].iRetainedKeywords != pCases[i].iExpectedCount ) {
            fprintf(
                stderr,
                "benchmark quality gate failed: %s compacted=%s summarized=%s retained=%u/%u before=%u after=%u summary_chars=%u\n",
                pCases[i].sId,
                pResults[i].bCompacted ? "true" : "false",
                pResults[i].bSummarized ? "true" : "false",
                (unsigned)pResults[i].iRetainedKeywords,
                (unsigned)pCases[i].iExpectedCount,
                (unsigned)pResults[i].uInputBefore,
                (unsigned)pResults[i].uInputAfter,
                (unsigned)pResults[i].iSummaryChars
            );
            goto cleanup;
        }
    }

    if ( bench_write_reports(sOutputDir, pResults, sizeof(pCases) / sizeof(pCases[0])) != 0 ) {
        fprintf(stderr, "failed to write reports\n");
        goto cleanup;
    }
    iRc = 0;

cleanup:
    xllm_runtime_destroy(pRuntime);
    return iRc;
}
