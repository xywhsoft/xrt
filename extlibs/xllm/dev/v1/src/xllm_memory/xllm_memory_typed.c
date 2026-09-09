XLLM_API int xllm_memory_ingest_turn_response(
    xllm_memory *pMemory,
    const xllm_memory_ingest_turn_response_options *pOptions,
    xllm_error *pError
)
{
    xllm_memory_ingest_turn_response_options tDefaultOptions;
    const xllm_memory_ingest_turn_response_options *pUseOptions = pOptions;
    xllm_memory_ingest_options tIngestOptions;
    char *sRecordId = NULL;
    char *sTitle = NULL;
    char *sSourceUri = NULL;
    char *sTranscript = NULL;
    xvalue tMetadata = 0;
    int iStatus;
    xllm_memory_extraction_policy eExtractionPolicy;

    if ( !pMemory ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory handle is required");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_ingest_turn_response_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    eExtractionPolicy = xllm__memory_resolve_extraction_policy(pUseOptions->eExtractionPolicy);
    if ( pUseOptions->eScope != XLLM_MEMORY_SCOPE_MEMORY &&
         pUseOptions->eScope != XLLM_MEMORY_SCOPE_KNOWLEDGE ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "turn_response ingest scope must be memory or knowledge");
        return XRT_NET_ERROR;
    }
    if ( eExtractionPolicy == XLLM_MEMORY_EXTRACTION_POLICY_NONE ) {
        return XRT_NET_OK;
    }
    if ( eExtractionPolicy != XLLM_MEMORY_EXTRACTION_POLICY_TURN_RESPONSE &&
         eExtractionPolicy != XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "unsupported turn_response extraction policy");
        return XRT_NET_ERROR;
    }
    if ( eExtractionPolicy == XLLM_MEMORY_EXTRACTION_POLICY_TURN_RESPONSE &&
         !pUseOptions->pTurn &&
         !pUseOptions->pResponse ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "turn_response ingest requires a turn, a response, or both");
        return XRT_NET_ERROR;
    }
    if ( eExtractionPolicy == XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY &&
         (!pUseOptions->sSummaryText || !pUseOptions->sSummaryText[0]) &&
         !pUseOptions->pResponse ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "summary ingest requires summary text or response visible text");
        return XRT_NET_ERROR;
    }

    sTitle = (pUseOptions->sTitle && pUseOptions->sTitle[0])
        ? xllm__dup_cstr(pUseOptions->sTitle)
        : xllm__dup_cstr(
            (eExtractionPolicy == XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY)
                ? "Conversation summary"
                : "Conversation memory"
        );
    sSourceUri = xllm__memory_make_turn_response_source_uri(pUseOptions, pUseOptions->sRecordId);
    sRecordId = xllm__memory_make_turn_response_record_id(pUseOptions, sSourceUri);
    if ( !sTitle || !sRecordId ) {
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate turn_response ingest record metadata");
        return XRT_NET_ERROR;
    }
    if ( !sSourceUri ) {
        sSourceUri = xllm__memory_make_turn_response_source_uri(pUseOptions, sRecordId);
    }
    if ( !sSourceUri ) {
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate turn_response ingest source uri");
        return XRT_NET_ERROR;
    }

    sTranscript = xllm__memory_build_turn_response_transcript(pUseOptions, pError);
    if ( !sTranscript ) {
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        return XRT_NET_ERROR;
    }

    {
        int64 iCreatedAtUnix = xllm__memory_lookup_record_metadata_int(
            pMemory,
            pUseOptions->eScope,
            sRecordId,
            XLLM__MEMORY_METADATA_KEY_CREATED_AT_UNIX
        );
        int64 iUpdatedAtUnix = (pUseOptions->iUpdatedAtUnix > 0) ? pUseOptions->iUpdatedAtUnix : xrtToUnixTime(xrtNow());

        if ( iCreatedAtUnix <= 0 ) {
            iCreatedAtUnix = iUpdatedAtUnix;
        }
        tMetadata = xllm__memory_make_turn_response_metadata(
            pUseOptions->tMetadata,
            pUseOptions,
            iCreatedAtUnix,
            iUpdatedAtUnix
        );
    }
    if ( !tMetadata ) {
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        xrtFree(sTranscript);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build turn_response metadata");
        return XRT_NET_ERROR;
    }

    xllm_memory_ingest_options_init(&tIngestOptions);
    tIngestOptions.eScope = pUseOptions->eScope;
    tIngestOptions.sRecordId = sRecordId;
    tIngestOptions.sTitle = sTitle;
    tIngestOptions.sSourceUri = sSourceUri;
    tIngestOptions.sText = sTranscript;
    tIngestOptions.bReplaceExisting = pUseOptions->bReplaceExisting;
    tIngestOptions.uChunkChars = pUseOptions->uChunkChars;
    tIngestOptions.uChunkOverlapChars = pUseOptions->uChunkOverlapChars;
    tIngestOptions.tMetadata = tMetadata;
    tIngestOptions.tVendorExtra = pUseOptions->tVendorExtra;
    iStatus = xllm_memory_ingest_text(pMemory, &tIngestOptions, pError);

    xvoUnref(tMetadata);
    xllm__free_cstr(&sRecordId);
    xllm__free_cstr(&sTitle);
    xllm__free_cstr(&sSourceUri);
    if ( sTranscript ) {
        xrtFree(sTranscript);
    }
    return iStatus;
}

