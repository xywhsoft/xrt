#include <sqlite3.h>

static const uint32 XLLM__MEMORY_SQLITE_SCHEMA_VERSION = 1u;
static const char *XLLM__MEMORY_META_SCHEMA_VERSION = "schema_version";
static const char *XLLM__MEMORY_META_MEMORY_PROFILE_ID = "memory_profile_id";
static const char *XLLM__MEMORY_META_MEMORY_SCHEME = "memory_scheme";

static int xllm__memory_sqlite_exec(sqlite3 *pDb, const char *sSql)
{
    char *sError = NULL;
    int iRc;

    if ( !pDb || !sSql ) {
        return SQLITE_MISUSE;
    }

    iRc = sqlite3_exec(pDb, sSql, NULL, NULL, &sError);
    if ( sError ) {
        sqlite3_free(sError);
    }
    return iRc;
}

static bool xllm__memory_sqlite_path_is_file_backed(const char *sPath)
{
    if ( !sPath || !sPath[0] ) {
        return false;
    }
    if ( strcmp(sPath, ":memory:") == 0 ) {
        return false;
    }
    if ( xllm__memory_ascii_strnicmp(sPath, "file::memory:", 13u) == 0 ) {
        return false;
    }
    return true;
}

static int xllm__memory_sqlite_configure_connection(xllm_memory *pMemory)
{
    int iRc;

    if ( !pMemory || !pMemory->pSqliteDb ) {
        return SQLITE_MISUSE;
    }

    sqlite3_busy_timeout(pMemory->pSqliteDb, (int)pMemory->uSqliteBusyTimeoutMs);
    iRc = xllm__memory_sqlite_exec(pMemory->pSqliteDb, "PRAGMA foreign_keys = ON;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    pMemory->bSqliteWalEnabled = false;
    if ( pMemory->bSqliteWalRequested && xllm__memory_sqlite_path_is_file_backed(pMemory->sSqlitePath) ) {
        iRc = xllm__memory_sqlite_exec(pMemory->pSqliteDb, "PRAGMA journal_mode = WAL;");
        if ( iRc != SQLITE_OK ) {
            return iRc;
        }
        iRc = xllm__memory_sqlite_exec(pMemory->pSqliteDb, "PRAGMA synchronous = NORMAL;");
        if ( iRc != SQLITE_OK ) {
            return iRc;
        }
        pMemory->bSqliteWalEnabled = true;
    }

    return SQLITE_OK;
}

static char *xllm__memory_json_stringify(xvalue tValue)
{
    if ( !tValue ) {
        return NULL;
    }
    return (char *)xrtStringifyJSON(tValue, FALSE, NULL);
}

static xvalue xllm__memory_json_parse(const char *sText)
{
    if ( !sText || !sText[0] ) {
        xvalue tNull = 0;
        return tNull;
    }
    return xrtParseJSON((str)sText, strlen(sText));
}

static char *xllm__memory_sqlite_normalize_path(const char *sPath)
{
    size_t i;
    size_t iLen;
    char *sCopy;

    if ( !sPath || !sPath[0] ) {
        return NULL;
    }

    iLen = strlen(sPath);
    sCopy = xllm__dup_cstr(sPath);
    if ( !sCopy ) {
        return NULL;
    }
    for ( i = 0u; i < iLen; ++i ) {
        if ( sCopy[i] == '\\' ) {
            sCopy[i] = '/';
        }
    }
    return sCopy;
}

static int xllm__memory_sqlite_bind_nullable_text(sqlite3_stmt *pStmt, int iIndex, const char *sText)
{
    if ( sText ) {
        return sqlite3_bind_text(pStmt, iIndex, sText, -1, SQLITE_TRANSIENT);
    }
    return sqlite3_bind_null(pStmt, iIndex);
}

static int xllm__memory_sqlite_bind_nullable_embedding(
    sqlite3_stmt *pStmt,
    int iBlobIndex,
    int iDimIndex,
    const float *pfValues,
    uint32 uValueCount
)
{
    int iRc;

    if ( pfValues && uValueCount > 0u ) {
        iRc = sqlite3_bind_blob(pStmt, iBlobIndex, pfValues, (int)(uValueCount * (uint32)sizeof(float)), SQLITE_TRANSIENT);
        if ( iRc != SQLITE_OK ) {
            return iRc;
        }
        return sqlite3_bind_int(pStmt, iDimIndex, (int)uValueCount);
    }
    iRc = sqlite3_bind_null(pStmt, iBlobIndex);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    return sqlite3_bind_null(pStmt, iDimIndex);
}

static int xllm__memory_sqlite_ensure_column(sqlite3 *pDb, const char *sTable, const char *sColumn, const char *sAlterSql)
{
    sqlite3_stmt *pStmt = NULL;
    int iRc;
    bool bFound = false;
    char sPragmaSql[128];

    if ( !pDb || !sTable || !sColumn || !sAlterSql ) {
        return SQLITE_MISUSE;
    }

    snprintf(sPragmaSql, sizeof(sPragmaSql), "PRAGMA table_info(%s);", sTable);
    iRc = sqlite3_prepare_v2(pDb, sPragmaSql, -1, &pStmt, NULL);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    while ( (iRc = sqlite3_step(pStmt)) == SQLITE_ROW ) {
        const char *sExisting = (const char *)sqlite3_column_text(pStmt, 1);
        if ( sExisting && strcmp(sExisting, sColumn) == 0 ) {
            bFound = true;
            break;
        }
    }
    sqlite3_finalize(pStmt);
    if ( bFound ) {
        return SQLITE_OK;
    }
    return xllm__memory_sqlite_exec(pDb, sAlterSql);
}

static int xllm__memory_sqlite_create_schema(sqlite3 *pDb)
{
    static const char *sSchemaSql =
        "PRAGMA foreign_keys = ON;"
        "CREATE TABLE IF NOT EXISTS xllm_memory_record ("
        "  namespace TEXT NOT NULL,"
        "  scope INTEGER NOT NULL,"
        "  record_id TEXT NOT NULL,"
        "  title TEXT,"
        "  source_uri TEXT,"
        "  text_body TEXT NOT NULL,"
        "  metadata_json TEXT,"
        "  vendor_extra_json TEXT,"
        "  memory_profile_id TEXT,"
        "  retrieval_profile_id TEXT,"
        "  embed_profile_id TEXT,"
        "  index_profile_id TEXT,"
        "  profile_version INTEGER,"
        "  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "  PRIMARY KEY(namespace, scope, record_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS xllm_memory_meta ("
        "  namespace TEXT NOT NULL,"
        "  key TEXT NOT NULL,"
        "  value_int INTEGER,"
        "  value_text TEXT,"
        "  PRIMARY KEY(namespace, key)"
        ");"
        "CREATE TABLE IF NOT EXISTS xllm_memory_chunk ("
        "  namespace TEXT NOT NULL,"
        "  scope INTEGER NOT NULL,"
        "  record_id TEXT NOT NULL,"
        "  chunk_index INTEGER NOT NULL,"
        "  chunk_id TEXT NOT NULL,"
        "  text_body TEXT NOT NULL,"
        "  embedding_blob BLOB,"
        "  embedding_dim INTEGER,"
        "  memory_profile_id TEXT,"
        "  chunk_profile_id TEXT,"
        "  retrieval_profile_id TEXT,"
        "  embed_profile_id TEXT,"
        "  index_profile_id TEXT,"
        "  profile_version INTEGER,"
        "  content_hash INTEGER,"
        "  start_byte INTEGER,"
        "  end_byte INTEGER,"
        "  previous_chunk_id TEXT,"
        "  next_chunk_id TEXT,"
        "  PRIMARY KEY(namespace, scope, record_id, chunk_index),"
        "  FOREIGN KEY(namespace, scope, record_id)"
        "    REFERENCES xllm_memory_record(namespace, scope, record_id)"
        "    ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS xllm_memory_record_scope_idx "
        "  ON xllm_memory_record(namespace, scope);"
        "CREATE INDEX IF NOT EXISTS xllm_memory_chunk_scope_idx "
        "  ON xllm_memory_chunk(namespace, scope, record_id);"
        "CREATE TABLE IF NOT EXISTS xllm_memory_sparse_posting ("
        "  namespace TEXT NOT NULL,"
        "  scope INTEGER NOT NULL,"
        "  record_id TEXT NOT NULL,"
        "  chunk_index INTEGER NOT NULL,"
        "  term TEXT NOT NULL,"
        "  term_count INTEGER NOT NULL,"
        "  token_count INTEGER NOT NULL,"
        "  PRIMARY KEY(namespace, scope, record_id, chunk_index, term),"
        "  FOREIGN KEY(namespace, scope, record_id, chunk_index)"
        "    REFERENCES xllm_memory_chunk(namespace, scope, record_id, chunk_index)"
        "    ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS xllm_memory_sparse_posting_term_idx "
        "  ON xllm_memory_sparse_posting(namespace, scope, term);";
    int iRc = xllm__memory_sqlite_exec(pDb, sSchemaSql);

    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(
        pDb,
        "xllm_memory_chunk",
        "embedding_blob",
        "ALTER TABLE xllm_memory_chunk ADD COLUMN embedding_blob BLOB;"
    );
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(
        pDb,
        "xllm_memory_chunk",
        "embedding_dim",
        "ALTER TABLE xllm_memory_chunk ADD COLUMN embedding_dim INTEGER;"
    );
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_record", "memory_profile_id", "ALTER TABLE xllm_memory_record ADD COLUMN memory_profile_id TEXT;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_record", "retrieval_profile_id", "ALTER TABLE xllm_memory_record ADD COLUMN retrieval_profile_id TEXT;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_record", "embed_profile_id", "ALTER TABLE xllm_memory_record ADD COLUMN embed_profile_id TEXT;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_record", "index_profile_id", "ALTER TABLE xllm_memory_record ADD COLUMN index_profile_id TEXT;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_record", "profile_version", "ALTER TABLE xllm_memory_record ADD COLUMN profile_version INTEGER;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_chunk", "memory_profile_id", "ALTER TABLE xllm_memory_chunk ADD COLUMN memory_profile_id TEXT;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_chunk", "chunk_profile_id", "ALTER TABLE xllm_memory_chunk ADD COLUMN chunk_profile_id TEXT;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_chunk", "retrieval_profile_id", "ALTER TABLE xllm_memory_chunk ADD COLUMN retrieval_profile_id TEXT;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_chunk", "embed_profile_id", "ALTER TABLE xllm_memory_chunk ADD COLUMN embed_profile_id TEXT;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_chunk", "index_profile_id", "ALTER TABLE xllm_memory_chunk ADD COLUMN index_profile_id TEXT;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_chunk", "profile_version", "ALTER TABLE xllm_memory_chunk ADD COLUMN profile_version INTEGER;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_chunk", "content_hash", "ALTER TABLE xllm_memory_chunk ADD COLUMN content_hash INTEGER;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_chunk", "start_byte", "ALTER TABLE xllm_memory_chunk ADD COLUMN start_byte INTEGER;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_chunk", "end_byte", "ALTER TABLE xllm_memory_chunk ADD COLUMN end_byte INTEGER;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_chunk", "previous_chunk_id", "ALTER TABLE xllm_memory_chunk ADD COLUMN previous_chunk_id TEXT;");
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    return xllm__memory_sqlite_ensure_column(pDb, "xllm_memory_chunk", "next_chunk_id", "ALTER TABLE xllm_memory_chunk ADD COLUMN next_chunk_id TEXT;");
}

static int xllm__memory_sqlite_meta_get_int_locked(
    xllm_memory *pMemory,
    const char *sKey,
    uint32 *puValue
)
{
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    if ( puValue ) {
        *puValue = 0u;
    }
    if ( !pMemory || !pMemory->pSqliteDb || !sKey ) {
        return SQLITE_OK;
    }

    iRc = sqlite3_prepare_v2(
        pMemory->pSqliteDb,
        "SELECT value_int FROM xllm_memory_meta WHERE namespace = ?1 AND key = ?2;",
        -1,
        &pStmt,
        NULL
    );
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    sqlite3_bind_text(pStmt, 1, xllm__memory_namespace_key(pMemory), -1, SQLITE_STATIC);
    sqlite3_bind_text(pStmt, 2, sKey, -1, SQLITE_STATIC);
    iRc = sqlite3_step(pStmt);
    if ( iRc == SQLITE_ROW ) {
        if ( puValue ) {
            *puValue = (uint32)sqlite3_column_int(pStmt, 0);
        }
        sqlite3_finalize(pStmt);
        return SQLITE_OK;
    }
    sqlite3_finalize(pStmt);
    return iRc == SQLITE_DONE ? SQLITE_OK : iRc;
}

static int xllm__memory_sqlite_meta_upsert_int_locked(
    xllm_memory *pMemory,
    const char *sKey,
    uint32 uValue
)
{
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    if ( !pMemory || !pMemory->pSqliteDb || !sKey ) {
        return SQLITE_OK;
    }

    iRc = sqlite3_prepare_v2(
        pMemory->pSqliteDb,
        "INSERT INTO xllm_memory_meta(namespace, key, value_int, value_text) "
        "VALUES(?1, ?2, ?3, NULL) "
        "ON CONFLICT(namespace, key) DO UPDATE SET value_int = excluded.value_int, value_text = NULL;",
        -1,
        &pStmt,
        NULL
    );
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    sqlite3_bind_text(pStmt, 1, xllm__memory_namespace_key(pMemory), -1, SQLITE_STATIC);
    sqlite3_bind_text(pStmt, 2, sKey, -1, SQLITE_STATIC);
    sqlite3_bind_int(pStmt, 3, (int)uValue);
    iRc = sqlite3_step(pStmt);
    sqlite3_finalize(pStmt);
    return iRc == SQLITE_DONE ? SQLITE_OK : iRc;
}

static int xllm__memory_sqlite_meta_get_text_locked(
    xllm_memory *pMemory,
    const char *sKey,
    char **psValue
)
{
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    if ( psValue ) {
        *psValue = NULL;
    }
    if ( !pMemory || !pMemory->pSqliteDb || !sKey || !psValue ) {
        return SQLITE_OK;
    }

    iRc = sqlite3_prepare_v2(
        pMemory->pSqliteDb,
        "SELECT value_text FROM xllm_memory_meta WHERE namespace = ?1 AND key = ?2;",
        -1,
        &pStmt,
        NULL
    );
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    sqlite3_bind_text(pStmt, 1, xllm__memory_namespace_key(pMemory), -1, SQLITE_STATIC);
    sqlite3_bind_text(pStmt, 2, sKey, -1, SQLITE_STATIC);
    iRc = sqlite3_step(pStmt);
    if ( iRc == SQLITE_ROW ) {
        const unsigned char *sText = sqlite3_column_text(pStmt, 0);
        if ( sText ) {
            *psValue = xllm__dup_cstr((const char *)sText);
            if ( !*psValue ) {
                sqlite3_finalize(pStmt);
                return SQLITE_NOMEM;
            }
        }
        sqlite3_finalize(pStmt);
        return SQLITE_OK;
    }
    sqlite3_finalize(pStmt);
    return iRc == SQLITE_DONE ? SQLITE_OK : iRc;
}

static int xllm__memory_sqlite_meta_upsert_text_locked(
    xllm_memory *pMemory,
    const char *sKey,
    const char *sValue
)
{
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    if ( !pMemory || !pMemory->pSqliteDb || !sKey ) {
        return SQLITE_OK;
    }

    iRc = sqlite3_prepare_v2(
        pMemory->pSqliteDb,
        "INSERT INTO xllm_memory_meta(namespace, key, value_int, value_text) "
        "VALUES(?1, ?2, NULL, ?3) "
        "ON CONFLICT(namespace, key) DO UPDATE SET value_int = NULL, value_text = excluded.value_text;",
        -1,
        &pStmt,
        NULL
    );
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    sqlite3_bind_text(pStmt, 1, xllm__memory_namespace_key(pMemory), -1, SQLITE_STATIC);
    sqlite3_bind_text(pStmt, 2, sKey, -1, SQLITE_STATIC);
    xllm__memory_sqlite_bind_nullable_text(pStmt, 3, sValue);
    iRc = sqlite3_step(pStmt);
    sqlite3_finalize(pStmt);
    return iRc == SQLITE_DONE ? SQLITE_OK : iRc;
}

static int xllm__memory_sqlite_sync_embed_extra_text_locked(
    xllm_memory *pMemory,
    const char *sKey
)
{
    const char *sValue = xllm__memory_embedder_extra_text(&pMemory->tEmbedder, sKey);

    if ( !sValue || !sValue[0] ) {
        return SQLITE_OK;
    }
    return xllm__memory_sqlite_meta_upsert_text_locked(pMemory, sKey, sValue);
}

static int xllm__memory_sqlite_sync_embed_extra_int_locked(
    xllm_memory *pMemory,
    const char *sKey
)
{
    int64 iValue = xllm__memory_embedder_extra_int(&pMemory->tEmbedder, sKey, -1);

    if ( iValue < 0 ) {
        return SQLITE_OK;
    }
    return xllm__memory_sqlite_meta_upsert_int_locked(pMemory, sKey, (uint32)iValue);
}

static int xllm__memory_sqlite_sync_embed_profile_meta_locked(xllm_memory *pMemory)
{
    int iRc;

    if ( !pMemory || !pMemory->pSqliteDb || !pMemory->tEmbedder.tVendorExtra ) {
        return SQLITE_OK;
    }

#define XLLM__SYNC_EMBED_TEXT_META(key) \
    do { \
        iRc = xllm__memory_sqlite_sync_embed_extra_text_locked(pMemory, key); \
        if ( iRc != SQLITE_OK ) { \
            return iRc; \
        } \
    } while (0)
#define XLLM__SYNC_EMBED_INT_META(key) \
    do { \
        iRc = xllm__memory_sqlite_sync_embed_extra_int_locked(pMemory, key); \
        if ( iRc != SQLITE_OK ) { \
            return iRc; \
        } \
    } while (0)

    XLLM__SYNC_EMBED_INT_META(XLLM__MEMORY_EMBED_EXTRA_BUILTIN_KIND);
    XLLM__SYNC_EMBED_TEXT_META(XLLM__MEMORY_EMBED_EXTRA_EMBEDDER_KIND);
    XLLM__SYNC_EMBED_TEXT_META(XLLM__MEMORY_EMBED_EXTRA_PROFILE_ID);
    XLLM__SYNC_EMBED_TEXT_META(XLLM__MEMORY_EMBED_EXTRA_MODEL_ID);
    XLLM__SYNC_EMBED_TEXT_META(XLLM__MEMORY_EMBED_EXTRA_REPO_ID);
    XLLM__SYNC_EMBED_TEXT_META(XLLM__MEMORY_EMBED_EXTRA_RUNTIME_DLL_PATH);
    XLLM__SYNC_EMBED_TEXT_META(XLLM__MEMORY_EMBED_EXTRA_MODEL_PATH);
    XLLM__SYNC_EMBED_TEXT_META(XLLM__MEMORY_EMBED_EXTRA_TOKENIZER_PATH);
    XLLM__SYNC_EMBED_TEXT_META(XLLM__MEMORY_EMBED_EXTRA_QUERY_PREFIX);
    XLLM__SYNC_EMBED_TEXT_META(XLLM__MEMORY_EMBED_EXTRA_DOCUMENT_PREFIX);
    XLLM__SYNC_EMBED_TEXT_META(XLLM__MEMORY_EMBED_EXTRA_POOLING_MODE);
    XLLM__SYNC_EMBED_INT_META(XLLM__MEMORY_EMBED_EXTRA_NORMALIZE);
    XLLM__SYNC_EMBED_INT_META(XLLM__MEMORY_EMBED_EXTRA_DIMENSIONS);
    XLLM__SYNC_EMBED_INT_META(XLLM__MEMORY_EMBED_EXTRA_MAX_INPUT_TOKENS);

#undef XLLM__SYNC_EMBED_TEXT_META
#undef XLLM__SYNC_EMBED_INT_META

    return SQLITE_OK;
}

static int xllm__memory_sqlite_sync_profile_meta_locked(xllm_memory *pMemory)
{
    char *sStoredProfileId = NULL;
    uint32 uStoredSchemaVersion = 0u;
    int iRc;

    if ( !pMemory || !pMemory->pSqliteDb ) {
        return SQLITE_OK;
    }

    iRc = xllm__memory_sqlite_meta_get_int_locked(
        pMemory,
        XLLM__MEMORY_META_SCHEMA_VERSION,
        &uStoredSchemaVersion
    );
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    if ( uStoredSchemaVersion == 0u ) {
        uStoredSchemaVersion = XLLM__MEMORY_SQLITE_SCHEMA_VERSION;
        iRc = xllm__memory_sqlite_meta_upsert_int_locked(
            pMemory,
            XLLM__MEMORY_META_SCHEMA_VERSION,
            uStoredSchemaVersion
        );
        if ( iRc != SQLITE_OK ) {
            return iRc;
        }
    }
    pMemory->uSqliteSchemaVersion = uStoredSchemaVersion;

    iRc = xllm__memory_sqlite_meta_get_text_locked(
        pMemory,
        XLLM__MEMORY_META_MEMORY_PROFILE_ID,
        &sStoredProfileId
    );
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }

    if ( sStoredProfileId && sStoredProfileId[0] ) {
        if ( pMemory->sMemoryProfileId &&
             strcmp(sStoredProfileId, pMemory->sMemoryProfileId) != 0 ) {
            xrtFree(sStoredProfileId);
            return SQLITE_MISMATCH;
        }
    } else if ( pMemory->sMemoryProfileId && pMemory->sMemoryProfileId[0] ) {
        if ( sStoredProfileId ) {
            xrtFree(sStoredProfileId);
            sStoredProfileId = NULL;
        }
        iRc = xllm__memory_sqlite_meta_upsert_text_locked(
            pMemory,
            XLLM__MEMORY_META_MEMORY_PROFILE_ID,
            pMemory->sMemoryProfileId
        );
        if ( iRc != SQLITE_OK ) {
            return iRc;
        }
        sStoredProfileId = xllm__dup_cstr(pMemory->sMemoryProfileId);
        if ( !sStoredProfileId ) {
            return SQLITE_NOMEM;
        }
    }

    iRc = xllm__memory_sqlite_meta_upsert_int_locked(
        pMemory,
        XLLM__MEMORY_META_MEMORY_SCHEME,
        (uint32)pMemory->eScheme
    );
    if ( iRc != SQLITE_OK ) {
        if ( sStoredProfileId ) {
            xrtFree(sStoredProfileId);
        }
        return iRc;
    }

    iRc = xllm__memory_sqlite_sync_embed_profile_meta_locked(pMemory);
    if ( iRc != SQLITE_OK ) {
        if ( sStoredProfileId ) {
            xrtFree(sStoredProfileId);
        }
        return iRc;
    }

    xllm__free_cstr(&pMemory->sStoredMemoryProfileId);
    pMemory->sStoredMemoryProfileId = sStoredProfileId;
    return SQLITE_OK;
}

#include "xllm_memory_vindex_sqlite.c"

static int xllm__memory_sqlite_load_chunks(
    xllm_memory *pMemory,
    xllm__memory_record_entry *pRecord
)
{
    static const char *sSql =
        "SELECT rowid, chunk_id, chunk_index, text_body, embedding_blob, embedding_dim, "
        "memory_profile_id, chunk_profile_id, retrieval_profile_id, embed_profile_id, index_profile_id, profile_version, "
        "content_hash, start_byte, end_byte, previous_chunk_id, next_chunk_id "
        "FROM xllm_memory_chunk "
        "WHERE namespace = ?1 AND scope = ?2 AND record_id = ?3 "
        "ORDER BY chunk_index ASC;";
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    if ( !pMemory || !pMemory->pSqliteDb || !pRecord || !pRecord->sRecordId ) {
        return SQLITE_MISUSE;
    }

    iRc = sqlite3_prepare_v2(pMemory->pSqliteDb, sSql, -1, &pStmt, NULL);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }

    sqlite3_bind_text(pStmt, 1, xllm__memory_namespace_key(pMemory), -1, SQLITE_STATIC);
    sqlite3_bind_int(pStmt, 2, (int)pRecord->eScope);
    sqlite3_bind_text(pStmt, 3, pRecord->sRecordId, -1, SQLITE_TRANSIENT);

    while ( (iRc = sqlite3_step(pStmt)) == SQLITE_ROW ) {
        xllm__memory_chunk_entry tChunk;
        memset(&tChunk, 0, sizeof(tChunk));

        tChunk.iVectorRowId = (int64)sqlite3_column_int64(pStmt, 0);
        tChunk.sChunkId = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 1));
        tChunk.uChunkIndex = (uint32)sqlite3_column_int(pStmt, 2);
        tChunk.sText = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 3));
        tChunk.uEmbeddingDim = (uint32)sqlite3_column_int(pStmt, 5);
        tChunk.sMemoryProfileId = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 6));
        tChunk.sChunkProfileId = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 7));
        tChunk.sRetrievalProfileId = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 8));
        tChunk.sEmbedProfileId = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 9));
        tChunk.sIndexProfileId = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 10));
        tChunk.uProfileVersion = (uint32)sqlite3_column_int(pStmt, 11);
        tChunk.uContentHash = (uint64)sqlite3_column_int64(pStmt, 12);
        tChunk.iStartByte = (size_t)sqlite3_column_int64(pStmt, 13);
        tChunk.iEndByte = (size_t)sqlite3_column_int64(pStmt, 14);
        tChunk.sPreviousChunkId = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 15));
        tChunk.sNextChunkId = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 16));
        if ( tChunk.uContentHash == 0u ) {
            tChunk.uContentHash = xllm__memory_hash_text(tChunk.sText);
        }
        if ( tChunk.uEmbeddingDim > 0u ) {
            const void *pBlob = sqlite3_column_blob(pStmt, 4);
            int iBlobBytes = sqlite3_column_bytes(pStmt, 4);
            if ( pBlob && iBlobBytes == (int)(tChunk.uEmbeddingDim * (uint32)sizeof(float)) ) {
                tChunk.pfEmbedding = (float *)xrtCalloc((size_t)tChunk.uEmbeddingDim, sizeof(float));
                if ( !tChunk.pfEmbedding ) {
                    xllm__memory_chunk_free(&tChunk);
                    sqlite3_finalize(pStmt);
                    return SQLITE_NOMEM;
                }
                memcpy(tChunk.pfEmbedding, pBlob, (size_t)iBlobBytes);
            } else {
                tChunk.uEmbeddingDim = 0u;
            }
        }
        if ( !tChunk.sChunkId || !tChunk.sText ) {
            xllm__memory_chunk_free(&tChunk);
            sqlite3_finalize(pStmt);
            return SQLITE_NOMEM;
        }
        if ( xllm__memory_chunk_assign_default_profiles(pMemory, pRecord, &tChunk) != XRT_NET_OK ) {
            xllm__memory_chunk_free(&tChunk);
            sqlite3_finalize(pStmt);
            return SQLITE_NOMEM;
        }
        if ( xllm__append_buffer(
                (void **)&pRecord->pChunks,
                sizeof(tChunk),
                &pRecord->iChunkCount,
                &pRecord->iChunkCapacity,
                &tChunk
             ) != XRT_NET_OK ) {
            xllm__memory_chunk_free(&tChunk);
            sqlite3_finalize(pStmt);
            return SQLITE_NOMEM;
        }
    }
    if ( xllm__memory_record_assign_chunk_adjacency(pRecord) != XRT_NET_OK ) {
        sqlite3_finalize(pStmt);
        return SQLITE_NOMEM;
    }

    sqlite3_finalize(pStmt);
    return iRc == SQLITE_DONE ? SQLITE_OK : iRc;
}

