static double xllm__memory_score_chunk_vector(
    const xllm_memory *pMemory,
    const xllm_memory_embedding *pQueryEmbedding,
    const xllm__memory_vector_candidate *pVectorCandidates,
    size_t iVectorCandidateCount,
    const xllm__memory_chunk_entry *pChunk
)
{
    size_t i;

    if ( pMemory->bEnableHybridSearch && pVectorCandidates && iVectorCandidateCount > 0u ) {
        for ( i = 0u; i < iVectorCandidateCount; ++i ) {
            if ( pVectorCandidates[i].iRowId == pChunk->iVectorRowId ) {
                return pVectorCandidates[i].fScore;
            }
        }
    } else if ( pMemory->bEnableHybridSearch &&
                pQueryEmbedding &&
                pQueryEmbedding->pfValues &&
                pQueryEmbedding->uValueCount > 0u &&
                pChunk->pfEmbedding &&
                pChunk->uEmbeddingDim == pQueryEmbedding->uValueCount ) {
        return xllm__memory_embedding_cosine_score(
            pQueryEmbedding->pfValues,
            pChunk->pfEmbedding,
            pQueryEmbedding->uValueCount
        );
    }

    return 0.0;
}

static uint32 xllm__memory_rank_chunk_vector_locked(
    const xllm_memory *pMemory,
    const xllm_memory_embedding *pQueryEmbedding,
    const xllm__memory_vector_candidate *pVectorCandidates,
    size_t iVectorCandidateCount,
    const xllm__memory_chunk_entry *pChunk,
    double fChunkVectorScore
)
{
    uint32 uRank = 1u;
    size_t i;

    if ( !pMemory || !pChunk || fChunkVectorScore <= 0.0 ) {
        return 0u;
    }
    if ( pVectorCandidates && iVectorCandidateCount > 0u ) {
        for ( i = 0u; i < iVectorCandidateCount; ++i ) {
            if ( pVectorCandidates[i].iRowId == pChunk->iVectorRowId ) {
                return (uint32)(i + 1u);
            }
        }
        return 0u;
    }

    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        const xllm__memory_record_entry *pRecord = &pMemory->pRecords[i];
        size_t j;

        for ( j = 0u; j < pRecord->iChunkCount; ++j ) {
            double fOtherScore = xllm__memory_score_chunk_vector(
                pMemory,
                pQueryEmbedding,
                NULL,
                0u,
                &pRecord->pChunks[j]
            );
            if ( fOtherScore > fChunkVectorScore ) {
                ++uRank;
            }
        }
    }
    return uRank;
}
