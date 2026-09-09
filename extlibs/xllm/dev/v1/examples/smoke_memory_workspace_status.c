#include <stdio.h>
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

static int require_true(int bCondition, const char *sMessage)
{
    if ( !bCondition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_workspace_options tWorkspaceOptions;
    xllm_memory_ingest_directory_result tIngestResult;
    xllm_memory_ingest_options tTextOptions;
    xllm_memory_workspace_status_options tStatusOptions;
    xllm_memory_workspace_status tStatus;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sRootDir = "build\\smoke_memory_workspace_status_tmp";
    const char *sSrcDir = "build\\smoke_memory_workspace_status_tmp\\src";
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_workspace_options_init(&tWorkspaceOptions);
    xllm_memory_ingest_directory_result_init(&tIngestResult);
    xllm_memory_ingest_options_init(&tTextOptions);
    xllm_memory_workspace_status_options_init(&tStatusOptions);
    xllm_memory_workspace_status_init(&tStatus);

    (void)xrtDirDelete((str)sRootDir);
    if ( !xrtDirCreateAll((str)sSrcDir) ) {
        fprintf(stderr, "failed to prepare workspace status directory\n");
        return 1;
    }
    if ( write_text_file("build\\smoke_memory_workspace_status_tmp\\README.md", "workspace status docs\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_status_tmp\\src\\main.c", "int main(void) { return 0; }\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_status_tmp\\secret_token.txt", "token should be skipped\n") != 0 ) {
        fprintf(stderr, "failed to write workspace status files\n");
        return 2;
    }

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-workspace-status";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tWorkspaceOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tWorkspaceOptions.sPath = sRootDir;
    tWorkspaceOptions.sRecordIdPrefix = "workspace-status";
    tWorkspaceOptions.sSourceUriPrefix = "workspace://status/";
    tWorkspaceOptions.bRecursive = true;
    iStatus = xllm_memory_ingest_workspace(pMemory, &tWorkspaceOptions, &tIngestResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "workspace ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tIngestResult.uVisitedFileCount == 3u, "expected three visited workspace files") != 0 ||
         require_true(tIngestResult.uIngestedFileCount == 2u, "expected two indexed workspace files") != 0 ||
         require_true(tIngestResult.uSkippedFileCount == 1u, "expected one skipped sensitive file") != 0 ) {
        goto cleanup;
    }

    tTextOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tTextOptions.sRecordId = "non-workspace";
    tTextOptions.sTitle = "non workspace";
    tTextOptions.sSourceUri = "memory://manual/non-workspace";
    tTextOptions.sText = "manual knowledge record without workspace file metadata";
    iStatus = xllm_memory_ingest_text(pMemory, &tTextOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "manual ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tStatusOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tStatusOptions.sRootPath = sRootDir;
    tStatusOptions.sSourceUriPrefix = "workspace://status/";
    iStatus = xllm_memory_get_workspace_status(pMemory, &tStatusOptions, &tStatus, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "workspace status failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tStatus.bRootPathFilterSet, "expected root path filter set") != 0 ||
         require_true(tStatus.bSourceUriPrefixFilterSet, "expected source uri prefix filter set") != 0 ||
         require_true(tStatus.iRecordCount == 2u, "expected two workspace records") != 0 ||
         require_true(tStatus.iChunkCount == 2u, "expected two workspace chunks") != 0 ||
         require_true(tStatus.uTotalFileBytes > 0u, "expected workspace bytes") != 0 ||
         require_true(tStatus.iOldestMtimeUnix > 0 && tStatus.iNewestMtimeUnix >= tStatus.iOldestMtimeUnix, "expected mtime range") != 0 ||
         require_true(tStatus.iSensitiveRecordCount == 0u, "expected no sensitive indexed records") != 0 ||
         require_true(tStatus.iUntrustedRecordCount == 0u, "expected no untrusted indexed records") != 0 ) {
        goto cleanup;
    }

    xllm_memory_workspace_status_options_init(&tStatusOptions);
    tStatusOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    iStatus = xllm_memory_get_workspace_status(pMemory, &tStatusOptions, &tStatus, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "global workspace status failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tStatus.iRecordCount == 3u, "expected global knowledge record count") != 0 ||
         require_true(tStatus.iMissingPathRecordCount == 1u, "expected one manual missing-path record") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_workspace_status ok\n");
    iRc = 0;

cleanup:
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
