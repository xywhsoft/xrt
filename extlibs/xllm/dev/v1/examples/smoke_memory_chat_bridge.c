#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xllm-memory-bridge.h"

static volatile uint32 g_demo_chat_gate_enabled = 0u;
static volatile uint32 g_demo_chat_gate_open = 0u;

static void demo_chat_gate_set(bool bEnabled, bool bOpen)
{
    __xrtAtomicStoreU32(&g_demo_chat_gate_open, bOpen ? 1u : 0u);
    __xrtAtomicStoreU32(&g_demo_chat_gate_enabled, bEnabled ? 1u : 0u);
}

static void demo_chat_gate_wait_if_needed(void)
{
    if ( __xrtAtomicLoadU32(&g_demo_chat_gate_enabled) == 0u ) {
        return;
    }

    while ( __xrtAtomicLoadU32(&g_demo_chat_gate_open) == 0u ) {
        xrtSleep(1u);
    }
}

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

static int write_text_file(const char *sPath, const char *sText)
{
    FILE *pFile = fopen(sPath, "wb");
    size_t iLen;

    if ( !pFile ) {
        return 1;
    }
    iLen = sText ? strlen(sText) : 0u;
    if ( iLen > 0u && fwrite(sText, 1u, iLen, pFile) != iLen ) {
        fclose(pFile);
        return 2;
    }
    fclose(pFile);
    return 0;
}

static int require_true(int condition, const char *sMessage)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static const char *demo_last_user_text(const xllm_request *pRequest)
{
    size_t i;

    if ( !pRequest ) {
        return NULL;
    }

    for ( i = pRequest->iMessageCount; i > 0u; --i ) {
        const xllm_message *pMessage = &pRequest->pMessages[i - 1u];
        size_t j;
        const char *sLastText = NULL;

        if ( pMessage->eRole != XLLM_ROLE_USER ) {
            continue;
        }

        for ( j = 0u; j < pMessage->iPartCount; ++j ) {
            const xllm_content_part *pPart = &pMessage->pParts[j];
            if ( pPart->eKind == XLLM_PART_TEXT &&
                 pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT &&
                 pPart->as.tSource.as.sText ) {
                sLastText = pPart->as.tSource.as.sText;
            }
        }

        if ( sLastText ) {
            return sLastText;
        }
    }

    return NULL;
}

static uint32 demo_count_user_messages(const xllm_request *pRequest)
{
    uint32 uCount = 0u;
    size_t i;

    if ( !pRequest ) {
        return 0u;
    }

    for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
        if ( pRequest->pMessages[i].eRole == XLLM_ROLE_USER ) {
            ++uCount;
        }
    }

    return uCount;
}

static bool demo_context_contains_text(const xllm_request *pRequest, const char *sNeedle)
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

