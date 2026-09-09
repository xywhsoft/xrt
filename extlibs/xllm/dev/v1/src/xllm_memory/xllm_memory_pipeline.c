static double xllm__memory_score_rrf(
    double fLexicalWeight,
    double fVectorWeight,
    uint32 uLexicalRank,
    uint32 uVectorRank
)
{
    const double fRankConstant = 60.0;
    double fScore = 0.0;

    if ( fLexicalWeight > 0.0 && uLexicalRank > 0u ) {
        fScore += fLexicalWeight / (fRankConstant + (double)uLexicalRank);
    }
    if ( fVectorWeight > 0.0 && uVectorRank > 0u ) {
        fScore += fVectorWeight / (fRankConstant + (double)uVectorRank);
    }
    return fScore * 100.0;
}

static double xllm__memory_score_chunk(
    const xllm_memory *pMemory,
    const xllm__memory_sparse_query *pSparseQuery,
    const xllm_memory_embedding *pQueryEmbedding,
    const xllm__memory_vector_candidate *pVectorCandidates,
    size_t iVectorCandidateCount,
    const char *sQuery,
    const xllm__memory_record_entry *pRecord,
    const xllm__memory_chunk_entry *pChunk,
    double *pfLexicalScore,
    double *pfVectorScore,
    double *pfRrfScore,
    uint32 *puLexicalRank,
    uint32 *puVectorRank
)
{
    double fLexicalWeight;
    double fVectorWeight;
    double fLexicalScore;
    double fVectorScore;
    double fRrfScore = 0.0;
    uint32 uLexicalRank = 0u;
    uint32 uVectorRank = 0u;

    if ( pfLexicalScore ) {
        *pfLexicalScore = 0.0;
    }
    if ( pfVectorScore ) {
        *pfVectorScore = 0.0;
    }
    if ( pfRrfScore ) {
        *pfRrfScore = 0.0;
    }
    if ( puLexicalRank ) {
        *puLexicalRank = 0u;
    }
    if ( puVectorRank ) {
        *puVectorRank = 0u;
    }
    if ( !pMemory || !pChunk ) {
        return 0.0;
    }

    fLexicalWeight = pMemory->fLexicalWeight;
    fVectorWeight = pMemory->fVectorWeight;
    fLexicalScore = xllm__memory_score_chunk_lexical(pSparseQuery, sQuery, pRecord, pChunk);
    fVectorScore = xllm__memory_score_chunk_vector(
        pMemory,
        pQueryEmbedding,
        pVectorCandidates,
        iVectorCandidateCount,
        pChunk
    );

    if ( pMemory->bEnableHybridSearch && fVectorWeight > 0.0 &&
         (fVectorScore > 0.0 || pVectorCandidates || (pQueryEmbedding && pQueryEmbedding->pfValues)) ) {
        uLexicalRank = xllm__memory_rank_chunk_lexical_locked(pMemory, pSparseQuery, sQuery, fLexicalScore);
        uVectorRank = xllm__memory_rank_chunk_vector_locked(
            pMemory,
            pQueryEmbedding,
            pVectorCandidates,
            iVectorCandidateCount,
            pChunk,
            fVectorScore
        );
        fRrfScore = xllm__memory_score_rrf(fLexicalWeight, fVectorWeight, uLexicalRank, uVectorRank);
        if ( pfLexicalScore ) {
            *pfLexicalScore = fLexicalScore;
        }
        if ( pfVectorScore ) {
            *pfVectorScore = fVectorScore;
        }
        if ( pfRrfScore ) {
            *pfRrfScore = fRrfScore;
        }
        if ( puLexicalRank ) {
            *puLexicalRank = uLexicalRank;
        }
        if ( puVectorRank ) {
            *puVectorRank = uVectorRank;
        }
        return fRrfScore + (fLexicalScore * 0.05);
    }

    if ( pfLexicalScore ) {
        *pfLexicalScore = fLexicalScore;
    }
    if ( pfVectorScore ) {
        *pfVectorScore = fVectorScore;
    }
    if ( fVectorWeight <= 0.0 ) {
        return fLexicalScore;
    }
    if ( fLexicalWeight <= 0.0 ) {
        return fVectorScore;
    }
    return (fLexicalScore * fLexicalWeight) + (fVectorScore * fVectorWeight);
}

