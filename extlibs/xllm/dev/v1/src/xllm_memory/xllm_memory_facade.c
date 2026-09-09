XLLM_API void xllm_memory_embedder_init(xllm_memory_embedder *pEmbedder)
{
    if ( !pEmbedder ) {
        return;
    }
    memset(pEmbedder, 0, sizeof(*pEmbedder));
}

XLLM_API void xllm_memory_embedder_reset(xllm_memory_embedder *pEmbedder)
{
    xllm__memory_embedder_release(pEmbedder, true);
}

XLLM_API void xllm_memory_builtin_embedder_options_init(
    xllm_memory_builtin_embedder_options *pOptions
)
{
    xllm__memory_builtin_embedder_options_init(pOptions);
}

XLLM_API void xllm_memory_builtin_embedder_probe_reset(
    xllm_memory_builtin_embedder_probe *pProbe
)
{
    xllm__memory_builtin_embedder_probe_reset(pProbe);
}

XLLM_API int xllm_memory_probe_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_builtin_embedder_probe *pProbe,
    xllm_error *pError
)
{
    return xllm__memory_probe_builtin_embedder(pOptions, pProbe, pError);
}

XLLM_API int xllm_memory_make_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_embedder *pEmbedder,
    xllm_error *pError
)
{
    return xllm__memory_make_builtin_embedder(pOptions, pEmbedder, pError);
}

XLLM_API void xllm_memory_options_init(xllm_memory_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScheme = XLLM_MEMORY_SCHEME_AUTO;
    pOptions->bEnableHybridSearch = true;
    pOptions->tLexicalWeight.bSet = true;
    pOptions->tLexicalWeight.fValue = 1.0;
    pOptions->tVectorWeight.bSet = true;
    pOptions->tVectorWeight.fValue = 1.0;
    pOptions->uDefaultChunkChars = 800u;
    pOptions->uDefaultChunkOverlapChars = 120u;
    pOptions->uDefaultMaxHits = 5u;
    pOptions->uSqliteBusyTimeoutMs = 5000u;
}

XLLM_API void xllm_memory_ingest_options_init(xllm_memory_ingest_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    pOptions->bReplaceExisting = true;
}

XLLM_API void xllm_memory_ingest_turn_response_options_init(xllm_memory_ingest_turn_response_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pOptions->eExtractionPolicy = XLLM_MEMORY_EXTRACTION_POLICY_TURN_RESPONSE;
    pOptions->bReplaceExisting = true;
    pOptions->bUseStableIdentity = false;
    pOptions->iPriority = 0;
    pOptions->iExpiresAtUnix = 0;
    pOptions->iUpdatedAtUnix = 0;
    pOptions->bIncludeSystemPrompt = false;
    pOptions->bIncludeContextBlocks = false;
    pOptions->bIncludeThinking = false;
}

XLLM_API void xllm_memory_ingest_task_options_init(xllm_memory_ingest_task_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pOptions->eStatus = XLLM_MEMORY_TASK_STATUS_OPEN;
    pOptions->bReplaceExisting = true;
    pOptions->iPriority = 0;
    pOptions->iExpiresAtUnix = 0;
    pOptions->iUpdatedAtUnix = 0;
}

XLLM_API void xllm_memory_ingest_fact_options_init(xllm_memory_ingest_fact_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pOptions->bReplaceExisting = true;
    pOptions->iPriority = 0;
    pOptions->iExpiresAtUnix = 0;
    pOptions->iUpdatedAtUnix = 0;
}

XLLM_API void xllm_memory_ingest_preference_options_init(xllm_memory_ingest_preference_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pOptions->bReplaceExisting = true;
    pOptions->iPriority = 0;
    pOptions->iExpiresAtUnix = 0;
    pOptions->iUpdatedAtUnix = 0;
}

XLLM_API void xllm_memory_remove_expired_options_init(xllm_memory_remove_expired_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pOptions->sSourceUriPrefix = XLLM__MEMORY_CONVERSATION_SOURCE_URI_PREFIX;
    pOptions->sMetadataKey = XLLM__MEMORY_METADATA_KEY_EXPIRES_AT_UNIX;
}

