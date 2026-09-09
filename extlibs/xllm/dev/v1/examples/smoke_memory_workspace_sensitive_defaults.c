#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

static int write_text_file(const char *sPath, const char *sText)
{
    FILE *pFile = fopen(sPath, "wb");
    size_t iLen;

    if ( !pFile ) {
        return 1;
    }
    iLen = sText ? strlen(sText) : 0u;
    if ( iLen > 0u && fwrite(sText, 1u, iLen, pFile) != iLen ) {
        fclose(pFile);
        return 2;
    }
    fclose(pFile);
    return 0;
}

static int require_true(int condition, const char *sMessage)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static size_t count_skip_reason(
    const xllm_memory_ingest_directory_result *pResult,
    xllm_memory_skip_reason eReason
)
{
    size_t i;
    size_t iCount = 0u;

    if ( !pResult ) {
        return 0u;
    }
    for ( i = 0u; i < pResult->iSkippedDetailCount; ++i ) {
        if ( pResult->pSkippedFiles[i].eReason == eReason ) {
            ++iCount;
        }
    }
    return iCount;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_ingest_directory_options tDirectoryOptions;
    xllm_memory_ingest_directory_result tDirectoryResult;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecords;
    xllm_memory_chunk_list_result tChunks;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sRootDir = "build\\smoke_memory_workspace_sensitive_defaults_tmp";
    const char *sSrcDir = "build\\smoke_memory_workspace_sensitive_defaults_tmp\\src";
    const char *sConfigDir = "build\\smoke_memory_workspace_sensitive_defaults_tmp\\config";
    const char *sCertDir = "build\\smoke_memory_workspace_sensitive_defaults_tmp\\certs";
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_directory_options_init(&tDirectoryOptions);
    xllm_memory_ingest_directory_result_init(&tDirectoryResult);
    xllm_memory_search_options_init(&tSearchOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_memory_list_options_init(&tListOptions);
    memset(&tRecords, 0, sizeof(tRecords));
    memset(&tChunks, 0, sizeof(tChunks));

    (void)xrtDirDelete((str)sRootDir);
    if ( !xrtDirCreateAll((str)sSrcDir) ||
         !xrtDirCreateAll((str)sConfigDir) ||
         !xrtDirCreateAll((str)sCertDir) ) {
        fprintf(stderr, "failed to prepare workspace directories\n");
        return 1;
    }
    if ( write_text_file("build\\smoke_memory_workspace_sensitive_defaults_tmp\\README.md", "safe workspace docs should be indexed.\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_sensitive_defaults_tmp\\src\\main.c", "int main(void) { return 0; } /* safe workspace code */\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_sensitive_defaults_tmp\\src\\config_inline.c", "const char *k = \"OPENAI_API_KEY=do-not-index\";\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_sensitive_defaults_tmp\\api_token.txt", "super-secret-token should not be indexed\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_sensitive_defaults_tmp\\config\\client_secret.json", "{\"client_secret\":\"do-not-index\"}\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_sensitive_defaults_tmp\\credentials.yaml", "password: do-not-index\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_sensitive_defaults_tmp\\.env.local", "API_KEY=do-not-index\n") != 0 ||
         write_text_file("build\\smoke_memory_workspace_sensitive_defaults_tmp\\certs\\prod.key", "PRIVATE KEY should not be indexed\n") != 0 ) {
        fprintf(stderr, "failed to write workspace files\n");
        return 2;
    }

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tEmbedderOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_HASH;
    tEmbedderOptions.uHashDimensions = 32u;
    iStatus = xllm_memory_make_builtin_embedder(&tEmbedderOptions, &tMemoryOptions.tEmbedder, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "builtin embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-sensitive-defaults";
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tDirectoryOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tDirectoryOptions.sPath = sRootDir;
    tDirectoryOptions.bRecursive = true;
    tDirectoryOptions.bSkipHidden = false;
    tDirectoryOptions.bUseWorkspaceDefaults = true;
    tDirectoryOptions.sRecordIdPrefix = "sensitive-defaults";
    iStatus = xllm_memory_ingest_directory(pMemory, &tDirectoryOptions, &tDirectoryResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "workspace ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    if ( require_true(tDirectoryResult.uVisitedFileCount == 8u, "expected eight visited files") != 0 ||
         require_true(tDirectoryResult.uIngestedFileCount == 2u, "expected two safe files ingested") != 0 ||
         require_true(tDirectoryResult.uSkippedFileCount == 6u, "expected six sensitive files skipped") != 0 ||
         require_true(count_skip_reason(&tDirectoryResult, XLLM_MEMORY_SKIP_IGNORED_PATH_PATTERN) >= 4u, "expected sensitive name path-pattern skips") != 0 ||
         require_true(count_skip_reason(&tDirectoryResult, XLLM_MEMORY_SKIP_IGNORED_EXTENSION) >= 1u, "expected key extension skip") != 0 ||
         require_true(count_skip_reason(&tDirectoryResult, XLLM_MEMORY_SKIP_SECRET_DETECTED) == 1u, "expected one content secret scanner skip") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) == 2u, "expected two records after ingest") != 0 ) {
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tListOptions.uMaxItems = 8u;
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_chunks(pMemory, &tListOptions, &tChunks, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list chunks failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tChunks.iChunkCount == 2u, "expected two safe chunks") != 0 ) {
        goto cleanup;
    }
    {
        size_t i;
        for ( i = 0u; i < tChunks.iChunkCount; ++i ) {
            const char *sText = tChunks.pChunks[i].sText ? tChunks.pChunks[i].sText : "";
            if ( require_true(strstr(sText, "super-secret-token") == NULL, "token content should not be chunked") != 0 ||
                 require_true(strstr(sText, "do-not-index") == NULL, "secret content should not be chunked") != 0 ||
                 require_true(strstr(sText, "OPENAI_API_KEY") == NULL, "inline api key should not be chunked") != 0 ||
                 require_true(strstr(sText, "PRIVATE KEY") == NULL, "private key content should not be chunked") != 0 ) {
                goto cleanup;
            }
        }
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tListOptions.uMaxItems = 8u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 2u, "expected two listed records") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(tRecords.pRecords[0].tMetadata, (str)"sensitivity", 0u), "normal") == 0, "missing sensitivity metadata") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(tRecords.pRecords[0].tMetadata, (str)"source_trust", 0u), "local_file") == 0, "missing source_trust metadata") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(tRecords.pRecords[0].tMetadata, (str)"user_scope", 0u), "project") == 0, "missing user_scope metadata") != 0 ) {
        goto cleanup;
    }

    tSearchOptions.sQuery = "safe workspace code docs";
    tSearchOptions.uMaxHits = 2u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "safe search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount > 0u, "safe workspace content should be searchable") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_workspace_sensitive_defaults ok\n");
    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_chunk_list_result_reset(&tChunks);
    xllm_memory_record_list_result_reset(&tRecords);
    xllm_memory_ingest_directory_result_reset(&tDirectoryResult);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
