#include <stdio.h>
#include <string.h>

#include "xllm-memory-bridge.h"

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

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_ingest_turn_response_options tTurnResponseOptions;
    xllm_memory_chat_bridge_options tBridgeOptions;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecords;
    xllm_memory_chunk_list_result tChunks;
    xllm_turn tTurn;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const xllm_memory_record_info *pRecord;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    xllm_memory_chat_bridge_options_init(&tBridgeOptions);
    xllm_memory_search_options_init(&tSearchOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_memory_list_options_init(&tListOptions);
    memset(&tRecords, 0, sizeof(tRecords));
    memset(&tChunks, 0, sizeof(tChunks));
    xllm_turn_init(&tTurn);
    xllm_error_init(&tError);

    if ( require_true(tTurnResponseOptions.eExtractionPolicy == XLLM_MEMORY_EXTRACTION_POLICY_TURN_RESPONSE,
                      "turn_response options should default to turn_response extraction") != 0 ||
         require_true(tBridgeOptions.bSearchBeforeChat, "bridge should search before chat by default") != 0 ||
         require_true(!tBridgeOptions.bIngestAfterChat, "bridge should not ingest after chat by default") != 0 ||
         require_true(tBridgeOptions.tIngest.eExtractionPolicy == XLLM_MEMORY_EXTRACTION_POLICY_TURN_RESPONSE,
                      "bridge ingest options should default to turn_response extraction") != 0 ) {
        goto cleanup;
    }

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

    tMemoryOptions.sNamespace = "smoke-memory-extraction-policy";
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    tTurnResponseOptions.eExtractionPolicy = XLLM_MEMORY_EXTRACTION_POLICY_NONE;
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "none policy ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 0u,
                      "none policy should not create memory records") != 0 ) {
        goto cleanup;
    }

    if ( xllm_turn_add_user_text(&tTurn, "Remember that citrus context must stay explicit.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to build turn\n");
        goto cleanup;
    }

    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    tTurnResponseOptions.pTurn = &tTurn;
    tTurnResponseOptions.sRecordId = "policy-turn";
    tTurnResponseOptions.sConversationId = "policy-conv";
    tTurnResponseOptions.sTurnId = "turn-001";
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "turn_response policy ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 1u, "expected one typed conversation memory record") != 0 ) {
        goto cleanup;
    }

    pRecord = find_record(&tRecords, "policy-turn");
    if ( require_true(pRecord != NULL, "missing policy-turn record") != 0 ||
         require_true(pRecord->tMetadata && xvoType(pRecord->tMetadata) == XVO_DT_TABLE, "policy metadata table missing") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"memory_type", 0u),
                             "conversation.turn_response.v1") == 0,
                      "memory_type metadata mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"extraction_policy", 0u),
                             "turn_response") == 0,
                      "extraction_policy metadata mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"conversation_kind", 0u),
                             "turn_response") == 0,
                      "conversation_kind compatibility metadata mismatch") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecords);

    xllm_memory_search_options_init(&tSearchOptions);
    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tSearchOptions.sQuery = "citrus context explicit";
    tSearchOptions.sMetadataKey = "memory_type";
    tSearchOptions.sMetadataValue = "conversation.turn_response.v1";
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "typed memory search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount == 1u, "typed memory search should return one hit") != 0 ||
         require_true(tSearchResult.pHits[0].sRecordId &&
                      strcmp(tSearchResult.pHits[0].sRecordId, "policy-turn") == 0,
                      "typed memory search returned wrong record") != 0 ) {
        goto cleanup;
    }
    xllm_memory_search_result_reset(&tSearchResult);

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sMetadataKey = "extraction_policy";
    tListOptions.sMetadataValue = "turn_response";
    iStatus = xllm_memory_list_chunks(pMemory, &tListOptions, &tChunks, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "typed memory chunk list failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tChunks.iChunkCount == 1u, "typed memory chunk list should return one chunk") != 0 ||
         require_true(tChunks.pChunks[0].sRecordId &&
                      strcmp(tChunks.pChunks[0].sRecordId, "policy-turn") == 0,
                      "typed memory chunk list returned wrong record") != 0 ) {
        goto cleanup;
    }
    xllm_memory_chunk_list_result_reset(&tChunks);

    printf("smoke_memory_extraction_policy ok\n");
    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_record_list_result_reset(&tRecords);
    xllm_memory_chunk_list_result_reset(&tChunks);
    xllm_turn_reset(&tTurn);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_reset(&tError);
    return iRc;
}
