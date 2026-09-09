#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"
#include "xllm-memory.h"

static int require_true(int condition, const char *sMessage)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static int smoke_skip(const char *sReason)
{
    printf("smoke_memory_builtin_e5 skipped: %s\n", sReason ? sReason : "unknown reason");
    return 0;
}

static int file_exists(const char *sPath)
{
    FILE *pFile;

    if ( !sPath || !sPath[0] ) {
        return 0;
    }
    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return 0;
    }
    fclose(pFile);
    return 1;
}

static const char *find_sqlite_vec_path(void)
{
    static const char *const sCandidates[] = {
        NULL,
        "lib\\sqlite-vec\\sqlite-vec.dll",
        "build\\sqlite-vec\\sqlite-vec.dll",
        "dev\\sqlite-vec\\build\\sqlite-vec.dll"
    };
    const char *sEnv = getenv("XLLM_MEMORY_SQLITE_VEC_PATH");
    size_t i;

    if ( sEnv && file_exists(sEnv) ) {
        return sEnv;
    }
    for ( i = 1u; i < (sizeof(sCandidates) / sizeof(sCandidates[0])); ++i ) {
        if ( file_exists(sCandidates[i]) ) {
            return sCandidates[i];
        }
    }
    return NULL;
}

static int query_meta_text(
    const char *sDbPath,
    const char *sNamespace,
    const char *sKey,
    char *sOut,
    size_t iOutCapacity
)
{
    sqlite3 *pDb = NULL;
    sqlite3_stmt *pStmt = NULL;
    int iRc;

    if ( sOut && iOutCapacity > 0u ) {
        sOut[0] = '\0';
    }
    if ( !sDbPath || !sNamespace || !sKey || !sOut || iOutCapacity == 0u ) {
        return 1;
    }

    iRc = sqlite3_open(sDbPath, &pDb);
    if ( iRc != SQLITE_OK ) {
        if ( pDb ) {
            sqlite3_close(pDb);
        }
        return 2;
    }

    iRc = sqlite3_prepare_v2(
        pDb,
        "SELECT value_text FROM xllm_memory_meta WHERE namespace = ?1 AND key = ?2;",
        -1,
        &pStmt,
        NULL
    );
    if ( iRc != SQLITE_OK ) {
        sqlite3_close(pDb);
        return 3;
    }

    sqlite3_bind_text(pStmt, 1, sNamespace, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(pStmt, 2, sKey, -1, SQLITE_TRANSIENT);
    iRc = sqlite3_step(pStmt);
    if ( iRc == SQLITE_ROW ) {
        const char *sValue = (const char *)sqlite3_column_text(pStmt, 0);
        snprintf(sOut, iOutCapacity, "%s", sValue ? sValue : "");
        iRc = SQLITE_DONE;
    }

    sqlite3_finalize(pStmt);
    sqlite3_close(pDb);
    return iRc == SQLITE_DONE ? 0 : 4;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_builtin_embedder_probe tProbe;
    xllm_memory_ingest_options tIngest;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    xllm_memory_diagnostics tDiagnostics;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    xllm_memory *pReloadedMemory = NULL;
    const char *sDbPath = "build\\smoke_memory_builtin_e5.db";
    const char *sNamespace = "smoke-e5";
    const char *sVectorExt = NULL;
    const char *sVectorMode = "in-memory cosine fallback";
    char sStoredModelId[128];
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    memset(&tProbe, 0, sizeof(tProbe));
    xllm_memory_ingest_options_init(&tIngest);
    xllm_memory_search_options_init(&tSearchOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_memory_diagnostics_init(&tDiagnostics);

    tEmbedderOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX;
    iStatus = xllm_memory_probe_builtin_embedder(&tEmbedderOptions, &tProbe, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "builtin e5 probe failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_error_free(&tError);
        return 1;
    }
    if ( !tProbe.bReady ) {
        xllm_memory_builtin_embedder_probe_reset(&tProbe);
        xllm_error_free(&tError);
        return smoke_skip("E5 runtime/model/tokenizer assets are not ready");
    }
    sVectorExt = find_sqlite_vec_path();
    if ( sVectorExt ) {
        sVectorMode = "sqlite-vec candidate retrieval";
    }

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        xllm_memory_builtin_embedder_probe_reset(&tProbe);
        xllm_error_free(&tError);
        return 2;
    }

    remove(sDbPath);
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_ONNX_E5;
    tMemoryOptions.sMemoryProfileId = "onnx_e5.v1";
    tMemoryOptions.sNamespace = sNamespace;
    tMemoryOptions.sSqlitePath = sDbPath;
    tMemoryOptions.sSqliteVectorExtensionPath = sVectorExt;
    tMemoryOptions.bLoadSqliteVectorExtension = sVectorExt != NULL;
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        xllm_runtime_destroy(pRuntime);
        xllm_memory_builtin_embedder_probe_reset(&tProbe);
        xllm_error_free(&tError);
        return 4;
    }

    iStatus = xllm_memory_get_diagnostics(pMemory, &tDiagnostics, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "builtin e5 diagnostics failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_memory_builtin_embedder_probe_reset(&tProbe);
        xllm_error_free(&tError);
        return 41;
    }
    if ( require_true(tDiagnostics.eBuiltinEmbedderKind == XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX, "e5 diagnostics builtin kind mismatch") != 0 ||
         require_true(tDiagnostics.sEmbedProfileId && strcmp(tDiagnostics.sEmbedProfileId, "multilingual-e5-small.onnx.v1") == 0, "e5 diagnostics embed profile mismatch") != 0 ||
         require_true(tDiagnostics.sEmbedModelId && strcmp(tDiagnostics.sEmbedModelId, "multilingual-e5-small") == 0, "e5 diagnostics model id mismatch") != 0 ||
         require_true(tDiagnostics.sEmbedRepoId && strstr(tDiagnostics.sEmbedRepoId, "multilingual-e5-small") != NULL, "e5 diagnostics repo id mismatch") != 0 ||
         require_true(tDiagnostics.sEmbedRuntimeDllPath && tDiagnostics.sEmbedRuntimeDllPath[0], "e5 diagnostics runtime path missing") != 0 ||
         require_true(tDiagnostics.sEmbedModelPath && tDiagnostics.sEmbedModelPath[0], "e5 diagnostics model path missing") != 0 ||
         require_true(tDiagnostics.sEmbedTokenizerPath && tDiagnostics.sEmbedTokenizerPath[0], "e5 diagnostics tokenizer path missing") != 0 ||
         require_true(tDiagnostics.sEmbedQueryPrefix && strcmp(tDiagnostics.sEmbedQueryPrefix, "query: ") == 0, "e5 diagnostics query prefix mismatch") != 0 ||
         require_true(tDiagnostics.sEmbedDocumentPrefix && strcmp(tDiagnostics.sEmbedDocumentPrefix, "passage: ") == 0, "e5 diagnostics document prefix mismatch") != 0 ||
         require_true(tDiagnostics.sEmbedPoolingMode && strcmp(tDiagnostics.sEmbedPoolingMode, "mean") == 0, "e5 diagnostics pooling mismatch") != 0 ||
         require_true(tDiagnostics.bEmbedNormalize, "e5 diagnostics normalize mismatch") != 0 ||
         require_true(tDiagnostics.uEmbedMaxInputTokens == 512u, "e5 diagnostics token limit mismatch") != 0 ||
         require_true(tDiagnostics.uEmbedDimensions == 384u, "e5 diagnostics dimension mismatch") != 0 ) {
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_memory_builtin_embedder_probe_reset(&tProbe);
        xllm_error_free(&tError);
        return 42;
    }
    if ( query_meta_text(sDbPath, sNamespace, "embed_model_id", sStoredModelId, sizeof(sStoredModelId)) != 0 ||
         require_true(strcmp(sStoredModelId, "multilingual-e5-small") == 0, "e5 sqlite embed model id metadata mismatch") != 0 ) {
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_memory_builtin_embedder_probe_reset(&tProbe);
        xllm_error_free(&tError);
        return 43;
    }

    tIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngest.sRecordId = "builtin-e5";
    tIngest.sTitle = "builtin e5 smoke";
    tIngest.sSourceUri = "workspace://smoke_memory_builtin_e5";
    tIngest.sText =
        "xllm-memory should support builtin ONNX embedding, sqlite persistence, "
        "embedding reload, vector retrieval, and context-oriented search over persisted chunks.";
    iStatus = xllm_memory_ingest_text(pMemory, &tIngest, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "builtin e5 ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_memory_builtin_embedder_probe_reset(&tProbe);
        xllm_error_free(&tError);
        return 5;
    }

    xllm_memory_destroy(pMemory);
    pMemory = NULL;

    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pReloadedMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory reload failed: %d\n", iStatus);
        xllm_runtime_destroy(pRuntime);
        xllm_memory_builtin_embedder_probe_reset(&tProbe);
        xllm_error_free(&tError);
        return 6;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSearchOptions.sQuery = "How does xllm-memory persist embeddings and search chunks?";
    tSearchOptions.uMaxHits = 3u;
    iStatus = xllm_memory_search(pReloadedMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "builtin e5 search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pReloadedMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_memory_builtin_embedder_probe_reset(&tProbe);
        xllm_error_free(&tError);
        return 7;
    }

    if ( tSearchResult.iHitCount == 0u ) {
        fprintf(stderr, "builtin e5 search returned no hits\n");
        xllm_memory_search_result_reset(&tSearchResult);
        xllm_memory_destroy(pReloadedMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_memory_builtin_embedder_probe_reset(&tProbe);
        xllm_error_free(&tError);
        return 8;
    }

    printf("smoke_memory_builtin_e5 ok (%s)\n", sVectorMode);

    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_destroy(pReloadedMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_memory_builtin_embedder_probe_reset(&tProbe);
    xllm_error_free(&tError);
    return 0;
}
