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

static int32 demo_session_chat(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    const char *sLastUser = NULL;
    uint32 uUserCount;
    char sText[128];
    xllm_response *pResponse;

    (void)pCtx;
    (void)pOptions;
    (void)pError;

    if ( !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    uUserCount = demo_count_user_messages(pRequest, &sLastUser);
    if ( !sLastUser ) {
        sLastUser = "(none)";
    }

    if ( snprintf(sText, sizeof(sText), "users=%u last=%s", (unsigned)uUserCount, sLastUser) <= 0 ) {
        return XRT_NET_ERROR;
    }

    pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        return XRT_NET_ERROR;
    }

    pResponse->sId = demo_dupstr("mock-session-state-options");
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

static int demo_expect_table_int(xvalue tValue, const char *sKey, int64 iExpected, int iExitCode)
{
    int64 iActual = xvoTableGetInt(tValue, (str)sKey, 0u);
    if ( iActual != iExpected ) {
        fprintf(stderr, "unexpected %s: %lld (expected %lld)\n", sKey, (long long)iActual, (long long)iExpected);
        return iExitCode;
    }
    return 0;
}

static int demo_expect_table_float(xvalue tValue, const char *sKey, double fExpected, int iExitCode)
{
    double fActual = xvoTableGetFloat(tValue, (str)sKey, 0u);
    double fDelta = fActual - fExpected;

    if ( fDelta < 0.0 ) {
        fDelta = -fDelta;
    }
    if ( fDelta > 0.0001 ) {
        fprintf(stderr, "unexpected %s: %.4f (expected %.4f)\n", sKey, fActual, fExpected);
        return iExitCode;
    }
    return 0;
}

static int demo_expect_table_text(xvalue tValue, const char *sKey, const char *sExpected, int iExitCode)
{
    const char *sActual = (const char *)xvoTableGetText(tValue, (str)sKey, 0u);
    if ( (sActual == NULL) != (sExpected == NULL) ||
         (sActual && sExpected && strcmp(sActual, sExpected) != 0) ) {
        fprintf(stderr, "unexpected %s: %s (expected %s)\n",
            sKey,
            sActual ? sActual : "(null)",
            sExpected ? sExpected : "(null)");
        return iExitCode;
    }
    return 0;
}

static int demo_expect_state_version_import(xllm_runtime *pRuntime, xvalue tSerializedState, int64 iVersion, int iExitCode)
{
    xllm_session_state *pCompatState = NULL;
    xllm_session *pCompatSession = NULL;

    if ( !xvoTableSetInt(tSerializedState, (str)"version", 0u, iVersion) ) {
        fprintf(stderr, "set compat state version failed: %lld\n", (long long)iVersion);
        return iExitCode;
    }
    if ( xllm_session_state_from_xvalue(tSerializedState, &pCompatState) != XRT_NET_OK || !pCompatState ) {
        fprintf(stderr, "compat state from xvalue failed: %lld\n", (long long)iVersion);
        return iExitCode;
    }
    if ( xllm_session_import_state(pRuntime, pCompatState, &pCompatSession) != XRT_NET_OK || !pCompatSession ) {
        fprintf(stderr, "compat state import failed: %lld\n", (long long)iVersion);
        xllm_session_state_free(pCompatState);
        return iExitCode;
    }

    xllm_session_destroy(pCompatSession);
    xllm_session_state_free(pCompatState);
    return 0;
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_session *pSession = NULL;
    xllm_session *pImported = NULL;
    xllm_session_state *pState = NULL;
    xllm_session_state *pRoundtripState = NULL;
    xllm_response *pResponse = NULL;
    xllm_adapter tAdapter;
    xllm_profile tProfile;
    xllm_session_options tSessionOptions;
    xllm_compact_result tCompactResult;
    xllm_turn tTurn;
    xvalue tSerializedState = NULL;
    int iStatus;

    memset(&tAdapter, 0, sizeof(tAdapter));
    memset(&tProfile, 0, sizeof(tProfile));
    memset(&tCompactResult, 0, sizeof(tCompactResult));

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 1;
    }

    tAdapter.sName = "mock_session_state_options";
    tAdapter.pfnChat = demo_session_chat;
    if ( xllm_register_adapter(pRuntime, &tAdapter) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        return 2;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "mock-session-state-options";
    tProfile.sProvider = "mock";
    tProfile.sAdapter = "mock_session_state_options";
    tProfile.tModels.tText.sModelId = "mock-text";
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 3;
    }

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "mock-session-state-options";
    tSessionOptions.sSystemPrompt = "session state options";
    tSessionOptions.bEnableAutoCompact = false;
    tSessionOptions.fCompactTriggerRatio = 0.25;
    tSessionOptions.uCompactTriggerTurns = 2u;
    tSessionOptions.uReserveOutputTokens = 77u;
    tSessionOptions.uKeepRecentTurns = 1u;
    tSessionOptions.bKeepActiveToolChain = false;
    tSessionOptions.eCompactStrategy = XLLM_COMPACT_TRUNCATE;
    tSessionOptions.sSummarizerProfileId = "mock-summarizer-profile";
    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "create session failed\n");
        return 4;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "alpha") != XRT_NET_OK ) return 5;
    if ( xllm_session_chat(pSession, &tTurn, NULL, &pResponse) != XRT_NET_OK ) return 6;
    iStatus = demo_expect_text(pResponse, "users=1 last=alpha", 7);
    if ( iStatus != 0 ) return iStatus;
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "beta") != XRT_NET_OK ) return 8;
    if ( xllm_session_chat(pSession, &tTurn, NULL, &pResponse) != XRT_NET_OK ) return 9;
    iStatus = demo_expect_text(pResponse, "users=2 last=beta", 10);
    if ( iStatus != 0 ) return iStatus;
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "gamma") != XRT_NET_OK ) return 11;
    if ( xllm_session_chat(pSession, &tTurn, NULL, &pResponse) != XRT_NET_OK ) return 12;
    iStatus = demo_expect_text(pResponse, "users=3 last=gamma", 13);
    if ( iStatus != 0 ) return iStatus;
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    if ( xllm_session_export_state(pSession, &pState) != XRT_NET_OK || !pState ) {
        fprintf(stderr, "export session state failed\n");
        return 14;
    }
    if ( xllm_session_state_to_xvalue(pState, &tSerializedState) != XRT_NET_OK || !tSerializedState ) {
        fprintf(stderr, "session state to xvalue failed\n");
        return 15;
    }
    iStatus = demo_expect_table_int(tSerializedState, "version", 2, 16);
    if ( iStatus != 0 ) return iStatus;
    iStatus = demo_expect_table_text(tSerializedState, "summarizer_profile_id", "mock-summarizer-profile", 17);
    if ( iStatus != 0 ) return iStatus;
    iStatus = demo_expect_table_int(tSerializedState, "enable_auto_compact", 0, 18);
    if ( iStatus != 0 ) return iStatus;
    iStatus = demo_expect_table_float(tSerializedState, "compact_trigger_ratio", 0.25, 19);
    if ( iStatus != 0 ) return iStatus;
    iStatus = demo_expect_table_int(tSerializedState, "compact_trigger_turns", 2, 20);
    if ( iStatus != 0 ) return iStatus;
    iStatus = demo_expect_table_int(tSerializedState, "reserve_output_tokens", 77, 21);
    if ( iStatus != 0 ) return iStatus;
    iStatus = demo_expect_table_int(tSerializedState, "keep_recent_turns", 1, 22);
    if ( iStatus != 0 ) return iStatus;
    iStatus = demo_expect_table_int(tSerializedState, "keep_active_tool_chain", 0, 23);
    if ( iStatus != 0 ) return iStatus;
    iStatus = demo_expect_table_int(tSerializedState, "compact_strategy", XLLM_COMPACT_TRUNCATE, 24);
    if ( iStatus != 0 ) return iStatus;

    if ( xllm_session_state_from_xvalue(tSerializedState, &pRoundtripState) != XRT_NET_OK || !pRoundtripState ) {
        fprintf(stderr, "session state from xvalue failed\n");
        return 25;
    }
    if ( xllm_session_import_state(pRuntime, pRoundtripState, &pImported) != XRT_NET_OK || !pImported ) {
        fprintf(stderr, "import session state failed\n");
        return 26;
    }

    if ( xllm_session_compact(pImported, NULL, &tCompactResult) != XRT_NET_OK ) {
        fprintf(stderr, "imported session compact failed\n");
        return 27;
    }
    if ( !tCompactResult.bCompacted || tCompactResult.bSummarized ) {
        fprintf(stderr, "imported session compact did not preserve truncate strategy\n");
        return 28;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "delta") != XRT_NET_OK ) return 29;
    if ( xllm_session_chat(pImported, &tTurn, NULL, &pResponse) != XRT_NET_OK ) return 30;
    iStatus = demo_expect_text(pResponse, "users=2 last=delta", 31);
    if ( iStatus != 0 ) return iStatus;

    iStatus = demo_expect_state_version_import(pRuntime, tSerializedState, 1, 32);
    if ( iStatus != 0 ) return iStatus;
    iStatus = demo_expect_state_version_import(pRuntime, tSerializedState, 99, 33);
    if ( iStatus != 0 ) return iStatus;
    if ( !xvoTableSetInt(tSerializedState, (str)"version", 0u, 2) ) return 34;

    printf("ok: %s\n", xllm_response_get_text(pResponse));

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    if ( tSerializedState ) {
        xvoUnref(tSerializedState);
    }
    xllm_session_state_free(pRoundtripState);
    xllm_session_state_free(pState);
    xllm_session_destroy(pImported);
    xllm_session_destroy(pSession);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