static int xllm__memory_sqlite_load_records(xllm_memory *pMemory)
{
    static const char *sSql =
        "SELECT scope, record_id, title, source_uri, text_body, metadata_json, vendor_extra_json, "
        "memory_profile_id, retrieval_profile_id, embed_profile_id, index_profile_id, profile_version "
        "FROM xllm_memory_record "
        "WHERE namespace = ?1 "
        "ORDER BY scope ASC, record_id ASC;";
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    if ( !pMemory || !pMemory->pSqliteDb ) {
        return SQLITE_OK;
    }

    iRc = sqlite3_prepare_v2(pMemory->pSqliteDb, sSql, -1, &pStmt, NULL);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }

    sqlite3_bind_text(pStmt, 1, xllm__memory_namespace_key(pMemory), -1, SQLITE_STATIC);
    while ( (iRc = sqlite3_step(pStmt)) == SQLITE_ROW ) {
        xllm__memory_record_entry tRecord;
        int iChunkRc;

        memset(&tRecord, 0, sizeof(tRecord));
        tRecord.eScope = (xllm_memory_scope)sqlite3_column_int(pStmt, 0);
        tRecord.sRecordId = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 1));
        tRecord.sTitle = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 2));
        tRecord.sSourceUri = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 3));
        tRecord.sText = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 4));
        tRecord.tMetadata = xllm__memory_json_parse((const char *)sqlite3_column_text(pStmt, 5));
        tRecord.tVendorExtra = xllm__memory_json_parse((const char *)sqlite3_column_text(pStmt, 6));
        tRecord.sMemoryProfileId = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 7));
        tRecord.sRetrievalProfileId = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 8));
        tRecord.sEmbedProfileId = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 9));
        tRecord.sIndexProfileId = xllm__dup_cstr((const char *)sqlite3_column_text(pStmt, 10));
        tRecord.uProfileVersion = (uint32)sqlite3_column_int(pStmt, 11);
        if ( !tRecord.sRecordId || !tRecord.sText ) {
            xllm__memory_record_free(&tRecord);
            sqlite3_finalize(pStmt);
            return SQLITE_NOMEM;
        }
        if ( xllm__memory_record_assign_default_profiles(pMemory, &tRecord) != XRT_NET_OK ) {
            xllm__memory_record_free(&tRecord);
            sqlite3_finalize(pStmt);
            return SQLITE_NOMEM;
        }

        iChunkRc = xllm__memory_sqlite_load_chunks(pMemory, &tRecord);
        if ( iChunkRc != SQLITE_OK ) {
            xllm__memory_record_free(&tRecord);
            sqlite3_finalize(pStmt);
            return iChunkRc;
        }
        if ( xllm__append_buffer(
                (void **)&pMemory->pRecords,
                sizeof(tRecord),
                &pMemory->iRecordCount,
                &pMemory->iRecordCapacity,
                &tRecord
             ) != XRT_NET_OK ) {
            xllm__memory_record_free(&tRecord);
            sqlite3_finalize(pStmt);
            return SQLITE_NOMEM;
        }
    }

    sqlite3_finalize(pStmt);
    return iRc == SQLITE_DONE ? SQLITE_OK : iRc;
}

