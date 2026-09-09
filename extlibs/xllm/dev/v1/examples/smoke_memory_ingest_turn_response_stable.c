#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

XLLM_API void xllm_turn_init(xllm_turn *pTurn);
XLLM_API void xllm_turn_reset(xllm_turn *pTurn);
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

static int init_mock_response(xllm_response *pResponse, const char *sText)
{
    xllm_output_item *pOutputs;
    xllm_content_part *pParts;

    if ( !pResponse || !sText ) {
        return 1;
    }

    memset(pResponse, 0, sizeof(*pResponse));
    pOutputs = (xllm_output_item *)xrtCalloc(1u, sizeof(xllm_output_item));
    if ( !pOutputs ) {
        return 2;
    }
    pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pParts ) {
        xrtFree(pOutputs);
        return 3;
    }

    pResponse->eStatus = XLLM_STATUS_COMPLETED;
    pResponse->sVisibleText = sText;
    pResponse->pOutputs = pOutputs;
    pResponse->iOutputCount = 1u;

    pOutputs[0].eKind = XLLM_OUTPUT_MESSAGE;
    pOutputs[0].as.tMessage.pParts = pParts;
    pOutputs[0].as.tMessage.iPartCount = 1u;
    pParts[0].eKind = XLLM_PART_TEXT;
    pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pParts[0].as.tSource.sMimeType = "text/plain";
    pParts[0].as.tSource.as.sText = sText;
    return 0;
}

static void reset_mock_response(xllm_response *pResponse)
{
    if ( !pResponse ) {
        return;
    }

    if ( pResponse->pOutputs ) {
        if ( pResponse->iOutputCount >= 1u &&
             pResponse->pOutputs[0].eKind == XLLM_OUTPUT_MESSAGE &&
             pResponse->pOutputs[0].as.tMessage.pParts ) {
            xrtFree(pResponse->pOutputs[0].as.tMessage.pParts);
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
    xllm_memory_ingest_turn_response_options tTurnResponseOptions;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecords;
    xllm_memory_chunk_list_result tChunks;
    xllm_turn tTurn;
    xllm_response tResponse;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const xllm_memory_record_info *pRecord;
    char sStableRecordId[128];
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    xllm_memory_list_options_init(&tListOptions);
    memset(&tRecords, 0, sizeof(tRecords));
    memset(&tChunks, 0, sizeof(tChunks));
    xllm_turn_init(&tTurn);
    memset(&tResponse, 0, sizeof(tResponse));
    xllm_error_init(&tError);
    memset(sStableRecordId, 0, sizeof(sStableRecordId));

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

    tMemoryOptions.sNamespace = "smoke-turn-response-stable";
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    iStatus = xllm_turn_add_user_text(&tTurn, "Should I pin account lookup context?");
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "add user text failed: %d\n", iStatus);
        goto cleanup;
    }

    if ( init_mock_response(&tResponse, "First answer for account 42.") != 0 ) {
        fprintf(stderr, "failed to build first mock response\n");
        goto cleanup;
    }

    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    tTurnResponseOptions.pTurn = &tTurn;
    tTurnResponseOptions.pResponse = &tResponse;
    tTurnResponseOptions.sConversationId = "thread-42";
    tTurnResponseOptions.sTurnId = "turn-001";
    tTurnResponseOptions.bUseStableIdentity = true;
    tTurnResponseOptions.iUpdatedAtUnix = 1000;
    tTurnResponseOptions.uChunkChars = 4096u;
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "first stable ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records after first ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 1u, "expected one record after first stable ingest") != 0 ) {
        goto cleanup;
    }
    pRecord = &tRecords.pRecords[0];
    if ( require_true(pRecord->sRecordId != NULL, "stable record_id missing") != 0 ||
         require_true(pRecord->sSourceUri != NULL, "stable source_uri missing") != 0 ||
         require_true(strcmp(pRecord->sSourceUri, "conversation://thread-42/turn-001") == 0, "stable source_uri mismatch") != 0 ||
         require_true(pRecord->tMetadata && xvoType(pRecord->tMetadata) == XVO_DT_TABLE, "stable metadata missing") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"conversation_id", 0u), "thread-42") == 0, "conversation_id metadata mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"turn_id", 0u), "turn-001") == 0, "turn_id metadata mismatch") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"stable_identity", 0u) == 1, "stable_identity metadata mismatch") != 0 ||
         require_true((long long)xvoTableGetInt(pRecord->tMetadata, (str)"created_at_unix", 0u) == 1000LL, "created_at_unix metadata mismatch after first ingest") != 0 ||
         require_true((long long)xvoTableGetInt(pRecord->tMetadata, (str)"updated_at_unix", 0u) == 1000LL, "updated_at_unix metadata mismatch after first ingest") != 0 ) {
        goto cleanup;
    }
    strncpy(sStableRecordId, pRecord->sRecordId, sizeof(sStableRecordId) - 1u);
    xllm_memory_record_list_result_reset(&tRecords);

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_chunks(pMemory, &tListOptions, &tChunks, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list chunks after first ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(record_contains_text(&tChunks, sStableRecordId, "First answer for account 42."), "first stable transcript missing initial answer") != 0 ) {
        goto cleanup;
    }
    xllm_memory_chunk_list_result_reset(&tChunks);

    reset_mock_response(&tResponse);
    if ( init_mock_response(&tResponse, "Updated answer for account 42 after tool lookup.") != 0 ) {
        fprintf(stderr, "failed to build updated mock response\n");
        goto cleanup;
    }

    tTurnResponseOptions.iUpdatedAtUnix = 2000;
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "second stable ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records after stable update failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 1u, "stable update should keep record count at one") != 0 ) {
        goto cleanup;
    }
    pRecord = find_record(&tRecords, sStableRecordId);
    if ( require_true(pRecord != NULL, "stable record id changed after update") != 0 ||
         require_true((long long)xvoTableGetInt(pRecord->tMetadata, (str)"created_at_unix", 0u) == 1000LL, "created_at_unix should remain stable after update") != 0 ||
         require_true((long long)xvoTableGetInt(pRecord->tMetadata, (str)"updated_at_unix", 0u) == 2000LL, "updated_at_unix should advance after update") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecords);

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_chunks(pMemory, &tListOptions, &tChunks, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list chunks after stable update failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(record_contains_text(&tChunks, sStableRecordId, "Updated answer for account 42 after tool lookup."), "stable transcript missing updated answer") != 0 ||
         require_true(!record_contains_text(&tChunks, sStableRecordId, "First answer for account 42."), "stable transcript still contains stale answer") != 0 ) {
        goto cleanup;
    }
    xllm_memory_chunk_list_result_reset(&tChunks);

    tTurnResponseOptions.sTurnId = "turn-002";
    reset_mock_response(&tResponse);
    if ( init_mock_response(&tResponse, "Second turn answer for account 42.") != 0 ) {
        fprintf(stderr, "failed to build second-turn mock response\n");
        goto cleanup;
    }

    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "second turn stable ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records after second turn failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 2u, "expected two records after second turn stable ingest") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_ingest_turn_response_stable ok\n");
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
