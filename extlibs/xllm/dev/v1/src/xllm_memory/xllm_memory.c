#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <time.h>
#endif

#include <sqlite3.h>

#include "xllm_memory.h"
#include "../xllm_chunk/xllm_chunk.h"
#include "../xllm_embed/xllm_embed.h"
#if XLLM__MEMORY_HAS_SCHEME_ONNX_E5
#include "../xllm_embed/xllm_embed_builtin.h"
#endif

typedef struct {
    char *sChunkId;
    char *sText;
    char *sMemoryProfileId;
    char *sChunkProfileId;
    char *sRetrievalProfileId;
    char *sEmbedProfileId;
    char *sIndexProfileId;
    char *sPreviousChunkId;
    char *sNextChunkId;
    size_t iStartByte;
    size_t iEndByte;
    uint64 uContentHash;
    uint32 uChunkIndex;
    uint32 uProfileVersion;
    int64 iVectorRowId;
    float *pfEmbedding;
    uint32 uEmbeddingDim;
} xllm__memory_chunk_entry;

typedef struct {
    int64 iRowId;
    double fScore;
} xllm__memory_vector_candidate;

typedef struct {
    xllm_memory_scheme eScheme;
    const char *sResolvedProfileId;
    bool bAutoCreateBuiltinEmbedder;
} xllm__memory_scheme_selection;

typedef struct {
    xllm_memory *pMemory;
    const xllm_memory_ingest_directory_options *pOptions;
    xllm_memory_ingest_directory_result *pResult;
    const char *sRootPath;
    bool bProgressAborted;
} xllm__memory_directory_ingest_state;

typedef struct {
    const char *sWorkspaceRootPath;
    char **psPatterns;
    size_t *piLength;
    size_t *piCapacity;
    bool bOutOfMemory;
} xllm__memory_ignore_file_scan_state;

typedef struct {
    xllm_memory_scope eScope;
    char *sRecordId;
    char *sTitle;
    char *sSourceUri;
    char *sText;
    char *sMemoryProfileId;
    char *sRetrievalProfileId;
    char *sEmbedProfileId;
    char *sIndexProfileId;
    uint32 uProfileVersion;
    xvalue tMetadata;
    xvalue tVendorExtra;
    xllm__memory_chunk_entry *pChunks;
    size_t iChunkCount;
    size_t iChunkCapacity;
} xllm__memory_record_entry;

struct xllm_memory {
    xllm_runtime *pRuntime;
    xmutex pMutex;
    xllm_memory_scheme eScheme;
    char *sNamespace;
    char *sMemoryProfileId;
    char *sSqlitePath;
    char *sSqliteVectorExtensionPath;
    char *sStoredMemoryProfileId;
    char *sVectorTableName;
    bool bLoadSqliteVectorExtension;
    bool bSqliteVectorExtensionLoaded;
    bool bSqliteWalEnabled;
    bool bSqliteWalRequested;
    bool bEnableHybridSearch;
    bool bVectorTableReady;
    double fLexicalWeight;
    double fVectorWeight;
    xllm_memory_embedder tEmbedder;
    bool bOwnsEmbedderCtx;
    sqlite3 *pSqliteDb;
    uint32 uSqliteSchemaVersion;
    uint32 uSqliteBusyTimeoutMs;
    uint32 uVectorDim;
    uint32 uDefaultChunkChars;
    uint32 uDefaultChunkOverlapChars;
    uint32 uDefaultMaxHits;
    xvalue tVendorExtra;
    xllm__memory_record_entry *pRecords;
    size_t iRecordCount;
    size_t iRecordCapacity;
};

struct xllm_memory_file_event_queue {
    xmutex pMutex;
    xllm_memory_file_event *pItems;
    size_t iItemCount;
    size_t iItemCapacity;
};

struct xllm_memory_watcher_bridge {
    xllm_memory *pMemory;
    xllm_memory_file_event_queue *pQueue;
    xllm_memory_file_event_queue_drain_options tDrainOptions;
};

struct xllm_memory_watcher_pump {
    xllm_memory_watcher_bridge *pBridge;
    size_t iAutoFlushThreshold;
};

struct xllm_memory_watcher_worker {
    xllm_memory_watcher_pump *pPump;
    uint32 uDebounceMs;
    size_t iDefaultMaxItems;
    uint64 uLastActivityAtMs;
};

static const char *XLLM__MEMORY_WORKSPACE_ALLOWED_EXTENSIONS =
    ".c;.cc;.cpp;.cxx;.h;.hh;.hpp;.hxx;.m;.mm;"
    ".java;.kt;.kts;.py;.rs;.go;.js;.jsx;.ts;.tsx;"
    ".json;.toml;.yaml;.yml;.xml;.html;.htm;.css;.scss;.less;"
    ".sql;.md;.txt;.bat;.ps1;.sh;.cmake;.gradle;.properties;"
    ".proto;.cs;.fs;.swift;.rb;.php;.lua;.dart;.vue";

static const char *XLLM__MEMORY_WORKSPACE_IGNORED_DIRECTORIES =
    ".git;.svn;.hg;node_modules;dist;build;out;target;"
    ".idea;.vs;.vscode;.venv;venv;__pycache__;coverage;"
    ".next;.nuxt;.turbo;.cache;bin;obj;Debug;Release";

static const char *XLLM__MEMORY_WORKSPACE_IGNORED_EXTENSIONS =
    ".png;.jpg;.jpeg;.gif;.bmp;.webp;.ico;.pdf;"
    ".zip;.7z;.rar;.tar;.gz;"
    ".dll;.so;.dylib;.exe;.class;.jar;.o;.obj;.a;.lib;.pdb;"
    ".onnx;.db;.sqlite;.sqlite3;"
    ".pem;.key;.p12;.pfx;.crt;.cer;.der";

static const char *XLLM__MEMORY_WORKSPACE_IGNORED_PATH_PATTERNS =
    ".env*;*.env;*.env.*;"
    "*secret*;*secrets*;*token*;*credential*;*credentials*;"
    "id_rsa;id_dsa;id_ecdsa;id_ed25519";

static uint64 xllm__memory_now_ms(void)
{
#ifdef _WIN32
    return (uint64)GetTickCount64();
#else
    struct timespec tNow;

    if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) {
        return 0u;
    }
    return ((uint64)tNow.tv_sec * 1000u) + (uint64)(tNow.tv_nsec / 1000000u);
#endif
}

static const uint64 XLLM__MEMORY_WORKSPACE_DEFAULT_MAX_FILE_BYTES = 1024u * 1024u;
static const char *XLLM__MEMORY_WORKSPACE_DEFAULT_SOURCE_URI_PREFIX = "workspace://";
static const char *XLLM__MEMORY_CONVERSATION_SOURCE_URI_PREFIX = "conversation://";
static const char *XLLM__MEMORY_TASK_SOURCE_URI_PREFIX = "task://";
static const char *XLLM__MEMORY_FACT_SOURCE_URI_PREFIX = "fact://";
static const char *XLLM__MEMORY_PREFERENCE_SOURCE_URI_PREFIX = "preference://";
static const char *XLLM__MEMORY_METADATA_KEY_PATH = "path";
static const char *XLLM__MEMORY_METADATA_KEY_RELATIVE_PATH = "relative_path";
static const char *XLLM__MEMORY_METADATA_KEY_BASENAME = "basename";
static const char *XLLM__MEMORY_METADATA_KEY_EXTENSION = "extension";
static const char *XLLM__MEMORY_METADATA_KEY_BYTES = "bytes";
static const char *XLLM__MEMORY_METADATA_KEY_MTIME_UNIX = "mtime_unix";
static const char *XLLM__MEMORY_METADATA_KEY_CONTENT_HASH = "content_hash";
static const char *XLLM__MEMORY_METADATA_KEY_SENSITIVITY = "sensitivity";
static const char *XLLM__MEMORY_METADATA_KEY_SOURCE_TRUST = "source_trust";
static const char *XLLM__MEMORY_METADATA_KEY_USER_SCOPE = "user_scope";
static const char *XLLM__MEMORY_METADATA_KEY_MEMORY_TYPE = "memory_type";
static const char *XLLM__MEMORY_METADATA_KEY_EXTRACTION_POLICY = "extraction_policy";
static const char *XLLM__MEMORY_METADATA_KEY_CONVERSATION_KIND = "conversation_kind";
static const char *XLLM__MEMORY_METADATA_KEY_CONVERSATION_ID = "conversation_id";
static const char *XLLM__MEMORY_METADATA_KEY_TURN_ID = "turn_id";
static const char *XLLM__MEMORY_METADATA_KEY_STABLE_IDENTITY = "stable_identity";
static const char *XLLM__MEMORY_METADATA_KEY_PRIORITY = "priority";
static const char *XLLM__MEMORY_METADATA_KEY_EXPIRES_AT_UNIX = "expires_at_unix";
static const char *XLLM__MEMORY_METADATA_KEY_CREATED_AT_UNIX = "created_at_unix";
static const char *XLLM__MEMORY_METADATA_KEY_UPDATED_AT_UNIX = "updated_at_unix";
static const char *XLLM__MEMORY_METADATA_KEY_TURN_MESSAGE_COUNT = "turn_message_count";
static const char *XLLM__MEMORY_METADATA_KEY_TURN_CONTEXT_BLOCK_COUNT = "turn_context_block_count";
static const char *XLLM__MEMORY_METADATA_KEY_HAS_SYSTEM_PROMPT = "has_system_prompt";
static const char *XLLM__MEMORY_METADATA_KEY_RESPONSE_OUTPUT_COUNT = "response_output_count";
static const char *XLLM__MEMORY_METADATA_KEY_RESPONSE_TOOL_CALL_COUNT = "response_tool_call_count";
static const char *XLLM__MEMORY_METADATA_KEY_RESPONSE_STATUS = "response_status";
static const char *XLLM__MEMORY_METADATA_KEY_INCLUDE_CONTEXT_BLOCKS = "include_context_blocks";
static const char *XLLM__MEMORY_METADATA_KEY_INCLUDE_THINKING = "include_thinking";
static const char *XLLM__MEMORY_METADATA_KEY_TASK_ID = "task_id";
static const char *XLLM__MEMORY_METADATA_KEY_TASK_STATUS = "task_status";
static const char *XLLM__MEMORY_METADATA_KEY_TASK_OWNER = "task_owner";
static const char *XLLM__MEMORY_METADATA_KEY_TASK_DEADLINE_UNIX = "task_deadline_unix";
static const char *XLLM__MEMORY_METADATA_KEY_FACT_ID = "fact_id";
static const char *XLLM__MEMORY_METADATA_KEY_FACT_SUBJECT = "fact_subject";
static const char *XLLM__MEMORY_METADATA_KEY_FACT_PREDICATE = "fact_predicate";
static const char *XLLM__MEMORY_METADATA_KEY_FACT_OBJECT = "fact_object";
static const char *XLLM__MEMORY_METADATA_KEY_PREFERENCE_ID = "preference_id";
static const char *XLLM__MEMORY_METADATA_KEY_PREFERENCE_SUBJECT = "preference_subject";
static const char *XLLM__MEMORY_METADATA_KEY_PREFERENCE_KEY = "preference_key";
static const char *XLLM__MEMORY_METADATA_KEY_PREFERENCE_VALUE = "preference_value";
static const char *XLLM__MEMORY_METADATA_KEY_SOURCE_CONVERSATION_ID = "source_conversation_id";
static const char *XLLM__MEMORY_METADATA_KEY_SOURCE_TURN_ID = "source_turn_id";
static const char *XLLM__MEMORY_EMBED_EXTRA_BUILTIN_KIND = "builtin_embedder_kind";
static const char *XLLM__MEMORY_EMBED_EXTRA_EMBEDDER_KIND = "embedder_kind";
static const char *XLLM__MEMORY_EMBED_EXTRA_PROFILE_ID = "embed_profile_id";
static const char *XLLM__MEMORY_EMBED_EXTRA_MODEL_ID = "embed_model_id";
static const char *XLLM__MEMORY_EMBED_EXTRA_REPO_ID = "embed_repo_id";
static const char *XLLM__MEMORY_EMBED_EXTRA_RUNTIME_DLL_PATH = "embed_runtime_dll_path";
static const char *XLLM__MEMORY_EMBED_EXTRA_MODEL_PATH = "embed_model_path";
static const char *XLLM__MEMORY_EMBED_EXTRA_TOKENIZER_PATH = "embed_tokenizer_path";
static const char *XLLM__MEMORY_EMBED_EXTRA_QUERY_PREFIX = "embed_query_prefix";
static const char *XLLM__MEMORY_EMBED_EXTRA_DOCUMENT_PREFIX = "embed_document_prefix";
static const char *XLLM__MEMORY_EMBED_EXTRA_POOLING_MODE = "embed_pooling_mode";
static const char *XLLM__MEMORY_EMBED_EXTRA_NORMALIZE = "embed_normalize";
static const char *XLLM__MEMORY_EMBED_EXTRA_DIMENSIONS = "embed_dimensions";
static const char *XLLM__MEMORY_EMBED_EXTRA_MAX_INPUT_TOKENS = "embed_max_input_tokens";

static const char *xllm__memory_extraction_policy_name(xllm_memory_extraction_policy ePolicy)
{
    switch ( ePolicy ) {
        case XLLM_MEMORY_EXTRACTION_POLICY_NONE:
            return "none";
        case XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY:
            return "summary";
        case XLLM_MEMORY_EXTRACTION_POLICY_TASK:
            return "task";
        case XLLM_MEMORY_EXTRACTION_POLICY_FACT:
            return "fact";
        case XLLM_MEMORY_EXTRACTION_POLICY_PREFERENCE:
            return "preference";
        case XLLM_MEMORY_EXTRACTION_POLICY_DEFAULT:
        case XLLM_MEMORY_EXTRACTION_POLICY_TURN_RESPONSE:
            return "turn_response";
        default:
            return "unknown";
    }
}

static const char *xllm__memory_task_status_name(xllm_memory_task_status eStatus)
{
    switch ( eStatus ) {
        case XLLM_MEMORY_TASK_STATUS_DONE:
            return "done";
        case XLLM_MEMORY_TASK_STATUS_CANCELED:
            return "canceled";
        case XLLM_MEMORY_TASK_STATUS_OPEN:
        default:
            return "open";
    }
}

static xllm_memory_extraction_policy xllm__memory_resolve_extraction_policy(
    xllm_memory_extraction_policy ePolicy
)
{
    if ( ePolicy == XLLM_MEMORY_EXTRACTION_POLICY_DEFAULT ) {
        return XLLM_MEMORY_EXTRACTION_POLICY_TURN_RESPONSE;
    }
    return ePolicy;
}

static int xllm__memory_sqlite_delete_record_locked(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sRecordId
);

typedef struct {
    char *sText;
    size_t iLength;
    size_t iCapacity;
} xllm__memory_text_builder;

static bool xllm__memory_embedder_is_configured(const xllm_memory_embedder *pEmbedder)
{
    return pEmbedder && pEmbedder->pfnEmbedText != NULL;
}

static uint64 xllm__memory_hash_namespace(const char *sNamespace);

static const char *xllm__memory_default_profile_id(xllm_memory_scheme eScheme)
{
    switch ( eScheme ) {
        case XLLM_MEMORY_SCHEME_BUILTIN_SPARSE:
            return "builtin_sparse.v1";
        case XLLM_MEMORY_SCHEME_ONNX_E5:
            return "onnx_e5.v1";
        case XLLM_MEMORY_SCHEME_CUSTOM:
            return "custom.v1";
        case XLLM_MEMORY_SCHEME_AUTO:
        default:
            return NULL;
    }
}

static const char *xllm__memory_default_chunk_profile_id(void)
{
    return "rule_chunker.v1";
}

static const char *xllm__memory_namespace_key(const xllm_memory *pMemory)
{
    return (pMemory && pMemory->sNamespace) ? pMemory->sNamespace : "";
}

static const char *xllm__memory_default_retrieval_profile_id(const xllm_memory *pMemory)
{
    if ( !pMemory ) {
        return "unknown.v1";
    }
    switch ( pMemory->eScheme ) {
        case XLLM_MEMORY_SCHEME_BUILTIN_SPARSE:
            return "builtin_sparse.bm25.v1";
        case XLLM_MEMORY_SCHEME_ONNX_E5:
            return "onnx_e5.hybrid_rrf.v1";
        case XLLM_MEMORY_SCHEME_CUSTOM:
            return pMemory->bEnableHybridSearch ? "custom.hybrid_rrf.v1" : "custom.retrieval.v1";
        case XLLM_MEMORY_SCHEME_AUTO:
        default:
            return "auto.retrieval.v1";
    }
}

static const char *xllm__memory_default_embed_profile_id(const xllm_memory *pMemory)
{
    if ( !pMemory ) {
        return "unknown.v1";
    }
    switch ( pMemory->eScheme ) {
        case XLLM_MEMORY_SCHEME_ONNX_E5:
            return "multilingual-e5-small.onnx.v1";
        case XLLM_MEMORY_SCHEME_CUSTOM:
            return xllm__memory_embedder_is_configured(&pMemory->tEmbedder) ? "custom.embedder.v1" : "none.v1";
        case XLLM_MEMORY_SCHEME_BUILTIN_SPARSE:
        case XLLM_MEMORY_SCHEME_AUTO:
        default:
            return "none.v1";
    }
}

static const char *xllm__memory_default_index_profile_id(const xllm_memory *pMemory)
{
    if ( !pMemory ) {
        return "unknown.v1";
    }
    switch ( pMemory->eScheme ) {
        case XLLM_MEMORY_SCHEME_BUILTIN_SPARSE:
            return "sqlite.postings.v1";
        case XLLM_MEMORY_SCHEME_ONNX_E5:
            return pMemory->bVectorTableReady ? "sqlite_vec.cosine.v1" : "memory.cosine.v1";
        case XLLM_MEMORY_SCHEME_CUSTOM:
            return pMemory->bVectorTableReady ? "custom.sqlite_vec.v1" : "custom.memory_index.v1";
        case XLLM_MEMORY_SCHEME_AUTO:
        default:
            return "auto.index.v1";
    }
}