static int xllm__memory_sqlite_open(xllm_memory *pMemory)
{
    int iRc;

    if ( !pMemory || !pMemory->sSqlitePath || !pMemory->sSqlitePath[0] ) {
        return SQLITE_OK;
    }

    iRc = sqlite3_open_v2(
        pMemory->sSqlitePath,
        &pMemory->pSqliteDb,
        SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX,
        NULL
    );
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }

    iRc = xllm__memory_sqlite_configure_connection(pMemory);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_create_schema(pMemory->pSqliteDb);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_sync_profile_meta_locked(pMemory);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_load_extension(pMemory);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    if ( pMemory->bSqliteVectorExtensionLoaded ) {
        iRc = xllm__memory_sqlite_meta_get_int_locked(pMemory, "embedding_dim", &pMemory->uVectorDim);
        if ( iRc != SQLITE_OK ) {
            return iRc;
        }
        if ( pMemory->uVectorDim > 0u ) {
            iRc = xllm__memory_sqlite_ensure_vector_table_locked(pMemory, pMemory->uVectorDim);
            if ( iRc != SQLITE_OK ) {
                return iRc;
            }
        }
    }
    return xllm__memory_sqlite_load_records(pMemory);
}

static void xllm__memory_sqlite_close(xllm_memory *pMemory)
{
    if ( pMemory && pMemory->pSqliteDb ) {
        sqlite3_close(pMemory->pSqliteDb);
        pMemory->pSqliteDb = NULL;
    }
}

XLLM_API int xllm_memory_create(
    xllm_runtime *pRuntime,
    const xllm_memory_options *pOptions,
    xllm_memory **ppMemory
)
{
    xllm_memory_options tDefaultOptions;
    const xllm_memory_options *pUseOptions = pOptions;
    xllm_memory *pMemory;
    xllm__memory_scheme_selection tScheme;
    xllm_memory_embedder tResolvedEmbedder;
    bool bHasResolvedEmbedder = false;
    bool bResolvedEmbedderOwned = false;

    if ( !pRuntime || !ppMemory ) {
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    memset(&tScheme, 0, sizeof(tScheme));
    xllm_memory_embedder_init(&tResolvedEmbedder);
    if ( xllm__memory_resolve_scheme(pUseOptions, &tScheme, NULL) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( tScheme.bAutoCreateBuiltinEmbedder ) {
#if XLLM__MEMORY_HAS_SCHEME_ONNX_E5
        xllm_memory_builtin_embedder_options tBuiltinOptions;

        xllm_memory_builtin_embedder_options_init(&tBuiltinOptions);
        tBuiltinOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX;
        if ( xllm__memory_make_builtin_embedder(&tBuiltinOptions, &tResolvedEmbedder, NULL) != XRT_NET_OK ) {
            xllm_memory_embedder_reset(&tResolvedEmbedder);
            return XRT_NET_ERROR;
        }
        bHasResolvedEmbedder = true;
        bResolvedEmbedderOwned = true;
#else
        return XRT_NET_ERROR;
#endif
    } else if ( xllm__memory_embedder_is_configured(&pUseOptions->tEmbedder) ) {
        tResolvedEmbedder = pUseOptions->tEmbedder;
        bHasResolvedEmbedder = true;
    }

    pMemory = (xllm_memory *)xrtCalloc(1u, sizeof(*pMemory));
    if ( !pMemory ) {
        if ( bResolvedEmbedderOwned ) {
            xllm_memory_embedder_reset(&tResolvedEmbedder);
        }
        return XRT_NET_ERROR;
    }

    pMemory->pRuntime = pRuntime;
    pMemory->eScheme = tScheme.eScheme;
    pMemory->uDefaultChunkChars = pUseOptions->uDefaultChunkChars > 0u ? pUseOptions->uDefaultChunkChars : 800u;
    pMemory->uDefaultChunkOverlapChars = pUseOptions->uDefaultChunkOverlapChars;
    pMemory->uDefaultMaxHits = pUseOptions->uDefaultMaxHits > 0u ? pUseOptions->uDefaultMaxHits : 5u;
    pMemory->sNamespace = xllm__dup_cstr(pUseOptions->sNamespace);
    pMemory->sMemoryProfileId = xllm__dup_cstr(tScheme.sResolvedProfileId);
    pMemory->sSqlitePath = xllm__dup_cstr(pUseOptions->sSqlitePath);
    pMemory->sSqliteVectorExtensionPath = xllm__dup_cstr(pUseOptions->sSqliteVectorExtensionPath);
    pMemory->sVectorTableName = xllm__memory_make_vector_table_name(pUseOptions->sNamespace);
    pMemory->bLoadSqliteVectorExtension = pUseOptions->bLoadSqliteVectorExtension;
    pMemory->bSqliteWalRequested = !pUseOptions->bDisableSqliteWal;
    pMemory->bEnableHybridSearch = pUseOptions->bEnableHybridSearch;
    pMemory->uSqliteBusyTimeoutMs = pUseOptions->uSqliteBusyTimeoutMs > 0u ? pUseOptions->uSqliteBusyTimeoutMs : 5000u;
    pMemory->fLexicalWeight = pUseOptions->tLexicalWeight.bSet ? pUseOptions->tLexicalWeight.fValue : 1.0;
    pMemory->fVectorWeight = pUseOptions->tVectorWeight.bSet ? pUseOptions->tVectorWeight.fValue : 1.0;
    if ( pMemory->eScheme == XLLM_MEMORY_SCHEME_BUILTIN_SPARSE ) {
        pMemory->bLoadSqliteVectorExtension = false;
        pMemory->bEnableHybridSearch = false;
        pMemory->fVectorWeight = 0.0;
    }
    if ( bHasResolvedEmbedder ) {
        pMemory->tEmbedder = tResolvedEmbedder;
    }
    if ( bHasResolvedEmbedder && !bResolvedEmbedderOwned &&
         tResolvedEmbedder.pfnCloneCtx && tResolvedEmbedder.pCtx ) {
        void *pClonedCtx = NULL;
        if ( tResolvedEmbedder.pfnCloneCtx(
                tResolvedEmbedder.pCtx,
                &pClonedCtx,
                NULL
             ) != XRT_NET_OK ) {
            xllm_memory_destroy(pMemory);
            return XRT_NET_ERROR;
        }
        pMemory->tEmbedder.pCtx = pClonedCtx;
        pMemory->bOwnsEmbedderCtx = true;
    } else {
        pMemory->bOwnsEmbedderCtx = bResolvedEmbedderOwned;
    }
    if ( bHasResolvedEmbedder && !bResolvedEmbedderOwned ) {
        xllm__xvalue_addref(pMemory->tEmbedder.tVendorExtra);
    }
    pMemory->tVendorExtra = pUseOptions->tVendorExtra;
    xllm__xvalue_addref(pMemory->tVendorExtra);
    pMemory->pMutex = xrtMutexCreate();
    if ( !pMemory->pMutex ) {
        xllm_memory_destroy(pMemory);
        return XRT_NET_ERROR;
    }
    if ( xllm__memory_sqlite_open(pMemory) != SQLITE_OK ) {
        xllm_memory_destroy(pMemory);
        return XRT_NET_ERROR;
    }

    *ppMemory = pMemory;
    return XRT_NET_OK;
}

XLLM_API void xllm_memory_destroy(xllm_memory *pMemory)
{
    size_t i;

    if ( !pMemory ) {
        return;
    }

    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        xllm__memory_record_free(&pMemory->pRecords[i]);
    }
    if ( pMemory->pRecords ) {
        xrtFree(pMemory->pRecords);
    }
    xllm__free_cstr(&pMemory->sNamespace);
    xllm__free_cstr(&pMemory->sMemoryProfileId);
    xllm__free_cstr(&pMemory->sSqlitePath);
    xllm__free_cstr(&pMemory->sSqliteVectorExtensionPath);
    xllm__free_cstr(&pMemory->sStoredMemoryProfileId);
    xllm__free_cstr(&pMemory->sVectorTableName);
    xllm__memory_embedder_release(&pMemory->tEmbedder, pMemory->bOwnsEmbedderCtx);
    xllm__xvalue_release(&pMemory->tVendorExtra);
    xllm__memory_sqlite_close(pMemory);
    if ( pMemory->pMutex ) {
        xrtMutexDestroy(pMemory->pMutex);
    }
    xrtFree(pMemory);
}

static int xllm__memory_sqlite_delete_record_locked(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sRecordId
)
{
    static const char *sSql =
        "DELETE FROM xllm_memory_record "
        "WHERE namespace = ?1 AND scope = ?2 AND record_id = ?3;";
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    if ( !pMemory || !pMemory->pSqliteDb || !sRecordId ) {
        return SQLITE_OK;
    }
    iRc = xllm__memory_sqlite_delete_vectors_for_record_locked(pMemory, eScope, sRecordId);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    iRc = xllm__memory_sqlite_delete_sparse_postings_for_record_locked(pMemory, eScope, sRecordId);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }

    iRc = sqlite3_prepare_v2(pMemory->pSqliteDb, sSql, -1, &pStmt, NULL);
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

static int xllm__memory_remove_record_at_locked(
    xllm_memory *pMemory,
    size_t iIndex,
    xllm_error *pError,
    const char *sErrorMessage
)
{
    size_t j;

    if ( !pMemory || iIndex >= pMemory->iRecordCount ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "invalid memory record removal index");
        return XRT_NET_ERROR;
    }

    if ( xllm__memory_sqlite_delete_record_locked(
            pMemory,
            pMemory->pRecords[iIndex].eScope,
            pMemory->pRecords[iIndex].sRecordId
         ) != SQLITE_OK ) {
        xllm__error_set(
            pError,
            XLLM_ERROR_INTERNAL,
            (sErrorMessage && sErrorMessage[0]) ? sErrorMessage : "failed to remove memory record from sqlite"
        );
        return XRT_NET_ERROR;
    }
    xllm__memory_record_free(&pMemory->pRecords[iIndex]);
    for ( j = iIndex + 1u; j < pMemory->iRecordCount; ++j ) {
        pMemory->pRecords[j - 1u] = pMemory->pRecords[j];
    }
    --pMemory->iRecordCount;
    return XRT_NET_OK;
}

static int64 xllm__memory_metadata_updated_at(xvalue tMetadata)
{
    if ( !tMetadata || xvoType(tMetadata) != XVO_DT_TABLE ) {
        return 0;
    }

    return xvoTableGetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_UPDATED_AT_UNIX, 0u);
}

static int xllm__memory_nullable_cstr_cmp(const char *sLeft, const char *sRight)
{
    if ( sLeft && sRight ) {
        return strcmp(sLeft, sRight);
    }
    if ( sLeft ) {
        return 1;
    }
    if ( sRight ) {
        return -1;
    }
    return 0;
}

static int xllm__memory_record_info_compare_updated_desc(
    const void *pLeftVoid,
    const void *pRightVoid
)
{
    const xllm_memory_record_info *pLeft = (const xllm_memory_record_info *)pLeftVoid;
    const xllm_memory_record_info *pRight = (const xllm_memory_record_info *)pRightVoid;
    int64 iLeftUpdatedAt = xllm__memory_metadata_updated_at(pLeft ? pLeft->tMetadata : 0);
    int64 iRightUpdatedAt = xllm__memory_metadata_updated_at(pRight ? pRight->tMetadata : 0);
    int iCmp;

    if ( iLeftUpdatedAt > iRightUpdatedAt ) {
        return -1;
    }
    if ( iLeftUpdatedAt < iRightUpdatedAt ) {
        return 1;
    }

    iCmp = xllm__memory_nullable_cstr_cmp(pLeft ? pLeft->sSourceUri : NULL, pRight ? pRight->sSourceUri : NULL);
    if ( iCmp != 0 ) {
        return iCmp;
    }
    return xllm__memory_nullable_cstr_cmp(pLeft ? pLeft->sRecordId : NULL, pRight ? pRight->sRecordId : NULL);
}

