static char *xllm__memory_embedding_json_stringify(const float *pfValues, uint32 uValueCount)
{
    size_t i;
    size_t iCapacity;
    size_t iOffset = 0u;
    char *sJson;

    if ( !pfValues || uValueCount == 0u ) {
        return NULL;
    }

    iCapacity = 32u + ((size_t)uValueCount * 32u);
    sJson = (char *)xrtCalloc(iCapacity, sizeof(char));
    if ( !sJson ) {
        return NULL;
    }

    sJson[iOffset++] = '[';
    for ( i = 0u; i < (size_t)uValueCount; ++i ) {
        int iWritten = snprintf(sJson + iOffset, iCapacity - iOffset, (i == 0u) ? "%.9g" : ",%.9g", (double)pfValues[i]);
        if ( iWritten <= 0 || (size_t)iWritten >= (iCapacity - iOffset) ) {
            xrtFree(sJson);
            return NULL;
        }
        iOffset += (size_t)iWritten;
    }
    if ( iOffset + 2u > iCapacity ) {
        xrtFree(sJson);
        return NULL;
    }
    sJson[iOffset++] = ']';
    sJson[iOffset] = '\0';
    return sJson;
}

static char *xllm__memory_make_vector_table_name(const char *sNamespace)
{
    char sBuffer[128];
    uint64 uHash = xllm__memory_hash_namespace(sNamespace);
    int iWritten = snprintf(
        sBuffer,
        sizeof(sBuffer),
        "xllm_memory_vec_%08x%08x",
        (unsigned)(uHash >> 32u),
        (unsigned)(uHash & 0xffffffffu)
    );
    if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sBuffer) ) {
        return NULL;
    }
    return xllm__dup_cstr(sBuffer);
}

static int xllm__memory_sqlite_ensure_vector_table_locked(
    xllm_memory *pMemory,
    uint32 uVectorDim
)
{
    char sSql[512];
    int iRc;

    if ( !pMemory || !pMemory->pSqliteDb || !pMemory->bSqliteVectorExtensionLoaded || !pMemory->sVectorTableName || !uVectorDim ) {
        return SQLITE_OK;
    }
    if ( pMemory->uVectorDim > 0u && pMemory->uVectorDim != uVectorDim ) {
        return SQLITE_MISMATCH;
    }
    if ( pMemory->bVectorTableReady ) {
        return SQLITE_OK;
    }

    snprintf(
        sSql,
        sizeof(sSql),
        "CREATE VIRTUAL TABLE IF NOT EXISTS %s USING vec0(embedding float[%u] distance_metric=cosine);",
        pMemory->sVectorTableName,
        (unsigned)uVectorDim
    );
    iRc = xllm__memory_sqlite_exec(pMemory->pSqliteDb, sSql);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_meta_upsert_int_locked(pMemory, "embedding_dim", uVectorDim);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    pMemory->uVectorDim = uVectorDim;
    pMemory->bVectorTableReady = true;
    return SQLITE_OK;
}

