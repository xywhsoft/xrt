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

static int ingest_knowledge(
    xllm_memory *pMemory,
    uint32 uIndex,
    xllm_error *pError
)
{
    xllm_memory_ingest_options tOptions;
    char sRecordId[64];
    char sSourceUri[96];
    char sText[256];

    snprintf(sRecordId, sizeof(sRecordId), "long-run-knowledge-%02u", (unsigned)uIndex);
    snprintf(sSourceUri, sizeof(sSourceUri), "workspace://long-run/doc-%02u.md", (unsigned)uIndex);
    snprintf(
        sText,
        sizeof(sText),
        "Long run knowledge record %02u contains anchor token longrun-anchor-%02u and durable sqlite reopen text.",
        (unsigned)uIndex,
        (unsigned)uIndex
    );

    xllm_memory_ingest_options_init(&tOptions);
    tOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tOptions.sRecordId = sRecordId;
    tOptions.sTitle = sRecordId;
    tOptions.sSourceUri = sSourceUri;
    tOptions.sText = sText;
    tOptions.bReplaceExisting = true;
    tOptions.uChunkChars = 256u;
    return xllm_memory_ingest_text(pMemory, &tOptions, pError);
}

static int ingest_turn_response(
    xllm_memory *pMemory,
    xllm_memory_ingest_turn_response_options *pOptions,
    xllm_response *pResponse,
    const char *sTurnId,
    int64 iUpdatedAtUnix,
    const char *sText,
    xllm_error *pError
)
{
    int iStatus;

    reset_mock_response(pResponse);
    if ( init_mock_response(pResponse, sText) != 0 ) {
        return XRT_NET_ERROR;
    }

    pOptions->sConversationId = "long-run-conversation";
    pOptions->sTurnId = sTurnId;
    pOptions->iUpdatedAtUnix = iUpdatedAtUnix;
    pOptions->pResponse = pResponse;
    iStatus = xllm_memory_ingest_turn_response(pMemory, pOptions, pError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "turn ingest %s failed: %s\n", sTurnId, pError && pError->sMessage ? pError->sMessage : "(null)");
    }
    return iStatus;
}

static int search_has_hit(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sQuery,
    const char *sExpectedSourceUri,
    xllm_error *pError
)
{
    xllm_memory_search_options tOptions;
    xllm_memory_search_result tResult;
    size_t i;
    int iStatus;
    int iFound = 0;

    memset(&tResult, 0, sizeof(tResult));
    xllm_memory_search_options_init(&tOptions);
    tOptions.eScope = eScope;
    tOptions.sQuery = sQuery;
    tOptions.uMaxHits = 8u;
    tOptions.uMaxCharsPerHit = 512u;

    iStatus = xllm_memory_search(pMemory, &tOptions, &tResult, pError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "search failed for %s: %s\n", sQuery, pError && pError->sMessage ? pError->sMessage : "(null)");
        return 0;
    }

    for ( i = 0u; i < tResult.iHitCount; ++i ) {
        if ( tResult.pHits[i].sSourceUri && strcmp(tResult.pHits[i].sSourceUri, sExpectedSourceUri) == 0 ) {
            iFound = 1;
            break;
        }
    }
    xllm_memory_search_result_reset(&tResult);
    return iFound;
}