static int xllm__memory_chunk_info_compare_updated_desc(
    const void *pLeftVoid,
    const void *pRightVoid
)
{
    const xllm_memory_chunk_info *pLeft = (const xllm_memory_chunk_info *)pLeftVoid;
    const xllm_memory_chunk_info *pRight = (const xllm_memory_chunk_info *)pRightVoid;
    int64 iLeftUpdatedAt = xllm__memory_metadata_updated_at(pLeft ? pLeft->tMetadata : 0);
    int64 iRightUpdatedAt = xllm__memory_metadata_updated_at(pRight ? pRight->tMetadata : 0);
    int iCmp;

    if ( iLeftUpdatedAt > iRightUpdatedAt ) {
        return -1;
    }
    if ( iLeftUpdatedAt < iRightUpdatedAt ) {
        return 1;
    }

    iCmp = xllm__memory_nullable_cstr_cmp(pLeft ? pLeft->sSourceUri : NULL, pRight ? pRight->sSourceUri : NULL);
    if ( iCmp != 0 ) {
        return iCmp;
    }
    if ( pLeft && pRight ) {
        if ( pLeft->uChunkIndex < pRight->uChunkIndex ) {
            return -1;
        }
        if ( pLeft->uChunkIndex > pRight->uChunkIndex ) {
            return 1;
        }
    }
    iCmp = xllm__memory_nullable_cstr_cmp(pLeft ? pLeft->sRecordId : NULL, pRight ? pRight->sRecordId : NULL);
    if ( iCmp != 0 ) {
        return iCmp;
    }
    return xllm__memory_nullable_cstr_cmp(pLeft ? pLeft->sChunkId : NULL, pRight ? pRight->sChunkId : NULL);
}

static int xllm__memory_record_info_trim_sorted_result(
    xllm_memory_record_list_result *pResult,
    size_t iOffset,
    size_t iLimit,
    xllm_error *pError
)
{
    xllm_memory_record_info *pTrimmed = NULL;
    size_t iSelectedCount = 0u;
    size_t i;

    if ( !pResult ) {
        return XRT_NET_ERROR;
    }
    if ( iOffset >= pResult->iRecordCount ) {
        xllm__memory_record_info_array_reset(pResult->pRecords, pResult->iRecordCount);
        pResult->pRecords = NULL;
        pResult->iRecordCount = 0u;
        return XRT_NET_OK;
    }

    iSelectedCount = pResult->iRecordCount - iOffset;
    if ( iSelectedCount > iLimit ) {
        iSelectedCount = iLimit;
    }
    pTrimmed = (xllm_memory_record_info *)xrtCalloc(iSelectedCount > 0u ? iSelectedCount : 1u, sizeof(*pTrimmed));
    if ( iSelectedCount > 0u && !pTrimmed ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate trimmed memory record list");
        return XRT_NET_ERROR;
    }
    for ( i = 0u; i < iSelectedCount; ++i ) {
        pTrimmed[i] = pResult->pRecords[iOffset + i];
        memset(&pResult->pRecords[iOffset + i], 0, sizeof(pResult->pRecords[iOffset + i]));
    }
    xllm__memory_record_info_array_reset(pResult->pRecords, pResult->iRecordCount);
    pResult->pRecords = pTrimmed;
    pResult->iRecordCount = iSelectedCount;
    return XRT_NET_OK;
}

static void xllm__memory_chunk_info_array_reset(
    xllm_memory_chunk_info *pInfos,
    size_t iInfoCount
)
{
    size_t i;

    if ( !pInfos ) {
        return;
    }
    for ( i = 0u; i < iInfoCount; ++i ) {
        xllm__memory_chunk_info_reset(&pInfos[i]);
    }
    xrtFree(pInfos);
}

static int xllm__memory_chunk_info_trim_sorted_result(
    xllm_memory_chunk_list_result *pResult,
    size_t iOffset,
    size_t iLimit,
    xllm_error *pError
)
{
    xllm_memory_chunk_info *pTrimmed = NULL;
    size_t iSelectedCount = 0u;
    size_t i;

    if ( !pResult ) {
        return XRT_NET_ERROR;
    }
    if ( iOffset >= pResult->iChunkCount ) {
        xllm__memory_chunk_info_array_reset(pResult->pChunks, pResult->iChunkCount);
        pResult->pChunks = NULL;
        pResult->iChunkCount = 0u;
        return XRT_NET_OK;
    }

    iSelectedCount = pResult->iChunkCount - iOffset;
    if ( iSelectedCount > iLimit ) {
        iSelectedCount = iLimit;
    }
    pTrimmed = (xllm_memory_chunk_info *)xrtCalloc(iSelectedCount > 0u ? iSelectedCount : 1u, sizeof(*pTrimmed));
    if ( iSelectedCount > 0u && !pTrimmed ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate trimmed memory chunk list");
        return XRT_NET_ERROR;
    }
    for ( i = 0u; i < iSelectedCount; ++i ) {
        pTrimmed[i] = pResult->pChunks[iOffset + i];
        memset(&pResult->pChunks[iOffset + i], 0, sizeof(pResult->pChunks[iOffset + i]));
    }
    xllm__memory_chunk_info_array_reset(pResult->pChunks, pResult->iChunkCount);
    pResult->pChunks = pTrimmed;
    pResult->iChunkCount = iSelectedCount;
    return XRT_NET_OK;
}

typedef struct {
    size_t iIndex;
    int64 iPriority;
    int64 iUpdatedAtUnix;
    size_t iTextLength;
    size_t iChunkCount;
    const char *sSourceUri;
    const char *sRecordId;
} xllm__memory_record_sort_entry;

static int xllm__memory_record_sort_entry_compare_updated_desc(
    const void *pLeftVoid,
    const void *pRightVoid
)
{
    const xllm__memory_record_sort_entry *pLeft = (const xllm__memory_record_sort_entry *)pLeftVoid;
    const xllm__memory_record_sort_entry *pRight = (const xllm__memory_record_sort_entry *)pRightVoid;
    int iCmp;

    if ( pLeft && pRight ) {
        if ( pLeft->iUpdatedAtUnix > pRight->iUpdatedAtUnix ) {
            return -1;
        }
        if ( pLeft->iUpdatedAtUnix < pRight->iUpdatedAtUnix ) {
            return 1;
        }
    }

    iCmp = xllm__memory_nullable_cstr_cmp(pLeft ? pLeft->sSourceUri : NULL, pRight ? pRight->sSourceUri : NULL);
    if ( iCmp != 0 ) {
        return iCmp;
    }
    return xllm__memory_nullable_cstr_cmp(pLeft ? pLeft->sRecordId : NULL, pRight ? pRight->sRecordId : NULL);
}

static int xllm__memory_record_sort_entry_compare_priority_updated_desc(
    const void *pLeftVoid,
    const void *pRightVoid
)
{
    const xllm__memory_record_sort_entry *pLeft = (const xllm__memory_record_sort_entry *)pLeftVoid;
    const xllm__memory_record_sort_entry *pRight = (const xllm__memory_record_sort_entry *)pRightVoid;

    if ( pLeft && pRight ) {
        if ( pLeft->iPriority > pRight->iPriority ) {
            return -1;
        }
        if ( pLeft->iPriority < pRight->iPriority ) {
            return 1;
        }
    }
    return xllm__memory_record_sort_entry_compare_updated_desc(pLeftVoid, pRightVoid);
}

static int xllm__memory_record_sort_entry_compare_index_desc(
    const void *pLeftVoid,
    const void *pRightVoid
)
{
    const xllm__memory_record_sort_entry *pLeft = (const xllm__memory_record_sort_entry *)pLeftVoid;
    const xllm__memory_record_sort_entry *pRight = (const xllm__memory_record_sort_entry *)pRightVoid;

    if ( pLeft && pRight ) {
        if ( pLeft->iIndex > pRight->iIndex ) {
            return -1;
        }
        if ( pLeft->iIndex < pRight->iIndex ) {
            return 1;
        }
    }
    return 0;
}

XLLM_API int xllm_memory_remove(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sRecordId,
    xllm_error *pError
)
{
    size_t i;

    if ( !pMemory || !sRecordId || !sRecordId[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory remove requires record_id");
        return XRT_NET_ERROR;
    }

    xrtMutexLock(pMemory->pMutex);
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        bool bScopeMatches = (eScope == XLLM_MEMORY_SCOPE_ANY) || (pMemory->pRecords[i].eScope == eScope);
        if ( bScopeMatches && strcmp(pMemory->pRecords[i].sRecordId, sRecordId) == 0 ) {
            if ( xllm__memory_remove_record_at_locked(
                    pMemory,
                    i,
                    pError,
                    "failed to remove memory record from sqlite"
                 ) != XRT_NET_OK ) {
                xrtMutexUnlock(pMemory->pMutex);
                return XRT_NET_ERROR;
            }
            xrtMutexUnlock(pMemory->pMutex);
            return XRT_NET_OK;
        }
    }
    xrtMutexUnlock(pMemory->pMutex);

    xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory record not found");
    return XRT_NET_ERROR;
}

