#ifndef XLLM_MEMORY_H
#define XLLM_MEMORY_H

#if defined(XLLM_MEMORY_IMPLEMENTATION) && !defined(XLLM_IMPLEMENTATION)
#define XLLM__MEMORY_BRIDGED_IMPLEMENTATION
#define XLLM_IMPLEMENTATION
#endif

#include "xllm.h"

#ifdef XLLM__MEMORY_BRIDGED_IMPLEMENTATION
#undef XLLM_IMPLEMENTATION
#undef XLLM__MEMORY_BRIDGED_IMPLEMENTATION
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define XLLM_MEMORY_SCHEME_MODE_ONNX_E5 1
#define XLLM_MEMORY_SCHEME_MODE_BUILTIN_SPARSE 2
#define XLLM_MEMORY_SCHEME_MODE_CUSTOM 3
#define XLLM_MEMORY_SCHEME_MODE_ALL 4

#ifndef XLLM_MEMORY_SCHEME_MODE
#define XLLM_MEMORY_SCHEME_MODE XLLM_MEMORY_SCHEME_MODE_ALL
#endif

#if XLLM_MEMORY_SCHEME_MODE == XLLM_MEMORY_SCHEME_MODE_ONNX_E5
#define XLLM__MEMORY_HAS_SCHEME_ONNX_E5 1
#define XLLM__MEMORY_HAS_SCHEME_BUILTIN_SPARSE 0
#define XLLM__MEMORY_HAS_SCHEME_CUSTOM 0
#elif XLLM_MEMORY_SCHEME_MODE == XLLM_MEMORY_SCHEME_MODE_BUILTIN_SPARSE
#define XLLM__MEMORY_HAS_SCHEME_ONNX_E5 0
#define XLLM__MEMORY_HAS_SCHEME_BUILTIN_SPARSE 1
#define XLLM__MEMORY_HAS_SCHEME_CUSTOM 0
#elif XLLM_MEMORY_SCHEME_MODE == XLLM_MEMORY_SCHEME_MODE_CUSTOM
#define XLLM__MEMORY_HAS_SCHEME_ONNX_E5 0
#define XLLM__MEMORY_HAS_SCHEME_BUILTIN_SPARSE 0
#define XLLM__MEMORY_HAS_SCHEME_CUSTOM 1
#elif XLLM_MEMORY_SCHEME_MODE == XLLM_MEMORY_SCHEME_MODE_ALL
#define XLLM__MEMORY_HAS_SCHEME_ONNX_E5 1
#define XLLM__MEMORY_HAS_SCHEME_BUILTIN_SPARSE 1
#define XLLM__MEMORY_HAS_SCHEME_CUSTOM 1
#else
#error "XLLM_MEMORY_SCHEME_MODE must be one of XLLM_MEMORY_SCHEME_MODE_ONNX_E5, XLLM_MEMORY_SCHEME_MODE_BUILTIN_SPARSE, XLLM_MEMORY_SCHEME_MODE_CUSTOM, or XLLM_MEMORY_SCHEME_MODE_ALL"
#endif

typedef struct xllm_memory xllm_memory;
typedef struct xllm_memory_hit xllm_memory_hit;
typedef struct xllm_memory_search_result xllm_memory_search_result;

typedef int (*xllm_memory_context_render_fn)(
    void *pCtx,
    const xllm_memory_search_result *pResult,
    const size_t *pSelectedHitIndices,
    const size_t *pTextLengths,
    size_t iHitCount,
    char **psText,
    xllm_error *pError
);
typedef struct xllm_memory_file_event_queue xllm_memory_file_event_queue;
typedef struct xllm_memory_watcher_bridge xllm_memory_watcher_bridge;
typedef struct xllm_memory_watcher_pump xllm_memory_watcher_pump;
typedef struct xllm_memory_watcher_worker xllm_memory_watcher_worker;

typedef enum {
    XLLM_MEMORY_SCHEME_AUTO = 0,
    XLLM_MEMORY_SCHEME_BUILTIN_SPARSE,
    XLLM_MEMORY_SCHEME_ONNX_E5,
    XLLM_MEMORY_SCHEME_CUSTOM
} xllm_memory_scheme;

typedef enum {
    XLLM_MEMORY_SCOPE_ANY = 0,
    XLLM_MEMORY_SCOPE_MEMORY,
    XLLM_MEMORY_SCOPE_KNOWLEDGE
} xllm_memory_scope;

typedef enum {
    XLLM_MEMORY_EXTRACTION_POLICY_DEFAULT = 0,
    XLLM_MEMORY_EXTRACTION_POLICY_NONE,
    XLLM_MEMORY_EXTRACTION_POLICY_TURN_RESPONSE,
    XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY,
    XLLM_MEMORY_EXTRACTION_POLICY_TASK,
    XLLM_MEMORY_EXTRACTION_POLICY_FACT,
    XLLM_MEMORY_EXTRACTION_POLICY_PREFERENCE
} xllm_memory_extraction_policy;

typedef enum {
    XLLM_MEMORY_TASK_STATUS_OPEN = 0,
    XLLM_MEMORY_TASK_STATUS_DONE,
    XLLM_MEMORY_TASK_STATUS_CANCELED
} xllm_memory_task_status;

typedef enum {
    XLLM_MEMORY_EMBED_QUERY = 1,
    XLLM_MEMORY_EMBED_DOCUMENT
} xllm_memory_embed_task;

typedef struct {
    float *pfValues;
    uint32 uValueCount;
} xllm_memory_embedding;

typedef int (*xllm_memory_embed_clone_fn)(
    void *pCtx,
    void **ppClonedCtx,
    xllm_error *pError
);

typedef int (*xllm_memory_embed_text_fn)(
    void *pCtx,
    xllm_memory_embed_task eTask,
    const char *sText,
    xllm_memory_embedding *pEmbedding,
    xllm_error *pError
);

typedef void (*xllm_memory_embed_reset_fn)(
    void *pCtx,
    xllm_memory_embedding *pEmbedding
);

typedef void (*xllm_memory_embed_dispose_fn)(
    void *pCtx
);

typedef struct {
    xllm_memory_embed_text_fn pfnEmbedText;
    xllm_memory_embed_reset_fn pfnResetEmbedding;
    xllm_memory_embed_clone_fn pfnCloneCtx;
    xllm_memory_embed_dispose_fn pfnDisposeCtx;
    void *pCtx;
    xvalue tVendorExtra;
} xllm_memory_embedder;

typedef enum {
    XLLM_MEMORY_BUILTIN_EMBEDDER_NONE = 0,
    XLLM_MEMORY_BUILTIN_EMBEDDER_HASH,
    XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX
} xllm_memory_builtin_embedder_kind;

