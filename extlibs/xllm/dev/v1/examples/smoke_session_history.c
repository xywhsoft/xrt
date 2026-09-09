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

    pResponse = (xllm_response *)xrtCalloc(1, sizeof(*pResponse));
    if ( !pResponse ) {
        return XRT_NET_ERROR;
    }

    pResponse->sId = demo_dupstr("mock-session");
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
    if ( !pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.as.sText ) {
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

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_session *pSession = NULL;
    xllm_session *pImported = NULL;
    xllm_session_state *pState = NULL;
    xllm_response *pResponse = NULL;
    xllm_adapter tAdapter;
    xllm_profile tProfile;
    xllm_session_options tSessionOptions;
    xllm_turn tTurn;
    int iStatus;

    memset(&tAdapter, 0, sizeof(tAdapter));
    memset(&tProfile, 0, sizeof(tProfile));

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 1;
    }

    tAdapter.sName = "mock_session";
    tAdapter.pfnChat = demo_session_chat;
    if ( xllm_register_adapter(pRuntime, &tAdapter) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        return 2;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "mock-session";
    tProfile.sProvider = "mock";
    tProfile.sAdapter = "mock_session";
    tProfile.tModels.tText.sModelId = "mock-text";
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 3;
    }

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "mock-session";
    tSessionOptions.sSystemPrompt = "remember recent turns";
    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "create session failed\n");
        return 4;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "alpha") != XRT_NET_OK ) {
        fprintf(stderr, "turn add alpha failed\n");
        return 5;
    }
    iStatus = xllm_session_chat(pSession, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "session chat alpha failed: %d\n", iStatus);
        return 6;
    }
    iStatus = demo_expect_text(pResponse, "users=1 last=alpha", 7);
    if ( iStatus != 0 ) {
        return iStatus;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "beta") != XRT_NET_OK ) {
        fprintf(stderr, "turn add beta failed\n");
        return 8;
    }
    iStatus = xllm_session_chat(pSession, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "session chat beta failed: %d\n", iStatus);
        return 9;
    }
    iStatus = demo_expect_text(pResponse, "users=2 last=beta", 10);
    if ( iStatus != 0 ) {
        return iStatus;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    if ( xllm_session_export_state(pSession, &pState) != XRT_NET_OK || !pState ) {
        fprintf(stderr, "export session state failed\n");
        return 11;
    }
    if ( xllm_session_import_state(pRuntime, pState, &pImported) != XRT_NET_OK || !pImported ) {
        fprintf(stderr, "import session state failed\n");
        return 12;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "gamma") != XRT_NET_OK ) {
        fprintf(stderr, "turn add gamma failed\n");
        return 13;
    }
    iStatus = xllm_session_chat(pImported, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "session chat gamma failed: %d\n", iStatus);
        return 14;
    }
    iStatus = demo_expect_text(pResponse, "users=3 last=gamma", 15);
    if ( iStatus != 0 ) {
        return iStatus;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    if ( xllm_session_clear_history(pImported) != XRT_NET_OK ) {
        fprintf(stderr, "clear history failed\n");
        return 16;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "delta") != XRT_NET_OK ) {
        fprintf(stderr, "turn add delta failed\n");
        return 17;
    }
    iStatus = xllm_session_chat(pImported, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "session chat delta failed: %d\n", iStatus);
        return 18;
    }
    iStatus = demo_expect_text(pResponse, "users=1 last=delta", 19);
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