static int xllm__memory_set_profile_text(char **psDest, const char *sValue)
{
    char *sCopy = NULL;

    if ( sValue ) {
        sCopy = xllm__dup_cstr(sValue);
        if ( !sCopy ) {
            return XRT_NET_ERROR;
        }
    }
    xllm__free_cstr(psDest);
    *psDest = sCopy;
    return XRT_NET_OK;
}
static int xllm__memory_record_assign_default_profiles(
    xllm_memory *pMemory,
    xllm__memory_record_entry *pRecord
)
{
    if ( !pMemory || !pRecord ) {
        return XRT_NET_ERROR;
    }
    if ( !pRecord->sMemoryProfileId &&
         xllm__memory_set_profile_text(&pRecord->sMemoryProfileId, pMemory->sMemoryProfileId) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( !pRecord->sRetrievalProfileId &&
         xllm__memory_set_profile_text(&pRecord->sRetrievalProfileId, xllm__memory_default_retrieval_profile_id(pMemory)) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( !pRecord->sEmbedProfileId &&
         xllm__memory_set_profile_text(&pRecord->sEmbedProfileId, xllm__memory_default_embed_profile_id(pMemory)) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( !pRecord->sIndexProfileId &&
         xllm__memory_set_profile_text(&pRecord->sIndexProfileId, xllm__memory_default_index_profile_id(pMemory)) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( pRecord->uProfileVersion == 0u ) {
        pRecord->uProfileVersion = 1u;
    }
    return XRT_NET_OK;
}
static int xllm__memory_chunk_assign_default_profiles(
    xllm_memory *pMemory,
    const xllm__memory_record_entry *pRecord,
    xllm__memory_chunk_entry *pChunk
)
{
    if ( !pMemory || !pRecord || !pChunk ) {
        return XRT_NET_ERROR;
    }
    if ( !pChunk->sMemoryProfileId &&
         xllm__memory_set_profile_text(
             &pChunk->sMemoryProfileId,
             pRecord->sMemoryProfileId ? pRecord->sMemoryProfileId : pMemory->sMemoryProfileId
         ) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( !pChunk->sChunkProfileId &&
         xllm__memory_set_profile_text(&pChunk->sChunkProfileId, xllm__memory_default_chunk_profile_id()) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( !pChunk->sRetrievalProfileId &&
         xllm__memory_set_profile_text(
             &pChunk->sRetrievalProfileId,
             pRecord->sRetrievalProfileId ? pRecord->sRetrievalProfileId : xllm__memory_default_retrieval_profile_id(pMemory)
         ) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( !pChunk->sEmbedProfileId &&
         xllm__memory_set_profile_text(
             &pChunk->sEmbedProfileId,
             pRecord->sEmbedProfileId ? pRecord->sEmbedProfileId : xllm__memory_default_embed_profile_id(pMemory)
         ) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( !pChunk->sIndexProfileId &&
         xllm__memory_set_profile_text(
             &pChunk->sIndexProfileId,
             pRecord->sIndexProfileId ? pRecord->sIndexProfileId : xllm__memory_default_index_profile_id(pMemory)
         ) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( pChunk->uProfileVersion == 0u ) {
        pChunk->uProfileVersion = pRecord->uProfileVersion > 0u ? pRecord->uProfileVersion : 1u;
    }
    return XRT_NET_OK;
}

static int xllm__memory_record_assign_chunk_adjacency(xllm__memory_record_entry *pRecord)
{
    size_t i;

    if ( !pRecord ) {
        return XRT_NET_ERROR;
    }
    for ( i = 0u; i < pRecord->iChunkCount; ++i ) {
        xllm__memory_chunk_entry *pChunk = &pRecord->pChunks[i];

        xllm__free_cstr(&pChunk->sPreviousChunkId);
        xllm__free_cstr(&pChunk->sNextChunkId);
        if ( i > 0u ) {
            pChunk->sPreviousChunkId = xllm__dup_cstr(pRecord->pChunks[i - 1u].sChunkId);
            if ( !pChunk->sPreviousChunkId ) {
                return XRT_NET_ERROR;
            }
        }
        if ( i + 1u < pRecord->iChunkCount ) {
            pChunk->sNextChunkId = xllm__dup_cstr(pRecord->pChunks[i + 1u].sChunkId);
            if ( !pChunk->sNextChunkId ) {
                return XRT_NET_ERROR;
            }
        }
    }
    return XRT_NET_OK;
}

static bool xllm__memory_scheme_is_available(xllm_memory_scheme eScheme)
{
    switch ( eScheme ) {
        case XLLM_MEMORY_SCHEME_BUILTIN_SPARSE:
            return XLLM__MEMORY_HAS_SCHEME_BUILTIN_SPARSE != 0;
        case XLLM_MEMORY_SCHEME_ONNX_E5:
            return XLLM__MEMORY_HAS_SCHEME_ONNX_E5 != 0;
        case XLLM_MEMORY_SCHEME_CUSTOM:
            return XLLM__MEMORY_HAS_SCHEME_CUSTOM != 0;
        case XLLM_MEMORY_SCHEME_AUTO:
        default:
            return false;
    }
}

static bool xllm__memory_try_scheme_from_profile_id(
    const char *sProfileId,
    xllm_memory_scheme *peScheme
)
{
    if ( !sProfileId || !sProfileId[0] || !peScheme ) {
        return false;
    }

    if ( strcmp(sProfileId, "builtin_sparse") == 0 ||
         strcmp(sProfileId, "builtin_sparse.v1") == 0 ) {
        *peScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
        return true;
    }
    if ( strcmp(sProfileId, "onnx_e5") == 0 ||
         strcmp(sProfileId, "onnx_e5.v1") == 0 ||
         strcmp(sProfileId, "multilingual-e5-small") == 0 ) {
        *peScheme = XLLM_MEMORY_SCHEME_ONNX_E5;
        return true;
    }
    if ( strcmp(sProfileId, "custom") == 0 ||
         strcmp(sProfileId, "custom.v1") == 0 ) {
        *peScheme = XLLM_MEMORY_SCHEME_CUSTOM;
        return true;
    }

    return false;
}

static int xllm__memory_resolve_scheme(
    const xllm_memory_options *pOptions,
    xllm__memory_scheme_selection *pSelection,
    xllm_error *pError
)
{
    xllm_memory_scheme eRequested;
    xllm_memory_scheme eResolved = XLLM_MEMORY_SCHEME_AUTO;
    xllm_memory_scheme eProfileScheme = XLLM_MEMORY_SCHEME_AUTO;
    bool bHasEmbedder;
    bool bProfileRecognized;

    if ( !pOptions || !pSelection ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory scheme resolution requires options and output");
        return XRT_NET_ERROR;
    }

    memset(pSelection, 0, sizeof(*pSelection));
    eRequested = pOptions->eScheme;
    bHasEmbedder = xllm__memory_embedder_is_configured(&pOptions->tEmbedder);
    bProfileRecognized = xllm__memory_try_scheme_from_profile_id(pOptions->sMemoryProfileId, &eProfileScheme);

    if ( eRequested != XLLM_MEMORY_SCHEME_AUTO && !xllm__memory_scheme_is_available(eRequested) ) {
        xllm__error_set(
            pError,
            XLLM_ERROR_INVALID_REQUEST,
            "requested memory scheme is not compiled into this build"
        );
        return XRT_NET_ERROR;
    }

    if ( eRequested != XLLM_MEMORY_SCHEME_AUTO ) {
        eResolved = eRequested;
    } else if ( bProfileRecognized ) {
        eResolved = eProfileScheme;
    }
#if XLLM_MEMORY_SCHEME_MODE == XLLM_MEMORY_SCHEME_MODE_ONNX_E5
    else {
        eResolved = XLLM_MEMORY_SCHEME_ONNX_E5;
    }
#elif XLLM_MEMORY_SCHEME_MODE == XLLM_MEMORY_SCHEME_MODE_BUILTIN_SPARSE
    else {
        eResolved = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    }
#elif XLLM_MEMORY_SCHEME_MODE == XLLM_MEMORY_SCHEME_MODE_CUSTOM
    else {
        eResolved = XLLM_MEMORY_SCHEME_CUSTOM;
    }
#else
    else if ( bHasEmbedder ) {
        eResolved = XLLM_MEMORY_SCHEME_CUSTOM;
    } else {
        eResolved = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    }
#endif

    if ( !xllm__memory_scheme_is_available(eResolved) ) {
        xllm__error_set(
            pError,
            XLLM_ERROR_INVALID_REQUEST,
            "resolved memory scheme is not available in this build"
        );
        return XRT_NET_ERROR;
    }

    if ( bProfileRecognized && eResolved != eProfileScheme ) {
        xllm__error_set(
            pError,
            XLLM_ERROR_INVALID_REQUEST,
            "memory profile does not match the selected memory scheme"
        );
        return XRT_NET_ERROR;
    }

    if ( pOptions->sMemoryProfileId && pOptions->sMemoryProfileId[0] &&
         !bProfileRecognized && eResolved != XLLM_MEMORY_SCHEME_CUSTOM ) {
        xllm__error_set(
            pError,
            XLLM_ERROR_INVALID_REQUEST,
            "unknown memory profile id for the selected memory scheme"
        );
        return XRT_NET_ERROR;
    }

    if ( eResolved == XLLM_MEMORY_SCHEME_BUILTIN_SPARSE && bHasEmbedder ) {
        xllm__error_set(
            pError,
            XLLM_ERROR_INVALID_REQUEST,
            "builtin_sparse does not accept an explicit embedder"
        );
        return XRT_NET_ERROR;
    }

    pSelection->eScheme = eResolved;
    pSelection->sResolvedProfileId =
        (pOptions->sMemoryProfileId && pOptions->sMemoryProfileId[0])
            ? pOptions->sMemoryProfileId
            : xllm__memory_default_profile_id(eResolved);
    pSelection->bAutoCreateBuiltinEmbedder =
        (eResolved == XLLM_MEMORY_SCHEME_ONNX_E5) && !bHasEmbedder;
    return XRT_NET_OK;
}

static const char *xllm__memory_scope_name(xllm_memory_scope eScope)
{
    switch ( eScope ) {
        case XLLM_MEMORY_SCOPE_MEMORY:
            return "memory";
        case XLLM_MEMORY_SCOPE_KNOWLEDGE:
            return "knowledge";
        default:
            return "any";
    }
}

static xllm_context_block_kind xllm__memory_scope_to_context_kind(xllm_memory_scope eScope)
{
    switch ( eScope ) {
        case XLLM_MEMORY_SCOPE_KNOWLEDGE:
            return XLLM_CONTEXT_KNOWLEDGE;
        case XLLM_MEMORY_SCOPE_MEMORY:
        default:
            return XLLM_CONTEXT_MEMORY;
    }
}

static const char *xllm__memory_role_name(xllm_role eRole)
{
    switch ( eRole ) {
        case XLLM_ROLE_SYSTEM:
            return "system";
        case XLLM_ROLE_USER:
            return "user";
        case XLLM_ROLE_ASSISTANT:
            return "assistant";
        case XLLM_ROLE_TOOL:
            return "tool";
        default:
            return "message";
    }
}

static const char *xllm__memory_context_kind_name(xllm_context_block_kind eKind)
{
    switch ( eKind ) {
        case XLLM_CONTEXT_SYSTEM:
            return "system";
        case XLLM_CONTEXT_SESSION_SUMMARY:
            return "session_summary";
        case XLLM_CONTEXT_HISTORY:
            return "history";
        case XLLM_CONTEXT_MEMORY:
            return "memory";
        case XLLM_CONTEXT_KNOWLEDGE:
            return "knowledge";
        case XLLM_CONTEXT_USER:
            return "user";
        case XLLM_CONTEXT_TOOL_RESULT:
            return "tool_result";
        default:
            return "context";
    }
}

static const char *xllm__memory_response_status_name(xllm_response_status eStatus)
{
    switch ( eStatus ) {
        case XLLM_STATUS_COMPLETED:
            return "completed";
        case XLLM_STATUS_INCOMPLETE:
            return "incomplete";
        case XLLM_STATUS_TOOL_CALL_REQUIRED:
            return "tool_call_required";
        case XLLM_STATUS_REFUSED:
            return "refused";
        case XLLM_STATUS_CONTENT_FILTERED:
            return "content_filtered";
        case XLLM_STATUS_CANCELLED:
            return "cancelled";
        case XLLM_STATUS_ERRORED:
            return "errored";
        default:
            return "unknown";
    }
}

static void xllm__memory_text_builder_init(xllm__memory_text_builder *pBuilder)
{
    if ( !pBuilder ) {
        return;
    }

    memset(pBuilder, 0, sizeof(*pBuilder));
}

static void xllm__memory_text_builder_reset(xllm__memory_text_builder *pBuilder)
{
    if ( !pBuilder ) {
        return;
    }

    if ( pBuilder->sText ) {
        xrtFree(pBuilder->sText);
    }
    memset(pBuilder, 0, sizeof(*pBuilder));
}

static int xllm__memory_text_builder_reserve(
    xllm__memory_text_builder *pBuilder,
    size_t iExtra
)
{
    size_t iRequired;
    size_t iCapacity;
    char *sNewText;

    if ( !pBuilder ) {
        return XRT_NET_ERROR;
    }

    iRequired = pBuilder->iLength + iExtra + 1u;
    if ( iRequired <= pBuilder->iCapacity ) {
        return XRT_NET_OK;
    }

    iCapacity = (pBuilder->iCapacity > 0u) ? pBuilder->iCapacity : 256u;
    while ( iCapacity < iRequired ) {
        if ( iCapacity > (SIZE_MAX / 2u) ) {
            iCapacity = iRequired;
            break;
        }
        iCapacity *= 2u;
    }

    sNewText = (char *)xrtRealloc(pBuilder->sText, iCapacity);
    if ( !sNewText ) {
        return XRT_NET_ERROR;
    }

    pBuilder->sText = sNewText;
    pBuilder->iCapacity = iCapacity;
    return XRT_NET_OK;
}

static int xllm__memory_text_builder_append_n(
    xllm__memory_text_builder *pBuilder,
    const char *sText,
    size_t iTextLength
)
{
    if ( !pBuilder || !sText || iTextLength == 0u ) {
        return XRT_NET_OK;
    }
    if ( xllm__memory_text_builder_reserve(pBuilder, iTextLength) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    memcpy(pBuilder->sText + pBuilder->iLength, sText, iTextLength);
    pBuilder->iLength += iTextLength;
    pBuilder->sText[pBuilder->iLength] = '\0';
    return XRT_NET_OK;
}

static int xllm__memory_text_builder_append_cstr(
    xllm__memory_text_builder *pBuilder,
    const char *sText
)
{
    return xllm__memory_text_builder_append_n(
        pBuilder,
        sText,
        sText ? strlen(sText) : 0u
    );
}

static int xllm__memory_text_builder_append_i32(
    xllm__memory_text_builder *pBuilder,
    int32 iValue
)
{
    char sBuffer[32];
    int iWritten = snprintf(sBuffer, sizeof(sBuffer), "%d", (int)iValue);
    if ( iWritten < 0 ) {
        return XRT_NET_ERROR;
    }
    return xllm__memory_text_builder_append_n(pBuilder, sBuffer, (size_t)iWritten);
}

static char *xllm__memory_text_builder_take(xllm__memory_text_builder *pBuilder)
{
    char *sText;

    if ( !pBuilder ) {
        return NULL;
    }

    sText = pBuilder->sText;
    pBuilder->sText = NULL;
    pBuilder->iLength = 0u;
    pBuilder->iCapacity = 0u;
    return sText;
}

static int xllm__memory_append_part_text(
    xllm__memory_text_builder *pBuilder,
    const xllm_content_part *pPart
)
{
    if ( !pBuilder || !pPart ) {
        return XRT_NET_ERROR;
    }

    switch ( pPart->eKind ) {
        case XLLM_PART_TEXT:
        case XLLM_PART_JSON:
            if ( pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT && pPart->as.tSource.as.sText ) {
                return xllm__memory_text_builder_append_cstr(pBuilder, pPart->as.tSource.as.sText);
            }
            return xllm__memory_text_builder_append_cstr(
                pBuilder,
                (pPart->eKind == XLLM_PART_JSON) ? "[json]" : "[text]"
            );
        case XLLM_PART_IMAGE:
            return xllm__memory_text_builder_append_cstr(pBuilder, "[image]");
        case XLLM_PART_FILE:
            return xllm__memory_text_builder_append_cstr(pBuilder, "[file]");
        case XLLM_PART_AUDIO:
            return xllm__memory_text_builder_append_cstr(pBuilder, "[audio]");
        case XLLM_PART_VIDEO:
            return xllm__memory_text_builder_append_cstr(pBuilder, "[video]");
        default:
            return xllm__memory_text_builder_append_cstr(pBuilder, "[part]");
    }
}

static int xllm__memory_append_message_line(
    xllm__memory_text_builder *pBuilder,
    const xllm_message *pMessage
)
{
    size_t i;
    bool bHasContent = false;

    if ( !pBuilder || !pMessage ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__memory_text_builder_append_cstr(pBuilder, xllm__memory_role_name(pMessage->eRole)) != XRT_NET_OK ||
         xllm__memory_text_builder_append_cstr(pBuilder, ": ") != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    if ( pMessage->eRole == XLLM_ROLE_TOOL ) {
        if ( pMessage->sToolName && pMessage->sToolName[0] ) {
            if ( xllm__memory_text_builder_append_cstr(pBuilder, "tool=") != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(pBuilder, pMessage->sToolName) != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(pBuilder, " ") != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
        }
        if ( pMessage->sToolCallId && pMessage->sToolCallId[0] ) {
            if ( xllm__memory_text_builder_append_cstr(pBuilder, "call_id=") != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(pBuilder, pMessage->sToolCallId) != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(pBuilder, " ") != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
        }
    }

    for ( i = 0u; i < pMessage->iToolCallCount; ++i ) {
        const xllm_tool_call *pToolCall = &pMessage->pToolCalls[i];

        if ( bHasContent && xllm__memory_text_builder_append_cstr(pBuilder, " | ") != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( xllm__memory_text_builder_append_cstr(pBuilder, "tool_call ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(
                 pBuilder,
                 (pToolCall->sToolName && pToolCall->sToolName[0]) ? pToolCall->sToolName : pToolCall->sToolId
             ) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( pToolCall->sArgumentsJson && pToolCall->sArgumentsJson[0] ) {
            if ( xllm__memory_text_builder_append_cstr(pBuilder, "(") != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(pBuilder, pToolCall->sArgumentsJson) != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(pBuilder, ")") != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
        }
        bHasContent = true;
    }

    for ( i = 0u; i < pMessage->iPartCount; ++i ) {
        if ( bHasContent && xllm__memory_text_builder_append_cstr(pBuilder, " | ") != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( xllm__memory_append_part_text(pBuilder, &pMessage->pParts[i]) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        bHasContent = true;
    }

    if ( !bHasContent && xllm__memory_text_builder_append_cstr(pBuilder, "(empty)") != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    return xllm__memory_text_builder_append_cstr(pBuilder, "\n");
}

static int xllm__memory_append_message_query_text(
    xllm__memory_text_builder *pBuilder,
    const xllm_message *pMessage
)
{
    size_t i;
    bool bHasContent = false;

    if ( !pBuilder || !pMessage ) {
        return XRT_NET_ERROR;
    }

    if ( pMessage->eRole == XLLM_ROLE_TOOL ) {
        if ( pMessage->sToolName && pMessage->sToolName[0] ) {
            if ( xllm__memory_text_builder_append_cstr(pBuilder, "tool=") != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(pBuilder, pMessage->sToolName) != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
            bHasContent = true;
        }
        if ( pMessage->sToolCallId && pMessage->sToolCallId[0] ) {
            if ( bHasContent && xllm__memory_text_builder_append_cstr(pBuilder, " ") != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
            if ( xllm__memory_text_builder_append_cstr(pBuilder, "call_id=") != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(pBuilder, pMessage->sToolCallId) != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
            bHasContent = true;
        }
    }

    for ( i = 0u; i < pMessage->iToolCallCount; ++i ) {
        const xllm_tool_call *pToolCall = &pMessage->pToolCalls[i];

        if ( bHasContent && xllm__memory_text_builder_append_cstr(pBuilder, " | ") != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( xllm__memory_text_builder_append_cstr(pBuilder, "tool_call ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(
                 pBuilder,
                 (pToolCall->sToolName && pToolCall->sToolName[0]) ? pToolCall->sToolName : pToolCall->sToolId
             ) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( pToolCall->sArgumentsJson && pToolCall->sArgumentsJson[0] ) {
            if ( xllm__memory_text_builder_append_cstr(pBuilder, "(") != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(pBuilder, pToolCall->sArgumentsJson) != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(pBuilder, ")") != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
        }
        bHasContent = true;
    }

    for ( i = 0u; i < pMessage->iPartCount; ++i ) {
        if ( bHasContent && xllm__memory_text_builder_append_cstr(pBuilder, " | ") != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( xllm__memory_append_part_text(pBuilder, &pMessage->pParts[i]) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        bHasContent = true;
    }

    if ( !bHasContent ) {
        return xllm__memory_text_builder_append_cstr(pBuilder, "(empty)");
    }

    return XRT_NET_OK;
}

static int xllm__memory_append_context_blocks(
    xllm__memory_text_builder *pBuilder,
    const xllm_turn *pTurn
)
{
    size_t i;

    if ( !pBuilder || !pTurn ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < pTurn->iContextBlockCount; ++i ) {
        const xllm_context_block *pBlock = &pTurn->pContextBlocks[i];
        size_t j;

        if ( xllm__memory_text_builder_append_cstr(pBuilder, "context[") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(pBuilder, xllm__memory_context_kind_name(pBlock->eKind)) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(pBuilder, "]") != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( pBlock->iPriority != 0 ) {
            if ( xllm__memory_text_builder_append_cstr(pBuilder, " priority=") != XRT_NET_OK ||
                 xllm__memory_text_builder_append_i32(pBuilder, pBlock->iPriority) != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
        }
        if ( pBlock->bPinned ) {
            if ( xllm__memory_text_builder_append_cstr(pBuilder, " pinned=true") != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
        }
        if ( xllm__memory_text_builder_append_cstr(pBuilder, "\n") != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }

        for ( j = 0u; j < pBlock->iMessageCount; ++j ) {
            if ( xllm__memory_append_message_line(pBuilder, &pBlock->pMessages[j]) != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
        }
    }

    return XRT_NET_OK;
}

static int xllm__memory_append_response_outputs(
    xllm__memory_text_builder *pBuilder,
    const xllm_response *pResponse,
    bool bIncludeThinking
)
{
    size_t i;
    bool bAppended = false;

    if ( !pBuilder || !pResponse ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < pResponse->iOutputCount; ++i ) {
        const xllm_output_item *pOutput = &pResponse->pOutputs[i];

        switch ( pOutput->eKind ) {
            case XLLM_OUTPUT_MESSAGE: {
                xllm_message tMessage;
                memset(&tMessage, 0, sizeof(tMessage));
                tMessage.eRole = XLLM_ROLE_ASSISTANT;
                tMessage.pParts = pOutput->as.tMessage.pParts;
                tMessage.iPartCount = pOutput->as.tMessage.iPartCount;
                if ( xllm__memory_append_message_line(pBuilder, &tMessage) != XRT_NET_OK ) {
                    return XRT_NET_ERROR;
                }
                bAppended = true;
                break;
            }
            case XLLM_OUTPUT_TOOL_CALL: {
                xllm_tool_call tToolCall;
                xllm_message tMessage;
                memset(&tToolCall, 0, sizeof(tToolCall));
                memset(&tMessage, 0, sizeof(tMessage));
                tToolCall.sCallId = pOutput->as.tToolCall.sCallId;
                tToolCall.sToolId = pOutput->as.tToolCall.sToolId;
                tToolCall.sToolName = pOutput->as.tToolCall.sToolName;
                tToolCall.sArgumentsJson = pOutput->as.tToolCall.sArgumentsJson;
                tMessage.eRole = XLLM_ROLE_ASSISTANT;
                tMessage.pToolCalls = &tToolCall;
                tMessage.iToolCallCount = 1u;
                if ( xllm__memory_append_message_line(pBuilder, &tMessage) != XRT_NET_OK ) {
                    return XRT_NET_ERROR;
                }
                bAppended = true;
                break;
            }
            case XLLM_OUTPUT_REFUSAL:
                if ( pOutput->as.tRefusal.sText && pOutput->as.tRefusal.sText[0] ) {
                    if ( xllm__memory_text_builder_append_cstr(pBuilder, "assistant: refusal ") != XRT_NET_OK ||
                         xllm__memory_text_builder_append_cstr(pBuilder, pOutput->as.tRefusal.sText) != XRT_NET_OK ||
                         xllm__memory_text_builder_append_cstr(pBuilder, "\n") != XRT_NET_OK ) {
                        return XRT_NET_ERROR;
                    }
                    bAppended = true;
                }
                break;
            case XLLM_OUTPUT_THINKING:
                if ( bIncludeThinking && pOutput->as.tThinking.sText && pOutput->as.tThinking.sText[0] ) {
                    if ( xllm__memory_text_builder_append_cstr(pBuilder, "assistant_thinking: ") != XRT_NET_OK ||
                         xllm__memory_text_builder_append_cstr(pBuilder, pOutput->as.tThinking.sText) != XRT_NET_OK ||
                         xllm__memory_text_builder_append_cstr(pBuilder, "\n") != XRT_NET_OK ) {
                        return XRT_NET_ERROR;
                    }
                    bAppended = true;
                }
                break;
            default:
                break;
        }
    }

    if ( !bAppended ) {
        const char *sText = xllm_response_get_text(pResponse);
        if ( sText && sText[0] ) {
            if ( xllm__memory_text_builder_append_cstr(pBuilder, "assistant: ") != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(pBuilder, sText) != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(pBuilder, "\n") != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
        }
    }

    return XRT_NET_OK;
}

static bool xllm__memory_table_set_owned_text(
    xvalue tTable,
    const char *sKey,
    const char *sValue
)
{
    str sCopy;
    xvalue tText;

    if ( !tTable || xvoType(tTable) != XVO_DT_TABLE || !sKey ) {
        return false;
    }

    if ( !sValue || !sValue[0] ) {
        return xvoTableSetText(tTable, (str)sKey, 0u, (str)"", 0u, FALSE);
    }

    sCopy = xrtCopyStr((str)sValue, 0u);
    if ( !sCopy ) {
        return false;
    }

    tText = xvoCreateText((ptr)sCopy, (uint32)strlen((const char *)sCopy), TRUE);
    if ( !tText ) {
        xrtFree(sCopy);
        return false;
    }

    if ( !xvoTableSetValue(tTable, (str)sKey, 0u, tText, TRUE) ) {
        xvoUnref(tText);
        return false;
    }

    return true;
}

typedef struct {
    xvalue tDestTable;
    bool bFailed;
} xllm__memory_shared_table_copy_state;

static bool xllm__memory_shared_table_copy_proc(
    Dict_Key *pKey,
    xvalue *ppVal,
    xllm__memory_shared_table_copy_state *pState
)
{
    xvalue tCopy;

    if ( !pKey || !ppVal || !pState || pState->bFailed ) {
        return FALSE;
    }

    tCopy = xvoDeepCopy(ppVal[0]);
    if ( !tCopy && ppVal[0] ) {
        tCopy = xvoCopy(ppVal[0]);
    }
    if ( ppVal[0] && !tCopy ) {
        pState->bFailed = true;
        return FALSE;
    }

    if ( !xvoTableSetValue(pState->tDestTable, pKey->Key, pKey->KeyLen, tCopy, TRUE) ) {
        if ( tCopy ) {
            xvoUnref(tCopy);
        }
        pState->bFailed = true;
    }

    return FALSE;
}

static xvalue xllm__memory_create_shared_table_copy(xvalue tSourceTable)
{
    xvalue tDestTable;
    xllm__memory_shared_table_copy_state tState;

    /* turn-response metadata can be created on worker threads and read later on the caller thread */
    tDestTable = xvoCreateTableEx(XRT_OBJMODE_SHARED);
    if ( !tDestTable ) {
        return 0;
    }

    if ( !tSourceTable || xvoType(tSourceTable) != XVO_DT_TABLE ) {
        return tDestTable;
    }

    memset(&tState, 0, sizeof(tState));
    tState.tDestTable = tDestTable;
    xrtDictWalk(tSourceTable->vTable, (ptr)xllm__memory_shared_table_copy_proc, &tState);
    if ( tState.bFailed ) {
        xvoUnref(tDestTable);
        return 0;
    }

    return tDestTable;
}

static xvalue xllm__memory_make_turn_response_metadata(
    xvalue tUserMetadata,
    const xllm_memory_ingest_turn_response_options *pOptions,
    int64 iCreatedAtUnix,
    int64 iUpdatedAtUnix
)
{
    xvalue tMetadata = 0;
    xllm_memory_extraction_policy eExtractionPolicy =
        xllm__memory_resolve_extraction_policy(
            pOptions ? pOptions->eExtractionPolicy : XLLM_MEMORY_EXTRACTION_POLICY_DEFAULT
        );
    uint32 uTurnMessageCount = 0u;
    uint32 uTurnContextBlockCount = 0u;
    uint32 uResponseOutputCount = 0u;
    uint32 uResponseToolCallCount = 0u;
    bool bHasSystemPrompt = false;
    const char *sConversationKind =
        (eExtractionPolicy == XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY) ? "summary" : "turn_response";
    const char *sMemoryType =
        (eExtractionPolicy == XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY)
            ? "conversation.summary.v1"
            : "conversation.turn_response.v1";

    if ( pOptions && pOptions->pTurn ) {
        uTurnMessageCount = (uint32)pOptions->pTurn->iMessageCount;
        uTurnContextBlockCount = (uint32)pOptions->pTurn->iContextBlockCount;
        bHasSystemPrompt = pOptions->pTurn->sSystemPrompt && pOptions->pTurn->sSystemPrompt[0];
    }
    if ( pOptions && pOptions->pResponse ) {
        uResponseOutputCount = (uint32)pOptions->pResponse->iOutputCount;
        uResponseToolCallCount = (uint32)xllm_response_get_tool_call_count(pOptions->pResponse);
    }

    tMetadata = xllm__memory_create_shared_table_copy(tUserMetadata);
    if ( !tMetadata ) {
        return 0;
    }

    if ( !xvoTableSetText(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_CONVERSATION_KIND, 0u, (str)sConversationKind, 0u, FALSE) ||
         !xvoTableSetText(
             tMetadata,
             (str)XLLM__MEMORY_METADATA_KEY_MEMORY_TYPE,
             0u,
             (str)sMemoryType,
             0u,
             FALSE
         ) ||
         !xvoTableSetText(
             tMetadata,
             (str)XLLM__MEMORY_METADATA_KEY_EXTRACTION_POLICY,
             0u,
             (str)xllm__memory_extraction_policy_name(eExtractionPolicy),
             0u,
             FALSE
         ) ||
         !xllm__memory_table_set_owned_text(
             tMetadata,
             XLLM__MEMORY_METADATA_KEY_CONVERSATION_ID,
             (pOptions && pOptions->sConversationId) ? pOptions->sConversationId : ""
         ) ||
         !xllm__memory_table_set_owned_text(
             tMetadata,
             XLLM__MEMORY_METADATA_KEY_TURN_ID,
             (pOptions && pOptions->sTurnId) ? pOptions->sTurnId : ""
         ) ||
         !xvoTableSetInt(
             tMetadata,
             (str)XLLM__MEMORY_METADATA_KEY_STABLE_IDENTITY,
             0u,
             (pOptions && pOptions->bUseStableIdentity) ? 1 : 0
         ) ||
         ((iCreatedAtUnix > 0) &&
          !xvoTableSetInt(
              tMetadata,
              (str)XLLM__MEMORY_METADATA_KEY_CREATED_AT_UNIX,
              0u,
              iCreatedAtUnix
          )) ||
         ((iUpdatedAtUnix > 0) &&
          !xvoTableSetInt(
              tMetadata,
              (str)XLLM__MEMORY_METADATA_KEY_UPDATED_AT_UNIX,
              0u,
              iUpdatedAtUnix
          )) ||
         ((pOptions && pOptions->iPriority != 0) &&
          !xvoTableSetInt(
              tMetadata,
              (str)XLLM__MEMORY_METADATA_KEY_PRIORITY,
              0u,
              (int64)pOptions->iPriority
          )) ||
         ((pOptions && pOptions->iExpiresAtUnix > 0) &&
          !xvoTableSetInt(
              tMetadata,
              (str)XLLM__MEMORY_METADATA_KEY_EXPIRES_AT_UNIX,
              0u,
              pOptions->iExpiresAtUnix
          )) ||
         !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_TURN_MESSAGE_COUNT, 0u, (int64)uTurnMessageCount) ||
         !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_TURN_CONTEXT_BLOCK_COUNT, 0u, (int64)uTurnContextBlockCount) ||
         !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_HAS_SYSTEM_PROMPT, 0u, bHasSystemPrompt ? 1 : 0) ||
         !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_RESPONSE_OUTPUT_COUNT, 0u, (int64)uResponseOutputCount) ||
         !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_RESPONSE_TOOL_CALL_COUNT, 0u, (int64)uResponseToolCallCount) ||
         !xvoTableSetText(
             tMetadata,
             (str)XLLM__MEMORY_METADATA_KEY_RESPONSE_STATUS,
             0u,
             (str)((pOptions && pOptions->pResponse) ? xllm__memory_response_status_name(pOptions->pResponse->eStatus) : "none"),
             0u,
             FALSE
         ) ||
         !xvoTableSetInt(
             tMetadata,
             (str)XLLM__MEMORY_METADATA_KEY_INCLUDE_CONTEXT_BLOCKS,
             0u,
             (pOptions && pOptions->bIncludeContextBlocks) ? 1 : 0
         ) ||
         !xvoTableSetInt(
             tMetadata,
             (str)XLLM__MEMORY_METADATA_KEY_INCLUDE_THINKING,
             0u,
             (pOptions && pOptions->bIncludeThinking) ? 1 : 0
         ) ) {
        xvoUnref(tMetadata);
        return 0;
    }

    return tMetadata;
}

static char *xllm__memory_make_turn_response_source_uri_from_suffix(const char *sSuffix)
{
    size_t iPrefixLength;
    size_t iSuffixLength;
    char *sSourceUri;

    if ( !sSuffix || !sSuffix[0] ) {
        return NULL;
    }

    iPrefixLength = strlen(XLLM__MEMORY_CONVERSATION_SOURCE_URI_PREFIX);
    iSuffixLength = strlen(sSuffix);
    sSourceUri = (char *)xrtCalloc(iPrefixLength + iSuffixLength + 1u, sizeof(char));
    if ( !sSourceUri ) {
        return NULL;
    }

    memcpy(sSourceUri, XLLM__MEMORY_CONVERSATION_SOURCE_URI_PREFIX, iPrefixLength);
    memcpy(sSourceUri + iPrefixLength, sSuffix, iSuffixLength);
    return sSourceUri;
}

static char *xllm__memory_make_turn_response_stable_source_suffix(
    const xllm_memory_ingest_turn_response_options *pOptions
)
{
    size_t iConversationLength;
    size_t iTurnLength;
    char *sSuffix;

    if ( !pOptions ) {
        return NULL;
    }
    if ( !pOptions->bUseStableIdentity ) {
        return NULL;
    }
    if ( !pOptions->sConversationId || !pOptions->sConversationId[0] ) {
        return NULL;
    }

    iConversationLength = strlen(pOptions->sConversationId);
    iTurnLength = (pOptions->sTurnId && pOptions->sTurnId[0]) ? strlen(pOptions->sTurnId) : 0u;
    sSuffix = (char *)xrtCalloc(iConversationLength + (iTurnLength > 0u ? (1u + iTurnLength) : 0u) + 1u, sizeof(char));
    if ( !sSuffix ) {
        return NULL;
    }

    memcpy(sSuffix, pOptions->sConversationId, iConversationLength);
    if ( iTurnLength > 0u ) {
        sSuffix[iConversationLength] = '/';
        memcpy(sSuffix + iConversationLength + 1u, pOptions->sTurnId, iTurnLength);
    }
    return sSuffix;
}

static char *xllm__memory_make_turn_response_source_uri(
    const xllm_memory_ingest_turn_response_options *pOptions,
    const char *sRecordId
)
{
    char *sStableSuffix = NULL;
    char *sSourceUri = NULL;

    if ( pOptions && pOptions->sSourceUri && pOptions->sSourceUri[0] ) {
        return xllm__dup_cstr(pOptions->sSourceUri);
    }

    sStableSuffix = xllm__memory_make_turn_response_stable_source_suffix(pOptions);
    if ( sStableSuffix ) {
        sSourceUri = xllm__memory_make_turn_response_source_uri_from_suffix(sStableSuffix);
        xllm__free_cstr(&sStableSuffix);
        return sSourceUri;
    }

    if ( !sRecordId || !sRecordId[0] ) {
        return NULL;
    }
    return xllm__memory_make_turn_response_source_uri_from_suffix(sRecordId);
}

static char *xllm__memory_make_turn_response_record_id(
    const xllm_memory_ingest_turn_response_options *pOptions,
    const char *sSourceUri
)
{
    char sBuffer[96];
    uint64 uNowMs;
    uint64 uRand;

    if ( pOptions && pOptions->sRecordId && pOptions->sRecordId[0] ) {
        return xllm__dup_cstr(pOptions->sRecordId);
    }

    if ( pOptions && pOptions->bUseStableIdentity && sSourceUri && sSourceUri[0] ) {
        uint64 uHash = xllm__memory_hash_namespace(sSourceUri);
        snprintf(
            sBuffer,
            sizeof(sBuffer),
            "conversation:%08x%08x",
            (unsigned)(uHash >> 32u),
            (unsigned)(uHash & 0xffffffffu)
        );
        return xllm__dup_cstr(sBuffer);
    }

    uNowMs = xllm__memory_now_ms();
    uRand = xrtRand64();
    snprintf(
        sBuffer,
        sizeof(sBuffer),
        "conversation_%llu_%08x%08x",
        (unsigned long long)uNowMs,
        (unsigned)(uRand >> 32),
        (unsigned)uRand
    );
    return xllm__dup_cstr(sBuffer);
}

static char *xllm__memory_make_prefixed_source_uri(const char *sPrefix, const char *sSuffix)
{
    size_t iPrefixLength;
    size_t iSuffixLength;
    char *sSourceUri;

    if ( !sPrefix || !sPrefix[0] || !sSuffix || !sSuffix[0] ) {
        return NULL;
    }

    iPrefixLength = strlen(sPrefix);
    iSuffixLength = strlen(sSuffix);
    sSourceUri = (char *)xrtCalloc(iPrefixLength + iSuffixLength + 1u, sizeof(char));
    if ( !sSourceUri ) {
        return NULL;
    }
    memcpy(sSourceUri, sPrefix, iPrefixLength);
    memcpy(sSourceUri + iPrefixLength, sSuffix, iSuffixLength);
    return sSourceUri;
}

static char *xllm__memory_make_task_record_id(const xllm_memory_ingest_task_options *pOptions)
{
    char sBuffer[96];
    uint64 uNowMs;
    uint64 uRand;

    if ( pOptions && pOptions->sRecordId && pOptions->sRecordId[0] ) {
        return xllm__dup_cstr(pOptions->sRecordId);
    }
    if ( pOptions && pOptions->sTaskId && pOptions->sTaskId[0] ) {
        size_t iPrefixLength = strlen("task:");
        size_t iTaskLength = strlen(pOptions->sTaskId);
        char *sRecordId = (char *)xrtCalloc(iPrefixLength + iTaskLength + 1u, sizeof(char));
        if ( !sRecordId ) {
            return NULL;
        }
        memcpy(sRecordId, "task:", iPrefixLength);
        memcpy(sRecordId + iPrefixLength, pOptions->sTaskId, iTaskLength);
        return sRecordId;
    }

    uNowMs = xllm__memory_now_ms();
    uRand = xrtRand64();
    snprintf(
        sBuffer,
        sizeof(sBuffer),
        "task_%llu_%08x%08x",
        (unsigned long long)uNowMs,
        (unsigned)(uRand >> 32),
        (unsigned)uRand
    );
    return xllm__dup_cstr(sBuffer);
}

static char *xllm__memory_make_task_source_uri(
    const xllm_memory_ingest_task_options *pOptions,
    const char *sRecordId
)
{
    if ( pOptions && pOptions->sSourceUri && pOptions->sSourceUri[0] ) {
        return xllm__dup_cstr(pOptions->sSourceUri);
    }
    if ( pOptions && pOptions->sTaskId && pOptions->sTaskId[0] ) {
        return xllm__memory_make_prefixed_source_uri(XLLM__MEMORY_TASK_SOURCE_URI_PREFIX, pOptions->sTaskId);
    }
    return xllm__memory_make_prefixed_source_uri(XLLM__MEMORY_TASK_SOURCE_URI_PREFIX, sRecordId);
}

static char *xllm__memory_make_fact_record_id(const xllm_memory_ingest_fact_options *pOptions)
{
    char sBuffer[96];
    uint64 uNowMs;
    uint64 uRand;

    if ( pOptions && pOptions->sRecordId && pOptions->sRecordId[0] ) {
        return xllm__dup_cstr(pOptions->sRecordId);
    }
    if ( pOptions && pOptions->sFactId && pOptions->sFactId[0] ) {
        size_t iPrefixLength = strlen("fact:");
        size_t iIdLength = strlen(pOptions->sFactId);
        char *sRecordId = (char *)xrtCalloc(iPrefixLength + iIdLength + 1u, sizeof(char));
        if ( !sRecordId ) {
            return NULL;
        }
        memcpy(sRecordId, "fact:", iPrefixLength);
        memcpy(sRecordId + iPrefixLength, pOptions->sFactId, iIdLength);
        return sRecordId;
    }

    uNowMs = xllm__memory_now_ms();
    uRand = xrtRand64();
    snprintf(
        sBuffer,
        sizeof(sBuffer),
        "fact_%llu_%08x%08x",
        (unsigned long long)uNowMs,
        (unsigned)(uRand >> 32),
        (unsigned)uRand
    );
    return xllm__dup_cstr(sBuffer);
}

static char *xllm__memory_make_preference_record_id(const xllm_memory_ingest_preference_options *pOptions)
{
    char sBuffer[96];
    uint64 uNowMs;
    uint64 uRand;

    if ( pOptions && pOptions->sRecordId && pOptions->sRecordId[0] ) {
        return xllm__dup_cstr(pOptions->sRecordId);
    }
    if ( pOptions && pOptions->sPreferenceId && pOptions->sPreferenceId[0] ) {
        size_t iPrefixLength = strlen("preference:");
        size_t iIdLength = strlen(pOptions->sPreferenceId);
        char *sRecordId = (char *)xrtCalloc(iPrefixLength + iIdLength + 1u, sizeof(char));
        if ( !sRecordId ) {
            return NULL;
        }
        memcpy(sRecordId, "preference:", iPrefixLength);
        memcpy(sRecordId + iPrefixLength, pOptions->sPreferenceId, iIdLength);
        return sRecordId;
    }

    uNowMs = xllm__memory_now_ms();
    uRand = xrtRand64();
    snprintf(
        sBuffer,
        sizeof(sBuffer),
        "preference_%llu_%08x%08x",
        (unsigned long long)uNowMs,
        (unsigned)(uRand >> 32),
        (unsigned)uRand
    );
    return xllm__dup_cstr(sBuffer);
}

static char *xllm__memory_make_fact_source_uri(
    const xllm_memory_ingest_fact_options *pOptions,
    const char *sRecordId
)
{
    if ( pOptions && pOptions->sSourceUri && pOptions->sSourceUri[0] ) {
        return xllm__dup_cstr(pOptions->sSourceUri);
    }
    if ( pOptions && pOptions->sFactId && pOptions->sFactId[0] ) {
        return xllm__memory_make_prefixed_source_uri(XLLM__MEMORY_FACT_SOURCE_URI_PREFIX, pOptions->sFactId);
    }
    return xllm__memory_make_prefixed_source_uri(XLLM__MEMORY_FACT_SOURCE_URI_PREFIX, sRecordId);
}

static char *xllm__memory_make_preference_source_uri(
    const xllm_memory_ingest_preference_options *pOptions,
    const char *sRecordId
)
{
    if ( pOptions && pOptions->sSourceUri && pOptions->sSourceUri[0] ) {
        return xllm__dup_cstr(pOptions->sSourceUri);
    }
    if ( pOptions && pOptions->sPreferenceId && pOptions->sPreferenceId[0] ) {
        return xllm__memory_make_prefixed_source_uri(XLLM__MEMORY_PREFERENCE_SOURCE_URI_PREFIX, pOptions->sPreferenceId);
    }
    return xllm__memory_make_prefixed_source_uri(XLLM__MEMORY_PREFERENCE_SOURCE_URI_PREFIX, sRecordId);
}

static int64 xllm__memory_lookup_record_metadata_int(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sRecordId,
    const char *sMetadataKey
)
{
    int64 iValue = 0;
    size_t i;

    if ( !pMemory || !sRecordId || !sRecordId[0] || !sMetadataKey || !sMetadataKey[0] ) {
        return 0;
    }

    xrtMutexLock(pMemory->pMutex);
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        if ( pMemory->pRecords[i].eScope == eScope &&
             pMemory->pRecords[i].sRecordId &&
             strcmp(pMemory->pRecords[i].sRecordId, sRecordId) == 0 ) {
            if ( pMemory->pRecords[i].tMetadata && xvoType(pMemory->pRecords[i].tMetadata) == XVO_DT_TABLE ) {
                iValue = xvoTableGetInt(pMemory->pRecords[i].tMetadata, (str)sMetadataKey, 0u);
            }
            break;
        }
    }
    xrtMutexUnlock(pMemory->pMutex);
    return iValue;
}

static char *xllm__memory_build_turn_response_transcript(
    const xllm_memory_ingest_turn_response_options *pOptions,
    xllm_error *pError
)
{
    xllm__memory_text_builder tBuilder;
    xllm_memory_extraction_policy eExtractionPolicy =
        xllm__memory_resolve_extraction_policy(
            pOptions ? pOptions->eExtractionPolicy : XLLM_MEMORY_EXTRACTION_POLICY_DEFAULT
        );

    if ( !pOptions ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "turn_response ingest options are required");
        return NULL;
    }

    xllm__memory_text_builder_init(&tBuilder);

    if ( eExtractionPolicy == XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY ) {
        const char *sSummaryText = (pOptions->sSummaryText && pOptions->sSummaryText[0])
            ? pOptions->sSummaryText
            : (pOptions->pResponse ? xllm_response_get_text(pOptions->pResponse) : NULL);

        if ( !sSummaryText || !sSummaryText[0] ) {
            xllm__error_set(
                pError,
                XLLM_ERROR_INVALID_REQUEST,
                "summary ingest requires summary text or response visible text"
            );
            goto fail;
        }
        if ( xllm__memory_text_builder_append_cstr(&tBuilder, "summary: ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, sSummaryText) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
            goto fail;
        }
        return xllm__memory_text_builder_take(&tBuilder);
    }

    if ( !pOptions->pTurn && !pOptions->pResponse ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "turn_response ingest requires a turn, a response, or both");
        goto fail;
    }

    if ( pOptions->pTurn ) {
        size_t i;

        if ( pOptions->bIncludeSystemPrompt &&
             pOptions->pTurn->sSystemPrompt &&
             pOptions->pTurn->sSystemPrompt[0] ) {
            if ( xllm__memory_text_builder_append_cstr(&tBuilder, "system: ") != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(&tBuilder, pOptions->pTurn->sSystemPrompt) != XRT_NET_OK ||
                 xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
                goto fail;
            }
        }

        if ( pOptions->bIncludeContextBlocks &&
             pOptions->pTurn->pContextBlocks &&
             pOptions->pTurn->iContextBlockCount > 0u &&
             xllm__memory_append_context_blocks(&tBuilder, pOptions->pTurn) != XRT_NET_OK ) {
            goto fail;
        }

        for ( i = 0u; i < pOptions->pTurn->iMessageCount; ++i ) {
            if ( xllm__memory_append_message_line(&tBuilder, &pOptions->pTurn->pMessages[i]) != XRT_NET_OK ) {
                goto fail;
            }
        }
    }

    if ( pOptions->pResponse &&
         xllm__memory_append_response_outputs(&tBuilder, pOptions->pResponse, pOptions->bIncludeThinking) != XRT_NET_OK ) {
        goto fail;
    }

    if ( tBuilder.iLength == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "turn_response ingest produced empty transcript");
        goto fail;
    }

    return xllm__memory_text_builder_take(&tBuilder);

fail:
    if ( !pError || !pError->sMessage ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build turn_response transcript");
    }
    xllm__memory_text_builder_reset(&tBuilder);
    return NULL;
}

static xvalue xllm__memory_make_task_metadata(
    xvalue tUserMetadata,
    const xllm_memory_ingest_task_options *pOptions,
    int64 iCreatedAtUnix,
    int64 iUpdatedAtUnix
)
{
    xvalue tMetadata = 0;

    tMetadata = xllm__memory_create_shared_table_copy(tUserMetadata);
    if ( !tMetadata ) {
        return 0;
    }

    if ( !xvoTableSetText(
             tMetadata,
             (str)XLLM__MEMORY_METADATA_KEY_MEMORY_TYPE,
             0u,
             (str)"task.v1",
             0u,
             FALSE
         ) ||
         !xvoTableSetText(
             tMetadata,
             (str)XLLM__MEMORY_METADATA_KEY_EXTRACTION_POLICY,
             0u,
             (str)xllm__memory_extraction_policy_name(XLLM_MEMORY_EXTRACTION_POLICY_TASK),
             0u,
             FALSE
         ) ||
         !xllm__memory_table_set_owned_text(
             tMetadata,
             XLLM__MEMORY_METADATA_KEY_TASK_ID,
             (pOptions && pOptions->sTaskId) ? pOptions->sTaskId : ""
         ) ||
         !xvoTableSetText(
             tMetadata,
             (str)XLLM__MEMORY_METADATA_KEY_TASK_STATUS,
             0u,
             (str)xllm__memory_task_status_name(pOptions ? pOptions->eStatus : XLLM_MEMORY_TASK_STATUS_OPEN),
             0u,
             FALSE
         ) ||
         !xllm__memory_table_set_owned_text(
             tMetadata,
             XLLM__MEMORY_METADATA_KEY_TASK_OWNER,
             (pOptions && pOptions->sOwner) ? pOptions->sOwner : ""
         ) ||
         !xllm__memory_table_set_owned_text(
             tMetadata,
             XLLM__MEMORY_METADATA_KEY_SOURCE_CONVERSATION_ID,
             (pOptions && pOptions->sSourceConversationId) ? pOptions->sSourceConversationId : ""
         ) ||
         !xllm__memory_table_set_owned_text(
             tMetadata,
             XLLM__MEMORY_METADATA_KEY_SOURCE_TURN_ID,
             (pOptions && pOptions->sSourceTurnId) ? pOptions->sSourceTurnId : ""
         ) ||
         ((pOptions && pOptions->iDeadlineUnix > 0) &&
          !xvoTableSetInt(
              tMetadata,
              (str)XLLM__MEMORY_METADATA_KEY_TASK_DEADLINE_UNIX,
              0u,
              pOptions->iDeadlineUnix
          )) ||
         ((iCreatedAtUnix > 0) &&
          !xvoTableSetInt(
              tMetadata,
              (str)XLLM__MEMORY_METADATA_KEY_CREATED_AT_UNIX,
              0u,
              iCreatedAtUnix
          )) ||
         ((iUpdatedAtUnix > 0) &&
          !xvoTableSetInt(
              tMetadata,
              (str)XLLM__MEMORY_METADATA_KEY_UPDATED_AT_UNIX,
              0u,
              iUpdatedAtUnix
          )) ||
         ((pOptions && pOptions->iPriority != 0) &&
          !xvoTableSetInt(
              tMetadata,
              (str)XLLM__MEMORY_METADATA_KEY_PRIORITY,
              0u,
              (int64)pOptions->iPriority
          )) ||
         ((pOptions && pOptions->iExpiresAtUnix > 0) &&
          !xvoTableSetInt(
              tMetadata,
              (str)XLLM__MEMORY_METADATA_KEY_EXPIRES_AT_UNIX,
              0u,
              pOptions->iExpiresAtUnix
          )) ) {
        xvoUnref(tMetadata);
        return 0;
    }

    return tMetadata;
}

static char *xllm__memory_build_task_text(
    const xllm_memory_ingest_task_options *pOptions,
    xllm_error *pError
)
{
    xllm__memory_text_builder tBuilder;
    const char *sStatus = xllm__memory_task_status_name(pOptions ? pOptions->eStatus : XLLM_MEMORY_TASK_STATUS_OPEN);

    if ( !pOptions ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "task ingest options are required");
        return NULL;
    }
    if ( (!pOptions->sTitle || !pOptions->sTitle[0]) &&
         (!pOptions->sText || !pOptions->sText[0]) ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "task ingest requires title or text");
        return NULL;
    }

    xllm__memory_text_builder_init(&tBuilder);
    if ( pOptions->sTitle && pOptions->sTitle[0] ) {
        if ( xllm__memory_text_builder_append_cstr(&tBuilder, "task: ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, pOptions->sTitle) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
            goto oom;
        }
    }
    if ( xllm__memory_text_builder_append_cstr(&tBuilder, "status: ") != XRT_NET_OK ||
         xllm__memory_text_builder_append_cstr(&tBuilder, sStatus) != XRT_NET_OK ||
         xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
        goto oom;
    }
    if ( pOptions->sOwner && pOptions->sOwner[0] ) {
        if ( xllm__memory_text_builder_append_cstr(&tBuilder, "owner: ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, pOptions->sOwner) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
            goto oom;
        }
    }
    if ( pOptions->sText && pOptions->sText[0] ) {
        if ( xllm__memory_text_builder_append_cstr(&tBuilder, "details: ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, pOptions->sText) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
            goto oom;
        }
    }
    return xllm__memory_text_builder_take(&tBuilder);

oom:
    xllm__memory_text_builder_reset(&tBuilder);
    xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build task memory text");
    return NULL;
}

static xvalue xllm__memory_make_fact_metadata(
    xvalue tUserMetadata,
    const xllm_memory_ingest_fact_options *pOptions,
    int64 iCreatedAtUnix,
    int64 iUpdatedAtUnix
)
{
    xvalue tMetadata = xllm__memory_create_shared_table_copy(tUserMetadata);
    if ( !tMetadata ) {
        return 0;
    }

    if ( !xvoTableSetText(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_MEMORY_TYPE, 0u, (str)"fact.v1", 0u, FALSE) ||
         !xvoTableSetText(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_EXTRACTION_POLICY, 0u, (str)xllm__memory_extraction_policy_name(XLLM_MEMORY_EXTRACTION_POLICY_FACT), 0u, FALSE) ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_FACT_ID, (pOptions && pOptions->sFactId) ? pOptions->sFactId : "") ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_FACT_SUBJECT, (pOptions && pOptions->sSubject) ? pOptions->sSubject : "") ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_FACT_PREDICATE, (pOptions && pOptions->sPredicate) ? pOptions->sPredicate : "") ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_FACT_OBJECT, (pOptions && pOptions->sObject) ? pOptions->sObject : "") ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_SOURCE_CONVERSATION_ID, (pOptions && pOptions->sSourceConversationId) ? pOptions->sSourceConversationId : "") ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_SOURCE_TURN_ID, (pOptions && pOptions->sSourceTurnId) ? pOptions->sSourceTurnId : "") ||
         ((iCreatedAtUnix > 0) && !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_CREATED_AT_UNIX, 0u, iCreatedAtUnix)) ||
         ((iUpdatedAtUnix > 0) && !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_UPDATED_AT_UNIX, 0u, iUpdatedAtUnix)) ||
         ((pOptions && pOptions->iPriority != 0) && !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_PRIORITY, 0u, (int64)pOptions->iPriority)) ||
         ((pOptions && pOptions->iExpiresAtUnix > 0) && !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_EXPIRES_AT_UNIX, 0u, pOptions->iExpiresAtUnix)) ) {
        xvoUnref(tMetadata);
        return 0;
    }

    return tMetadata;
}

static xvalue xllm__memory_make_preference_metadata(
    xvalue tUserMetadata,
    const xllm_memory_ingest_preference_options *pOptions,
    int64 iCreatedAtUnix,
    int64 iUpdatedAtUnix
)
{
    xvalue tMetadata = xllm__memory_create_shared_table_copy(tUserMetadata);
    if ( !tMetadata ) {
        return 0;
    }

    if ( !xvoTableSetText(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_MEMORY_TYPE, 0u, (str)"preference.v1", 0u, FALSE) ||
         !xvoTableSetText(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_EXTRACTION_POLICY, 0u, (str)xllm__memory_extraction_policy_name(XLLM_MEMORY_EXTRACTION_POLICY_PREFERENCE), 0u, FALSE) ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_PREFERENCE_ID, (pOptions && pOptions->sPreferenceId) ? pOptions->sPreferenceId : "") ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_PREFERENCE_SUBJECT, (pOptions && pOptions->sSubject) ? pOptions->sSubject : "") ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_PREFERENCE_KEY, (pOptions && pOptions->sKey) ? pOptions->sKey : "") ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_PREFERENCE_VALUE, (pOptions && pOptions->sValue) ? pOptions->sValue : "") ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_SOURCE_CONVERSATION_ID, (pOptions && pOptions->sSourceConversationId) ? pOptions->sSourceConversationId : "") ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_SOURCE_TURN_ID, (pOptions && pOptions->sSourceTurnId) ? pOptions->sSourceTurnId : "") ||
         ((iCreatedAtUnix > 0) && !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_CREATED_AT_UNIX, 0u, iCreatedAtUnix)) ||
         ((iUpdatedAtUnix > 0) && !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_UPDATED_AT_UNIX, 0u, iUpdatedAtUnix)) ||
         ((pOptions && pOptions->iPriority != 0) && !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_PRIORITY, 0u, (int64)pOptions->iPriority)) ||
         ((pOptions && pOptions->iExpiresAtUnix > 0) && !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_EXPIRES_AT_UNIX, 0u, pOptions->iExpiresAtUnix)) ) {
        xvoUnref(tMetadata);
        return 0;
    }

    return tMetadata;
}

static char *xllm__memory_build_fact_text(
    const xllm_memory_ingest_fact_options *pOptions,
    xllm_error *pError
)
{
    xllm__memory_text_builder tBuilder;

    if ( !pOptions ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "fact ingest options are required");
        return NULL;
    }
    if ( (!pOptions->sSubject || !pOptions->sSubject[0]) &&
         (!pOptions->sPredicate || !pOptions->sPredicate[0]) &&
         (!pOptions->sObject || !pOptions->sObject[0]) &&
         (!pOptions->sText || !pOptions->sText[0]) ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "fact ingest requires subject/predicate/object or text");
        return NULL;
    }

    xllm__memory_text_builder_init(&tBuilder);
    if ( pOptions->sSubject && pOptions->sSubject[0] ) {
        if ( xllm__memory_text_builder_append_cstr(&tBuilder, "subject: ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, pOptions->sSubject) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
            goto oom;
        }
    }
    if ( pOptions->sPredicate && pOptions->sPredicate[0] ) {
        if ( xllm__memory_text_builder_append_cstr(&tBuilder, "predicate: ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, pOptions->sPredicate) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
            goto oom;
        }
    }
    if ( pOptions->sObject && pOptions->sObject[0] ) {
        if ( xllm__memory_text_builder_append_cstr(&tBuilder, "object: ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, pOptions->sObject) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
            goto oom;
        }
    }
    if ( pOptions->sText && pOptions->sText[0] ) {
        if ( xllm__memory_text_builder_append_cstr(&tBuilder, "details: ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, pOptions->sText) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
            goto oom;
        }
    }
    return xllm__memory_text_builder_take(&tBuilder);

oom:
    xllm__memory_text_builder_reset(&tBuilder);
    xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build fact memory text");
    return NULL;
}

static char *xllm__memory_build_preference_text(
    const xllm_memory_ingest_preference_options *pOptions,
    xllm_error *pError
)
{
    xllm__memory_text_builder tBuilder;

    if ( !pOptions ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "preference ingest options are required");
        return NULL;
    }
    if ( (!pOptions->sKey || !pOptions->sKey[0]) &&
         (!pOptions->sValue || !pOptions->sValue[0]) &&
         (!pOptions->sText || !pOptions->sText[0]) ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "preference ingest requires key/value or text");
        return NULL;
    }

    xllm__memory_text_builder_init(&tBuilder);
    if ( pOptions->sSubject && pOptions->sSubject[0] ) {
        if ( xllm__memory_text_builder_append_cstr(&tBuilder, "subject: ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, pOptions->sSubject) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
            goto oom;
        }
    }
    if ( pOptions->sKey && pOptions->sKey[0] ) {
        if ( xllm__memory_text_builder_append_cstr(&tBuilder, "preference: ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, pOptions->sKey) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
            goto oom;
        }
    }
    if ( pOptions->sValue && pOptions->sValue[0] ) {
        if ( xllm__memory_text_builder_append_cstr(&tBuilder, "value: ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, pOptions->sValue) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
            goto oom;
        }
    }
    if ( pOptions->sText && pOptions->sText[0] ) {
        if ( xllm__memory_text_builder_append_cstr(&tBuilder, "details: ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, pOptions->sText) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
            goto oom;
        }
    }
    return xllm__memory_text_builder_take(&tBuilder);

oom:
    xllm__memory_text_builder_reset(&tBuilder);
    xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build preference memory text");
    return NULL;
}

static int xllm__memory_dup_query_text(
    const char *sText,
    size_t iTextLength,
    uint32 uMaxQueryChars,
    char **psQuery,
    xllm_error *pError
)
{
    size_t iStart = 0u;
    size_t iEnd = iTextLength;
    char *sQuery;

    if ( !sText || !psQuery ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory turn search/apply requires query text");
        return XRT_NET_ERROR;
    }

    *psQuery = NULL;
    if ( uMaxQueryChars > 0u && iTextLength > (size_t)uMaxQueryChars ) {
        iStart = iTextLength - (size_t)uMaxQueryChars;
    }
    while ( iStart < iEnd && isspace((unsigned char)sText[iStart]) ) {
        ++iStart;
    }
    while ( iEnd > iStart && isspace((unsigned char)sText[iEnd - 1u]) ) {
        --iEnd;
    }
    if ( iEnd <= iStart ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory turn search/apply produced empty query");
        return XRT_NET_ERROR;
    }

    sQuery = (char *)xrtCalloc((iEnd - iStart) + 1u, sizeof(char));
    if ( !sQuery ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate memory turn search query");
        return XRT_NET_ERROR;
    }
    memcpy(sQuery, sText + iStart, iEnd - iStart);
    sQuery[iEnd - iStart] = '\0';
    *psQuery = sQuery;
    return XRT_NET_OK;
}

static int xllm__memory_build_search_query_from_turn(
    const xllm_turn *pTurn,
    const xllm_memory_turn_search_apply_options *pOptions,
    char **psQuery,
    xllm_error *pError
)
{
    xllm_memory_turn_search_apply_options tDefaultOptions;
    const xllm_memory_turn_search_apply_options *pUseOptions = pOptions;
    xllm__memory_text_builder tBuilder;
    bool bAppended = false;
    size_t i;
    int iStatus = XRT_NET_ERROR;

    if ( !pTurn || !psQuery ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory turn search/apply requires source turn");
        return XRT_NET_ERROR;
    }

    *psQuery = NULL;
    if ( !pUseOptions ) {
        xllm_memory_turn_search_apply_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    if ( pUseOptions->tSearchOptions.sQuery && pUseOptions->tSearchOptions.sQuery[0] ) {
        return xllm__memory_dup_query_text(
            pUseOptions->tSearchOptions.sQuery,
            strlen(pUseOptions->tSearchOptions.sQuery),
            pUseOptions->uMaxQueryChars,
            psQuery,
            pError
        );
    }

    xllm__memory_text_builder_init(&tBuilder);

    if ( pUseOptions->bIncludeSystemPrompt &&
         pTurn->sSystemPrompt &&
         pTurn->sSystemPrompt[0] ) {
        if ( xllm__memory_text_builder_append_cstr(&tBuilder, "system: ") != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, pTurn->sSystemPrompt) != XRT_NET_OK ||
             xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
            goto oom;
        }
        bAppended = true;
    }

    if ( pUseOptions->bIncludeContextBlocks && pTurn->iContextBlockCount > 0u ) {
        if ( xllm__memory_append_context_blocks(&tBuilder, pTurn) != XRT_NET_OK ) {
            goto oom;
        }
        bAppended = true;
    }

    switch ( pUseOptions->eQueryMode ) {
        case XLLM_MEMORY_TURN_QUERY_ALL_USER_TEXT:
            for ( i = 0u; i < pTurn->iMessageCount; ++i ) {
                if ( pTurn->pMessages[i].eRole != XLLM_ROLE_USER ) {
                    continue;
                }
                if ( bAppended && xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
                    goto oom;
                }
                if ( xllm__memory_append_message_query_text(&tBuilder, &pTurn->pMessages[i]) != XRT_NET_OK ) {
                    goto oom;
                }
                bAppended = true;
            }
            break;
        case XLLM_MEMORY_TURN_QUERY_VISIBLE_TEXT:
            for ( i = 0u; i < pTurn->iMessageCount; ++i ) {
                if ( xllm__memory_append_message_line(&tBuilder, &pTurn->pMessages[i]) != XRT_NET_OK ) {
                    goto oom;
                }
                bAppended = true;
            }
            break;
        case XLLM_MEMORY_TURN_QUERY_LAST_USER_TEXT:
        default:
            for ( i = pTurn->iMessageCount; i > 0u; --i ) {
                if ( pTurn->pMessages[i - 1u].eRole != XLLM_ROLE_USER ) {
                    continue;
                }
                if ( bAppended && xllm__memory_text_builder_append_cstr(&tBuilder, "\n") != XRT_NET_OK ) {
                    goto oom;
                }
                if ( xllm__memory_append_message_query_text(&tBuilder, &pTurn->pMessages[i - 1u]) != XRT_NET_OK ) {
                    goto oom;
                }
                bAppended = true;
                break;
            }
            break;
    }

    if ( !bAppended || tBuilder.iLength == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory turn search/apply produced empty query");
        goto cleanup;
    }

    iStatus = xllm__memory_dup_query_text(
        tBuilder.sText,
        tBuilder.iLength,
        pUseOptions->uMaxQueryChars,
        psQuery,
        pError
    );
    goto cleanup;

oom:
    xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build memory turn search query");

cleanup:
    xllm__memory_text_builder_reset(&tBuilder);
    return iStatus;
}

static void xllm__memory_chunk_free(xllm__memory_chunk_entry *pChunk)
{
    if ( !pChunk ) {
        return;
    }

    xllm__free_cstr(&pChunk->sChunkId);
    xllm__free_cstr(&pChunk->sText);
    xllm__free_cstr(&pChunk->sMemoryProfileId);
    xllm__free_cstr(&pChunk->sChunkProfileId);
    xllm__free_cstr(&pChunk->sRetrievalProfileId);
    xllm__free_cstr(&pChunk->sEmbedProfileId);
    xllm__free_cstr(&pChunk->sIndexProfileId);
    xllm__free_cstr(&pChunk->sPreviousChunkId);
    xllm__free_cstr(&pChunk->sNextChunkId);
    if ( pChunk->pfEmbedding ) {
        xrtFree(pChunk->pfEmbedding);
    }
    memset(pChunk, 0, sizeof(*pChunk));
}

static void xllm__memory_record_free(xllm__memory_record_entry *pRecord)
{
    size_t i;

    if ( !pRecord ) {
        return;
    }

    xllm__free_cstr(&pRecord->sRecordId);
    xllm__free_cstr(&pRecord->sTitle);
    xllm__free_cstr(&pRecord->sSourceUri);
    xllm__free_cstr(&pRecord->sText);
    xllm__free_cstr(&pRecord->sMemoryProfileId);
    xllm__free_cstr(&pRecord->sRetrievalProfileId);
    xllm__free_cstr(&pRecord->sEmbedProfileId);
    xllm__free_cstr(&pRecord->sIndexProfileId);
    xllm__xvalue_release(&pRecord->tMetadata);
    xllm__xvalue_release(&pRecord->tVendorExtra);
    for ( i = 0u; i < pRecord->iChunkCount; ++i ) {
        xllm__memory_chunk_free(&pRecord->pChunks[i]);
    }
    if ( pRecord->pChunks ) {
        xrtFree(pRecord->pChunks);
    }
    memset(pRecord, 0, sizeof(*pRecord));
}

static xvalue xllm__memory_clone_public_metadata(xvalue tMetadata)
{
    xvalue tCopy;

    if ( !tMetadata ) {
        return 0;
    }

    tCopy = xvoDeepCopy(tMetadata);
    if ( tCopy ) {
        return tCopy;
    }

    return xvoCopy(tMetadata);
}

static int xllm__memory_append_record_info_clone(
    xllm_memory_record_info **ppInfos,
    size_t *piInfoCount,
    const xllm__memory_record_entry *pRecord
);
static unsigned char xllm__memory_path_fold_char(unsigned char c);

static int xllm__memory_append_skipped_file_info(
    xllm_memory_skipped_file_info **ppInfos,
    size_t *piInfoCount,
    const char *sRootPath,
    const char *sPath,
    const char *sRelativePathHint,
    const char *sSourceUriPrefix,
    xllm_memory_skip_reason eReason,
    uint64 uFileBytes
);

static int xllm__memory_append_failed_file_info(
    xllm_memory_failed_file_info **ppInfos,
    size_t *piInfoCount,
    const char *sRootPath,
    const char *sPath,
    const char *sRelativePathHint,
    const char *sSourceUriPrefix,
    xllm_memory_fail_reason eReason,
    uint64 uFileBytes,
    const xllm_error *pError
);

static int xllm__memory_record_info_copy_public(
    xllm_memory_record_info *pDest,
    const xllm_memory_record_info *pSrc
);

static int xllm__memory_skipped_file_info_copy(
    xllm_memory_skipped_file_info *pDest,
    const xllm_memory_skipped_file_info *pSrc
);

static int xllm__memory_failed_file_info_copy(
    xllm_memory_failed_file_info *pDest,
    const xllm_memory_failed_file_info *pSrc
);

static void xllm__memory_hit_reset(xllm_memory_hit *pHit)
{
    if ( !pHit ) {
        return;
    }

    xllm__free_cstr((char **)&pHit->sRecordId);
    xllm__free_cstr((char **)&pHit->sChunkId);
    xllm__free_cstr((char **)&pHit->sTitle);
    xllm__free_cstr((char **)&pHit->sSourceUri);
    xllm__free_cstr((char **)&pHit->sText);
    xllm__free_cstr((char **)&pHit->sRetrievalProfileId);
    xllm__free_cstr((char **)&pHit->sEmbedProfileId);
    xllm__free_cstr((char **)&pHit->sIndexProfileId);
    xllm__xvalue_release(&pHit->tMetadata);
    xllm__xvalue_release(&pHit->tVendorExtra);
    memset(pHit, 0, sizeof(*pHit));
}

static void xllm__memory_record_info_reset(xllm_memory_record_info *pInfo)
{
    if ( !pInfo ) {
        return;
    }

    xllm__free_cstr((char **)&pInfo->sRecordId);
    xllm__free_cstr((char **)&pInfo->sTitle);
    xllm__free_cstr((char **)&pInfo->sSourceUri);
    xllm__free_cstr((char **)&pInfo->sMemoryProfileId);
    xllm__free_cstr((char **)&pInfo->sRetrievalProfileId);
    xllm__free_cstr((char **)&pInfo->sEmbedProfileId);
    xllm__free_cstr((char **)&pInfo->sIndexProfileId);
    xllm__xvalue_release(&pInfo->tMetadata);
    xllm__xvalue_release(&pInfo->tVendorExtra);
    memset(pInfo, 0, sizeof(*pInfo));
}

static void xllm__memory_chunk_info_reset(xllm_memory_chunk_info *pInfo)
{
    if ( !pInfo ) {
        return;
    }

    xllm__free_cstr((char **)&pInfo->sRecordId);
    xllm__free_cstr((char **)&pInfo->sChunkId);
    xllm__free_cstr((char **)&pInfo->sTitle);
    xllm__free_cstr((char **)&pInfo->sSourceUri);
    xllm__free_cstr((char **)&pInfo->sText);
    xllm__free_cstr((char **)&pInfo->sMemoryProfileId);
    xllm__free_cstr((char **)&pInfo->sChunkProfileId);
    xllm__free_cstr((char **)&pInfo->sRetrievalProfileId);
    xllm__free_cstr((char **)&pInfo->sEmbedProfileId);
    xllm__free_cstr((char **)&pInfo->sIndexProfileId);
    xllm__free_cstr((char **)&pInfo->sPreviousChunkId);
    xllm__free_cstr((char **)&pInfo->sNextChunkId);
    xllm__xvalue_release(&pInfo->tMetadata);
    xllm__xvalue_release(&pInfo->tVendorExtra);
    memset(pInfo, 0, sizeof(*pInfo));
}

static void xllm__memory_skipped_file_info_reset(xllm_memory_skipped_file_info *pInfo)
{
    if ( !pInfo ) {
        return;
    }

    xllm__free_cstr((char **)&pInfo->sPath);
    xllm__free_cstr((char **)&pInfo->sRelativePath);
    xllm__free_cstr((char **)&pInfo->sSourceUri);
    xllm__xvalue_release(&pInfo->tVendorExtra);
    memset(pInfo, 0, sizeof(*pInfo));
}

static void xllm__memory_failed_file_info_reset(xllm_memory_failed_file_info *pInfo)
{
    if ( !pInfo ) {
        return;
    }

    xllm__free_cstr((char **)&pInfo->sPath);
    xllm__free_cstr((char **)&pInfo->sRelativePath);
    xllm__free_cstr((char **)&pInfo->sSourceUri);
    xllm__free_cstr((char **)&pInfo->sMessage);
    xllm__xvalue_release(&pInfo->tVendorExtra);
    memset(pInfo, 0, sizeof(*pInfo));
}

static void xllm__memory_change_info_reset(xllm_memory_change_info *pInfo)
{
    if ( !pInfo ) {
        return;
    }

    xllm__memory_record_info_reset(&pInfo->tRecord);
    xllm__memory_skipped_file_info_reset(&pInfo->tSkipped);
    xllm__memory_failed_file_info_reset(&pInfo->tFailed);
    memset(pInfo, 0, sizeof(*pInfo));
}

static void xllm__memory_file_event_reset(xllm_memory_file_event *pEvent)
{
    if ( !pEvent ) {
        return;
    }

    xllm__free_cstr((char **)&pEvent->sPath);
    xllm__free_cstr((char **)&pEvent->sPreviousPath);
    xllm__xvalue_release(&pEvent->tVendorExtra);
    memset(pEvent, 0, sizeof(*pEvent));
}

static void xllm__memory_file_event_array_reset(
    xllm_memory_file_event *pEvents,
    size_t iEventCount
)
{
    size_t i;

    if ( !pEvents ) {
        return;
    }
    for ( i = 0u; i < iEventCount; ++i ) {
        xllm__memory_file_event_reset(&pEvents[i]);
    }
    xrtFree(pEvents);
}

static int xllm__memory_file_event_copy(
    xllm_memory_file_event *pDest,
    const xllm_memory_file_event *pSrc
)
{
    if ( !pDest || !pSrc ) {
        return XRT_NET_ERROR;
    }

    memset(pDest, 0, sizeof(*pDest));
    pDest->eKind = pSrc->eKind;
    if ( pSrc->sPath ) {
        pDest->sPath = xllm__dup_cstr(pSrc->sPath);
        if ( !pDest->sPath ) {
            xllm__memory_file_event_reset(pDest);
            return XRT_NET_ERROR;
        }
    }
    if ( pSrc->sPreviousPath ) {
        pDest->sPreviousPath = xllm__dup_cstr(pSrc->sPreviousPath);
        if ( !pDest->sPreviousPath ) {
            xllm__memory_file_event_reset(pDest);
            return XRT_NET_ERROR;
        }
    }
    pDest->tVendorExtra = pSrc->tVendorExtra;
    xllm__xvalue_addref(pDest->tVendorExtra);
    return XRT_NET_OK;
}

static int xllm__memory_file_event_replace(
    xllm_memory_file_event *pDest,
    const xllm_memory_file_event *pSrc,
    const char *sWhat,
    xllm_error *pError
)
{
    xllm_memory_file_event tCopy;

    if ( !pDest || !pSrc ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory file event replace requires destination and source");
        return XRT_NET_ERROR;
    }
    if ( xllm__memory_file_event_copy(&tCopy, pSrc) != XRT_NET_OK ) {
        xllm__error_set(
            pError,
            XLLM_ERROR_INTERNAL,
            sWhat ? sWhat : "failed to replace memory file event"
        );
        return XRT_NET_ERROR;
    }

    xllm__memory_file_event_reset(pDest);
    *pDest = tCopy;
    return XRT_NET_OK;
}

static bool xllm__memory_path_equal_ci(
    const char *sA,
    const char *sB
)
{
    unsigned char a;
    unsigned char b;

    if ( sA == sB ) {
        return true;
    }
    if ( !sA || !sB ) {
        return false;
    }

    while ( *sA || *sB ) {
        a = xllm__memory_path_fold_char((unsigned char)*sA++);
        b = xllm__memory_path_fold_char((unsigned char)*sB++);
        if ( a != b ) {
            return false;
        }
    }
    return true;
}

static int xllm__memory_file_event_try_merge(
    xllm_memory_file_event *pExisting,
    const xllm_memory_file_event *pIncoming,
    bool *pbHandled,
    bool *pbRemoveExisting,
    xllm_error *pError
)
{
    if ( pbHandled ) {
        *pbHandled = false;
    }
    if ( pbRemoveExisting ) {
        *pbRemoveExisting = false;
    }
    if ( !pExisting || !pIncoming ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory file event merge requires existing and incoming events");
        return XRT_NET_ERROR;
    }

    if ( pExisting->eKind == XLLM_MEMORY_FILE_EVENT_CREATED &&
         pIncoming->eKind == XLLM_MEMORY_FILE_EVENT_UPDATED &&
         xllm__memory_path_equal_ci(pExisting->sPath, pIncoming->sPath) ) {
        if ( pbHandled ) {
            *pbHandled = true;
        }
        return XRT_NET_OK;
    }
    if ( pExisting->eKind == XLLM_MEMORY_FILE_EVENT_UPDATED &&
         pIncoming->eKind == XLLM_MEMORY_FILE_EVENT_UPDATED &&
         xllm__memory_path_equal_ci(pExisting->sPath, pIncoming->sPath) ) {
        if ( pbHandled ) {
            *pbHandled = true;
        }
        return XRT_NET_OK;
    }
    if ( pExisting->eKind == XLLM_MEMORY_FILE_EVENT_CREATED &&
         pIncoming->eKind == XLLM_MEMORY_FILE_EVENT_DELETED &&
         xllm__memory_path_equal_ci(pExisting->sPath, pIncoming->sPath) ) {
        if ( pbHandled ) {
            *pbHandled = true;
        }
        if ( pbRemoveExisting ) {
            *pbRemoveExisting = true;
        }
        return XRT_NET_OK;
    }
    if ( pExisting->eKind == XLLM_MEMORY_FILE_EVENT_UPDATED &&
         pIncoming->eKind == XLLM_MEMORY_FILE_EVENT_DELETED &&
         xllm__memory_path_equal_ci(pExisting->sPath, pIncoming->sPath) ) {
        if ( xllm__memory_file_event_replace(
                pExisting,
                pIncoming,
                "failed to coalesce memory file delete event",
                pError
             ) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( pbHandled ) {
            *pbHandled = true;
        }
        return XRT_NET_OK;
    }
    if ( pExisting->eKind == XLLM_MEMORY_FILE_EVENT_CREATED &&
         pIncoming->eKind == XLLM_MEMORY_FILE_EVENT_RENAMED &&
         xllm__memory_path_equal_ci(pExisting->sPath, pIncoming->sPreviousPath) ) {
        char *sNewPath = xllm__dup_cstr(pIncoming->sPath);

        if ( !sNewPath ) {
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to coalesce memory file rename event");
            return XRT_NET_ERROR;
        }
        xllm__free_cstr((char **)&pExisting->sPath);
        pExisting->sPath = sNewPath;
        if ( pbHandled ) {
            *pbHandled = true;
        }
        return XRT_NET_OK;
    }
    if ( pExisting->eKind == XLLM_MEMORY_FILE_EVENT_UPDATED &&
         pIncoming->eKind == XLLM_MEMORY_FILE_EVENT_RENAMED &&
         xllm__memory_path_equal_ci(pExisting->sPath, pIncoming->sPreviousPath) ) {
        if ( xllm__memory_file_event_replace(
                pExisting,
                pIncoming,
                "failed to coalesce memory file rename event",
                pError
             ) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( pbHandled ) {
            *pbHandled = true;
        }
        return XRT_NET_OK;
    }
    if ( pExisting->eKind == XLLM_MEMORY_FILE_EVENT_RENAMED &&
         pIncoming->eKind == XLLM_MEMORY_FILE_EVENT_UPDATED &&
         xllm__memory_path_equal_ci(pExisting->sPath, pIncoming->sPath) ) {
        if ( pbHandled ) {
            *pbHandled = true;
        }
        return XRT_NET_OK;
    }
    if ( pExisting->eKind == XLLM_MEMORY_FILE_EVENT_DELETED &&
         pIncoming->eKind == XLLM_MEMORY_FILE_EVENT_DELETED &&
         xllm__memory_path_equal_ci(pExisting->sPath, pIncoming->sPath) ) {
        if ( pbHandled ) {
            *pbHandled = true;
        }
        return XRT_NET_OK;
    }
    return XRT_NET_OK;
}

static void xllm__memory_file_event_array_remove_at(
    xllm_memory_file_event *pItems,
    size_t *piItemCount,
    size_t iIndex
)
{
    size_t iCount;

    if ( !pItems || !piItemCount || iIndex >= *piItemCount ) {
        return;
    }

    iCount = *piItemCount;
    xllm__memory_file_event_reset(&pItems[iIndex]);
    if ( iIndex + 1u < iCount ) {
        memmove(
            pItems + iIndex,
            pItems + iIndex + 1u,
            (iCount - iIndex - 1u) * sizeof(*pItems)
        );
    }
    memset(pItems + (iCount - 1u), 0, sizeof(*pItems));
    *piItemCount = iCount - 1u;
}

static void xllm__memory_embedder_release(
    xllm_memory_embedder *pEmbedder,
    bool bDisposeCtx
)
{
    if ( !pEmbedder ) {
        return;
    }

    if ( bDisposeCtx && pEmbedder->pfnDisposeCtx && pEmbedder->pCtx ) {
        pEmbedder->pfnDisposeCtx(pEmbedder->pCtx);
    }
    pEmbedder->pCtx = NULL;
    xllm__xvalue_release(&pEmbedder->tVendorExtra);
    memset(pEmbedder, 0, sizeof(*pEmbedder));
}

static const char *xllm__memory_embedder_extra_text(
    const xllm_memory_embedder *pEmbedder,
    const char *sKey
)
{
    if ( !pEmbedder || !pEmbedder->tVendorExtra || xvoType(pEmbedder->tVendorExtra) != XVO_DT_TABLE || !sKey ) {
        return NULL;
    }
    return (const char *)xvoTableGetText(pEmbedder->tVendorExtra, (str)sKey, 0u);
}

static int64 xllm__memory_embedder_extra_int(
    const xllm_memory_embedder *pEmbedder,
    const char *sKey,
    int64 iDefaultValue
)
{
    if ( !pEmbedder || !pEmbedder->tVendorExtra || xvoType(pEmbedder->tVendorExtra) != XVO_DT_TABLE || !sKey ) {
        return iDefaultValue;
    }
    return xvoTableGetInt(pEmbedder->tVendorExtra, (str)sKey, 0u);
}

static int xllm__memory_make_chunk_id(
    const char *sNamespace,
    const char *sRecordId,
    uint32 uChunkIndex,
    char **psChunkId
)
{
    char sBuffer[512];
    int iWritten;

    if ( !psChunkId || !sRecordId || !sRecordId[0] ) {
        return XRT_NET_ERROR;
    }

    iWritten = snprintf(
        sBuffer,
        sizeof(sBuffer),
        "%s%s%s#%u",
        (sNamespace && sNamespace[0]) ? sNamespace : "",
        (sNamespace && sNamespace[0]) ? ":" : "",
        sRecordId,
        (unsigned)uChunkIndex
    );
    if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sBuffer) ) {
        return XRT_NET_ERROR;
    }

    *psChunkId = xllm__dup_cstr(sBuffer);
    return *psChunkId ? XRT_NET_OK : XRT_NET_ERROR;
}

static uint64 xllm__memory_hash_namespace(const char *sNamespace)
{
    const unsigned char *p = (const unsigned char *)(sNamespace ? sNamespace : "");
    uint64 uHash = 1469598103934665603ull;

    while ( *p ) {
        uHash ^= (uint64)(*p++);
        uHash *= 1099511628211ull;
    }
    return uHash;
}

static uint64 xllm__memory_hash_bytes(const char *sText, size_t iLength)
{
    const unsigned char *p = (const unsigned char *)(sText ? sText : "");
    size_t i;
    uint64 uHash = 1469598103934665603ull;

    for ( i = 0u; i < iLength; ++i ) {
        uHash ^= (uint64)p[i];
        uHash *= 1099511628211ull;
    }
    return uHash;
}

static uint64 xllm__memory_hash_text(const char *sText)
{
    return xllm__memory_hash_bytes(sText, sText ? strlen(sText) : 0u);
}

static char *xllm__memory_dup_file_basename(const char *sPath)
{
    const char *sName;

    if ( !sPath || !sPath[0] ) {
        return NULL;
    }

    sName = strrchr(sPath, '\\');
    if ( !sName ) {
        sName = strrchr(sPath, '/');
    }
    sName = sName ? (sName + 1) : sPath;
    return xllm__dup_cstr(sName);
}

static size_t xllm__memory_find_record_index_locked(
    const xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sRecordId
)
{
    size_t i;

    if ( !pMemory || !sRecordId || !sRecordId[0] ) {
        return (size_t)-1;
    }
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        if ( pMemory->pRecords[i].eScope == eScope &&
             pMemory->pRecords[i].sRecordId &&
             strcmp(pMemory->pRecords[i].sRecordId, sRecordId) == 0 ) {
            return i;
        }
    }
    return (size_t)-1;
}

static const char *xllm__memory_path_basename_ptr(const char *sPath)
{
    const char *sName;

    if ( !sPath ) {
        return NULL;
    }

    sName = strrchr(sPath, '\\');
    if ( !sName ) {
        sName = strrchr(sPath, '/');
    }
    return sName ? (sName + 1) : sPath;
}

static const char *xllm__memory_path_extension_ptr(const char *sPath)
{
    const char *sName = xllm__memory_path_basename_ptr(sPath);
    const char *sExt;

    if ( !sName || !sName[0] ) {
        return NULL;
    }

    sExt = strrchr(sName, '.');
    if ( !sExt || sExt == sName ) {
        return NULL;
    }
    return sExt;
}

static char *xllm__memory_dup_relative_path(
    const char *sRootPath,
    const char *sPath
);

static int xllm__memory_ascii_stricmp(const char *sA, const char *sB)
{
    unsigned char a;
    unsigned char b;

    if ( sA == sB ) {
        return 0;
    }
    if ( !sA ) {
        return -1;
    }
    if ( !sB ) {
        return 1;
    }

    while ( *sA || *sB ) {
        a = (unsigned char)*sA++;
        b = (unsigned char)*sB++;
        if ( a >= 'A' && a <= 'Z' ) {
            a = (unsigned char)(a - 'A' + 'a');
        }
        if ( b >= 'A' && b <= 'Z' ) {
            b = (unsigned char)(b - 'A' + 'a');
        }
        if ( a != b ) {
            return (int)a - (int)b;
        }
    }
    return 0;
}

static int xllm__memory_ascii_strnicmp(
    const char *sA,
    const char *sB,
    size_t iCount
)
{
    unsigned char a;
    unsigned char b;

    if ( sA == sB || iCount == 0u ) {
        return 0;
    }
    if ( !sA ) {
        return -1;
    }
    if ( !sB ) {
        return 1;
    }

    while ( iCount-- > 0u ) {
        a = (unsigned char)*sA++;
        b = (unsigned char)*sB++;
        if ( a >= 'A' && a <= 'Z' ) {
            a = (unsigned char)(a - 'A' + 'a');
        }
        if ( b >= 'A' && b <= 'Z' ) {
            b = (unsigned char)(b - 'A' + 'a');
        }
        if ( a != b ) {
            return (int)a - (int)b;
        }
        if ( a == '\0' ) {
            return 0;
        }
    }
    return 0;
}

static bool xllm__memory_ascii_contains(
    const char *sHaystack,
    const char *sNeedle
)
{
    size_t iNeedleLength;
    const char *p;

    if ( !sNeedle || !sNeedle[0] ) {
        return true;
    }
    if ( !sHaystack || !sHaystack[0] ) {
        return false;
    }

    iNeedleLength = strlen(sNeedle);
    for ( p = sHaystack; *p; ++p ) {
        if ( xllm__memory_ascii_strnicmp(p, sNeedle, iNeedleLength) == 0 ) {
            return true;
        }
    }
    return false;
}

static bool xllm__memory_path_is_hidden(const char *sPath)
{
    const char *sName = xllm__memory_path_basename_ptr(sPath);
    return sName && sName[0] == '.';
}

static bool xllm__memory_path_has_hidden_component(const char *sPath)
{
    const char *p = sPath;

    if ( !sPath || !sPath[0] ) {
        return false;
    }
    if ( xllm__memory_path_is_hidden(sPath) ) {
        return true;
    }

    while ( *p ) {
        const char *sStart;
        size_t iLen;

        while ( *p == '\\' || *p == '/' ) {
            ++p;
        }
        if ( !*p ) {
            break;
        }
        sStart = p;
        while ( *p && *p != '\\' && *p != '/' ) {
            ++p;
        }
        iLen = (size_t)(p - sStart);
        if ( iLen > 0u && sStart[0] == '.' ) {
            if ( !(iLen == 1u && sStart[0] == '.') &&
                 !(iLen == 2u && sStart[0] == '.' && sStart[1] == '.') ) {
                return true;
            }
        }
    }
    return false;
}

static bool xllm__memory_token_list_contains_ci(
    const char *sList,
    const char *sValue
)
{
    const char *p = sList;

    if ( !sList || !sList[0] || !sValue || !sValue[0] ) {
        return false;
    }

    while ( *p ) {
        char sToken[128];
        size_t iTokenLength = 0u;

        while ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',' || *p == ';' || *p == '|' ) {
            ++p;
        }
        if ( !*p ) {
            break;
        }
        while ( *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != ',' && *p != ';' && *p != '|' ) {
            if ( iTokenLength + 1u < sizeof(sToken) ) {
                sToken[iTokenLength++] = *p;
            }
            ++p;
        }
        sToken[iTokenLength] = '\0';
        if ( iTokenLength > 0u && xllm__memory_ascii_stricmp(sToken, sValue) == 0 ) {
            return true;
        }
    }
    return false;
}

static unsigned char xllm__memory_path_fold_char(unsigned char c)
{
    if ( c == '\\' ) {
        return '/';
    }
    if ( c >= 'A' && c <= 'Z' ) {
        return (unsigned char)(c - 'A' + 'a');
    }
    return c;
}

static bool xllm__memory_path_glob_match_ci(
    const char *sPattern,
    const char *sText
)
{
    unsigned char p;
    unsigned char t;

    if ( !sPattern || !sText ) {
        return false;
    }

    while ( *sPattern ) {
        if ( *sPattern == '*' ) {
            while ( *sPattern == '*' ) {
                ++sPattern;
            }
            if ( !*sPattern ) {
                return true;
            }
            while ( *sText ) {
                if ( xllm__memory_path_glob_match_ci(sPattern, sText) ) {
                    return true;
                }
                ++sText;
            }
            return false;
        }
        if ( !*sText ) {
            return false;
        }
        p = xllm__memory_path_fold_char((unsigned char)*sPattern);
        t = xllm__memory_path_fold_char((unsigned char)*sText);
        if ( p != '?' && p != t ) {
            return false;
        }
        ++sPattern;
        ++sText;
    }
    return *sText == '\0';
}

static bool xllm__memory_path_matches_ignored_patterns(
    const char *sPatterns,
    const char *sRelativePath
)
{
    const char *p = sPatterns;
    bool bIgnored = false;

    if ( !sPatterns || !sPatterns[0] || !sRelativePath || !sRelativePath[0] ) {
        return false;
    }

    while ( *p ) {
        char sToken[260];
        size_t iTokenLength = 0u;
        bool bHasSeparator = false;
        bool bNegated = false;
        bool bMatched = false;
        const char *sPathCursor;

        while ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',' || *p == ';' || *p == '|' ) {
            ++p;
        }
        if ( !*p ) {
            break;
        }
        while ( *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != ',' && *p != ';' && *p != '|' ) {
            if ( *p == '\\' || *p == '/' ) {
                bHasSeparator = true;
            }
            if ( iTokenLength + 1u < sizeof(sToken) ) {
                sToken[iTokenLength++] = *p;
            }
            ++p;
        }
        sToken[iTokenLength] = '\0';
        if ( iTokenLength == 0u ) {
            continue;
        }

        if ( sToken[0] == '!' ) {
            bNegated = true;
            memmove(sToken, sToken + 1, iTokenLength);
            --iTokenLength;
        }
        if ( iTokenLength == 0u ) {
            continue;
        }

        if ( !bHasSeparator ) {
            sPathCursor = sRelativePath;
            while ( *sPathCursor ) {
                const char *sComponentStart = sPathCursor;
                char sComponent[260];
                size_t iComponentLength = 0u;

                while ( *sPathCursor && *sPathCursor != '\\' && *sPathCursor != '/' ) {
                    ++sPathCursor;
                }
                while ( sComponentStart + iComponentLength < sPathCursor &&
                        iComponentLength + 1u < sizeof(sComponent) ) {
                    sComponent[iComponentLength] = sComponentStart[iComponentLength];
                    ++iComponentLength;
                }
                sComponent[iComponentLength] = '\0';
                if ( iComponentLength > 0u &&
                     xllm__memory_path_glob_match_ci(sToken, sComponent) ) {
                    bMatched = true;
                    break;
                }
                while ( *sPathCursor == '\\' || *sPathCursor == '/' ) {
                    ++sPathCursor;
                }
            }
        } else {
            bMatched = xllm__memory_path_glob_match_ci(sToken, sRelativePath);
        }

        if ( bMatched ) {
            bIgnored = !bNegated;
        }
    }
    return bIgnored;
}

static int xllm__memory_append_pattern_token(
    char **psPatterns,
    size_t *piLength,
    size_t *piCapacity,
    const char *sToken
)
{
    size_t iTokenLength;
    size_t iRequired;
    size_t iNewCapacity;
    char *sNewBuffer;

    if ( !psPatterns || !piLength || !piCapacity || !sToken || !sToken[0] ) {
        return XRT_NET_OK;
    }

    iTokenLength = strlen(sToken);
    iRequired = *piLength + iTokenLength + ((*piLength > 0u) ? 1u : 0u) + 1u;
    if ( iRequired > *piCapacity ) {
        iNewCapacity = (*piCapacity > 0u) ? *piCapacity : 64u;
        while ( iNewCapacity < iRequired ) {
            iNewCapacity *= 2u;
        }
        sNewBuffer = (char *)xrtRealloc(*psPatterns, iNewCapacity);
        if ( !sNewBuffer ) {
            return XRT_NET_ERROR;
        }
        *psPatterns = sNewBuffer;
        *piCapacity = iNewCapacity;
    }

    if ( *piLength > 0u ) {
        (*psPatterns)[(*piLength)++] = ';';
    }
    memcpy(*psPatterns + *piLength, sToken, iTokenLength);
    *piLength += iTokenLength;
    (*psPatterns)[*piLength] = '\0';
    return XRT_NET_OK;
}

static int xllm__memory_append_ignore_pattern_span(
    char **psPatterns,
    size_t *piLength,
    size_t *piCapacity,
    const char *sPattern,
    size_t iPatternLength,
    const char *sBasePrefix
)
{
    const char *sStart = sPattern;
    const char *sEnd;
    char *sNormalized;
    char *sCombined = NULL;
    size_t iOutLength = 0u;
    size_t i;
    int iStatus = XRT_NET_OK;
    bool bNegated = false;
    bool bDirectoryOnly = false;
    bool bHasSeparator = false;
    bool bAnchored = false;

    if ( !psPatterns || !piLength || !piCapacity || !sPattern || iPatternLength == 0u ) {
        return XRT_NET_OK;
    }

    sEnd = sPattern + iPatternLength;
    while ( sStart < sEnd && (*sStart == ' ' || *sStart == '\t' || *sStart == '\r' || *sStart == '\n') ) {
        ++sStart;
    }
    while ( sEnd > sStart && (sEnd[-1] == ' ' || sEnd[-1] == '\t' || sEnd[-1] == '\r' || sEnd[-1] == '\n') ) {
        --sEnd;
    }
    if ( sStart >= sEnd || *sStart == '#' ) {
        return XRT_NET_OK;
    }

    if ( *sStart == '!' ) {
        bNegated = true;
        ++sStart;
        while ( sStart < sEnd && (*sStart == ' ' || *sStart == '\t') ) {
            ++sStart;
        }
        if ( sStart >= sEnd || *sStart == '#' ) {
            return XRT_NET_OK;
        }
    }

    while ( sStart < sEnd && (*sStart == '/' || *sStart == '\\') ) {
        bAnchored = true;
        ++sStart;
    }
    while ( sEnd > sStart && (sEnd[-1] == ' ' || sEnd[-1] == '\t') ) {
        --sEnd;
    }
    if ( sStart >= sEnd ) {
        return XRT_NET_OK;
    }

    sNormalized = (char *)xrtCalloc((size_t)(sEnd - sStart) + 3u, sizeof(char));
    if ( !sNormalized ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0u; sStart + i < sEnd; ++i ) {
        char ch = sStart[i];
        if ( ch == '\\' || ch == '/' ) {
            bHasSeparator = true;
        }
        sNormalized[iOutLength++] = (ch == '\\') ? '/' : ch;
    }
    while ( iOutLength > 0u &&
            (sNormalized[iOutLength - 1u] == ' ' || sNormalized[iOutLength - 1u] == '\t') ) {
        --iOutLength;
    }
    if ( iOutLength > 0u && sNormalized[iOutLength - 1u] == '/' ) {
        bDirectoryOnly = true;
        --iOutLength;
    }
    while ( iOutLength > 0u && sNormalized[iOutLength - 1u] == '/' ) {
        --iOutLength;
    }
    while ( iOutLength > 0u && sNormalized[0] == '/' ) {
        memmove(sNormalized, sNormalized + 1, iOutLength - 1u);
        --iOutLength;
    }
    sNormalized[iOutLength] = '\0';
    if ( iOutLength == 0u ) {
        xrtFree(sNormalized);
        return XRT_NET_OK;
    }

    if ( sBasePrefix && sBasePrefix[0] ) {
        size_t iBaseLength = strlen(sBasePrefix);
        size_t iCombinedCapacity = iBaseLength + iOutLength + 6u;

        sCombined = (char *)xrtCalloc(iCombinedCapacity, sizeof(char));
        if ( !sCombined ) {
            xrtFree(sNormalized);
            return XRT_NET_ERROR;
        }

        if ( bNegated ) {
            sCombined[0] = '!';
            memcpy(sCombined + 1u, sBasePrefix, iBaseLength);
            sCombined[1u + iBaseLength] = '/';
            memcpy(sCombined + 1u + iBaseLength + 1u, sNormalized, iOutLength);
            if ( bDirectoryOnly ) {
                memcpy(sCombined + 1u + iBaseLength + 1u + iOutLength, "/*", 3u);
            }
        } else {
            memcpy(sCombined, sBasePrefix, iBaseLength);
            sCombined[iBaseLength] = '/';
            memcpy(sCombined + iBaseLength + 1u, sNormalized, iOutLength);
            if ( bDirectoryOnly ) {
                memcpy(sCombined + iBaseLength + 1u + iOutLength, "/*", 3u);
            }
        }
        iStatus = xllm__memory_append_pattern_token(psPatterns, piLength, piCapacity, sCombined);
        if ( iStatus == XRT_NET_OK && !bHasSeparator && !bDirectoryOnly && !bAnchored ) {
            size_t iOffset = bNegated ? 1u : 0u;
            size_t iCurrentLength = strlen(sCombined);
            char *sDeepCombined = (char *)xrtCalloc(iCurrentLength + 4u, sizeof(char));

            if ( !sDeepCombined ) {
                iStatus = XRT_NET_ERROR;
            } else {
                memcpy(sDeepCombined, sCombined, iCurrentLength);
                memmove(
                    sDeepCombined + iOffset + iBaseLength + 3u,
                    sDeepCombined + iOffset + iBaseLength + 1u,
                    iOutLength + 1u
                );
                memcpy(sDeepCombined + iOffset + iBaseLength + 1u, "/*/", 3u);
                iStatus = xllm__memory_append_pattern_token(psPatterns, piLength, piCapacity, sDeepCombined);
                xrtFree(sDeepCombined);
            }
        }
        xrtFree(sCombined);
    } else {
        if ( bNegated ) {
            sCombined = (char *)xrtCalloc(iOutLength + 4u, sizeof(char));
            if ( !sCombined ) {
                xrtFree(sNormalized);
                return XRT_NET_ERROR;
            }
            sCombined[0] = '!';
            memcpy(sCombined + 1u, sNormalized, iOutLength);
            if ( bDirectoryOnly ) {
                memcpy(sCombined + 1u + iOutLength, "/*", 3u);
            }
            iStatus = xllm__memory_append_pattern_token(psPatterns, piLength, piCapacity, sCombined);
            xrtFree(sCombined);
        } else {
            if ( bDirectoryOnly ) {
                sCombined = (char *)xrtCalloc(iOutLength + 3u, sizeof(char));
                if ( !sCombined ) {
                    xrtFree(sNormalized);
                    return XRT_NET_ERROR;
                }
                memcpy(sCombined, sNormalized, iOutLength);
                memcpy(sCombined + iOutLength, "/*", 3u);
                iStatus = xllm__memory_append_pattern_token(psPatterns, piLength, piCapacity, sCombined);
                xrtFree(sCombined);
            } else {
                iStatus = xllm__memory_append_pattern_token(psPatterns, piLength, piCapacity, sNormalized);
            }
        }
    }

    xrtFree(sNormalized);
    return iStatus;
}

static int xllm__memory_append_patterns_from_text(
    char **psPatterns,
    size_t *piLength,
    size_t *piCapacity,
    const char *sPatternList,
    const char *sBasePrefix
)
{
    const char *p = sPatternList;

    if ( !sPatternList || !sPatternList[0] ) {
        return XRT_NET_OK;
    }

    while ( *p ) {
        const char *sStart = p;

        while ( *p && *p != ';' && *p != ',' && *p != '|' && *p != '\r' && *p != '\n' ) {
            ++p;
        }
        if ( p > sStart ) {
            int iStatus = xllm__memory_append_ignore_pattern_span(
                psPatterns,
                piLength,
                piCapacity,
                sStart,
                (size_t)(p - sStart),
                sBasePrefix
            );
            if ( iStatus != XRT_NET_OK ) {
                return iStatus;
            }
        }
        while ( *p == ';' || *p == ',' || *p == '|' || *p == '\r' || *p == '\n' ) {
            ++p;
        }
    }
    return XRT_NET_OK;
}

static bool xllm__memory_path_is_absolute(const char *sPath)
{
    if ( !sPath || !sPath[0] ) {
        return false;
    }
    if ( (unsigned char)sPath[0] >= 'A' && (unsigned char)sPath[0] <= 'Z' && sPath[1] == ':' ) {
        return true;
    }
    if ( (unsigned char)sPath[0] >= 'a' && (unsigned char)sPath[0] <= 'z' && sPath[1] == ':' ) {
        return true;
    }
    if ( (sPath[0] == '\\' || sPath[0] == '/') &&
         (sPath[1] == '\\' || sPath[1] == '/') ) {
        return true;
    }
    return sPath[0] == '\\' || sPath[0] == '/';
}

static char *xllm__memory_join_path(
    const char *sRootPath,
    const char *sPath
)
{
    size_t iRootLength;
    size_t iPathLength;
    bool bNeedsSeparator;
    char *sJoined;

    if ( !sPath || !sPath[0] ) {
        return NULL;
    }
    if ( !sRootPath || !sRootPath[0] || xllm__memory_path_is_absolute(sPath) ) {
        return xllm__dup_cstr(sPath);
    }

    iRootLength = strlen(sRootPath);
    iPathLength = strlen(sPath);
    bNeedsSeparator = iRootLength > 0u &&
        sRootPath[iRootLength - 1u] != '\\' &&
        sRootPath[iRootLength - 1u] != '/';
    sJoined = (char *)xrtCalloc(iRootLength + iPathLength + (bNeedsSeparator ? 2u : 1u), sizeof(char));
    if ( !sJoined ) {
        return NULL;
    }
    memcpy(sJoined, sRootPath, iRootLength);
    if ( bNeedsSeparator ) {
        sJoined[iRootLength++] = '\\';
    }
    memcpy(sJoined + iRootLength, sPath, iPathLength);
    sJoined[iRootLength + iPathLength] = '\0';
    return sJoined;
}

static int xllm__memory_append_patterns_from_ignore_file(
    char **psPatterns,
    size_t *piLength,
    size_t *piCapacity,
    const char *sFilePath,
    const char *sBasePrefix
)
{
    FILE *pFile;
    char sLine[4096];

    if ( !psPatterns || !piLength || !piCapacity || !sFilePath || !sFilePath[0] ) {
        return XRT_NET_OK;
    }

    pFile = fopen(sFilePath, "rb");
    if ( !pFile ) {
        return XRT_NET_OK;
    }

    while ( fgets(sLine, (int)sizeof(sLine), pFile) != NULL ) {
        int iStatus = xllm__memory_append_ignore_pattern_span(
            psPatterns,
            piLength,
            piCapacity,
            sLine,
            strlen(sLine),
            sBasePrefix
        );
        if ( iStatus != XRT_NET_OK ) {
            fclose(pFile);
            return iStatus;
        }
    }
    fclose(pFile);
    return XRT_NET_OK;
}

static char *xllm__memory_dup_relative_dir_for_file(
    const char *sRootPath,
    const char *sFilePath
)
{
    char *sRelativePath;
    char *sLastSeparator;

    sRelativePath = xllm__memory_dup_relative_path(sRootPath, sFilePath);
    if ( !sRelativePath ) {
        return NULL;
    }
    sLastSeparator = strrchr(sRelativePath, '\\');
    if ( !sLastSeparator ) {
        sLastSeparator = strrchr(sRelativePath, '/');
    }
    if ( !sLastSeparator ) {
        sRelativePath[0] = '\0';
        return sRelativePath;
    }
    *sLastSeparator = '\0';
    while ( *sRelativePath == '\\' || *sRelativePath == '/' ) {
        memmove(sRelativePath, sRelativePath + 1u, strlen(sRelativePath));
    }
    {
        size_t i;
        for ( i = 0u; sRelativePath[i]; ++i ) {
            if ( sRelativePath[i] == '\\' ) {
                sRelativePath[i] = '/';
            }
        }
    }
    return sRelativePath;
}

static int xllm__memory_collect_gitignore_recursive_callback(
    str sScanPath,
    size_t iPathLength,
    int bDir,
    void *pData,
    void *pParam
)
{
    xllm__memory_ignore_file_scan_state *pState = (xllm__memory_ignore_file_scan_state *)pParam;
    const char *sPath = (const char *)sScanPath;
    const char *sName;
    char *sBasePrefix = NULL;
    int iStatus;

    (void)iPathLength;
    (void)pData;

    if ( !pState || pState->bOutOfMemory || bDir != 0 ) {
        return 0;
    }

    sName = xllm__memory_path_basename_ptr(sPath);
    if ( !sName || xllm__memory_ascii_stricmp(sName, ".gitignore") != 0 ) {
        return 0;
    }

    sBasePrefix = xllm__memory_dup_relative_dir_for_file(pState->sWorkspaceRootPath, sPath);
    if ( !sBasePrefix ) {
        pState->bOutOfMemory = true;
        return 0;
    }
    if ( !sBasePrefix[0] ) {
        xrtFree(sBasePrefix);
        return 0;
    }

    iStatus = xllm__memory_append_patterns_from_ignore_file(
        pState->psPatterns,
        pState->piLength,
        pState->piCapacity,
        sPath,
        sBasePrefix[0] ? sBasePrefix : NULL
    );
    xrtFree(sBasePrefix);
    if ( iStatus != XRT_NET_OK ) {
        pState->bOutOfMemory = true;
    }
    return 0;
}

static int xllm__memory_build_workspace_ignore_patterns(
    const xllm_memory_ingest_workspace_options *pOptions,
    char **psPatterns,
    xllm_error *pError
)
{
    char *sPatterns = NULL;
    size_t iLength = 0u;
    size_t iCapacity = 0u;
    int iStatus;

    if ( psPatterns ) {
        *psPatterns = NULL;
    }
    if ( !pOptions ) {
        return XRT_NET_OK;
    }

    iStatus = xllm__memory_append_patterns_from_text(
        &sPatterns,
        &iLength,
        &iCapacity,
        XLLM__MEMORY_WORKSPACE_IGNORED_PATH_PATTERNS,
        NULL
    );
    if ( iStatus != XRT_NET_OK ) {
        goto oom;
    }

    iStatus = xllm__memory_append_patterns_from_text(
        &sPatterns,
        &iLength,
        &iCapacity,
        pOptions->sIgnoredPathPatterns,
        NULL
    );
    if ( iStatus != XRT_NET_OK ) {
        goto oom;
    }

    if ( pOptions->bLoadGitIgnore && pOptions->sPath && pOptions->sPath[0] ) {
        char *sGitIgnorePath = xllm__memory_join_path(pOptions->sPath, ".gitignore");
        xllm__memory_ignore_file_scan_state tState;

        if ( !sGitIgnorePath ) {
            goto oom;
        }
        iStatus = xllm__memory_append_patterns_from_ignore_file(
            &sPatterns,
            &iLength,
            &iCapacity,
            sGitIgnorePath,
            NULL
        );
        xrtFree(sGitIgnorePath);
        if ( iStatus != XRT_NET_OK ) {
            goto oom;
        }

        memset(&tState, 0, sizeof(tState));
        tState.sWorkspaceRootPath = pOptions->sPath;
        tState.psPatterns = &sPatterns;
        tState.piLength = &iLength;
        tState.piCapacity = &iCapacity;
        xrtDirScan((str)pOptions->sPath, TRUE, xllm__memory_collect_gitignore_recursive_callback, &tState);
        if ( tState.bOutOfMemory ) {
            goto oom;
        }
    }

    if ( pOptions->sIgnoreFiles && pOptions->sIgnoreFiles[0] ) {
        const char *p = pOptions->sIgnoreFiles;
        while ( *p ) {
            const char *sStart = p;
            const char *sEnd;
            char *sTokenPath = NULL;
            char *sIgnoreFilePath = NULL;
            char *sIgnoreFileBasePrefix = NULL;

            while ( *p && *p != ';' && *p != ',' && *p != '|' && *p != '\r' && *p != '\n' ) {
                ++p;
            }
            sEnd = p;
            while ( sStart < sEnd && (*sStart == ' ' || *sStart == '\t') ) {
                ++sStart;
            }
            while ( sEnd > sStart && (sEnd[-1] == ' ' || sEnd[-1] == '\t') ) {
                --sEnd;
            }
            if ( sEnd > sStart ) {
                sTokenPath = (char *)xrtCalloc((size_t)(sEnd - sStart) + 1u, sizeof(char));
                if ( !sTokenPath ) {
                    goto oom;
                }
                memcpy(sTokenPath, sStart, (size_t)(sEnd - sStart));
                sTokenPath[sEnd - sStart] = '\0';
                sIgnoreFilePath = xllm__memory_join_path(pOptions->sPath, sTokenPath);
                xrtFree(sTokenPath);
                if ( !sIgnoreFilePath ) {
                    goto oom;
                }
                sIgnoreFileBasePrefix = xllm__memory_dup_relative_dir_for_file(pOptions->sPath, sIgnoreFilePath);
                if ( !sIgnoreFileBasePrefix ) {
                    xrtFree(sIgnoreFilePath);
                    goto oom;
                }
                iStatus = xllm__memory_append_patterns_from_ignore_file(
                    &sPatterns,
                    &iLength,
                    &iCapacity,
                    sIgnoreFilePath,
                    sIgnoreFileBasePrefix[0] ? sIgnoreFileBasePrefix : NULL
                );
                xrtFree(sIgnoreFileBasePrefix);
                xrtFree(sIgnoreFilePath);
                if ( iStatus != XRT_NET_OK ) {
                    goto oom;
                }
            }
            while ( *p == ';' || *p == ',' || *p == '|' || *p == '\r' || *p == '\n' ) {
                ++p;
            }
        }
    }

    if ( psPatterns ) {
        *psPatterns = sPatterns;
    } else if ( sPatterns ) {
        xrtFree(sPatterns);
    }
    return XRT_NET_OK;

oom:
    if ( sPatterns ) {
        xrtFree(sPatterns);
    }
    xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build workspace ignore patterns");
    return XRT_NET_ERROR;
}

static bool xllm__memory_path_has_ignored_directory(
    const char *sPath,
    const char *sIgnoredDirectories
)
{
    const char *p = sPath;

    if ( !sPath || !sPath[0] || !sIgnoredDirectories || !sIgnoredDirectories[0] ) {
        return false;
    }

    while ( *p ) {
        const char *sStart;
        size_t iLen;
        char sComponent[128];

        while ( *p == '\\' || *p == '/' ) {
            ++p;
        }
        if ( !*p ) {
            break;
        }
        sStart = p;
        while ( *p && *p != '\\' && *p != '/' ) {
            ++p;
        }
        iLen = (size_t)(p - sStart);
        if ( iLen == 0u ) {
            continue;
        }
        if ( iLen >= sizeof(sComponent) ) {
            iLen = sizeof(sComponent) - 1u;
        }
        memcpy(sComponent, sStart, iLen);
        sComponent[iLen] = '\0';
        if ( xllm__memory_token_list_contains_ci(sIgnoredDirectories, sComponent) ) {
            return true;
        }
    }
    return false;
}

static bool xllm__memory_extension_is_allowed(
    const char *sAllowedExtensions,
    const char *sPath
)
{
    const char *sExt = xllm__memory_path_extension_ptr(sPath);
    const char *p;

    if ( !sAllowedExtensions || !sAllowedExtensions[0] ) {
        return true;
    }
    if ( !sExt || !sExt[0] ) {
        return false;
    }

    p = sAllowedExtensions;
    while ( *p ) {
        char sToken[64];
        size_t iTokenLength = 0u;
        const char *sCompareToken = sToken;

        while ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',' || *p == ';' || *p == '|' ) {
            ++p;
        }
        if ( !*p ) {
            break;
        }
        while ( *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != ',' && *p != ';' && *p != '|' ) {
            if ( iTokenLength + 1u < sizeof(sToken) ) {
                sToken[iTokenLength++] = *p;
            }
            ++p;
        }
        sToken[iTokenLength] = '\0';
        if ( iTokenLength == 0u ) {
            continue;
        }
        if ( sCompareToken[0] != '.' ) {
            char sWithDot[65];

            if ( iTokenLength + 2u > sizeof(sWithDot) ) {
                continue;
            }
            sWithDot[0] = '.';
            memcpy(sWithDot + 1u, sToken, iTokenLength + 1u);
            sCompareToken = sWithDot;
            if ( xllm__memory_ascii_stricmp(sExt, sCompareToken) == 0 ) {
                return true;
            }
            continue;
        }
        if ( xllm__memory_ascii_stricmp(sExt, sCompareToken) == 0 ) {
            return true;
        }
    }
    return false;
}

static bool xllm__memory_extension_is_ignored(
    const char *sIgnoredExtensions,
    const char *sPath
)
{
    const char *sExt = xllm__memory_path_extension_ptr(sPath);
    const char *p;

    if ( !sIgnoredExtensions || !sIgnoredExtensions[0] ) {
        return false;
    }
    if ( !sExt || !sExt[0] ) {
        return false;
    }

    p = sIgnoredExtensions;
    while ( *p ) {
        char sToken[64];
        size_t iTokenLength = 0u;
        const char *sCompareToken = sToken;

        while ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',' || *p == ';' || *p == '|' ) {
            ++p;
        }
        if ( !*p ) {
            break;
        }
        while ( *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != ',' && *p != ';' && *p != '|' ) {
            if ( iTokenLength + 1u < sizeof(sToken) ) {
                sToken[iTokenLength++] = *p;
            }
            ++p;
        }
        sToken[iTokenLength] = '\0';
        if ( iTokenLength == 0u ) {
            continue;
        }
        if ( sCompareToken[0] != '.' ) {
            char sWithDot[65];

            if ( iTokenLength + 2u > sizeof(sWithDot) ) {
                continue;
            }
            sWithDot[0] = '.';
            memcpy(sWithDot + 1u, sToken, iTokenLength + 1u);
            sCompareToken = sWithDot;
        }
        if ( xllm__memory_ascii_stricmp(sExt, sCompareToken) == 0 ) {
            return true;
        }
    }
    return false;
}

static bool xllm__memory_scalar_matches_text(
    xvalue tValue,
    const char *sExpected
)
{
    int iType;
    char sBuffer[96];

    if ( !tValue || xvoType(tValue) == XVO_DT_NULL ) {
        return false;
    }
    if ( !sExpected || !sExpected[0] ) {
        return true;
    }

    iType = xvoType(tValue);
    switch ( iType ) {
        case XVO_DT_TEXT:
            return xllm__memory_ascii_stricmp((const char *)xvoGetText(tValue), sExpected) == 0;
        case XVO_DT_BOOL:
            if ( xvoGetBool(tValue) ) {
                return xllm__memory_ascii_stricmp(sExpected, "true") == 0 ||
                       strcmp(sExpected, "1") == 0;
            }
            return xllm__memory_ascii_stricmp(sExpected, "false") == 0 ||
                   strcmp(sExpected, "0") == 0;
        case XVO_DT_INT:
            snprintf(sBuffer, sizeof(sBuffer), "%lld", (long long)xvoGetInt(tValue));
            return strcmp(sBuffer, sExpected) == 0;
        case XVO_DT_FLOAT:
            snprintf(sBuffer, sizeof(sBuffer), "%.17g", xvoGetFloat(tValue));
            return strcmp(sBuffer, sExpected) == 0;
        default:
            return false;
    }
}

static bool xllm__memory_metadata_matches(
    xvalue tMetadata,
    const char *sMetadataKey,
    const char *sMetadataValue
)
{
    xvalue tValue;

    if ( (!sMetadataKey || !sMetadataKey[0]) && (!sMetadataValue || !sMetadataValue[0]) ) {
        return true;
    }
    if ( !sMetadataKey || !sMetadataKey[0] ) {
        return false;
    }
    if ( !tMetadata || xvoType(tMetadata) != XVO_DT_TABLE ) {
        return false;
    }

    tValue = xvoTableGetValue(tMetadata, sMetadataKey, 0u);
    if ( !tValue ) {
        return false;
    }
    return xllm__memory_scalar_matches_text(tValue, sMetadataValue);
}

static char *xllm__memory_make_file_source_uri(const char *sPath)
{
    size_t iLen;
    size_t i;
    char *sUri;

    if ( !sPath || !sPath[0] ) {
        return NULL;
    }

    iLen = strlen(sPath);
    sUri = (char *)xrtCalloc(iLen + 9u, sizeof(char));
    if ( !sUri ) {
        return NULL;
    }

    memcpy(sUri, "file:///", 8u);
    for ( i = 0u; i < iLen; ++i ) {
        sUri[8u + i] = (sPath[i] == '\\') ? '/' : sPath[i];
    }
    sUri[8u + iLen] = '\0';
    return sUri;
}

static char *xllm__memory_make_prefixed_relative_source_uri(
    const char *sPrefix,
    const char *sRootPath,
    const char *sPath
)
{
    char *sRelative = NULL;
    char *sUri = NULL;
    size_t iPrefixLen;
    size_t iRelativeLen;
    size_t i;

    sRelative = xllm__memory_dup_relative_path(sRootPath, sPath);
    if ( !sRelative ) {
        return NULL;
    }
    iPrefixLen = (sPrefix && sPrefix[0]) ? strlen(sPrefix) : 0u;
    iRelativeLen = strlen(sRelative);
    sUri = (char *)xrtCalloc(iPrefixLen + iRelativeLen + 1u, sizeof(char));
    if ( !sUri ) {
        xllm__free_cstr(&sRelative);
        return NULL;
    }
    if ( iPrefixLen > 0u ) {
        memcpy(sUri, sPrefix, iPrefixLen);
    }
    for ( i = 0u; i < iRelativeLen; ++i ) {
        sUri[iPrefixLen + i] = (sRelative[i] == '\\') ? '/' : sRelative[i];
    }
    sUri[iPrefixLen + iRelativeLen] = '\0';
    xllm__free_cstr(&sRelative);
    return sUri;
}

static int xllm__memory_make_file_record_id(
    const char *sPath,
    char **psRecordId
)
{
    char sBuffer[96];
    uint64 uHash;
    int iWritten;

    if ( !psRecordId || !sPath || !sPath[0] ) {
        return XRT_NET_ERROR;
    }

    uHash = xllm__memory_hash_namespace(sPath);
    iWritten = snprintf(
        sBuffer,
        sizeof(sBuffer),
        "file:%08x%08x",
        (unsigned)(uHash >> 32u),
        (unsigned)(uHash & 0xffffffffu)
    );
    if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sBuffer) ) {
        return XRT_NET_ERROR;
    }

    *psRecordId = xllm__dup_cstr(sBuffer);
    return *psRecordId ? XRT_NET_OK : XRT_NET_ERROR;
}

static int xllm__memory_make_file_record_id_prefixed(
    const char *sPrefix,
    const char *sPath,
    char **psRecordId
)
{
    char sBuffer[160];
    uint64 uHash;
    int iWritten;

    if ( !psRecordId || !sPath || !sPath[0] ) {
        return XRT_NET_ERROR;
    }
    if ( !sPrefix || !sPrefix[0] ) {
        return xllm__memory_make_file_record_id(sPath, psRecordId);
    }

    uHash = xllm__memory_hash_namespace(sPath);
    iWritten = snprintf(
        sBuffer,
        sizeof(sBuffer),
        "%s:%08x%08x",
        sPrefix,
        (unsigned)(uHash >> 32u),
        (unsigned)(uHash & 0xffffffffu)
    );
    if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sBuffer) ) {
        return XRT_NET_ERROR;
    }

    *psRecordId = xllm__dup_cstr(sBuffer);
    return *psRecordId ? XRT_NET_OK : XRT_NET_ERROR;
}

