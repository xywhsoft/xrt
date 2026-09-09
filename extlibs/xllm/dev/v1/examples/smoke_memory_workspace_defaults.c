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
    xllm_memory_ingest_directory_options tDirectoryOptions;
    xllm_memory_ingest_directory_result tDirectoryResult;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sRootDir = "build\\smoke_memory_workspace_defaults_tmp";
    const char *sSrcDir = "build\\smoke_memory_workspace_defaults_tmp\\src";
    const char *sNodeDir = "build\\smoke_memory_workspace_defaults_tmp\\node_modules";
    const char *sBuildDir = "build\\smoke_memory_workspace_defaults_tmp\\build";
    const char *sGitDir = "build\\smoke_memory_workspace_defaults_tmp\\.git";
    const char *sReadmePath = "build\\smoke_memory_workspace_defaults_tmp\\README.md";
    const char *sSourcePath = "build\\smoke_memory_workspace_defaults_tmp\\src\\main.c";
    const char *sNodePath = "build\\smoke_memory_workspace_defaults_tmp\\node_modules\\dep.js";
    const char *sBuildPath = "build\\smoke_memory_workspace_defaults_tmp\\build\\artifact.txt";
    const char *sGitPath = "build\\smoke_memory_workspace_defaults_tmp\\.git\\config";
    const char *sImagePath = "build\\smoke_memory_workspace_defaults_tmp\\logo.png";
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_directory_options_init(&tDirectoryOptions);
    xllm_memory_ingest_directory_result_init(&tDirectoryResult);
    xllm_memory_search_options_init(&tSearchOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));

    if ( !xrtDirCreateAll((str)sSrcDir) ||
         !xrtDirCreateAll((str)sNodeDir) ||
         !xrtDirCreateAll((str)sBuildDir) ||
         !xrtDirCreateAll((str)sGitDir) ) {
        fprintf(stderr, "failed to prepare workspace directories\n");
        return 1;
    }
    if ( write_text_file(sReadmePath, "workspace readme: project memory should index docs and source.\n") != 0 ||
         write_text_file(sSourcePath, "int main(void) { return 0; } /* project memory */\n") != 0 ||
         write_text_file(sNodePath, "console.log('dependency');\n") != 0 ||
         write_text_file(sBuildPath, "generated build artifact\n") != 0 ||
         write_text_file(sGitPath, "[core]\nrepositoryformatversion = 0\n") != 0 ||
         write_text_file(sImagePath, "not-a-real-image-but-extension-should-skip\n") != 0 ) {
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
        fprintf(stderr, "builtin embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 4;
    }

    tMemoryOptions.sNamespace = "smoke-workspace";
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

    tDirectoryOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tDirectoryOptions.sPath = sRootDir;
    tDirectoryOptions.bRecursive = true;
    tDirectoryOptions.bSkipHidden = true;
    tDirectoryOptions.bUseWorkspaceDefaults = true;
    tDirectoryOptions.sRecordIdPrefix = "workspace";
    iStatus = xllm_memory_ingest_directory(pMemory, &tDirectoryOptions, &tDirectoryResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "workspace ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 6;
    }

    if ( tDirectoryResult.uVisitedFileCount != 6u ||
         tDirectoryResult.uIngestedFileCount != 2u ||
         tDirectoryResult.uSkippedFileCount != 4u ||
         tDirectoryResult.uFailedFileCount != 0u ) {
        fprintf(stderr,
                "unexpected workspace ingest result: visited=%u ingested=%u skipped=%u failed=%u\n",
                (unsigned)tDirectoryResult.uVisitedFileCount,
                (unsigned)tDirectoryResult.uIngestedFileCount,
                (unsigned)tDirectoryResult.uSkippedFileCount,
                (unsigned)tDirectoryResult.uFailedFileCount);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 7;
    }

    if ( xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_ANY) != 2u ) {
        fprintf(stderr, "unexpected record count after workspace ingest\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 8;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSearchOptions.sQuery = "Where should workspace memory index project code and docs?";
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "workspace search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 9;
    }
    if ( tSearchResult.iHitCount == 0u ) {
        fprintf(stderr, "expected at least one workspace search hit\n");
        xllm_memory_search_result_reset(&tSearchResult);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 10;
    }

    printf("smoke_memory_workspace_defaults ok\n");

    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