XLLM_API void xllm_memory_remove_by_metadata_options_init(xllm_memory_remove_by_metadata_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_ANY;
}

XLLM_API void xllm_memory_trim_conversation_options_init(xllm_memory_trim_conversation_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pOptions->uKeepLatestRecords = 32u;
}

XLLM_API void xllm_memory_compact_conversation_options_init(xllm_memory_compact_conversation_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pOptions->bReplaceSummary = true;
    pOptions->bRemoveSourceRecords = true;
    pOptions->uMaxSourceRecords = 0u;
}

XLLM_API void xllm_memory_compact_conversation_result_init(xllm_memory_compact_conversation_result *pResult)
{
    if ( !pResult ) {
        return;
    }

    memset(pResult, 0, sizeof(*pResult));
}

XLLM_API void xllm_memory_ingest_file_options_init(xllm_memory_ingest_file_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    pOptions->bReplaceExisting = true;
}

XLLM_API void xllm_memory_ingest_directory_options_init(xllm_memory_ingest_directory_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    pOptions->bRecursive = true;
    pOptions->bReplaceExisting = true;
    pOptions->bSkipHidden = true;
    pOptions->bSkipUnchanged = false;
}

XLLM_API void xllm_memory_ingest_workspace_options_init(xllm_memory_ingest_workspace_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    pOptions->bRecursive = true;
    pOptions->bReplaceExisting = true;
    pOptions->bSkipHidden = true;
    pOptions->bSkipUnchanged = true;
    pOptions->bLoadGitIgnore = true;
    pOptions->sSourceUriPrefix = XLLM__MEMORY_WORKSPACE_DEFAULT_SOURCE_URI_PREFIX;
    pOptions->uMaxFileBytes = XLLM__MEMORY_WORKSPACE_DEFAULT_MAX_FILE_BYTES;
}

XLLM_API void xllm_memory_sync_file_options_init(xllm_memory_sync_file_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    pOptions->bReplaceExisting = true;
    pOptions->bSkipHidden = true;
    pOptions->bSkipUnchanged = true;
}

XLLM_API void xllm_memory_sync_files_options_init(xllm_memory_sync_files_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->bContinueOnError = true;
}

XLLM_API void xllm_memory_sync_file_event_init(xllm_memory_file_event *pEvent)
{
    if ( !pEvent ) {
        return;
    }

    memset(pEvent, 0, sizeof(*pEvent));
    pEvent->eKind = XLLM_MEMORY_FILE_EVENT_UPDATED;
}

XLLM_API void xllm_memory_sync_file_events_options_init(xllm_memory_sync_file_events_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    xllm_memory_sync_file_options_init(&pOptions->tBaseOptions);
    pOptions->bContinueOnError = true;
}

XLLM_API void xllm_memory_file_event_queue_drain_options_init(xllm_memory_file_event_queue_drain_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    xllm_memory_sync_file_options_init(&pOptions->tBaseOptions);
    pOptions->bContinueOnError = true;
}

XLLM_API void xllm_memory_watcher_bridge_options_init(xllm_memory_watcher_bridge_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    xllm_memory_sync_file_options_init(&pOptions->tBaseOptions);
    pOptions->bContinueOnError = true;
}

XLLM_API void xllm_memory_watcher_pump_options_init(xllm_memory_watcher_pump_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    xllm_memory_watcher_bridge_options_init(&pOptions->tBridgeOptions);
}

XLLM_API void xllm_memory_watcher_worker_options_init(xllm_memory_watcher_worker_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    xllm_memory_watcher_pump_options_init(&pOptions->tPumpOptions);
}

XLLM_API void xllm_memory_watcher_worker_run_options_init(xllm_memory_watcher_worker_run_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
}

XLLM_API void xllm_memory_watcher_worker_run_result_init(xllm_memory_watcher_worker_run_result *pResult)
{
    if ( !pResult ) {
        return;
    }

    memset(pResult, 0, sizeof(*pResult));
}

XLLM_API void xllm_memory_watcher_worker_loop_options_init(xllm_memory_watcher_worker_loop_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    xllm_memory_watcher_worker_run_options_init(&pOptions->tRunOptions);
}

