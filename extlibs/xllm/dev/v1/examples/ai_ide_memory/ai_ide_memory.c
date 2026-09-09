#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xllm-memory.h"

static int require_true(int condition, const char *sMessage)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
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

static const char *first_context_text(const xllm_context_block *pBlock)
{
    if ( !pBlock ||
         pBlock->iMessageCount == 0u ||
         !pBlock->pMessages ||
         pBlock->pMessages[0].iPartCount == 0u ||
         !pBlock->pMessages[0].pParts ||
         pBlock->pMessages[0].pParts[0].eKind != XLLM_PART_TEXT ||
         pBlock->pMessages[0].pParts[0].as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
        return NULL;
    }

    return pBlock->pMessages[0].pParts[0].as.tSource.as.sText;
}

int main(void)
{
    const char *sWorkspaceRoot = "build\\ai_ide_memory_workspace";
    const char *sSrcDir = "build\\ai_ide_memory_workspace\\src";
    const char *sDocsDir = "build\\ai_ide_memory_workspace\\docs";
    const char *sMemoryDb = "build\\ai_ide_memory.db";
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_workspace_options tWorkspaceOptions;
    xllm_memory_ingest_directory_result tWorkspaceResult;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_context_options tContextOptions;
    xllm_memory_search_result tSearchResult;
    xllm_memory_chunk_list_result tChunkList;
    xllm_memory_list_options tListOptions;
    xllm_request tRequest;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sContextText;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_workspace_options_init(&tWorkspaceOptions);
    xllm_memory_ingest_directory_result_init(&tWorkspaceResult);
    xllm_memory_search_options_init(&tSearchOptions);
    xllm_memory_context_options_init(&tContextOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    memset(&tChunkList, 0, sizeof(tChunkList));
    xllm_memory_list_options_init(&tListOptions);
    xllm_request_init(&tRequest);
    xllm_error_init(&tError);

    remove(sMemoryDb);
    if ( !xrtDirCreateAll((str)sSrcDir) || !xrtDirCreateAll((str)sDocsDir) ) {
        fprintf(stderr, "failed to prepare AI IDE workspace directories\n");
        goto cleanup;
    }
    if ( write_text_file(
             "build\\ai_ide_memory_workspace\\src\\memory_bridge.c",
             "/* AI IDE memory bridge: call xllm_memory_search_and_apply_to_request before provider execution. */\n"
             "int ai_ide_memory_bridge(void) { return 42; }\n"
         ) != 0 ||
         write_text_file(
             "build\\ai_ide_memory_workspace\\docs\\architecture.md",
             "# AI IDE memory architecture\n"
             "The AI IDE indexes workspace code and docs, retrieves local context, and injects citations into the LLM request.\n"
         ) != 0 ||
         write_text_file(
             "build\\ai_ide_memory_workspace\\.env",
             "OPENAI_API_KEY=should_not_be_indexed\n"
         ) != 0 ) {
        fprintf(stderr, "failed to write AI IDE workspace files\n");
        goto cleanup;
    }

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "example-ai-ide-memory";
    tMemoryOptions.sSqlitePath = sMemoryDb;
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.bEnableHybridSearch = false;
    tMemoryOptions.uDefaultChunkChars = 512u;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    tWorkspaceOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tWorkspaceOptions.sPath = sWorkspaceRoot;
    tWorkspaceOptions.bSkipHidden = true;
    tWorkspaceOptions.sRecordIdPrefix = "workspace";
    tWorkspaceOptions.sSourceUriPrefix = "workspace://";
    tWorkspaceOptions.uMaxFileBytes = 4096u;
    tWorkspaceOptions.uChunkChars = 512u;
    iStatus = xllm_memory_ingest_workspace(pMemory, &tWorkspaceOptions, &tWorkspaceResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "workspace ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tWorkspaceResult.uIngestedFileCount == 2u, "expected code and docs to be indexed") != 0 ||
         require_true(tWorkspaceResult.uSkippedFileCount == 1u, "expected .env to be skipped by workspace defaults") != 0 ) {
        goto cleanup;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSearchOptions.sQuery = "How should the AI IDE inject workspace memory before provider execution?";
    tSearchOptions.uMaxHits = 2u;
    tSearchOptions.uMaxCharsPerHit = 512u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "workspace search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount > 0u, "expected at least one workspace memory hit") != 0 ||
         require_true(tSearchResult.pHits[0].sSourceUri != NULL, "top hit source uri missing") != 0 ||
         require_true(tSearchResult.pHits[0].sChunkId != NULL, "top hit chunk id missing") != 0 ) {
        goto cleanup;
    }

    tListOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tListOptions.sChunkId = tSearchResult.pHits[0].sChunkId;
    iStatus = xllm_memory_list_chunks(pMemory, &tListOptions, &tChunkList, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "chunk lookup failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tChunkList.iChunkCount == 1u, "expected one chunk for top hit chunk id") != 0 ||
         require_true(tChunkList.pChunks[0].iEndByte > tChunkList.pChunks[0].iStartByte, "expected chunk byte range") != 0 ) {
        goto cleanup;
    }

    tContextOptions.sLabel = "AI IDE workspace context:";
    tContextOptions.eKindOverride = XLLM_CONTEXT_MEMORY;
    tContextOptions.uMaxHits = 2u;
    tContextOptions.uMaxCharsPerHit = 256u;
    iStatus = xllm_memory_search_and_apply_to_request(pMemory, &tSearchOptions, &tRequest, &tContextOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "search/apply failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRequest.iContextBlockCount == 1u, "expected one request context block") != 0 ||
         require_true(tRequest.pContextBlocks[0].eKind == XLLM_CONTEXT_MEMORY, "expected memory context block kind") != 0 ) {
        goto cleanup;
    }
    sContextText = first_context_text(&tRequest.pContextBlocks[0]);
    if ( require_true(sContextText != NULL, "context text missing") != 0 ||
         require_true(strstr(sContextText, "AI IDE workspace context:") != NULL, "context label missing") != 0 ||
         require_true(strstr(sContextText, "Source: workspace://") != NULL, "context source citation missing") != 0 ||
         require_true(strstr(sContextText, "Chunk: ") != NULL, "context chunk citation missing") != 0 ||
         require_true(strstr(sContextText, "bytes=") != NULL, "context byte range missing") != 0 ||
         require_true(strstr(sContextText, "OPENAI_API_KEY") == NULL, "sensitive .env content should not be injected") != 0 ) {
        goto cleanup;
    }

    printf("ai_ide_memory example ok\n");
    iRc = 0;

cleanup:
    xllm_request_reset(&tRequest);
    xllm_memory_chunk_list_result_reset(&tChunkList);
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_ingest_directory_result_reset(&tWorkspaceResult);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
