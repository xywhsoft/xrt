#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xllm-memory.h"

XLLM_API void xllm_turn_init(xllm_turn *pTurn);
XLLM_API void xllm_turn_reset(xllm_turn *pTurn);
XLLM_API int xllm_turn_set_system_prompt(xllm_turn *pTurn, const char *sText);
XLLM_API int xllm_turn_add_user_text(xllm_turn *pTurn, const char *sText);

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

static const char *first_context_text(const xllm_context_block *pBlock)
{
    if ( !pBlock ||
         pBlock->iMessageCount == 0u ||
         !pBlock->pMessages ||
         pBlock->pMessages[0].iPartCount == 0u ||
         !pBlock->pMessages[0].pParts ) {
        return NULL;
    }

    if ( pBlock->pMessages[0].pParts[0].eKind != XLLM_PART_TEXT ||
         pBlock->pMessages[0].pParts[0].as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
        return NULL;
    }

    return pBlock->pMessages[0].pParts[0].as.tSource.as.sText;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_ingest_directory_options tIngestOptions;
    xllm_memory_ingest_directory_result tIngestResult;
    xllm_memory_turn_search_apply_options tBridgeOptions;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    xllm_turn tSourceTurn;
    xllm_turn tDerivedTurn;
    xllm_turn tSystemOnlyTurn;
    xllm_turn_request tSystemTargetTurn;
    xllm_request tRequest;
    const xllm_context_block *pBlock;
    const char *sText;
    const char *sRootDir = "build\\smoke_memory_search_apply_from_turn_tmp";
    const char *sAppleDocPath = "build\\smoke_memory_search_apply_from_turn_tmp\\apple_note.txt";
    const char *sBananaDocPath = "build\\smoke_memory_search_apply_from_turn_tmp\\banana_note.txt";
    const char *sSystemDocPath = "build\\smoke_memory_search_apply_from_turn_tmp\\system_note.txt";
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_directory_options_init(&tIngestOptions);
    xllm_memory_ingest_directory_result_init(&tIngestResult);
    xllm_memory_turn_search_apply_options_init(&tBridgeOptions);
    xllm_turn_init(&tSourceTurn);
    xllm_turn_init(&tDerivedTurn);
    xllm_turn_init(&tSystemOnlyTurn);
    xllm_turn_init(&tSystemTargetTurn);
    xllm_request_init(&tRequest);

    if ( !xrtDirCreateAll((str)sRootDir) ) {
        fprintf(stderr, "failed to create temp root directory\n");
        goto cleanup;
    }
    if ( write_text_file(
            sAppleDocPath,
            "apple override reference should only appear when search query override is used.\n"
         ) != 0 ||
         write_text_file(
            sBananaDocPath,
            "banana session bridge answer should come from the latest user text.\n"
         ) != 0 ||
         write_text_file(
            sSystemDocPath,
            "system prompt memory bridge can derive search queries without user messages.\n"
         ) != 0 ) {
        fprintf(stderr, "failed to write temp memory documents\n");
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

    tMemoryOptions.sNamespace = "smoke-search-apply-from-turn";
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    tIngestOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngestOptions.sPath = sRootDir;
    tIngestOptions.sRecordIdPrefix = "turn-bridge";
    iStatus = xllm_memory_ingest_directory(pMemory, &tIngestOptions, &tIngestResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    if ( xllm_turn_add_user_text(&tSourceTurn, "Earlier request: apple override reference.") != XRT_NET_OK ||
         xllm_turn_add_user_text(&tSourceTurn, "Need the banana session bridge answer from the latest user text.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to populate source turn\n");
        goto cleanup;
    }

    xllm_memory_turn_search_apply_options_init(&tBridgeOptions);
    tBridgeOptions.tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tBridgeOptions.tSearchOptions.uMaxHits = 1u;
    tBridgeOptions.tContextOptions.sLabel = "Derived turn context:";
    tBridgeOptions.tContextOptions.uMaxHits = 1u;
    iStatus = xllm_memory_search_and_apply_from_turn_to_turn(
        pMemory,
        &tSourceTurn,
        &tDerivedTurn,
        &tBridgeOptions,
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "derived turn search/apply failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tDerivedTurn.iContextBlockCount == 1u, "expected one derived turn context block") != 0 ) {
        goto cleanup;
    }
    pBlock = &tDerivedTurn.pContextBlocks[0];
    sText = first_context_text(pBlock);
    if ( require_true(sText != NULL, "derived turn context text missing") != 0 ||
         require_true(strstr(sText, "Derived turn context:") != NULL, "derived turn context label missing") != 0 ||
         require_true(strstr(sText, "banana session bridge answer") != NULL, "derived turn should use the latest user text") != 0 ||
         require_true(strstr(sText, "apple override reference should only appear") == NULL, "derived turn should not use the earlier user text as top hit") != 0 ) {
        goto cleanup;
    }

    xllm_memory_turn_search_apply_options_init(&tBridgeOptions);
    tBridgeOptions.tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tBridgeOptions.tSearchOptions.sQuery = "apple override reference";
    tBridgeOptions.tSearchOptions.uMaxHits = 1u;
    tBridgeOptions.tContextOptions.sLabel = "Override request context:";
    tBridgeOptions.tContextOptions.uMaxHits = 1u;
    iStatus = xllm_memory_search_and_apply_from_turn_to_request(
        pMemory,
        &tSourceTurn,
        &tRequest,
        &tBridgeOptions,
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "override request search/apply failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRequest.iContextBlockCount == 1u, "expected one override request context block") != 0 ) {
        goto cleanup;
    }
    pBlock = &tRequest.pContextBlocks[0];
    sText = first_context_text(pBlock);
    if ( require_true(sText != NULL, "override request context text missing") != 0 ||
         require_true(strstr(sText, "Override request context:") != NULL, "override request context label missing") != 0 ||
         require_true(strstr(sText, "apple override reference should only appear") != NULL, "explicit query override should win") != 0 ) {
        goto cleanup;
    }

    if ( xllm_turn_set_system_prompt(
            &tSystemOnlyTurn,
            "system prompt memory bridge can derive search queries without user messages."
         ) != XRT_NET_OK ) {
        fprintf(stderr, "failed to set system prompt on system-only turn\n");
        goto cleanup;
    }

    xllm_memory_turn_search_apply_options_init(&tBridgeOptions);
    tBridgeOptions.tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tBridgeOptions.tSearchOptions.uMaxHits = 1u;
    tBridgeOptions.eQueryMode = XLLM_MEMORY_TURN_QUERY_VISIBLE_TEXT;
    tBridgeOptions.bIncludeSystemPrompt = true;
    tBridgeOptions.tContextOptions.sLabel = "System-derived context:";
    tBridgeOptions.tContextOptions.uMaxHits = 1u;
    iStatus = xllm_memory_search_and_apply_from_turn_request_to_turn_request(
        pMemory,
        &tSystemOnlyTurn,
        &tSystemTargetTurn,
        &tBridgeOptions,
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "system-only search/apply failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSystemTargetTurn.iContextBlockCount == 1u, "expected one system-derived context block") != 0 ) {
        goto cleanup;
    }
    pBlock = &tSystemTargetTurn.pContextBlocks[0];
    sText = first_context_text(pBlock);
    if ( require_true(sText != NULL, "system-derived context text missing") != 0 ||
         require_true(strstr(sText, "System-derived context:") != NULL, "system-derived label missing") != 0 ||
         require_true(strstr(sText, "system prompt memory bridge can derive search queries without user messages") != NULL, "system prompt should be queryable") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_search_apply_from_turn ok\n");
    iRc = 0;

cleanup:
    xllm_request_reset(&tRequest);
    xllm_turn_reset(&tSystemTargetTurn);
    xllm_turn_reset(&tSystemOnlyTurn);
    xllm_turn_reset(&tDerivedTurn);
    xllm_turn_reset(&tSourceTurn);
    xllm_memory_ingest_directory_result_reset(&tIngestResult);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
