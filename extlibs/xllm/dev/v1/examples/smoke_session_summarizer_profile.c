#include "xllm-session.h"

#include <stdio.h>
#include <string.h>

static char *demo_dupstr(const char *sText)
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

typedef struct {
    bool bSawCompactResult;
    bool bSawRemoteSummary;
    bool bSawLocalFallback;
    uint32 uRemovedTurns;
    uint32 uSummaryTurnsAfter;
} demo_trace_state;

static bool demo_request_contains_text(const xllm_request *pRequest, const char *sNeedle)
{
    size_t i;

    if ( !pRequest || !sNeedle || !sNeedle[0] ) {
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

static uint32 demo_count_user_messages(const xllm_request *pRequest, const char **psLastUserText)
{
    uint32 uCount = 0u;
    size_t i;

    if ( psLastUserText ) {
        *psLastUserText = NULL;
    }
    if ( !pRequest ) {
        return 0u;
    }

    for ( i = 0; i < pRequest->iMessageCount; ++i ) {
        const xllm_message *pMessage = &pRequest->pMessages[i];
        size_t j;

        if ( pMessage->eRole != XLLM_ROLE_USER ) {
            continue;
        }

        ++uCount;
        for ( j = 0; j < pMessage->iPartCount; ++j ) {
            const xllm_content_part *pPart = &pMessage->pParts[j];
            if ( pPart->eKind == XLLM_PART_TEXT &&
                 pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT &&
                 pPart->as.tSource.as.sText ) {
                if ( psLastUserText ) {
                    *psLastUserText = pPart->as.tSource.as.sText;
                }
            }
        }
    }

    return uCount;
}

static bool demo_request_has_remote_summary(const xllm_request *pRequest)
{
    return demo_request_contains_text(pRequest, "REMOTE_ONLY_SUMMARY");
}

static int32 demo_mock_chat(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    const char *sLastUser = NULL;
    const char *sText;
    char sBuffer[160];
    xllm_response *pResponse;

    (void)pCtx;
    (void)pOptions;
    (void)pError;

    if ( !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    if ( strcmp(pProfile->sId, "mock-session-summarizer") == 0 ) {
        if ( !demo_request_contains_text(pRequest, "Update the rolling session summary.") ||
             !demo_request_contains_text(pRequest, "alpha") ) {
            return XRT_NET_ERROR;
        }
        sText = "Facts:\n- REMOTE_ONLY_SUMMARY\nOpen items:\n- beta follow-up\nTool state:\n- none\nRecent context:\n- gamma completed";
    } else {
        uint32 uUserCount = demo_count_user_messages(pRequest, &sLastUser);

        if ( !sLastUser ) {
            sLastUser = "(none)";
        }
        if ( snprintf(
                sBuffer,
                sizeof(sBuffer),
                "summary_remote=%s users=%u last=%s",
                demo_request_has_remote_summary(pRequest) ? "yes" : "no",
                (unsigned)uUserCount,
                sLastUser
             ) <= 0 ) {
            return XRT_NET_ERROR;
        }
        sText = sBuffer;
    }

    pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        return XRT_NET_ERROR;
    }

    pResponse->sId = demo_dupstr("mock-session-summarizer");
    pResponse->sProvider = demo_dupstr(pProfile->sProvider ? pProfile->sProvider : "mock");
    pResponse->sProfileId = demo_dupstr(pProfile->sId);
    pResponse->sModel = demo_dupstr(pProfile->tModels.tText.sModelId ? pProfile->tModels.tText.sModelId : "mock-text");
    pResponse->eStatus = XLLM_STATUS_COMPLETED;
    pResponse->sFinishReason = demo_dupstr("stop");
    pResponse->sVisibleText = demo_dupstr(sText);
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
    pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.sMimeType = demo_dupstr("text/plain");
    pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.as.sText = demo_dupstr(sText);
    if ( !pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.sMimeType ||
         !pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.as.sText ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    *ppResponse = pResponse;
    return XRT_NET_OK;
}

static int demo_expect_text(const xllm_response *pResponse, const char *sExpected, int iExitCode)
{
    const char *sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, sExpected) != 0 ) {
        fprintf(stderr, "unexpected response text: %s (expected %s)\n", sText ? sText : "(null)", sExpected);
        return iExitCode;
    }
    return 0;
}

static void demo_trace_callback(void *pCtx, xllm_trace_kind eKind, const xvalue *pPayload)
{
    demo_trace_state *pState = (demo_trace_state *)pCtx;
    const char *sPhase;
    const char *sSummarySource;

    if ( !pState || eKind != XLLM_TRACE_COMPACT || !pPayload || !*pPayload ) {
        return;
    }

    sPhase = (const char *)xvoTableGetText(*pPayload, (str)"phase", 0u);
    if ( !sPhase || strcmp(sPhase, "compact_result") != 0 ) {
        return;
    }

    pState->bSawCompactResult = xvoTableGetBool(*pPayload, (str)"compacted", 0u);
    sSummarySource = (const char *)xvoTableGetText(*pPayload, (str)"summary_source", 0u);
    if ( sSummarySource && strcmp(sSummarySource, "remote") == 0 ) {
        pState->bSawRemoteSummary = true;
    }
    if ( sSummarySource && strcmp(sSummarySource, "local_fallback") == 0 ) {
        pState->bSawLocalFallback = true;
    }
    pState->uRemovedTurns = (uint32)xvoTableGetInt(*pPayload, (str)"removed_turns", 0u);
    pState->uSummaryTurnsAfter = (uint32)xvoTableGetInt(*pPayload, (str)"summary_turns_after", 0u);
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_session *pSession = NULL;
    xllm_session *pImported = NULL;
    xllm_session_state *pState = NULL;
    xllm_response *pResponse = NULL;
    xllm_adapter tAdapter;
    xllm_profile tMainProfile;
    xllm_profile tSummarizerProfile;
    xllm_session_options tSessionOptions;
    xllm_compact_result tCompactResult;
    xllm_turn tTurn;
    demo_trace_state tTraceState;
    int iStatus;

    memset(&tAdapter, 0, sizeof(tAdapter));
    memset(&tMainProfile, 0, sizeof(tMainProfile));
    memset(&tSummarizerProfile, 0, sizeof(tSummarizerProfile));
    memset(&tCompactResult, 0, sizeof(tCompactResult));
    memset(&tTraceState, 0, sizeof(tTraceState));

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 1;
    }
    if ( xllm_runtime_set_trace_callback(pRuntime, demo_trace_callback, &tTraceState) != XRT_NET_OK ) {
        fprintf(stderr, "set trace callback failed\n");
        return 2;
    }

    tAdapter.sName = "mock_session_summarizer";
    tAdapter.pfnChat = demo_mock_chat;
    if ( xllm_register_adapter(pRuntime, &tAdapter) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        return 3;
    }

    xllm_profile_init(&tMainProfile);
    tMainProfile.sId = "mock-session-main";
    tMainProfile.sProvider = "mock";
    tMainProfile.sAdapter = "mock_session_summarizer";
    tMainProfile.tModels.tText.sModelId = "mock-main";
    if ( xllm_register_profile(pRuntime, &tMainProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register main profile failed\n");
        return 4;
    }

    xllm_profile_init(&tSummarizerProfile);
    tSummarizerProfile.sId = "mock-session-summarizer";
    tSummarizerProfile.sProvider = "mock";
    tSummarizerProfile.sAdapter = "mock_session_summarizer";
    tSummarizerProfile.tModels.tText.sModelId = "mock-summarizer";
    if ( xllm_register_profile(pRuntime, &tSummarizerProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register summarizer profile failed\n");
        return 5;
    }

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "mock-session-main";
    tSessionOptions.sSystemPrompt = "session summarizer profile test";
    tSessionOptions.uKeepRecentTurns = 1u;
    tSessionOptions.eCompactStrategy = XLLM_COMPACT_SUMMARIZE;
    tSessionOptions.sSummarizerProfileId = "mock-session-summarizer";
    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "create session failed\n");
        return 6;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "alpha") != XRT_NET_OK ) return 6;
    if ( xllm_session_chat(pSession, &tTurn, NULL, &pResponse) != XRT_NET_OK ) return 7;
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "beta") != XRT_NET_OK ) return 8;
    if ( xllm_session_chat(pSession, &tTurn, NULL, &pResponse) != XRT_NET_OK ) return 9;
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "gamma") != XRT_NET_OK ) return 10;
    if ( xllm_session_chat(pSession, &tTurn, NULL, &pResponse) != XRT_NET_OK ) return 11;
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    if ( xllm_session_compact(pSession, NULL, &tCompactResult) != XRT_NET_OK ) {
        fprintf(stderr, "session compact failed\n");
        return 13;
    }
    if ( !tCompactResult.bCompacted || !tCompactResult.bSummarized ) {
        fprintf(stderr, "session compact did not use summarizer profile\n");
        return 14;
    }
    if ( !tTraceState.bSawCompactResult || !tTraceState.bSawRemoteSummary || tTraceState.bSawLocalFallback ) {
        fprintf(stderr, "trace callback did not capture remote summary result correctly\n");
        return 15;
    }
    if ( tTraceState.uRemovedTurns == 0u || tTraceState.uSummaryTurnsAfter == 0u ) {
        fprintf(stderr, "trace callback did not include compact counters\n");
        return 16;
    }

    if ( xllm_session_export_state(pSession, &pState) != XRT_NET_OK || !pState ) {
        fprintf(stderr, "export session state failed\n");
        return 17;
    }
    if ( xllm_session_import_state(pRuntime, pState, &pImported) != XRT_NET_OK || !pImported ) {
        fprintf(stderr, "import session state failed\n");
        return 18;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "delta") != XRT_NET_OK ) return 16;
    if ( xllm_session_chat(pImported, &tTurn, NULL, &pResponse) != XRT_NET_OK ) return 17;
    iStatus = demo_expect_text(pResponse, "summary_remote=yes users=2 last=delta", 18);
    if ( iStatus != 0 ) {
        return iStatus;
    }

    printf("ok: %s\n", xllm_response_get_text(pResponse));

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_session_state_free(pState);
    xllm_session_destroy(pImported);
    xllm_session_destroy(pSession);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