static int xllm__memory_hit_clone_from_chunk(
    xllm_memory_hit *pHit,
    const xllm__memory_record_entry *pRecord,
    const xllm__memory_chunk_entry *pChunk,
    double fScore,
    double fLexicalScore,
    double fVectorScore,
    double fRrfScore,
    uint32 uLexicalRank,
    uint32 uVectorRank,
    uint32 uMaxCharsPerHit
)
{
    size_t iLen;

    if ( !pHit || !pRecord || !pChunk ) {
        return XRT_NET_ERROR;
    }

    memset(pHit, 0, sizeof(*pHit));
    pHit->eScope = pRecord->eScope;
    pHit->sRecordId = xllm__dup_cstr(pRecord->sRecordId);
    pHit->sChunkId = xllm__dup_cstr(pChunk->sChunkId);
    pHit->sTitle = xllm__dup_cstr(pRecord->sTitle);
    pHit->sSourceUri = xllm__dup_cstr(pRecord->sSourceUri);
    pHit->fScore = fScore;
    pHit->fLexicalScore = fLexicalScore;
    pHit->fVectorScore = fVectorScore;
    pHit->fRrfScore = fRrfScore;
    pHit->uChunkIndex = pChunk->uChunkIndex;
    pHit->iStartByte = pChunk->iStartByte;
    pHit->iEndByte = pChunk->iEndByte;
    pHit->uLexicalRank = uLexicalRank;
    pHit->uVectorRank = uVectorRank;
    pHit->sRetrievalProfileId = xllm__dup_cstr(pChunk->sRetrievalProfileId);
    pHit->sEmbedProfileId = xllm__dup_cstr(pChunk->sEmbedProfileId);
    pHit->sIndexProfileId = xllm__dup_cstr(pChunk->sIndexProfileId);
    pHit->tVendorExtra = pRecord->tVendorExtra;
    pHit->tMetadata = xllm__memory_clone_public_metadata(pRecord->tMetadata);
    xllm__xvalue_addref(pHit->tVendorExtra);
    if ( (pRecord->tMetadata && !pHit->tMetadata) ||
         (pChunk->sRetrievalProfileId && !pHit->sRetrievalProfileId) ||
         (pChunk->sEmbedProfileId && !pHit->sEmbedProfileId) ||
         (pChunk->sIndexProfileId && !pHit->sIndexProfileId) ) {
        xllm__memory_hit_reset(pHit);
        return XRT_NET_ERROR;
    }

    iLen = pChunk->sText ? strlen(pChunk->sText) : 0u;
    if ( uMaxCharsPerHit > 0u && iLen > (size_t)uMaxCharsPerHit ) {
        iLen = (size_t)uMaxCharsPerHit;
    }
    pHit->sText = (char *)xrtCalloc(iLen + 1u, sizeof(char));
    if ( !pHit->sText ) {
        xllm__memory_hit_reset(pHit);
        return XRT_NET_ERROR;
    }
    if ( iLen > 0u ) {
        memcpy((char *)pHit->sText, pChunk->sText, iLen);
    }
    ((char *)pHit->sText)[iLen] = '\0';
    return XRT_NET_OK;
}

static int xllm__memory_result_insert_sorted(
    xllm_memory_search_result *pResult,
    size_t iMaxHits,
    xllm_memory_hit *pCandidate
)
{
    size_t iInsertAt;
    size_t i;

    if ( !pResult || !pCandidate ) {
        return XRT_NET_ERROR;
    }

    iInsertAt = pResult->iHitCount;
    for ( i = 0u; i < pResult->iHitCount; ++i ) {
        if ( pCandidate->fScore > pResult->pHits[i].fScore ) {
            iInsertAt = i;
            break;
        }
    }

    if ( pResult->iHitCount < iMaxHits ) {
        xllm_memory_hit *pNewHits = (xllm_memory_hit *)xrtRealloc(
            pResult->pHits,
            (pResult->iHitCount + 1u) * sizeof(xllm_memory_hit)
        );
        if ( !pNewHits ) {
            return XRT_NET_ERROR;
        }
        pResult->pHits = pNewHits;
        for ( i = pResult->iHitCount; i > iInsertAt; --i ) {
            pResult->pHits[i] = pResult->pHits[i - 1u];
        }
        pResult->pHits[iInsertAt] = *pCandidate;
        ++pResult->iHitCount;
        memset(pCandidate, 0, sizeof(*pCandidate));
        return XRT_NET_OK;
    }

    if ( iInsertAt >= iMaxHits ) {
        return XRT_NET_OK;
    }

    xllm__memory_hit_reset(&pResult->pHits[iMaxHits - 1u]);
    for ( i = iMaxHits - 1u; i > iInsertAt; --i ) {
        pResult->pHits[i] = pResult->pHits[i - 1u];
    }
    pResult->pHits[iInsertAt] = *pCandidate;
    memset(pCandidate, 0, sizeof(*pCandidate));
    return XRT_NET_OK;
}