static int xllm__memory_sqlite_delete_vectors_for_record_locked(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sRecordId
)
{
    sqlite3_stmt *pQueryStmt = NULL;
    sqlite3_stmt *pDeleteStmt = NULL;
    char sDeleteSql[256];
    int iRc;

    if ( !pMemory || !pMemory->pSqliteDb || !pMemory->bVectorTableReady || !pMemory->sVectorTableName || !sRecordId ) {
        return SQLITE_OK;
    }

    iRc = sqlite3_prepare_v2(
        pMemory->pSqliteDb,
        "SELECT rowid FROM xllm_memory_chunk WHERE namespace = ?1 AND scope = ?2 AND record_id = ?3;",
        -1,
        &pQueryStmt,
        NULL
    );
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    snprintf(sDeleteSql, sizeof(sDeleteSql), "DELETE FROM %s WHERE rowid = ?1;", pMemory->sVectorTableName);
    iRc = sqlite3_prepare_v2(pMemory->pSqliteDb, sDeleteSql, -1, &pDeleteStmt, NULL);
    if ( iRc != SQLITE_OK ) {
        sqlite3_finalize(pQueryStmt);
        return iRc;
    }

    sqlite3_bind_text(pQueryStmt, 1, xllm__memory_namespace_key(pMemory), -1, SQLITE_STATIC);
    sqlite3_bind_int(pQueryStmt, 2, (int)eScope);
    sqlite3_bind_text(pQueryStmt, 3, sRecordId, -1, SQLITE_TRANSIENT);
    while ( (iRc = sqlite3_step(pQueryStmt)) == SQLITE_ROW ) {
        sqlite3_int64 iRowId = sqlite3_column_int64(pQueryStmt, 0);
        sqlite3_reset(pDeleteStmt);
        sqlite3_clear_bindings(pDeleteStmt);
        sqlite3_bind_int64(pDeleteStmt, 1, iRowId);
        iRc = sqlite3_step(pDeleteStmt);
        if ( iRc != SQLITE_DONE ) {
            sqlite3_finalize(pQueryStmt);
            sqlite3_finalize(pDeleteStmt);
            return iRc;
        }
    }
    sqlite3_finalize(pQueryStmt);
    sqlite3_finalize(pDeleteStmt);
    return iRc == SQLITE_DONE ? SQLITE_OK : iRc;
}

static int xllm__memory_sqlite_upsert_chunk_vector_locked(
    xllm_memory *pMemory,
    const xllm__memory_chunk_entry *pChunk
)
{
    sqlite3_stmt *pStmt = NULL;
    char sSql[256];
    char *sEmbeddingJson = NULL;
    int iRc;

    if ( !pMemory || !pMemory->pSqliteDb || !pChunk || !pChunk->pfEmbedding || pChunk->uEmbeddingDim == 0u ) {
        return SQLITE_OK;
    }

    iRc = xllm__memory_sqlite_ensure_vector_table_locked(pMemory, pChunk->uEmbeddingDim);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    if ( !pMemory->bVectorTableReady ) {
        return SQLITE_OK;
    }

    sEmbeddingJson = xllm__memory_embedding_json_stringify(pChunk->pfEmbedding, pChunk->uEmbeddingDim);
    if ( !sEmbeddingJson ) {
        return SQLITE_NOMEM;
    }

    snprintf(
        sSql,
        sizeof(sSql),
        "INSERT INTO %s(rowid, embedding) VALUES(?1, ?2);",
        pMemory->sVectorTableName
    );
    iRc = sqlite3_prepare_v2(pMemory->pSqliteDb, sSql, -1, &pStmt, NULL);
    if ( iRc != SQLITE_OK ) {
        xrtFree(sEmbeddingJson);
        return iRc;
    }
    sqlite3_bind_int64(pStmt, 1, (sqlite3_int64)pChunk->iVectorRowId);
    sqlite3_bind_text(pStmt, 2, sEmbeddingJson, -1, SQLITE_TRANSIENT);
    iRc = sqlite3_step(pStmt);
    sqlite3_finalize(pStmt);
    xrtFree(sEmbeddingJson);
    return iRc == SQLITE_DONE ? SQLITE_OK : iRc;
}

