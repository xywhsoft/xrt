XLLM_API int xllm_memory_ingest_file(
    xllm_memory *pMemory,
    const xllm_memory_ingest_file_options *pOptions,
    xllm_error *pError
)
{
    xllm_memory_ingest_file_options tDefaultOptions;
    const xllm_memory_ingest_file_options *pUseOptions = pOptions;
    xllm_memory_ingest_options tTextOptions;
    char *sText = NULL;
    char *sRecordId = NULL;
    char *sTitle = NULL;
    char *sSourceUri = NULL;
    xvalue tMetadata = 0;
    size_t iSize = 0u;
    uint64 uContentHash = 0u;
    int iStatus;

    if ( !pMemory ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory handle is required");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_ingest_file_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( !pUseOptions->sPath || !pUseOptions->sPath[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory ingest file requires a path");
        return XRT_NET_ERROR;
    }

    iStatus = xllm__memory_read_text_file(pUseOptions->sPath, pUseOptions->uMaxFileBytes, &sText, &iSize, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }
    if ( iSize == 0u ) {
        xrtFree(sText);
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory ingest file is empty");
        return XRT_NET_ERROR;
    }
    if ( xllm__memory_text_contains_secret_pattern(sText, iSize) ) {
        xrtFree(sText);
        xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "memory ingest file rejected by secret scanner");
        return XRT_NET_ERROR;
    }
    uContentHash = xllm__memory_hash_bytes(sText, iSize);

    if ( pUseOptions->sRecordId && pUseOptions->sRecordId[0] ) {
        sRecordId = xllm__dup_cstr(pUseOptions->sRecordId);
    } else {
        iStatus = xllm__memory_make_file_record_id(pUseOptions->sPath, &sRecordId);
        if ( iStatus != XRT_NET_OK ) {
            xrtFree(sText);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build memory file record id");
            return XRT_NET_ERROR;
        }
    }
    sTitle = (pUseOptions->sTitle && pUseOptions->sTitle[0])
        ? xllm__dup_cstr(pUseOptions->sTitle)
        : xllm__memory_dup_file_basename(pUseOptions->sPath);
    if ( pUseOptions->sSourceUri && pUseOptions->sSourceUri[0] ) {
        sSourceUri = xllm__dup_cstr(pUseOptions->sSourceUri);
    } else {
        sSourceUri = xllm__memory_make_file_source_uri(pUseOptions->sPath);
    }
    if ( !sRecordId || !sTitle || !sSourceUri ) {
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        xrtFree(sText);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to prepare memory file ingest metadata");
        return XRT_NET_ERROR;
    }

    if ( xllm__memory_make_file_content_metadata(pUseOptions->tMetadata, uContentHash, &tMetadata) != XRT_NET_OK ) {
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        xrtFree(sText);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to prepare memory file content metadata");
        return XRT_NET_ERROR;
    }

    xllm_memory_ingest_options_init(&tTextOptions);
    tTextOptions.eScope = pUseOptions->eScope;
    tTextOptions.sRecordId = sRecordId;
    tTextOptions.sTitle = sTitle;
    tTextOptions.sSourceUri = sSourceUri;
    tTextOptions.sText = sText;
    tTextOptions.bReplaceExisting = pUseOptions->bReplaceExisting;
    tTextOptions.uChunkChars = pUseOptions->uChunkChars;
    tTextOptions.uChunkOverlapChars = pUseOptions->uChunkOverlapChars;
    tTextOptions.tMetadata = tMetadata;
    tTextOptions.tVendorExtra = pUseOptions->tVendorExtra;
    iStatus = xllm_memory_ingest_text(pMemory, &tTextOptions, pError);

    xvoUnref(tMetadata);
    xllm__free_cstr(&sRecordId);
    xllm__free_cstr(&sTitle);
    xllm__free_cstr(&sSourceUri);
    if ( sText ) {
        xrtFree(sText);
    }
    return iStatus;
}