static bool xllm__memory_record_matches_filters(
    const xllm__memory_record_entry *pRecord,
    xllm_memory_scope eScope,
    const char *sConversationId,
    const char *sTurnId,
    const char *sRecordId,
    const char *sSourceUri,
    const char *sRecordIdContains,
    const char *sTitleContains,
    const char *sSourceUriContains,
    const char *sTextContains,
    const char *sMetadataKey,
    const char *sMetadataValue
)
{
    if ( !pRecord ) {
        return false;
    }
    if ( eScope != XLLM_MEMORY_SCOPE_ANY && pRecord->eScope != eScope ) {
        return false;
    }
    if ( sConversationId && sConversationId[0] ) {
        const char *sValue = NULL;

        if ( !pRecord->tMetadata || xvoType(pRecord->tMetadata) != XVO_DT_TABLE ) {
            return false;
        }
        sValue = (const char *)xvoTableGetText(pRecord->tMetadata, (str)XLLM__MEMORY_METADATA_KEY_CONVERSATION_ID, 0u);
        if ( !sValue || strcmp(sValue, sConversationId) != 0 ) {
            return false;
        }
    }
    if ( sTurnId && sTurnId[0] ) {
        const char *sValue = NULL;

        if ( !pRecord->tMetadata || xvoType(pRecord->tMetadata) != XVO_DT_TABLE ) {
            return false;
        }
        sValue = (const char *)xvoTableGetText(pRecord->tMetadata, (str)XLLM__MEMORY_METADATA_KEY_TURN_ID, 0u);
        if ( !sValue || strcmp(sValue, sTurnId) != 0 ) {
            return false;
        }
    }
    if ( sRecordId && sRecordId[0] ) {
        if ( !pRecord->sRecordId || strcmp(pRecord->sRecordId, sRecordId) != 0 ) {
            return false;
        }
    }
    if ( sSourceUri && sSourceUri[0] ) {
        if ( !pRecord->sSourceUri || strcmp(pRecord->sSourceUri, sSourceUri) != 0 ) {
            return false;
        }
    }
    if ( !xllm__memory_ascii_contains(pRecord->sRecordId, sRecordIdContains) ) {
        return false;
    }
    if ( !xllm__memory_ascii_contains(pRecord->sTitle, sTitleContains) ) {
        return false;
    }
    if ( !xllm__memory_ascii_contains(pRecord->sSourceUri, sSourceUriContains) ) {
        return false;
    }
    if ( !xllm__memory_ascii_contains(pRecord->sText, sTextContains) ) {
        return false;
    }
    if ( !xllm__memory_metadata_matches(pRecord->tMetadata, sMetadataKey, sMetadataValue) ) {
        return false;
    }
    return true;
}

static bool xllm__memory_chunk_matches_filters(
    const xllm__memory_record_entry *pRecord,
    const xllm__memory_chunk_entry *pChunk,
    const xllm_memory_list_options *pOptions
)
{
    if ( !pRecord || !pChunk || !pOptions ) {
        return false;
    }
    if ( !xllm__memory_record_matches_filters(
            pRecord,
            pOptions->eScope,
            pOptions->sConversationId,
            pOptions->sTurnId,
            pOptions->sRecordId,
            pOptions->sSourceUri,
            pOptions->sRecordIdContains,
            pOptions->sTitleContains,
            pOptions->sSourceUriContains,
            NULL,
            pOptions->sMetadataKey,
            pOptions->sMetadataValue
         ) ) {
        return false;
    }
    if ( pOptions->sChunkId && pOptions->sChunkId[0] ) {
        if ( !pChunk->sChunkId || strcmp(pChunk->sChunkId, pOptions->sChunkId) != 0 ) {
            return false;
        }
    }
    if ( !xllm__memory_ascii_contains(pChunk->sText, pOptions->sTextContains) ) {
        return false;
    }
    return true;
}

XLLM_API void xllm_memory_search_result_reset(xllm_memory_search_result *pResult)
{
    size_t i;

    if ( !pResult ) {
        return;
    }

    for ( i = 0u; i < pResult->iHitCount; ++i ) {
        xllm__memory_hit_reset(&pResult->pHits[i]);
    }
    if ( pResult->pHits ) {
        xrtFree(pResult->pHits);
    }
    xllm__xvalue_release(&pResult->tVendorExtra);
    memset(pResult, 0, sizeof(*pResult));
}

static void xllm__memory_retrieval_debug_candidate_reset(xllm_memory_retrieval_debug_candidate *pCandidate)
{
    if ( !pCandidate ) {
        return;
    }

    xllm__free_cstr((char **)&pCandidate->sRecordId);
    xllm__free_cstr((char **)&pCandidate->sChunkId);
    xllm__free_cstr((char **)&pCandidate->sTitle);
    xllm__free_cstr((char **)&pCandidate->sSourceUri);
    memset(pCandidate, 0, sizeof(*pCandidate));
}