typedef struct {
    xllm_memory_builtin_embedder_kind eKind;
    const char *sRuntimeDllPath;
    const char *sModelPath;
    const char *sTokenizerPath;
    bool bAutoDiscoverAssets;
    uint32 uHashDimensions;
    xvalue tVendorExtra;
} xllm_memory_builtin_embedder_options;

typedef struct {
    xllm_memory_builtin_embedder_kind eKind;
    bool bRuntimeFound;
    bool bModelFound;
    bool bTokenizerFound;
    bool bImplemented;
    bool bReady;
    char *sResolvedRuntimeDllPath;
    char *sResolvedModelPath;
    char *sResolvedTokenizerPath;
    char *sMessage;
    xvalue tVendorExtra;
} xllm_memory_builtin_embedder_probe;

typedef struct {
    const char *sNamespace;
    const char *sSqlitePath;
    const char *sSqliteVectorExtensionPath;
    bool bLoadSqliteVectorExtension;
    xllm_memory_embedder tEmbedder;
    xllm_memory_scheme eScheme;
    const char *sMemoryProfileId;
    bool bEnableHybridSearch;
    xllm_opt_f64 tLexicalWeight;
    xllm_opt_f64 tVectorWeight;
    uint32 uDefaultChunkChars;
    uint32 uDefaultChunkOverlapChars;
    uint32 uDefaultMaxHits;
    uint32 uSqliteBusyTimeoutMs;
    bool bDisableSqliteWal;
    xvalue tVendorExtra;
} xllm_memory_options;

