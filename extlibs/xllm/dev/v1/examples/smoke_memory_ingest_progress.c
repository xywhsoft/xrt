#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

typedef struct {
    uint32 uVisited;
    uint32 uIngested;
    uint32 uSkipped;
    uint32 uFailed;
    uint32 uLastVisitedTotal;
    uint32 uLastIngestedTotal;
    uint32 uLastSkippedTotal;
    int bSawSourceUri;
    int bAbortOnFirstVisited;
} progress_state;

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

static int on_progress(void *pCtx, const xllm_memory_ingest_progress *pProgress)
{
    progress_state *pState = (progress_state *)pCtx;

    if ( !pState || !pProgress || !pProgress->sPath || !pProgress->sRelativePath ) {
        return 1;
    }
    if ( pProgress->eKind == XLLM_MEMORY_INGEST_PROGRESS_VISITED ) {
        ++pState->uVisited;
        pState->uLastVisitedTotal = pProgress->uVisitedFileCount;
        if ( pState->bAbortOnFirstVisited ) {
            return 1;
        }
    } else if ( pProgress->eKind == XLLM_MEMORY_INGEST_PROGRESS_INGESTED ) {
        ++pState->uIngested;
        pState->uLastIngestedTotal = pProgress->uIngestedFileCount;
        if ( pProgress->sSourceUri && strstr(pProgress->sSourceUri, "workspace://progress/") == pProgress->sSourceUri ) {
            pState->bSawSourceUri = 1;
        }
    } else if ( pProgress->eKind == XLLM_MEMORY_INGEST_PROGRESS_SKIPPED ) {
        ++pState->uSkipped;
        pState->uLastSkippedTotal = pProgress->uSkippedFileCount;
        if ( pProgress->eSkipReason == 0 ) {
            return 1;
        }
    } else if ( pProgress->eKind == XLLM_MEMORY_INGEST_PROGRESS_FAILED ) {
        ++pState->uFailed;
        if ( pProgress->eFailReason == 0 ) {
            return 1;
        }
    } else {
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
    xllm_memory_ingest_directory_result tResult;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    progress_state tProgress;
    const char *sRootDir = "build\\smoke_memory_ingest_progress_tmp";
    const char *sAbortDir = "build\\smoke_memory_ingest_progress_abort_tmp";
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    memset(&tProgress, 0, sizeof(tProgress));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_workspace_options_init(&tWorkspaceOptions);
    xllm_memory_ingest_directory_result_init(&tResult);

    (void)xrtDirDelete((str)sRootDir);
    (void)xrtDirDelete((str)sAbortDir);
    if ( !xrtDirCreateAll((str)sRootDir) || !xrtDirCreateAll((str)sAbortDir) ) {
        fprintf(stderr, "failed to prepare progress directories\n");
        return 1;
    }
    if ( write_text_file("build\\smoke_memory_ingest_progress_tmp\\README.md", "progress docs\n") != 0 ||
         write_text_file("build\\smoke_memory_ingest_progress_tmp\\main.c", "int main(void) { return 0; }\n") != 0 ||
         write_text_file("build\\smoke_memory_ingest_progress_tmp\\token_secret.txt", "secret should skip\n") != 0 ||
         write_text_file("build\\smoke_memory_ingest_progress_abort_tmp\\README.md", "abort docs\n") != 0 ) {
        fprintf(stderr, "failed to write progress files\n");
        return 2;
    }

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-ingest-progress";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tWorkspaceOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tWorkspaceOptions.sPath = sRootDir;
    tWorkspaceOptions.sRecordIdPrefix = "progress";
    tWorkspaceOptions.sSourceUriPrefix = "workspace://progress/";
    tWorkspaceOptions.pfnProgress = on_progress;
    tWorkspaceOptions.pProgressCtx = &tProgress;
    iStatus = xllm_memory_ingest_workspace(pMemory, &tWorkspaceOptions, &tResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "workspace ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tProgress.uVisited == 3u, "expected three visited progress events") != 0 ||
         require_true(tProgress.uIngested == 2u, "expected two ingested progress events") != 0 ||
         require_true(tProgress.uSkipped == 1u, "expected one skipped progress event") != 0 ||
         require_true(tProgress.uFailed == 0u, "expected no failed progress events") != 0 ||
         require_true(tProgress.uLastVisitedTotal == 3u, "expected visited total propagated") != 0 ||
         require_true(tProgress.uLastIngestedTotal == 2u, "expected ingested total propagated") != 0 ||
         require_true(tProgress.uLastSkippedTotal == 1u, "expected skipped total propagated") != 0 ||
         require_true(tProgress.bSawSourceUri, "expected ingested source uri in progress") != 0 ) {
        goto cleanup;
    }

    xllm_memory_ingest_directory_result_reset(&tResult);
    memset(&tProgress, 0, sizeof(tProgress));
    tProgress.bAbortOnFirstVisited = 1;
    xllm_memory_ingest_workspace_options_init(&tWorkspaceOptions);
    tWorkspaceOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tWorkspaceOptions.sPath = sAbortDir;
    tWorkspaceOptions.pfnProgress = on_progress;
    tWorkspaceOptions.pProgressCtx = &tProgress;
    iStatus = xllm_memory_ingest_workspace(pMemory, &tWorkspaceOptions, &tResult, &tError);
    if ( iStatus == XRT_NET_OK ) {
        fprintf(stderr, "expected progress abort to fail ingest\n");
        goto cleanup;
    }
    if ( require_true(tProgress.uVisited >= 1u, "expected abort progress visit") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_ingest_progress ok\n");
    iRc = 0;

cleanup:
    xllm_memory_ingest_directory_result_reset(&tResult);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
