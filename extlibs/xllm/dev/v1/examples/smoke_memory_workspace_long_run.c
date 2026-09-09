#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xllm-memory.h"

static int write_text_file(const char *sPath, const char *sText)
{
    FILE *pFile = fopen(sPath, "wb");
    size_t iLen = sText ? strlen(sText) : 0u;

    if ( !pFile ) {
        return 1;
    }
    if ( iLen > 0u && fwrite(sText, 1u, iLen, pFile) != iLen ) {
        fclose(pFile);
        return 2;
    }
    fclose(pFile);
    return 0;
}

static int remove_sqlite_files(const char *sDbPath)
{
    char sWalPath[260];
    char sShmPath[260];

    remove(sDbPath);
    snprintf(sWalPath, sizeof(sWalPath), "%s-wal", sDbPath);
    snprintf(sShmPath, sizeof(sShmPath), "%s-shm", sDbPath);
    remove(sWalPath);
    remove(sShmPath);
    return 0;
}

static const xllm_memory_record_info *find_record(
    const xllm_memory_record_info *pRecords,
    size_t iCount,
    const char *sSourceUri
)
{
    size_t i;

    if ( !pRecords || !sSourceUri ) {
        return NULL;
    }
    for ( i = 0u; i < iCount; ++i ) {
        if ( pRecords[i].sSourceUri && strcmp(pRecords[i].sSourceUri, sSourceUri) == 0 ) {
            return &pRecords[i];
        }
    }
    return NULL;
}

static const xllm_memory_record_info *find_removed(
    const xllm_memory_sync_workspace_result *pResult,
    const char *sSourceUri
)
{
    if ( !pResult ) {
        return NULL;
    }
    return find_record(pResult->pRemovedRecords, pResult->iRemovedDetailCount, sSourceUri);
}

static const xllm_memory_skipped_file_info *find_skipped(
    const xllm_memory_sync_workspace_result *pResult,
    const char *sRelativePath,
    xllm_memory_skip_reason eReason
)
{
    size_t i;

    if ( !pResult || !sRelativePath ) {
        return NULL;
    }
    for ( i = 0u; i < pResult->tIngest.iSkippedDetailCount; ++i ) {
        if ( pResult->tIngest.pSkippedFiles[i].sRelativePath &&
             strcmp(pResult->tIngest.pSkippedFiles[i].sRelativePath, sRelativePath) == 0 &&
             pResult->tIngest.pSkippedFiles[i].eReason == eReason ) {
            return &pResult->tIngest.pSkippedFiles[i];
        }
    }
    return NULL;
}

static int create_runtime_memory(
    const char *sNamespace,
    const char *sDbPath,
    xllm_runtime **ppRuntime,
    xllm_memory **ppMemory
)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_memory_options tMemoryOptions;
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    *ppRuntime = NULL;
    *ppMemory = NULL;

    iStatus = xllm_runtime_create(&tRuntimeOptions, ppRuntime);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    tMemoryOptions.sNamespace = sNamespace;
    tMemoryOptions.sSqlitePath = sDbPath;
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.bEnableHybridSearch = false;
    tMemoryOptions.uDefaultChunkChars = 4096u;
    iStatus = xllm_memory_create(*ppRuntime, &tMemoryOptions, ppMemory);
    if ( iStatus != XRT_NET_OK ) {
        xllm_runtime_destroy(*ppRuntime);
        *ppRuntime = NULL;
    }
    return iStatus;
}

static void destroy_runtime_memory(xllm_runtime **ppRuntime, xllm_memory **ppMemory)
{
    if ( ppMemory && *ppMemory ) {
        xllm_memory_destroy(*ppMemory);
        *ppMemory = NULL;
    }
    if ( ppRuntime && *ppRuntime ) {
        xllm_runtime_destroy(*ppRuntime);
        *ppRuntime = NULL;
    }
}

static int search_top_source(
    xllm_memory *pMemory,
    const char *sQuery,
    const char *sExpectedSourceUri,
    xllm_error *pError
)
{
    xllm_memory_search_options tOptions;
    xllm_memory_search_result tResult;
    int iStatus;
    int iOk = 0;

    xllm_memory_search_options_init(&tOptions);
    memset(&tResult, 0, sizeof(tResult));
    tOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tOptions.sQuery = sQuery;
    tOptions.uMaxHits = 3u;
    tOptions.uMaxCharsPerHit = 1024u;

    iStatus = xllm_memory_search(pMemory, &tOptions, &tResult, pError);
    if ( iStatus != XRT_NET_OK ) {
        return 1;
    }
    if ( tResult.iHitCount > 0u &&
         tResult.pHits[0].sSourceUri &&
         strcmp(tResult.pHits[0].sSourceUri, sExpectedSourceUri) == 0 ) {
        iOk = 1;
    }
    xllm_memory_search_result_reset(&tResult);
    return iOk ? 0 : 2;
}