XLLM_API int xllm_memory_ingest_task(
    xllm_memory *pMemory,
    const xllm_memory_ingest_task_options *pOptions,
    xllm_error *pError
)
{
    xllm_memory_ingest_task_options tDefaultOptions;
    const xllm_memory_ingest_task_options *pUseOptions = pOptions;
    xllm_memory_ingest_options tIngestOptions;
    char *sRecordId = NULL;
    char *sTitle = NULL;
    char *sSourceUri = NULL;
    char *sText = NULL;
    xvalue tMetadata = 0;
    int64 iCreatedAtUnix;
    int64 iUpdatedAtUnix;
    int iStatus;

    if ( !pMemory ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory handle is required");
        return XRT_NET_ERROR;
    }
    if ( !pUseOptions ) {
        xllm_memory_ingest_task_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( pUseOptions->eScope != XLLM_MEMORY_SCOPE_MEMORY &&
         pUseOptions->eScope != XLLM_MEMORY_SCOPE_KNOWLEDGE ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "task ingest scope must be memory or knowledge");
        return XRT_NET_ERROR;
    }

    sRecordId = xllm__memory_make_task_record_id(pUseOptions);
    if ( !sRecordId ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate task memory record id");
        return XRT_NET_ERROR;
    }
    sTitle = xllm__dup_cstr((pUseOptions->sTitle && pUseOptions->sTitle[0]) ? pUseOptions->sTitle : "Task memory");
    sSourceUri = xllm__memory_make_task_source_uri(pUseOptions, sRecordId);
    sText = xllm__memory_build_task_text(pUseOptions, pError);
    if ( !sTitle || !sSourceUri || !sText ) {
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        if ( sText ) {
            xrtFree(sText);
        }
        if ( !pError || !pError->sMessage ) {
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build task memory ingest payload");
        }
        return XRT_NET_ERROR;
    }

    iCreatedAtUnix = xllm__memory_lookup_record_metadata_int(
        pMemory,
        pUseOptions->eScope,
        sRecordId,
        XLLM__MEMORY_METADATA_KEY_CREATED_AT_UNIX
    );
    iUpdatedAtUnix = (pUseOptions->iUpdatedAtUnix > 0) ? pUseOptions->iUpdatedAtUnix : xrtToUnixTime(xrtNow());
    if ( iCreatedAtUnix <= 0 ) {
        iCreatedAtUnix = iUpdatedAtUnix;
    }
    tMetadata = xllm__memory_make_task_metadata(
        pUseOptions->tMetadata,
        pUseOptions,
        iCreatedAtUnix,
        iUpdatedAtUnix
    );
    if ( !tMetadata ) {
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        xrtFree(sText);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build task memory metadata");
        return XRT_NET_ERROR;
    }

    xllm_memory_ingest_options_init(&tIngestOptions);
    tIngestOptions.eScope = pUseOptions->eScope;
    tIngestOptions.sRecordId = sRecordId;
    tIngestOptions.sTitle = sTitle;
    tIngestOptions.sSourceUri = sSourceUri;
    tIngestOptions.sText = sText;
    tIngestOptions.bReplaceExisting = pUseOptions->bReplaceExisting;
    tIngestOptions.uChunkChars = pUseOptions->uChunkChars;
    tIngestOptions.uChunkOverlapChars = pUseOptions->uChunkOverlapChars;
    tIngestOptions.tMetadata = tMetadata;
    tIngestOptions.tVendorExtra = pUseOptions->tVendorExtra;
    iStatus = xllm_memory_ingest_text(pMemory, &tIngestOptions, pError);

    xvoUnref(tMetadata);
    xllm__free_cstr(&sRecordId);
    xllm__free_cstr(&sTitle);
    xllm__free_cstr(&sSourceUri);
    xrtFree(sText);
    return iStatus;
}