static int xllm__memory_retrieval_debug_append_candidate(
    xllm_memory_retrieval_debug_dump *pDump,
    size_t iMaxCandidates,
    const xllm__memory_record_entry *pRecord,
    const xllm__memory_chunk_entry *pChunk,
    double fScore,
    double fLexicalScore,
    double fVectorScore,
    double fRrfScore,
    uint32 uLexicalRank,
    uint32 uVectorRank,
    bool bPassedMinScore
)
{
    xllm_memory_retrieval_debug_candidate *pNewCandidates;
    xllm_memory_retrieval_debug_candidate *pCandidate;

    if ( !pDump || !pRecord || !pChunk ) {
        return XRT_NET_ERROR;
    }
    if ( iMaxCandidates > 0u && pDump->iCandidateCount >= iMaxCandidates ) {
        return XRT_NET_OK;
    }

    pNewCandidates = (xllm_memory_retrieval_debug_candidate *)xrtRealloc(
        pDump->pCandidates,
        (pDump->iCandidateCount + 1u) * sizeof(*pDump->pCandidates)
    );
    if ( !pNewCandidates ) {
        return XRT_NET_ERROR;
    }
    pDump->pCandidates = pNewCandidates;
    pCandidate = &pDump->pCandidates[pDump->iCandidateCount];
    memset(pCandidate, 0, sizeof(*pCandidate));

    pCandidate->eScope = pRecord->eScope;
    pCandidate->sRecordId = xllm__dup_cstr(pRecord->sRecordId);
    pCandidate->sChunkId = xllm__dup_cstr(pChunk->sChunkId);
    pCandidate->sTitle = xllm__dup_cstr(pRecord->sTitle);
    pCandidate->sSourceUri = xllm__dup_cstr(pRecord->sSourceUri);
    pCandidate->fScore = fScore;
    pCandidate->fLexicalScore = fLexicalScore;
    pCandidate->fVectorScore = fVectorScore;
    pCandidate->fRrfScore = fRrfScore;
    pCandidate->uLexicalRank = uLexicalRank;
    pCandidate->uVectorRank = uVectorRank;
    pCandidate->bPassedMinScore = bPassedMinScore;
    if ( (pRecord->sRecordId && !pCandidate->sRecordId) ||
         (pChunk->sChunkId && !pCandidate->sChunkId) ||
         (pRecord->sTitle && !pCandidate->sTitle) ||
         (pRecord->sSourceUri && !pCandidate->sSourceUri) ) {
        xllm__memory_retrieval_debug_candidate_reset(pCandidate);
        return XRT_NET_ERROR;
    }

    ++pDump->iCandidateCount;
    return XRT_NET_OK;
}

static void xllm__memory_retrieval_debug_mark_included(xllm_memory_retrieval_debug_dump *pDump)
{
    size_t i;

    if ( !pDump ) {
        return;
    }
    for ( i = 0u; i < pDump->iCandidateCount; ++i ) {
        size_t j;
        xllm_memory_retrieval_debug_candidate *pCandidate = &pDump->pCandidates[i];

        for ( j = 0u; j < pDump->tSearchResult.iHitCount; ++j ) {
            const xllm_memory_hit *pHit = &pDump->tSearchResult.pHits[j];
            const char *sCandidateRecordId = pCandidate->sRecordId ? pCandidate->sRecordId : "";
            const char *sCandidateChunkId = pCandidate->sChunkId ? pCandidate->sChunkId : "";
            const char *sHitRecordId = pHit->sRecordId ? pHit->sRecordId : "";
            const char *sHitChunkId = pHit->sChunkId ? pHit->sChunkId : "";

            if ( strcmp(sCandidateRecordId, sHitRecordId) == 0 && strcmp(sCandidateChunkId, sHitChunkId) == 0 ) {
                pCandidate->bIncludedInResult = true;
                break;
            }
        }
    }
}

XLLM_API void xllm_memory_retrieval_debug_dump_reset(xllm_memory_retrieval_debug_dump *pDump)
{
    size_t i;

    if ( !pDump ) {
        return;
    }

    xllm__free_cstr((char **)&pDump->sQuery);
    for ( i = 0u; i < pDump->iTermCount; ++i ) {
        xllm__free_cstr((char **)&pDump->pTerms[i].sTerm);
    }
    if ( pDump->pTerms ) {
        xrtFree(pDump->pTerms);
    }
    for ( i = 0u; i < pDump->iCandidateCount; ++i ) {
        xllm__memory_retrieval_debug_candidate_reset(&pDump->pCandidates[i]);
    }
    if ( pDump->pCandidates ) {
        xrtFree(pDump->pCandidates);
    }
    xllm_memory_search_result_reset(&pDump->tSearchResult);
    xllm__xvalue_release(&pDump->tVendorExtra);
    memset(pDump, 0, sizeof(*pDump));
}