static char *xllm__memory_dup_relative_path(
    const char *sRootPath,
    const char *sPath
)
{
    size_t iRootLength;
    const char *sRelative;

    if ( !sPath || !sPath[0] ) {
        return NULL;
    }
    if ( !sRootPath || !sRootPath[0] ) {
        return xllm__dup_cstr(xllm__memory_path_basename_ptr(sPath));
    }

    iRootLength = strlen(sRootPath);
    if ( xllm__memory_ascii_strnicmp(sRootPath, sPath, iRootLength) != 0 ) {
        return xllm__dup_cstr(xllm__memory_path_basename_ptr(sPath));
    }
    sRelative = sPath + iRootLength;
    while ( *sRelative == '\\' || *sRelative == '/' ) {
        ++sRelative;
    }
    if ( !sRelative[0] ) {
        sRelative = xllm__memory_path_basename_ptr(sPath);
    }
    return xllm__dup_cstr(sRelative);
}

static int xllm__memory_get_file_size(
    const char *sPath,
    uint64 *puFileSize
)
{
    FILE *pFile = NULL;
    long iFileSize;

    if ( puFileSize ) {
        *puFileSize = 0u;
    }
    if ( !sPath || !sPath[0] || !puFileSize ) {
        return XRT_NET_ERROR;
    }

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return XRT_NET_ERROR;
    }
    if ( fseek(pFile, 0, SEEK_END) != 0 ) {
        fclose(pFile);
        return XRT_NET_ERROR;
    }
    iFileSize = ftell(pFile);
    fclose(pFile);
    if ( iFileSize < 0 ) {
        return XRT_NET_ERROR;
    }
    *puFileSize = (uint64)iFileSize;
    return XRT_NET_OK;
}