XLLM_API int xllm_memory_ingest_directory(
    xllm_memory *pMemory,
    const xllm_memory_ingest_directory_options *pOptions,
    xllm_memory_ingest_directory_result *pResult,
    xllm_error *pError
)
{
    xllm_memory_ingest_directory_options tDefaultOptions;
    const xllm_memory_ingest_directory_options *pUseOptions = pOptions;
    xllm_memory_ingest_directory_result tLocalResult;
    xllm_memory_ingest_directory_result *pUseResult = pResult;
    xllm__memory_directory_ingest_state tState;

    if ( !pMemory ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory handle is required");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_ingest_directory_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( !pUseOptions->sPath || !pUseOptions->sPath[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory ingest directory requires a path");
        return XRT_NET_ERROR;
    }
    if ( !xrtDirExists((str)pUseOptions->sPath) ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory ingest directory path does not exist");
        return XRT_NET_ERROR;
    }

    if ( !pUseResult ) {
        pUseResult = &tLocalResult;
        xllm_memory_ingest_directory_result_init(pUseResult);
    } else {
        xllm_memory_ingest_directory_result_reset(pUseResult);
    }

    memset(&tState, 0, sizeof(tState));
    tState.pMemory = pMemory;
    tState.pOptions = pUseOptions;
    tState.pResult = pUseResult;
    tState.sRootPath = pUseOptions->sPath;

    xrtDirScan(
        (str)pUseOptions->sPath,
        pUseOptions->bRecursive ? 1 : 0,
        xllm__memory_ingest_directory_callback,
        &tState
    );

    if ( tState.bProgressAborted ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory ingest directory aborted by progress callback");
        return XRT_NET_ERROR;
    }
    if ( pUseResult->uIngestedFileCount == 0u && pUseResult->uFailedFileCount > 0u ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory ingest directory completed without ingesting any files");
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_ingest_workspace(
    xllm_memory *pMemory,
    const xllm_memory_ingest_workspace_options *pOptions,
    xllm_memory_ingest_directory_result *pResult,
    xllm_error *pError
)
{
    xllm_memory_ingest_workspace_options tDefaultOptions;
    const xllm_memory_ingest_workspace_options *pUseOptions = pOptions;
    xllm_memory_ingest_directory_options tDirectoryOptions;
    char *sMergedIgnoredPatterns = NULL;
    int iStatus;

    if ( !pUseOptions ) {
        xllm_memory_ingest_workspace_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    xllm_memory_ingest_directory_options_init(&tDirectoryOptions);
    tDirectoryOptions.eScope = pUseOptions->eScope;
    tDirectoryOptions.sPath = pUseOptions->sPath;
    tDirectoryOptions.sRecordIdPrefix = pUseOptions->sRecordIdPrefix;
    tDirectoryOptions.sAllowedExtensions = pUseOptions->sAllowedExtensions;
    tDirectoryOptions.bRecursive = pUseOptions->bRecursive;
    tDirectoryOptions.bReplaceExisting = pUseOptions->bReplaceExisting;
    tDirectoryOptions.bSkipHidden = pUseOptions->bSkipHidden;
    tDirectoryOptions.bUseWorkspaceDefaults = true;
    tDirectoryOptions.bSkipUnchanged = pUseOptions->bSkipUnchanged;
    tDirectoryOptions.sIgnoredDirectories = pUseOptions->sIgnoredDirectories;
    tDirectoryOptions.sIgnoredExtensions = pUseOptions->sIgnoredExtensions;
    tDirectoryOptions.sSourceUriPrefix = pUseOptions->sSourceUriPrefix;
    tDirectoryOptions.uMaxFileBytes = pUseOptions->uMaxFileBytes;
    tDirectoryOptions.uChunkChars = pUseOptions->uChunkChars;
    tDirectoryOptions.uChunkOverlapChars = pUseOptions->uChunkOverlapChars;
    tDirectoryOptions.pfnProgress = pUseOptions->pfnProgress;
    tDirectoryOptions.pProgressCtx = pUseOptions->pProgressCtx;
    tDirectoryOptions.tMetadata = pUseOptions->tMetadata;
    tDirectoryOptions.tVendorExtra = pUseOptions->tVendorExtra;
    iStatus = xllm__memory_build_workspace_ignore_patterns(pUseOptions, &sMergedIgnoredPatterns, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }
    tDirectoryOptions.sIgnoredPathPatterns = sMergedIgnoredPatterns ? sMergedIgnoredPatterns : pUseOptions->sIgnoredPathPatterns;
    iStatus = xllm_memory_ingest_directory(pMemory, &tDirectoryOptions, pResult, pError);
    if ( sMergedIgnoredPatterns ) {
        xrtFree(sMergedIgnoredPatterns);
    }
    return iStatus;
}

XLLM_API int xllm_memory_make_change_set_from_ingest(
    const xllm_memory_ingest_directory_result *pIngest,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    size_t i;

    if ( !pIngest || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory change set requires ingest result and output");
        return XRT_NET_ERROR;
    }

    xllm_memory_change_set_reset(pResult);

    for ( i = 0u; i < pIngest->iCreatedDetailCount; ++i ) {
        if ( xllm__memory_append_change_record(&pResult->pChanges,
                                               &pResult->iChangeCount,
                                               XLLM_MEMORY_CHANGE_CREATED,
                                               &pIngest->pCreatedRecords[i]) != XRT_NET_OK ) {
            xllm_memory_change_set_reset(pResult);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build created change set entries");
            return XRT_NET_ERROR;
        }
    }
    for ( i = 0u; i < pIngest->iUpdatedDetailCount; ++i ) {
        if ( xllm__memory_append_change_record(&pResult->pChanges,
                                               &pResult->iChangeCount,
                                               XLLM_MEMORY_CHANGE_UPDATED,
                                               &pIngest->pUpdatedRecords[i]) != XRT_NET_OK ) {
            xllm_memory_change_set_reset(pResult);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build updated change set entries");
            return XRT_NET_ERROR;
        }
    }
    for ( i = 0u; i < pIngest->iSkippedDetailCount; ++i ) {
        if ( xllm__memory_append_change_skipped(&pResult->pChanges,
                                                &pResult->iChangeCount,
                                                &pIngest->pSkippedFiles[i]) != XRT_NET_OK ) {
            xllm_memory_change_set_reset(pResult);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build skipped change set entries");
            return XRT_NET_ERROR;
        }
    }
    for ( i = 0u; i < pIngest->iFailedDetailCount; ++i ) {
        if ( xllm__memory_append_change_failed(&pResult->pChanges,
                                               &pResult->iChangeCount,
                                               &pIngest->pFailedFiles[i]) != XRT_NET_OK ) {
            xllm_memory_change_set_reset(pResult);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build failed change set entries");
            return XRT_NET_ERROR;
        }
    }

    return XRT_NET_OK;
}

XLLM_API int xllm_memory_make_change_set_from_sync(
    const xllm_memory_sync_workspace_result *pSync,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    size_t i;
    int iStatus;

    if ( !pSync || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory change set requires sync result and output");
        return XRT_NET_ERROR;
    }

    iStatus = xllm_memory_make_change_set_from_ingest(&pSync->tIngest, pResult, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    for ( i = 0u; i < pSync->iRemovedDetailCount; ++i ) {
        if ( xllm__memory_append_change_record(&pResult->pChanges,
                                               &pResult->iChangeCount,
                                               XLLM_MEMORY_CHANGE_REMOVED,
                                               &pSync->pRemovedRecords[i]) != XRT_NET_OK ) {
            xllm_memory_change_set_reset(pResult);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build removed change set entries");
            return XRT_NET_ERROR;
        }
    }

    return XRT_NET_OK;
}

XLLM_API int xllm_memory_sync_file(
    xllm_memory *pMemory,
    const xllm_memory_sync_file_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    xllm_memory_sync_file_options tDefaultOptions;
    const xllm_memory_sync_file_options *pUseOptions = pOptions;
    xllm_memory_sync_workspace_result tSyncResult;
    xllm_memory_ingest_file_options tFileOptions;
    const char *sAllowedExtensions;
    const char *sIgnoredDirectories;
    const char *sIgnoredExtensions;
    const char *sIgnoredPathPatterns;
    const char *sSourceUriPrefix;
    uint64 uEffectiveMaxFileBytes;
    const char *sRelativePath = NULL;
    char *sRecordId = NULL;
    char *sTitle = NULL;
    char *sSourceUri = NULL;
    xvalue tMetadata = 0;
    uint64 uFileSize = 0u;
    int64 iMtimeUnix = 0;
    size_t iExistingByRecordId = (size_t)-1;
    bool bHadExistingRecord = false;
    bool bSecretDetected = false;
    size_t i;
    int iStatus;
    struct _stat64 tStat;

    if ( !pMemory || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory sync file requires memory handle and output");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_sync_file_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( !pUseOptions->sPath || !pUseOptions->sPath[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory sync file requires a path");
        return XRT_NET_ERROR;
    }

    xllm_memory_change_set_reset(pResult);
    xllm_memory_sync_workspace_result_init(&tSyncResult);
    xllm_memory_ingest_file_options_init(&tFileOptions);

    sAllowedExtensions = pUseOptions->sAllowedExtensions;
    sIgnoredDirectories = pUseOptions->sIgnoredDirectories;
    sIgnoredExtensions = pUseOptions->sIgnoredExtensions;
    sIgnoredPathPatterns = pUseOptions->sIgnoredPathPatterns;
    sSourceUriPrefix = pUseOptions->sSourceUriPrefix;
    uEffectiveMaxFileBytes = pUseOptions->uMaxFileBytes;
    if ( pUseOptions->bUseWorkspaceDefaults ) {
        if ( !sAllowedExtensions || !sAllowedExtensions[0] ) {
            sAllowedExtensions = XLLM__MEMORY_WORKSPACE_ALLOWED_EXTENSIONS;
        }
        if ( !sIgnoredDirectories || !sIgnoredDirectories[0] ) {
            sIgnoredDirectories = XLLM__MEMORY_WORKSPACE_IGNORED_DIRECTORIES;
        }
        if ( !sIgnoredExtensions || !sIgnoredExtensions[0] ) {
            sIgnoredExtensions = XLLM__MEMORY_WORKSPACE_IGNORED_EXTENSIONS;
        }
        if ( !sIgnoredPathPatterns || !sIgnoredPathPatterns[0] ) {
            sIgnoredPathPatterns = XLLM__MEMORY_WORKSPACE_IGNORED_PATH_PATTERNS;
        }
        if ( !sSourceUriPrefix || !sSourceUriPrefix[0] ) {
            sSourceUriPrefix = XLLM__MEMORY_WORKSPACE_DEFAULT_SOURCE_URI_PREFIX;
        }
        if ( uEffectiveMaxFileBytes == 0u ) {
            uEffectiveMaxFileBytes = XLLM__MEMORY_WORKSPACE_DEFAULT_MAX_FILE_BYTES;
        }
    }

    if ( pUseOptions->sRootPath && pUseOptions->sRootPath[0] &&
         xllm__memory_path_is_under_root_ci(pUseOptions->sRootPath, pUseOptions->sPath, &sRelativePath) ) {
        if ( !sRelativePath || !sRelativePath[0] ) {
            sRelativePath = pUseOptions->sPath;
        }
    } else {
        sRelativePath = pUseOptions->sPath;
    }

    if ( pUseOptions->sRecordId && pUseOptions->sRecordId[0] ) {
        sRecordId = xllm__dup_cstr(pUseOptions->sRecordId);
    } else if ( pUseOptions->sRecordIdPrefix && pUseOptions->sRecordIdPrefix[0] ) {
        iStatus = xllm__memory_make_file_record_id_prefixed(pUseOptions->sRecordIdPrefix, pUseOptions->sPath, &sRecordId);
        if ( iStatus != XRT_NET_OK ) {
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build memory sync file record id");
            return XRT_NET_ERROR;
        }
    } else {
        iStatus = xllm__memory_make_file_record_id(pUseOptions->sPath, &sRecordId);
        if ( iStatus != XRT_NET_OK ) {
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build memory sync file record id");
            return XRT_NET_ERROR;
        }
    }

    if ( pUseOptions->sTitle && pUseOptions->sTitle[0] ) {
        sTitle = xllm__dup_cstr(pUseOptions->sTitle);
    } else if ( pUseOptions->sRootPath && pUseOptions->sRootPath[0] ) {
        sTitle = xllm__memory_dup_relative_path(pUseOptions->sRootPath, pUseOptions->sPath);
    } else {
        sTitle = xllm__memory_dup_file_basename(pUseOptions->sPath);
    }

    if ( pUseOptions->sSourceUri && pUseOptions->sSourceUri[0] ) {
        sSourceUri = xllm__dup_cstr(pUseOptions->sSourceUri);
    } else if ( sSourceUriPrefix && sSourceUriPrefix[0] ) {
        sSourceUri = xllm__memory_make_prefixed_relative_source_uri(
            sSourceUriPrefix,
            pUseOptions->sRootPath,
            pUseOptions->sPath
        );
    } else {
        sSourceUri = xllm__memory_make_file_source_uri(pUseOptions->sPath);
    }

    if ( !sRecordId || !sTitle || !sSourceUri ) {
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to prepare memory sync file metadata");
        return XRT_NET_ERROR;
    }

    xrtMutexLock(pMemory->pMutex);
    iExistingByRecordId = xllm__memory_find_record_index_locked(pMemory, pUseOptions->eScope, sRecordId);
    xrtMutexUnlock(pMemory->pMutex);
    bHadExistingRecord = (iExistingByRecordId != (size_t)-1);

    if ( _stat64(pUseOptions->sPath, &tStat) != 0 || !(tStat.st_mode & _S_IFREG) ) {
        xrtMutexLock(pMemory->pMutex);
        for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
            bool bScopeMatches = (pMemory->pRecords[i].eScope == pUseOptions->eScope);
            bool bMatch = false;
            if ( !bScopeMatches ) {
                continue;
            }
            if ( sSourceUri && pMemory->pRecords[i].sSourceUri &&
                 strcmp(pMemory->pRecords[i].sSourceUri, sSourceUri) == 0 ) {
                bMatch = true;
            } else if ( sRecordId && pMemory->pRecords[i].sRecordId &&
                        strcmp(pMemory->pRecords[i].sRecordId, sRecordId) == 0 ) {
                bMatch = true;
            }
            if ( bMatch ) {
                (void)xllm__memory_append_record_info_clone(
                    &tSyncResult.pRemovedRecords,
                    &tSyncResult.iRemovedDetailCount,
                    &pMemory->pRecords[i]
                );
            }
        }
        xrtMutexUnlock(pMemory->pMutex);

        if ( tSyncResult.iRemovedDetailCount > 0u ) {
            uint32 uRemovedCount = 0u;
            iStatus = xllm_memory_remove_by_source_uri(pMemory, pUseOptions->eScope, sSourceUri, &uRemovedCount, pError);
            if ( iStatus == XRT_NET_OK && uRemovedCount == 0u && bHadExistingRecord ) {
                iStatus = xllm_memory_remove(pMemory, pUseOptions->eScope, sRecordId, pError);
                if ( iStatus == XRT_NET_OK ) {
                    uRemovedCount = 1u;
                }
            }
            if ( iStatus != XRT_NET_OK ) {
                if ( sRecordId ) xllm__free_cstr(&sRecordId);
                if ( sTitle ) xllm__free_cstr(&sTitle);
                if ( sSourceUri ) xllm__free_cstr(&sSourceUri);
                xllm_memory_sync_workspace_result_reset(&tSyncResult);
                return iStatus;
            }
            tSyncResult.uRemovedRecordCount = uRemovedCount;
        }

        iStatus = xllm_memory_make_change_set_from_sync(&tSyncResult, pResult, pError);
        xllm_memory_sync_workspace_result_reset(&tSyncResult);
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        return iStatus;
    }

    if ( pUseOptions->bSkipHidden && xllm__memory_path_has_hidden_component(sRelativePath) ) {
        (void)xllm__memory_append_skipped_file_info(&tSyncResult.tIngest.pSkippedFiles,
                                                    &tSyncResult.tIngest.iSkippedDetailCount,
                                                    pUseOptions->sRootPath,
                                                    pUseOptions->sPath,
                                                    sRelativePath,
                                                    sSourceUriPrefix,
                                                    XLLM_MEMORY_SKIP_HIDDEN,
                                                    0u);
        ++tSyncResult.tIngest.uSkippedFileCount;
    } else if ( xllm__memory_path_has_ignored_directory(sRelativePath, sIgnoredDirectories) ) {
        (void)xllm__memory_append_skipped_file_info(&tSyncResult.tIngest.pSkippedFiles,
                                                    &tSyncResult.tIngest.iSkippedDetailCount,
                                                    pUseOptions->sRootPath,
                                                    pUseOptions->sPath,
                                                    sRelativePath,
                                                    sSourceUriPrefix,
                                                    XLLM_MEMORY_SKIP_IGNORED_DIRECTORY,
                                                    0u);
        ++tSyncResult.tIngest.uSkippedFileCount;
    } else if ( xllm__memory_path_matches_ignored_patterns(sIgnoredPathPatterns, sRelativePath) ) {
        (void)xllm__memory_append_skipped_file_info(&tSyncResult.tIngest.pSkippedFiles,
                                                    &tSyncResult.tIngest.iSkippedDetailCount,
                                                    pUseOptions->sRootPath,
                                                    pUseOptions->sPath,
                                                    sRelativePath,
                                                    sSourceUriPrefix,
                                                    XLLM_MEMORY_SKIP_IGNORED_PATH_PATTERN,
                                                    0u);
        ++tSyncResult.tIngest.uSkippedFileCount;
    } else if ( xllm__memory_extension_is_ignored(sIgnoredExtensions, sRelativePath) ) {
        (void)xllm__memory_append_skipped_file_info(&tSyncResult.tIngest.pSkippedFiles,
                                                    &tSyncResult.tIngest.iSkippedDetailCount,
                                                    pUseOptions->sRootPath,
                                                    pUseOptions->sPath,
                                                    sRelativePath,
                                                    sSourceUriPrefix,
                                                    XLLM_MEMORY_SKIP_IGNORED_EXTENSION,
                                                    0u);
        ++tSyncResult.tIngest.uSkippedFileCount;
    } else if ( !xllm__memory_extension_is_allowed(sAllowedExtensions, sRelativePath) ) {
        (void)xllm__memory_append_skipped_file_info(&tSyncResult.tIngest.pSkippedFiles,
                                                    &tSyncResult.tIngest.iSkippedDetailCount,
                                                    pUseOptions->sRootPath,
                                                    pUseOptions->sPath,
                                                    sRelativePath,
                                                    sSourceUriPrefix,
                                                    XLLM_MEMORY_SKIP_DISALLOWED_EXTENSION,
                                                    0u);
        ++tSyncResult.tIngest.uSkippedFileCount;
    } else if ( xllm__memory_get_file_size(pUseOptions->sPath, &uFileSize) != XRT_NET_OK ) {
        (void)xllm__memory_append_failed_file_info(&tSyncResult.tIngest.pFailedFiles,
                                                   &tSyncResult.tIngest.iFailedDetailCount,
                                                   pUseOptions->sRootPath,
                                                   pUseOptions->sPath,
                                                   sRelativePath,
                                                   sSourceUriPrefix,
                                                   XLLM_MEMORY_FAIL_GET_SIZE,
                                                   0u,
                                                   NULL);
        ++tSyncResult.tIngest.uFailedFileCount;
    } else if ( uEffectiveMaxFileBytes > 0u && uFileSize > uEffectiveMaxFileBytes ) {
        (void)xllm__memory_append_skipped_file_info(&tSyncResult.tIngest.pSkippedFiles,
                                                    &tSyncResult.tIngest.iSkippedDetailCount,
                                                    pUseOptions->sRootPath,
                                                    pUseOptions->sPath,
                                                    sRelativePath,
                                                    sSourceUriPrefix,
                                                    XLLM_MEMORY_SKIP_TOO_LARGE,
                                                    uFileSize);
        ++tSyncResult.tIngest.uSkippedFileCount;
    }
    if ( tSyncResult.tIngest.uSkippedFileCount == 0u && tSyncResult.tIngest.uFailedFileCount == 0u ) {
        iStatus = xllm__memory_file_contains_secret_pattern(pUseOptions->sPath, uEffectiveMaxFileBytes, &bSecretDetected, NULL);
        if ( iStatus != XRT_NET_OK ) {
            (void)xllm__memory_append_failed_file_info(&tSyncResult.tIngest.pFailedFiles,
                                                       &tSyncResult.tIngest.iFailedDetailCount,
                                                       pUseOptions->sRootPath,
                                                       pUseOptions->sPath,
                                                       sRelativePath,
                                                       sSourceUriPrefix,
                                                       XLLM_MEMORY_FAIL_INGEST,
                                                       uFileSize,
                                                       NULL);
            ++tSyncResult.tIngest.uFailedFileCount;
        } else if ( bSecretDetected ) {
            (void)xllm__memory_append_skipped_file_info(&tSyncResult.tIngest.pSkippedFiles,
                                                        &tSyncResult.tIngest.iSkippedDetailCount,
                                                        pUseOptions->sRootPath,
                                                        pUseOptions->sPath,
                                                        sRelativePath,
                                                        sSourceUriPrefix,
                                                        XLLM_MEMORY_SKIP_SECRET_DETECTED,
                                                        uFileSize);
            ++tSyncResult.tIngest.uSkippedFileCount;
        }
    }

    if ( tSyncResult.tIngest.uSkippedFileCount > 0u || tSyncResult.tIngest.uFailedFileCount > 0u ) {
        if ( tSyncResult.tIngest.uSkippedFileCount > 0u && iExistingByRecordId != (size_t)-1 ) {
            xrtMutexLock(pMemory->pMutex);
            if ( iExistingByRecordId < pMemory->iRecordCount &&
                 pMemory->pRecords[iExistingByRecordId].sRecordId &&
                 strcmp(pMemory->pRecords[iExistingByRecordId].sRecordId, sRecordId) == 0 ) {
                (void)xllm__memory_append_record_info_clone(
                    &tSyncResult.pRemovedRecords,
                    &tSyncResult.iRemovedDetailCount,
                    &pMemory->pRecords[iExistingByRecordId]
                );
            }
            xrtMutexUnlock(pMemory->pMutex);
            if ( tSyncResult.iRemovedDetailCount > 0u ) {
                iStatus = xllm_memory_remove(pMemory, pUseOptions->eScope, sRecordId, pError);
                if ( iStatus != XRT_NET_OK ) {
                    uint32 uRemovedCount = 0u;
                    iStatus = xllm_memory_remove_by_source_uri(pMemory, pUseOptions->eScope, sSourceUri, &uRemovedCount, pError);
                }
                if ( iStatus != XRT_NET_OK ) {
                    xllm_memory_sync_workspace_result_reset(&tSyncResult);
                    xllm__free_cstr(&sRecordId);
                    xllm__free_cstr(&sTitle);
                    xllm__free_cstr(&sSourceUri);
                    return iStatus;
                }
                tSyncResult.uRemovedRecordCount = (uint32)tSyncResult.iRemovedDetailCount;
            }
        }
        iStatus = xllm_memory_make_change_set_from_sync(&tSyncResult, pResult, pError);
        xllm_memory_sync_workspace_result_reset(&tSyncResult);
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        return iStatus;
    }

    if ( xllm__memory_get_file_mtime_unix(pUseOptions->sPath, &iMtimeUnix) != XRT_NET_OK ) {
        (void)xllm__memory_append_failed_file_info(&tSyncResult.tIngest.pFailedFiles,
                                                   &tSyncResult.tIngest.iFailedDetailCount,
                                                   pUseOptions->sRootPath,
                                                   pUseOptions->sPath,
                                                   sRelativePath,
                                                   sSourceUriPrefix,
                                                   XLLM_MEMORY_FAIL_GET_MTIME,
                                                   uFileSize,
                                                   NULL);
        ++tSyncResult.tIngest.uFailedFileCount;
        iStatus = xllm_memory_make_change_set_from_sync(&tSyncResult, pResult, pError);
        xllm_memory_sync_workspace_result_reset(&tSyncResult);
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        return iStatus;
    }

    tMetadata = xllm__memory_make_file_metadata(pUseOptions->tMetadata, pUseOptions->sRootPath, pUseOptions->sPath, uFileSize, iMtimeUnix);
    if ( !tMetadata ) {
        (void)xllm__memory_append_failed_file_info(&tSyncResult.tIngest.pFailedFiles,
                                                   &tSyncResult.tIngest.iFailedDetailCount,
                                                   pUseOptions->sRootPath,
                                                   pUseOptions->sPath,
                                                   sRelativePath,
                                                   sSourceUriPrefix,
                                                   XLLM_MEMORY_FAIL_BUILD_METADATA,
                                                   uFileSize,
                                                   NULL);
        ++tSyncResult.tIngest.uFailedFileCount;
        iStatus = xllm_memory_make_change_set_from_sync(&tSyncResult, pResult, pError);
        xllm_memory_sync_workspace_result_reset(&tSyncResult);
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        return iStatus;
    }

    if ( pUseOptions->bSkipUnchanged && iExistingByRecordId != (size_t)-1 ) {
        bool bUnchanged = false;
        xrtMutexLock(pMemory->pMutex);
        if ( iExistingByRecordId < pMemory->iRecordCount &&
             pMemory->pRecords[iExistingByRecordId].sRecordId &&
             strcmp(pMemory->pRecords[iExistingByRecordId].sRecordId, sRecordId) == 0 &&
             xllm__memory_record_matches_file_fingerprint(&pMemory->pRecords[iExistingByRecordId],
                                                          pUseOptions->sPath,
                                                          uFileSize,
                                                          iMtimeUnix) ) {
            bUnchanged = true;
        }
        xrtMutexUnlock(pMemory->pMutex);
        if ( !bUnchanged ) {
            bUnchanged = xllm__memory_existing_record_matches_file_content(
                pMemory,
                pUseOptions->eScope,
                sRecordId,
                pUseOptions->sPath,
                uEffectiveMaxFileBytes,
                NULL
            );
        }
        if ( bUnchanged ) {
            xvoUnref(tMetadata);
            (void)xllm__memory_append_skipped_file_info(&tSyncResult.tIngest.pSkippedFiles,
                                                        &tSyncResult.tIngest.iSkippedDetailCount,
                                                        pUseOptions->sRootPath,
                                                        pUseOptions->sPath,
                                                        sRelativePath,
                                                        sSourceUriPrefix,
                                                        XLLM_MEMORY_SKIP_UNCHANGED,
                                                        uFileSize);
            ++tSyncResult.tIngest.uSkippedFileCount;
            iStatus = xllm_memory_make_change_set_from_sync(&tSyncResult, pResult, pError);
            xllm_memory_sync_workspace_result_reset(&tSyncResult);
            xllm__free_cstr(&sRecordId);
            xllm__free_cstr(&sTitle);
            xllm__free_cstr(&sSourceUri);
            return iStatus;
        }
    }

    tFileOptions.eScope = pUseOptions->eScope;
    tFileOptions.sPath = pUseOptions->sPath;
    tFileOptions.sRecordId = sRecordId;
    tFileOptions.sTitle = sTitle;
    tFileOptions.sSourceUri = sSourceUri;
    tFileOptions.bReplaceExisting = pUseOptions->bReplaceExisting;
    tFileOptions.uMaxFileBytes = uEffectiveMaxFileBytes;
    tFileOptions.uChunkChars = pUseOptions->uChunkChars;
    tFileOptions.uChunkOverlapChars = pUseOptions->uChunkOverlapChars;
    tFileOptions.tMetadata = tMetadata;
    tFileOptions.tVendorExtra = pUseOptions->tVendorExtra;

    iStatus = xllm_memory_ingest_file(pMemory, &tFileOptions, pError);
    xvoUnref(tMetadata);
    if ( iStatus != XRT_NET_OK ) {
        (void)xllm__memory_append_failed_file_info(&tSyncResult.tIngest.pFailedFiles,
                                                   &tSyncResult.tIngest.iFailedDetailCount,
                                                   pUseOptions->sRootPath,
                                                   pUseOptions->sPath,
                                                   sRelativePath,
                                                   sSourceUriPrefix,
                                                   XLLM_MEMORY_FAIL_INGEST,
                                                   uFileSize,
                                                   pError);
        ++tSyncResult.tIngest.uFailedFileCount;
        iStatus = xllm_memory_make_change_set_from_sync(&tSyncResult, pResult, pError);
        xllm_memory_sync_workspace_result_reset(&tSyncResult);
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        return iStatus;
    }

    xrtMutexLock(pMemory->pMutex);
    iExistingByRecordId = xllm__memory_find_record_index_locked(pMemory, pUseOptions->eScope, sRecordId);
    if ( iExistingByRecordId != (size_t)-1 ) {
        if ( xllm__memory_append_record_info_clone(
                (bHadExistingRecord ? &tSyncResult.tIngest.pUpdatedRecords : &tSyncResult.tIngest.pCreatedRecords),
                (bHadExistingRecord ? &tSyncResult.tIngest.iUpdatedDetailCount : &tSyncResult.tIngest.iCreatedDetailCount),
                &pMemory->pRecords[iExistingByRecordId]
             ) == XRT_NET_OK ) {
            if ( bHadExistingRecord ) {
                ++tSyncResult.tIngest.uUpdatedRecordCount;
            } else {
                ++tSyncResult.tIngest.uCreatedRecordCount;
            }
            ++tSyncResult.tIngest.uIngestedFileCount;
        }
    }
    xrtMutexUnlock(pMemory->pMutex);

    if ( tSyncResult.tIngest.uIngestedFileCount == 0u ) {
        xrtMutexLock(pMemory->pMutex);
        iExistingByRecordId = xllm__memory_find_record_index_locked(pMemory, pUseOptions->eScope, sRecordId);
        if ( iExistingByRecordId != (size_t)-1 ) {
            (void)xllm__memory_append_record_info_clone(&tSyncResult.tIngest.pCreatedRecords,
                                                        &tSyncResult.tIngest.iCreatedDetailCount,
                                                        &pMemory->pRecords[iExistingByRecordId]);
            ++tSyncResult.tIngest.uCreatedRecordCount;
            ++tSyncResult.tIngest.uIngestedFileCount;
        }
        xrtMutexUnlock(pMemory->pMutex);
    }

    iStatus = xllm_memory_make_change_set_from_sync(&tSyncResult, pResult, pError);
    xllm_memory_sync_workspace_result_reset(&tSyncResult);
    xllm__free_cstr(&sRecordId);
    xllm__free_cstr(&sTitle);
    xllm__free_cstr(&sSourceUri);
    return iStatus;
}

XLLM_API int xllm_memory_sync_workspace(
    xllm_memory *pMemory,
    const xllm_memory_ingest_workspace_options *pOptions,
    xllm_memory_sync_workspace_result *pResult,
    xllm_error *pError
)
{
    xllm_memory_ingest_workspace_options tDefaultOptions;
    const xllm_memory_ingest_workspace_options *pUseOptions = pOptions;
    xllm_memory_sync_workspace_result tLocalResult;
    xllm_memory_sync_workspace_result *pUseResult = pResult;
    xllm_memory_ingest_directory_options tDirectoryOptions;
    char *sMergedIgnoredPatterns = NULL;
    char **ppRemoveIds = NULL;
    size_t iRemoveCount = 0u;
    size_t iRemoveCapacity = 0u;
    const char *sSourceUriPrefix;
    size_t i;
    int iStatus;

    if ( !pMemory ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory handle is required");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_ingest_workspace_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( !pUseOptions->sPath || !pUseOptions->sPath[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory sync workspace requires a path");
        return XRT_NET_ERROR;
    }

    if ( !pUseResult ) {
        pUseResult = &tLocalResult;
        xllm_memory_sync_workspace_result_init(pUseResult);
    } else {
        xllm_memory_sync_workspace_result_reset(pUseResult);
    }

    xllm_memory_ingest_directory_options_init(&tDirectoryOptions);
    tDirectoryOptions.eScope = pUseOptions->eScope;
    tDirectoryOptions.sPath = pUseOptions->sPath;
    tDirectoryOptions.sRecordIdPrefix = pUseOptions->sRecordIdPrefix;
    tDirectoryOptions.sAllowedExtensions = pUseOptions->sAllowedExtensions;
    tDirectoryOptions.bRecursive = pUseOptions->bRecursive;
    tDirectoryOptions.bReplaceExisting = pUseOptions->bReplaceExisting;
    tDirectoryOptions.bSkipHidden = pUseOptions->bSkipHidden;
    tDirectoryOptions.bUseWorkspaceDefaults = true;
    tDirectoryOptions.bSkipUnchanged = pUseOptions->bSkipUnchanged;
    tDirectoryOptions.sIgnoredDirectories = pUseOptions->sIgnoredDirectories;
    tDirectoryOptions.sIgnoredExtensions = pUseOptions->sIgnoredExtensions;
    tDirectoryOptions.sSourceUriPrefix = pUseOptions->sSourceUriPrefix;
    tDirectoryOptions.uMaxFileBytes = pUseOptions->uMaxFileBytes;
    tDirectoryOptions.uChunkChars = pUseOptions->uChunkChars;
    tDirectoryOptions.uChunkOverlapChars = pUseOptions->uChunkOverlapChars;
    tDirectoryOptions.pfnProgress = pUseOptions->pfnProgress;
    tDirectoryOptions.pProgressCtx = pUseOptions->pProgressCtx;
    tDirectoryOptions.tMetadata = pUseOptions->tMetadata;
    tDirectoryOptions.tVendorExtra = pUseOptions->tVendorExtra;

    iStatus = xllm__memory_build_workspace_ignore_patterns(pUseOptions, &sMergedIgnoredPatterns, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }
    tDirectoryOptions.sIgnoredPathPatterns = sMergedIgnoredPatterns ? sMergedIgnoredPatterns : pUseOptions->sIgnoredPathPatterns;

    iStatus = xllm_memory_ingest_directory(pMemory, &tDirectoryOptions, &pUseResult->tIngest, pError);
    if ( iStatus != XRT_NET_OK ) {
        if ( sMergedIgnoredPatterns ) {
            xrtFree(sMergedIgnoredPatterns);
        }
        return iStatus;
    }

    sSourceUriPrefix = (pUseOptions->sSourceUriPrefix && pUseOptions->sSourceUriPrefix[0])
        ? pUseOptions->sSourceUriPrefix
        : XLLM__MEMORY_WORKSPACE_DEFAULT_SOURCE_URI_PREFIX;

    xrtMutexLock(pMemory->pMutex);
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        const xllm__memory_record_entry *pRecord = &pMemory->pRecords[i];
        const char *sPath;
        const char *sSourceUri;
        char *sRecordIdCopy;
        bool bManagedByWorkspaceSync;

        if ( pRecord->eScope != pUseOptions->eScope ) {
            continue;
        }
        if ( !pRecord->sRecordId || !pRecord->sRecordId[0] ) {
            continue;
        }
        if ( pUseOptions->sRecordIdPrefix && pUseOptions->sRecordIdPrefix[0] ) {
            size_t iPrefixLength = strlen(pUseOptions->sRecordIdPrefix);
            if ( xllm__memory_ascii_strnicmp(pRecord->sRecordId, pUseOptions->sRecordIdPrefix, iPrefixLength) != 0 ) {
                continue;
            }
        }

        sPath = (const char *)xvoTableGetText(pRecord->tMetadata, XLLM__MEMORY_METADATA_KEY_PATH, 0u);
        sSourceUri = pRecord->sSourceUri;
        bManagedByWorkspaceSync = false;
        if ( sPath && sPath[0] && xllm__memory_path_is_under_root_ci(pUseOptions->sPath, sPath, NULL) ) {
            bManagedByWorkspaceSync = true;
        } else if ( sSourceUri && sSourceUri[0] && sSourceUriPrefix && sSourceUriPrefix[0] ) {
            size_t iPrefixLength = strlen(sSourceUriPrefix);
            if ( xllm__memory_ascii_strnicmp(sSourceUri, sSourceUriPrefix, iPrefixLength) == 0 ) {
                bManagedByWorkspaceSync = true;
            }
        }
        if ( !bManagedByWorkspaceSync ) {
            continue;
        }

        ++pUseResult->uExaminedRecordCount;
        if ( sPath && sPath[0] && xllm__memory_directory_path_should_be_ingested(&tDirectoryOptions, sPath) ) {
            continue;
        }

        sRecordIdCopy = xllm__dup_cstr(pRecord->sRecordId);
        if ( !sRecordIdCopy ) {
            xrtMutexUnlock(pMemory->pMutex);
            if ( ppRemoveIds ) {
                size_t j;
                for ( j = 0u; j < iRemoveCount; ++j ) {
                    xllm__free_cstr(&ppRemoveIds[j]);
                }
                xrtFree(ppRemoveIds);
            }
            if ( sMergedIgnoredPatterns ) {
                xrtFree(sMergedIgnoredPatterns);
            }
            xllm_memory_sync_workspace_result_reset(pUseResult);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to collect stale workspace records");
            return XRT_NET_ERROR;
        }
        if ( xllm__append_buffer(
                (void **)&ppRemoveIds,
                sizeof(char *),
                &iRemoveCount,
                &iRemoveCapacity,
                &sRecordIdCopy
             ) != XRT_NET_OK ) {
            xllm__free_cstr(&sRecordIdCopy);
            xrtMutexUnlock(pMemory->pMutex);
            if ( ppRemoveIds ) {
                size_t j;
                for ( j = 0u; j < iRemoveCount; ++j ) {
                    xllm__free_cstr(&ppRemoveIds[j]);
                }
                xrtFree(ppRemoveIds);
            }
            if ( sMergedIgnoredPatterns ) {
                xrtFree(sMergedIgnoredPatterns);
            }
            xllm_memory_sync_workspace_result_reset(pUseResult);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to collect stale workspace records");
            return XRT_NET_ERROR;
        }
        if ( xllm__memory_append_record_info_clone(
                &pUseResult->pRemovedRecords,
                &pUseResult->iRemovedDetailCount,
                pRecord
             ) != XRT_NET_OK ) {
            xrtMutexUnlock(pMemory->pMutex);
            if ( ppRemoveIds ) {
                size_t j;
                for ( j = 0u; j < iRemoveCount; ++j ) {
                    xllm__free_cstr(&ppRemoveIds[j]);
                }
                xrtFree(ppRemoveIds);
            }
            if ( sMergedIgnoredPatterns ) {
                xrtFree(sMergedIgnoredPatterns);
            }
            xllm_memory_sync_workspace_result_reset(pUseResult);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to collect stale workspace records");
            return XRT_NET_ERROR;
        }
    }
    xrtMutexUnlock(pMemory->pMutex);

    for ( i = 0u; i < iRemoveCount; ++i ) {
        iStatus = xllm_memory_remove(pMemory, pUseOptions->eScope, ppRemoveIds[i], pError);
        if ( iStatus != XRT_NET_OK ) {
            size_t j;
            for ( j = i; j < iRemoveCount; ++j ) {
                xllm__free_cstr(&ppRemoveIds[j]);
            }
            xrtFree(ppRemoveIds);
            if ( sMergedIgnoredPatterns ) {
                xrtFree(sMergedIgnoredPatterns);
            }
            return iStatus;
        }
        ++pUseResult->uRemovedRecordCount;
        xllm__free_cstr(&ppRemoveIds[i]);
    }
    if ( ppRemoveIds ) {
        xrtFree(ppRemoveIds);
    }
    if ( sMergedIgnoredPatterns ) {
        xrtFree(sMergedIgnoredPatterns);
    }
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_sync_files(
    xllm_memory *pMemory,
    const xllm_memory_sync_files_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    xllm_memory_sync_files_options tDefaultOptions;
    const xllm_memory_sync_files_options *pUseOptions = pOptions;
    size_t i;

    if ( !pMemory || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory sync files requires memory handle and output");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_sync_files_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( !pUseOptions->pItems || pUseOptions->iItemCount == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory sync files requires at least one item");
        return XRT_NET_ERROR;
    }

    xllm_memory_change_set_reset(pResult);

    for ( i = 0u; i < pUseOptions->iItemCount; ++i ) {
        if ( xllm__memory_sync_file_batch_run_one(
                pMemory,
                &pUseOptions->pItems[i],
                pUseOptions->bContinueOnError,
                pResult,
                pError
             ) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
    }

    return XRT_NET_OK;
}

