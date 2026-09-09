#include <stdio.h>
#include <string.h>

#include "xllm-session.h"
#include "xllm-memory.h"

typedef struct {
    int iToolCallCount;
    int iToolExecCount;
    int iFinalCount;
    bool bSawMemoryContext;
} agent_demo_state;

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

static int require_true(int bCondition, const char *sMessage)
{
    if ( !bCondition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static bool request_context_contains(const xllm_request *pRequest, const char *sNeedle)
{
    size_t i;

    if ( !pRequest || !sNeedle || !sNeedle[0] ) {
        return false;
    }

    for ( i = 0u; i < pRequest->iContextBlockCount; ++i ) {
        const xllm_context_block *pBlock = &pRequest->pContextBlocks[i];
        size_t j;

        for ( j = 0u; j < pBlock->iMessageCount; ++j ) {
            const xllm_message *pMessage = &pBlock->pMessages[j];
            size_t k;

            for ( k = 0u; k < pMessage->iPartCount; ++k ) {
                const xllm_content_part *pPart = &pMessage->pParts[k];
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
    }

    return false;
}

static const char *find_last_tool_text(const xllm_request *pRequest)
{
    size_t i;

    if ( !pRequest ) {
        return NULL;
    }

    for ( i = pRequest->iMessageCount; i > 0u; --i ) {
        const xllm_message *pMessage = &pRequest->pMessages[i - 1u];
        size_t j;

        if ( pMessage->eRole != XLLM_ROLE_TOOL ) {
            continue;
        }
        for ( j = 0u; j < pMessage->iPartCount; ++j ) {
            const xllm_content_part *pPart = &pMessage->pParts[j];
            if ( pPart->eKind == XLLM_PART_TEXT &&
                 pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT &&
                 pPart->as.tSource.as.sText ) {
                return pPart->as.tSource.as.sText;
            }
        }
    }

    return NULL;
}

static int make_text_response(
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

    pResponse->sId = demo_dupstr(sId);
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

static int make_tool_response(const xllm_profile *pProfile, xllm_response **ppResponse)
{
    xllm_response *pResponse;

    if ( !pProfile || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        return XRT_NET_ERROR;
    }

    pResponse->sId = demo_dupstr("mock-agent-tool-call");
    pResponse->sProvider = demo_dupstr(pProfile->sProvider ? pProfile->sProvider : "mock");
    pResponse->sProfileId = demo_dupstr(pProfile->sId);
    pResponse->sModel = demo_dupstr(pProfile->tModels.tText.sModelId ? pProfile->tModels.tText.sModelId : "mock-text");
    pResponse->eStatus = XLLM_STATUS_TOOL_CALL_REQUIRED;
    pResponse->sFinishReason = demo_dupstr("tool_calls");
    pResponse->iOutputCount = 1u;
    pResponse->pOutputs = (xllm_output_item *)xrtCalloc(1u, sizeof(xllm_output_item));
    if ( !pResponse->pOutputs ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    pResponse->pOutputs[0].eKind = XLLM_OUTPUT_TOOL_CALL;
    pResponse->pOutputs[0].as.tToolCall.sCallId = demo_dupstr("call-inspect-workspace-1");
    pResponse->pOutputs[0].as.tToolCall.sToolId = demo_dupstr("xwork.workspace.inspect");
    pResponse->pOutputs[0].as.tToolCall.sToolName = demo_dupstr("inspect_workspace");
    pResponse->pOutputs[0].as.tToolCall.sArgumentsJson = demo_dupstr("{\"path\":\".\"}");
    if ( !pResponse->pOutputs[0].as.tToolCall.sCallId ||
         !pResponse->pOutputs[0].as.tToolCall.sToolId ||
         !pResponse->pOutputs[0].as.tToolCall.sToolName ||
         !pResponse->pOutputs[0].as.tToolCall.sArgumentsJson ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    *ppResponse = pResponse;
    return XRT_NET_OK;
}

static int32 mock_agent_chat(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    agent_demo_state *pState = (agent_demo_state *)pCtx;
    const char *sToolText;
    char sText[256];

    (void)pOptions;
    (void)pError;

    if ( !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }
    if ( pState && request_context_contains(pRequest, "agent loop should inspect workspace before editing") ) {
        pState->bSawMemoryContext = true;
    }

    sToolText = find_last_tool_text(pRequest);
    if ( !sToolText ) {
        if ( pState ) {
            ++pState->iToolCallCount;
        }
        return make_tool_response(pProfile, ppResponse);
    }

    if ( snprintf(
            sText,
            sizeof(sText),
            "agent final: memory=%s tool=%s",
            (pState && pState->bSawMemoryContext) ? "yes" : "no",
            sToolText
         ) <= 0 ) {
        return XRT_NET_ERROR;
    }
    if ( pState ) {
        ++pState->iFinalCount;
    }
    return make_text_response(pProfile, "mock-agent-final", sText, ppResponse);
}

static int32 xwork_like_execute(
    void *pCtx,
    const xllm_tool_exec_request *pRequest,
    xllm_tool_exec_result *pResult,
    xllm_error *pError
)
{
    agent_demo_state *pState = (agent_demo_state *)pCtx;

    (void)pError;

    if ( !pRequest || !pResult || !pRequest->sToolId ||
         strcmp(pRequest->sToolId, "xwork.workspace.inspect") != 0 ) {
        return XRT_NET_ERROR;
    }

    pResult->pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pResult->pParts ) {
        return XRT_NET_ERROR;
    }
    pResult->iPartCount = 1u;
    pResult->pParts[0].eKind = XLLM_PART_TEXT;
    pResult->pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pResult->pParts[0].as.tSource.sMimeType = demo_dupstr("text/plain");
    pResult->pParts[0].as.tSource.as.sText = demo_dupstr("workspace_status=clean; executor=xwork-like");
    if ( !pResult->pParts[0].as.tSource.sMimeType ||
         !pResult->pParts[0].as.tSource.as.sText ) {
        xllm_tool_exec_result_free(pResult);
        return XRT_NET_ERROR;
    }
    if ( pState ) {
        ++pState->iToolExecCount;
    }
    return XRT_NET_OK;
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_session *pSession = NULL;
    xllm_memory *pMemory = NULL;
    xllm_response *pResponse = NULL;
    xllm_adapter tAdapter;
    xllm_profile tProfile;
    xllm_session_options tSessionOptions;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_options tIngestOptions;
    xllm_memory_turn_search_apply_options tMemoryApplyOptions;
    xllm_memory_ingest_turn_response_options tWriteOptions;
    xllm_memory_search_options tVerifySearchOptions;
    xllm_memory_search_result tVerifySearchResult;
    xllm_turn tTurn;
    xllm_tool_def tTool;
    xllm_tool_executor tExecutor;
    xllm_error tError;
    agent_demo_state tState;
    const char *sText;
    int iStatus;
    int iRc = 1;

    memset(&tAdapter, 0, sizeof(tAdapter));
    memset(&tProfile, 0, sizeof(tProfile));
    memset(&tState, 0, sizeof(tState));
    xllm_session_options_init(&tSessionOptions);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_options_init(&tIngestOptions);
    xllm_memory_turn_search_apply_options_init(&tMemoryApplyOptions);
    xllm_memory_ingest_turn_response_options_init(&tWriteOptions);
    xllm_memory_search_options_init(&tVerifySearchOptions);
    memset(&tVerifySearchResult, 0, sizeof(tVerifySearchResult));
    xllm_turn_init(&tTurn);
    memset(&tTool, 0, sizeof(tTool));
    memset(&tExecutor, 0, sizeof(tExecutor));
    xllm_error_init(&tError);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "runtime create failed\n");
        goto cleanup;
    }

    tAdapter.sName = "mock_agent_loop";
    tAdapter.pCtx = &tState;
    tAdapter.pfnChat = mock_agent_chat;
    if ( xllm_register_adapter(pRuntime, &tAdapter) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        goto cleanup;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "mock-agent-loop";
    tProfile.sProvider = "mock";
    tProfile.sAdapter = "mock_agent_loop";
    tProfile.tModels.tText.sModelId = "mock-text";
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_TOOL_CALL_OUT |
        XLLM_CAP_TOOL_RESULT_IN;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "example-agent-loop";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.uDefaultMaxHits = 3u;
    if ( xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory) != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed\n");
        goto cleanup;
    }

    tIngestOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tIngestOptions.sRecordId = "agent-loop-policy";
    tIngestOptions.sTitle = "Agent loop policy";
    tIngestOptions.sSourceUri = "memory://agent-loop/policy";
    tIngestOptions.sText = "project memory: agent loop should inspect workspace before editing and use an xwork-like executor for tools.";
    tIngestOptions.bReplaceExisting = true;
    tIngestOptions.uChunkChars = 1024u;
    if ( xllm_memory_ingest_text(pMemory, &tIngestOptions, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "memory ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tSessionOptions.sProfileId = "mock-agent-loop";
    tSessionOptions.sSystemPrompt = "You are an agent loop integration demo.";
    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "session create failed\n");
        goto cleanup;
    }

    tExecutor.pCtx = &tState;
    tExecutor.pfnExecute = xwork_like_execute;
    if ( xllm_session_set_tool_executor(pSession, &tExecutor) != XRT_NET_OK ) {
        fprintf(stderr, "set session tool executor failed\n");
        goto cleanup;
    }

    if ( xllm_turn_add_user_text(&tTurn, "Before editing, inspect workspace and answer with the result.") != XRT_NET_OK ) {
        fprintf(stderr, "add user text failed\n");
        goto cleanup;
    }
    tTool.sToolId = "xwork.workspace.inspect";
    tTool.sWireName = "inspect_workspace";
    tTool.sDescription = "Inspect workspace state using the host xwork-like executor.";
    if ( xllm_turn_add_tool(&tTurn, &tTool) != XRT_NET_OK ) {
        fprintf(stderr, "add tool failed\n");
        goto cleanup;
    }

    tMemoryApplyOptions.tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tMemoryApplyOptions.tSearchOptions.uMaxHits = 1u;
    tMemoryApplyOptions.tContextOptions.sLabel = "Agent memory:";
    tMemoryApplyOptions.tContextOptions.uMaxHits = 1u;
    tMemoryApplyOptions.tContextOptions.uMaxCharsPerHit = 512u;
    iStatus = xllm_memory_search_and_apply_from_turn_to_turn(pMemory, &tTurn, &tTurn, &tMemoryApplyOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory search/apply failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    iStatus = xllm_session_chat_ex(pSession, &tTurn, NULL, &pResponse, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "session chat failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    sText = xllm_response_get_text(pResponse);
    if ( require_true(sText != NULL, "final response text missing") != 0 ||
         require_true(strstr(sText, "memory=yes") != NULL, "memory context was not observed by provider") != 0 ||
         require_true(strstr(sText, "workspace_status=clean") != NULL, "tool result missing from final response") != 0 ||
         require_true(tState.iToolCallCount == 1, "expected one model tool call") != 0 ||
         require_true(tState.iToolExecCount == 1, "expected one xwork-like tool execution") != 0 ||
         require_true(tState.iFinalCount == 1, "expected one final model response") != 0 ) {
        goto cleanup;
    }

    tWriteOptions.eExtractionPolicy = XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY;
    tWriteOptions.pTurn = &tTurn;
    tWriteOptions.pResponse = pResponse;
    tWriteOptions.sRecordId = "agent-loop-summary";
    tWriteOptions.sConversationId = "agent-loop-demo";
    tWriteOptions.sTurnId = "turn-001";
    tWriteOptions.sSummaryText = "summary: Agent inspected workspace through xwork-like executor and produced a final response.";
    tWriteOptions.bReplaceExisting = true;
    tWriteOptions.uChunkChars = 1024u;
    if ( xllm_memory_ingest_turn_response(pMemory, &tWriteOptions, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "post-chat memory write failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tVerifySearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tVerifySearchOptions.sQuery = "inspected workspace through xwork-like executor";
    tVerifySearchOptions.uMaxHits = 1u;
    if ( xllm_memory_search(pMemory, &tVerifySearchOptions, &tVerifySearchResult, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "summary verify search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tVerifySearchResult.iHitCount == 1u, "expected explicit post-chat memory write") != 0 ||
         require_true(strcmp(tVerifySearchResult.pHits[0].sRecordId, "agent-loop-summary") == 0, "post-chat memory hit mismatch") != 0 ) {
        goto cleanup;
    }

    printf("agent_loop example ok\n");
    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tVerifySearchResult);
    if ( pResponse ) {
        xllm_response_free(pResponse);
    }
    xllm_turn_reset(&tTurn);
    if ( pSession ) {
        xllm_session_destroy(pSession);
    }
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
