#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

XLLM_API void xllm_turn_init(xllm_turn *pTurn);
XLLM_API void xllm_turn_reset(xllm_turn *pTurn);
XLLM_API int xllm_turn_set_system_prompt(xllm_turn *pTurn, const char *sText);
XLLM_API int xllm_turn_add_user_text(xllm_turn *pTurn, const char *sText);

static int require_true(int condition, const char *sMessage)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static const xllm_memory_record_info *find_record(
    const xllm_memory_record_list_result *pRecords,
    const char *sRecordId
)
{
    size_t i;

    if ( !pRecords || !sRecordId ) {
        return NULL;
    }

    for ( i = 0u; i < pRecords->iRecordCount; ++i ) {
        if ( pRecords->pRecords[i].sRecordId &&
             strcmp(pRecords->pRecords[i].sRecordId, sRecordId) == 0 ) {
            return &pRecords->pRecords[i];
        }
    }

    return NULL;
}

static const xllm_memory_chunk_info *find_chunk(
    const xllm_memory_chunk_list_result *pChunks,
    const char *sRecordId
)
{
    size_t i;

    if ( !pChunks || !sRecordId ) {
        return NULL;
    }

    for ( i = 0u; i < pChunks->iChunkCount; ++i ) {
        if ( pChunks->pChunks[i].sRecordId &&
             strcmp(pChunks->pChunks[i].sRecordId, sRecordId) == 0 ) {
            return &pChunks->pChunks[i];
        }
    }

    return NULL;
}

static int record_contains_text(
    const xllm_memory_chunk_list_result *pChunks,
    const char *sRecordId,
    const char *sNeedle
)
{
    size_t i;

    if ( !pChunks || !sRecordId || !sNeedle ) {
        return 0;
    }

    for ( i = 0u; i < pChunks->iChunkCount; ++i ) {
        const xllm_memory_chunk_info *pChunk = &pChunks->pChunks[i];
        if ( pChunk->sRecordId &&
             strcmp(pChunk->sRecordId, sRecordId) == 0 &&
             pChunk->sText &&
             strstr(pChunk->sText, sNeedle) != NULL ) {
            return 1;
        }
    }

    return 0;
}

static int init_mock_response(xllm_response *pResponse)
{
    xllm_output_item *pOutputs;
    xllm_content_part *pParts;

    if ( !pResponse ) {
        return 1;
    }

    memset(pResponse, 0, sizeof(*pResponse));
    pOutputs = (xllm_output_item *)xrtCalloc(3u, sizeof(xllm_output_item));
    if ( !pOutputs ) {
        return 2;
    }
    pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pParts ) {
        xrtFree(pOutputs);
        return 3;
    }

    pResponse->eStatus = XLLM_STATUS_COMPLETED;
    pResponse->sVisibleText = "Attach the retrieved account context as a pinned block.";
    pResponse->pOutputs = pOutputs;
    pResponse->iOutputCount = 3u;

    pOutputs[0].eKind = XLLM_OUTPUT_THINKING;
    pOutputs[0].as.tThinking.bVisible = false;
    pOutputs[0].as.tThinking.sText = "private reasoning should stay optional";

    pOutputs[1].eKind = XLLM_OUTPUT_TOOL_CALL;
    pOutputs[1].as.tToolCall.sCallId = "call_lookup_01";
    pOutputs[1].as.tToolCall.sToolName = "lookup_account";
    pOutputs[1].as.tToolCall.sArgumentsJson = "{\"id\":42}";

    pOutputs[2].eKind = XLLM_OUTPUT_MESSAGE;
    pOutputs[2].as.tMessage.pParts = pParts;
    pOutputs[2].as.tMessage.iPartCount = 1u;
    pParts[0].eKind = XLLM_PART_TEXT;
    pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pParts[0].as.tSource.sMimeType = "text/plain";
    pParts[0].as.tSource.as.sText = "Attach the retrieved account context as a pinned block.";
    return 0;
}