XLLM_API int xllm_memory_ingest_fact(
    xllm_memory *pMemory,
    const xllm_memory_ingest_fact_options *pOptions,
    xllm_error *pError
)
{
    xllm_memory_ingest_fact_options tDefaultOptions;
    const xllm_memory_ingest_fact_options *pUseOptions = pOptions;
    xllm_memory_ingest_options tIngestOptions;
    char *sRecordId = NULL;
    char *sTitle = NULL;
    char *sSourceUri = NULL;
    char *sText = NULL;
    xvalue tMetadata = 0;
    int64 iCreatedAtUnix;
    int64 iUpdatedAtUnix;
    int iStatus;

    if ( !pMemory ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory handle is required");
        return XRT_NET_ERROR;
    }
    if ( !pUseOptions ) {
        xllm_memory_ingest_fact_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( pUseOptions->eScope != XLLM_MEMORY_SCOPE_MEMORY &&
         pUseOptions->eScope != XLLM_MEMORY_SCOPE_KNOWLEDGE ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "fact ingest scope must be memory or knowledge");
        return XRT_NET_ERROR;
    }

    sRecordId = xllm__memory_make_fact_record_id(pUseOptions);
    if ( !sRecordId ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate fact memory record id");
        return XRT_NET_ERROR;
    }
    sTitle = xllm__dup_cstr((pUseOptions->sTitle && pUseOptions->sTitle[0]) ? pUseOptions->sTitle : "Fact memory");
    sSourceUri = xllm__memory_make_fact_source_uri(pUseOptions, sRecordId);
    sText = xllm__memory_build_fact_text(pUseOptions, pError);
    if ( !sTitle || !sSourceUri || !sText ) {
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        if ( sText ) {
            xrtFree(sText);
        }
        if ( !pError || !pError->sMessage ) {
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build fact memory ingest payload");
        }
        return XRT_NET_ERROR;
    }

    iCreatedAtUnix = xllm__memory_lookup_record_metadata_int(
        pMemory,
        pUseOptions->eScope,
        sRecordId,
        XLLM__MEMORY_METADATA_KEY_CREATED_AT_UNIX
    );
    iUpdatedAtUnix = (pUseOptions->iUpdatedAtUnix > 0) ? pUseOptions->iUpdatedAtUnix : xrtToUnixTime(xrtNow());
    if ( iCreatedAtUnix <= 0 ) {
        iCreatedAtUnix = iUpdatedAtUnix;
    }
    tMetadata = xllm__memory_make_fact_metadata(
        pUseOptions->tMetadata,
        pUseOptions,
        iCreatedAtUnix,
        iUpdatedAtUnix
    );
    if ( !tMetadata ) {
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        xrtFree(sText);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build fact memory metadata");
        return XRT_NET_ERROR;
    }

    xllm_memory_ingest_options_init(&tIngestOptions);
    tIngestOptions.eScope = pUseOptions->eScope;
    tIngestOptions.sRecordId = sRecordId;
    tIngestOptions.sTitle = sTitle;
    tIngestOptions.sSourceUri = sSourceUri;
    tIngestOptions.sText = sText;
    tIngestOptions.bReplaceExisting = pUseOptions->bReplaceExisting;
    tIngestOptions.uChunkChars = pUseOptions->uChunkChars;
    tIngestOptions.uChunkOverlapChars = pUseOptions->uChunkOverlapChars;
    tIngestOptions.tMetadata = tMetadata;
    tIngestOptions.tVendorExtra = pUseOptions->tVendorExtra;
    iStatus = xllm_memory_ingest_text(pMemory, &tIngestOptions, pError);

    xvoUnref(tMetadata);
    xllm__free_cstr(&sRecordId);
    xllm__free_cstr(&sTitle);
    xllm__free_cstr(&sSourceUri);
    xrtFree(sText);
    return iStatus;
}

