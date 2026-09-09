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

static const xllm_memory_record_info *find_record(
    const xllm_memory_record_list_result *pRecords,
    const char *sSourceUri
)
{
    size_t i;

    if ( !pRecords || !sSourceUri ) {
        return NULL;
    }

    for ( i = 0u; i < pRecords->iRecordCount; ++i ) {
        if ( pRecords->pRecords[i].sSourceUri &&
             strcmp(pRecords->pRecords[i].sSourceUri, sSourceUri) == 0 ) {
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
    xllm_memory_remove_expired_options tExpireOptions;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecords;
    xllm_turn tTurn;
    xllm_response tResponse;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const xllm_memory_record_info *pRecord;
    uint32 uRemoved = 0u;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    xllm_memory_remove_expired_options_init(&tExpireOptions);
    xllm_memory_list_options_init(&tListOptions);
    memset(&tRecords, 0, sizeof(tRecords));
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

    tMemoryOptions.sNamespace = "remove-expired";
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    iStatus = xllm_turn_add_user_text(&tTurn, "Remember the account guidance.");
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "add user text failed: %d\n", iStatus);
        goto cleanup;
    }

    if ( init_mock_response(&tResponse, "Expired answer.") != 0 ) {
        fprintf(stderr, "failed to initialize expired mock response\n");
        goto cleanup;
    }

    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    tTurnResponseOptions.pTurn = &tTurn;
    tTurnResponseOptions.pResponse = &tResponse;
    tTurnResponseOptions.sConversationId = "thread-expire";
    tTurnResponseOptions.sTurnId = "expired";
    tTurnResponseOptions.bUseStableIdentity = true;
    tTurnResponseOptions.iPriority = 7;
    tTurnResponseOptions.iExpiresAtUnix = 1000;
    tTurnResponseOptions.uChunkChars = 4096u;
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "expired ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    reset_mock_response(&tResponse);
    if ( init_mock_response(&tResponse, "Future answer.") != 0 ) {
        fprintf(stderr, "failed to initialize future mock response\n");
        goto cleanup;
    }

    tTurnResponseOptions.sTurnId = "future";
    tTurnResponseOptions.iPriority = 3;
    tTurnResponseOptions.iExpiresAtUnix = 3000;
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "future ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    reset_mock_response(&tResponse);
    if ( init_mock_response(&tResponse, "Sticky answer.") != 0 ) {
        fprintf(stderr, "failed to initialize sticky mock response\n");
        goto cleanup;
    }

    tTurnResponseOptions.sTurnId = "sticky";
    tTurnResponseOptions.iPriority = 1;
    tTurnResponseOptions.iExpiresAtUnix = 0;
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "sticky ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records before expiry removal failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 3u, "expected three conversation memory records before prune") != 0 ) {
        goto cleanup;
    }
    pRecord = find_record(&tRecords, "conversation://thread-expire/future");
    if ( require_true(pRecord != NULL, "missing future conversation record") != 0 ||
         require_true(pRecord->tMetadata && xvoType(pRecord->tMetadata) == XVO_DT_TABLE, "future metadata missing") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"priority", 0u) == 3, "future priority metadata mismatch") != 0 ||
         require_true((long long)xvoTableGetInt(pRecord->tMetadata, (str)"expires_at_unix", 0u) == 3000LL, "future expires_at_unix metadata mismatch") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecords);

    xllm_memory_remove_expired_options_init(&tExpireOptions);
    tExpireOptions.iNowUnix = 2000;
    iStatus = xllm_memory_remove_expired(pMemory, &tExpireOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "remove expired failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 1u, "expected one expired conversation record removed") != 0 ) {
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records after expiry removal failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 2u, "expected two records after pruning expired conversation memory") != 0 ||
         require_true(find_record(&tRecords, "conversation://thread-expire/expired") == NULL, "expired record should be removed") != 0 ||
         require_true(find_record(&tRecords, "conversation://thread-expire/future") != NULL, "future record should remain") != 0 ||
         require_true(find_record(&tRecords, "conversation://thread-expire/sticky") != NULL, "non-expiring record should remain") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecords);

    iStatus = xllm_memory_remove_expired(pMemory, &tExpireOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "second remove expired failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 0u, "expected no removals on second prune pass") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_remove_expired ok\n");
    iRc = 0;

cleanup:
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
