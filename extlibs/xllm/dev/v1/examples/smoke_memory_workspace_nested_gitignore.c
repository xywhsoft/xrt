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

static const xllm_memory_record_info *find_record_by_source_uri(
    const xllm_memory_record_info *pInfos,
    size_t iInfoCount,
    const char *sSourceUri
)
{
    size_t i;

    if ( !pInfos || !sSourceUri || !sSourceUri[0] ) {
        return NULL;
    }
    for ( i = 0u; i < iInfoCount; ++i ) {
        if ( pInfos[i].sSourceUri && strcmp(pInfos[i].sSourceUri, sSourceUri) == 0 ) {
            return &pInfos[i];
        }
    }
    return NULL;
}

static const xllm_memory_skipped_file_info *find_skipped_file(
    const xllm_memory_skipped_file_info *pInfos,
    size_t iInfoCount,
    const char *sRelativePath,
    xllm_memory_skip_reason eReason
)
{
    size_t i;

    if ( !pInfos || !sRelativePath || !sRelativePath[0] ) {
        return NULL;
    }
    for ( i = 0u; i < iInfoCount; ++i ) {
        if ( pInfos[i].sRelativePath &&
             strcmp(pInfos[i].sRelativePath, sRelativePath) == 0 &&
             pInfos[i].eReason == eReason ) {
            return &pInfos[i];
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
    xllm_memory_ingest_workspace_options tWorkspaceOptions;
    xllm_memory_ingest_directory_result tResult;
    xllm_memory_record_list_result tRecords;
    xllm_memory_list_options tListOptions;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sRootDir = "build\\smoke_memory_workspace_nested_gitignore_tmp";
    const char *sSrcDir = "build\\smoke_memory_workspace_nested_gitignore_tmp\\src";
    const char *sVendorDir = "build\\smoke_memory_workspace_nested_gitignore_tmp\\src\\vendor";
    const char *sPrivateDir = "build\\smoke_memory_workspace_nested_gitignore_tmp\\docs\\private";
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_workspace_options_init(&tWorkspaceOptions);
    xllm_memory_ingest_directory_result_init(&tResult);
    memset(&tRecords, 0, sizeof(tRecords));
    xllm_memory_list_options_init(&tListOptions);

    xrtDirDelete((str)sRootDir);
    if ( !xrtDirCreateAll((str)sSrcDir) ||
         !xrtDirCreateAll((str)sVendorDir) ||
         !xrtDirCreateAll((str)sPrivateDir) ) {
        fprintf(stderr, "failed to prepare workspace directories\n");
        return 1;
    }

    if ( write_text_file("build\\smoke_memory_workspace_nested_gitignore_tmp\\.gitignore",
                         "*.auto.c\n!src/keep.auto.c\ndocs/private/\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_nested_gitignore_tmp\\.xllmignore",
                         "README.md\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_nested_gitignore_tmp\\README.md",
                         "# ignored readme\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_nested_gitignore_tmp\\src\\main.c",
                         "int main(void) { return 0; }\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_nested_gitignore_tmp\\src\\generated.auto.c",
                         "/* ignored by root .gitignore */\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_nested_gitignore_tmp\\src\\keep.auto.c",
                         "/* unignored by root .gitignore */\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_nested_gitignore_tmp\\docs\\private\\secret.md",
                         "private docs should stay ignored\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_nested_gitignore_tmp\\src\\vendor\\.gitignore",
                         "temp.txt\n!keep.txt\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_nested_gitignore_tmp\\src\\vendor\\temp.txt",
                         "ignored by nested .gitignore\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_nested_gitignore_tmp\\src\\vendor\\keep.txt",
                         "unignored by nested .gitignore\n") != 0 ) {
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

    tMemoryOptions.sNamespace = "smoke-workspace-nested-gitignore";
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
    tWorkspaceOptions.sSourceUriPrefix = "workspace://";
    tWorkspaceOptions.bLoadGitIgnore = true;
    tWorkspaceOptions.sIgnoreFiles = ".xllmignore";

    iStatus = xllm_memory_ingest_workspace(pMemory, &tWorkspaceOptions, &tResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "workspace ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 6;
    }

    if ( tResult.uVisitedFileCount != 10u ||
         tResult.uIngestedFileCount != 3u ||
         tResult.uCreatedRecordCount != 3u ||
         tResult.uUpdatedRecordCount != 0u ||
         tResult.uSkippedFileCount != 7u ||
         tResult.uFailedFileCount != 0u ) {
        fprintf(stderr,
                "unexpected ingest result: visited=%u ingested=%u created=%u updated=%u skipped=%u failed=%u\n",
                (unsigned)tResult.uVisitedFileCount,
                (unsigned)tResult.uIngestedFileCount,
                (unsigned)tResult.uCreatedRecordCount,
                (unsigned)tResult.uUpdatedRecordCount,
                (unsigned)tResult.uSkippedFileCount,
                (unsigned)tResult.uFailedFileCount);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 7;
    }

    if ( !find_skipped_file(tResult.pSkippedFiles, tResult.iSkippedDetailCount, "README.md", XLLM_MEMORY_SKIP_IGNORED_PATH_PATTERN) ||
         !find_skipped_file(tResult.pSkippedFiles, tResult.iSkippedDetailCount, "src\\generated.auto.c", XLLM_MEMORY_SKIP_IGNORED_PATH_PATTERN) ||
         !find_skipped_file(tResult.pSkippedFiles, tResult.iSkippedDetailCount, "docs\\private\\secret.md", XLLM_MEMORY_SKIP_IGNORED_PATH_PATTERN) ||
         !find_skipped_file(tResult.pSkippedFiles, tResult.iSkippedDetailCount, "src\\vendor\\temp.txt", XLLM_MEMORY_SKIP_IGNORED_PATH_PATTERN) ) {
        fprintf(stderr, "missing expected skipped file details\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 8;
    }

    if ( xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_ANY) != 3u ) {
        fprintf(stderr, "expected 3 retained records\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 9;
    }

    tListOptions.eScope = XLLM_MEMORY_SCOPE_ANY;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 10;
    }

    if ( tRecords.iRecordCount != 3u ||
         !find_record_by_source_uri(tRecords.pRecords, tRecords.iRecordCount, "workspace://src/main.c") ||
         !find_record_by_source_uri(tRecords.pRecords, tRecords.iRecordCount, "workspace://src/keep.auto.c") ||
         !find_record_by_source_uri(tRecords.pRecords, tRecords.iRecordCount, "workspace://src/vendor/keep.txt") ) {
        fprintf(stderr, "missing expected retained records\n");
        xllm_memory_record_list_result_reset(&tRecords);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 11;
    }

    printf("smoke_memory_workspace_nested_gitignore ok\n");

    xllm_memory_record_list_result_reset(&tRecords);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