XLLM_API int xllm_memory_compact_conversation(
    xllm_memory *pMemory,
    const xllm_memory_compact_conversation_options *pOptions,
    xllm_memory_compact_conversation_result *pResult,
    xllm_error *pError
)
{
    xllm_memory_compact_conversation_options tDefaultOptions;
    const xllm_memory_compact_conversation_options *pUseOptions = pOptions;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecords;
    xllm_memory_ingest_turn_response_options tSummaryOptions;
    uint32 uMaxSourceRecords;
    uint32 uMatchedRecordCount = 0u;
    uint32 uCompactedRecordCount = 0u;
    uint32 uRemovedRecordCount = 0u;
    size_t i;
    int iStatus;

    if ( pResult ) {
        xllm_memory_compact_conversation_result_init(pResult);
    }
    memset(&tRecords, 0, sizeof(tRecords));

    if ( !pMemory ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory compact conversation requires memory handle");
        return XRT_NET_ERROR;
    }
    if ( !pUseOptions ) {
        xllm_memory_compact_conversation_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( pUseOptions->eScope != XLLM_MEMORY_SCOPE_MEMORY &&
         pUseOptions->eScope != XLLM_MEMORY_SCOPE_KNOWLEDGE ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory compact conversation scope must be memory or knowledge");
        return XRT_NET_ERROR;
    }
    if ( !pUseOptions->sConversationId || !pUseOptions->sConversationId[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory compact conversation requires conversation_id");
        return XRT_NET_ERROR;
    }
    if ( !pUseOptions->sSummaryText || !pUseOptions->sSummaryText[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory compact conversation requires host-provided summary text");
        return XRT_NET_ERROR;
    }

    uMaxSourceRecords = pUseOptions->uMaxSourceRecords;
    if ( uMaxSourceRecords == 0u ) {
        uMaxSourceRecords = 4096u;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = pUseOptions->eScope;
    tListOptions.sConversationId = pUseOptions->sConversationId;
    tListOptions.sMetadataKey = XLLM__MEMORY_METADATA_KEY_MEMORY_TYPE;
    tListOptions.sMetadataValue = "conversation.turn_response.v1";
    tListOptions.uMaxItems = uMaxSourceRecords;
    tListOptions.uMaxCharsPerText = 0u;

    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    uMatchedRecordCount = (uint32)tRecords.iRecordCount;
    if ( uMatchedRecordCount == 0u ) {
        if ( pResult ) {
            pResult->uMatchedRecordCount = 0u;
        }
        xllm_memory_record_list_result_reset(&tRecords);
        return XRT_NET_OK;
    }

    xllm_memory_ingest_turn_response_options_init(&tSummaryOptions);
    tSummaryOptions.eScope = pUseOptions->eScope;
    tSummaryOptions.eExtractionPolicy = XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY;
    tSummaryOptions.sConversationId = pUseOptions->sConversationId;
    tSummaryOptions.sSummaryText = pUseOptions->sSummaryText;
    tSummaryOptions.sRecordId = pUseOptions->sSummaryRecordId;
    tSummaryOptions.sTitle = pUseOptions->sSummaryTitle;
    tSummaryOptions.sSourceUri = pUseOptions->sSummarySourceUri;
    tSummaryOptions.bReplaceExisting = pUseOptions->bReplaceSummary;
    tSummaryOptions.bUseStableIdentity = true;
    tSummaryOptions.iPriority = pUseOptions->iSummaryPriority;
    tSummaryOptions.iUpdatedAtUnix = pUseOptions->iSummaryUpdatedAtUnix;
    tSummaryOptions.uChunkChars = pUseOptions->uSummaryChunkChars;
    tSummaryOptions.uChunkOverlapChars = pUseOptions->uSummaryChunkOverlapChars;
    tSummaryOptions.tMetadata = pUseOptions->tSummaryMetadata;
    tSummaryOptions.tVendorExtra = pUseOptions->tVendorExtra;

    iStatus = xllm_memory_ingest_turn_response(pMemory, &tSummaryOptions, pError);
    if ( iStatus != XRT_NET_OK ) {
        xllm_memory_record_list_result_reset(&tRecords);
        return iStatus;
    }
    uCompactedRecordCount = uMatchedRecordCount;

    if ( pUseOptions->bRemoveSourceRecords ) {
        for ( i = 0u; i < tRecords.iRecordCount; ++i ) {
            uint32 uRemovedNow = 0u;

            if ( !tRecords.pRecords[i].sSourceUri || !tRecords.pRecords[i].sSourceUri[0] ) {
                continue;
            }
            iStatus = xllm_memory_remove_by_source_uri(
                pMemory,
                pUseOptions->eScope,
                tRecords.pRecords[i].sSourceUri,
                &uRemovedNow,
                pError
            );
            if ( iStatus != XRT_NET_OK ) {
                xllm_memory_record_list_result_reset(&tRecords);
                return iStatus;
            }
            uRemovedRecordCount += uRemovedNow;
        }
    }

    if ( pResult ) {
        pResult->uMatchedRecordCount = uMatchedRecordCount;
        pResult->uCompactedRecordCount = uCompactedRecordCount;
        pResult->uRemovedRecordCount = uRemovedRecordCount;
    }
    xllm_memory_record_list_result_reset(&tRecords);
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_ingest_preference(
    xllm_memory *pMemory,
    const xllm_memory_ingest_preference_options *pOptions,
    xllm_error *pError
)
{
    xllm_memory_ingest_preference_options tDefaultOptions;
    const xllm_memory_ingest_preference_options *pUseOptions = pOptions;
    xllm_memory_ingest_options tIngestOptions;
    char *sRecordId = NULL;
    char *sTitle = NULL;
    char *sSourceUri = NULL;
    char *sText = NULL;
    xvalue tMetadata = 0;
    int64 iCreatedAtUnix;
    int64 iUpdatedAtUnix;
    int iStatus;

    if ( !pMemory ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory handle is required");
        return XRT_NET_ERROR;
    }
    if ( !pUseOptions ) {
        xllm_memory_ingest_preference_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( pUseOptions->eScope != XLLM_MEMORY_SCOPE_MEMORY &&
         pUseOptions->eScope != XLLM_MEMORY_SCOPE_KNOWLEDGE ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "preference ingest scope must be memory or knowledge");
        return XRT_NET_ERROR;
    }

    sRecordId = xllm__memory_make_preference_record_id(pUseOptions);
    if ( !sRecordId ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate preference memory record id");
        return XRT_NET_ERROR;
    }
    sTitle = xllm__dup_cstr((pUseOptions->sTitle && pUseOptions->sTitle[0]) ? pUseOptions->sTitle : "Preference memory");
    sSourceUri = xllm__memory_make_preference_source_uri(pUseOptions, sRecordId);
    sText = xllm__memory_build_preference_text(pUseOptions, pError);
    if ( !sTitle || !sSourceUri || !sText ) {
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        if ( sText ) {
            xrtFree(sText);
        }
        if ( !pError || !pError->sMessage ) {
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build preference memory ingest payload");
        }
        return XRT_NET_ERROR;
    }

    iCreatedAtUnix = xllm__memory_lookup_record_metadata_int(
        pMemory,
        pUseOptions->eScope,
        sRecordId,
        XLLM__MEMORY_METADATA_KEY_CREATED_AT_UNIX
    );
    iUpdatedAtUnix = (pUseOptions->iUpdatedAtUnix > 0) ? pUseOptions->iUpdatedAtUnix : xrtToUnixTime(xrtNow());
    if ( iCreatedAtUnix <= 0 ) {
        iCreatedAtUnix = iUpdatedAtUnix;
    }
    tMetadata = xllm__memory_make_preference_metadata(
        pUseOptions->tMetadata,
        pUseOptions,
        iCreatedAtUnix,
        iUpdatedAtUnix
    );
    if ( !tMetadata ) {
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sTitle);
        xllm__free_cstr(&sSourceUri);
        xrtFree(sText);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build preference memory metadata");
        return XRT_NET_ERROR;
    }

    xllm_memory_ingest_options_init(&tIngestOptions);
    tIngestOptions.eScope = pUseOptions->eScope;
    tIngestOptions.sRecordId = sRecordId;
    tIngestOptions.sTitle = sTitle;
    tIngestOptions.sSourceUri = sSourceUri;
    tIngestOptions.sText = sText;
    tIngestOptions.bReplaceExisting = pUseOptions->bReplaceExisting;
    tIngestOptions.uChunkChars = pUseOptions->uChunkChars;
    tIngestOptions.uChunkOverlapChars = pUseOptions->uChunkOverlapChars;
    tIngestOptions.tMetadata = tMetadata;
    tIngestOptions.tVendorExtra = pUseOptions->tVendorExtra;
    iStatus = xllm_memory_ingest_text(pMemory, &tIngestOptions, pError);

    xvoUnref(tMetadata);
    xllm__free_cstr(&sRecordId);
    xllm__free_cstr(&sTitle);
    xllm__free_cstr(&sSourceUri);
    xrtFree(sText);
    return iStatus;
}