static int xllm__memory_make_context_block_from_search_result(
    const xllm_memory_search_result *pResult,
    const xllm_memory_context_options *pOptions,
    xllm_context_block *pBlock,
    xllm_error *pError
);

static int xllm__memory_append_context_block(
    xllm_context_block **ppContextBlocks,
    size_t *piContextBlockCount,
    xllm_context_block *pBlock,
    xllm_error *pError
);

XLLM_API int xllm_memory_apply_search_to_request(
    xllm_request *pRequest,
    const xllm_memory_search_result *pResult,
    const xllm_memory_context_options *pOptions,
    xllm_error *pError
)
{
    xllm_context_block tBlock;
    int iStatus;

    if ( !pRequest || !pResult || pResult->iHitCount == 0u ) {
        return XRT_NET_OK;
    }

    memset(&tBlock, 0, sizeof(tBlock));
    iStatus = xllm__memory_make_context_block_from_search_result(pResult, pOptions, &tBlock, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }
    if ( tBlock.iMessageCount == 0u || !tBlock.pMessages ) {
        return XRT_NET_OK;
    }

    iStatus = xllm__memory_append_context_block(
        &pRequest->pContextBlocks,
        &pRequest->iContextBlockCount,
        &tBlock,
        pError
    );
    if ( iStatus != XRT_NET_OK ) {
        xllm__context_block_free(&tBlock);
        return iStatus;
    }

    return XRT_NET_OK;
}