static int xllm__memory_get_file_mtime_unix(
    const char *sPath,
    int64 *piMtimeUnix
)
{
    struct _stat64 tStat;

    if ( piMtimeUnix ) {
        *piMtimeUnix = 0;
    }
    if ( !sPath || !sPath[0] || !piMtimeUnix ) {
        return XRT_NET_ERROR;
    }
    if ( _stat64(sPath, &tStat) != 0 ) {
        return XRT_NET_ERROR;
    }
    *piMtimeUnix = (int64)tStat.st_mtime;
    return XRT_NET_OK;
}

static bool xllm__memory_path_is_under_root_ci(
    const char *sRootPath,
    const char *sPath,
    const char **psRelative
)
{
    size_t iRootLength;
    const char *sRelative;

    if ( psRelative ) {
        *psRelative = NULL;
    }
    if ( !sRootPath || !sRootPath[0] || !sPath || !sPath[0] ) {
        return false;
    }

    iRootLength = strlen(sRootPath);
    if ( xllm__memory_ascii_strnicmp(sRootPath, sPath, iRootLength) != 0 ) {
        return false;
    }

    sRelative = sPath + iRootLength;
    if ( sRelative > sPath ) {
        char ch = sPath[iRootLength - 1u];
        if ( ch != '\\' && ch != '/' && *sRelative && *sRelative != '\\' && *sRelative != '/' ) {
            return false;
        }
    }
    while ( *sRelative == '\\' || *sRelative == '/' ) {
        ++sRelative;
    }
    if ( psRelative ) {
        *psRelative = sRelative;
    }
    return true;
}