static int verify_retained_conversation(xllm_memory *pMemory, xllm_error *pError)
{
    xllm_memory_list_options tOptions;
    xllm_memory_record_list_result tRecords;
    int iStatus;
    int iRc = 1;

    memset(&tRecords, 0, sizeof(tRecords));
    xllm_memory_list_options_init(&tOptions);
    tOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tOptions.sConversationId = "long-run-conversation";
    tOptions.tSortByUpdatedAtDesc.bSet = true;
    tOptions.tSortByUpdatedAtDesc.bValue = true;
    tOptions.uMaxCharsPerText = 1024u;

    iStatus = xllm_memory_list_records(pMemory, &tOptions, &tRecords, pError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list retained conversation failed: %s\n", pError && pError->sMessage ? pError->sMessage : "(null)");
        goto cleanup;
    }

    if ( require_true(tRecords.iRecordCount == 2u, "expected two retained conversation records") != 0 ||
         require_true(tRecords.pRecords[0].sSourceUri != NULL, "newest retained source uri missing") != 0 ||
         require_true(strcmp(tRecords.pRecords[0].sSourceUri, "conversation://long-run-conversation/turn-004") == 0, "expected turn-004 first") != 0 ||
         require_true(tRecords.pRecords[1].sSourceUri != NULL, "second retained source uri missing") != 0 ||
         require_true(strcmp(tRecords.pRecords[1].sSourceUri, "conversation://long-run-conversation/turn-003") == 0, "expected turn-003 second") != 0 ) {
        goto cleanup;
    }
    iRc = 0;

cleanup:
    xllm_memory_record_list_result_reset(&tRecords);
    return iRc;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_turn_response_options tTurnOptions;
    xllm_memory_trim_conversation_options tTrimOptions;
    xllm_turn tTurn;
    xllm_response tResponse;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    char sNamespace[96];
    char sDbPath[192];
    uint64 uRunId;
    uint32 uRemoved = 0u;
    uint32 i;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_turn_response_options_init(&tTurnOptions);
    xllm_memory_trim_conversation_options_init(&tTrimOptions);
    xllm_turn_init(&tTurn);
    memset(&tResponse, 0, sizeof(tResponse));
    xllm_error_init(&tError);

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    uRunId = xrtRand64();
    snprintf(sNamespace, sizeof(sNamespace), "smoke-memory-long-run-%llu", (unsigned long long)uRunId);
    snprintf(sDbPath, sizeof(sDbPath), "build\\smoke_memory_long_run_%llu.db", (unsigned long long)uRunId);
    remove(sDbPath);

    tMemoryOptions.sNamespace = sNamespace;
    tMemoryOptions.sSqlitePath = sDbPath;
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.bEnableHybridSearch = false;
    tMemoryOptions.uDefaultChunkChars = 256u;

    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    for ( i = 1u; i <= 8u; ++i ) {
        iStatus = ingest_knowledge(pMemory, i, &tError);
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "knowledge ingest %u failed: %s\n", (unsigned)i, tError.sMessage ? tError.sMessage : "(null)");
            goto cleanup;
        }
    }

    if ( require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) == 8u, "expected eight knowledge records") != 0 ||
         require_true(search_has_hit(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE, "longrun-anchor-06 durable", "workspace://long-run/doc-06.md", &tError), "expected search hit before delete") != 0 ) {
        goto cleanup;
    }

    iStatus = xllm_memory_remove_by_source_uri(
        pMemory,
        XLLM_MEMORY_SCOPE_KNOWLEDGE,
        "workspace://long-run/doc-02.md",
        &uRemoved,
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "remove by source uri failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 1u, "expected one removed knowledge record") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) == 7u, "expected seven knowledge records after delete") != 0 ) {
        goto cleanup;
    }

    if ( xllm_turn_add_user_text(&tTurn, "Remember durable conversation state for long run smoke.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to build turn\n");
        goto cleanup;
    }
    tTurnOptions.pTurn = &tTurn;
    tTurnOptions.bUseStableIdentity = true;
    tTurnOptions.uChunkChars = 2048u;

    if ( ingest_turn_response(pMemory, &tTurnOptions, &tResponse, "turn-001", 1000, "Long run old response.", &tError) != XRT_NET_OK ||
         ingest_turn_response(pMemory, &tTurnOptions, &tResponse, "turn-002", 2000, "Long run mid response.", &tError) != XRT_NET_OK ||
         ingest_turn_response(pMemory, &tTurnOptions, &tResponse, "turn-003", 3000, "Long run retained response three.", &tError) != XRT_NET_OK ||
         ingest_turn_response(pMemory, &tTurnOptions, &tResponse, "turn-004", 4000, "Long run retained response four.", &tError) != XRT_NET_OK ) {
        goto cleanup;
    }
    if ( require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 4u, "expected four conversation memory records") != 0 ) {
        goto cleanup;
    }

    tTrimOptions.sConversationId = "long-run-conversation";
    tTrimOptions.uKeepLatestRecords = 2u;
    iStatus = xllm_memory_trim_conversation(pMemory, &tTrimOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "trim conversation failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 2u, "expected two trimmed conversation records") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 2u, "expected two conversation memory records after trim") != 0 ||
         verify_retained_conversation(pMemory, &tError) != 0 ) {
        goto cleanup;
    }

    xllm_memory_destroy(pMemory);
    pMemory = NULL;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory reopen failed: %d\n", iStatus);
        goto cleanup;
    }

    if ( require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) == 7u, "expected seven persisted knowledge records after reopen") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 2u, "expected two persisted memory records after reopen") != 0 ||
         require_true(search_has_hit(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE, "longrun-anchor-06 reopen", "workspace://long-run/doc-06.md", &tError), "expected persisted search hit after reopen") != 0 ||
         verify_retained_conversation(pMemory, &tError) != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_long_run ok\n");
    iRc = 0;

cleanup:
    reset_mock_response(&tResponse);
    xllm_turn_reset(&tTurn);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_reset(&tError);
    if ( iRc == 0 ) {
        remove(sDbPath);
    }
    return iRc;
}