int main(void)
{
    enum { FILE_COUNT = 16 };
    const char *sNamespace = "smoke-workspace-long-run";
    const char *sDbPath = "build\\smoke_memory_workspace_long_run.db";
    const char *sRootDir = "build\\smoke_memory_workspace_long_run_tmp";
    const char *sSrcDir = "build\\smoke_memory_workspace_long_run_tmp\\src";
    xllm_runtime *pRuntime = NULL;
    xllm_memory *pMemory = NULL;
    xllm_memory_ingest_workspace_options tWorkspaceOptions;
    xllm_memory_sync_workspace_result tSyncResult;
    xllm_error tError;
    char sPath[260];
    char sText[512];
    int i;
    int iStatus;

    xllm_error_init(&tError);
    xllm_memory_ingest_workspace_options_init(&tWorkspaceOptions);
    xllm_memory_sync_workspace_result_init(&tSyncResult);

    remove_sqlite_files(sDbPath);
    if ( xrtPathExists((str)sRootDir) ) {
        xrtDirDelete((str)sRootDir);
    }
    if ( !xrtDirCreateAll((str)sSrcDir) ) {
        fprintf(stderr, "failed to create workspace\n");
        return 1;
    }

    for ( i = 0; i < FILE_COUNT; ++i ) {
        snprintf(sPath, sizeof(sPath), "%s\\file_%02d.c", sSrcDir, i);
        snprintf(sText, sizeof(sText),
                 "int workspace_file_%02d(void) { return %d; }\n"
                 "const char *sentinel_%02d = \"workspace long run seed %02d\";\n",
                 i, i, i, i);
        if ( write_text_file(sPath, sText) != 0 ) {
            fprintf(stderr, "failed to write seed file\n");
            return 2;
        }
    }

    iStatus = create_runtime_memory(sNamespace, sDbPath, &pRuntime, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        return 3;
    }

    tWorkspaceOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tWorkspaceOptions.sPath = sRootDir;
    tWorkspaceOptions.sRecordIdPrefix = "workspace";
    tWorkspaceOptions.sSourceUriPrefix = "workspace://";
    tWorkspaceOptions.sAllowedExtensions = ".c";
    tWorkspaceOptions.uMaxFileBytes = 4096u;

    iStatus = xllm_memory_sync_workspace(pMemory, &tWorkspaceOptions, &tSyncResult, &tError);
    if ( iStatus != XRT_NET_OK ||
         tSyncResult.tIngest.uVisitedFileCount != FILE_COUNT ||
         tSyncResult.tIngest.uIngestedFileCount != FILE_COUNT ||
         tSyncResult.tIngest.uCreatedRecordCount != FILE_COUNT ||
         tSyncResult.uRemovedRecordCount != 0u ||
         xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) != FILE_COUNT ) {
        fprintf(stderr, "unexpected initial sync result\n");
        return 4;
    }

    snprintf(sPath, sizeof(sPath), "%s\\file_02.c", sSrcDir);
    if ( write_text_file(sPath,
                         "int workspace_file_02(void) { return 2; }\n"
                         "const char *sentinel_02 = \"workspace long run seed 02\";\n") != 0 ) {
        fprintf(stderr, "failed to touch file\n");
        return 5;
    }
    xllm_memory_sync_workspace_result_reset(&tSyncResult);
    iStatus = xllm_memory_sync_workspace(pMemory, &tWorkspaceOptions, &tSyncResult, &tError);
    if ( iStatus != XRT_NET_OK ||
         tSyncResult.tIngest.uIngestedFileCount != 0u ||
         tSyncResult.tIngest.uSkippedFileCount != FILE_COUNT ||
         !find_skipped(&tSyncResult, "src\\file_02.c", XLLM_MEMORY_SKIP_UNCHANGED) ) {
        fprintf(stderr, "unexpected touch-only sync result\n");
        return 6;
    }

    snprintf(sPath, sizeof(sPath), "%s\\file_04.c", sSrcDir);
    if ( write_text_file(sPath,
                         "int workspace_file_04(void) { return 400; }\n"
                         "const char *sentinel_04 = \"workspace long run modified alpha\";\n") != 0 ) {
        fprintf(stderr, "failed to modify file\n");
        return 7;
    }
    xllm_memory_sync_workspace_result_reset(&tSyncResult);
    iStatus = xllm_memory_sync_workspace(pMemory, &tWorkspaceOptions, &tSyncResult, &tError);
    if ( iStatus != XRT_NET_OK ||
         tSyncResult.tIngest.uIngestedFileCount != 1u ||
         tSyncResult.tIngest.uUpdatedRecordCount != 1u ||
         !find_record(tSyncResult.tIngest.pUpdatedRecords,
                      tSyncResult.tIngest.iUpdatedDetailCount,
                      "workspace://src/file_04.c") ) {
        fprintf(stderr, "unexpected modify sync result\n");
        return 8;
    }

    snprintf(sPath, sizeof(sPath), "%s\\added_omega.c", sSrcDir);
    if ( write_text_file(sPath,
                         "int added_omega(void) { return 900; }\n"
                         "const char *added_omega_sentinel = \"workspace long run added omega\";\n") != 0 ) {
        fprintf(stderr, "failed to add file\n");
        return 9;
    }
    xllm_memory_sync_workspace_result_reset(&tSyncResult);
    iStatus = xllm_memory_sync_workspace(pMemory, &tWorkspaceOptions, &tSyncResult, &tError);
    if ( iStatus != XRT_NET_OK ||
         tSyncResult.tIngest.uCreatedRecordCount != 1u ||
         xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) != FILE_COUNT + 1u ||
         !find_record(tSyncResult.tIngest.pCreatedRecords,
                      tSyncResult.tIngest.iCreatedDetailCount,
                      "workspace://src/added_omega.c") ) {
        fprintf(stderr, "unexpected add sync result\n");
        return 10;
    }

    snprintf(sPath, sizeof(sPath), "%s\\file_06.c", sSrcDir);
    if ( remove(sPath) != 0 ) {
        fprintf(stderr, "failed to delete file\n");
        return 11;
    }
    xllm_memory_sync_workspace_result_reset(&tSyncResult);
    iStatus = xllm_memory_sync_workspace(pMemory, &tWorkspaceOptions, &tSyncResult, &tError);
    if ( iStatus != XRT_NET_OK ||
         tSyncResult.uRemovedRecordCount != 1u ||
         xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) != FILE_COUNT ||
         !find_removed(&tSyncResult, "workspace://src/file_06.c") ) {
        fprintf(stderr, "unexpected delete sync result\n");
        return 12;
    }

    if ( rename("build\\smoke_memory_workspace_long_run_tmp\\src\\file_08.c",
                "build\\smoke_memory_workspace_long_run_tmp\\src\\renamed_08.c") != 0 ) {
        fprintf(stderr, "failed to rename file\n");
        return 13;
    }
    xllm_memory_sync_workspace_result_reset(&tSyncResult);
    iStatus = xllm_memory_sync_workspace(pMemory, &tWorkspaceOptions, &tSyncResult, &tError);
    if ( iStatus != XRT_NET_OK ||
         tSyncResult.tIngest.uCreatedRecordCount != 1u ||
         tSyncResult.uRemovedRecordCount != 1u ||
         xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) != FILE_COUNT ||
         !find_record(tSyncResult.tIngest.pCreatedRecords,
                      tSyncResult.tIngest.iCreatedDetailCount,
                      "workspace://src/renamed_08.c") ||
         !find_removed(&tSyncResult, "workspace://src/file_08.c") ) {
        fprintf(stderr, "unexpected rename sync result\n");
        return 14;
    }

    destroy_runtime_memory(&pRuntime, &pMemory);
    iStatus = create_runtime_memory(sNamespace, sDbPath, &pRuntime, &pMemory);
    if ( iStatus != XRT_NET_OK ||
         xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) != FILE_COUNT ) {
        fprintf(stderr, "unexpected reopened memory state\n");
        return 15;
    }
    if ( search_top_source(pMemory,
                           "workspace_file_08 seed 08",
                           "workspace://src/renamed_08.c",
                           &tError) != 0 ) {
        fprintf(stderr, "reopened search did not find renamed file\n");
        return 16;
    }

    printf("smoke_memory_workspace_long_run ok\n");

    xllm_memory_sync_workspace_result_reset(&tSyncResult);
    destroy_runtime_memory(&pRuntime, &pMemory);
    xllm_error_free(&tError);
    return 0;
}