static bool xllm__memory_directory_path_should_be_ingested(
    const xllm_memory_ingest_directory_options *pOptions,
    const char *sPath
)
{
    const char *sAllowedExtensions;
    const char *sIgnoredDirectories;
    const char *sIgnoredExtensions;
    const char *sIgnoredPathPatterns;
    const char *sRelativePath = NULL;
    uint64 uFileSize = 0u;

    if ( !pOptions || !sPath || !sPath[0] ) {
        return false;
    }
    if ( !xllm__memory_path_is_under_root_ci(pOptions->sPath, sPath, &sRelativePath) ) {
        return false;
    }
    if ( xllm__memory_get_file_size(sPath, &uFileSize) != XRT_NET_OK ) {
        return false;
    }

    sAllowedExtensions = pOptions->sAllowedExtensions;
    sIgnoredDirectories = pOptions->sIgnoredDirectories;
    sIgnoredExtensions = pOptions->sIgnoredExtensions;
    sIgnoredPathPatterns = pOptions->sIgnoredPathPatterns;

    if ( !sAllowedExtensions || !sAllowedExtensions[0] ) {
        sAllowedExtensions = XLLM__MEMORY_WORKSPACE_ALLOWED_EXTENSIONS;
    }
    if ( !sIgnoredDirectories || !sIgnoredDirectories[0] ) {
        sIgnoredDirectories = XLLM__MEMORY_WORKSPACE_IGNORED_DIRECTORIES;
    }
    if ( !sIgnoredExtensions || !sIgnoredExtensions[0] ) {
        sIgnoredExtensions = XLLM__MEMORY_WORKSPACE_IGNORED_EXTENSIONS;
    }
    if ( !sIgnoredPathPatterns || !sIgnoredPathPatterns[0] ) {
        sIgnoredPathPatterns = XLLM__MEMORY_WORKSPACE_IGNORED_PATH_PATTERNS;
    }

    if ( !sRelativePath || !sRelativePath[0] ) {
        sRelativePath = sPath;
    }
    if ( pOptions->bSkipHidden && xllm__memory_path_has_hidden_component(sRelativePath) ) {
        return false;
    }
    if ( xllm__memory_path_has_ignored_directory(sRelativePath, sIgnoredDirectories) ) {
        return false;
    }
    if ( xllm__memory_path_matches_ignored_patterns(sIgnoredPathPatterns, sRelativePath) ) {
        return false;
    }
    if ( xllm__memory_extension_is_ignored(sIgnoredExtensions, sRelativePath) ) {
        return false;
    }
    if ( !xllm__memory_extension_is_allowed(sAllowedExtensions, sRelativePath) ) {
        return false;
    }
    if ( pOptions->uMaxFileBytes > 0u && uFileSize > pOptions->uMaxFileBytes ) {
        return false;
    }
    return true;
}

