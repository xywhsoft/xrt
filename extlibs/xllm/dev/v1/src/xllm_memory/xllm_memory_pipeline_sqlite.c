XLLM_API int xllm_memory_search(
    xllm_memory *pMemory,
    const xllm_memory_search_options *pOptions,
    xllm_memory_search_result *pResult,
    xllm_error *pError
)
{
    xllm_memory_search_options tDefaultOptions;
    const xllm_memory_search_options *pUseOptions = pOptions;
    xllm_memory_embedding tQueryEmbedding;
    xllm__memory_sparse_query tSparseQuery;
    xllm__memory_vector_candidate *pVectorCandidates = NULL;
    size_t iVectorCandidateCount = 0u;
    size_t iMaxHits;
    uint32 uVectorCandidateLimit;
    double fMinScore;
    int64 iNowUnix = 0;
    size_t i;

    if ( !pMemory || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory search requires a result object");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_search_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( !pUseOptions->sQuery || !pUseOptions->sQuery[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory search query is required");
        return XRT_NET_ERROR;
    }
    if ( pUseOptions->sMetadataValue && pUseOptions->sMetadataValue[0] &&
         (!pUseOptions->sMetadataKey || !pUseOptions->sMetadataKey[0]) ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory search metadata_value requires metadata_key");
        return XRT_NET_ERROR;
    }

    memset(&tQueryEmbedding, 0, sizeof(tQueryEmbedding));
    xllm__memory_sparse_query_init(&tSparseQuery, pUseOptions->sQuery);
    xllm_memory_search_result_reset(pResult);
    pResult->tVendorExtra = pUseOptions->tVendorExtra;
    xllm__xvalue_addref(pResult->tVendorExtra);
    iMaxHits = pUseOptions->uMaxHits > 0u ? (size_t)pUseOptions->uMaxHits : (size_t)pMemory->uDefaultMaxHits;
    if ( iMaxHits == 0u ) {
        iMaxHits = 5u;
    }
    uVectorCandidateLimit = (uint32)((iMaxHits * 8u) + 8u);
    fMinScore = pUseOptions->tMinScore.bSet ? pUseOptions->tMinScore.fValue : 0.25;
    if ( pUseOptions->bSkipExpired || pUseOptions->tRecencyWeight.bSet ) {
        iNowUnix = pUseOptions->iNowUnix > 0 ? pUseOptions->iNowUnix : xrtToUnixTime(xrtNow());
    }
    if ( pMemory->bEnableHybridSearch && pMemory->tEmbedder.pfnEmbedText ) {
        if ( xllm__memory_embed_text(
                &pMemory->tEmbedder,
                XLLM_MEMORY_EMBED_QUERY,
                pUseOptions->sQuery,
                &tQueryEmbedding,
                pError
             ) != XRT_NET_OK ) {
            xllm_memory_search_result_reset(pResult);
            return XRT_NET_ERROR;
        }
    }

    xrtMutexLock(pMemory->pMutex);
    xllm__memory_sparse_query_prepare_corpus_locked(pMemory, &tSparseQuery);
    if ( tQueryEmbedding.pfValues && tQueryEmbedding.uValueCount > 0u ) {
        if ( xllm__memory_sqlite_query_vectors_locked(
                pMemory,
                tQueryEmbedding.pfValues,
                tQueryEmbedding.uValueCount,
                uVectorCandidateLimit,
                &pVectorCandidates,
                &iVectorCandidateCount
             ) != SQLITE_OK ) {
            pVectorCandidates = NULL;
            iVectorCandidateCount = 0u;
        }
    }
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        const xllm__memory_record_entry *pRecord = &pMemory->pRecords[i];
        double fPriorityBoost = xllm__memory_score_priority_boost(pRecord, pUseOptions);
        double fRecencyBoost = xllm__memory_score_recency_boost(pRecord, pUseOptions, iNowUnix);
        size_t j;

        if ( !xllm__memory_record_matches_filters(
                pRecord,
                pUseOptions->eScope,
                pUseOptions->sConversationId,
                pUseOptions->sTurnId,
                pUseOptions->sRecordId,
                pUseOptions->sSourceUri,
                pUseOptions->sRecordIdContains,
                pUseOptions->sTitleContains,
                pUseOptions->sSourceUriContains,
                NULL,
                pUseOptions->sMetadataKey,
                pUseOptions->sMetadataValue
             ) ) {
            continue;
        }
        if ( pUseOptions->bSkipExpired && xllm__memory_record_is_expired(pRecord, iNowUnix) ) {
            continue;
        }

        for ( j = 0u; j < pRecord->iChunkCount; ++j ) {
            double fLexicalScore = 0.0;
            double fVectorScore = 0.0;
            double fRrfScore = 0.0;
            uint32 uLexicalRank = 0u;
            uint32 uVectorRank = 0u;
            double fScore = xllm__memory_score_chunk(
                pMemory,
                &tSparseQuery,
                &tQueryEmbedding,
                pVectorCandidates,
                iVectorCandidateCount,
                pUseOptions->sQuery,
                pRecord,
                &pRecord->pChunks[j],
                &fLexicalScore,
                &fVectorScore,
                &fRrfScore,
                &uLexicalRank,
                &uVectorRank
            );
            xllm_memory_hit tHit;

            if ( !xllm__memory_ascii_contains(pRecord->pChunks[j].sText, pUseOptions->sTextContains) ) {
                continue;
            }
            fScore += fPriorityBoost + fRecencyBoost;
            if ( fScore < fMinScore ) {
                continue;
            }
            if ( xllm__memory_hit_clone_from_chunk(
                    &tHit,
                    pRecord,
                    &pRecord->pChunks[j],
                    fScore,
                    fLexicalScore,
                    fVectorScore,
                    fRrfScore,
                    uLexicalRank,
                    uVectorRank,
                    pUseOptions->uMaxCharsPerHit
                 ) != XRT_NET_OK ) {
                xrtMutexUnlock(pMemory->pMutex);
                if ( pVectorCandidates ) {
                    xrtFree(pVectorCandidates);
                }
                xllm_memory_search_result_reset(pResult);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to clone memory search hit");
                return XRT_NET_ERROR;
            }
            if ( xllm__memory_result_insert_sorted(pResult, iMaxHits, &tHit) != XRT_NET_OK ) {
                xllm__memory_hit_reset(&tHit);
                xrtMutexUnlock(pMemory->pMutex);
                if ( pVectorCandidates ) {
                    xrtFree(pVectorCandidates);
                }
                xllm__memory_embedding_reset_with_embedder(&pMemory->tEmbedder, &tQueryEmbedding);
                xllm_memory_search_result_reset(pResult);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to append memory search hit");
                return XRT_NET_ERROR;
            }
        }
    }
    xrtMutexUnlock(pMemory->pMutex);
    if ( pVectorCandidates ) {
        xrtFree(pVectorCandidates);
    }
    xllm__memory_embedding_reset_with_embedder(&pMemory->tEmbedder, &tQueryEmbedding);
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_search_debug(
    xllm_memory *pMemory,
    const xllm_memory_retrieval_debug_options *pOptions,
    xllm_memory_retrieval_debug_dump *pDump,
    xllm_error *pError
)
{
    xllm_memory_retrieval_debug_options tDefaultOptions;
    const xllm_memory_retrieval_debug_options *pUseOptions = pOptions;
    const xllm_memory_search_options *pSearchOptions;
    xllm_memory_embedding tQueryEmbedding;
    xllm__memory_sparse_query tSparseQuery;
    xllm__memory_vector_candidate *pVectorCandidates = NULL;
    size_t iVectorCandidateCount = 0u;
    size_t iMaxHits;
    size_t iMaxCandidates;
    uint32 uVectorCandidateLimit;
    double fMinScore;
    int64 iNowUnix = 0;
    size_t i;

    if ( !pMemory || !pDump ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory search debug requires memory and output");
        return XRT_NET_ERROR;
    }
    if ( !pUseOptions ) {
        xllm_memory_retrieval_debug_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    pSearchOptions = &pUseOptions->tSearchOptions;
    if ( !pSearchOptions->sQuery || !pSearchOptions->sQuery[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory search debug query is required");
        return XRT_NET_ERROR;
    }
    if ( pSearchOptions->sMetadataValue && pSearchOptions->sMetadataValue[0] &&
         (!pSearchOptions->sMetadataKey || !pSearchOptions->sMetadataKey[0]) ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory search debug metadata_value requires metadata_key");
        return XRT_NET_ERROR;
    }

    memset(&tQueryEmbedding, 0, sizeof(tQueryEmbedding));
    xllm__memory_sparse_query_init(&tSparseQuery, pSearchOptions->sQuery);
    xllm_memory_retrieval_debug_dump_reset(pDump);
    pDump->sQuery = xllm__dup_cstr(pSearchOptions->sQuery);
    pDump->tVendorExtra = pUseOptions->tVendorExtra;
    xllm__xvalue_addref(pDump->tVendorExtra);
    if ( !pDump->sQuery ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to clone memory search debug query");
        return XRT_NET_ERROR;
    }

    iMaxHits = pSearchOptions->uMaxHits > 0u ? (size_t)pSearchOptions->uMaxHits : (size_t)pMemory->uDefaultMaxHits;
    if ( iMaxHits == 0u ) {
        iMaxHits = 5u;
    }
    iMaxCandidates = pUseOptions->uMaxCandidates > 0u ? (size_t)pUseOptions->uMaxCandidates : 32u;
    uVectorCandidateLimit = (uint32)((iMaxHits * 8u) + 8u);
    fMinScore = pSearchOptions->tMinScore.bSet ? pSearchOptions->tMinScore.fValue : 0.25;
    pDump->fMinScore = fMinScore;
    pDump->uVectorCandidateLimit = uVectorCandidateLimit;
    if ( pSearchOptions->bSkipExpired || pSearchOptions->tRecencyWeight.bSet ) {
        iNowUnix = pSearchOptions->iNowUnix > 0 ? pSearchOptions->iNowUnix : xrtToUnixTime(xrtNow());
    }
    if ( pMemory->bEnableHybridSearch && pMemory->tEmbedder.pfnEmbedText ) {
        if ( xllm__memory_embed_text(
                &pMemory->tEmbedder,
                XLLM_MEMORY_EMBED_QUERY,
                pSearchOptions->sQuery,
                &tQueryEmbedding,
                pError
             ) != XRT_NET_OK ) {
            xllm_memory_retrieval_debug_dump_reset(pDump);
            return XRT_NET_ERROR;
        }
    }

    xrtMutexLock(pMemory->pMutex);
    pDump->eScheme = pMemory->eScheme;
    pDump->bHybridSearchEnabled = pMemory->bEnableHybridSearch;
    pDump->fLexicalWeight = pMemory->fLexicalWeight;
    pDump->fVectorWeight = pMemory->fVectorWeight;
    xllm__memory_sparse_query_prepare_corpus_locked(pMemory, &tSparseQuery);
    if ( tSparseQuery.iTermCount > 0u ) {
        pDump->pTerms = (xllm_memory_retrieval_debug_term *)xrtCalloc(
            tSparseQuery.iTermCount,
            sizeof(*pDump->pTerms)
        );
        if ( !pDump->pTerms ) {
            xrtMutexUnlock(pMemory->pMutex);
            xllm__memory_embedding_reset_with_embedder(&pMemory->tEmbedder, &tQueryEmbedding);
            xllm_memory_retrieval_debug_dump_reset(pDump);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate memory search debug terms");
            return XRT_NET_ERROR;
        }
        pDump->iTermCount = tSparseQuery.iTermCount;
        for ( i = 0u; i < tSparseQuery.iTermCount; ++i ) {
            pDump->pTerms[i].sTerm = xllm__dup_cstr(tSparseQuery.pTerms[i].sTerm);
            pDump->pTerms[i].uQueryCount = tSparseQuery.pTerms[i].uQueryCount;
            pDump->pTerms[i].uDocumentFrequency = tSparseQuery.pTerms[i].uDocumentFrequency;
            if ( !pDump->pTerms[i].sTerm ) {
                xrtMutexUnlock(pMemory->pMutex);
                xllm__memory_embedding_reset_with_embedder(&pMemory->tEmbedder, &tQueryEmbedding);
                xllm_memory_retrieval_debug_dump_reset(pDump);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to clone memory search debug term");
                return XRT_NET_ERROR;
            }
        }
    }
    if ( tQueryEmbedding.pfValues && tQueryEmbedding.uValueCount > 0u ) {
        if ( xllm__memory_sqlite_query_vectors_locked(
                pMemory,
                tQueryEmbedding.pfValues,
                tQueryEmbedding.uValueCount,
                uVectorCandidateLimit,
                &pVectorCandidates,
                &iVectorCandidateCount
             ) != SQLITE_OK ) {
            pVectorCandidates = NULL;
            iVectorCandidateCount = 0u;
        }
    }
    pDump->iVectorCandidateCount = iVectorCandidateCount;

    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        const xllm__memory_record_entry *pRecord = &pMemory->pRecords[i];
        double fPriorityBoost = xllm__memory_score_priority_boost(pRecord, pSearchOptions);
        double fRecencyBoost = xllm__memory_score_recency_boost(pRecord, pSearchOptions, iNowUnix);
        size_t j;

        if ( !xllm__memory_record_matches_filters(
                pRecord,
                pSearchOptions->eScope,
                pSearchOptions->sConversationId,
                pSearchOptions->sTurnId,
                pSearchOptions->sRecordId,
                pSearchOptions->sSourceUri,
                pSearchOptions->sRecordIdContains,
                pSearchOptions->sTitleContains,
                pSearchOptions->sSourceUriContains,
                NULL,
                pSearchOptions->sMetadataKey,
                pSearchOptions->sMetadataValue
             ) ) {
            continue;
        }
        if ( pSearchOptions->bSkipExpired && xllm__memory_record_is_expired(pRecord, iNowUnix) ) {
            continue;
        }
        ++pDump->iFilteredRecordCount;

        for ( j = 0u; j < pRecord->iChunkCount; ++j ) {
            xllm__memory_chunk_entry *pChunk = &pRecord->pChunks[j];
            double fLexicalScore = 0.0;
            double fVectorScore = 0.0;
            double fRrfScore = 0.0;
            uint32 uLexicalRank = 0u;
            uint32 uVectorRank = 0u;
            double fScore;
            bool bPassedMinScore;
            xllm_memory_hit tHit;

            if ( !xllm__memory_ascii_contains(pChunk->sText, pSearchOptions->sTextContains) ) {
                continue;
            }
            ++pDump->iFilteredChunkCount;
            fScore = xllm__memory_score_chunk(
                pMemory,
                &tSparseQuery,
                &tQueryEmbedding,
                pVectorCandidates,
                iVectorCandidateCount,
                pSearchOptions->sQuery,
                pRecord,
                pChunk,
                &fLexicalScore,
                &fVectorScore,
                &fRrfScore,
                &uLexicalRank,
                &uVectorRank
            );
            fScore += fPriorityBoost + fRecencyBoost;
            bPassedMinScore = fScore >= fMinScore;
            ++pDump->iScoredCandidateCount;
            if ( !bPassedMinScore ) {
                ++pDump->iBelowMinScoreCount;
            }
            if ( (bPassedMinScore || pUseOptions->bIncludeBelowMinScore) &&
                 xllm__memory_retrieval_debug_append_candidate(
                    pDump,
                    iMaxCandidates,
                    pRecord,
                    pChunk,
                    fScore,
                    fLexicalScore,
                    fVectorScore,
                    fRrfScore,
                    uLexicalRank,
                    uVectorRank,
                    bPassedMinScore
                 ) != XRT_NET_OK ) {
                xrtMutexUnlock(pMemory->pMutex);
                if ( pVectorCandidates ) {
                    xrtFree(pVectorCandidates);
                }
                xllm__memory_embedding_reset_with_embedder(&pMemory->tEmbedder, &tQueryEmbedding);
                xllm_memory_retrieval_debug_dump_reset(pDump);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to append memory search debug candidate");
                return XRT_NET_ERROR;
            }
            if ( !bPassedMinScore ) {
                continue;
            }
            if ( xllm__memory_hit_clone_from_chunk(
                    &tHit,
                    pRecord,
                    pChunk,
                    fScore,
                    fLexicalScore,
                    fVectorScore,
                    fRrfScore,
                    uLexicalRank,
                    uVectorRank,
                    pSearchOptions->uMaxCharsPerHit
                 ) != XRT_NET_OK ) {
                xrtMutexUnlock(pMemory->pMutex);
                if ( pVectorCandidates ) {
                    xrtFree(pVectorCandidates);
                }
                xllm__memory_embedding_reset_with_embedder(&pMemory->tEmbedder, &tQueryEmbedding);
                xllm_memory_retrieval_debug_dump_reset(pDump);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to clone memory search debug hit");
                return XRT_NET_ERROR;
            }
            if ( xllm__memory_result_insert_sorted(&pDump->tSearchResult, iMaxHits, &tHit) != XRT_NET_OK ) {
                xllm__memory_hit_reset(&tHit);
                xrtMutexUnlock(pMemory->pMutex);
                if ( pVectorCandidates ) {
                    xrtFree(pVectorCandidates);
                }
                xllm__memory_embedding_reset_with_embedder(&pMemory->tEmbedder, &tQueryEmbedding);
                xllm_memory_retrieval_debug_dump_reset(pDump);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to append memory search debug hit");
                return XRT_NET_ERROR;
            }
        }
    }
    xllm__memory_retrieval_debug_mark_included(pDump);
    xrtMutexUnlock(pMemory->pMutex);
    if ( pVectorCandidates ) {
        xrtFree(pVectorCandidates);
    }
    xllm__memory_embedding_reset_with_embedder(&pMemory->tEmbedder, &tQueryEmbedding);
    return XRT_NET_OK;
}
