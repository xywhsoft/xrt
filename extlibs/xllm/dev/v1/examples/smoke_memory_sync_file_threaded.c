#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xllm-memory.h"

typedef struct {
    xllm_memory *pMemory;
    xllm_memory_sync_file_options tOptions;
    xllm_memory_change_set tChanges;
    xllm_error tError;
    str sPath;
    str sRootPath;
    str sRecordIdPrefix;
    str sSourceUriPrefix;
    str sAllowedExtensions;
} demo_sync_file_task;

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

static void demo_sync_file_task_destroy(demo_sync_file_task *pTask)
{
    if ( !pTask ) {
        return;
    }

    xllm_memory_change_set_reset(&pTask->tChanges);
    xllm_error_free(&pTask->tError);
    if ( pTask->sPath ) {
        xrtFree(pTask->sPath);
    }
    if ( pTask->sRootPath ) {
        xrtFree(pTask->sRootPath);
    }
    if ( pTask->sRecordIdPrefix ) {
        xrtFree(pTask->sRecordIdPrefix);
    }
    if ( pTask->sSourceUriPrefix ) {
        xrtFree(pTask->sSourceUriPrefix);
    }
    if ( pTask->sAllowedExtensions ) {
        xrtFree(pTask->sAllowedExtensions);
    }
    xrtFree(pTask);
}

