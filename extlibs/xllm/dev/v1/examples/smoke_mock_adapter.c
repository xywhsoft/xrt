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
    sCopy = (char *)xrtCalloc(iLen + 1, sizeof(char));
    if ( !sCopy ) {
        return NULL;
    }

    memcpy(sCopy, sText, iLen);
    sCopy[iLen] = '\0';
    return sCopy;
}

static const char *demo_last_user_text(const xllm_request *pRequest)
{
    size_t i;

    if ( !pRequest ) {
        return NULL;
    }

    for ( i = pRequest->iMessageCount; i > 0; --i ) {
        const xllm_message *pMessage = &pRequest->pMessages[i - 1];
        size_t j;

        if ( pMessage->eRole != XLLM_ROLE_USER ) {
            continue;
        }

        for ( j = 0; j < pMessage->iPartCount; ++j ) {
            const xllm_content_part *pPart = &pMessage->pParts[j];
            if ( pPart->eKind == XLLM_PART_TEXT &&
                 pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT ) {
                return pPart->as.tSource.as.sText;
            }
        }
    }

    return NULL;
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
    const char *sUserText;
    const char *sPrefix = "echo: ";
    size_t iPrefixLen;
    size_t iUserLen;
    char *sText;
    xllm_response *pResponse;

    (void)pCtx;
    (void)pError;

    if ( !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    sUserText = demo_last_user_text(pRequest);
    if ( !sUserText ) {
        sUserText = "(empty)";
    }

    iPrefixLen = strlen(sPrefix);
    iUserLen = strlen(sUserText);
    sText = (char *)xrtCalloc(iPrefixLen + iUserLen + 1, sizeof(char));
    if ( !sText ) {
        return XRT_NET_ERROR;
    }

    memcpy(sText, sPrefix, iPrefixLen);
    memcpy(sText + iPrefixLen, sUserText, iUserLen);
    sText[iPrefixLen + iUserLen] = '\0';

    if ( pOptions && pOptions->pfnOnEvent ) {
        xllm_event tEvent;
        memset(&tEvent, 0, sizeof(tEvent));

        tEvent.eType = XLLM_EVENT_START;
        tEvent.as.tStart.sResponseId = "mock-response";
        tEvent.as.tStart.sModel = pProfile->tModels.tText.sModelId;
        if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
            xrtFree(sText);
            return XRT_NET_CANCELLED;
        }

        memset(&tEvent, 0, sizeof(tEvent));
        tEvent.eType = XLLM_EVENT_OUTPUT_BEGIN;
        tEvent.as.tOutputBegin.eKind = XLLM_OUTPUT_MESSAGE;
        if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
            xrtFree(sText);
            return XRT_NET_CANCELLED;
        }

        memset(&tEvent, 0, sizeof(tEvent));
        tEvent.eType = XLLM_EVENT_TEXT_DELTA;
        tEvent.as.tTextDelta.sText = sText;
        if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
            xrtFree(sText);
            return XRT_NET_CANCELLED;
        }

        memset(&tEvent, 0, sizeof(tEvent));
        tEvent.eType = XLLM_EVENT_OUTPUT_END;
        if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
            xrtFree(sText);
            return XRT_NET_CANCELLED;
        }

        memset(&tEvent, 0, sizeof(tEvent));
        tEvent.eType = XLLM_EVENT_END;
        if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
            xrtFree(sText);
            return XRT_NET_CANCELLED;
        }
    }

    pResponse = (xllm_response *)xrtCalloc(1, sizeof(*pResponse));
    if ( !pResponse ) {
        xrtFree(sText);
        return XRT_NET_ERROR;
    }

    pResponse->sId = demo_dupstr("mock-response");
    pResponse->sProvider = demo_dupstr(pProfile->sProvider ? pProfile->sProvider : "mock");
    pResponse->sProfileId = demo_dupstr(pProfile->sId);
    pResponse->sModel = demo_dupstr(pProfile->tModels.tText.sModelId ? pProfile->tModels.tText.sModelId : "mock-text");
    pResponse->eStatus = XLLM_STATUS_COMPLETED;
    pResponse->sFinishReason = demo_dupstr("stop");
    pResponse->sVisibleText = sText;
    pResponse->iOutputCount = 1;
    pResponse->pOutputs = (xllm_output_item *)xrtCalloc(1, sizeof(xllm_output_item));
    if ( !pResponse->pOutputs ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    pResponse->pOutputs[0].eKind = XLLM_OUTPUT_MESSAGE;
    pResponse->pOutputs[0].as.tMessage.iPartCount = 1;
    pResponse->pOutputs[0].as.tMessage.pParts = (xllm_content_part *)xrtCalloc(1, sizeof(xllm_content_part));
    if ( !pResponse->pOutputs[0].as.tMessage.pParts ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    pResponse->pOutputs[0].as.tMessage.pParts[0].eKind = XLLM_PART_TEXT;
    pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.sMimeType = demo_dupstr("text/plain");
    pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.as.sText = demo_dupstr(sText);
    pResponse->tUsage.uInputTokens = 8;
    pResponse->tUsage.uOutputTokens = 4;

    *ppResponse = pResponse;
    return XRT_NET_OK;
}

static bool demo_on_event(const xllm_event *pEvent, void *pUserData)
{
    int *piEventCount = (int *)pUserData;

    if ( piEventCount ) {
        ++(*piEventCount);
    }

    if ( pEvent && pEvent->eType == XLLM_EVENT_TEXT_DELTA ) {
        printf("stream: %s\n", pEvent->as.tTextDelta.sText ? pEvent->as.tTextDelta.sText : "");
    }

    return true;
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_adapter tAdapter;
    xllm_profile tProfile;
    xllm_create_options tCreate;
    xllm_turn tTurn;
    xllm_call_options tCall;
    int iEventCount = 0;
    int iStatus;
    const char *sText;

    memset(&tAdapter, 0, sizeof(tAdapter));
    memset(&tProfile, 0, sizeof(tProfile));
    memset(&tCreate, 0, sizeof(tCreate));

    xllm_runtime_create(NULL, &pRuntime);

    tAdapter.sName = "mock_echo";
    tAdapter.pfnChat = demo_mock_chat;
    if ( xllm_register_adapter(pRuntime, &tAdapter) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        return 1;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "mock";
    tProfile.sProvider = "mock";
    tProfile.sAdapter = "mock_echo";
    tProfile.tModels.tText.sModelId = "mock-text";
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 2;
    }

    tCreate.sInitialProfileId = "mock";
    tCreate.sSystemPrompt = "you are a mock";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 3;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "hello xllm") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 4;
    }

    xllm_call_options_init(&tCall);
    tCall.pfnOnEvent = demo_on_event;
    tCall.pUserData = &iEventCount;

    iStatus = xllm_send(pLlm, &tTurn, &tCall, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 5;
    }

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "echo: hello xllm") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 6;
    }
    if ( iEventCount < 4 ) {
        fprintf(stderr, "unexpected event count: %d\n", iEventCount);
        return 7;
    }

    printf("ok: %s\n", sText);

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