XLLM_API void xllm_memory_watcher_worker_loop_result_init(xllm_memory_watcher_worker_loop_result *pResult)
{
    if ( !pResult ) {
        return;
    }

    memset(pResult, 0, sizeof(*pResult));
}

XLLM_API void xllm_memory_watcher_worker_state_init(xllm_memory_watcher_worker_state *pState)
{
    if ( !pState ) {
        return;
    }

    memset(pState, 0, sizeof(*pState));
}

XLLM_API void xllm_memory_ingest_directory_result_init(xllm_memory_ingest_directory_result *pResult)
{
    if ( !pResult ) {
        return;
    }

    memset(pResult, 0, sizeof(*pResult));
}

XLLM_API void xllm_memory_sync_workspace_result_init(xllm_memory_sync_workspace_result *pResult)
{
    if ( !pResult ) {
        return;
    }

    memset(pResult, 0, sizeof(*pResult));
}

XLLM_API void xllm_memory_change_set_init(xllm_memory_change_set *pResult)
{
    if ( !pResult ) {
        return;
    }

    memset(pResult, 0, sizeof(*pResult));
}

XLLM_API void xllm_memory_ingest_directory_result_reset(xllm_memory_ingest_directory_result *pResult)
{
    if ( !pResult ) {
        return;
    }

    xllm__memory_record_info_array_reset(pResult->pCreatedRecords, pResult->iCreatedDetailCount);
    xllm__memory_record_info_array_reset(pResult->pUpdatedRecords, pResult->iUpdatedDetailCount);
    xllm__memory_skipped_file_info_array_reset(pResult->pSkippedFiles, pResult->iSkippedDetailCount);
    xllm__memory_failed_file_info_array_reset(pResult->pFailedFiles, pResult->iFailedDetailCount);
    memset(pResult, 0, sizeof(*pResult));
}

XLLM_API void xllm_memory_sync_workspace_result_reset(xllm_memory_sync_workspace_result *pResult)
{
    if ( !pResult ) {
        return;
    }

    xllm_memory_ingest_directory_result_reset(&pResult->tIngest);
    xllm__memory_record_info_array_reset(pResult->pRemovedRecords, pResult->iRemovedDetailCount);
    memset(pResult, 0, sizeof(*pResult));
}

XLLM_API void xllm_memory_change_set_reset(xllm_memory_change_set *pResult)
{
    if ( !pResult ) {
        return;
    }

    xllm__memory_change_info_array_reset(pResult->pChanges, pResult->iChangeCount);
    memset(pResult, 0, sizeof(*pResult));
}

XLLM_API void xllm_memory_search_options_init(xllm_memory_search_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_ANY;
    pOptions->uMaxHits = 5u;
    pOptions->uMaxCharsPerHit = 800u;
}

XLLM_API void xllm_memory_retrieval_debug_options_init(xllm_memory_retrieval_debug_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    xllm_memory_search_options_init(&pOptions->tSearchOptions);
    pOptions->uMaxCandidates = 32u;
    pOptions->bIncludeBelowMinScore = true;
}

XLLM_API void xllm_memory_list_options_init(xllm_memory_list_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_ANY;
    pOptions->uMaxItems = 32u;
    pOptions->uMaxCharsPerText = 800u;
}

XLLM_API void xllm_memory_context_options_init(xllm_memory_context_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->iPriority = 10;
    pOptions->uMaxHits = 4u;
    pOptions->uMaxCharsPerHit = 600u;
    pOptions->uMaxTotalChars = 0u;
    pOptions->bDistinctByRecord = false;
}

XLLM_API void xllm_memory_turn_search_apply_options_init(xllm_memory_turn_search_apply_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    xllm_memory_search_options_init(&pOptions->tSearchOptions);
    xllm_memory_context_options_init(&pOptions->tContextOptions);
    pOptions->eQueryMode = XLLM_MEMORY_TURN_QUERY_LAST_USER_TEXT;
    pOptions->bIncludeSystemPrompt = false;
    pOptions->bIncludeContextBlocks = false;
    pOptions->uMaxQueryChars = 0u;
}