static int xllm__memory_sqlite_query_vectors_locked(
    xllm_memory *pMemory,
    const float *pfQuery,
    uint32 uQueryDim,
    uint32 uMaxCandidates,
    xllm__memory_vector_candidate **ppCandidates,
    size_t *piCandidateCount
)
{
    sqlite3_stmt *pStmt = NULL;
    char sSql[320];
    char *sEmbeddingJson = NULL;
    int iRc;
    size_t iCapacity = 0u;

    if ( ppCandidates ) {
        *ppCandidates = NULL;
    }
    if ( piCandidateCount ) {
        *piCandidateCount = 0u;
    }
    if ( !pMemory || !pMemory->pSqliteDb || !pMemory->bVectorTableReady || !pMemory->sVectorTableName ||
         !pfQuery || uQueryDim == 0u || !ppCandidates || !piCandidateCount || uMaxCandidates == 0u ) {
        return SQLITE_OK;
    }
    if ( pMemory->uVectorDim > 0u && pMemory->uVectorDim != uQueryDim ) {
        return SQLITE_OK;
    }

    sEmbeddingJson = xllm__memory_embedding_json_stringify(pfQuery, uQueryDim);
    if ( !sEmbeddingJson ) {
        return SQLITE_NOMEM;
    }
    snprintf(
        sSql,
        sizeof(sSql),
        "SELECT rowid, distance FROM %s WHERE embedding MATCH ?1 AND k = ?2 ORDER BY distance ASC;",
        pMemory->sVectorTableName
    );
    iRc = sqlite3_prepare_v2(pMemory->pSqliteDb, sSql, -1, &pStmt, NULL);
    if ( iRc != SQLITE_OK ) {
        xrtFree(sEmbeddingJson);
        return iRc;
    }
    sqlite3_bind_text(pStmt, 1, sEmbeddingJson, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(pStmt, 2, (int)uMaxCandidates);

    while ( (iRc = sqlite3_step(pStmt)) == SQLITE_ROW ) {
        xllm__memory_vector_candidate tCandidate;
        double fDistance = sqlite3_column_double(pStmt, 1);
        memset(&tCandidate, 0, sizeof(tCandidate));
        tCandidate.iRowId = (int64)sqlite3_column_int64(pStmt, 0);
        tCandidate.fScore = 1.0 / (1.0 + (fDistance < 0.0 ? 0.0 : fDistance));
        if ( xllm__append_buffer(
                (void **)ppCandidates,
                sizeof(tCandidate),
                piCandidateCount,
                &iCapacity,
                &tCandidate
             ) != XRT_NET_OK ) {
            sqlite3_finalize(pStmt);
            xrtFree(sEmbeddingJson);
            if ( *ppCandidates ) {
                xrtFree(*ppCandidates);
                *ppCandidates = NULL;
            }
            *piCandidateCount = 0u;
            return SQLITE_NOMEM;
        }
    }
    sqlite3_finalize(pStmt);
    xrtFree(sEmbeddingJson);
    return iRc == SQLITE_DONE ? SQLITE_OK : iRc;
}

static int xllm__memory_sqlite_load_extension(xllm_memory *pMemory)
{
    int iRc;
    char *sError = NULL;
    char *sNormalizedPath = NULL;

    if ( !pMemory || !pMemory->pSqliteDb ) {
        return SQLITE_MISUSE;
    }
    if ( !pMemory->bLoadSqliteVectorExtension || !pMemory->sSqliteVectorExtensionPath || !pMemory->sSqliteVectorExtensionPath[0] ) {
        return SQLITE_OK;
    }

    iRc = sqlite3_enable_load_extension(pMemory->pSqliteDb, 1);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    sNormalizedPath = xllm__memory_sqlite_normalize_path(pMemory->sSqliteVectorExtensionPath);
    if ( !sNormalizedPath ) {
        sqlite3_enable_load_extension(pMemory->pSqliteDb, 0);
        return SQLITE_NOMEM;
    }
    iRc = sqlite3_load_extension(pMemory->pSqliteDb, sNormalizedPath, NULL, &sError);
    if ( iRc != SQLITE_OK ) {
        if ( sError ) {
            sqlite3_free(sError);
            sError = NULL;
        }
        iRc = sqlite3_load_extension(pMemory->pSqliteDb, sNormalizedPath, "sqlite3_vec_init", &sError);
    }
    if ( sError ) {
        sqlite3_free(sError);
    }
    sqlite3_enable_load_extension(pMemory->pSqliteDb, 0);
    if ( iRc == SQLITE_OK ) {
        pMemory->bSqliteVectorExtensionLoaded = true;
    }
    xllm__free_cstr(&sNormalizedPath);
    return iRc;
}