static int xllm__memory_make_context_block_from_search_result(
    const xllm_memory_search_result *pResult,
    const xllm_memory_context_options *pOptions,
    xllm_context_block *pBlock,
    xllm_error *pError
)
{
    xllm_memory_context_options tDefaultOptions;
    const xllm_memory_context_options *pUseOptions = pOptions;
    xllm_context_block tBlock;
    xllm_message tMessage;
    xllm_content_part tPart;
    xllm_context_block_kind eKind;
    size_t iCandidateCount;
    size_t iMaxHitCount;
    size_t iHitCount;
    size_t iSelectedHitCount = 0u;
    size_t iBufferSize = 64u;
    size_t i;
    size_t *pSelectedHitIndices = NULL;
    size_t *pTextLengths = NULL;
    char *sText = NULL;
    size_t iOffset = 0u;
    uint64 uRemainingTotalChars = 0u;
    bool bHasTotalCharsBudget = false;

    if ( !pResult || !pBlock || pResult->iHitCount == 0u ) {
        return XRT_NET_OK;
    }

    if ( !pUseOptions ) {
        xllm_memory_context_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    iCandidateCount = pResult->iHitCount;
    iMaxHitCount = pUseOptions->uMaxHits > 0u ? (size_t)pUseOptions->uMaxHits : iCandidateCount;
    if ( iMaxHitCount > iCandidateCount ) {
        iMaxHitCount = iCandidateCount;
    }
    bHasTotalCharsBudget = pUseOptions->uMaxTotalChars > 0u;
    uRemainingTotalChars = (uint64)pUseOptions->uMaxTotalChars;
    pSelectedHitIndices = (size_t *)xrtCalloc(iCandidateCount > 0u ? iCandidateCount : 1u, sizeof(*pSelectedHitIndices));
    pTextLengths = (size_t *)xrtCalloc(iCandidateCount > 0u ? iCandidateCount : 1u, sizeof(*pTextLengths));
    if ( (iCandidateCount > 0u && !pSelectedHitIndices) ||
         (iCandidateCount > 0u && !pTextLengths) ) {
        xrtFree(pSelectedHitIndices);
        xrtFree(pTextLengths);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate memory context hit lengths");
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < iCandidateCount; ++i ) {
        size_t iTextLength = pResult->pHits[i].sText ? strlen(pResult->pHits[i].sText) : 0u;
        size_t j;
        bool bDuplicateRecord = false;

        if ( iSelectedHitCount >= iMaxHitCount ) {
            break;
        }
        if ( pUseOptions->tMinScore.bSet && pResult->pHits[i].fScore < pUseOptions->tMinScore.fValue ) {
            continue;
        }
        if ( pUseOptions->bDistinctByRecord ) {
            for ( j = 0u; j < iSelectedHitCount; ++j ) {
                const xllm_memory_hit *pSelectedHit = &pResult->pHits[pSelectedHitIndices[j]];
                const char *sSelectedRecordId = pSelectedHit->sRecordId ? pSelectedHit->sRecordId : "";
                const char *sCandidateRecordId = pResult->pHits[i].sRecordId ? pResult->pHits[i].sRecordId : "";

                if ( strcmp(sSelectedRecordId, sCandidateRecordId) == 0 ) {
                    bDuplicateRecord = true;
                    break;
                }
            }
        }
        if ( bDuplicateRecord ) {
            continue;
        }
        if ( pUseOptions->uMaxCharsPerHit > 0u && iTextLength > (size_t)pUseOptions->uMaxCharsPerHit ) {
            iTextLength = (size_t)pUseOptions->uMaxCharsPerHit;
        }
        if ( bHasTotalCharsBudget ) {
            if ( uRemainingTotalChars == 0u ) {
                break;
            }
            if ( (uint64)iTextLength > uRemainingTotalChars ) {
                iTextLength = (size_t)uRemainingTotalChars;
            }
            uRemainingTotalChars -= (uint64)iTextLength;
        }
        pSelectedHitIndices[iSelectedHitCount] = i;
        pTextLengths[iSelectedHitCount] = iTextLength;
        ++iSelectedHitCount;
        iBufferSize += 160u + iTextLength;
        if ( pResult->pHits[i].sTitle ) {
            iBufferSize += strlen(pResult->pHits[i].sTitle);
        }
        if ( pResult->pHits[i].sSourceUri ) {
            iBufferSize += strlen(pResult->pHits[i].sSourceUri);
        }
        if ( pResult->pHits[i].sChunkId ) {
            iBufferSize += strlen(pResult->pHits[i].sChunkId);
        }
        if ( pResult->pHits[i].sRecordId ) {
            iBufferSize += strlen(pResult->pHits[i].sRecordId);
        }
    }
    iHitCount = iSelectedHitCount;
    if ( iHitCount == 0u ) {
        xrtFree(pSelectedHitIndices);
        xrtFree(pTextLengths);
        return XRT_NET_OK;
    }

    if ( pUseOptions->pfnRender ) {
        int iRenderStatus = pUseOptions->pfnRender(
            pUseOptions->pRenderCtx,
            pResult,
            pSelectedHitIndices,
            pTextLengths,
            iHitCount,
            &sText,
            pError
        );

        xrtFree(pSelectedHitIndices);
        xrtFree(pTextLengths);
        pSelectedHitIndices = NULL;
        pTextLengths = NULL;
        if ( iRenderStatus != XRT_NET_OK ) {
            if ( sText ) {
                xrtFree(sText);
            }
            return iRenderStatus;
        }
        if ( !sText || !sText[0] ) {
            if ( sText ) {
                xrtFree(sText);
            }
            return XRT_NET_OK;
        }
    } else {
        if ( pUseOptions->sLabel ) {
            iBufferSize += strlen(pUseOptions->sLabel);
        }

        sText = (char *)xrtCalloc(iBufferSize + 1u, sizeof(char));
        if ( !sText ) {
            xrtFree(pSelectedHitIndices);
            xrtFree(pTextLengths);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate memory context text");
            return XRT_NET_ERROR;
        }

        iOffset += (size_t)snprintf(
            sText + iOffset,
            iBufferSize + 1u - iOffset,
            "%s%sRetrieved %s context (%u hits):\n",
            pUseOptions->sLabel ? pUseOptions->sLabel : "",
            pUseOptions->sLabel ? "\n" : "",
            xllm__memory_scope_name(pResult->pHits[0].eScope),
            (unsigned)iHitCount
        );
        for ( i = 0u; i < iHitCount && iOffset < iBufferSize; ++i ) {
            const xllm_memory_hit *pHit = &pResult->pHits[pSelectedHitIndices[i]];
            size_t iTextLength = pTextLengths[i];

            iOffset += (size_t)snprintf(
                sText + iOffset,
                iBufferSize + 1u - iOffset,
                "\n[%u] score=%.3f record=%s\n",
                (unsigned)(i + 1u),
                pHit->fScore,
                pHit->sRecordId ? pHit->sRecordId : "(unknown)"
            );
            if ( pHit->sTitle && iOffset < iBufferSize ) {
                iOffset += (size_t)snprintf(sText + iOffset, iBufferSize + 1u - iOffset, "Title: %s\n", pHit->sTitle);
            }
            if ( pHit->sSourceUri && iOffset < iBufferSize ) {
                iOffset += (size_t)snprintf(sText + iOffset, iBufferSize + 1u - iOffset, "Source: %s\n", pHit->sSourceUri);
            }
            if ( iOffset < iBufferSize ) {
                iOffset += (size_t)snprintf(
                    sText + iOffset,
                    iBufferSize + 1u - iOffset,
                    "Chunk: %s index=%u bytes=%llu-%llu\n",
                    pHit->sChunkId ? pHit->sChunkId : "(unknown)",
                    (unsigned)pHit->uChunkIndex,
                    (unsigned long long)pHit->iStartByte,
                    (unsigned long long)pHit->iEndByte
                );
            }
            if ( iOffset < iBufferSize ) {
                iOffset += (size_t)snprintf(sText + iOffset, iBufferSize + 1u - iOffset, "Content:\n%.*s\n", (int)iTextLength, pHit->sText ? pHit->sText : "");
            }
        }
        xrtFree(pSelectedHitIndices);
        pSelectedHitIndices = NULL;
        xrtFree(pTextLengths);
        pTextLengths = NULL;
    }

    memset(&tPart, 0, sizeof(tPart));
    tPart.eKind = XLLM_PART_TEXT;
    tPart.as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    tPart.as.tSource.sMimeType = xllm__dup_cstr("text/plain");
    tPart.as.tSource.as.sText = sText;
    sText = NULL;

    memset(&tMessage, 0, sizeof(tMessage));
    tMessage.eRole = XLLM_ROLE_SYSTEM;
    tMessage.pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !tMessage.pParts ) {
        xllm__content_part_free(&tPart);
        xrtFree(pSelectedHitIndices);
        xrtFree(pTextLengths);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate memory context message");
        return XRT_NET_ERROR;
    }
    tMessage.pParts[0] = tPart;
    memset(&tPart, 0, sizeof(tPart));
    tMessage.iPartCount = 1u;

    memset(&tBlock, 0, sizeof(tBlock));
    eKind = pUseOptions->eKindOverride != 0 ? pUseOptions->eKindOverride : xllm__memory_scope_to_context_kind(pResult->pHits[0].eScope);
    tBlock.eKind = eKind;
    tBlock.iPriority = pUseOptions->iPriority;
    tBlock.bPinned = pUseOptions->bPinned;
    tBlock.tVendorExtra = pUseOptions->tVendorExtra;
    xllm__xvalue_addref(tBlock.tVendorExtra);
    tBlock.pMessages = (xllm_message *)xrtCalloc(1u, sizeof(xllm_message));
    if ( !tBlock.pMessages ) {
        xllm__message_free(&tMessage);
        xllm__xvalue_release(&tBlock.tVendorExtra);
        xrtFree(pSelectedHitIndices);
        xrtFree(pTextLengths);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate memory context block");
        return XRT_NET_ERROR;
    }
    tBlock.pMessages[0] = tMessage;
    memset(&tMessage, 0, sizeof(tMessage));
    tBlock.iMessageCount = 1u;

    *pBlock = tBlock;
    memset(&tBlock, 0, sizeof(tBlock));
    return XRT_NET_OK;
}