static void reset_mock_response(xllm_response *pResponse)
{
    if ( !pResponse ) {
        return;
    }

    if ( pResponse->pOutputs ) {
        if ( pResponse->iOutputCount >= 3u &&
             pResponse->pOutputs[2].eKind == XLLM_OUTPUT_MESSAGE &&
             pResponse->pOutputs[2].as.tMessage.pParts ) {
            xrtFree(pResponse->pOutputs[2].as.tMessage.pParts);
        }
        xrtFree(pResponse->pOutputs);
    }
    memset(pResponse, 0, sizeof(*pResponse));
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_ingest_options tKnowledgeIngest;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_context_options tContextOptions;
    xllm_memory_ingest_turn_response_options tTurnResponseOptions;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecords;
    xllm_memory_chunk_list_result tChunks;
    xllm_turn tTurn;
    xllm_response tResponse;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const xllm_memory_record_info *pRecord;
    const xllm_memory_chunk_info *pChunk;
    const char *sText;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_options_init(&tKnowledgeIngest);
    xllm_memory_search_options_init(&tSearchOptions);
    xllm_memory_context_options_init(&tContextOptions);
    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    xllm_memory_list_options_init(&tListOptions);
    memset(&tRecords, 0, sizeof(tRecords));
    memset(&tChunks, 0, sizeof(tChunks));
    xllm_turn_init(&tTurn);
    memset(&tResponse, 0, sizeof(tResponse));
    xllm_error_init(&tError);

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tEmbedderOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_HASH;
    tEmbedderOptions.uHashDimensions = 32u;
    iStatus = xllm_memory_make_builtin_embedder(&tEmbedderOptions, &tMemoryOptions.tEmbedder, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "builtin embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-turn-response";
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    tKnowledgeIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tKnowledgeIngest.sRecordId = "knowledge-policy";
    tKnowledgeIngest.sTitle = "Policy";
    tKnowledgeIngest.sSourceUri = "workspace://policy";
    tKnowledgeIngest.sText = "Retrieved account context should be attached as a pinned knowledge block.";
    iStatus = xllm_memory_ingest_text(pMemory, &tKnowledgeIngest, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "knowledge ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    iStatus = xllm_turn_set_system_prompt(&tTurn, "You are a helpful agent.");
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "set system prompt failed: %d\n", iStatus);
        goto cleanup;
    }
    iStatus = xllm_turn_add_user_text(&tTurn, "How should account lookup context be attached?");
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "add user text failed: %d\n", iStatus);
        goto cleanup;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSearchOptions.sQuery = "How should account lookup context be attached?";
    tContextOptions.iPriority = 9;
    tContextOptions.bPinned = true;
    tContextOptions.uMaxHits = 1u;
    iStatus = xllm_memory_search_and_apply_to_turn(pMemory, &tSearchOptions, &tTurn, &tContextOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "search and apply to turn failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tTurn.iContextBlockCount == 1u, "expected one injected context block on turn") != 0 ) {
        goto cleanup;
    }

    if ( init_mock_response(&tResponse) != 0 ) {
        fprintf(stderr, "failed to initialize mock response\n");
        goto cleanup;
    }

    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    tTurnResponseOptions.pTurn = &tTurn;
    tTurnResponseOptions.pResponse = &tResponse;
    tTurnResponseOptions.sRecordId = "conv-default";
    tTurnResponseOptions.uChunkChars = 4096u;
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "default turn_response ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    tTurnResponseOptions.pTurn = &tTurn;
    tTurnResponseOptions.pResponse = &tResponse;
    tTurnResponseOptions.sRecordId = "conv-full";
    tTurnResponseOptions.sTitle = "Conversation transcript";
    tTurnResponseOptions.sSourceUri = "conversation://explicit-full";
    tTurnResponseOptions.bIncludeSystemPrompt = true;
    tTurnResponseOptions.bIncludeContextBlocks = true;
    tTurnResponseOptions.bIncludeThinking = true;
    tTurnResponseOptions.uChunkChars = 4096u;
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "full turn_response ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 2u, "expected two memory conversation records") != 0 ) {
        goto cleanup;
    }

    pRecord = find_record(&tRecords, "conv-default");
    if ( require_true(pRecord != NULL, "missing conv-default record") != 0 ) {
        goto cleanup;
    }
    if ( require_true(strcmp(pRecord->sSourceUri, "conversation://conv-default") == 0, "default source_uri mismatch") != 0 ||
         require_true(pRecord->tMetadata && xvoType(pRecord->tMetadata) == XVO_DT_TABLE, "default metadata table missing") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"memory_type", 0u), "conversation.turn_response.v1") == 0, "memory_type metadata mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"extraction_policy", 0u), "turn_response") == 0, "extraction policy metadata mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"conversation_kind", 0u), "turn_response") == 0, "conversation kind metadata mismatch") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"turn_message_count", 0u) == 1, "turn message count metadata mismatch") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"turn_context_block_count", 0u) == 1, "turn context block count metadata mismatch") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"has_system_prompt", 0u) == 1, "has_system_prompt metadata mismatch") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"response_output_count", 0u) == 3, "response output count metadata mismatch") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"response_tool_call_count", 0u) == 1, "response tool call count metadata mismatch") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"include_context_blocks", 0u) == 0, "default include_context_blocks metadata mismatch") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"include_thinking", 0u) == 0, "default include_thinking metadata mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"response_status", 0u), "completed") == 0, "response_status metadata mismatch") != 0 ) {
        goto cleanup;
    }

    pRecord = find_record(&tRecords, "conv-full");
    if ( require_true(pRecord != NULL, "missing conv-full record") != 0 ) {
        goto cleanup;
    }
    if ( require_true(strcmp(pRecord->sTitle, "Conversation transcript") == 0, "explicit title mismatch") != 0 ||
         require_true(strcmp(pRecord->sSourceUri, "conversation://explicit-full") == 0, "explicit source_uri mismatch") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"include_context_blocks", 0u) == 1, "full include_context_blocks metadata mismatch") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"include_thinking", 0u) == 1, "full include_thinking metadata mismatch") != 0 ) {
        goto cleanup;
    }

    xllm_memory_record_list_result_reset(&tRecords);

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    iStatus = xllm_memory_list_chunks(pMemory, &tListOptions, &tChunks, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list chunks failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    pChunk = find_chunk(&tChunks, "conv-default");
    if ( require_true(pChunk != NULL, "missing conv-default chunk") != 0 ) {
        goto cleanup;
    }
    sText = pChunk->sText;
    if ( require_true(sText != NULL, "default transcript text missing") != 0 ||
         require_true(record_contains_text(&tChunks, "conv-default", "user: How should account lookup context be attached?"), "default transcript missing user line") != 0 ||
         require_true(record_contains_text(&tChunks, "conv-default", "assistant: tool_call lookup_account({\"id\":42})"), "default transcript missing tool call line") != 0 ||
         require_true(record_contains_text(&tChunks, "conv-default", "assistant: Attach the retrieved account context as a pinned block."), "default transcript missing assistant line") != 0 ||
         require_true(!record_contains_text(&tChunks, "conv-default", "system: You are a helpful agent."), "default transcript should not include system prompt") != 0 ||
         require_true(!record_contains_text(&tChunks, "conv-default", "context[knowledge]"), "default transcript should not include context blocks") != 0 ||
         require_true(!record_contains_text(&tChunks, "conv-default", "assistant_thinking:"), "default transcript should not include thinking") != 0 ) {
        goto cleanup;
    }

    pChunk = find_chunk(&tChunks, "conv-full");
    if ( require_true(pChunk != NULL, "missing conv-full chunk") != 0 ) {
        goto cleanup;
    }
    sText = pChunk->sText;
    if ( require_true(sText != NULL, "full transcript text missing") != 0 ||
         require_true(record_contains_text(&tChunks, "conv-full", "system: You are a helpful agent."), "full transcript missing system prompt") != 0 ||
         require_true(record_contains_text(&tChunks, "conv-full", "context[knowledge]"), "full transcript missing context block") != 0 ||
         require_true(record_contains_text(&tChunks, "conv-full", "Retrieved knowledge context"), "full transcript missing retrieved context text") != 0 ||
         require_true(record_contains_text(&tChunks, "conv-full", "assistant_thinking: private reasoning should stay optional"), "full transcript missing thinking text") != 0 ||
         require_true(record_contains_text(&tChunks, "conv-full", "assistant: Attach the retrieved account context as a pinned block."), "full transcript missing assistant line") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_ingest_turn_response ok\n");
    iRc = 0;

cleanup:
    xllm_memory_chunk_list_result_reset(&tChunks);
    xllm_memory_record_list_result_reset(&tRecords);
    reset_mock_response(&tResponse);
    xllm_turn_reset(&tTurn);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