XLLM_API int xllm_memory_sync_file_events(
    xllm_memory *pMemory,
    const xllm_memory_sync_file_events_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    xllm_memory_sync_file_events_options tDefaultOptions;
    const xllm_memory_sync_file_events_options *pUseOptions = pOptions;
    xllm_memory_sync_file_options tSyncOptions;
    size_t i;

    if ( !pMemory || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory sync file events requires memory handle and output");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_sync_file_events_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( !pUseOptions->pItems || pUseOptions->iItemCount == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory sync file events requires at least one item");
        return XRT_NET_ERROR;
    }

    xllm_memory_change_set_reset(pResult);

    for ( i = 0u; i < pUseOptions->iItemCount; ++i ) {
        const xllm_memory_file_event *pEvent = &pUseOptions->pItems[i];

        tSyncOptions = pUseOptions->tBaseOptions;
        if ( pEvent->eKind != XLLM_MEMORY_FILE_EVENT_RENAMED &&
             (!pEvent->sPath || !pEvent->sPath[0]) ) {
            xllm_error tItemError;
            xllm_error_init(&tItemError);
            xllm__error_set(&tItemError, XLLM_ERROR_INVALID_REQUEST, "memory sync file event requires path");
            if ( xllm__memory_sync_file_batch_handle_failure(
                    pResult,
                    &tSyncOptions,
                    pUseOptions->bContinueOnError,
                    XRT_NET_ERROR,
                    &tItemError,
                    pError
                 ) != XRT_NET_OK ) {
                xllm_error_free(&tItemError);
                return XRT_NET_ERROR;
            }
            xllm_error_free(&tItemError);
            continue;
        }

        switch ( pEvent->eKind ) {
            case XLLM_MEMORY_FILE_EVENT_CREATED:
            case XLLM_MEMORY_FILE_EVENT_UPDATED:
            case XLLM_MEMORY_FILE_EVENT_DELETED:
                tSyncOptions.sPath = pEvent->sPath;
                if ( xllm__memory_sync_file_batch_run_one(
                        pMemory,
                        &tSyncOptions,
                        pUseOptions->bContinueOnError,
                        pResult,
                        pError
                     ) != XRT_NET_OK ) {
                    return XRT_NET_ERROR;
                }
                break;
            case XLLM_MEMORY_FILE_EVENT_RENAMED:
                if ( !pEvent->sPreviousPath || !pEvent->sPreviousPath[0] ||
                     !pEvent->sPath || !pEvent->sPath[0] ) {
                    xllm_error tItemError;
                    xllm_error_init(&tItemError);
                    xllm__error_set(&tItemError, XLLM_ERROR_INVALID_REQUEST, "memory rename event requires previous and current path");
                    tSyncOptions.sPath = pEvent->sPreviousPath ? pEvent->sPreviousPath : pEvent->sPath;
                    if ( xllm__memory_sync_file_batch_handle_failure(
                            pResult,
                            &tSyncOptions,
                            pUseOptions->bContinueOnError,
                            XRT_NET_ERROR,
                            &tItemError,
                            pError
                         ) != XRT_NET_OK ) {
                        xllm_error_free(&tItemError);
                        return XRT_NET_ERROR;
                    }
                    xllm_error_free(&tItemError);
                    break;
                }
                tSyncOptions.sPath = pEvent->sPreviousPath;
                if ( xllm__memory_sync_file_batch_run_one(
                        pMemory,
                        &tSyncOptions,
                        pUseOptions->bContinueOnError,
                        pResult,
                        pError
                     ) != XRT_NET_OK ) {
                    return XRT_NET_ERROR;
                }
                tSyncOptions.sPath = pEvent->sPath;
                if ( xllm__memory_sync_file_batch_run_one(
                        pMemory,
                        &tSyncOptions,
                        pUseOptions->bContinueOnError,
                        pResult,
                        pError
                     ) != XRT_NET_OK ) {
                    return XRT_NET_ERROR;
                }
                break;
            default:
            {
                xllm_error tItemError;
                xllm_error_init(&tItemError);
                tSyncOptions.sPath = pEvent->sPath;
                xllm__error_set(&tItemError, XLLM_ERROR_INVALID_REQUEST, "memory sync file event kind is unsupported");
                if ( xllm__memory_sync_file_batch_handle_failure(
                        pResult,
                        &tSyncOptions,
                        pUseOptions->bContinueOnError,
                        XRT_NET_ERROR,
                        &tItemError,
                        pError
                     ) != XRT_NET_OK ) {
                    xllm_error_free(&tItemError);
                    return XRT_NET_ERROR;
                }
                xllm_error_free(&tItemError);
                break;
            }
        }
    }

    return XRT_NET_OK;
}