static int xllm__memory_append_context_block(
    xllm_context_block **ppContextBlocks,
    size_t *piContextBlockCount,
    xllm_context_block *pBlock,
    xllm_error *pError
)
{
    xllm_context_block *pNewBlocks;

    if ( !ppContextBlocks || !piContextBlockCount || !pBlock ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "invalid memory context append target");
        return XRT_NET_ERROR;
    }

    pNewBlocks = (xllm_context_block *)xrtRealloc(
        *ppContextBlocks,
        (*piContextBlockCount + 1u) * sizeof(xllm_context_block)
    );
    if ( !pNewBlocks ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to append memory context block");
        return XRT_NET_ERROR;
    }

    *ppContextBlocks = pNewBlocks;
    (*ppContextBlocks)[*piContextBlockCount] = *pBlock;
    ++(*piContextBlockCount);
    memset(pBlock, 0, sizeof(*pBlock));
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_apply_search_to_turn(
    xllm_turn *pTurn,
    const xllm_memory_search_result *pResult,
    const xllm_memory_context_options *pOptions,
    xllm_error *pError
)
{
    xllm_context_block tBlock;
    int iStatus;

    if ( !pTurn || !pResult || pResult->iHitCount == 0u ) {
        return XRT_NET_OK;
    }

    memset(&tBlock, 0, sizeof(tBlock));
    iStatus = xllm__memory_make_context_block_from_search_result(pResult, pOptions, &tBlock, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }
    if ( tBlock.iMessageCount == 0u || !tBlock.pMessages ) {
        return XRT_NET_OK;
    }

    iStatus = xllm__memory_append_context_block(
        &pTurn->pContextBlocks,
        &pTurn->iContextBlockCount,
        &tBlock,
        pError
    );
    if ( iStatus != XRT_NET_OK ) {
        xllm__context_block_free(&tBlock);
        return iStatus;
    }

    return XRT_NET_OK;
}