static xvalue xllm__memory_make_file_metadata(
    xvalue tUserMetadata,
    const char *sRootPath,
    const char *sPath,
    uint64 uFileSize,
    int64 iMtimeUnix
)
{
    xvalue tMetadata = 0;
    char *sRelativePath = NULL;
    const char *sBasename;
    const char *sExtension;

    tMetadata = xllm__memory_create_shared_table_copy(tUserMetadata);
    if ( !tMetadata ) {
        return 0;
    }

    sRelativePath = xllm__memory_dup_relative_path(sRootPath, sPath);
    sBasename = xllm__memory_path_basename_ptr(sPath);
    sExtension = xllm__memory_path_extension_ptr(sPath);

    if ( !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_PATH, sPath) ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_RELATIVE_PATH, sRelativePath ? sRelativePath : "") ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_BASENAME, sBasename ? sBasename : "") ||
         !xllm__memory_table_set_owned_text(tMetadata, XLLM__MEMORY_METADATA_KEY_EXTENSION, sExtension ? sExtension : "") ||
         (!xvoTableGetText(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_SENSITIVITY, 0u) &&
          !xvoTableSetText(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_SENSITIVITY, 0u, (str)"normal", 0u, FALSE)) ||
         (!xvoTableGetText(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_SOURCE_TRUST, 0u) &&
          !xvoTableSetText(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_SOURCE_TRUST, 0u, (str)"local_file", 0u, FALSE)) ||
         (!xvoTableGetText(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_USER_SCOPE, 0u) &&
          !xvoTableSetText(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_USER_SCOPE, 0u, (str)"project", 0u, FALSE)) ||
         !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_BYTES, 0u, (int64)uFileSize) ||
         !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_MTIME_UNIX, 0u, iMtimeUnix) ) {
        if ( sRelativePath ) {
            xllm__free_cstr(&sRelativePath);
        }
        xvoUnref(tMetadata);
        return 0;
    }

    if ( sRelativePath ) {
        xllm__free_cstr(&sRelativePath);
    }
    return tMetadata;
}

static bool xllm__memory_record_matches_file_fingerprint(
    const xllm__memory_record_entry *pRecord,
    const char *sPath,
    uint64 uFileSize,
    int64 iMtimeUnix
)
{
    if ( !pRecord || !pRecord->tMetadata || xvoType(pRecord->tMetadata) != XVO_DT_TABLE ) {
        return false;
    }
    if ( xllm__memory_ascii_stricmp((const char *)xvoTableGetText(pRecord->tMetadata, XLLM__MEMORY_METADATA_KEY_PATH, 0u), sPath) != 0 ) {
        return false;
    }
    if ( xvoTableGetInt(pRecord->tMetadata, XLLM__MEMORY_METADATA_KEY_BYTES, 0u) != (int64)uFileSize ) {
        return false;
    }
    if ( xvoTableGetInt(pRecord->tMetadata, XLLM__MEMORY_METADATA_KEY_MTIME_UNIX, 0u) != iMtimeUnix ) {
        return false;
    }
    return true;
}

static bool xllm__memory_record_matches_content_hash(
    const xllm__memory_record_entry *pRecord,
    uint64 uContentHash
)
{
    if ( !pRecord || uContentHash == 0u ) {
        return false;
    }
    if ( pRecord->tMetadata && xvoType(pRecord->tMetadata) == XVO_DT_TABLE ) {
        int64 iStoredHash = xvoTableGetInt(pRecord->tMetadata, (str)XLLM__MEMORY_METADATA_KEY_CONTENT_HASH, 0u);
        if ( iStoredHash != 0 && (uint64)iStoredHash == uContentHash ) {
            return true;
        }
    }
    return xllm__memory_hash_text(pRecord->sText) == uContentHash;
}

static int xllm__memory_make_file_content_metadata(
    xvalue tUserMetadata,
    uint64 uContentHash,
    xvalue *ptMetadata
)
{
    xvalue tMetadata;

    if ( ptMetadata ) {
        *ptMetadata = 0;
    }
    if ( !ptMetadata ) {
        return XRT_NET_ERROR;
    }

    tMetadata = xllm__memory_create_shared_table_copy(tUserMetadata);
    if ( !tMetadata ) {
        return XRT_NET_ERROR;
    }
    if ( !xvoTableSetInt(tMetadata, (str)XLLM__MEMORY_METADATA_KEY_CONTENT_HASH, 0u, (int64)uContentHash) ) {
        xvoUnref(tMetadata);
        return XRT_NET_ERROR;
    }
    *ptMetadata = tMetadata;
    return XRT_NET_OK;
}

static int xllm__memory_read_text_file(
    const char *sPath,
    uint64 uMaxFileBytes,
    char **psText,
    size_t *piSize,
    xllm_error *pError
)
{
    FILE *pFile = NULL;
    long iFileSize;
    size_t iRead;
    char *sBuffer = NULL;
    size_t i;

    if ( !psText || !sPath || !sPath[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory ingest file requires a path");
        return XRT_NET_ERROR;
    }

    *psText = NULL;
    if ( piSize ) {
        *piSize = 0u;
    }

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to open memory ingest file");
        return XRT_NET_ERROR;
    }
    if ( fseek(pFile, 0, SEEK_END) != 0 ) {
        fclose(pFile);
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to seek memory ingest file");
        return XRT_NET_ERROR;
    }
    iFileSize = ftell(pFile);
    if ( iFileSize < 0 ) {
        fclose(pFile);
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to stat memory ingest file");
        return XRT_NET_ERROR;
    }
    if ( uMaxFileBytes > 0u && (uint64)iFileSize > uMaxFileBytes ) {
        fclose(pFile);
        xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "memory ingest file exceeds max_file_bytes");
        return XRT_NET_ERROR;
    }
    if ( fseek(pFile, 0, SEEK_SET) != 0 ) {
        fclose(pFile);
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to rewind memory ingest file");
        return XRT_NET_ERROR;
    }

    sBuffer = (char *)xrtCalloc((size_t)iFileSize + 1u, sizeof(char));
    if ( !sBuffer ) {
        fclose(pFile);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate memory ingest buffer");
        return XRT_NET_ERROR;
    }

    iRead = fread(sBuffer, 1u, (size_t)iFileSize, pFile);
    fclose(pFile);
    if ( iRead != (size_t)iFileSize ) {
        xrtFree(sBuffer);
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to read memory ingest file");
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < iRead; ++i ) {
        if ( sBuffer[i] == '\0' ) {
            xrtFree(sBuffer);
            xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "memory ingest file does not accept binary content");
            return XRT_NET_ERROR;
        }
    }

    sBuffer[iRead] = '\0';
    *psText = sBuffer;
    if ( piSize ) {
        *piSize = iRead;
    }
    return XRT_NET_OK;
}

static bool xllm__memory_text_contains_secret_pattern(const char *sText, size_t iSize)
{
    if ( !sText || iSize == 0u ) {
        return false;
    }

    if ( xllm__memory_ascii_contains(sText, "-----BEGIN PRIVATE KEY-----") ||
         xllm__memory_ascii_contains(sText, "-----BEGIN RSA PRIVATE KEY-----") ||
         xllm__memory_ascii_contains(sText, "-----BEGIN OPENSSH PRIVATE KEY-----") ||
         xllm__memory_ascii_contains(sText, "OPENAI_API_KEY=") ||
         xllm__memory_ascii_contains(sText, "ANTHROPIC_API_KEY=") ||
         xllm__memory_ascii_contains(sText, "GEMINI_API_KEY=") ||
         xllm__memory_ascii_contains(sText, "AZURE_OPENAI_API_KEY=") ||
         xllm__memory_ascii_contains(sText, "api_key=") ||
         xllm__memory_ascii_contains(sText, "apiKey=") ||
         xllm__memory_ascii_contains(sText, "client_secret") ||
         xllm__memory_ascii_contains(sText, "access_token=") ||
         xllm__memory_ascii_contains(sText, "password:") ||
         xllm__memory_ascii_contains(sText, "password=") ) {
        return true;
    }

    return false;
}

static int xllm__memory_file_contains_secret_pattern(
    const char *sPath,
    uint64 uMaxFileBytes,
    bool *pbDetected,
    xllm_error *pError
)
{
    char *sText = NULL;
    size_t iSize = 0u;
    int iStatus;

    if ( pbDetected ) {
        *pbDetected = false;
    }
    if ( !pbDetected ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory secret scanner requires output storage");
        return XRT_NET_ERROR;
    }

    iStatus = xllm__memory_read_text_file(sPath, uMaxFileBytes, &sText, &iSize, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }
    *pbDetected = xllm__memory_text_contains_secret_pattern(sText, iSize);
    xrtFree(sText);
    return XRT_NET_OK;
}

static bool xllm__memory_existing_record_matches_file_content(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sRecordId,
    const char *sPath,
    uint64 uMaxFileBytes,
    uint64 *puContentHash
)
{
    char *sText = NULL;
    size_t iTextSize = 0u;
    uint64 uContentHash;
    size_t iExisting;
    bool bMatches = false;

    if ( puContentHash ) {
        *puContentHash = 0u;
    }
    if ( !pMemory || !sRecordId || !sRecordId[0] || !sPath || !sPath[0] ) {
        return false;
    }
    if ( xllm__memory_read_text_file(sPath, uMaxFileBytes, &sText, &iTextSize, NULL) != XRT_NET_OK ) {
        return false;
    }
    uContentHash = xllm__memory_hash_bytes(sText, iTextSize);
    if ( puContentHash ) {
        *puContentHash = uContentHash;
    }

    xrtMutexLock(pMemory->pMutex);
    iExisting = xllm__memory_find_record_index_locked(pMemory, eScope, sRecordId);
    if ( iExisting != (size_t)-1 ) {
        bMatches = xllm__memory_record_matches_content_hash(&pMemory->pRecords[iExisting], uContentHash);
    }
    xrtMutexUnlock(pMemory->pMutex);

    xrtFree(sText);
    return bMatches;
}

