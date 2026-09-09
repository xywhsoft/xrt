#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xllm-memory.h"

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

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_ingest_workspace_options tWorkspaceOptions;
    xllm_memory_ingest_directory_result tResult;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sRootDir = "build\\smoke_memory_ingest_workspace_ignore_patterns_tmp";
    const char *sSrcDir = "build\\smoke_memory_ingest_workspace_ignore_patterns_tmp\\src";
    const char *sDocsPrivateDir = "build\\smoke_memory_ingest_workspace_ignore_patterns_tmp\\docs\\private";
    const char *sReadmePath = "build\\smoke_memory_ingest_workspace_ignore_patterns_tmp\\README.md";
    const char *sMainPath = "build\\smoke_memory_ingest_workspace_ignore_patterns_tmp\\src\\main.c";
    const char *sGeneratedPath = "build\\smoke_memory_ingest_workspace_ignore_patterns_tmp\\src\\generated.auto.c";
    const char *sPrivatePath = "build\\smoke_memory_ingest_workspace_ignore_patterns_tmp\\docs\\private\\secret.md";
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_workspace_options_init(&tWorkspaceOptions);
    xllm_memory_ingest_directory_result_init(&tResult);
    xllm_memory_search_options_init(&tSearchOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));

    if ( !xrtDirCreateAll((str)sSrcDir) || !xrtDirCreateAll((str)sDocsPrivateDir) ) {
        fprintf(stderr, "failed to create workspace directories\n");
        return 1;
    }
    if ( write_text_file(sReadmePath, "workspace readme should be indexed\n") != 0 ||
         write_text_file(sMainPath, "int main(void) { return 0; } /* index me */\n") != 0 ||
         write_text_file(sGeneratedPath, "auto generated source should be ignored\n") != 0 ||
         write_text_file(sPrivatePath, "private docs should be ignored\n") != 0 ) {
        fprintf(stderr, "failed to write workspace files\n");
        return 2;
    }

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        return 3;
    }

    tEmbedderOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_HASH;
    tEmbedderOptions.uHashDimensions = 32u;
    iStatus = xllm_memory_make_builtin_embedder(&tEmbedderOptions, &tMemoryOptions.tEmbedder, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 4;
    }

    tMemoryOptions.sNamespace = "smoke-workspace-ignore-patterns";
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 5;
    }

    tWorkspaceOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tWorkspaceOptions.sPath = sRootDir;
    tWorkspaceOptions.sRecordIdPrefix = "workspace";
    tWorkspaceOptions.sIgnoredPathPatterns = "*.auto.c;docs/private/*";
    iStatus = xllm_memory_ingest_workspace(pMemory, &tWorkspaceOptions, &tResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "workspace ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 6;
    }

    if ( tResult.uVisitedFileCount != 4u ||
         tResult.uIngestedFileCount != 2u ||
         tResult.iSkippedDetailCount != 2u ||
         tResult.uSkippedFileCount != 2u ||
         tResult.uFailedFileCount != 0u ) {
        fprintf(stderr,
                "unexpected workspace ingest result: visited=%u ingested=%u skipped=%u failed=%u\n",
                (unsigned)tResult.uVisitedFileCount,
                (unsigned)tResult.uIngestedFileCount,
                (unsigned)tResult.uSkippedFileCount,
                (unsigned)tResult.uFailedFileCount);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 7;
    }
    if ( !(tResult.pSkippedFiles[0].eReason == XLLM_MEMORY_SKIP_IGNORED_PATH_PATTERN ||
           tResult.pSkippedFiles[1].eReason == XLLM_MEMORY_SKIP_IGNORED_PATH_PATTERN) ) {
        fprintf(stderr, "expected ignored-path-pattern skip details\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 8;
    }

    if ( xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_ANY) != 2u ) {
        fprintf(stderr, "unexpected record count after ignore-pattern ingest\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 9;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSearchOptions.sQuery = "private docs and generated sources should be absent";
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 10;
    }
    if ( tSearchResult.iHitCount == 0u ) {
        fprintf(stderr, "expected search hits from retained workspace records\n");
        xllm_memory_search_result_reset(&tSearchResult);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 11;
    }

    printf("smoke_memory_ingest_workspace_ignore_patterns ok\n");

    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_ingest_directory_result_reset(&tResult);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