XLLM_API int xllm_memory_search_and_apply_to_request(
    xllm_memory *pMemory,
    const xllm_memory_search_options *pSearchOptions,
    xllm_request *pRequest,
    const xllm_memory_context_options *pContextOptions,
    xllm_error *pError
)
{
    xllm_memory_search_result tSearchResult;
    int iStatus;

    memset(&tSearchResult, 0, sizeof(tSearchResult));
    iStatus = xllm_memory_search(pMemory, pSearchOptions, &tSearchResult, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    iStatus = xllm_memory_apply_search_to_request(pRequest, &tSearchResult, pContextOptions, pError);
    xllm_memory_search_result_reset(&tSearchResult);
    return iStatus;
}

XLLM_API int xllm_memory_search_and_apply_to_turn(
    xllm_memory *pMemory,
    const xllm_memory_search_options *pSearchOptions,
    xllm_turn *pTurn,
    const xllm_memory_context_options *pContextOptions,
    xllm_error *pError
)
{
    xllm_memory_search_result tSearchResult;
    int iStatus;

    memset(&tSearchResult, 0, sizeof(tSearchResult));
    iStatus = xllm_memory_search(pMemory, pSearchOptions, &tSearchResult, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    iStatus = xllm_memory_apply_search_to_turn(pTurn, &tSearchResult, pContextOptions, pError);
    xllm_memory_search_result_reset(&tSearchResult);
    return iStatus;
}

static int xllm__memory_search_and_apply_from_turn(
    xllm_memory *pMemory,
    const xllm_turn *pSourceTurn,
    xllm_request *pRequest,
    xllm_turn *pTurn,
    const xllm_memory_turn_search_apply_options *pOptions,
    xllm_error *pError
)
{
    xllm_memory_turn_search_apply_options tDefaultOptions;
    const xllm_memory_turn_search_apply_options *pUseOptions = pOptions;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    char *sDerivedQuery = NULL;
    int iStatus;

    if ( !pMemory || !pSourceTurn || (!pRequest && !pTurn) ) {
        xllm__error_set(
            pError,
            XLLM_ERROR_INVALID_REQUEST,
            "memory turn search/apply requires memory, source turn, and apply target"
        );
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_turn_search_apply_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    memset(&tSearchResult, 0, sizeof(tSearchResult));
    tSearchOptions = pUseOptions->tSearchOptions;
    if ( !tSearchOptions.sQuery || !tSearchOptions.sQuery[0] ) {
        iStatus = xllm__memory_build_search_query_from_turn(pSourceTurn, pUseOptions, &sDerivedQuery, pError);
        if ( iStatus != XRT_NET_OK ) {
            return iStatus;
        }
        tSearchOptions.sQuery = sDerivedQuery;
    }

    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, pError);
    if ( iStatus == XRT_NET_OK ) {
        if ( pRequest ) {
            iStatus = xllm_memory_apply_search_to_request(
                pRequest,
                &tSearchResult,
                &pUseOptions->tContextOptions,
                pError
            );
        } else {
            iStatus = xllm_memory_apply_search_to_turn(
                pTurn,
                &tSearchResult,
                &pUseOptions->tContextOptions,
                pError
            );
        }
    }

    xllm_memory_search_result_reset(&tSearchResult);
    if ( sDerivedQuery ) {
        xrtFree(sDerivedQuery);
    }
    return iStatus;
}

XLLM_API int xllm_memory_search_and_apply_from_turn_to_request(
    xllm_memory *pMemory,
    const xllm_turn *pSourceTurn,
    xllm_request *pRequest,
    const xllm_memory_turn_search_apply_options *pOptions,
    xllm_error *pError
)
{
    return xllm__memory_search_and_apply_from_turn(
        pMemory,
        pSourceTurn,
        pRequest,
        NULL,
        pOptions,
        pError
    );
}

XLLM_API int xllm_memory_search_and_apply_from_turn_to_turn(
    xllm_memory *pMemory,
    const xllm_turn *pSourceTurn,
    xllm_turn *pTurn,
    const xllm_memory_turn_search_apply_options *pOptions,
    xllm_error *pError
)
{
    return xllm__memory_search_and_apply_from_turn(
        pMemory,
        pSourceTurn,
        NULL,
        pTurn,
        pOptions,
        pError
    );
}