static int32 demo_bridge_chat(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    const char *sLastUser;
    bool bHasCitrusContext;
    uint32 uUserCount;
    char sText[256];
    xllm_response *pResponse;

    (void)pCtx;
    (void)pOptions;
    (void)pError;

    if ( !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    demo_chat_gate_wait_if_needed();

    sLastUser = demo_last_user_text(pRequest);
    if ( !sLastUser ) {
        sLastUser = "(none)";
    }
    bHasCitrusContext = demo_context_contains_text(pRequest, "citrus bridge context");
    uUserCount = demo_count_user_messages(pRequest);
    if ( snprintf(
            sText,
            sizeof(sText),
            "context=%s users=%u last=%s",
            bHasCitrusContext ? "yes" : "no",
            (unsigned)uUserCount,
            sLastUser
         ) <= 0 ) {
        return XRT_NET_ERROR;
    }

    pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        return XRT_NET_ERROR;
    }

    pResponse->sId = demo_dupstr("mock-memory-bridge");
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

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_adapter tAdapter;
    xllm_profile tProfile;
    xllm_create_options tCreateOptions;
    xllm_session_options tSessionOptions;
    xllm *pLlm = NULL;
    xllm_session *pSession = NULL;
    xllm_session *pAsyncSession = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_ingest_directory_options tIngestOptions;
    xllm_memory_ingest_directory_result tIngestResult;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tListResult;
    xllm_memory_chat_bridge_options tBridgeOptions;
    xllm_turn tStatelessTurn;
    xllm_turn tSessionTurn1;
    xllm_turn tSessionTurn2;
    xllm_turn tAsyncStatelessTurn;
    xllm_turn tAsyncSessionTurn1;
    xllm_turn tAsyncSessionTurn2;
    xllm_response *pResponse = NULL;
    xfuture *pFuture = NULL;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const xllm_memory_record_info *pRecord;
    const char *sText;
    char sAsyncConversationId1[32];
    char sAsyncConversationId2[32];
    char sAsyncTurnId1[16];
    char sAsyncTurnId2[16];
    int iFoundAsyncTurn1 = 0;
    int iFoundAsyncTurn2 = 0;
    const char *sRootDir = "build\\smoke_memory_chat_bridge_tmp";
    const char *sCitrusDocPath = "build\\smoke_memory_chat_bridge_tmp\\citrus.txt";
    const char *sBerryDocPath = "build\\smoke_memory_chat_bridge_tmp\\berry.txt";
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    memset(&tAdapter, 0, sizeof(tAdapter));
    xllm_profile_init(&tProfile);
    memset(&tCreateOptions, 0, sizeof(tCreateOptions));
    xllm_session_options_init(&tSessionOptions);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_directory_options_init(&tIngestOptions);
    xllm_memory_ingest_directory_result_init(&tIngestResult);
    xllm_memory_list_options_init(&tListOptions);
    memset(&tListResult, 0, sizeof(tListResult));
    xllm_memory_chat_bridge_options_init(&tBridgeOptions);
    xllm_turn_init(&tStatelessTurn);
    xllm_turn_init(&tSessionTurn1);
    xllm_turn_init(&tSessionTurn2);
    xllm_turn_init(&tAsyncStatelessTurn);
    xllm_turn_init(&tAsyncSessionTurn1);
    xllm_turn_init(&tAsyncSessionTurn2);
    xllm_error_init(&tError);
    demo_chat_gate_set(false, false);

    if ( !xrtDirCreateAll((str)sRootDir) ) {
        fprintf(stderr, "failed to create temp root directory\n");
        goto cleanup;
    }
    if ( write_text_file(
            sCitrusDocPath,
            "citrus bridge context should be retrieved and attached before chat.\n"
         ) != 0 ||
         write_text_file(
            sBerryDocPath,
            "berry context is unrelated to the bridge smoke.\n"
         ) != 0 ) {
        fprintf(stderr, "failed to write temp bridge documents\n");
        goto cleanup;
    }

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "runtime create failed\n");
        goto cleanup;
    }

    tAdapter.sName = "mock_memory_bridge";
    tAdapter.pfnChat = demo_bridge_chat;
    if ( xllm_register_adapter(pRuntime, &tAdapter) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        goto cleanup;
    }

    tProfile.sId = "mock-memory-bridge";
    tProfile.sProvider = "mock";
    tProfile.sAdapter = "mock_memory_bridge";
    tProfile.tModels.tText.sModelId = "mock-text";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        goto cleanup;
    }

    tCreateOptions.sInitialProfileId = "mock-memory-bridge";
    pLlm = xllm_create(pRuntime, &tCreateOptions);
    if ( !pLlm ) {
        fprintf(stderr, "llm create failed\n");
        goto cleanup;
    }

    tSessionOptions.sProfileId = "mock-memory-bridge";
    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "session create failed\n");
        goto cleanup;
    }

    tEmbedderOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_HASH;
    tEmbedderOptions.uHashDimensions = 32u;
    iStatus = xllm_memory_make_builtin_embedder(&tEmbedderOptions, &tMemoryOptions.tEmbedder, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "builtin embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-memory-chat-bridge";
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory create failed\n");
        goto cleanup;
    }

    tIngestOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngestOptions.sPath = sRootDir;
    tIngestOptions.sRecordIdPrefix = "bridge-doc";
    iStatus = xllm_memory_ingest_directory(pMemory, &tIngestOptions, &tIngestResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory ingest directory failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    if ( xllm_turn_add_user_text(&tStatelessTurn, "Earlier user text about berries.") != XRT_NET_OK ||
         xllm_turn_add_user_text(&tStatelessTurn, "Need citrus bridge context before stateless chat.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to build stateless turn\n");
        goto cleanup;
    }

    xllm_memory_chat_bridge_options_init(&tBridgeOptions);
    tBridgeOptions.tSearch.tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tBridgeOptions.tSearch.tSearchOptions.uMaxHits = 1u;
    tBridgeOptions.tSearch.tContextOptions.sLabel = "Bridge context:";
    tBridgeOptions.tSearch.tContextOptions.uMaxHits = 1u;
    iStatus = xllm_memory_bridge_send_ex(
        pMemory,
        pLlm,
        &tStatelessTurn,
        NULL,
        &tBridgeOptions,
        &pResponse,
        &tError
    );
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "memory bridge send failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    sText = xllm_response_get_text(pResponse);
    if ( require_true(sText != NULL, "stateless bridge response text missing") != 0 ||
         require_true(strstr(sText, "context=yes") != NULL, "stateless bridge should inject retrieved context") != 0 ||
         require_true(strstr(sText, "users=1") != NULL, "stateless bridge should preserve the single current user turn") != 0 ||
         require_true(strstr(sText, "Need citrus bridge context before stateless chat.") != NULL, "stateless bridge last user mismatch") != 0 ||
         require_true(tStatelessTurn.iContextBlockCount == 0u, "stateless source turn should not be mutated") != 0 ) {
        goto cleanup;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;

    if ( xllm_turn_add_user_text(&tSessionTurn1, "Need citrus bridge context before session chat.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to build first session turn\n");
        goto cleanup;
    }

    xllm_memory_chat_bridge_options_init(&tBridgeOptions);
    tBridgeOptions.tSearch.tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tBridgeOptions.tSearch.tSearchOptions.uMaxHits = 1u;
    tBridgeOptions.tSearch.tContextOptions.sLabel = "Session bridge context:";
    tBridgeOptions.tSearch.tContextOptions.uMaxHits = 1u;
    tBridgeOptions.bIngestAfterChat = true;
    tBridgeOptions.tIngest.sConversationId = "bridge-conv";
    tBridgeOptions.tIngest.sTurnId = "turn-001";
    tBridgeOptions.tIngest.bUseStableIdentity = true;
    iStatus = xllm_memory_bridge_session_chat_ex(
        pMemory,
        pSession,
        &tSessionTurn1,
        NULL,
        &tBridgeOptions,
        &pResponse,
        &tError
    );
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "memory bridge session first chat failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    sText = xllm_response_get_text(pResponse);
    if ( require_true(sText != NULL, "first session bridge response text missing") != 0 ||
         require_true(strstr(sText, "context=yes") != NULL, "first session bridge should inject retrieved context") != 0 ||
         require_true(strstr(sText, "users=1") != NULL, "first session bridge user count mismatch") != 0 ||
         require_true(tSessionTurn1.iContextBlockCount == 0u, "first session source turn should not be mutated") != 0 ) {
        goto cleanup;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;

    if ( xllm_turn_add_user_text(&tSessionTurn2, "Second follow-up after memory bridge with citrus context.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to build second session turn\n");
        goto cleanup;
    }

    xllm_memory_chat_bridge_options_init(&tBridgeOptions);
    tBridgeOptions.tSearch.tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tBridgeOptions.tSearch.tSearchOptions.uMaxHits = 1u;
    tBridgeOptions.tSearch.tContextOptions.sLabel = "Session bridge context:";
    tBridgeOptions.tSearch.tContextOptions.uMaxHits = 1u;
    tBridgeOptions.bIngestAfterChat = true;
    tBridgeOptions.tIngest.sConversationId = "bridge-conv";
    tBridgeOptions.tIngest.sTurnId = "turn-002";
    tBridgeOptions.tIngest.bUseStableIdentity = true;
    iStatus = xllm_memory_bridge_session_chat_ex(
        pMemory,
        pSession,
        &tSessionTurn2,
        NULL,
        &tBridgeOptions,
        &pResponse,
        &tError
    );
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "memory bridge session second chat failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    sText = xllm_response_get_text(pResponse);
    if ( require_true(sText != NULL, "second session bridge response text missing") != 0 ||
         require_true(strstr(sText, "context=yes") != NULL, "second session bridge should inject retrieved context") != 0 ||
         require_true(strstr(sText, "users=2") != NULL, "second session bridge should preserve session history") != 0 ||
         require_true(tSessionTurn2.iContextBlockCount == 0u, "second session source turn should not be mutated") != 0 ) {
        goto cleanup;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sConversationId = "bridge-conv";
    tListOptions.uMaxItems = 8u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tListResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory list records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tListResult.iRecordCount == 2u, "expected two ingested conversation memory records") != 0 ) {
        goto cleanup;
    }
    pRecord = &tListResult.pRecords[0];
    if ( require_true(pRecord->tMetadata && xvoType(pRecord->tMetadata) == XVO_DT_TABLE, "bridge metadata missing") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"memory_type", 0u), "conversation.turn_response.v1") == 0, "bridge memory_type metadata mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"extraction_policy", 0u), "turn_response") == 0, "bridge extraction_policy metadata mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"conversation_id", 0u), "bridge-conv") == 0, "bridge conversation_id metadata mismatch") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"stable_identity", 0u) == 1, "bridge stable_identity metadata mismatch") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tListResult);
    memset(&tListResult, 0, sizeof(tListResult));

    tSessionOptions.sProfileId = "mock-memory-bridge";
    if ( xllm_session_create(pRuntime, &tSessionOptions, &pAsyncSession) != XRT_NET_OK || !pAsyncSession ) {
        fprintf(stderr, "async session create failed\n");
        goto cleanup;
    }

    if ( xllm_turn_add_user_text(&tAsyncStatelessTurn, "Need citrus bridge context before async stateless chat.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to build async stateless turn\n");
        goto cleanup;
    }

    xllm_memory_chat_bridge_options_init(&tBridgeOptions);
    tBridgeOptions.tSearch.tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tBridgeOptions.tSearch.tSearchOptions.uMaxHits = 1u;
    tBridgeOptions.tSearch.tContextOptions.sLabel = "Async bridge context:";
    tBridgeOptions.tSearch.tContextOptions.uMaxHits = 1u;
    pFuture = xllm_memory_bridge_send_async_thread(
        pMemory,
        pLlm,
        &tAsyncStatelessTurn,
        NULL,
        &tBridgeOptions
    );
    if ( !pFuture ) {
        fprintf(stderr, "memory bridge async send future create failed\n");
        goto cleanup;
    }
    pResponse = (xllm_response *)xFutureWaitValue(pFuture);
    if ( xFutureStatus(pFuture) != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "memory bridge async send failed: %d\n", (int)xFutureStatus(pFuture));
        goto cleanup;
    }
    sText = xllm_response_get_text(pResponse);
    if ( require_true(sText != NULL, "async stateless bridge response text missing") != 0 ||
         require_true(strstr(sText, "context=yes") != NULL, "async stateless bridge should inject retrieved context") != 0 ||
         require_true(strstr(sText, "users=1") != NULL, "async stateless bridge should preserve the single current user turn") != 0 ||
         require_true(tAsyncStatelessTurn.iContextBlockCount == 0u, "async stateless source turn should not be mutated") != 0 ) {
        goto cleanup;
    }
    xFutureRelease(pFuture);
    pFuture = NULL;
    xllm_response_free(pResponse);
    pResponse = NULL;

    if ( xllm_turn_add_user_text(&tAsyncSessionTurn1, "Need citrus bridge context before async session chat.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to build first async session turn\n");
        goto cleanup;
    }

    strcpy(sAsyncConversationId1, "bridge-async-conv");
    strcpy(sAsyncTurnId1, "turn-001");
    xllm_memory_chat_bridge_options_init(&tBridgeOptions);
    tBridgeOptions.tSearch.tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tBridgeOptions.tSearch.tSearchOptions.uMaxHits = 1u;
    tBridgeOptions.tSearch.tContextOptions.sLabel = "Async session bridge context:";
    tBridgeOptions.tSearch.tContextOptions.uMaxHits = 1u;
    tBridgeOptions.bIngestAfterChat = true;
    tBridgeOptions.tIngest.sConversationId = sAsyncConversationId1;
    tBridgeOptions.tIngest.sTurnId = sAsyncTurnId1;
    tBridgeOptions.tIngest.bUseStableIdentity = true;
    demo_chat_gate_set(true, false);
    pFuture = xllm_memory_bridge_session_chat_async_thread(
        pMemory,
        pAsyncSession,
        &tAsyncSessionTurn1,
        NULL,
        &tBridgeOptions
    );
    if ( !pFuture ) {
        fprintf(stderr, "memory bridge async session first future create failed\n");
        goto cleanup;
    }
    strcpy(sAsyncConversationId1, "mutated-conv-1");
    strcpy(sAsyncTurnId1, "mut-turn-1");
    demo_chat_gate_set(true, true);
    pResponse = (xllm_response *)xFutureWaitValue(pFuture);
    if ( xFutureStatus(pFuture) != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "memory bridge async session first chat failed: %d\n", (int)xFutureStatus(pFuture));
        goto cleanup;
    }
    sText = xllm_response_get_text(pResponse);
    if ( require_true(sText != NULL, "first async session bridge response text missing") != 0 ||
         require_true(strstr(sText, "context=yes") != NULL, "first async session bridge should inject retrieved context") != 0 ||
         require_true(strstr(sText, "users=1") != NULL, "first async session bridge user count mismatch") != 0 ||
         require_true(tAsyncSessionTurn1.iContextBlockCount == 0u, "first async session source turn should not be mutated") != 0 ) {
        goto cleanup;
    }
    xFutureRelease(pFuture);
    pFuture = NULL;
    xllm_response_free(pResponse);
    pResponse = NULL;
    demo_chat_gate_set(false, false);

    if ( xllm_turn_add_user_text(&tAsyncSessionTurn2, "Second async follow-up after memory bridge with citrus context.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to build second async session turn\n");
        goto cleanup;
    }

    strcpy(sAsyncConversationId2, "bridge-async-conv");
    strcpy(sAsyncTurnId2, "turn-002");
    xllm_memory_chat_bridge_options_init(&tBridgeOptions);
    tBridgeOptions.tSearch.tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tBridgeOptions.tSearch.tSearchOptions.uMaxHits = 1u;
    tBridgeOptions.tSearch.tContextOptions.sLabel = "Async session bridge context:";
    tBridgeOptions.tSearch.tContextOptions.uMaxHits = 1u;
    tBridgeOptions.bIngestAfterChat = true;
    tBridgeOptions.tIngest.sConversationId = sAsyncConversationId2;
    tBridgeOptions.tIngest.sTurnId = sAsyncTurnId2;
    tBridgeOptions.tIngest.bUseStableIdentity = true;
    demo_chat_gate_set(true, false);
    pFuture = xllm_memory_bridge_session_chat_async_thread(
        pMemory,
        pAsyncSession,
        &tAsyncSessionTurn2,
        NULL,
        &tBridgeOptions
    );
    if ( !pFuture ) {
        fprintf(stderr, "memory bridge async session second future create failed\n");
        goto cleanup;
    }
    strcpy(sAsyncConversationId2, "mutated-conv-2");
    strcpy(sAsyncTurnId2, "mut-turn-2");
    demo_chat_gate_set(true, true);
    pResponse = (xllm_response *)xFutureWaitValue(pFuture);
    if ( xFutureStatus(pFuture) != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "memory bridge async session second chat failed: %d\n", (int)xFutureStatus(pFuture));
        goto cleanup;
    }
    sText = xllm_response_get_text(pResponse);
    if ( require_true(sText != NULL, "second async session bridge response text missing") != 0 ||
         require_true(strstr(sText, "context=yes") != NULL, "second async session bridge should inject retrieved context") != 0 ||
         require_true(strstr(sText, "users=2") != NULL, "second async session bridge should preserve session history") != 0 ||
         require_true(tAsyncSessionTurn2.iContextBlockCount == 0u, "second async session source turn should not be mutated") != 0 ) {
        goto cleanup;
    }
    xFutureRelease(pFuture);
    pFuture = NULL;
    xllm_response_free(pResponse);
    pResponse = NULL;
    demo_chat_gate_set(false, false);

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sConversationId = "bridge-async-conv";
    tListOptions.uMaxItems = 8u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tListResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory list async records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tListResult.iRecordCount == 2u, "expected two ingested async conversation memory records") != 0 ) {
        goto cleanup;
    }
    for ( iStatus = 0; (size_t)iStatus < tListResult.iRecordCount; ++iStatus ) {
        const xllm_memory_record_info *pAsyncRecord = &tListResult.pRecords[iStatus];
        const char *sConversationId = NULL;
        const char *sTurnId = NULL;

        if ( require_true(pAsyncRecord->tMetadata && xvoType(pAsyncRecord->tMetadata) == XVO_DT_TABLE, "async bridge metadata missing") != 0 ) {
            goto cleanup;
        }

        sConversationId = (const char *)xvoTableGetText(pAsyncRecord->tMetadata, (str)"conversation_id", 0u);
        sTurnId = (const char *)xvoTableGetText(pAsyncRecord->tMetadata, (str)"turn_id", 0u);
        if ( require_true(sConversationId && strcmp(sConversationId, "bridge-async-conv") == 0, "async bridge conversation_id metadata mismatch") != 0 ||
             require_true((int)xvoTableGetInt(pAsyncRecord->tMetadata, (str)"stable_identity", 0u) == 1, "async bridge stable_identity metadata mismatch") != 0 ) {
            goto cleanup;
        }

        if ( sTurnId && strcmp(sTurnId, "turn-001") == 0 ) {
            iFoundAsyncTurn1 = 1;
        } else if ( sTurnId && strcmp(sTurnId, "turn-002") == 0 ) {
            iFoundAsyncTurn2 = 1;
        }
    }
    if ( require_true(iFoundAsyncTurn1, "async bridge should preserve original first turn_id after caller mutation") != 0 ||
         require_true(iFoundAsyncTurn2, "async bridge should preserve original second turn_id after caller mutation") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_chat_bridge ok\n");
    iRc = 0;

cleanup:
    demo_chat_gate_set(false, true);
    xllm_memory_record_list_result_reset(&tListResult);
    if ( pFuture ) {
        (void)xFutureWait(pFuture);
        xFutureRelease(pFuture);
    }
    if ( pResponse ) {
        xllm_response_free(pResponse);
    }
    xllm_turn_reset(&tAsyncSessionTurn2);
    xllm_turn_reset(&tAsyncSessionTurn1);
    xllm_turn_reset(&tAsyncStatelessTurn);
    xllm_turn_reset(&tSessionTurn2);
    xllm_turn_reset(&tSessionTurn1);
    xllm_turn_reset(&tStatelessTurn);
    xllm_memory_ingest_directory_result_reset(&tIngestResult);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pAsyncSession ) {
        xllm_session_destroy(pAsyncSession);
    }
    if ( pSession ) {
        xllm_session_destroy(pSession);
    }
    if ( pLlm ) {
        xllm_destroy(pLlm);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