typedef struct {
    xllm_memory_scope eScope;
    const char *sRecordId;
    const char *sTitle;
    const char *sSourceUri;
    const char *sText;
    bool bReplaceExisting;
    uint32 uChunkChars;
    uint32 uChunkOverlapChars;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_ingest_options;

typedef struct {
    xllm_memory_scope eScope;
    xllm_memory_extraction_policy eExtractionPolicy;
    const xllm_turn *pTurn;
    const xllm_response *pResponse;
    const char *sSummaryText;
    const char *sConversationId;
    const char *sTurnId;
    const char *sRecordId;
    const char *sTitle;
    const char *sSourceUri;
    bool bReplaceExisting;
    bool bUseStableIdentity;
    int32 iPriority;
    int64 iExpiresAtUnix;
    int64 iUpdatedAtUnix;
    bool bIncludeSystemPrompt;
    bool bIncludeContextBlocks;
    bool bIncludeThinking;
    uint32 uChunkChars;
    uint32 uChunkOverlapChars;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_ingest_turn_response_options;

typedef struct {
    xllm_memory_scope eScope;
    xllm_memory_task_status eStatus;
    const char *sTaskId;
    const char *sOwner;
    int64 iDeadlineUnix;
    const char *sSourceConversationId;
    const char *sSourceTurnId;
    const char *sRecordId;
    const char *sTitle;
    const char *sSourceUri;
    const char *sText;
    bool bReplaceExisting;
    int32 iPriority;
    int64 iExpiresAtUnix;
    int64 iUpdatedAtUnix;
    uint32 uChunkChars;
    uint32 uChunkOverlapChars;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_ingest_task_options;

typedef struct {
    xllm_memory_scope eScope;
    const char *sFactId;
    const char *sSubject;
    const char *sPredicate;
    const char *sObject;
    const char *sSourceConversationId;
    const char *sSourceTurnId;
    const char *sRecordId;
    const char *sTitle;
    const char *sSourceUri;
    const char *sText;
    bool bReplaceExisting;
    int32 iPriority;
    int64 iExpiresAtUnix;
    int64 iUpdatedAtUnix;
    uint32 uChunkChars;
    uint32 uChunkOverlapChars;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_ingest_fact_options;

typedef struct {
    xllm_memory_scope eScope;
    const char *sPreferenceId;
    const char *sSubject;
    const char *sKey;
    const char *sValue;
    const char *sSourceConversationId;
    const char *sSourceTurnId;
    const char *sRecordId;
    const char *sTitle;
    const char *sSourceUri;
    const char *sText;
    bool bReplaceExisting;
    int32 iPriority;
    int64 iExpiresAtUnix;
    int64 iUpdatedAtUnix;
    uint32 uChunkChars;
    uint32 uChunkOverlapChars;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_ingest_preference_options;

typedef struct {
    xllm_memory_scope eScope;
    const char *sSourceUriPrefix;
    const char *sMetadataKey;
    int64 iNowUnix;
    xvalue tVendorExtra;
} xllm_memory_remove_expired_options;

typedef struct {
    xllm_memory_scope eScope;
    const char *sMetadataKey;
    const char *sMetadataValue;
    xvalue tVendorExtra;
} xllm_memory_remove_by_metadata_options;

typedef struct {
    xllm_memory_scope eScope;
    const char *sConversationId;
    uint32 uKeepLatestRecords;
    uint64 uMaxTotalChars;
    uint32 uMaxTotalChunks;
    xllm_opt_bool tPreferPriorityDesc;
    xvalue tVendorExtra;
} xllm_memory_trim_conversation_options;

typedef struct {
    xllm_memory_scope eScope;
    const char *sConversationId;
    const char *sSummaryText;
    const char *sSummaryRecordId;
    const char *sSummaryTitle;
    const char *sSummarySourceUri;
    bool bReplaceSummary;
    bool bRemoveSourceRecords;
    uint32 uMaxSourceRecords;
    int32 iSummaryPriority;
    int64 iSummaryUpdatedAtUnix;
    uint32 uSummaryChunkChars;
    uint32 uSummaryChunkOverlapChars;
    xvalue tSummaryMetadata;
    xvalue tVendorExtra;
} xllm_memory_compact_conversation_options;

typedef struct {
    uint32 uMatchedRecordCount;
    uint32 uCompactedRecordCount;
    uint32 uRemovedRecordCount;
    xvalue tVendorExtra;
} xllm_memory_compact_conversation_result;

typedef struct {
    xllm_memory_scope eScope;
    const char *sPath;
    const char *sRecordId;
    const char *sTitle;
    const char *sSourceUri;
    bool bReplaceExisting;
    uint64 uMaxFileBytes;
    uint32 uChunkChars;
    uint32 uChunkOverlapChars;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_ingest_file_options;

typedef enum {
    XLLM_MEMORY_INGEST_PROGRESS_VISITED = 1,
    XLLM_MEMORY_INGEST_PROGRESS_INGESTED,
    XLLM_MEMORY_INGEST_PROGRESS_SKIPPED,
    XLLM_MEMORY_INGEST_PROGRESS_FAILED
} xllm_memory_ingest_progress_kind;

typedef enum {
    XLLM_MEMORY_SKIP_HIDDEN = 1,
    XLLM_MEMORY_SKIP_IGNORED_DIRECTORY,
    XLLM_MEMORY_SKIP_IGNORED_PATH_PATTERN,
    XLLM_MEMORY_SKIP_IGNORED_EXTENSION,
    XLLM_MEMORY_SKIP_DISALLOWED_EXTENSION,
    XLLM_MEMORY_SKIP_TOO_LARGE,
    XLLM_MEMORY_SKIP_UNCHANGED,
    XLLM_MEMORY_SKIP_SECRET_DETECTED
} xllm_memory_skip_reason;

typedef enum {
    XLLM_MEMORY_FAIL_GET_SIZE = 1,
    XLLM_MEMORY_FAIL_GET_MTIME,
    XLLM_MEMORY_FAIL_BUILD_METADATA,
    XLLM_MEMORY_FAIL_INGEST,
    XLLM_MEMORY_FAIL_SYNC_FILE
} xllm_memory_fail_reason;

typedef struct {
    xllm_memory_ingest_progress_kind eKind;
    xllm_memory_scope eScope;
    const char *sPath;
    const char *sRelativePath;
    const char *sSourceUri;
    xllm_memory_skip_reason eSkipReason;
    xllm_memory_fail_reason eFailReason;
    xllm_error_code eErrorCode;
    int32 iStatus;
    uint64 uFileBytes;
    uint32 uVisitedFileCount;
    uint32 uIngestedFileCount;
    uint32 uCreatedRecordCount;
    uint32 uUpdatedRecordCount;
    uint32 uSkippedFileCount;
    uint32 uFailedFileCount;
    xvalue tVendorExtra;
} xllm_memory_ingest_progress;

typedef int (*xllm_memory_ingest_progress_fn)(
    void *pCtx,
    const xllm_memory_ingest_progress *pProgress
);

typedef struct {
    xllm_memory_scope eScope;
    const char *sPath;
    const char *sRecordIdPrefix;
    const char *sAllowedExtensions;
    bool bRecursive;
    bool bReplaceExisting;
    bool bSkipHidden;
    bool bUseWorkspaceDefaults;
    bool bSkipUnchanged;
    const char *sIgnoredDirectories;
    const char *sIgnoredExtensions;
    const char *sIgnoredPathPatterns;
    const char *sSourceUriPrefix;
    uint64 uMaxFileBytes;
    uint32 uChunkChars;
    uint32 uChunkOverlapChars;
    xllm_memory_ingest_progress_fn pfnProgress;
    void *pProgressCtx;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_ingest_directory_options;

typedef struct {
    xllm_memory_scope eScope;
    const char *sPath;
    const char *sRecordIdPrefix;
    bool bRecursive;
    bool bReplaceExisting;
    bool bSkipHidden;
    bool bSkipUnchanged;
    bool bLoadGitIgnore;
    const char *sAllowedExtensions;
    const char *sIgnoredDirectories;
    const char *sIgnoredExtensions;
    const char *sIgnoredPathPatterns;
    const char *sIgnoreFiles;
    const char *sSourceUriPrefix;
    uint64 uMaxFileBytes;
    uint32 uChunkChars;
    uint32 uChunkOverlapChars;
    xllm_memory_ingest_progress_fn pfnProgress;
    void *pProgressCtx;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_ingest_workspace_options;

typedef struct {
    xllm_memory_scope eScope;
    const char *sPath;
    const char *sRootPath;
    const char *sRecordId;
    const char *sRecordIdPrefix;
    const char *sTitle;
    const char *sSourceUri;
    bool bReplaceExisting;
    bool bUseWorkspaceDefaults;
    bool bSkipHidden;
    bool bSkipUnchanged;
    const char *sAllowedExtensions;
    const char *sIgnoredDirectories;
    const char *sIgnoredExtensions;
    const char *sIgnoredPathPatterns;
    const char *sSourceUriPrefix;
    uint64 uMaxFileBytes;
    uint32 uChunkChars;
    uint32 uChunkOverlapChars;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_sync_file_options;

typedef struct {
    const xllm_memory_sync_file_options *pItems;
    size_t iItemCount;
    bool bContinueOnError;
    xvalue tVendorExtra;
} xllm_memory_sync_files_options;

typedef enum {
    XLLM_MEMORY_FILE_EVENT_CREATED = 1,
    XLLM_MEMORY_FILE_EVENT_UPDATED,
    XLLM_MEMORY_FILE_EVENT_DELETED,
    XLLM_MEMORY_FILE_EVENT_RENAMED
} xllm_memory_file_event_kind;

typedef struct {
    xllm_memory_file_event_kind eKind;
    const char *sPath;
    const char *sPreviousPath;
    xvalue tVendorExtra;
} xllm_memory_file_event;

typedef struct {
    xllm_memory_sync_file_options tBaseOptions;
    const xllm_memory_file_event *pItems;
    size_t iItemCount;
    bool bContinueOnError;
    xvalue tVendorExtra;
} xllm_memory_sync_file_events_options;

typedef struct {
    xllm_memory_sync_file_options tBaseOptions;
    size_t iMaxItems;
    bool bContinueOnError;
    xvalue tVendorExtra;
} xllm_memory_file_event_queue_drain_options;

typedef struct {
    xllm_memory_sync_file_options tBaseOptions;
    size_t iDefaultMaxItems;
    bool bContinueOnError;
    xvalue tVendorExtra;
} xllm_memory_watcher_bridge_options;

typedef struct {
    xllm_memory_watcher_bridge_options tBridgeOptions;
    size_t iAutoFlushThreshold;
    xvalue tVendorExtra;
} xllm_memory_watcher_pump_options;

typedef struct {
    xllm_memory_watcher_pump_options tPumpOptions;
    uint32 uDebounceMs;
    size_t iDefaultMaxItems;
    xvalue tVendorExtra;
} xllm_memory_watcher_worker_options;

typedef struct {
    xllm_memory_scope eScope;
    const char *sQuery;
    const char *sConversationId;
    const char *sTurnId;
    const char *sRecordId;
    const char *sSourceUri;
    const char *sRecordIdContains;
    const char *sTitleContains;
    const char *sSourceUriContains;
    const char *sTextContains;
    const char *sMetadataKey;
    const char *sMetadataValue;
    uint32 uMaxHits;
    xllm_opt_f64 tMinScore;
    xllm_opt_f64 tPriorityWeight;
    xllm_opt_f64 tRecencyWeight;
    bool bSkipExpired;
    int64 iNowUnix;
    uint32 uMaxCharsPerHit;
    xvalue tVendorExtra;
} xllm_memory_search_options;

typedef struct {
    xllm_memory_search_options tSearchOptions;
    uint32 uMaxCandidates;
    bool bIncludeBelowMinScore;
    xvalue tVendorExtra;
} xllm_memory_retrieval_debug_options;

typedef struct {
    xllm_memory_scope eScope;
    const char *sConversationId;
    const char *sTurnId;
    const char *sRecordId;
    const char *sSourceUri;
    const char *sRecordIdContains;
    const char *sChunkId;
    const char *sTitleContains;
    const char *sSourceUriContains;
    const char *sTextContains;
    const char *sMetadataKey;
    const char *sMetadataValue;
    uint32 uOffset;
    uint32 uMaxItems;
    xllm_opt_bool tSortByUpdatedAtDesc;
    bool bSkipExpired;
    int64 iNowUnix;
    uint32 uMaxCharsPerText;
    xvalue tVendorExtra;
} xllm_memory_list_options;

typedef struct {
    xllm_context_block_kind eKindOverride;
    int32 iPriority;
    bool bPinned;
    bool bDistinctByRecord;
    xllm_opt_f64 tMinScore;
    const char *sLabel;
    uint32 uMaxHits;
    uint32 uMaxCharsPerHit;
    uint32 uMaxTotalChars;
    xllm_memory_context_render_fn pfnRender;
    void *pRenderCtx;
    xvalue tVendorExtra;
} xllm_memory_context_options;

typedef enum {
    XLLM_MEMORY_TURN_QUERY_LAST_USER_TEXT = 0,
    XLLM_MEMORY_TURN_QUERY_ALL_USER_TEXT,
    XLLM_MEMORY_TURN_QUERY_VISIBLE_TEXT
} xllm_memory_turn_query_mode;

typedef struct {
    xllm_memory_search_options tSearchOptions;
    xllm_memory_context_options tContextOptions;
    xllm_memory_turn_query_mode eQueryMode;
    bool bIncludeSystemPrompt;
    bool bIncludeContextBlocks;
    uint32 uMaxQueryChars;
    xvalue tVendorExtra;
} xllm_memory_turn_search_apply_options;

struct xllm_memory_hit {
    xllm_memory_scope eScope;
    const char *sRecordId;
    const char *sChunkId;
    const char *sTitle;
    const char *sSourceUri;
    const char *sText;
    double fScore;
    double fLexicalScore;
    double fVectorScore;
    double fRrfScore;
    uint32 uChunkIndex;
    size_t iStartByte;
    size_t iEndByte;
    uint32 uLexicalRank;
    uint32 uVectorRank;
    const char *sRetrievalProfileId;
    const char *sEmbedProfileId;
    const char *sIndexProfileId;
    xvalue tMetadata;
    xvalue tVendorExtra;
};

typedef struct {
    xllm_memory_scope eScope;
    const char *sRecordId;
    const char *sTitle;
    const char *sSourceUri;
    const char *sMemoryProfileId;
    const char *sRetrievalProfileId;
    const char *sEmbedProfileId;
    const char *sIndexProfileId;
    size_t iTextLength;
    uint32 uChunkCount;
    uint32 uProfileVersion;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_record_info;

typedef struct {
    xllm_memory_scope eScope;
    const char *sRecordId;
    const char *sChunkId;
    const char *sTitle;
    const char *sSourceUri;
    const char *sText;
    const char *sMemoryProfileId;
    const char *sChunkProfileId;
    const char *sRetrievalProfileId;
    const char *sEmbedProfileId;
    const char *sIndexProfileId;
    const char *sPreviousChunkId;
    const char *sNextChunkId;
    size_t iTextLength;
    size_t iStartByte;
    size_t iEndByte;
    uint64 uContentHash;
    uint32 uChunkIndex;
    uint32 uEmbeddingDim;
    uint32 uProfileVersion;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_chunk_info;

typedef struct {
    const char *sPath;
    const char *sRelativePath;
    const char *sSourceUri;
    xllm_memory_skip_reason eReason;
    uint64 uFileBytes;
    xvalue tVendorExtra;
} xllm_memory_skipped_file_info;

typedef struct {
    const char *sPath;
    const char *sRelativePath;
    const char *sSourceUri;
    xllm_memory_fail_reason eReason;
    xllm_error_code eErrorCode;
    int32 iStatus;
    const char *sMessage;
    uint64 uFileBytes;
    xvalue tVendorExtra;
} xllm_memory_failed_file_info;

typedef struct {
    uint32 uVisitedFileCount;
    uint32 uIngestedFileCount;
    uint32 uCreatedRecordCount;
    uint32 uUpdatedRecordCount;
    uint32 uSkippedFileCount;
    uint32 uFailedFileCount;
    xllm_memory_record_info *pCreatedRecords;
    size_t iCreatedDetailCount;
    xllm_memory_record_info *pUpdatedRecords;
    size_t iUpdatedDetailCount;
    xllm_memory_skipped_file_info *pSkippedFiles;
    size_t iSkippedDetailCount;
    xllm_memory_failed_file_info *pFailedFiles;
    size_t iFailedDetailCount;
} xllm_memory_ingest_directory_result;

typedef struct {
    xllm_memory_ingest_directory_result tIngest;
    uint32 uExaminedRecordCount;
    uint32 uRemovedRecordCount;
    xllm_memory_record_info *pRemovedRecords;
    size_t iRemovedDetailCount;
} xllm_memory_sync_workspace_result;

typedef struct {
    xllm_memory_scope eScope;
    const char *sRootPath;
    const char *sSourceUriPrefix;
    xvalue tVendorExtra;
} xllm_memory_workspace_status_options;

typedef struct {
    xllm_memory_scope eScope;
    const char *sRootPath;
    const char *sSourceUriPrefix;
    bool bRootPathFilterSet;
    bool bSourceUriPrefixFilterSet;
    size_t iRecordCount;
    size_t iChunkCount;
    size_t iMissingPathRecordCount;
    size_t iSensitiveRecordCount;
    size_t iUntrustedRecordCount;
    uint64 uTotalFileBytes;
    int64 iOldestMtimeUnix;
    int64 iNewestMtimeUnix;
    xvalue tVendorExtra;
} xllm_memory_workspace_status;

typedef struct {
    xllm_memory_scope eScope;
    bool bCheckSqlite;
    bool bCheckSparsePostings;
    xvalue tVendorExtra;
} xllm_memory_health_check_options;

typedef struct {
    xllm_memory_scope eScope;
    bool bOk;
    bool bSqliteOpen;
    bool bSchemaOk;
    bool bProfileOk;
    bool bSparsePostingsOk;
    uint32 uSqliteSchemaVersion;
    const char *sMemoryProfileId;
    const char *sStoredMemoryProfileId;
    size_t iRecordCount;
    size_t iChunkCount;
    size_t iSqliteRecordCount;
    size_t iSqliteChunkCount;
    size_t iOrphanChunkCount;
    size_t iSparseChunkCount;
    size_t iSparseMissingChunkCount;
    size_t iSparseOrphanPostingCount;
    xvalue tVendorExtra;
} xllm_memory_health_check;

typedef enum {
    XLLM_MEMORY_CHANGE_CREATED = 1,
    XLLM_MEMORY_CHANGE_UPDATED,
    XLLM_MEMORY_CHANGE_REMOVED,
    XLLM_MEMORY_CHANGE_SKIPPED,
    XLLM_MEMORY_CHANGE_FAILED
} xllm_memory_change_kind;

typedef struct {
    xllm_memory_change_kind eKind;
    xllm_memory_record_info tRecord;
    xllm_memory_skipped_file_info tSkipped;
    xllm_memory_failed_file_info tFailed;
} xllm_memory_change_info;

typedef struct {
    xllm_memory_change_info *pChanges;
    size_t iChangeCount;
} xllm_memory_change_set;

typedef int (*xllm_memory_watcher_worker_batch_fn)(
    void *pCtx,
    const xllm_memory_change_set *pChanges,
    xllm_error *pError
);

typedef struct {
    size_t iMaxBatches;
    size_t iMaxItemsPerBatch;
    xvalue tVendorExtra;
} xllm_memory_watcher_worker_run_options;

typedef struct {
    size_t iBatchCount;
    size_t iChangeCount;
    size_t iPendingCount;
    bool bBlockedByDebounce;
    xvalue tVendorExtra;
} xllm_memory_watcher_worker_run_result;

typedef struct {
    xllm_memory_watcher_worker_run_options tRunOptions;
    uint32 uSleepMs;
    uint32 uMaxWaitMs;
    bool bForceFlushOnTimeout;
    xvalue tVendorExtra;
} xllm_memory_watcher_worker_loop_options;

typedef struct {
    size_t iBatchCount;
    size_t iChangeCount;
    size_t iPendingCount;
    uint32 uLoopCount;
    uint32 uWaitedMs;
    bool bBlockedByDebounce;
    bool bStoppedByBatchLimit;
    bool bTimedOut;
    xvalue tVendorExtra;
} xllm_memory_watcher_worker_loop_result;

typedef struct {
    size_t iPendingCount;
    uint32 uDebounceMs;
    uint32 uElapsedSinceActivityMs;
    uint32 uWaitMsRemaining;
    bool bReady;
    bool bBlockedByDebounce;
    xvalue tVendorExtra;
} xllm_memory_watcher_worker_state;

struct xllm_memory_search_result {
    xllm_memory_hit *pHits;
    size_t iHitCount;
    xvalue tVendorExtra;
};

typedef struct {
    const char *sTerm;
    uint32 uQueryCount;
    uint32 uDocumentFrequency;
} xllm_memory_retrieval_debug_term;

typedef struct {
    xllm_memory_scope eScope;
    const char *sRecordId;
    const char *sChunkId;
    const char *sTitle;
    const char *sSourceUri;
    double fScore;
    double fLexicalScore;
    double fVectorScore;
    double fRrfScore;
    uint32 uLexicalRank;
    uint32 uVectorRank;
    bool bPassedMinScore;
    bool bIncludedInResult;
} xllm_memory_retrieval_debug_candidate;

typedef struct {
    const char *sQuery;
    xllm_memory_scheme eScheme;
    bool bHybridSearchEnabled;
    double fLexicalWeight;
    double fVectorWeight;
    double fMinScore;
    uint32 uVectorCandidateLimit;
    size_t iVectorCandidateCount;
    size_t iFilteredRecordCount;
    size_t iFilteredChunkCount;
    size_t iScoredCandidateCount;
    size_t iBelowMinScoreCount;
    xllm_memory_retrieval_debug_term *pTerms;
    size_t iTermCount;
    xllm_memory_retrieval_debug_candidate *pCandidates;
    size_t iCandidateCount;
    xllm_memory_search_result tSearchResult;
    xvalue tVendorExtra;
} xllm_memory_retrieval_debug_dump;

typedef struct {
    xllm_memory_record_info *pRecords;
    size_t iRecordCount;
    xvalue tVendorExtra;
} xllm_memory_record_list_result;

typedef struct {
    xllm_memory_chunk_info *pChunks;
    size_t iChunkCount;
    xvalue tVendorExtra;
} xllm_memory_chunk_list_result;

typedef struct {
    xllm_memory_scheme eScheme;
    const char *sMemoryProfileId;
    const char *sEmbedProfileId;
    const char *sNamespace;
    const char *sSqlitePath;
    const char *sSqliteVectorExtensionPath;
    xllm_memory_builtin_embedder_kind eBuiltinEmbedderKind;
    const char *sEmbedderKind;
    const char *sEmbedModelId;
    const char *sEmbedRepoId;
    const char *sEmbedRuntimeDllPath;
    const char *sEmbedModelPath;
    const char *sEmbedTokenizerPath;
    const char *sEmbedQueryPrefix;
    const char *sEmbedDocumentPrefix;
    const char *sEmbedPoolingMode;
    bool bSqliteOpen;
    bool bSqliteWalEnabled;
    bool bSqliteWalRequested;
    bool bSqliteVectorExtensionRequested;
    bool bSqliteVectorExtensionLoaded;
    bool bVectorTableReady;
    bool bHybridSearchEnabled;
    bool bEmbedderConfigured;
    bool bOwnsEmbedderCtx;
    bool bEmbedNormalize;
    bool bStorageProfileMatch;
    uint32 uVectorDim;
    uint32 uEmbedDimensions;
    uint32 uEmbedMaxInputTokens;
    uint32 uSqliteSchemaVersion;
    uint32 uSqliteBusyTimeoutMs;
    uint32 uDefaultChunkChars;
    uint32 uDefaultChunkOverlapChars;
    uint32 uDefaultMaxHits;
    double fLexicalWeight;
    double fVectorWeight;
    const char *sStoredMemoryProfileId;
    size_t iRecordCount;
    size_t iMemoryRecordCount;
    size_t iKnowledgeRecordCount;
    size_t iChunkCount;
    size_t iMemoryChunkCount;
    size_t iKnowledgeChunkCount;
} xllm_memory_diagnostics;

XLLM_API void xllm_memory_embedder_init(xllm_memory_embedder *pEmbedder);
XLLM_API void xllm_memory_embedder_reset(xllm_memory_embedder *pEmbedder);
XLLM_API void xllm_memory_builtin_embedder_options_init(xllm_memory_builtin_embedder_options *pOptions);
XLLM_API void xllm_memory_builtin_embedder_probe_reset(xllm_memory_builtin_embedder_probe *pProbe);
XLLM_API int xllm_memory_probe_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_builtin_embedder_probe *pProbe,
    xllm_error *pError
);
XLLM_API int xllm_memory_make_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_embedder *pEmbedder,
    xllm_error *pError
);

XLLM_API void xllm_memory_options_init(xllm_memory_options *pOptions);
XLLM_API void xllm_memory_ingest_options_init(xllm_memory_ingest_options *pOptions);
XLLM_API void xllm_memory_ingest_turn_response_options_init(xllm_memory_ingest_turn_response_options *pOptions);
XLLM_API void xllm_memory_ingest_task_options_init(xllm_memory_ingest_task_options *pOptions);
XLLM_API void xllm_memory_ingest_fact_options_init(xllm_memory_ingest_fact_options *pOptions);
XLLM_API void xllm_memory_ingest_preference_options_init(xllm_memory_ingest_preference_options *pOptions);
XLLM_API void xllm_memory_remove_expired_options_init(xllm_memory_remove_expired_options *pOptions);
XLLM_API void xllm_memory_remove_by_metadata_options_init(xllm_memory_remove_by_metadata_options *pOptions);
XLLM_API void xllm_memory_trim_conversation_options_init(xllm_memory_trim_conversation_options *pOptions);
XLLM_API void xllm_memory_compact_conversation_options_init(xllm_memory_compact_conversation_options *pOptions);
XLLM_API void xllm_memory_compact_conversation_result_init(xllm_memory_compact_conversation_result *pResult);
XLLM_API void xllm_memory_ingest_file_options_init(xllm_memory_ingest_file_options *pOptions);
XLLM_API void xllm_memory_ingest_directory_options_init(xllm_memory_ingest_directory_options *pOptions);
XLLM_API void xllm_memory_ingest_workspace_options_init(xllm_memory_ingest_workspace_options *pOptions);
XLLM_API void xllm_memory_sync_file_options_init(xllm_memory_sync_file_options *pOptions);
XLLM_API void xllm_memory_sync_files_options_init(xllm_memory_sync_files_options *pOptions);
XLLM_API void xllm_memory_sync_file_event_init(xllm_memory_file_event *pEvent);
XLLM_API void xllm_memory_sync_file_events_options_init(xllm_memory_sync_file_events_options *pOptions);
XLLM_API void xllm_memory_file_event_queue_drain_options_init(xllm_memory_file_event_queue_drain_options *pOptions);
XLLM_API void xllm_memory_watcher_bridge_options_init(xllm_memory_watcher_bridge_options *pOptions);
XLLM_API void xllm_memory_watcher_pump_options_init(xllm_memory_watcher_pump_options *pOptions);
XLLM_API void xllm_memory_watcher_worker_options_init(xllm_memory_watcher_worker_options *pOptions);
XLLM_API void xllm_memory_watcher_worker_run_options_init(xllm_memory_watcher_worker_run_options *pOptions);
XLLM_API void xllm_memory_watcher_worker_run_result_init(xllm_memory_watcher_worker_run_result *pResult);
XLLM_API void xllm_memory_watcher_worker_loop_options_init(xllm_memory_watcher_worker_loop_options *pOptions);
XLLM_API void xllm_memory_watcher_worker_loop_result_init(xllm_memory_watcher_worker_loop_result *pResult);
XLLM_API void xllm_memory_watcher_worker_state_init(xllm_memory_watcher_worker_state *pState);
XLLM_API void xllm_memory_ingest_directory_result_init(xllm_memory_ingest_directory_result *pResult);
XLLM_API void xllm_memory_sync_workspace_result_init(xllm_memory_sync_workspace_result *pResult);
XLLM_API void xllm_memory_workspace_status_options_init(xllm_memory_workspace_status_options *pOptions);
XLLM_API void xllm_memory_workspace_status_init(xllm_memory_workspace_status *pStatus);
XLLM_API void xllm_memory_health_check_options_init(xllm_memory_health_check_options *pOptions);
XLLM_API void xllm_memory_health_check_init(xllm_memory_health_check *pHealth);
XLLM_API void xllm_memory_change_set_init(xllm_memory_change_set *pResult);
XLLM_API void xllm_memory_ingest_directory_result_reset(xllm_memory_ingest_directory_result *pResult);
XLLM_API void xllm_memory_sync_workspace_result_reset(xllm_memory_sync_workspace_result *pResult);
XLLM_API void xllm_memory_change_set_reset(xllm_memory_change_set *pResult);
XLLM_API void xllm_memory_search_options_init(xllm_memory_search_options *pOptions);
XLLM_API void xllm_memory_retrieval_debug_options_init(xllm_memory_retrieval_debug_options *pOptions);
XLLM_API void xllm_memory_list_options_init(xllm_memory_list_options *pOptions);
XLLM_API void xllm_memory_context_options_init(xllm_memory_context_options *pOptions);
XLLM_API void xllm_memory_turn_search_apply_options_init(xllm_memory_turn_search_apply_options *pOptions);

XLLM_API int xllm_memory_create(
    xllm_runtime *pRuntime,
    const xllm_memory_options *pOptions,
    xllm_memory **ppMemory
);

XLLM_API void xllm_memory_destroy(xllm_memory *pMemory);

XLLM_API int xllm_memory_ingest_text(
    xllm_memory *pMemory,
    const xllm_memory_ingest_options *pOptions,
    xllm_error *pError
);

XLLM_API int xllm_memory_ingest_turn_response(
    xllm_memory *pMemory,
    const xllm_memory_ingest_turn_response_options *pOptions,
    xllm_error *pError
);

XLLM_API int xllm_memory_ingest_task(
    xllm_memory *pMemory,
    const xllm_memory_ingest_task_options *pOptions,
    xllm_error *pError
);

XLLM_API int xllm_memory_ingest_fact(
    xllm_memory *pMemory,
    const xllm_memory_ingest_fact_options *pOptions,
    xllm_error *pError
);

XLLM_API int xllm_memory_ingest_preference(
    xllm_memory *pMemory,
    const xllm_memory_ingest_preference_options *pOptions,
    xllm_error *pError
);

XLLM_API int xllm_memory_ingest_file(
    xllm_memory *pMemory,
    const xllm_memory_ingest_file_options *pOptions,
    xllm_error *pError
);

XLLM_API int xllm_memory_ingest_directory(
    xllm_memory *pMemory,
    const xllm_memory_ingest_directory_options *pOptions,
    xllm_memory_ingest_directory_result *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_ingest_workspace(
    xllm_memory *pMemory,
    const xllm_memory_ingest_workspace_options *pOptions,
    xllm_memory_ingest_directory_result *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_sync_workspace(
    xllm_memory *pMemory,
    const xllm_memory_ingest_workspace_options *pOptions,
    xllm_memory_sync_workspace_result *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_get_workspace_status(
    const xllm_memory *pMemory,
    const xllm_memory_workspace_status_options *pOptions,
    xllm_memory_workspace_status *pStatus,
    xllm_error *pError
);

XLLM_API int xllm_memory_check_health(
    const xllm_memory *pMemory,
    const xllm_memory_health_check_options *pOptions,
    xllm_memory_health_check *pHealth,
    xllm_error *pError
);

XLLM_API int xllm_memory_sync_file(
    xllm_memory *pMemory,
    const xllm_memory_sync_file_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_sync_files(
    xllm_memory *pMemory,
    const xllm_memory_sync_files_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_sync_file_events(
    xllm_memory *pMemory,
    const xllm_memory_sync_file_events_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_file_event_queue_create(
    xllm_memory_file_event_queue **ppQueue
);

XLLM_API void xllm_memory_file_event_queue_destroy(
    xllm_memory_file_event_queue *pQueue
);

XLLM_API void xllm_memory_file_event_queue_clear(
    xllm_memory_file_event_queue *pQueue
);

XLLM_API size_t xllm_memory_file_event_queue_count(
    const xllm_memory_file_event_queue *pQueue
);

XLLM_API int xllm_memory_file_event_queue_compact(
    xllm_memory_file_event_queue *pQueue,
    xllm_error *pError
);

XLLM_API int xllm_memory_file_event_queue_push(
    xllm_memory_file_event_queue *pQueue,
    const xllm_memory_file_event *pEvent,
    xllm_error *pError
);

XLLM_API int xllm_memory_file_event_queue_push_created(
    xllm_memory_file_event_queue *pQueue,
    const char *sPath,
    xllm_error *pError
);

XLLM_API int xllm_memory_file_event_queue_push_updated(
    xllm_memory_file_event_queue *pQueue,
    const char *sPath,
    xllm_error *pError
);

XLLM_API int xllm_memory_file_event_queue_push_deleted(
    xllm_memory_file_event_queue *pQueue,
    const char *sPath,
    xllm_error *pError
);

XLLM_API int xllm_memory_file_event_queue_push_renamed(
    xllm_memory_file_event_queue *pQueue,
    const char *sPreviousPath,
    const char *sPath,
    xllm_error *pError
);

XLLM_API int xllm_memory_file_event_queue_drain(
    xllm_memory *pMemory,
    xllm_memory_file_event_queue *pQueue,
    const xllm_memory_file_event_queue_drain_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_bridge_create(
    xllm_memory *pMemory,
    const xllm_memory_watcher_bridge_options *pOptions,
    xllm_memory_watcher_bridge **ppBridge,
    xllm_error *pError
);

XLLM_API void xllm_memory_watcher_bridge_destroy(
    xllm_memory_watcher_bridge *pBridge
);

XLLM_API void xllm_memory_watcher_bridge_clear(
    xllm_memory_watcher_bridge *pBridge
);

XLLM_API size_t xllm_memory_watcher_bridge_pending_count(
    const xllm_memory_watcher_bridge *pBridge
);

XLLM_API int xllm_memory_watcher_bridge_compact_pending(
    xllm_memory_watcher_bridge *pBridge,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_bridge_push(
    xllm_memory_watcher_bridge *pBridge,
    const xllm_memory_file_event *pEvent,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_bridge_push_created(
    xllm_memory_watcher_bridge *pBridge,
    const char *sPath,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_bridge_push_updated(
    xllm_memory_watcher_bridge *pBridge,
    const char *sPath,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_bridge_push_deleted(
    xllm_memory_watcher_bridge *pBridge,
    const char *sPath,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_bridge_push_renamed(
    xllm_memory_watcher_bridge *pBridge,
    const char *sPreviousPath,
    const char *sPath,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_bridge_flush(
    xllm_memory_watcher_bridge *pBridge,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_bridge_flush_max(
    xllm_memory_watcher_bridge *pBridge,
    size_t iMaxItems,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_pump_create(
    xllm_memory *pMemory,
    const xllm_memory_watcher_pump_options *pOptions,
    xllm_memory_watcher_pump **ppPump,
    xllm_error *pError
);

XLLM_API void xllm_memory_watcher_pump_destroy(
    xllm_memory_watcher_pump *pPump
);

XLLM_API void xllm_memory_watcher_pump_clear(
    xllm_memory_watcher_pump *pPump
);

XLLM_API size_t xllm_memory_watcher_pump_pending_count(
    const xllm_memory_watcher_pump *pPump
);

XLLM_API int xllm_memory_watcher_pump_compact_pending(
    xllm_memory_watcher_pump *pPump,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_pump_push(
    xllm_memory_watcher_pump *pPump,
    const xllm_memory_file_event *pEvent,
    bool *pbFlushed,
    xllm_memory_change_set *pAutoFlushResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_pump_push_created(
    xllm_memory_watcher_pump *pPump,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pAutoFlushResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_pump_push_updated(
    xllm_memory_watcher_pump *pPump,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pAutoFlushResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_pump_push_deleted(
    xllm_memory_watcher_pump *pPump,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pAutoFlushResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_pump_push_renamed(
    xllm_memory_watcher_pump *pPump,
    const char *sPreviousPath,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pAutoFlushResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_pump_flush(
    xllm_memory_watcher_pump *pPump,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_pump_flush_max(
    xllm_memory_watcher_pump *pPump,
    size_t iMaxItems,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_create(
    xllm_memory *pMemory,
    const xllm_memory_watcher_worker_options *pOptions,
    xllm_memory_watcher_worker **ppWorker,
    xllm_error *pError
);

XLLM_API void xllm_memory_watcher_worker_destroy(
    xllm_memory_watcher_worker *pWorker
);

XLLM_API void xllm_memory_watcher_worker_clear(
    xllm_memory_watcher_worker *pWorker
);

XLLM_API size_t xllm_memory_watcher_worker_pending_count(
    const xllm_memory_watcher_worker *pWorker
);

XLLM_API int xllm_memory_watcher_worker_compact_pending(
    xllm_memory_watcher_worker *pWorker,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_get_state(
    const xllm_memory_watcher_worker *pWorker,
    xllm_memory_watcher_worker_state *pState,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_push(
    xllm_memory_watcher_worker *pWorker,
    const xllm_memory_file_event *pEvent,
    bool *pbFlushed,
    xllm_memory_change_set *pPushResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_push_created(
    xllm_memory_watcher_worker *pWorker,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pPushResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_push_updated(
    xllm_memory_watcher_worker *pWorker,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pPushResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_push_deleted(
    xllm_memory_watcher_worker *pWorker,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pPushResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_push_renamed(
    xllm_memory_watcher_worker *pWorker,
    const char *sPreviousPath,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pPushResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_poll(
    xllm_memory_watcher_worker *pWorker,
    bool *pbFlushed,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_poll_max(
    xllm_memory_watcher_worker *pWorker,
    size_t iMaxItems,
    bool *pbFlushed,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_flush(
    xllm_memory_watcher_worker *pWorker,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_flush_max(
    xllm_memory_watcher_worker *pWorker,
    size_t iMaxItems,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_run_ready(
    xllm_memory_watcher_worker *pWorker,
    const xllm_memory_watcher_worker_run_options *pOptions,
    xllm_memory_watcher_worker_batch_fn fnOnBatch,
    void *pBatchCtx,
    xllm_memory_watcher_worker_run_result *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_run_loop(
    xllm_memory_watcher_worker *pWorker,
    const xllm_memory_watcher_worker_loop_options *pOptions,
    xllm_memory_watcher_worker_batch_fn fnOnBatch,
    void *pBatchCtx,
    xllm_memory_watcher_worker_loop_result *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_make_change_set_from_ingest(
    const xllm_memory_ingest_directory_result *pIngest,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_make_change_set_from_sync(
    const xllm_memory_sync_workspace_result *pSync,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_remove(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sRecordId,
    xllm_error *pError
);

XLLM_API int xllm_memory_remove_by_source_uri(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sSourceUri,
    uint32 *puRemovedCount,
    xllm_error *pError
);

XLLM_API int xllm_memory_remove_by_conversation(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sConversationId,
    const char *sTurnId,
    uint32 *puRemovedCount,
    xllm_error *pError
);

XLLM_API int xllm_memory_remove_by_metadata(
    xllm_memory *pMemory,
    const xllm_memory_remove_by_metadata_options *pOptions,
    uint32 *puRemovedCount,
    xllm_error *pError
);

XLLM_API int xllm_memory_trim_conversation(
    xllm_memory *pMemory,
    const xllm_memory_trim_conversation_options *pOptions,
    uint32 *puRemovedCount,
    xllm_error *pError
);

XLLM_API int xllm_memory_compact_conversation(
    xllm_memory *pMemory,
    const xllm_memory_compact_conversation_options *pOptions,
    xllm_memory_compact_conversation_result *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_remove_expired(
    xllm_memory *pMemory,
    const xllm_memory_remove_expired_options *pOptions,
    uint32 *puRemovedCount,
    xllm_error *pError
);

XLLM_API int xllm_memory_search(
    xllm_memory *pMemory,
    const xllm_memory_search_options *pOptions,
    xllm_memory_search_result *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_search_debug(
    xllm_memory *pMemory,
    const xllm_memory_retrieval_debug_options *pOptions,
    xllm_memory_retrieval_debug_dump *pDump,
    xllm_error *pError
);

XLLM_API int xllm_memory_list_records(
    xllm_memory *pMemory,
    const xllm_memory_list_options *pOptions,
    xllm_memory_record_list_result *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_list_chunks(
    xllm_memory *pMemory,
    const xllm_memory_list_options *pOptions,
    xllm_memory_chunk_list_result *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_apply_search_to_request(
    xllm_request *pRequest,
    const xllm_memory_search_result *pResult,
    const xllm_memory_context_options *pOptions,
    xllm_error *pError
);

XLLM_API int xllm_memory_apply_search_to_turn(
    xllm_turn *pTurn,
    const xllm_memory_search_result *pResult,
    const xllm_memory_context_options *pOptions,
    xllm_error *pError
);

XLLM_API int xllm_memory_search_and_apply_to_request(
    xllm_memory *pMemory,
    const xllm_memory_search_options *pSearchOptions,
    xllm_request *pRequest,
    const xllm_memory_context_options *pContextOptions,
    xllm_error *pError
);

XLLM_API int xllm_memory_search_and_apply_to_turn(
    xllm_memory *pMemory,
    const xllm_memory_search_options *pSearchOptions,
    xllm_turn *pTurn,
    const xllm_memory_context_options *pContextOptions,
    xllm_error *pError
);

XLLM_API int xllm_memory_search_and_apply_from_turn_to_request(
    xllm_memory *pMemory,
    const xllm_turn *pSourceTurn,
    xllm_request *pRequest,
    const xllm_memory_turn_search_apply_options *pOptions,
    xllm_error *pError
);

XLLM_API int xllm_memory_search_and_apply_from_turn_to_turn(
    xllm_memory *pMemory,
    const xllm_turn *pSourceTurn,
    xllm_turn *pTurn,
    const xllm_memory_turn_search_apply_options *pOptions,
    xllm_error *pError
);

#define xllm_memory_apply_search_to_turn_request xllm_memory_apply_search_to_turn
#define xllm_memory_search_and_apply_to_turn_request xllm_memory_search_and_apply_to_turn
#define xllm_memory_search_and_apply_from_turn_request_to_request xllm_memory_search_and_apply_from_turn_to_request
#define xllm_memory_search_and_apply_from_turn_request_to_turn xllm_memory_search_and_apply_from_turn_to_turn
#define xllm_memory_search_and_apply_from_turn_request_to_turn_request xllm_memory_search_and_apply_from_turn_to_turn

XLLM_API xllm_memory_scheme xllm_memory_get_scheme(const xllm_memory *pMemory);
XLLM_API const char *xllm_memory_get_profile_id(const xllm_memory *pMemory);
XLLM_API size_t xllm_memory_record_count(const xllm_memory *pMemory, xllm_memory_scope eScope);
XLLM_API size_t xllm_memory_chunk_count(const xllm_memory *pMemory, xllm_memory_scope eScope);

XLLM_API void xllm_memory_search_result_reset(xllm_memory_search_result *pResult);
XLLM_API void xllm_memory_retrieval_debug_dump_reset(xllm_memory_retrieval_debug_dump *pDump);
XLLM_API void xllm_memory_record_list_result_reset(xllm_memory_record_list_result *pResult);
XLLM_API void xllm_memory_chunk_list_result_reset(xllm_memory_chunk_list_result *pResult);
XLLM_API void xllm_memory_diagnostics_init(xllm_memory_diagnostics *pDiagnostics);

XLLM_API int xllm_memory_get_diagnostics(
    const xllm_memory *pMemory,
    xllm_memory_diagnostics *pDiagnostics,
    xllm_error *pError
);

#ifdef __cplusplus
}
#endif

#if defined(XLLM_IMPLEMENTATION) || defined(XLLM_MEMORY_IMPLEMENTATION)
#include "src/xllm_memory/xllm_memory.c"
#endif

#endif