XLLM_API int xllm_memory_remove_by_source_uri(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sSourceUri,
    uint32 *puRemovedCount,
    xllm_error *pError
)
{
    uint32 uRemovedCount = 0u;
    size_t i = 0u;

    if ( puRemovedCount ) {
        *puRemovedCount = 0u;
    }
    if ( !pMemory || !sSourceUri || !sSourceUri[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory remove by source_uri requires source_uri");
        return XRT_NET_ERROR;
    }

    xrtMutexLock(pMemory->pMutex);
    while ( i < pMemory->iRecordCount ) {
        bool bScopeMatches = (eScope == XLLM_MEMORY_SCOPE_ANY) || (pMemory->pRecords[i].eScope == eScope);
        bool bSourceUriMatches = pMemory->pRecords[i].sSourceUri && strcmp(pMemory->pRecords[i].sSourceUri, sSourceUri) == 0;

        if ( bScopeMatches && bSourceUriMatches ) {
            if ( xllm__memory_remove_record_at_locked(
                    pMemory,
                    i,
                    pError,
                    "failed to remove memory record from sqlite"
                 ) != XRT_NET_OK ) {
                xrtMutexUnlock(pMemory->pMutex);
                return XRT_NET_ERROR;
            }
            ++uRemovedCount;
            continue;
        }
        ++i;
    }
    xrtMutexUnlock(pMemory->pMutex);
    if ( puRemovedCount ) {
        *puRemovedCount = uRemovedCount;
    }
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_remove_by_metadata(
    xllm_memory *pMemory,
    const xllm_memory_remove_by_metadata_options *pOptions,
    uint32 *puRemovedCount,
    xllm_error *pError
)
{
    xllm_memory_remove_by_metadata_options tDefaultOptions;
    const xllm_memory_remove_by_metadata_options *pUseOptions = pOptions;
    uint32 uRemovedCount = 0u;
    size_t i = 0u;

    if ( puRemovedCount ) {
        *puRemovedCount = 0u;
    }
    if ( !pMemory ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory remove by metadata requires memory handle");
        return XRT_NET_ERROR;
    }
    if ( !pUseOptions ) {
        xllm_memory_remove_by_metadata_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( !pUseOptions->sMetadataKey || !pUseOptions->sMetadataKey[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory remove by metadata requires metadata_key");
        return XRT_NET_ERROR;
    }
    if ( pUseOptions->eScope != XLLM_MEMORY_SCOPE_ANY &&
         pUseOptions->eScope != XLLM_MEMORY_SCOPE_MEMORY &&
         pUseOptions->eScope != XLLM_MEMORY_SCOPE_KNOWLEDGE ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory remove by metadata scope must be any, memory, or knowledge");
        return XRT_NET_ERROR;
    }

    xrtMutexLock(pMemory->pMutex);
    while ( i < pMemory->iRecordCount ) {
        bool bScopeMatches =
            (pUseOptions->eScope == XLLM_MEMORY_SCOPE_ANY) ||
            (pMemory->pRecords[i].eScope == pUseOptions->eScope);
        if ( bScopeMatches &&
             xllm__memory_metadata_matches(
                 pMemory->pRecords[i].tMetadata,
                 pUseOptions->sMetadataKey,
                 pUseOptions->sMetadataValue
             ) ) {
            if ( xllm__memory_remove_record_at_locked(
                    pMemory,
                    i,
                    pError,
                    "failed to remove memory record from sqlite"
                 ) != XRT_NET_OK ) {
                xrtMutexUnlock(pMemory->pMutex);
                return XRT_NET_ERROR;
            }
            ++uRemovedCount;
            continue;
        }
        ++i;
    }
    xrtMutexUnlock(pMemory->pMutex);

    if ( puRemovedCount ) {
        *puRemovedCount = uRemovedCount;
    }
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_remove_by_conversation(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sConversationId,
    const char *sTurnId,
    uint32 *puRemovedCount,
    xllm_error *pError
)
{
    uint32 uRemovedCount = 0u;
    size_t i = 0u;

    if ( puRemovedCount ) {
        *puRemovedCount = 0u;
    }
    if ( !pMemory || !sConversationId || !sConversationId[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory remove by conversation requires conversation_id");
        return XRT_NET_ERROR;
    }

    xrtMutexLock(pMemory->pMutex);
    while ( i < pMemory->iRecordCount ) {
        if ( xllm__memory_record_matches_filters(
                &pMemory->pRecords[i],
                eScope,
                sConversationId,
                sTurnId,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
             ) ) {
            if ( xllm__memory_remove_record_at_locked(
                    pMemory,
                    i,
                    pError,
                    "failed to remove conversation memory record from sqlite"
                 ) != XRT_NET_OK ) {
                xrtMutexUnlock(pMemory->pMutex);
                return XRT_NET_ERROR;
            }
            ++uRemovedCount;
            continue;
        }
        ++i;
    }
    xrtMutexUnlock(pMemory->pMutex);
    if ( puRemovedCount ) {
        *puRemovedCount = uRemovedCount;
    }
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_trim_conversation(
    xllm_memory *pMemory,
    const xllm_memory_trim_conversation_options *pOptions,
    uint32 *puRemovedCount,
    xllm_error *pError
)
{
    xllm_memory_trim_conversation_options tDefaultOptions;
    const xllm_memory_trim_conversation_options *pUseOptions = pOptions;
    xllm__memory_record_sort_entry *pMatches = NULL;
    int (*fnKeepOrder)(const void *, const void *) = xllm__memory_record_sort_entry_compare_updated_desc;
    size_t iMatchCount = 0u;
    size_t iRemoveCount = 0u;
    size_t iWrite = 0u;
    size_t iKeepCount = 0u;
    uint64 uKeptChars = 0u;
    uint64 uKeptChunks = 0u;
    uint32 uRemovedCount = 0u;
    size_t i;

    if ( puRemovedCount ) {
        *puRemovedCount = 0u;
    }
    if ( !pMemory ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory trim conversation requires memory handle");
        return XRT_NET_ERROR;
    }
    if ( !pUseOptions ) {
        xllm_memory_trim_conversation_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( !pUseOptions->sConversationId || !pUseOptions->sConversationId[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory trim conversation requires conversation_id");
        return XRT_NET_ERROR;
    }
    if ( pUseOptions->tPreferPriorityDesc.bSet && pUseOptions->tPreferPriorityDesc.bValue ) {
        fnKeepOrder = xllm__memory_record_sort_entry_compare_priority_updated_desc;
    }

    xrtMutexLock(pMemory->pMutex);
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        if ( xllm__memory_record_matches_filters(
                &pMemory->pRecords[i],
                pUseOptions->eScope,
                pUseOptions->sConversationId,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
             ) ) {
            ++iMatchCount;
        }
    }

    if ( iMatchCount == 0u ) {
        xrtMutexUnlock(pMemory->pMutex);
        return XRT_NET_OK;
    }

    pMatches = (xllm__memory_record_sort_entry *)xrtCalloc(iMatchCount, sizeof(*pMatches));
    if ( !pMatches ) {
        xrtMutexUnlock(pMemory->pMutex);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate conversation trim candidate list");
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        if ( xllm__memory_record_matches_filters(
                &pMemory->pRecords[i],
                pUseOptions->eScope,
                pUseOptions->sConversationId,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
             ) ) {
            pMatches[iWrite].iIndex = i;
            pMatches[iWrite].iPriority = xllm__memory_record_priority(&pMemory->pRecords[i]);
            pMatches[iWrite].iUpdatedAtUnix = xllm__memory_metadata_updated_at(pMemory->pRecords[i].tMetadata);
            pMatches[iWrite].iTextLength = pMemory->pRecords[i].sText ? strlen(pMemory->pRecords[i].sText) : 0u;
            pMatches[iWrite].iChunkCount = pMemory->pRecords[i].iChunkCount;
            pMatches[iWrite].sSourceUri = pMemory->pRecords[i].sSourceUri;
            pMatches[iWrite].sRecordId = pMemory->pRecords[i].sRecordId;
            ++iWrite;
        }
    }

    qsort(pMatches, iMatchCount, sizeof(*pMatches), fnKeepOrder);
    for ( i = 0u; i < iMatchCount; ++i ) {
        bool bKeep = true;
        uint64 uCandidateChars = (uint64)pMatches[i].iTextLength;
        uint64 uCandidateChunks = (uint64)pMatches[i].iChunkCount;

        if ( iKeepCount >= (size_t)pUseOptions->uKeepLatestRecords ) {
            bKeep = false;
        }
        if ( bKeep &&
             pUseOptions->uMaxTotalChars > 0u &&
             (uKeptChars + uCandidateChars) > pUseOptions->uMaxTotalChars ) {
            bKeep = false;
        }
        if ( bKeep &&
             pUseOptions->uMaxTotalChunks > 0u &&
             (uKeptChunks + uCandidateChunks) > (uint64)pUseOptions->uMaxTotalChunks ) {
            bKeep = false;
        }

        if ( bKeep ) {
            ++iKeepCount;
            uKeptChars += uCandidateChars;
            uKeptChunks += uCandidateChunks;
        } else {
            pMatches[iRemoveCount++] = pMatches[i];
        }
    }

    if ( iRemoveCount > 1u ) {
        qsort(
            pMatches,
            iRemoveCount,
            sizeof(*pMatches),
            xllm__memory_record_sort_entry_compare_index_desc
        );
    }

    for ( i = 0u; i < iRemoveCount; ++i ) {
        if ( xllm__memory_remove_record_at_locked(
                pMemory,
                pMatches[i].iIndex,
                pError,
                "failed to trim conversation memory record from sqlite"
             ) != XRT_NET_OK ) {
            xrtMutexUnlock(pMemory->pMutex);
            xrtFree(pMatches);
            return XRT_NET_ERROR;
        }
        ++uRemovedCount;
    }
    xrtMutexUnlock(pMemory->pMutex);
    xrtFree(pMatches);

    if ( puRemovedCount ) {
        *puRemovedCount = uRemovedCount;
    }
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_remove_expired(
    xllm_memory *pMemory,
    const xllm_memory_remove_expired_options *pOptions,
    uint32 *puRemovedCount,
    xllm_error *pError
)
{
    xllm_memory_remove_expired_options tDefaultOptions;
    const xllm_memory_remove_expired_options *pUseOptions = pOptions;
    const char *sMetadataKey;
    const char *sSourceUriPrefix;
    int64 iNowUnix;
    uint32 uRemovedCount = 0u;
    size_t i = 0u;

    if ( puRemovedCount ) {
        *puRemovedCount = 0u;
    }
    if ( !pMemory ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory remove_expired requires memory handle");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_remove_expired_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    sMetadataKey = (pUseOptions->sMetadataKey && pUseOptions->sMetadataKey[0])
        ? pUseOptions->sMetadataKey
        : XLLM__MEMORY_METADATA_KEY_EXPIRES_AT_UNIX;
    sSourceUriPrefix = (pUseOptions->sSourceUriPrefix && pUseOptions->sSourceUriPrefix[0])
        ? pUseOptions->sSourceUriPrefix
        : NULL;
    iNowUnix = (pUseOptions->iNowUnix > 0) ? pUseOptions->iNowUnix : xrtToUnixTime(xrtNow());

    xrtMutexLock(pMemory->pMutex);
    while ( i < pMemory->iRecordCount ) {
        const xllm__memory_record_entry *pRecord = &pMemory->pRecords[i];
        bool bScopeMatches = (pUseOptions->eScope == XLLM_MEMORY_SCOPE_ANY) || (pRecord->eScope == pUseOptions->eScope);
        bool bSourceUriMatches = true;
        int64 iExpiresAtUnix = 0;

        if ( sSourceUriPrefix ) {
            size_t iPrefixLength = strlen(sSourceUriPrefix);
            bSourceUriMatches =
                pRecord->sSourceUri &&
                strncmp(pRecord->sSourceUri, sSourceUriPrefix, iPrefixLength) == 0;
        }
        if ( pRecord->tMetadata && xvoType(pRecord->tMetadata) == XVO_DT_TABLE ) {
            iExpiresAtUnix = xvoTableGetInt(pRecord->tMetadata, sMetadataKey, 0u);
        }

        if ( bScopeMatches && bSourceUriMatches && iExpiresAtUnix > 0 && iExpiresAtUnix <= iNowUnix ) {
            if ( xllm__memory_remove_record_at_locked(
                    pMemory,
                    i,
                    pError,
                    "failed to remove expired memory record from sqlite"
                 ) != XRT_NET_OK ) {
                xrtMutexUnlock(pMemory->pMutex);
                return XRT_NET_ERROR;
            }
            ++uRemovedCount;
            continue;
        }
        ++i;
    }
    xrtMutexUnlock(pMemory->pMutex);

    if ( puRemovedCount ) {
        *puRemovedCount = uRemovedCount;
    }
    return XRT_NET_OK;
}

XLLM_API void xllm_memory_record_list_result_reset(xllm_memory_record_list_result *pResult)
{
    size_t i;

    if ( !pResult ) {
        return;
    }

    for ( i = 0u; i < pResult->iRecordCount; ++i ) {
        xllm__memory_record_info_reset(&pResult->pRecords[i]);
    }
    if ( pResult->pRecords ) {
        xrtFree(pResult->pRecords);
    }
    xllm__xvalue_release(&pResult->tVendorExtra);
    memset(pResult, 0, sizeof(*pResult));
}

XLLM_API void xllm_memory_chunk_list_result_reset(xllm_memory_chunk_list_result *pResult)
{
    size_t i;

    if ( !pResult ) {
        return;
    }

    for ( i = 0u; i < pResult->iChunkCount; ++i ) {
        xllm__memory_chunk_info_reset(&pResult->pChunks[i]);
    }
    if ( pResult->pChunks ) {
        xrtFree(pResult->pChunks);
    }
    xllm__xvalue_release(&pResult->tVendorExtra);
    memset(pResult, 0, sizeof(*pResult));
}

#include "xllm_memory_pipeline_sqlite.c"

XLLM_API int xllm_memory_list_records(
    xllm_memory *pMemory,
    const xllm_memory_list_options *pOptions,
    xllm_memory_record_list_result *pResult,
    xllm_error *pError
)
{
    xllm_memory_list_options tDefaultOptions;
    const xllm_memory_list_options *pUseOptions = pOptions;
    size_t iOffset;
    size_t iLimit;
    size_t iMatched = 0u;
    size_t iCapacity = 0u;
    int64 iNowUnix = 0;
    bool bSortByUpdatedAtDesc = false;
    size_t i;

    if ( !pMemory || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory list records requires a result object");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_list_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( pUseOptions->sMetadataValue && pUseOptions->sMetadataValue[0] &&
         (!pUseOptions->sMetadataKey || !pUseOptions->sMetadataKey[0]) ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory list metadata_value requires metadata_key");
        return XRT_NET_ERROR;
    }

    xllm_memory_record_list_result_reset(pResult);
    pResult->tVendorExtra = pUseOptions->tVendorExtra;
    xllm__xvalue_addref(pResult->tVendorExtra);
    iOffset = (size_t)pUseOptions->uOffset;
    iLimit = pUseOptions->uMaxItems > 0u ? (size_t)pUseOptions->uMaxItems : 32u;
    bSortByUpdatedAtDesc = pUseOptions->tSortByUpdatedAtDesc.bSet && pUseOptions->tSortByUpdatedAtDesc.bValue;
    if ( pUseOptions->bSkipExpired ) {
        iNowUnix = pUseOptions->iNowUnix > 0 ? pUseOptions->iNowUnix : xrtToUnixTime(xrtNow());
    }

    xrtMutexLock(pMemory->pMutex);
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        xllm_memory_record_info tInfo;

        if ( !xllm__memory_record_matches_filters(
                &pMemory->pRecords[i],
                pUseOptions->eScope,
                pUseOptions->sConversationId,
                pUseOptions->sTurnId,
                pUseOptions->sRecordId,
                pUseOptions->sSourceUri,
                pUseOptions->sRecordIdContains,
                pUseOptions->sTitleContains,
                pUseOptions->sSourceUriContains,
                pUseOptions->sTextContains,
                pUseOptions->sMetadataKey,
                pUseOptions->sMetadataValue
             ) ) {
            continue;
        }
        if ( pUseOptions->bSkipExpired && xllm__memory_record_is_expired(&pMemory->pRecords[i], iNowUnix) ) {
            continue;
        }
        if ( !bSortByUpdatedAtDesc ) {
            if ( iMatched++ < iOffset ) {
                continue;
            }
            if ( pResult->iRecordCount >= iLimit ) {
                break;
            }
        }
        if ( xllm__memory_record_info_clone(&tInfo, &pMemory->pRecords[i]) != XRT_NET_OK ) {
            xrtMutexUnlock(pMemory->pMutex);
            xllm_memory_record_list_result_reset(pResult);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to clone memory record info");
            return XRT_NET_ERROR;
        }
        if ( xllm__append_buffer(
                (void **)&pResult->pRecords,
                sizeof(tInfo),
                &pResult->iRecordCount,
                &iCapacity,
                &tInfo
             ) != XRT_NET_OK ) {
            xllm__memory_record_info_reset(&tInfo);
            xrtMutexUnlock(pMemory->pMutex);
            xllm_memory_record_list_result_reset(pResult);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to append memory record info");
            return XRT_NET_ERROR;
        }
        if ( bSortByUpdatedAtDesc ) {
            ++iMatched;
        }
    }
    xrtMutexUnlock(pMemory->pMutex);
    if ( bSortByUpdatedAtDesc && pResult->iRecordCount > 1u ) {
        qsort(
            pResult->pRecords,
            pResult->iRecordCount,
            sizeof(*pResult->pRecords),
            xllm__memory_record_info_compare_updated_desc
        );
    }
    if ( bSortByUpdatedAtDesc ) {
        if ( xllm__memory_record_info_trim_sorted_result(pResult, iOffset, iLimit, pError) != XRT_NET_OK ) {
            xllm_memory_record_list_result_reset(pResult);
            return XRT_NET_ERROR;
        }
    }
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_list_chunks(
    xllm_memory *pMemory,
    const xllm_memory_list_options *pOptions,
    xllm_memory_chunk_list_result *pResult,
    xllm_error *pError
)
{
    xllm_memory_list_options tDefaultOptions;
    const xllm_memory_list_options *pUseOptions = pOptions;
    size_t iOffset;
    size_t iLimit;
    size_t iMatched = 0u;
    size_t iCapacity = 0u;
    int64 iNowUnix = 0;
    bool bSortByUpdatedAtDesc = false;
    size_t i;

    if ( !pMemory || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory list chunks requires a result object");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_list_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( pUseOptions->sMetadataValue && pUseOptions->sMetadataValue[0] &&
         (!pUseOptions->sMetadataKey || !pUseOptions->sMetadataKey[0]) ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory list metadata_value requires metadata_key");
        return XRT_NET_ERROR;
    }

    xllm_memory_chunk_list_result_reset(pResult);
    pResult->tVendorExtra = pUseOptions->tVendorExtra;
    xllm__xvalue_addref(pResult->tVendorExtra);
    iOffset = (size_t)pUseOptions->uOffset;
    iLimit = pUseOptions->uMaxItems > 0u ? (size_t)pUseOptions->uMaxItems : 32u;
    bSortByUpdatedAtDesc = pUseOptions->tSortByUpdatedAtDesc.bSet && pUseOptions->tSortByUpdatedAtDesc.bValue;
    if ( pUseOptions->bSkipExpired ) {
        iNowUnix = pUseOptions->iNowUnix > 0 ? pUseOptions->iNowUnix : xrtToUnixTime(xrtNow());
    }

    xrtMutexLock(pMemory->pMutex);
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        size_t j;

        if ( pUseOptions->bSkipExpired && xllm__memory_record_is_expired(&pMemory->pRecords[i], iNowUnix) ) {
            continue;
        }

        for ( j = 0u; j < pMemory->pRecords[i].iChunkCount; ++j ) {
            xllm_memory_chunk_info tInfo;

            if ( !xllm__memory_chunk_matches_filters(&pMemory->pRecords[i], &pMemory->pRecords[i].pChunks[j], pUseOptions) ) {
                continue;
            }
            if ( !bSortByUpdatedAtDesc ) {
                if ( iMatched++ < iOffset ) {
                    continue;
                }
                if ( pResult->iChunkCount >= iLimit ) {
                    xrtMutexUnlock(pMemory->pMutex);
                    return XRT_NET_OK;
                }
            }
            if ( xllm__memory_chunk_info_clone(
                    &tInfo,
                    &pMemory->pRecords[i],
                    &pMemory->pRecords[i].pChunks[j],
                    pUseOptions->uMaxCharsPerText
                 ) != XRT_NET_OK ) {
                xrtMutexUnlock(pMemory->pMutex);
                xllm_memory_chunk_list_result_reset(pResult);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to clone memory chunk info");
                return XRT_NET_ERROR;
            }
            if ( xllm__append_buffer(
                    (void **)&pResult->pChunks,
                    sizeof(tInfo),
                    &pResult->iChunkCount,
                    &iCapacity,
                    &tInfo
                 ) != XRT_NET_OK ) {
                xllm__memory_chunk_info_reset(&tInfo);
                xrtMutexUnlock(pMemory->pMutex);
                xllm_memory_chunk_list_result_reset(pResult);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to append memory chunk info");
                return XRT_NET_ERROR;
            }
            if ( bSortByUpdatedAtDesc ) {
                ++iMatched;
            }
        }
    }
    xrtMutexUnlock(pMemory->pMutex);
    if ( bSortByUpdatedAtDesc && pResult->iChunkCount > 1u ) {
        qsort(
            pResult->pChunks,
            pResult->iChunkCount,
            sizeof(*pResult->pChunks),
            xllm__memory_chunk_info_compare_updated_desc
        );
    }
    if ( bSortByUpdatedAtDesc ) {
        if ( xllm__memory_chunk_info_trim_sorted_result(pResult, iOffset, iLimit, pError) != XRT_NET_OK ) {
            xllm_memory_chunk_list_result_reset(pResult);
            return XRT_NET_ERROR;
        }
    }
    return XRT_NET_OK;
}

static int xllm__memory_sqlite_persist_record_locked(
    xllm_memory *pMemory,
    const xllm__memory_record_entry *pRecord
)
{
    static const char *sInsertRecordSql =
        "INSERT INTO xllm_memory_record("
        "namespace, scope, record_id, title, source_uri, text_body, metadata_json, vendor_extra_json, "
        "memory_profile_id, retrieval_profile_id, embed_profile_id, index_profile_id, profile_version, updated_at"
        ") VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, CURRENT_TIMESTAMP);";
    static const char *sInsertChunkSql =
        "INSERT INTO xllm_memory_chunk("
        "namespace, scope, record_id, chunk_index, chunk_id, text_body, embedding_blob, embedding_dim, "
        "memory_profile_id, chunk_profile_id, retrieval_profile_id, embed_profile_id, index_profile_id, profile_version, "
        "content_hash, start_byte, end_byte, previous_chunk_id, next_chunk_id"
        ") VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19);";
    sqlite3_stmt *pRecordStmt = NULL;
    sqlite3_stmt *pChunkStmt = NULL;
    char *sMetadataJson = NULL;
    char *sVendorExtraJson = NULL;
    int iRc;
    size_t i;

    if ( !pMemory || !pRecord || !pMemory->pSqliteDb ) {
        return SQLITE_OK;
    }

    sMetadataJson = xllm__memory_json_stringify(pRecord->tMetadata);
    sVendorExtraJson = xllm__memory_json_stringify(pRecord->tVendorExtra);

    iRc = xllm__memory_sqlite_exec(pMemory->pSqliteDb, "BEGIN IMMEDIATE TRANSACTION;");
    if ( iRc != SQLITE_OK ) {
        goto cleanup;
    }

    iRc = xllm__memory_sqlite_delete_record_locked(pMemory, pRecord->eScope, pRecord->sRecordId);
    if ( iRc != SQLITE_OK ) {
        goto rollback;
    }

    iRc = sqlite3_prepare_v2(pMemory->pSqliteDb, sInsertRecordSql, -1, &pRecordStmt, NULL);
    if ( iRc != SQLITE_OK ) {
        goto rollback;
    }
    sqlite3_bind_text(pRecordStmt, 1, xllm__memory_namespace_key(pMemory), -1, SQLITE_STATIC);
    sqlite3_bind_int(pRecordStmt, 2, (int)pRecord->eScope);
    sqlite3_bind_text(pRecordStmt, 3, pRecord->sRecordId, -1, SQLITE_TRANSIENT);
    xllm__memory_sqlite_bind_nullable_text(pRecordStmt, 4, pRecord->sTitle);
    xllm__memory_sqlite_bind_nullable_text(pRecordStmt, 5, pRecord->sSourceUri);
    sqlite3_bind_text(pRecordStmt, 6, pRecord->sText, -1, SQLITE_TRANSIENT);
    xllm__memory_sqlite_bind_nullable_text(pRecordStmt, 7, sMetadataJson);
    xllm__memory_sqlite_bind_nullable_text(pRecordStmt, 8, sVendorExtraJson);
    xllm__memory_sqlite_bind_nullable_text(pRecordStmt, 9, pRecord->sMemoryProfileId);
    xllm__memory_sqlite_bind_nullable_text(pRecordStmt, 10, pRecord->sRetrievalProfileId);
    xllm__memory_sqlite_bind_nullable_text(pRecordStmt, 11, pRecord->sEmbedProfileId);
    xllm__memory_sqlite_bind_nullable_text(pRecordStmt, 12, pRecord->sIndexProfileId);
    sqlite3_bind_int(pRecordStmt, 13, (int)pRecord->uProfileVersion);
    iRc = sqlite3_step(pRecordStmt);
    if ( iRc != SQLITE_DONE ) {
        goto rollback;
    }
    sqlite3_finalize(pRecordStmt);
    pRecordStmt = NULL;

    iRc = sqlite3_prepare_v2(pMemory->pSqliteDb, sInsertChunkSql, -1, &pChunkStmt, NULL);
    if ( iRc != SQLITE_OK ) {
        goto rollback;
    }
    for ( i = 0u; i < pRecord->iChunkCount; ++i ) {
        sqlite3_reset(pChunkStmt);
        sqlite3_clear_bindings(pChunkStmt);
        sqlite3_bind_text(pChunkStmt, 1, xllm__memory_namespace_key(pMemory), -1, SQLITE_STATIC);
        sqlite3_bind_int(pChunkStmt, 2, (int)pRecord->eScope);
        sqlite3_bind_text(pChunkStmt, 3, pRecord->sRecordId, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(pChunkStmt, 4, (int)pRecord->pChunks[i].uChunkIndex);
        sqlite3_bind_text(pChunkStmt, 5, pRecord->pChunks[i].sChunkId, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(pChunkStmt, 6, pRecord->pChunks[i].sText, -1, SQLITE_TRANSIENT);
        xllm__memory_sqlite_bind_nullable_embedding(
            pChunkStmt,
            7,
            8,
            pRecord->pChunks[i].pfEmbedding,
            pRecord->pChunks[i].uEmbeddingDim
        );
        xllm__memory_sqlite_bind_nullable_text(pChunkStmt, 9, pRecord->pChunks[i].sMemoryProfileId);
        xllm__memory_sqlite_bind_nullable_text(pChunkStmt, 10, pRecord->pChunks[i].sChunkProfileId);
        xllm__memory_sqlite_bind_nullable_text(pChunkStmt, 11, pRecord->pChunks[i].sRetrievalProfileId);
        xllm__memory_sqlite_bind_nullable_text(pChunkStmt, 12, pRecord->pChunks[i].sEmbedProfileId);
        xllm__memory_sqlite_bind_nullable_text(pChunkStmt, 13, pRecord->pChunks[i].sIndexProfileId);
        sqlite3_bind_int(pChunkStmt, 14, (int)pRecord->pChunks[i].uProfileVersion);
        sqlite3_bind_int64(pChunkStmt, 15, (sqlite3_int64)pRecord->pChunks[i].uContentHash);
        sqlite3_bind_int64(pChunkStmt, 16, (sqlite3_int64)pRecord->pChunks[i].iStartByte);
        sqlite3_bind_int64(pChunkStmt, 17, (sqlite3_int64)pRecord->pChunks[i].iEndByte);
        xllm__memory_sqlite_bind_nullable_text(pChunkStmt, 18, pRecord->pChunks[i].sPreviousChunkId);
        xllm__memory_sqlite_bind_nullable_text(pChunkStmt, 19, pRecord->pChunks[i].sNextChunkId);
        iRc = sqlite3_step(pChunkStmt);
        if ( iRc != SQLITE_DONE ) {
            goto rollback;
        }
        pRecord->pChunks[i].iVectorRowId = (int64)sqlite3_last_insert_rowid(pMemory->pSqliteDb);
        iRc = xllm__memory_sqlite_upsert_chunk_vector_locked(pMemory, &pRecord->pChunks[i]);
        if ( iRc != SQLITE_OK ) {
            goto rollback;
        }
        iRc = xllm__memory_sqlite_insert_sparse_postings_locked(pMemory, pRecord, &pRecord->pChunks[i]);
        if ( iRc != SQLITE_OK ) {
            goto rollback;
        }
    }

    sqlite3_finalize(pChunkStmt);
    pChunkStmt = NULL;
    iRc = xllm__memory_sqlite_exec(pMemory->pSqliteDb, "COMMIT;");
    if ( iRc != SQLITE_OK ) {
        goto rollback;
    }
    goto cleanup;

rollback:
    xllm__memory_sqlite_exec(pMemory->pSqliteDb, "ROLLBACK;");

cleanup:
    if ( pRecordStmt ) {
        sqlite3_finalize(pRecordStmt);
    }
    if ( pChunkStmt ) {
        sqlite3_finalize(pChunkStmt);
    }
    if ( sMetadataJson ) {
        xrtFree(sMetadataJson);
    }
    if ( sVendorExtraJson ) {
        xrtFree(sVendorExtraJson);
    }
    return iRc == SQLITE_DONE ? SQLITE_OK : iRc;
}

XLLM_API int xllm_memory_ingest_text(
    xllm_memory *pMemory,
    const xllm_memory_ingest_options *pOptions,
    xllm_error *pError
)
{
    xllm_memory_ingest_options tDefaultOptions;
    const xllm_memory_ingest_options *pUseOptions = pOptions;
    xllm__memory_record_entry tRecord;
    size_t iExisting = (size_t)-1;
    int iPersistRc = SQLITE_OK;
    size_t i;

    if ( !pMemory ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory handle is required");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_ingest_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }
    if ( !pUseOptions->sRecordId || !pUseOptions->sRecordId[0] || !pUseOptions->sText || !pUseOptions->sText[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "record_id and text are required for memory ingest");
        return XRT_NET_ERROR;
    }
    if ( pUseOptions->eScope != XLLM_MEMORY_SCOPE_MEMORY && pUseOptions->eScope != XLLM_MEMORY_SCOPE_KNOWLEDGE ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory ingest scope must be memory or knowledge");
        return XRT_NET_ERROR;
    }

    memset(&tRecord, 0, sizeof(tRecord));
    tRecord.eScope = pUseOptions->eScope;
    tRecord.sRecordId = xllm__dup_cstr(pUseOptions->sRecordId);
    tRecord.sTitle = xllm__dup_cstr(pUseOptions->sTitle);
    tRecord.sSourceUri = xllm__dup_cstr(pUseOptions->sSourceUri);
    tRecord.sText = xllm__dup_cstr(pUseOptions->sText);
    tRecord.tMetadata = pUseOptions->tMetadata;
    tRecord.tVendorExtra = pUseOptions->tVendorExtra;
    xllm__xvalue_addref(tRecord.tMetadata);
    xllm__xvalue_addref(tRecord.tVendorExtra);
    if ( !tRecord.sRecordId || !tRecord.sText ) {
        xllm__memory_record_free(&tRecord);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate memory record");
        return XRT_NET_ERROR;
    }
    if ( xllm__memory_record_assign_default_profiles(pMemory, &tRecord) != XRT_NET_OK ) {
        xllm__memory_record_free(&tRecord);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate memory record profiles");
        return XRT_NET_ERROR;
    }
    if ( xllm__memory_record_rechunk(pMemory, &tRecord, pUseOptions->uChunkChars, pUseOptions->uChunkOverlapChars) != XRT_NET_OK ) {
        xllm__memory_record_free(&tRecord);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to chunk memory record");
        return XRT_NET_ERROR;
    }
    if ( xllm__memory_record_embed_chunks(pMemory, &tRecord, pError) != XRT_NET_OK ) {
        xllm__memory_record_free(&tRecord);
        return XRT_NET_ERROR;
    }

    xrtMutexLock(pMemory->pMutex);
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        if ( pMemory->pRecords[i].eScope == pUseOptions->eScope &&
             strcmp(pMemory->pRecords[i].sRecordId, pUseOptions->sRecordId) == 0 ) {
            iExisting = i;
            break;
        }
    }

    if ( iExisting != (size_t)-1 ) {
        if ( !pUseOptions->bReplaceExisting ) {
            xrtMutexUnlock(pMemory->pMutex);
            xllm__memory_record_free(&tRecord);
            xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory record already exists");
            return XRT_NET_ERROR;
        }
        iPersistRc = xllm__memory_sqlite_persist_record_locked(pMemory, &tRecord);
        if ( iPersistRc != SQLITE_OK ) {
            char sBuffer[512];
            const char *sSqliteMessage = (pMemory->pSqliteDb != NULL) ? sqlite3_errmsg(pMemory->pSqliteDb) : NULL;
            xrtMutexUnlock(pMemory->pMutex);
            xllm__memory_record_free(&tRecord);
            snprintf(
                sBuffer,
                sizeof(sBuffer),
                "failed to persist memory record to sqlite rc=%d%s%s",
                iPersistRc,
                (sSqliteMessage && sSqliteMessage[0]) ? " msg=" : "",
                (sSqliteMessage && sSqliteMessage[0]) ? sSqliteMessage : ""
            );
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, sBuffer);
            return XRT_NET_ERROR;
        }
        xllm__memory_record_free(&pMemory->pRecords[iExisting]);
        pMemory->pRecords[iExisting] = tRecord;
    } else {
        iPersistRc = xllm__memory_sqlite_persist_record_locked(pMemory, &tRecord);
        if ( iPersistRc != SQLITE_OK ) {
            char sBuffer[512];
            const char *sSqliteMessage = (pMemory->pSqliteDb != NULL) ? sqlite3_errmsg(pMemory->pSqliteDb) : NULL;
            xrtMutexUnlock(pMemory->pMutex);
            xllm__memory_record_free(&tRecord);
            snprintf(
                sBuffer,
                sizeof(sBuffer),
                "failed to persist memory record to sqlite rc=%d%s%s",
                iPersistRc,
                (sSqliteMessage && sSqliteMessage[0]) ? " msg=" : "",
                (sSqliteMessage && sSqliteMessage[0]) ? sSqliteMessage : ""
            );
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, sBuffer);
            return XRT_NET_ERROR;
        }
        if ( xllm__append_buffer(
                    (void **)&pMemory->pRecords,
                    sizeof(tRecord),
                    &pMemory->iRecordCount,
                    &pMemory->iRecordCapacity,
                    &tRecord
                ) != XRT_NET_OK ) {
            xrtMutexUnlock(pMemory->pMutex);
            xllm__memory_record_free(&tRecord);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to append memory record");
            return XRT_NET_ERROR;
        }
    }
    xrtMutexUnlock(pMemory->pMutex);
    return XRT_NET_OK;
}

XLLM_API xllm_memory_scheme xllm_memory_get_scheme(const xllm_memory *pMemory)
{
    if ( !pMemory ) {
        return XLLM_MEMORY_SCHEME_AUTO;
    }
    return pMemory->eScheme;
}

XLLM_API const char *xllm_memory_get_profile_id(const xllm_memory *pMemory)
{
    if ( !pMemory ) {
        return NULL;
    }
    return pMemory->sMemoryProfileId;
}

XLLM_API size_t xllm_memory_record_count(const xllm_memory *pMemory, xllm_memory_scope eScope)
{
    size_t i;
    size_t iCount = 0u;

    if ( !pMemory ) {
        return 0u;
    }

    xrtMutexLock(pMemory->pMutex);
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        if ( eScope == XLLM_MEMORY_SCOPE_ANY || pMemory->pRecords[i].eScope == eScope ) {
            ++iCount;
        }
    }
    xrtMutexUnlock(pMemory->pMutex);
    return iCount;
}

XLLM_API size_t xllm_memory_chunk_count(const xllm_memory *pMemory, xllm_memory_scope eScope)
{
    size_t i;
    size_t iCount = 0u;

    if ( !pMemory ) {
        return 0u;
    }

    xrtMutexLock(pMemory->pMutex);
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        if ( eScope == XLLM_MEMORY_SCOPE_ANY || pMemory->pRecords[i].eScope == eScope ) {
            iCount += pMemory->pRecords[i].iChunkCount;
        }
    }
    xrtMutexUnlock(pMemory->pMutex);
    return iCount;
}

XLLM_API void xllm_memory_diagnostics_init(xllm_memory_diagnostics *pDiagnostics)
{
    if ( !pDiagnostics ) {
        return;
    }

    memset(pDiagnostics, 0, sizeof(*pDiagnostics));
    pDiagnostics->eScheme = XLLM_MEMORY_SCHEME_AUTO;
    pDiagnostics->eBuiltinEmbedderKind = XLLM_MEMORY_BUILTIN_EMBEDDER_NONE;
}

XLLM_API void xllm_memory_workspace_status_options_init(xllm_memory_workspace_status_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
}

XLLM_API void xllm_memory_workspace_status_init(xllm_memory_workspace_status *pStatus)
{
    if ( !pStatus ) {
        return;
    }

    memset(pStatus, 0, sizeof(*pStatus));
    pStatus->eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
}

XLLM_API void xllm_memory_health_check_options_init(xllm_memory_health_check_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eScope = XLLM_MEMORY_SCOPE_ANY;
    pOptions->bCheckSqlite = true;
    pOptions->bCheckSparsePostings = true;
}

XLLM_API void xllm_memory_health_check_init(xllm_memory_health_check *pHealth)
{
    if ( !pHealth ) {
        return;
    }

    memset(pHealth, 0, sizeof(*pHealth));
    pHealth->eScope = XLLM_MEMORY_SCOPE_ANY;
    pHealth->bProfileOk = true;
    pHealth->bSparsePostingsOk = true;
}

XLLM_API int xllm_memory_get_diagnostics(
    const xllm_memory *pMemory,
    xllm_memory_diagnostics *pDiagnostics,
    xllm_error *pError
)
{
    size_t i;

    if ( !pMemory || !pDiagnostics ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory diagnostics requires memory and output");
        return XRT_NET_ERROR;
    }

    xllm_memory_diagnostics_init(pDiagnostics);

    xrtMutexLock(pMemory->pMutex);
    pDiagnostics->eScheme = pMemory->eScheme;
    pDiagnostics->sMemoryProfileId = pMemory->sMemoryProfileId;
    pDiagnostics->sEmbedProfileId = xllm__memory_embedder_extra_text(&pMemory->tEmbedder, XLLM__MEMORY_EMBED_EXTRA_PROFILE_ID);
    pDiagnostics->sNamespace = pMemory->sNamespace;
    pDiagnostics->sSqlitePath = pMemory->sSqlitePath;
    pDiagnostics->sSqliteVectorExtensionPath = pMemory->sSqliteVectorExtensionPath;
    pDiagnostics->eBuiltinEmbedderKind = (xllm_memory_builtin_embedder_kind)xllm__memory_embedder_extra_int(
        &pMemory->tEmbedder,
        XLLM__MEMORY_EMBED_EXTRA_BUILTIN_KIND,
        (int64)XLLM_MEMORY_BUILTIN_EMBEDDER_NONE
    );
    pDiagnostics->sEmbedderKind = xllm__memory_embedder_extra_text(&pMemory->tEmbedder, XLLM__MEMORY_EMBED_EXTRA_EMBEDDER_KIND);
    pDiagnostics->sEmbedModelId = xllm__memory_embedder_extra_text(&pMemory->tEmbedder, XLLM__MEMORY_EMBED_EXTRA_MODEL_ID);
    pDiagnostics->sEmbedRepoId = xllm__memory_embedder_extra_text(&pMemory->tEmbedder, XLLM__MEMORY_EMBED_EXTRA_REPO_ID);
    pDiagnostics->sEmbedRuntimeDllPath = xllm__memory_embedder_extra_text(&pMemory->tEmbedder, XLLM__MEMORY_EMBED_EXTRA_RUNTIME_DLL_PATH);
    pDiagnostics->sEmbedModelPath = xllm__memory_embedder_extra_text(&pMemory->tEmbedder, XLLM__MEMORY_EMBED_EXTRA_MODEL_PATH);
    pDiagnostics->sEmbedTokenizerPath = xllm__memory_embedder_extra_text(&pMemory->tEmbedder, XLLM__MEMORY_EMBED_EXTRA_TOKENIZER_PATH);
    pDiagnostics->sEmbedQueryPrefix = xllm__memory_embedder_extra_text(&pMemory->tEmbedder, XLLM__MEMORY_EMBED_EXTRA_QUERY_PREFIX);
    pDiagnostics->sEmbedDocumentPrefix = xllm__memory_embedder_extra_text(&pMemory->tEmbedder, XLLM__MEMORY_EMBED_EXTRA_DOCUMENT_PREFIX);
    pDiagnostics->sEmbedPoolingMode = xllm__memory_embedder_extra_text(&pMemory->tEmbedder, XLLM__MEMORY_EMBED_EXTRA_POOLING_MODE);
    pDiagnostics->bSqliteOpen = pMemory->pSqliteDb != NULL;
    pDiagnostics->bSqliteWalEnabled = pMemory->bSqliteWalEnabled;
    pDiagnostics->bSqliteWalRequested = pMemory->bSqliteWalRequested;
    pDiagnostics->bSqliteVectorExtensionRequested = pMemory->bLoadSqliteVectorExtension;
    pDiagnostics->bSqliteVectorExtensionLoaded = pMemory->bSqliteVectorExtensionLoaded;
    pDiagnostics->bVectorTableReady = pMemory->bVectorTableReady;
    pDiagnostics->bHybridSearchEnabled = pMemory->bEnableHybridSearch;
    pDiagnostics->bEmbedderConfigured = xllm__memory_embedder_is_configured(&pMemory->tEmbedder);
    pDiagnostics->bOwnsEmbedderCtx = pMemory->bOwnsEmbedderCtx;
    pDiagnostics->bEmbedNormalize =
        xllm__memory_embedder_extra_int(&pMemory->tEmbedder, XLLM__MEMORY_EMBED_EXTRA_NORMALIZE, 0) != 0;
    pDiagnostics->bStorageProfileMatch =
        !pMemory->sStoredMemoryProfileId ||
        !pMemory->sMemoryProfileId ||
        strcmp(pMemory->sStoredMemoryProfileId, pMemory->sMemoryProfileId) == 0;
    pDiagnostics->uVectorDim = pMemory->uVectorDim;
    pDiagnostics->uEmbedDimensions = (uint32)xllm__memory_embedder_extra_int(
        &pMemory->tEmbedder,
        XLLM__MEMORY_EMBED_EXTRA_DIMENSIONS,
        0
    );
    pDiagnostics->uEmbedMaxInputTokens = (uint32)xllm__memory_embedder_extra_int(
        &pMemory->tEmbedder,
        XLLM__MEMORY_EMBED_EXTRA_MAX_INPUT_TOKENS,
        0
    );
    pDiagnostics->uSqliteSchemaVersion = pMemory->uSqliteSchemaVersion;
    pDiagnostics->uSqliteBusyTimeoutMs = pMemory->uSqliteBusyTimeoutMs;
    pDiagnostics->uDefaultChunkChars = pMemory->uDefaultChunkChars;
    pDiagnostics->uDefaultChunkOverlapChars = pMemory->uDefaultChunkOverlapChars;
    pDiagnostics->uDefaultMaxHits = pMemory->uDefaultMaxHits;
    pDiagnostics->fLexicalWeight = pMemory->fLexicalWeight;
    pDiagnostics->fVectorWeight = pMemory->fVectorWeight;
    pDiagnostics->sStoredMemoryProfileId = pMemory->sStoredMemoryProfileId;

    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        const xllm__memory_record_entry *pRecord = &pMemory->pRecords[i];
        pDiagnostics->iRecordCount += 1u;
        pDiagnostics->iChunkCount += pRecord->iChunkCount;
        if ( pRecord->eScope == XLLM_MEMORY_SCOPE_MEMORY ) {
            pDiagnostics->iMemoryRecordCount += 1u;
            pDiagnostics->iMemoryChunkCount += pRecord->iChunkCount;
        } else if ( pRecord->eScope == XLLM_MEMORY_SCOPE_KNOWLEDGE ) {
            pDiagnostics->iKnowledgeRecordCount += 1u;
            pDiagnostics->iKnowledgeChunkCount += pRecord->iChunkCount;
        }
    }
    xrtMutexUnlock(pMemory->pMutex);
    return XRT_NET_OK;
}

static bool xllm__memory_status_scope_matches(
    xllm_memory_scope eFilter,
    xllm_memory_scope eRecordScope
)
{
    return eFilter == XLLM_MEMORY_SCOPE_ANY || eFilter == eRecordScope;
}

static bool xllm__memory_status_source_prefix_matches(
    const char *sPrefix,
    const char *sSourceUri
)
{
    size_t iPrefixLen;

    if ( !sPrefix || !sPrefix[0] ) {
        return true;
    }
    if ( !sSourceUri ) {
        return false;
    }

    iPrefixLen = strlen(sPrefix);
    return xllm__memory_ascii_strnicmp(sPrefix, sSourceUri, iPrefixLen) == 0;
}

static bool xllm__memory_status_is_non_normal_sensitivity(const char *sSensitivity)
{
    if ( !sSensitivity || !sSensitivity[0] ) {
        return false;
    }
    return xllm__memory_ascii_stricmp(sSensitivity, "normal") != 0;
}

static bool xllm__memory_status_is_untrusted_source(const char *sSourceTrust)
{
    if ( !sSourceTrust || !sSourceTrust[0] ) {
        return false;
    }
    return xllm__memory_ascii_stricmp(sSourceTrust, "local_file") != 0;
}

static int xllm__memory_sqlite_table_exists(sqlite3 *pDb, const char *sTableName, bool *pbExists)
{
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    if ( !pDb || !sTableName || !pbExists ) {
        return SQLITE_MISUSE;
    }
    *pbExists = false;
    iRc = sqlite3_prepare_v2(
        pDb,
        "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?1;",
        -1,
        &pStmt,
        NULL
    );
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    sqlite3_bind_text(pStmt, 1, sTableName, -1, SQLITE_STATIC);
    iRc = sqlite3_step(pStmt);
    if ( iRc == SQLITE_ROW ) {
        *pbExists = sqlite3_column_int64(pStmt, 0) > 0;
        iRc = SQLITE_OK;
    }
    sqlite3_finalize(pStmt);
    return iRc;
}

static int xllm__memory_sqlite_count_scoped(
    sqlite3 *pDb,
    const char *sSql,
    const char *sNamespace,
    xllm_memory_scope eScope,
    size_t *piCount
)
{
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    if ( !pDb || !sSql || !sNamespace || !piCount ) {
        return SQLITE_MISUSE;
    }
    *piCount = 0u;
    iRc = sqlite3_prepare_v2(pDb, sSql, -1, &pStmt, NULL);
    if ( iRc != SQLITE_OK ) {
        return iRc;
    }
    sqlite3_bind_text(pStmt, 1, sNamespace, -1, SQLITE_STATIC);
    sqlite3_bind_int(pStmt, 2, (int)eScope);
    iRc = sqlite3_step(pStmt);
    if ( iRc == SQLITE_ROW ) {
        sqlite3_int64 iValue = sqlite3_column_int64(pStmt, 0);
        *piCount = iValue > 0 ? (size_t)iValue : 0u;
        iRc = SQLITE_OK;
    }
    sqlite3_finalize(pStmt);
    return iRc;
}

static int xllm__memory_sqlite_health_schema_ok(sqlite3 *pDb, bool *pbSchemaOk)
{
    static const char *apsTables[] = {
        "xllm_memory_record",
        "xllm_memory_meta",
        "xllm_memory_chunk",
        "xllm_memory_sparse_posting"
    };
    size_t i;

    if ( !pDb || !pbSchemaOk ) {
        return SQLITE_MISUSE;
    }
    *pbSchemaOk = true;
    for ( i = 0u; i < sizeof(apsTables) / sizeof(apsTables[0]); ++i ) {
        bool bExists = false;
        int iRc = xllm__memory_sqlite_table_exists(pDb, apsTables[i], &bExists);
        if ( iRc != SQLITE_OK ) {
            return iRc;
        }
        if ( !bExists ) {
            *pbSchemaOk = false;
        }
    }
    return SQLITE_OK;
}

XLLM_API int xllm_memory_check_health(
    const xllm_memory *pMemory,
    const xllm_memory_health_check_options *pOptions,
    xllm_memory_health_check *pHealth,
    xllm_error *pError
)
{
    static const char *sRecordCountSql =
        "SELECT COUNT(*) FROM xllm_memory_record "
        "WHERE namespace = ?1 AND (?2 = 0 OR scope = ?2);";
    static const char *sChunkCountSql =
        "SELECT COUNT(*) FROM xllm_memory_chunk "
        "WHERE namespace = ?1 AND (?2 = 0 OR scope = ?2);";
    static const char *sOrphanChunkCountSql =
        "SELECT COUNT(*) FROM xllm_memory_chunk c "
        "LEFT JOIN xllm_memory_record r "
        "ON r.namespace = c.namespace AND r.scope = c.scope AND r.record_id = c.record_id "
        "WHERE c.namespace = ?1 AND (?2 = 0 OR c.scope = ?2) AND r.record_id IS NULL;";
    static const char *sSparseChunkCountSql =
        "SELECT COUNT(*) FROM ("
        "SELECT DISTINCT namespace, scope, record_id, chunk_index FROM xllm_memory_sparse_posting "
        "WHERE namespace = ?1 AND (?2 = 0 OR scope = ?2)"
        ");";
    static const char *sOrphanPostingCountSql =
        "SELECT COUNT(*) FROM xllm_memory_sparse_posting p "
        "LEFT JOIN xllm_memory_chunk c "
        "ON c.namespace = p.namespace AND c.scope = p.scope AND c.record_id = p.record_id AND c.chunk_index = p.chunk_index "
        "WHERE p.namespace = ?1 AND (?2 = 0 OR p.scope = ?2) AND c.record_id IS NULL;";
    xllm_memory_health_check_options tDefaultOptions;
    const xllm_memory_health_check_options *pUseOptions = pOptions;
    const char *sNamespace;
    size_t i;
    int iRc;

    if ( !pMemory || !pHealth ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory health check requires memory and output");
        return XRT_NET_ERROR;
    }
    if ( !pUseOptions ) {
        xllm_memory_health_check_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    xllm_memory_health_check_init(pHealth);
    pHealth->eScope = pUseOptions->eScope;
    pHealth->tVendorExtra = pUseOptions->tVendorExtra;

    xrtMutexLock(pMemory->pMutex);
    pHealth->bSqliteOpen = pMemory->pSqliteDb != NULL;
    pHealth->uSqliteSchemaVersion = pMemory->uSqliteSchemaVersion;
    pHealth->sMemoryProfileId = pMemory->sMemoryProfileId;
    pHealth->sStoredMemoryProfileId = pMemory->sStoredMemoryProfileId;
    pHealth->bProfileOk =
        !pMemory->sStoredMemoryProfileId ||
        !pMemory->sMemoryProfileId ||
        strcmp(pMemory->sStoredMemoryProfileId, pMemory->sMemoryProfileId) == 0;

    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        const xllm__memory_record_entry *pRecord = &pMemory->pRecords[i];
        if ( pUseOptions->eScope != XLLM_MEMORY_SCOPE_ANY && pRecord->eScope != pUseOptions->eScope ) {
            continue;
        }
        ++pHealth->iRecordCount;
        pHealth->iChunkCount += pRecord->iChunkCount;
    }

    if ( pUseOptions->bCheckSqlite && pMemory->pSqliteDb ) {
        sNamespace = xllm__memory_namespace_key(pMemory);
        iRc = xllm__memory_sqlite_health_schema_ok(pMemory->pSqliteDb, &pHealth->bSchemaOk);
        if ( iRc != SQLITE_OK ) {
            xrtMutexUnlock(pMemory->pMutex);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, sqlite3_errmsg(pMemory->pSqliteDb));
            return XRT_NET_ERROR;
        }
        iRc = xllm__memory_sqlite_count_scoped(
            pMemory->pSqliteDb,
            sRecordCountSql,
            sNamespace,
            pUseOptions->eScope,
            &pHealth->iSqliteRecordCount
        );
        if ( iRc != SQLITE_OK ) {
            xrtMutexUnlock(pMemory->pMutex);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, sqlite3_errmsg(pMemory->pSqliteDb));
            return XRT_NET_ERROR;
        }
        iRc = xllm__memory_sqlite_count_scoped(
            pMemory->pSqliteDb,
            sChunkCountSql,
            sNamespace,
            pUseOptions->eScope,
            &pHealth->iSqliteChunkCount
        );
        if ( iRc != SQLITE_OK ) {
            xrtMutexUnlock(pMemory->pMutex);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, sqlite3_errmsg(pMemory->pSqliteDb));
            return XRT_NET_ERROR;
        }
        iRc = xllm__memory_sqlite_count_scoped(
            pMemory->pSqliteDb,
            sOrphanChunkCountSql,
            sNamespace,
            pUseOptions->eScope,
            &pHealth->iOrphanChunkCount
        );
        if ( iRc != SQLITE_OK ) {
            xrtMutexUnlock(pMemory->pMutex);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, sqlite3_errmsg(pMemory->pSqliteDb));
            return XRT_NET_ERROR;
        }
        if ( pUseOptions->bCheckSparsePostings ) {
            iRc = xllm__memory_sqlite_count_scoped(
                pMemory->pSqliteDb,
                sSparseChunkCountSql,
                sNamespace,
                pUseOptions->eScope,
                &pHealth->iSparseChunkCount
            );
            if ( iRc != SQLITE_OK ) {
                xrtMutexUnlock(pMemory->pMutex);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, sqlite3_errmsg(pMemory->pSqliteDb));
                return XRT_NET_ERROR;
            }
            iRc = xllm__memory_sqlite_count_scoped(
                pMemory->pSqliteDb,
                sOrphanPostingCountSql,
                sNamespace,
                pUseOptions->eScope,
                &pHealth->iSparseOrphanPostingCount
            );
            if ( iRc != SQLITE_OK ) {
                xrtMutexUnlock(pMemory->pMutex);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, sqlite3_errmsg(pMemory->pSqliteDb));
                return XRT_NET_ERROR;
            }
            if ( pMemory->eScheme == XLLM_MEMORY_SCHEME_BUILTIN_SPARSE &&
                 pHealth->iSqliteChunkCount > pHealth->iSparseChunkCount ) {
                pHealth->iSparseMissingChunkCount = pHealth->iSqliteChunkCount - pHealth->iSparseChunkCount;
            }
            pHealth->bSparsePostingsOk =
                pHealth->iSparseOrphanPostingCount == 0u &&
                pHealth->iSparseMissingChunkCount == 0u;
        }
    } else {
        pHealth->bSchemaOk = !pUseOptions->bCheckSqlite;
    }

    pHealth->bOk =
        pHealth->bProfileOk &&
        (!pUseOptions->bCheckSqlite ||
         (pHealth->bSqliteOpen &&
          pHealth->bSchemaOk &&
          pHealth->iRecordCount == pHealth->iSqliteRecordCount &&
          pHealth->iChunkCount == pHealth->iSqliteChunkCount &&
          pHealth->iOrphanChunkCount == 0u &&
          (!pUseOptions->bCheckSparsePostings || pHealth->bSparsePostingsOk)));
    xrtMutexUnlock(pMemory->pMutex);

    return XRT_NET_OK;
}

