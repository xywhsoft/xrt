static char *xllm__memory_ascii_lower_copy(const char *sText)
{
    size_t i;
    size_t iLen;
    char *sCopy;

    if ( !sText ) {
        return NULL;
    }

    iLen = strlen(sText);
    sCopy = (char *)xrtCalloc(iLen + 1u, sizeof(char));
    if ( !sCopy ) {
        return NULL;
    }

    for ( i = 0u; i < iLen; ++i ) {
        unsigned char c = (unsigned char)sText[i];
        sCopy[i] = (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : (char)c;
    }
    sCopy[iLen] = '\0';
    return sCopy;
}

enum {
    XLLM__MEMORY_SPARSE_MAX_QUERY_TERMS = 32,
    XLLM__MEMORY_SPARSE_MAX_TERM_BYTES = 96
};

typedef struct {
    char sTerm[XLLM__MEMORY_SPARSE_MAX_TERM_BYTES];
    uint32 uQueryCount;
    uint32 uDocumentFrequency;
} xllm__memory_sparse_query_term;

typedef struct {
    xllm__memory_sparse_query_term pTerms[XLLM__MEMORY_SPARSE_MAX_QUERY_TERMS];
    size_t iTermCount;
    uint32 uDocumentCount;
    double fAverageDocumentLength;
} xllm__memory_sparse_query;

static bool xllm__memory_sparse_next_token(
    const char **ppText,
    char *sToken,
    size_t iTokenCapacity
)
{
    const char *p;
    size_t iTokenLength = 0u;

    if ( !ppText || !*ppText || !sToken || iTokenCapacity == 0u ) {
        return false;
    }

    p = *ppText;
    while ( *p ) {
        while ( *p && !isalnum((unsigned char)*p) ) {
            ++p;
        }
        while ( *p && isalnum((unsigned char)*p) ) {
            unsigned char c = (unsigned char)*p;
            if ( iTokenLength + 1u < iTokenCapacity ) {
                sToken[iTokenLength++] =
                    (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : (char)c;
            }
            ++p;
        }
        if ( iTokenLength >= 2u ) {
            sToken[iTokenLength] = '\0';
            *ppText = p;
            return true;
        }
        iTokenLength = 0u;
    }

    sToken[0] = '\0';
    *ppText = p;
    return false;
}

static uint32 xllm__memory_sparse_token_count(const char *sText)
{
    const char *p = sText;
    char sToken[XLLM__MEMORY_SPARSE_MAX_TERM_BYTES];
    uint32 uCount = 0u;

    while ( xllm__memory_sparse_next_token(&p, sToken, sizeof(sToken)) ) {
        ++uCount;
    }
    return uCount;
}

static uint32 xllm__memory_sparse_term_count(
    const char *sText,
    const char *sTerm
)
{
    const char *p = sText;
    char sToken[XLLM__MEMORY_SPARSE_MAX_TERM_BYTES];
    uint32 uCount = 0u;

    if ( !sText || !sTerm || !sTerm[0] ) {
        return 0u;
    }
    while ( xllm__memory_sparse_next_token(&p, sToken, sizeof(sToken)) ) {
        if ( strcmp(sToken, sTerm) == 0 ) {
            ++uCount;
        }
    }
    return uCount;
}

static bool xllm__memory_sparse_phrase_contains_ci(
    const char *sText,
    const char *sQuery
)
{
    char *sTextLower = NULL;
    char *sQueryLower = NULL;
    char *p;
    size_t iQueryLength;

    if ( !sText || !sText[0] || !sQuery || !sQuery[0] ) {
        return false;
    }

    sTextLower = xllm__memory_ascii_lower_copy(sText);
    sQueryLower = xllm__memory_ascii_lower_copy(sQuery);
    if ( !sTextLower || !sQueryLower ) {
        xllm__free_cstr(&sTextLower);
        xllm__free_cstr(&sQueryLower);
        return false;
    }

    iQueryLength = strlen(sQueryLower);
    p = sTextLower;
    while ( (p = strstr(p, sQueryLower)) != NULL ) {
        bool bStartBoundary = (p == sTextLower) || !isalnum((unsigned char)*(p - 1));
        bool bEndBoundary = !isalnum((unsigned char)p[iQueryLength]);
        if ( bStartBoundary && bEndBoundary ) {
            xllm__free_cstr(&sTextLower);
            xllm__free_cstr(&sQueryLower);
            return true;
        }
        ++p;
    }

    xllm__free_cstr(&sTextLower);
    xllm__free_cstr(&sQueryLower);
    return false;
}

static void xllm__memory_sparse_query_add_term(
    xllm__memory_sparse_query *pQuery,
    const char *sTerm
)
{
    size_t i;

    if ( !pQuery || !sTerm || !sTerm[0] ) {
        return;
    }
    for ( i = 0u; i < pQuery->iTermCount; ++i ) {
        if ( strcmp(pQuery->pTerms[i].sTerm, sTerm) == 0 ) {
            ++pQuery->pTerms[i].uQueryCount;
            return;
        }
    }
    if ( pQuery->iTermCount >= XLLM__MEMORY_SPARSE_MAX_QUERY_TERMS ) {
        return;
    }
    snprintf(
        pQuery->pTerms[pQuery->iTermCount].sTerm,
        sizeof(pQuery->pTerms[pQuery->iTermCount].sTerm),
        "%s",
        sTerm
    );
    pQuery->pTerms[pQuery->iTermCount].uQueryCount = 1u;
    ++pQuery->iTermCount;
}

static void xllm__memory_sparse_query_init(
    xllm__memory_sparse_query *pQuery,
    const char *sQuery
)
{
    const char *p = sQuery;
    char sToken[XLLM__MEMORY_SPARSE_MAX_TERM_BYTES];

    if ( !pQuery ) {
        return;
    }
    memset(pQuery, 0, sizeof(*pQuery));
    while ( xllm__memory_sparse_next_token(&p, sToken, sizeof(sToken)) ) {
        xllm__memory_sparse_query_add_term(pQuery, sToken);
    }
}

static void xllm__memory_sparse_query_prepare_corpus_locked(
    const xllm_memory *pMemory,
    xllm__memory_sparse_query *pQuery
)
{
    uint64 uTotalDocumentLength = 0u;
    size_t i;

    if ( !pMemory || !pQuery || pQuery->iTermCount == 0u ) {
        return;
    }

    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        const xllm__memory_record_entry *pRecord = &pMemory->pRecords[i];
        size_t j;

        for ( j = 0u; j < pRecord->iChunkCount; ++j ) {
            const xllm__memory_chunk_entry *pChunk = &pRecord->pChunks[j];
            size_t k;

            ++pQuery->uDocumentCount;
            uTotalDocumentLength += (uint64)xllm__memory_sparse_token_count(pChunk->sText);
            for ( k = 0u; k < pQuery->iTermCount; ++k ) {
                if ( xllm__memory_sparse_term_count(pChunk->sText, pQuery->pTerms[k].sTerm) > 0u ) {
                    ++pQuery->pTerms[k].uDocumentFrequency;
                }
            }
        }
    }

    if ( pQuery->uDocumentCount > 0u && uTotalDocumentLength > 0u ) {
        pQuery->fAverageDocumentLength = (double)uTotalDocumentLength / (double)pQuery->uDocumentCount;
    }
    if ( pQuery->fAverageDocumentLength <= 0.0 ) {
        pQuery->fAverageDocumentLength = 1.0;
    }
}

static double xllm__memory_score_sparse_chunk(
    const xllm__memory_sparse_query *pQuery,
    const xllm__memory_chunk_entry *pChunk
)
{
    const double fK1 = 1.2;
    const double fB = 0.75;
    double fScore = 0.0;
    uint32 uMatchedTermCount = 0u;
    uint32 uDocumentLength;
    size_t i;

    if ( !pQuery || !pChunk || !pChunk->sText || pQuery->iTermCount == 0u || pQuery->uDocumentCount == 0u ) {
        return 0.0;
    }

    uDocumentLength = xllm__memory_sparse_token_count(pChunk->sText);
    if ( uDocumentLength == 0u ) {
        uDocumentLength = 1u;
    }

    for ( i = 0u; i < pQuery->iTermCount; ++i ) {
        const xllm__memory_sparse_query_term *pTerm = &pQuery->pTerms[i];
        uint32 uTermFrequency = xllm__memory_sparse_term_count(pChunk->sText, pTerm->sTerm);
        double fIdf;
        double fDenominator;

        if ( uTermFrequency == 0u ) {
            continue;
        }
        ++uMatchedTermCount;
        fIdf = log(1.0 + (((double)pQuery->uDocumentCount - (double)pTerm->uDocumentFrequency + 0.5) /
                          ((double)pTerm->uDocumentFrequency + 0.5)));
        fDenominator =
            (double)uTermFrequency +
            fK1 * (1.0 - fB + (fB * ((double)uDocumentLength / pQuery->fAverageDocumentLength)));
        if ( fDenominator > 0.0 ) {
            fScore += fIdf * ((((double)uTermFrequency) * (fK1 + 1.0)) / fDenominator) * (double)pTerm->uQueryCount;
        }
    }

    if ( uMatchedTermCount > 0u ) {
        fScore += ((double)uMatchedTermCount / (double)pQuery->iTermCount);
    }
    return fScore;
}

static double xllm__memory_score_text(const char *sQuery, const char *sText)
{
    char *sQueryLower = NULL;
    char *sTextLower = NULL;
    const char *p;
    double fScore = 0.0;
    uint32 uTokenCount = 0u;
    uint32 uMatchedTokenCount = 0u;

    if ( !sQuery || !sQuery[0] || !sText || !sText[0] ) {
        return 0.0;
    }

    sQueryLower = xllm__memory_ascii_lower_copy(sQuery);
    sTextLower = xllm__memory_ascii_lower_copy(sText);
    if ( !sQueryLower || !sTextLower ) {
        xllm__free_cstr(&sQueryLower);
        xllm__free_cstr(&sTextLower);
        return 0.0;
    }

    if ( strstr(sTextLower, sQueryLower) ) {
        fScore += 2.5;
    }

    p = sQueryLower;
    while ( *p ) {
        char sToken[96];
        size_t iTokenLen = 0u;

        while ( *p && !isalnum((unsigned char)*p) ) {
            ++p;
        }
        while ( *p && isalnum((unsigned char)*p) ) {
            if ( iTokenLen + 1u < sizeof(sToken) ) {
                sToken[iTokenLen++] = *p;
            }
            ++p;
        }
        if ( iTokenLen < 2u ) {
            continue;
        }
        sToken[iTokenLen] = '\0';
        ++uTokenCount;
        if ( strstr(sTextLower, sToken) ) {
            ++uMatchedTokenCount;
            fScore += 1.0;
        }
    }

    if ( uTokenCount > 0u ) {
        fScore += ((double)uMatchedTokenCount / (double)uTokenCount);
    }

    xllm__free_cstr(&sQueryLower);
    xllm__free_cstr(&sTextLower);
    return fScore;
}

static double xllm__memory_score_metadata_boost(
    const xllm__memory_sparse_query *pSparseQuery,
    const char *sQuery,
    const xllm__memory_record_entry *pRecord
)
{
    double fScore = 0.0;
    const char *sPath = NULL;
    const char *sBasename = NULL;
    size_t i;

    if ( !pRecord || !sQuery || !sQuery[0] ) {
        return 0.0;
    }

    if ( pRecord->sTitle && xllm__memory_sparse_phrase_contains_ci(pRecord->sTitle, sQuery) ) {
        fScore += 0.75;
    }
    if ( pRecord->sSourceUri && xllm__memory_sparse_phrase_contains_ci(pRecord->sSourceUri, sQuery) ) {
        fScore += 0.50;
    }
    if ( pRecord->tMetadata && xvoType(pRecord->tMetadata) == XVO_DT_TABLE ) {
        sPath = (const char *)xvoTableGetText(pRecord->tMetadata, (str)XLLM__MEMORY_METADATA_KEY_PATH, 0u);
        sBasename = (const char *)xvoTableGetText(pRecord->tMetadata, (str)XLLM__MEMORY_METADATA_KEY_BASENAME, 0u);
    }
    if ( sPath && xllm__memory_sparse_phrase_contains_ci(sPath, sQuery) ) {
        fScore += 0.60;
    }
    if ( sBasename && xllm__memory_sparse_phrase_contains_ci(sBasename, sQuery) ) {
        fScore += 0.80;
    }
    if ( pSparseQuery && pSparseQuery->iTermCount > 0u ) {
        for ( i = 0u; i < pSparseQuery->iTermCount; ++i ) {
            const char *sTerm = pSparseQuery->pTerms[i].sTerm;
            if ( (pRecord->sTitle && xllm__memory_sparse_term_count(pRecord->sTitle, sTerm) > 0u) ||
                 (pRecord->sSourceUri && xllm__memory_sparse_term_count(pRecord->sSourceUri, sTerm) > 0u) ||
                 (sPath && xllm__memory_sparse_term_count(sPath, sTerm) > 0u) ||
                 (sBasename && xllm__memory_sparse_term_count(sBasename, sTerm) > 0u) ) {
                fScore += 0.10;
            }
        }
    }
    return fScore;
}

static double xllm__memory_score_chunk_lexical(
    const xllm__memory_sparse_query *pSparseQuery,
    const char *sQuery,
    const xllm__memory_record_entry *pRecord,
    const xllm__memory_chunk_entry *pChunk
)
{
    double fLexicalScore;

    if ( !pChunk ) {
        return 0.0;
    }
    fLexicalScore =
        (pSparseQuery && pSparseQuery->iTermCount > 0u)
            ? xllm__memory_score_sparse_chunk(pSparseQuery, pChunk)
            : xllm__memory_score_text(sQuery, pChunk->sText);
    if ( xllm__memory_sparse_phrase_contains_ci(pChunk->sText, sQuery) ) {
        fLexicalScore += 2.5;
    }
    fLexicalScore += xllm__memory_score_metadata_boost(pSparseQuery, sQuery, pRecord);
    return fLexicalScore;
}

static uint32 xllm__memory_rank_chunk_lexical_locked(
    const xllm_memory *pMemory,
    const xllm__memory_sparse_query *pSparseQuery,
    const char *sQuery,
    double fChunkLexicalScore
)
{
    uint32 uRank = 1u;
    size_t i;

    if ( !pMemory || fChunkLexicalScore <= 0.0 ) {
        return 0u;
    }

    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        const xllm__memory_record_entry *pRecord = &pMemory->pRecords[i];
        size_t j;

        for ( j = 0u; j < pRecord->iChunkCount; ++j ) {
            double fOtherScore = xllm__memory_score_chunk_lexical(
                pSparseQuery,
                sQuery,
                pRecord,
                &pRecord->pChunks[j]
            );
            if ( fOtherScore > fChunkLexicalScore ) {
                ++uRank;
            }
        }
    }
    return uRank;
}