static int xllm__memory_emit_ingest_progress(
    xllm__memory_directory_ingest_state *pState,
    xllm_memory_ingest_progress_kind eKind,
    const char *sPath,
    const char *sRelativePath,
    const char *sSourceUri,
    xllm_memory_skip_reason eSkipReason,
    xllm_memory_fail_reason eFailReason,
    xllm_error_code eErrorCode,
    int32 iStatus,
    uint64 uFileBytes
)
{
    xllm_memory_ingest_progress tProgress;

    if ( !pState || !pState->pOptions || !pState->pOptions->pfnProgress ) {
        return XRT_NET_OK;
    }

    memset(&tProgress, 0, sizeof(tProgress));
    tProgress.eKind = eKind;
    tProgress.eScope = pState->pOptions->eScope;
    tProgress.sPath = sPath;
    tProgress.sRelativePath = sRelativePath;
    tProgress.sSourceUri = sSourceUri;
    tProgress.eSkipReason = eSkipReason;
    tProgress.eFailReason = eFailReason;
    tProgress.eErrorCode = eErrorCode;
    tProgress.iStatus = iStatus;
    tProgress.uFileBytes = uFileBytes;
    if ( pState->pResult ) {
        tProgress.uVisitedFileCount = pState->pResult->uVisitedFileCount;
        tProgress.uIngestedFileCount = pState->pResult->uIngestedFileCount;
        tProgress.uCreatedRecordCount = pState->pResult->uCreatedRecordCount;
        tProgress.uUpdatedRecordCount = pState->pResult->uUpdatedRecordCount;
        tProgress.uSkippedFileCount = pState->pResult->uSkippedFileCount;
        tProgress.uFailedFileCount = pState->pResult->uFailedFileCount;
    }
    tProgress.tVendorExtra = pState->pOptions->tVendorExtra;

    if ( pState->pOptions->pfnProgress(pState->pOptions->pProgressCtx, &tProgress) != 0 ) {
        pState->bProgressAborted = true;
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static int xllm__memory_ingest_directory_callback(
    str sScanPath,
    size_t iPathLength,
    int bDir,
    void *pData,
    void *pParam
)
{
    xllm__memory_directory_ingest_state *pState = (xllm__memory_directory_ingest_state *)pParam;
    xllm_memory_ingest_file_options tFileOptions;
    const char *sPath = (const char *)sScanPath;
    const char *sAllowedExtensions;
    const char *sIgnoredDirectories;
    const char *sIgnoredExtensions;
    const char *sIgnoredPathPatterns;
    const char *sSourceUriPrefix;
    const char *sFilterPath = sPath;
    char *sRelativeTitle = NULL;
    char *sRecordId = NULL;
    char *sSourceUri = NULL;
    xvalue tMetadata = 0;
    uint64 uFileSize = 0u;
    int64 iMtimeUnix = 0;
    int iStatus;
    size_t iExisting = (size_t)-1;
    bool bSecretDetected = false;
    xllm_error tIngestError;

#define XLLM__MEMORY_CAPTURE_SKIP(reasonValue, fileBytesValue) \
    do { \
        ++pState->pResult->uSkippedFileCount; \
        (void)xllm__memory_append_skipped_file_info( \
            &pState->pResult->pSkippedFiles, \
            &pState->pResult->iSkippedDetailCount, \
            pState->sRootPath, \
            sPath, \
            sFilterPath, \
            sSourceUriPrefix, \
            (reasonValue), \
            (fileBytesValue) \
        ); \
        return xllm__memory_emit_ingest_progress( \
            pState, \
            XLLM_MEMORY_INGEST_PROGRESS_SKIPPED, \
            sPath, \
            sFilterPath, \
            NULL, \
            (reasonValue), \
            0, \
            XLLM_ERROR_NONE, \
            XRT_NET_OK, \
            (fileBytesValue) \
        ) == XRT_NET_OK ? 0 : 1; \
    } while (0)

#define XLLM__MEMORY_CAPTURE_FAIL(reasonValue, fileBytesValue, errorPtr) \
    do { \
        const xllm_error *pCaptureError = (errorPtr); \
        ++pState->pResult->uFailedFileCount; \
        (void)xllm__memory_append_failed_file_info( \
            &pState->pResult->pFailedFiles, \
            &pState->pResult->iFailedDetailCount, \
            pState->sRootPath, \
            sPath, \
            sFilterPath, \
            sSourceUriPrefix, \
            (reasonValue), \
            (fileBytesValue), \
            pCaptureError \
        ); \
        return xllm__memory_emit_ingest_progress( \
            pState, \
            XLLM_MEMORY_INGEST_PROGRESS_FAILED, \
            sPath, \
            sFilterPath, \
            NULL, \
            0, \
            (reasonValue), \
            pCaptureError ? pCaptureError->eCode : XLLM_ERROR_NONE, \
            XRT_NET_ERROR, \
            (fileBytesValue) \
        ) == XRT_NET_OK ? 0 : 1; \
    } while (0)

    (void)iPathLength;
    (void)pData;

    if ( !pState || !pState->pMemory || !pState->pOptions || !pState->pResult ) {
        return 1;
    }
    if ( bDir != 0 ) {
        return 0;
    }
    xllm_error_init(&tIngestError);

    sAllowedExtensions = pState->pOptions->sAllowedExtensions;
    sIgnoredDirectories = pState->pOptions->sIgnoredDirectories;
    sIgnoredExtensions = pState->pOptions->sIgnoredExtensions;
    sIgnoredPathPatterns = pState->pOptions->sIgnoredPathPatterns;
    sSourceUriPrefix = pState->pOptions->sSourceUriPrefix;
    if ( pState->pOptions->bUseWorkspaceDefaults ) {
        if ( !sAllowedExtensions || !sAllowedExtensions[0] ) {
            sAllowedExtensions = XLLM__MEMORY_WORKSPACE_ALLOWED_EXTENSIONS;
        }
        if ( !sIgnoredDirectories || !sIgnoredDirectories[0] ) {
            sIgnoredDirectories = XLLM__MEMORY_WORKSPACE_IGNORED_DIRECTORIES;
        }
        if ( !sIgnoredExtensions || !sIgnoredExtensions[0] ) {
            sIgnoredExtensions = XLLM__MEMORY_WORKSPACE_IGNORED_EXTENSIONS;
        }
        if ( !sIgnoredPathPatterns || !sIgnoredPathPatterns[0] ) {
            sIgnoredPathPatterns = XLLM__MEMORY_WORKSPACE_IGNORED_PATH_PATTERNS;
        }
        if ( !sSourceUriPrefix || !sSourceUriPrefix[0] ) {
            sSourceUriPrefix = XLLM__MEMORY_WORKSPACE_DEFAULT_SOURCE_URI_PREFIX;
        }
    }
    if ( pState->sRootPath && pState->sRootPath[0] ) {
        size_t iRootLength = strlen(pState->sRootPath);
        if ( xllm__memory_ascii_strnicmp(pState->sRootPath, sPath, iRootLength) == 0 ) {
            sFilterPath = sPath + iRootLength;
            while ( *sFilterPath == '\\' || *sFilterPath == '/' ) {
                ++sFilterPath;
            }
            if ( !sFilterPath[0] ) {
                sFilterPath = sPath;
            }
        }
    }

    ++pState->pResult->uVisitedFileCount;
    if ( xllm__memory_emit_ingest_progress(
            pState,
            XLLM_MEMORY_INGEST_PROGRESS_VISITED,
            sPath,
            sFilterPath,
            NULL,
            0,
            0,
            XLLM_ERROR_NONE,
            XRT_NET_OK,
            0u
         ) != XRT_NET_OK ) {
        return 1;
    }
    if ( pState->pOptions->bSkipHidden && xllm__memory_path_has_hidden_component(sFilterPath) ) {
        XLLM__MEMORY_CAPTURE_SKIP(XLLM_MEMORY_SKIP_HIDDEN, 0u);
    }
    if ( xllm__memory_path_has_ignored_directory(sFilterPath, sIgnoredDirectories) ) {
        XLLM__MEMORY_CAPTURE_SKIP(XLLM_MEMORY_SKIP_IGNORED_DIRECTORY, 0u);
    }
    if ( xllm__memory_path_matches_ignored_patterns(sIgnoredPathPatterns, sFilterPath) ) {
        XLLM__MEMORY_CAPTURE_SKIP(XLLM_MEMORY_SKIP_IGNORED_PATH_PATTERN, 0u);
    }
    if ( xllm__memory_extension_is_ignored(sIgnoredExtensions, sFilterPath) ) {
        XLLM__MEMORY_CAPTURE_SKIP(XLLM_MEMORY_SKIP_IGNORED_EXTENSION, 0u);
    }
    if ( !xllm__memory_extension_is_allowed(sAllowedExtensions, sFilterPath) ) {
        XLLM__MEMORY_CAPTURE_SKIP(XLLM_MEMORY_SKIP_DISALLOWED_EXTENSION, 0u);
    }
    if ( pState->pOptions->uMaxFileBytes > 0u ) {
        if ( xllm__memory_get_file_size(sPath, &uFileSize) != XRT_NET_OK ) {
            XLLM__MEMORY_CAPTURE_FAIL(XLLM_MEMORY_FAIL_GET_SIZE, 0u, NULL);
        }
        if ( uFileSize > pState->pOptions->uMaxFileBytes ) {
            XLLM__MEMORY_CAPTURE_SKIP(XLLM_MEMORY_SKIP_TOO_LARGE, uFileSize);
        }
    }
    iStatus = xllm__memory_file_contains_secret_pattern(sPath, pState->pOptions->uMaxFileBytes, &bSecretDetected, &tIngestError);
    if ( iStatus != XRT_NET_OK ) {
        int iProgressStatus;

        ++pState->pResult->uFailedFileCount;
        (void)xllm__memory_append_failed_file_info(
            &pState->pResult->pFailedFiles,
            &pState->pResult->iFailedDetailCount,
            pState->sRootPath,
            sPath,
            sFilterPath,
            sSourceUriPrefix,
            XLLM_MEMORY_FAIL_INGEST,
            uFileSize,
            &tIngestError
        );
        iProgressStatus = xllm__memory_emit_ingest_progress(
            pState,
            XLLM_MEMORY_INGEST_PROGRESS_FAILED,
            sPath,
            sFilterPath,
            NULL,
            0,
            XLLM_MEMORY_FAIL_INGEST,
            tIngestError.eCode,
            iStatus,
            uFileSize
        );
        xllm_error_free(&tIngestError);
        return iProgressStatus == XRT_NET_OK ? 0 : 1;
    }
    if ( bSecretDetected ) {
        XLLM__MEMORY_CAPTURE_SKIP(XLLM_MEMORY_SKIP_SECRET_DETECTED, uFileSize);
    }
    if ( xllm__memory_get_file_mtime_unix(sPath, &iMtimeUnix) != XRT_NET_OK ) {
        XLLM__MEMORY_CAPTURE_FAIL(XLLM_MEMORY_FAIL_GET_MTIME, uFileSize, NULL);
    }

    xllm_memory_ingest_file_options_init(&tFileOptions);
    tFileOptions.eScope = pState->pOptions->eScope;
    tFileOptions.sPath = sPath;
    tFileOptions.bReplaceExisting = pState->pOptions->bReplaceExisting;
    tFileOptions.uMaxFileBytes = pState->pOptions->uMaxFileBytes;
    tFileOptions.uChunkChars = pState->pOptions->uChunkChars;
    tFileOptions.uChunkOverlapChars = pState->pOptions->uChunkOverlapChars;
    tFileOptions.tVendorExtra = pState->pOptions->tVendorExtra;

    sRelativeTitle = xllm__memory_dup_relative_path(pState->sRootPath, sPath);
    if ( sRelativeTitle ) {
        tFileOptions.sTitle = sRelativeTitle;
    }
    if ( pState->pOptions->sRecordIdPrefix && pState->pOptions->sRecordIdPrefix[0] ) {
        if ( xllm__memory_make_file_record_id_prefixed(pState->pOptions->sRecordIdPrefix, sPath, &sRecordId) == XRT_NET_OK ) {
            tFileOptions.sRecordId = sRecordId;
        }
    } else if ( xllm__memory_make_file_record_id(sPath, &sRecordId) == XRT_NET_OK ) {
        tFileOptions.sRecordId = sRecordId;
    }
    if ( sSourceUriPrefix && sSourceUriPrefix[0] ) {
        sSourceUri = xllm__memory_make_prefixed_relative_source_uri(sSourceUriPrefix, pState->sRootPath, sPath);
        if ( sSourceUri ) {
            tFileOptions.sSourceUri = sSourceUri;
        }
    }
    tMetadata = xllm__memory_make_file_metadata(pState->pOptions->tMetadata, pState->sRootPath, sPath, uFileSize, iMtimeUnix);
    if ( !tMetadata ) {
        xllm__free_cstr(&sRelativeTitle);
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sSourceUri);
        XLLM__MEMORY_CAPTURE_FAIL(XLLM_MEMORY_FAIL_BUILD_METADATA, uFileSize, NULL);
    }
    tFileOptions.tMetadata = tMetadata;
    if ( pState->pOptions->bSkipUnchanged && tFileOptions.sRecordId && tFileOptions.sRecordId[0] ) {
        bool bUnchanged = false;

        xrtMutexLock(pState->pMemory->pMutex);
        iExisting = xllm__memory_find_record_index_locked(pState->pMemory, tFileOptions.eScope, tFileOptions.sRecordId);
        if ( iExisting != (size_t)-1 &&
             xllm__memory_record_matches_file_fingerprint(&pState->pMemory->pRecords[iExisting], sPath, uFileSize, iMtimeUnix) ) {
            bUnchanged = true;
        }
        xrtMutexUnlock(pState->pMemory->pMutex);
        if ( !bUnchanged && iExisting != (size_t)-1 ) {
            bUnchanged = xllm__memory_existing_record_matches_file_content(
                pState->pMemory,
                tFileOptions.eScope,
                tFileOptions.sRecordId,
                sPath,
                pState->pOptions->uMaxFileBytes,
                NULL
            );
        }
        if ( bUnchanged ) {
            xvoUnref(tMetadata);
            xllm__free_cstr(&sRelativeTitle);
            xllm__free_cstr(&sRecordId);
            xllm__free_cstr(&sSourceUri);
            XLLM__MEMORY_CAPTURE_SKIP(XLLM_MEMORY_SKIP_UNCHANGED, uFileSize);
        }
    }

    iStatus = xllm_memory_ingest_file(pState->pMemory, &tFileOptions, &tIngestError);
    if ( iStatus == XRT_NET_OK ) {
        size_t iCurrent;

        ++pState->pResult->uIngestedFileCount;
        xrtMutexLock(pState->pMemory->pMutex);
        iCurrent = xllm__memory_find_record_index_locked(pState->pMemory, tFileOptions.eScope, tFileOptions.sRecordId);
        if ( iExisting != (size_t)-1 ) {
            ++pState->pResult->uUpdatedRecordCount;
            if ( iCurrent != (size_t)-1 ) {
                (void)xllm__memory_append_record_info_clone(
                    &pState->pResult->pUpdatedRecords,
                    &pState->pResult->iUpdatedDetailCount,
                    &pState->pMemory->pRecords[iCurrent]
                );
            }
        } else {
            ++pState->pResult->uCreatedRecordCount;
            if ( iCurrent != (size_t)-1 ) {
                (void)xllm__memory_append_record_info_clone(
                    &pState->pResult->pCreatedRecords,
                    &pState->pResult->iCreatedDetailCount,
                    &pState->pMemory->pRecords[iCurrent]
                );
            }
        }
        xrtMutexUnlock(pState->pMemory->pMutex);
    } else {
        int iProgressStatus;

        ++pState->pResult->uFailedFileCount;
        (void)xllm__memory_append_failed_file_info(
            &pState->pResult->pFailedFiles,
            &pState->pResult->iFailedDetailCount,
            pState->sRootPath,
            sPath,
            sFilterPath,
            sSourceUriPrefix,
            XLLM_MEMORY_FAIL_INGEST,
            uFileSize,
            &tIngestError
        );
        iProgressStatus = xllm__memory_emit_ingest_progress(
            pState,
            XLLM_MEMORY_INGEST_PROGRESS_FAILED,
            sPath,
            sFilterPath,
            tFileOptions.sSourceUri,
            0,
            XLLM_MEMORY_FAIL_INGEST,
            tIngestError.eCode,
            iStatus,
            uFileSize
        );
        xllm_error_free(&tIngestError);
        return iProgressStatus == XRT_NET_OK ? 0 : 1;
    }

    if ( xllm__memory_emit_ingest_progress(
            pState,
            XLLM_MEMORY_INGEST_PROGRESS_INGESTED,
            sPath,
            sFilterPath,
            tFileOptions.sSourceUri,
            0,
            0,
            XLLM_ERROR_NONE,
            XRT_NET_OK,
            uFileSize
         ) != XRT_NET_OK ) {
        xllm__free_cstr(&sRelativeTitle);
        xllm__free_cstr(&sRecordId);
        xllm__free_cstr(&sSourceUri);
        if ( tMetadata ) {
            xvoUnref(tMetadata);
        }
        xllm_error_free(&tIngestError);
        return 1;
    }

    xllm__free_cstr(&sRelativeTitle);
    xllm__free_cstr(&sRecordId);
    xllm__free_cstr(&sSourceUri);
    if ( tMetadata ) {
        xvoUnref(tMetadata);
    }
    xllm_error_free(&tIngestError);
#undef XLLM__MEMORY_CAPTURE_FAIL
#undef XLLM__MEMORY_CAPTURE_SKIP
    return 0;
}

static int xllm__memory_record_rechunk(
    xllm_memory *pMemory,
    xllm__memory_record_entry *pRecord,
    uint32 uChunkChars,
    uint32 uChunkOverlapChars
)
{
    xllm__chunk_entry *pChunks = NULL;
    size_t iChunkCount = 0u;
    size_t iChunkCapacity = 0u;
    size_t i;

    if ( !pMemory || !pRecord || !pRecord->sText ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < pRecord->iChunkCount; ++i ) {
        xllm__memory_chunk_free(&pRecord->pChunks[i]);
    }
    if ( pRecord->pChunks ) {
        xrtFree(pRecord->pChunks);
    }
    pRecord->pChunks = NULL;
    pRecord->iChunkCount = 0u;
    pRecord->iChunkCapacity = 0u;

    if ( xllm__chunk_text(
            pRecord->sText,
            uChunkChars > 0u ? uChunkChars : pMemory->uDefaultChunkChars,
            uChunkOverlapChars > 0u ? uChunkOverlapChars : pMemory->uDefaultChunkOverlapChars,
            &pChunks,
            &iChunkCount,
            &iChunkCapacity
         ) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < iChunkCount; ++i ) {
        xllm__memory_chunk_entry tChunk;

        memset(&tChunk, 0, sizeof(tChunk));
        tChunk.sText = xllm__dup_cstr(pChunks[i].sText);
        tChunk.uChunkIndex = pChunks[i].uChunkIndex;
        tChunk.iStartByte = pChunks[i].iStartByte;
        tChunk.iEndByte = pChunks[i].iEndByte;
        tChunk.uContentHash = xllm__memory_hash_text(pChunks[i].sText);
        if ( !tChunk.sText ) {
            xllm__chunk_entries_free(pChunks, iChunkCount);
            return XRT_NET_ERROR;
        }
        if ( xllm__memory_make_chunk_id(pMemory->sNamespace, pRecord->sRecordId, tChunk.uChunkIndex, &tChunk.sChunkId) != XRT_NET_OK ) {
            xllm__memory_chunk_free(&tChunk);
            xllm__chunk_entries_free(pChunks, iChunkCount);
            return XRT_NET_ERROR;
        }
        if ( xllm__memory_chunk_assign_default_profiles(pMemory, pRecord, &tChunk) != XRT_NET_OK ) {
            xllm__memory_chunk_free(&tChunk);
            xllm__chunk_entries_free(pChunks, iChunkCount);
            return XRT_NET_ERROR;
        }

        if ( xllm__append_buffer(
                (void **)&pRecord->pChunks,
                sizeof(tChunk),
                &pRecord->iChunkCount,
                &pRecord->iChunkCapacity,
                &tChunk
             ) != XRT_NET_OK ) {
            xllm__memory_chunk_free(&tChunk);
            xllm__chunk_entries_free(pChunks, iChunkCount);
            return XRT_NET_ERROR;
        }
    }
    if ( xllm__memory_record_assign_chunk_adjacency(pRecord) != XRT_NET_OK ) {
        xllm__chunk_entries_free(pChunks, iChunkCount);
        return XRT_NET_ERROR;
    }

    xllm__chunk_entries_free(pChunks, iChunkCount);
    return pRecord->iChunkCount > 0u ? XRT_NET_OK : XRT_NET_ERROR;
}

static int xllm__memory_record_embed_chunks(
    xllm_memory *pMemory,
    xllm__memory_record_entry *pRecord,
    xllm_error *pError
)
{
    size_t i;

    if ( !pMemory || !pRecord ) {
        return XRT_NET_ERROR;
    }
    if ( !pMemory->tEmbedder.pfnEmbedText ) {
        return XRT_NET_OK;
    }

    for ( i = 0u; i < pRecord->iChunkCount; ++i ) {
        xllm_memory_embedding tEmbedding;
        float *pfCopy;

        memset(&tEmbedding, 0, sizeof(tEmbedding));
        if ( xllm__memory_embed_text(
                &pMemory->tEmbedder,
                XLLM_MEMORY_EMBED_DOCUMENT,
                pRecord->pChunks[i].sText,
                &tEmbedding,
                pError
             ) != XRT_NET_OK ) {
            xllm__memory_embedding_reset_with_embedder(&pMemory->tEmbedder, &tEmbedding);
            return XRT_NET_ERROR;
        }
        pfCopy = (float *)xrtCalloc((size_t)tEmbedding.uValueCount, sizeof(float));
        if ( !pfCopy ) {
            xllm__memory_embedding_reset_with_embedder(&pMemory->tEmbedder, &tEmbedding);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate chunk embedding");
            return XRT_NET_ERROR;
        }
        memcpy(pfCopy, tEmbedding.pfValues, (size_t)tEmbedding.uValueCount * sizeof(float));
        pRecord->pChunks[i].pfEmbedding = pfCopy;
        pRecord->pChunks[i].uEmbeddingDim = tEmbedding.uValueCount;
        xllm__memory_embedding_reset_with_embedder(&pMemory->tEmbedder, &tEmbedding);
    }

    return XRT_NET_OK;
}

#include "xllm_memory_lindex.c"
#include "xllm_memory_vindex.c"

static int64 xllm__memory_record_priority(const xllm__memory_record_entry *pRecord)
{
    if ( !pRecord || !pRecord->tMetadata || xvoType(pRecord->tMetadata) != XVO_DT_TABLE ) {
        return 0;
    }

    return xvoTableGetInt(pRecord->tMetadata, (str)XLLM__MEMORY_METADATA_KEY_PRIORITY, 0u);
}

static double xllm__memory_score_priority_boost(
    const xllm__memory_record_entry *pRecord,
    const xllm_memory_search_options *pOptions
)
{
    double fWeight;
    int64 iPriority;

    if ( !pRecord || !pOptions || !pOptions->tPriorityWeight.bSet ) {
        return 0.0;
    }

    fWeight = pOptions->tPriorityWeight.fValue;
    if ( fWeight == 0.0 ) {
        return 0.0;
    }

    iPriority = xllm__memory_record_priority(pRecord);
    if ( iPriority == 0 ) {
        return 0.0;
    }

    return ((double)iPriority) * fWeight;
}

static int64 xllm__memory_record_updated_at(const xllm__memory_record_entry *pRecord)
{
    if ( !pRecord || !pRecord->tMetadata || xvoType(pRecord->tMetadata) != XVO_DT_TABLE ) {
        return 0;
    }

    return xvoTableGetInt(pRecord->tMetadata, (str)XLLM__MEMORY_METADATA_KEY_UPDATED_AT_UNIX, 0u);
}

static double xllm__memory_score_recency_boost(
    const xllm__memory_record_entry *pRecord,
    const xllm_memory_search_options *pOptions,
    int64 iNowUnix
)
{
    double fWeight;
    int64 iUpdatedAtUnix;
    double fAgeDays;

    if ( !pRecord || !pOptions || !pOptions->tRecencyWeight.bSet ) {
        return 0.0;
    }

    fWeight = pOptions->tRecencyWeight.fValue;
    if ( fWeight == 0.0 ) {
        return 0.0;
    }

    iUpdatedAtUnix = xllm__memory_record_updated_at(pRecord);
    if ( iUpdatedAtUnix <= 0 ) {
        return 0.0;
    }
    if ( iNowUnix <= 0 || iUpdatedAtUnix >= iNowUnix ) {
        return fWeight;
    }

    fAgeDays = ((double)(iNowUnix - iUpdatedAtUnix)) / 86400.0;
    if ( fAgeDays < 0.0 ) {
        fAgeDays = 0.0;
    }
    return fWeight / (1.0 + fAgeDays);
}

static bool xllm__memory_record_is_expired(
    const xllm__memory_record_entry *pRecord,
    int64 iNowUnix
)
{
    int64 iExpiresAtUnix;

    if ( !pRecord || !pRecord->tMetadata || xvoType(pRecord->tMetadata) != XVO_DT_TABLE ) {
        return false;
    }

    iExpiresAtUnix = xvoTableGetInt(pRecord->tMetadata, (str)XLLM__MEMORY_METADATA_KEY_EXPIRES_AT_UNIX, 0u);
    return iExpiresAtUnix > 0 && iExpiresAtUnix <= iNowUnix;
}

#include "xllm_memory_pipeline.c"

static int xllm__memory_record_info_clone(
    xllm_memory_record_info *pInfo,
    const xllm__memory_record_entry *pRecord
)
{
    if ( !pInfo || !pRecord ) {
        return XRT_NET_ERROR;
    }

    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->eScope = pRecord->eScope;
    pInfo->sRecordId = xllm__dup_cstr(pRecord->sRecordId);
    pInfo->sTitle = xllm__dup_cstr(pRecord->sTitle);
    pInfo->sSourceUri = xllm__dup_cstr(pRecord->sSourceUri);
    pInfo->sMemoryProfileId = xllm__dup_cstr(pRecord->sMemoryProfileId);
    pInfo->sRetrievalProfileId = xllm__dup_cstr(pRecord->sRetrievalProfileId);
    pInfo->sEmbedProfileId = xllm__dup_cstr(pRecord->sEmbedProfileId);
    pInfo->sIndexProfileId = xllm__dup_cstr(pRecord->sIndexProfileId);
    pInfo->iTextLength = pRecord->sText ? strlen(pRecord->sText) : 0u;
    pInfo->uChunkCount = (uint32)pRecord->iChunkCount;
    pInfo->uProfileVersion = pRecord->uProfileVersion;
    pInfo->tVendorExtra = pRecord->tVendorExtra;
    pInfo->tMetadata = xllm__memory_clone_public_metadata(pRecord->tMetadata);
    xllm__xvalue_addref(pInfo->tVendorExtra);
    if ( (pRecord->sRecordId && !pInfo->sRecordId) ||
         (pRecord->sTitle && !pInfo->sTitle) ||
         (pRecord->sSourceUri && !pInfo->sSourceUri) ||
         (pRecord->sMemoryProfileId && !pInfo->sMemoryProfileId) ||
         (pRecord->sRetrievalProfileId && !pInfo->sRetrievalProfileId) ||
         (pRecord->sEmbedProfileId && !pInfo->sEmbedProfileId) ||
         (pRecord->sIndexProfileId && !pInfo->sIndexProfileId) ||
         (pRecord->tMetadata && !pInfo->tMetadata) ) {
        xllm__memory_record_info_reset(pInfo);
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static void xllm__memory_record_info_array_reset(
    xllm_memory_record_info *pInfos,
    size_t iInfoCount
)
{
    size_t i;

    if ( !pInfos ) {
        return;
    }

    for ( i = 0u; i < iInfoCount; ++i ) {
        xllm__memory_record_info_reset(&pInfos[i]);
    }
    xrtFree(pInfos);
}

static void xllm__memory_skipped_file_info_array_reset(
    xllm_memory_skipped_file_info *pInfos,
    size_t iInfoCount
)
{
    size_t i;

    if ( !pInfos ) {
        return;
    }
    for ( i = 0u; i < iInfoCount; ++i ) {
        xllm__memory_skipped_file_info_reset(&pInfos[i]);
    }
    xrtFree(pInfos);
}

static void xllm__memory_failed_file_info_array_reset(
    xllm_memory_failed_file_info *pInfos,
    size_t iInfoCount
)
{
    size_t i;

    if ( !pInfos ) {
        return;
    }
    for ( i = 0u; i < iInfoCount; ++i ) {
        xllm__memory_failed_file_info_reset(&pInfos[i]);
    }
    xrtFree(pInfos);
}

static void xllm__memory_change_info_array_reset(
    xllm_memory_change_info *pInfos,
    size_t iInfoCount
)
{
    size_t i;

    if ( !pInfos ) {
        return;
    }
    for ( i = 0u; i < iInfoCount; ++i ) {
        xllm__memory_change_info_reset(&pInfos[i]);
    }
    xrtFree(pInfos);
}

static int xllm__memory_append_record_info_clone(
    xllm_memory_record_info **ppInfos,
    size_t *piInfoCount,
    const xllm__memory_record_entry *pRecord
)
{
    xllm_memory_record_info tInfo;
    xllm_memory_record_info *pNewInfos;

    if ( !ppInfos || !piInfoCount || !pRecord ) {
        return XRT_NET_ERROR;
    }

    memset(&tInfo, 0, sizeof(tInfo));
    if ( xllm__memory_record_info_clone(&tInfo, pRecord) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    pNewInfos = (xllm_memory_record_info *)xrtRealloc(*ppInfos, (*piInfoCount + 1u) * sizeof(*pNewInfos));
    if ( !pNewInfos ) {
        xllm__memory_record_info_reset(&tInfo);
        return XRT_NET_ERROR;
    }

    pNewInfos[*piInfoCount] = tInfo;
    *ppInfos = pNewInfos;
    ++(*piInfoCount);
    return XRT_NET_OK;
}

static int xllm__memory_record_info_copy_public(
    xllm_memory_record_info *pDest,
    const xllm_memory_record_info *pSrc
)
{
    if ( !pDest || !pSrc ) {
        return XRT_NET_ERROR;
    }

    memset(pDest, 0, sizeof(*pDest));
    pDest->eScope = pSrc->eScope;
    pDest->iTextLength = pSrc->iTextLength;
    pDest->uChunkCount = pSrc->uChunkCount;
    pDest->uProfileVersion = pSrc->uProfileVersion;
    pDest->sRecordId = xllm__dup_cstr(pSrc->sRecordId);
    pDest->sTitle = xllm__dup_cstr(pSrc->sTitle);
    pDest->sSourceUri = xllm__dup_cstr(pSrc->sSourceUri);
    pDest->sMemoryProfileId = xllm__dup_cstr(pSrc->sMemoryProfileId);
    pDest->sRetrievalProfileId = xllm__dup_cstr(pSrc->sRetrievalProfileId);
    pDest->sEmbedProfileId = xllm__dup_cstr(pSrc->sEmbedProfileId);
    pDest->sIndexProfileId = xllm__dup_cstr(pSrc->sIndexProfileId);
    pDest->tVendorExtra = pSrc->tVendorExtra;
    pDest->tMetadata = xllm__memory_clone_public_metadata(pSrc->tMetadata);
    xllm__xvalue_addref(pDest->tVendorExtra);
    if ( (pSrc->sRecordId && !pDest->sRecordId) ||
         (pSrc->sTitle && !pDest->sTitle) ||
         (pSrc->sSourceUri && !pDest->sSourceUri) ||
         (pSrc->sMemoryProfileId && !pDest->sMemoryProfileId) ||
         (pSrc->sRetrievalProfileId && !pDest->sRetrievalProfileId) ||
         (pSrc->sEmbedProfileId && !pDest->sEmbedProfileId) ||
         (pSrc->sIndexProfileId && !pDest->sIndexProfileId) ||
         (pSrc->tMetadata && !pDest->tMetadata) ) {
        xllm__memory_record_info_reset(pDest);
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static int xllm__memory_skipped_file_info_copy(
    xllm_memory_skipped_file_info *pDest,
    const xllm_memory_skipped_file_info *pSrc
)
{
    if ( !pDest || !pSrc ) {
        return XRT_NET_ERROR;
    }

    memset(pDest, 0, sizeof(*pDest));
    pDest->eReason = pSrc->eReason;
    pDest->uFileBytes = pSrc->uFileBytes;
    pDest->sPath = xllm__dup_cstr(pSrc->sPath);
    pDest->sRelativePath = xllm__dup_cstr(pSrc->sRelativePath);
    pDest->sSourceUri = xllm__dup_cstr(pSrc->sSourceUri);
    pDest->tVendorExtra = pSrc->tVendorExtra;
    xllm__xvalue_addref(pDest->tVendorExtra);
    if ( (pSrc->sPath && !pDest->sPath) ||
         (pSrc->sRelativePath && !pDest->sRelativePath) ||
         (pSrc->sSourceUri && !pDest->sSourceUri) ) {
        xllm__memory_skipped_file_info_reset(pDest);
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static int xllm__memory_failed_file_info_copy(
    xllm_memory_failed_file_info *pDest,
    const xllm_memory_failed_file_info *pSrc
)
{
    if ( !pDest || !pSrc ) {
        return XRT_NET_ERROR;
    }

    memset(pDest, 0, sizeof(*pDest));
    pDest->eReason = pSrc->eReason;
    pDest->eErrorCode = pSrc->eErrorCode;
    pDest->iStatus = pSrc->iStatus;
    pDest->uFileBytes = pSrc->uFileBytes;
    pDest->sPath = xllm__dup_cstr(pSrc->sPath);
    pDest->sRelativePath = xllm__dup_cstr(pSrc->sRelativePath);
    pDest->sSourceUri = xllm__dup_cstr(pSrc->sSourceUri);
    pDest->sMessage = xllm__dup_cstr(pSrc->sMessage);
    pDest->tVendorExtra = pSrc->tVendorExtra;
    xllm__xvalue_addref(pDest->tVendorExtra);
    if ( (pSrc->sPath && !pDest->sPath) ||
         (pSrc->sRelativePath && !pDest->sRelativePath) ||
         (pSrc->sSourceUri && !pDest->sSourceUri) ||
         (pSrc->sMessage && !pDest->sMessage) ) {
        xllm__memory_failed_file_info_reset(pDest);
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static int xllm__memory_append_change_record(
    xllm_memory_change_info **ppChanges,
    size_t *piChangeCount,
    xllm_memory_change_kind eKind,
    const xllm_memory_record_info *pRecord
)
{
    xllm_memory_change_info tInfo;
    xllm_memory_change_info *pNewItems;

    if ( !ppChanges || !piChangeCount || !pRecord ) {
        return XRT_NET_ERROR;
    }

    memset(&tInfo, 0, sizeof(tInfo));
    tInfo.eKind = eKind;
    if ( xllm__memory_record_info_copy_public(&tInfo.tRecord, pRecord) != XRT_NET_OK ) {
        xllm__memory_change_info_reset(&tInfo);
        return XRT_NET_ERROR;
    }

    pNewItems = (xllm_memory_change_info *)xrtRealloc(*ppChanges, (*piChangeCount + 1u) * sizeof(*pNewItems));
    if ( !pNewItems ) {
        xllm__memory_change_info_reset(&tInfo);
        return XRT_NET_ERROR;
    }

    pNewItems[*piChangeCount] = tInfo;
    *ppChanges = pNewItems;
    ++(*piChangeCount);
    return XRT_NET_OK;
}

static int xllm__memory_append_change_skipped(
    xllm_memory_change_info **ppChanges,
    size_t *piChangeCount,
    const xllm_memory_skipped_file_info *pSkipped
)
{
    xllm_memory_change_info tInfo;
    xllm_memory_change_info *pNewItems;

    if ( !ppChanges || !piChangeCount || !pSkipped ) {
        return XRT_NET_ERROR;
    }

    memset(&tInfo, 0, sizeof(tInfo));
    tInfo.eKind = XLLM_MEMORY_CHANGE_SKIPPED;
    if ( xllm__memory_skipped_file_info_copy(&tInfo.tSkipped, pSkipped) != XRT_NET_OK ) {
        xllm__memory_change_info_reset(&tInfo);
        return XRT_NET_ERROR;
    }

    pNewItems = (xllm_memory_change_info *)xrtRealloc(*ppChanges, (*piChangeCount + 1u) * sizeof(*pNewItems));
    if ( !pNewItems ) {
        xllm__memory_change_info_reset(&tInfo);
        return XRT_NET_ERROR;
    }

    pNewItems[*piChangeCount] = tInfo;
    *ppChanges = pNewItems;
    ++(*piChangeCount);
    return XRT_NET_OK;
}

static int xllm__memory_append_change_failed(
    xllm_memory_change_info **ppChanges,
    size_t *piChangeCount,
    const xllm_memory_failed_file_info *pFailed
)
{
    xllm_memory_change_info tInfo;
    xllm_memory_change_info *pNewItems;

    if ( !ppChanges || !piChangeCount || !pFailed ) {
        return XRT_NET_ERROR;
    }

    memset(&tInfo, 0, sizeof(tInfo));
    tInfo.eKind = XLLM_MEMORY_CHANGE_FAILED;
    if ( xllm__memory_failed_file_info_copy(&tInfo.tFailed, pFailed) != XRT_NET_OK ) {
        xllm__memory_change_info_reset(&tInfo);
        return XRT_NET_ERROR;
    }

    pNewItems = (xllm_memory_change_info *)xrtRealloc(*ppChanges, (*piChangeCount + 1u) * sizeof(*pNewItems));
    if ( !pNewItems ) {
        xllm__memory_change_info_reset(&tInfo);
        return XRT_NET_ERROR;
    }

    pNewItems[*piChangeCount] = tInfo;
    *ppChanges = pNewItems;
    ++(*piChangeCount);
    return XRT_NET_OK;
}

static int xllm__memory_append_change_copy(
    xllm_memory_change_info **ppChanges,
    size_t *piChangeCount,
    const xllm_memory_change_info *pChange
)
{
    if ( !ppChanges || !piChangeCount || !pChange ) {
        return XRT_NET_ERROR;
    }

    switch ( pChange->eKind ) {
        case XLLM_MEMORY_CHANGE_CREATED:
        case XLLM_MEMORY_CHANGE_UPDATED:
        case XLLM_MEMORY_CHANGE_REMOVED:
            return xllm__memory_append_change_record(ppChanges, piChangeCount, pChange->eKind, &pChange->tRecord);
        case XLLM_MEMORY_CHANGE_SKIPPED:
            return xllm__memory_append_change_skipped(ppChanges, piChangeCount, &pChange->tSkipped);
        case XLLM_MEMORY_CHANGE_FAILED:
            return xllm__memory_append_change_failed(ppChanges, piChangeCount, &pChange->tFailed);
        default:
            return XRT_NET_ERROR;
    }
}

static int xllm__memory_append_change_set_copy(
    xllm_memory_change_set *pDest,
    const xllm_memory_change_set *pSrc
)
{
    size_t i;

    if ( !pDest || !pSrc ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < pSrc->iChangeCount; ++i ) {
        if ( xllm__memory_append_change_copy(&pDest->pChanges, &pDest->iChangeCount, &pSrc->pChanges[i]) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
    }
    return XRT_NET_OK;
}

static int xllm__memory_append_sync_file_failure_change(
    xllm_memory_change_set *pResult,
    const xllm_memory_sync_file_options *pOptions,
    int iStatus,
    const xllm_error *pError
)
{
    xllm_memory_failed_file_info tInfo;
    const char *sSourceUriPrefix = NULL;

    if ( !pResult ) {
        return XRT_NET_ERROR;
    }

    memset(&tInfo, 0, sizeof(tInfo));
    tInfo.eReason = XLLM_MEMORY_FAIL_SYNC_FILE;
    tInfo.eErrorCode = pError ? pError->eCode : XLLM_ERROR_INTERNAL;
    tInfo.iStatus = iStatus;

    if ( pOptions ) {
        if ( pOptions->bUseWorkspaceDefaults &&
             (!pOptions->sSourceUriPrefix || !pOptions->sSourceUriPrefix[0]) ) {
            sSourceUriPrefix = XLLM__MEMORY_WORKSPACE_DEFAULT_SOURCE_URI_PREFIX;
        } else {
            sSourceUriPrefix = pOptions->sSourceUriPrefix;
        }

        tInfo.sPath = xllm__dup_cstr(pOptions->sPath);
        if ( pOptions->sPath && pOptions->sPath[0] ) {
            if ( pOptions->sRootPath && pOptions->sRootPath[0] ) {
                tInfo.sRelativePath = xllm__memory_dup_relative_path(pOptions->sRootPath, pOptions->sPath);
            }
            if ( !tInfo.sRelativePath ) {
                tInfo.sRelativePath = xllm__dup_cstr(pOptions->sPath);
            }
        }
        if ( pOptions->sSourceUri && pOptions->sSourceUri[0] ) {
            tInfo.sSourceUri = xllm__dup_cstr(pOptions->sSourceUri);
        } else if ( pOptions->sPath && pOptions->sPath[0] && sSourceUriPrefix && sSourceUriPrefix[0] ) {
            tInfo.sSourceUri = xllm__memory_make_prefixed_relative_source_uri(
                sSourceUriPrefix,
                pOptions->sRootPath,
                pOptions->sPath
            );
        } else if ( pOptions->sPath && pOptions->sPath[0] ) {
            tInfo.sSourceUri = xllm__memory_make_file_source_uri(pOptions->sPath);
        }
    }
    tInfo.sMessage = xllm__dup_cstr((pError && pError->sMessage) ? pError->sMessage : "memory sync file batch item failed");
    if ( (pOptions && pOptions->sPath && pOptions->sPath[0] && !tInfo.sPath) ||
         (pOptions && pOptions->sPath && pOptions->sPath[0] && !tInfo.sRelativePath) ||
         ((pOptions && pOptions->sSourceUri && pOptions->sSourceUri[0]) && !tInfo.sSourceUri) ||
         !tInfo.sMessage ) {
        xllm__memory_failed_file_info_reset(&tInfo);
        return XRT_NET_ERROR;
    }

    if ( xllm__memory_append_change_failed(&pResult->pChanges, &pResult->iChangeCount, &tInfo) != XRT_NET_OK ) {
        xllm__memory_failed_file_info_reset(&tInfo);
        return XRT_NET_ERROR;
    }

    xllm__memory_failed_file_info_reset(&tInfo);
    return XRT_NET_OK;
}

static int xllm__memory_sync_file_batch_handle_failure(
    xllm_memory_change_set *pAccumulated,
    const xllm_memory_sync_file_options *pOptions,
    bool bContinueOnError,
    int iStatus,
    const xllm_error *pItemError,
    xllm_error *pError
)
{
    if ( !bContinueOnError ) {
        if ( pItemError && (pItemError->eCode != XLLM_ERROR_NONE || (pItemError->sMessage && pItemError->sMessage[0])) ) {
            xllm__error_set(pError,
                            pItemError->eCode != XLLM_ERROR_NONE ? pItemError->eCode : XLLM_ERROR_INTERNAL,
                            pItemError->sMessage ? pItemError->sMessage : "memory sync item failed");
        } else {
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "memory sync item failed");
        }
        return iStatus;
    }

    if ( xllm__memory_append_sync_file_failure_change(pAccumulated, pOptions, iStatus, pItemError) != XRT_NET_OK ) {
        xllm_memory_change_set_reset(pAccumulated);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to append memory sync item failure entry");
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static int xllm__memory_sync_file_batch_run_one(
    xllm_memory *pMemory,
    const xllm_memory_sync_file_options *pOptions,
    bool bContinueOnError,
    xllm_memory_change_set *pAccumulated,
    xllm_error *pError
)
{
    xllm_memory_change_set tItemChanges;
    xllm_error tItemError;
    int iStatus;

    xllm_memory_change_set_init(&tItemChanges);
    xllm_error_init(&tItemError);

    iStatus = xllm_memory_sync_file(pMemory, pOptions, &tItemChanges, &tItemError);
    if ( iStatus != XRT_NET_OK ) {
        iStatus = xllm__memory_sync_file_batch_handle_failure(
            pAccumulated,
            pOptions,
            bContinueOnError,
            iStatus,
            &tItemError,
            pError
        );
        xllm_memory_change_set_reset(&tItemChanges);
        xllm_error_free(&tItemError);
        return iStatus;
    }

    if ( xllm__memory_append_change_set_copy(pAccumulated, &tItemChanges) != XRT_NET_OK ) {
        xllm_memory_change_set_reset(&tItemChanges);
        xllm_error_free(&tItemError);
        xllm_memory_change_set_reset(pAccumulated);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to append memory sync item change set");
        return XRT_NET_ERROR;
    }

    xllm_memory_change_set_reset(&tItemChanges);
    xllm_error_free(&tItemError);
    return XRT_NET_OK;
}

static int xllm__memory_append_skipped_file_info(
    xllm_memory_skipped_file_info **ppInfos,
    size_t *piInfoCount,
    const char *sRootPath,
    const char *sPath,
    const char *sRelativePathHint,
    const char *sSourceUriPrefix,
    xllm_memory_skip_reason eReason,
    uint64 uFileBytes
)
{
    xllm_memory_skipped_file_info tInfo;
    xllm_memory_skipped_file_info *pNewInfos;

    if ( !ppInfos || !piInfoCount || !sPath || !sPath[0] ) {
        return XRT_NET_ERROR;
    }

    memset(&tInfo, 0, sizeof(tInfo));
    tInfo.sPath = xllm__dup_cstr(sPath);
    if ( !tInfo.sPath ) {
        return XRT_NET_ERROR;
    }
    if ( sRelativePathHint && sRelativePathHint[0] ) {
        tInfo.sRelativePath = xllm__dup_cstr(sRelativePathHint);
    } else {
        tInfo.sRelativePath = xllm__memory_dup_relative_path(sRootPath, sPath);
    }
    if ( !tInfo.sRelativePath ) {
        xllm__memory_skipped_file_info_reset(&tInfo);
        return XRT_NET_ERROR;
    }
    if ( sSourceUriPrefix && sSourceUriPrefix[0] ) {
        tInfo.sSourceUri = xllm__memory_make_prefixed_relative_source_uri(sSourceUriPrefix, sRootPath, sPath);
        if ( !tInfo.sSourceUri ) {
            xllm__memory_skipped_file_info_reset(&tInfo);
            return XRT_NET_ERROR;
        }
    }
    tInfo.eReason = eReason;
    tInfo.uFileBytes = uFileBytes;

    pNewInfos = (xllm_memory_skipped_file_info *)xrtRealloc(*ppInfos, (*piInfoCount + 1u) * sizeof(*pNewInfos));
    if ( !pNewInfos ) {
        xllm__memory_skipped_file_info_reset(&tInfo);
        return XRT_NET_ERROR;
    }

    pNewInfos[*piInfoCount] = tInfo;
    *ppInfos = pNewInfos;
    ++(*piInfoCount);
    return XRT_NET_OK;
}

static int xllm__memory_append_failed_file_info(
    xllm_memory_failed_file_info **ppInfos,
    size_t *piInfoCount,
    const char *sRootPath,
    const char *sPath,
    const char *sRelativePathHint,
    const char *sSourceUriPrefix,
    xllm_memory_fail_reason eReason,
    uint64 uFileBytes,
    const xllm_error *pError
)
{
    xllm_memory_failed_file_info tInfo;
    xllm_memory_failed_file_info *pNewInfos;

    if ( !ppInfos || !piInfoCount || !sPath || !sPath[0] ) {
        return XRT_NET_ERROR;
    }

    memset(&tInfo, 0, sizeof(tInfo));
    tInfo.sPath = xllm__dup_cstr(sPath);
    if ( !tInfo.sPath ) {
        return XRT_NET_ERROR;
    }
    if ( sRelativePathHint && sRelativePathHint[0] ) {
        tInfo.sRelativePath = xllm__dup_cstr(sRelativePathHint);
    } else {
        tInfo.sRelativePath = xllm__memory_dup_relative_path(sRootPath, sPath);
    }
    if ( !tInfo.sRelativePath ) {
        xllm__memory_failed_file_info_reset(&tInfo);
        return XRT_NET_ERROR;
    }
    if ( sSourceUriPrefix && sSourceUriPrefix[0] ) {
        tInfo.sSourceUri = xllm__memory_make_prefixed_relative_source_uri(sSourceUriPrefix, sRootPath, sPath);
        if ( !tInfo.sSourceUri ) {
            xllm__memory_failed_file_info_reset(&tInfo);
            return XRT_NET_ERROR;
        }
    }
    tInfo.eReason = eReason;
    tInfo.uFileBytes = uFileBytes;
    if ( pError ) {
        tInfo.eErrorCode = pError->eCode;
        tInfo.iStatus = pError->iStatus;
        if ( pError->sMessage && pError->sMessage[0] ) {
            tInfo.sMessage = xllm__dup_cstr(pError->sMessage);
            if ( !tInfo.sMessage ) {
                xllm__memory_failed_file_info_reset(&tInfo);
                return XRT_NET_ERROR;
            }
        }
    }

    pNewInfos = (xllm_memory_failed_file_info *)xrtRealloc(*ppInfos, (*piInfoCount + 1u) * sizeof(*pNewInfos));
    if ( !pNewInfos ) {
        xllm__memory_failed_file_info_reset(&tInfo);
        return XRT_NET_ERROR;
    }

    pNewInfos[*piInfoCount] = tInfo;
    *ppInfos = pNewInfos;
    ++(*piInfoCount);
    return XRT_NET_OK;
}

static int xllm__memory_chunk_info_clone(
    xllm_memory_chunk_info *pInfo,
    const xllm__memory_record_entry *pRecord,
    const xllm__memory_chunk_entry *pChunk,
    uint32 uMaxCharsPerText
)
{
    size_t iLength;

    if ( !pInfo || !pRecord || !pChunk ) {
        return XRT_NET_ERROR;
    }

    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->eScope = pRecord->eScope;
    pInfo->sRecordId = xllm__dup_cstr(pRecord->sRecordId);
    pInfo->sChunkId = xllm__dup_cstr(pChunk->sChunkId);
    pInfo->sTitle = xllm__dup_cstr(pRecord->sTitle);
    pInfo->sSourceUri = xllm__dup_cstr(pRecord->sSourceUri);
    pInfo->sMemoryProfileId = xllm__dup_cstr(pChunk->sMemoryProfileId);
    pInfo->sChunkProfileId = xllm__dup_cstr(pChunk->sChunkProfileId);
    pInfo->sRetrievalProfileId = xllm__dup_cstr(pChunk->sRetrievalProfileId);
    pInfo->sEmbedProfileId = xllm__dup_cstr(pChunk->sEmbedProfileId);
    pInfo->sIndexProfileId = xllm__dup_cstr(pChunk->sIndexProfileId);
    pInfo->sPreviousChunkId = xllm__dup_cstr(pChunk->sPreviousChunkId);
    pInfo->sNextChunkId = xllm__dup_cstr(pChunk->sNextChunkId);
    pInfo->iStartByte = pChunk->iStartByte;
    pInfo->iEndByte = pChunk->iEndByte;
    pInfo->uContentHash = pChunk->uContentHash;
    pInfo->uChunkIndex = pChunk->uChunkIndex;
    pInfo->uEmbeddingDim = pChunk->uEmbeddingDim;
    pInfo->uProfileVersion = pChunk->uProfileVersion;
    pInfo->tVendorExtra = pRecord->tVendorExtra;
    pInfo->tMetadata = xllm__memory_clone_public_metadata(pRecord->tMetadata);
    xllm__xvalue_addref(pInfo->tVendorExtra);

    iLength = pChunk->sText ? strlen(pChunk->sText) : 0u;
    pInfo->iTextLength = iLength;
    if ( uMaxCharsPerText > 0u && iLength > (size_t)uMaxCharsPerText ) {
        iLength = (size_t)uMaxCharsPerText;
    }
    pInfo->sText = (char *)xrtCalloc(iLength + 1u, sizeof(char));
    if ( !pInfo->sText ) {
        xllm__memory_chunk_info_reset(pInfo);
        return XRT_NET_ERROR;
    }
    if ( iLength > 0u && pChunk->sText ) {
        memcpy((char *)pInfo->sText, pChunk->sText, iLength);
    }
    if ( (pRecord->sRecordId && !pInfo->sRecordId) ||
         (pChunk->sChunkId && !pInfo->sChunkId) ||
         (pRecord->sTitle && !pInfo->sTitle) ||
         (pRecord->sSourceUri && !pInfo->sSourceUri) ||
         (pChunk->sMemoryProfileId && !pInfo->sMemoryProfileId) ||
         (pChunk->sChunkProfileId && !pInfo->sChunkProfileId) ||
         (pChunk->sRetrievalProfileId && !pInfo->sRetrievalProfileId) ||
         (pChunk->sEmbedProfileId && !pInfo->sEmbedProfileId) ||
         (pChunk->sIndexProfileId && !pInfo->sIndexProfileId) ||
         (pChunk->sPreviousChunkId && !pInfo->sPreviousChunkId) ||
         (pChunk->sNextChunkId && !pInfo->sNextChunkId) ||
         (pRecord->tMetadata && !pInfo->tMetadata) ) {
        xllm__memory_chunk_info_reset(pInfo);
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static int xllm__memory_make_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_embedder *pEmbedder,
    xllm_error *pError
);

#include "xllm_memory_sqlite.c"
#include "../xllm_chunk/xllm_chunk.c"
#include "../xllm_embed/xllm_embed.c"
#if XLLM__MEMORY_HAS_SCHEME_ONNX_E5
#include "../xllm_embed/xllm_embed_e5_onnx.c"
#include "../xllm_embed/xllm_embed_builtin.c"
#else
static void xllm__memory_builtin_embedder_options_init(
    xllm_memory_builtin_embedder_options *pOptions
)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_NONE;
    pOptions->bAutoDiscoverAssets = true;
    pOptions->uHashDimensions = 16u;
}

static void xllm__memory_builtin_embedder_probe_reset(
    xllm_memory_builtin_embedder_probe *pProbe
)
{
    if ( !pProbe ) {
        return;
    }

    xllm__free_cstr(&pProbe->sResolvedRuntimeDllPath);
    xllm__free_cstr(&pProbe->sResolvedModelPath);
    xllm__free_cstr(&pProbe->sResolvedTokenizerPath);
    xllm__free_cstr(&pProbe->sMessage);
    xllm__xvalue_release(&pProbe->tVendorExtra);
    memset(pProbe, 0, sizeof(*pProbe));
}

static int xllm__memory_probe_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_builtin_embedder_probe *pProbe,
    xllm_error *pError
)
{
    xllm_memory_builtin_embedder_options tDefaultOptions;
    const xllm_memory_builtin_embedder_options *pUseOptions = pOptions;

    if ( !pProbe ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "builtin embedder probe output is required");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm__memory_builtin_embedder_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    xllm__memory_builtin_embedder_probe_reset(pProbe);
    pProbe->eKind = pUseOptions->eKind;
    pProbe->tVendorExtra = pUseOptions->tVendorExtra;
    xllm__xvalue_addref(pProbe->tVendorExtra);
    pProbe->sMessage = xllm__dup_cstr("builtin embedders are unavailable in this build");
    xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "builtin embedders are unavailable in this build");
    return XRT_NET_ERROR;
}

static int xllm__memory_make_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_embedder *pEmbedder,
    xllm_error *pError
)
{
    (void)pOptions;

    if ( pEmbedder ) {
        xllm_memory_embedder_reset(pEmbedder);
    }
    xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "builtin embedders are unavailable in this build");
    return XRT_NET_ERROR;
}
#endif

#include "xllm_memory_facade.c"
#include "xllm_memory_typed.c"
#include "xllm_memory_files.c"

static int xllm__memory_validate_file_event(
    const xllm_memory_file_event *pEvent,
    xllm_error *pError
)
{
    if ( !pEvent ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory file event is required");
        return XRT_NET_ERROR;
    }

    switch ( pEvent->eKind ) {
        case XLLM_MEMORY_FILE_EVENT_CREATED:
        case XLLM_MEMORY_FILE_EVENT_UPDATED:
        case XLLM_MEMORY_FILE_EVENT_DELETED:
            if ( !pEvent->sPath || !pEvent->sPath[0] ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory file event requires path");
                return XRT_NET_ERROR;
            }
            return XRT_NET_OK;
        case XLLM_MEMORY_FILE_EVENT_RENAMED:
            if ( !pEvent->sPreviousPath || !pEvent->sPreviousPath[0] ||
                 !pEvent->sPath || !pEvent->sPath[0] ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory rename event requires previous and current path");
                return XRT_NET_ERROR;
            }
            return XRT_NET_OK;
        default:
            xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory file event kind is unsupported");
            return XRT_NET_ERROR;
    }
}

#include "xllm_memory_watcher.c"