static int32 demo_sync_file_task_run(ptr pArg, xfuture_result *pOut)
{
    demo_sync_file_task *pTask = (demo_sync_file_task *)pArg;
    int32 iStatus;

    if ( !pTask || !pOut ) {
        demo_sync_file_task_destroy(pTask);
        return XRT_NET_ERROR;
    }

    iStatus = xllm_memory_sync_file(pTask->pMemory, &pTask->tOptions, &pTask->tChanges, &pTask->tError);
    if ( iStatus != XRT_NET_OK ) {
        memset(pOut, 0, sizeof(*pOut));
        pOut->iStatus = iStatus;
        pOut->sError = pTask->tError.sMessage
            ? xrtCopyStr((str)pTask->tError.sMessage, 0u)
            : xrtCopyStr((str)"threaded sync_file failed", 0u);
        pOut->iFlags = XFUTURE_RESULT_F_OWN_ERROR;
        demo_sync_file_task_destroy(pTask);
        return iStatus;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->iStatus = XRT_NET_OK;
    pOut->pValue = pTask;
    pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
    return XRT_NET_OK;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tListResult;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    xfuture *pFuture = NULL;
    demo_sync_file_task *pTask = NULL;
    const xllm_memory_record_info *pRecord;
    const char *sPathValue;
    const char *sRelativePathValue;
    const char *sBasenameValue;
    const char *sExtensionValue;
    const char *sRootDir = "build\\smoke_memory_sync_file_threaded_tmp";
    const char *sFilePath = "build\\smoke_memory_sync_file_threaded_tmp\\main.c";
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_list_options_init(&tListOptions);
    memset(&tListResult, 0, sizeof(tListResult));

    if ( !xrtDirCreateAll((str)sRootDir) ) {
        fprintf(stderr, "failed to create temp dir\n");
        return 1;
    }
    if ( write_text_file(sFilePath, "int main(void) { return 0; }\n") != 0 ) {
        fprintf(stderr, "failed to write temp file\n");
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

    tMemoryOptions.sNamespace = "smoke-sync-file-threaded";
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

    pTask = (demo_sync_file_task *)xrtCalloc(1u, sizeof(*pTask));
    if ( !pTask ) {
        fprintf(stderr, "failed to allocate threaded sync task\n");
        return 6;
    }

    pTask->pMemory = pMemory;
    xllm_memory_sync_file_options_init(&pTask->tOptions);
    xllm_memory_change_set_init(&pTask->tChanges);
    xllm_error_init(&pTask->tError);
    pTask->sPath = xrtCopyStr((str)sFilePath, 0u);
    pTask->sRootPath = xrtCopyStr((str)sRootDir, 0u);
    pTask->sRecordIdPrefix = xrtCopyStr((str)"workspace", 0u);
    pTask->sSourceUriPrefix = xrtCopyStr((str)"workspace://", 0u);
    pTask->sAllowedExtensions = xrtCopyStr((str)".c", 0u);
    if ( !pTask->sPath || !pTask->sRootPath || !pTask->sRecordIdPrefix || !pTask->sSourceUriPrefix || !pTask->sAllowedExtensions ) {
        fprintf(stderr, "failed to allocate threaded sync options\n");
        return 7;
    }

    pTask->tOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    pTask->tOptions.sPath = (const char *)pTask->sPath;
    pTask->tOptions.sRootPath = (const char *)pTask->sRootPath;
    pTask->tOptions.sRecordIdPrefix = (const char *)pTask->sRecordIdPrefix;
    pTask->tOptions.sSourceUriPrefix = (const char *)pTask->sSourceUriPrefix;
    pTask->tOptions.bUseWorkspaceDefaults = true;
    pTask->tOptions.sAllowedExtensions = (const char *)pTask->sAllowedExtensions;
    pTask->tOptions.uMaxFileBytes = 2048u;

    pFuture = xTaskRunThread(demo_sync_file_task_run, pTask, 0u);
    if ( !pFuture ) {
        fprintf(stderr, "failed to create threaded sync future\n");
        return 8;
    }
    pTask = (demo_sync_file_task *)xFutureWaitValue(pFuture);
    if ( xFutureStatus(pFuture) != XRT_NET_OK || !pTask ) {
        fprintf(stderr, "threaded sync future failed: %d\n", (int)xFutureStatus(pFuture));
        return 9;
    }
    if ( require_true(pTask->tChanges.iChangeCount == 1u, "threaded sync_file should produce one change") != 0 ||
         require_true(pTask->tChanges.pChanges[0].eKind == XLLM_MEMORY_CHANGE_CREATED, "threaded sync_file should create a record") != 0 ) {
        return 10;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tListOptions.sRecordId = pTask->tChanges.pChanges[0].tRecord.sRecordId;
    tListOptions.uMaxItems = 4u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tListResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory list threaded record failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 11;
    }
    if ( require_true(tListResult.iRecordCount == 1u, "threaded sync_file should persist one record") != 0 ) {
        return 12;
    }

    pRecord = &tListResult.pRecords[0];
    if ( require_true(pRecord->tMetadata && xvoType(pRecord->tMetadata) == XVO_DT_TABLE, "threaded sync metadata missing") != 0 ) {
        return 13;
    }

    sPathValue = (const char *)xvoTableGetText(pRecord->tMetadata, (str)"path", 0u);
    sRelativePathValue = (const char *)xvoTableGetText(pRecord->tMetadata, (str)"relative_path", 0u);
    sBasenameValue = (const char *)xvoTableGetText(pRecord->tMetadata, (str)"basename", 0u);
    sExtensionValue = (const char *)xvoTableGetText(pRecord->tMetadata, (str)"extension", 0u);
    if ( require_true(sPathValue && strcmp(sPathValue, sFilePath) == 0, "threaded sync path metadata mismatch") != 0 ||
         require_true(sRelativePathValue && strcmp(sRelativePathValue, "main.c") == 0, "threaded sync relative_path metadata mismatch") != 0 ||
         require_true(sBasenameValue && strcmp(sBasenameValue, "main.c") == 0, "threaded sync basename metadata mismatch") != 0 ||
         require_true(sExtensionValue && strcmp(sExtensionValue, ".c") == 0, "threaded sync extension metadata mismatch") != 0 ) {
        return 14;
    }

    printf("smoke_memory_sync_file_threaded ok\n");

    xllm_memory_record_list_result_reset(&tListResult);
    if ( pTask ) {
        demo_sync_file_task_destroy(pTask);
    }
    if ( pFuture ) {
        xFutureRelease(pFuture);
    }
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
