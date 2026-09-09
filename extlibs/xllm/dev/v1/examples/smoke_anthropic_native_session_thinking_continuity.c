#include "xllm-session.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    uint32 uCallCount;
    bool bSawAssistantThinking;
    bool bSawAssistantText;
} demo_state;

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

static int demo_make_text_message_output(
    xllm_output_item *pOutput,
    const char *sText
)
{
    if ( !pOutput || !sText ) {
        return XRT_NET_ERROR;
    }

    memset(pOutput, 0, sizeof(*pOutput));
    pOutput->eKind = XLLM_OUTPUT_MESSAGE;
    pOutput->as.tMessage.iPartCount = 1u;
    pOutput->as.tMessage.pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pOutput->as.tMessage.pParts ) {
        return XRT_NET_ERROR;
    }

    pOutput->as.tMessage.pParts[0].eKind = XLLM_PART_TEXT;
    pOutput->as.tMessage.pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pOutput->as.tMessage.pParts[0].as.tSource.sMimeType = demo_dupstr("text/plain");
    pOutput->as.tMessage.pParts[0].as.tSource.as.sText = demo_dupstr(sText);
    if ( !pOutput->as.tMessage.pParts[0].as.tSource.sMimeType ||
         !pOutput->as.tMessage.pParts[0].as.tSource.as.sText ) {
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int demo_make_first_response(
    const xllm_profile *pProfile,
    xllm_response **ppResponse
)
{
    xllm_response *pResponse;
    xvalue tThinkingExtra;

    if ( !pProfile || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    *ppResponse = NULL;
    pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        return XRT_NET_ERROR;
    }

    pResponse->sId = demo_dupstr("mock_anthropic_session_turn_1");
    pResponse->sProvider = demo_dupstr(pProfile->sProvider ? pProfile->sProvider : "anthropic");
    pResponse->sProfileId = demo_dupstr(pProfile->sId);
    pResponse->sModel = demo_dupstr(pProfile->tModels.tText.sModelId ? pProfile->tModels.tText.sModelId : "claude-mock");
    pResponse->eStatus = XLLM_STATUS_COMPLETED;
    pResponse->sFinishReason = demo_dupstr("end_turn");
    pResponse->sVisibleText = demo_dupstr("First answer.");
    pResponse->iOutputCount = 2u;
    pResponse->pOutputs = (xllm_output_item *)xrtCalloc(2u, sizeof(xllm_output_item));
    if ( !pResponse->sId || !pResponse->sProvider || !pResponse->sProfileId ||
         !pResponse->sModel || !pResponse->sFinishReason || !pResponse->sVisibleText ||
         !pResponse->pOutputs ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    tThinkingExtra = xvoCreateTable();
    if ( !tThinkingExtra ||
         !xvoTableSetText(tThinkingExtra, (str)"anthropic_block_type", 0u, (str)"thinking", 8u, FALSE) ||
         !xvoTableSetText(tThinkingExtra, (str)"signature", 0u, (str)"sig_1", 5u, FALSE) ) {
        if ( tThinkingExtra ) {
            xvoUnref(tThinkingExtra);
        }
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    pResponse->pOutputs[0].eKind = XLLM_OUTPUT_THINKING;
    pResponse->pOutputs[0].as.tThinking.bVisible = true;
    pResponse->pOutputs[0].as.tThinking.sFormat = demo_dupstr("full");
    pResponse->pOutputs[0].as.tThinking.sText = demo_dupstr("Plan it first.");
    pResponse->pOutputs[0].as.tThinking.tVendorExtra = tThinkingExtra;
    if ( !pResponse->pOutputs[0].as.tThinking.sFormat ||
         !pResponse->pOutputs[0].as.tThinking.sText ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    if ( demo_make_text_message_output(&pResponse->pOutputs[1], "First answer.") != XRT_NET_OK ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    *ppResponse = pResponse;
    return XRT_NET_OK;
}

static int demo_make_second_response(
    const xllm_profile *pProfile,
    xllm_response **ppResponse
)
{
    xllm_response *pResponse;

    if ( !pProfile || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    *ppResponse = NULL;
    pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        return XRT_NET_ERROR;
    }

    pResponse->sId = demo_dupstr("mock_anthropic_session_turn_2");
    pResponse->sProvider = demo_dupstr(pProfile->sProvider ? pProfile->sProvider : "anthropic");
    pResponse->sProfileId = demo_dupstr(pProfile->sId);
    pResponse->sModel = demo_dupstr(pProfile->tModels.tText.sModelId ? pProfile->tModels.tText.sModelId : "claude-mock");
    pResponse->eStatus = XLLM_STATUS_COMPLETED;
    pResponse->sFinishReason = demo_dupstr("end_turn");
    pResponse->sVisibleText = demo_dupstr("Second answer.");
    pResponse->iOutputCount = 1u;
    pResponse->pOutputs = (xllm_output_item *)xrtCalloc(1u, sizeof(*pResponse->pOutputs));
    if ( !pResponse->sId || !pResponse->sProvider || !pResponse->sProfileId ||
         !pResponse->sModel || !pResponse->sFinishReason || !pResponse->sVisibleText ||
         !pResponse->pOutputs ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    if ( demo_make_text_message_output(&pResponse->pOutputs[0], "Second answer.") != XRT_NET_OK ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    *ppResponse = pResponse;
    return XRT_NET_OK;
}

static bool demo_request_has_assistant_thinking_history(
    const xllm_request *pRequest,
    bool *pbSawThinking,
    bool *pbSawText
)
{
    size_t i;

    if ( pbSawThinking ) {
        *pbSawThinking = false;
    }
    if ( pbSawText ) {
        *pbSawText = false;
    }
    if ( !pRequest ) {
        return false;
    }

    for ( i = 0; i < pRequest->iMessageCount; ++i ) {
        const xllm_message *pMessage = &pRequest->pMessages[i];
        bool bSawThinking = false;
        bool bSawText = false;
        size_t j;

        if ( pMessage->eRole != XLLM_ROLE_ASSISTANT ) {
            continue;
        }

        for ( j = 0; j < pMessage->iPartCount; ++j ) {
            const xllm_content_part *pPart = &pMessage->pParts[j];
            const char *sText;
            const char *sBlockType;
            const char *sSignature;

            if ( pPart->eKind != XLLM_PART_TEXT ||
                 pPart->as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
                continue;
            }

            sText = pPart->as.tSource.as.sText;
            if ( sText && strcmp(sText, "First answer.") == 0 ) {
                bSawText = true;
            }

            if ( !pPart->tVendorExtra || xvoType(pPart->tVendorExtra) != XVO_DT_TABLE ) {
                continue;
            }

            sBlockType = (const char *)xvoTableGetText(pPart->tVendorExtra, (str)"anthropic_block_type", 0u);
            sSignature = (const char *)xvoTableGetText(pPart->tVendorExtra, (str)"signature", 0u);
            if ( sBlockType && strcmp(sBlockType, "thinking") == 0 &&
                 sSignature && strcmp(sSignature, "sig_1") == 0 &&
                 sText && strcmp(sText, "Plan it first.") == 0 ) {
                bSawThinking = true;
            }
        }

        if ( bSawThinking && bSawText ) {
            if ( pbSawThinking ) {
                *pbSawThinking = true;
            }
            if ( pbSawText ) {
                *pbSawText = true;
            }
            return true;
        }
    }

    return false;
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
    demo_state *pState = (demo_state *)pCtx;

    (void)pOptions;
    (void)pError;

    if ( !pState || !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    ++pState->uCallCount;
    if ( pState->uCallCount == 1u ) {
        return demo_make_first_response(pProfile, ppResponse);
    }
    if ( pState->uCallCount == 2u ) {
        if ( !demo_request_has_assistant_thinking_history(
                pRequest,
                &pState->bSawAssistantThinking,
                &pState->bSawAssistantText
             ) ) {
            return XRT_NET_ERROR;
        }
        return demo_make_second_response(pProfile, ppResponse);
    }

    return XRT_NET_ERROR;
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_session *pSession = NULL;
    xllm_session_state *pSessionState = NULL;
    xllm_response *pResponse = NULL;
    xllm_adapter tAdapter;
    xllm_profile tProfile;
    xllm_session_options tSessionOptions;
    xllm_turn tTurn;
    demo_state tState;

    memset(&tAdapter, 0, sizeof(tAdapter));
    memset(&tProfile, 0, sizeof(tProfile));
    memset(&tState, 0, sizeof(tState));

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 1;
    }

    tAdapter.sName = "mock_anthropic_session";
    tAdapter.pCtx = &tState;
    tAdapter.pfnChat = demo_session_chat;
    if ( xllm_register_adapter(pRuntime, &tAdapter) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        xllm_runtime_destroy(pRuntime);
        return 2;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "mock-anthropic-session";
    tProfile.sProvider = "anthropic";
    tProfile.sAdapter = "mock_anthropic_session";
    tProfile.tModels.tText.sModelId = "claude-thinking-mock";
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        xllm_runtime_destroy(pRuntime);
        return 3;
    }

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "mock-anthropic-session";
    tSessionOptions.sSystemPrompt = "session continuity test";
    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "create session failed\n");
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "first turn") != XRT_NET_OK ) {
        fprintf(stderr, "add first turn failed\n");
        return 5;
    }
    if ( xllm_session_chat(pSession, &tTurn, NULL, &pResponse) != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "first session chat failed\n");
        return 6;
    }
    if ( !xllm_response_get_text(pResponse) ||
         strcmp(xllm_response_get_text(pResponse), "First answer.") != 0 ) {
        fprintf(stderr, "first response text mismatch\n");
        return 7;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    if ( xllm_session_export_state(pSession, &pSessionState) != XRT_NET_OK || !pSessionState ) {
        fprintf(stderr, "export session state failed\n");
        return 8;
    }
    xllm_session_destroy(pSession);
    pSession = NULL;
    if ( xllm_session_import_state(pRuntime, pSessionState, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "import session state failed\n");
        return 9;
    }
    xllm_session_state_free(pSessionState);
    pSessionState = NULL;

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "follow up") != XRT_NET_OK ) {
        fprintf(stderr, "add second turn failed\n");
        return 10;
    }
    if ( xllm_session_chat(pSession, &tTurn, NULL, &pResponse) != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "second session chat failed\n");
        return 11;
    }
    if ( !xllm_response_get_text(pResponse) ||
         strcmp(xllm_response_get_text(pResponse), "Second answer.") != 0 ) {
        fprintf(stderr, "second response text mismatch\n");
        return 12;
    }
    if ( !tState.bSawAssistantThinking || !tState.bSawAssistantText ) {
        fprintf(stderr, "assistant thinking continuity missing\n");
        return 13;
    }

    printf("ok: anthropic session thinking continuity preserved across export/import\n");

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_session_state_free(pSessionState);
    xllm_session_destroy(pSession);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