static int xllm__memory_sqlite_delete_sparse_postings_for_record_locked(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sRecordId
)
{
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    if ( !pMemory || !pMemory->pSqliteDb || !sRecordId ) {
        return SQLITE_OK;
    }

    iRc = sqlite3_prepare_v2(
        pMemory->pSqliteDb,
        "DELETE FROM xllm_memory_sparse_posting "
        "WHERE namespace = ?1 AND scope = ?2 AND record_id = ?3;",
        -1,
        &pStmt,
        NULL
    );
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    sqlite3_bind_text(pStmt, 1, xllm__memory_namespace_key(pMemory), -1, SQLITE_STATIC);
    sqlite3_bind_int(pStmt, 2, (int)eScope);
    sqlite3_bind_text(pStmt, 3, sRecordId, -1, SQLITE_TRANSIENT);
    iRc = sqlite3_step(pStmt);
    sqlite3_finalize(pStmt);
    return iRc == SQLITE_DONE ? SQLITE_OK : iRc;
}

static int xllm__memory_sqlite_insert_sparse_postings_locked(
    xllm_memory *pMemory,
    const xllm__memory_record_entry *pRecord,
    const xllm__memory_chunk_entry *pChunk
)
{
    sqlite3_stmt *pStmt = NULL;
    const char *p;
    char sToken[XLLM__MEMORY_SPARSE_MAX_TERM_BYTES];
    uint32 uTokenCount;
    int iRc;

    if ( !pMemory || !pMemory->pSqliteDb || !pRecord || !pChunk || !pRecord->sRecordId || !pChunk->sText ) {
        return SQLITE_OK;
    }

    uTokenCount = xllm__memory_sparse_token_count(pChunk->sText);
    if ( uTokenCount == 0u ) {
        return SQLITE_OK;
    }

    iRc = sqlite3_prepare_v2(
        pMemory->pSqliteDb,
        "INSERT INTO xllm_memory_sparse_posting("
        "namespace, scope, record_id, chunk_index, term, term_count, token_count"
        ") VALUES(?1, ?2, ?3, ?4, ?5, 1, ?6) "
        "ON CONFLICT(namespace, scope, record_id, chunk_index, term) DO UPDATE SET "
        "term_count = term_count + 1, token_count = excluded.token_count;",
        -1,
        &pStmt,
        NULL
    );
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }

    p = pChunk->sText;
    while ( xllm__memory_sparse_next_token(&p, sToken, sizeof(sToken)) ) {
        sqlite3_reset(pStmt);
        sqlite3_clear_bindings(pStmt);
        sqlite3_bind_text(pStmt, 1, xllm__memory_namespace_key(pMemory), -1, SQLITE_STATIC);
        sqlite3_bind_int(pStmt, 2, (int)pRecord->eScope);
        sqlite3_bind_text(pStmt, 3, pRecord->sRecordId, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(pStmt, 4, (int)pChunk->uChunkIndex);
        sqlite3_bind_text(pStmt, 5, sToken, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(pStmt, 6, (int)uTokenCount);
        iRc = sqlite3_step(pStmt);
        if ( iRc != SQLITE_DONE ) {
            sqlite3_finalize(pStmt);
            return iRc;
        }
    }

    sqlite3_finalize(pStmt);
    return SQLITE_OK;
}