XLLM_API int xllm_memory_get_workspace_status(
    const xllm_memory *pMemory,
    const xllm_memory_workspace_status_options *pOptions,
    xllm_memory_workspace_status *pStatus,
    xllm_error *pError
)
{
    xllm_memory_workspace_status_options tDefaultOptions;
    const xllm_memory_workspace_status_options *pUseOptions = pOptions;
    size_t i;

    if ( !pMemory || !pStatus ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory workspace status requires memory and output");
        return XRT_NET_ERROR;
    }
    if ( !pUseOptions ) {
        xllm_memory_workspace_status_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    xllm_memory_workspace_status_init(pStatus);
    pStatus->eScope = pUseOptions->eScope;
    pStatus->sRootPath = pUseOptions->sRootPath;
    pStatus->sSourceUriPrefix = pUseOptions->sSourceUriPrefix;
    pStatus->bRootPathFilterSet = pUseOptions->sRootPath && pUseOptions->sRootPath[0];
    pStatus->bSourceUriPrefixFilterSet = pUseOptions->sSourceUriPrefix && pUseOptions->sSourceUriPrefix[0];
    pStatus->tVendorExtra = pUseOptions->tVendorExtra;

    xrtMutexLock(pMemory->pMutex);
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        const xllm__memory_record_entry *pRecord = &pMemory->pRecords[i];
        const char *sPath = NULL;
        const char *sSensitivity = NULL;
        const char *sSourceTrust = NULL;
        int64 iMtimeUnix = 0;
        int64 iFileBytes = 0;

        if ( !xllm__memory_status_scope_matches(pUseOptions->eScope, pRecord->eScope) ) {
            continue;
        }
        if ( !xllm__memory_status_source_prefix_matches(pUseOptions->sSourceUriPrefix, pRecord->sSourceUri) ) {
            continue;
        }
        if ( pRecord->tMetadata && xvoType(pRecord->tMetadata) == XVO_DT_TABLE ) {
            sPath = (const char *)xvoTableGetText(pRecord->tMetadata, (str)XLLM__MEMORY_METADATA_KEY_PATH, 0u);
            sSensitivity = (const char *)xvoTableGetText(pRecord->tMetadata, (str)XLLM__MEMORY_METADATA_KEY_SENSITIVITY, 0u);
            sSourceTrust = (const char *)xvoTableGetText(pRecord->tMetadata, (str)XLLM__MEMORY_METADATA_KEY_SOURCE_TRUST, 0u);
            iMtimeUnix = xvoTableGetInt(pRecord->tMetadata, (str)XLLM__MEMORY_METADATA_KEY_MTIME_UNIX, 0u);
            iFileBytes = xvoTableGetInt(pRecord->tMetadata, (str)XLLM__MEMORY_METADATA_KEY_BYTES, 0u);
        }
        if ( pStatus->bRootPathFilterSet ) {
            if ( !sPath || !sPath[0] ) {
                ++pStatus->iMissingPathRecordCount;
                continue;
            }
            if ( !xllm__memory_path_is_under_root_ci(pUseOptions->sRootPath, sPath, NULL) ) {
                continue;
            }
        } else if ( !sPath || !sPath[0] ) {
            ++pStatus->iMissingPathRecordCount;
        }

        ++pStatus->iRecordCount;
        pStatus->iChunkCount += pRecord->iChunkCount;
        if ( iFileBytes > 0 ) {
            pStatus->uTotalFileBytes += (uint64)iFileBytes;
        }
        if ( iMtimeUnix > 0 ) {
            if ( pStatus->iOldestMtimeUnix == 0 || iMtimeUnix < pStatus->iOldestMtimeUnix ) {
                pStatus->iOldestMtimeUnix = iMtimeUnix;
            }
            if ( iMtimeUnix > pStatus->iNewestMtimeUnix ) {
                pStatus->iNewestMtimeUnix = iMtimeUnix;
            }
        }
        if ( xllm__memory_status_is_non_normal_sensitivity(sSensitivity) ) {
            ++pStatus->iSensitiveRecordCount;
        }
        if ( xllm__memory_status_is_untrusted_source(sSourceTrust) ) {
            ++pStatus->iUntrustedRecordCount;
        }
    }
    xrtMutexUnlock(pMemory->pMutex);

    return XRT_NET_OK;
}
