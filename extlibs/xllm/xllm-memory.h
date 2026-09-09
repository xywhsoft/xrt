#ifndef XLLM_MEMORY_H
#define XLLM_MEMORY_H

#include "xllm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xllm_memory xllm_memory;

typedef enum xllm_memory_scope {
    XLLM_MEMORY_SCOPE_ANY = 0,
    XLLM_MEMORY_SCOPE_MEMORY,
    XLLM_MEMORY_SCOPE_KNOWLEDGE
} xllm_memory_scope;

typedef enum xllm_memory_kind {
    XLLM_MEMORY_KIND_ANY = 0,
    XLLM_MEMORY_KIND_FACT,
    XLLM_MEMORY_KIND_PREFERENCE,
    XLLM_MEMORY_KIND_TASK,
    XLLM_MEMORY_KIND_SUMMARY,
    XLLM_MEMORY_KIND_KNOWLEDGE
} xllm_memory_kind;

typedef enum xllm_memory_trust {
    XLLM_MEMORY_TRUST_UNTRUSTED = 0,
    XLLM_MEMORY_TRUST_LOCAL,
    XLLM_MEMORY_TRUST_USER_APPROVED
} xllm_memory_trust;

typedef enum xllm_memory_sensitivity {
    XLLM_MEMORY_SENSITIVITY_PUBLIC = 0,
    XLLM_MEMORY_SENSITIVITY_INTERNAL,
    XLLM_MEMORY_SENSITIVITY_SENSITIVE,
    XLLM_MEMORY_SENSITIVITY_SECRET
} xllm_memory_sensitivity;

typedef enum xllm_memory_action {
    XLLM_MEMORY_ACTION_NONE = 0,
    XLLM_MEMORY_ACTION_CREATED,
    XLLM_MEMORY_ACTION_REPLACED,
    XLLM_MEMORY_ACTION_UNCHANGED,
    XLLM_MEMORY_ACTION_REMOVED
} xllm_memory_action;

typedef struct xllm_memory_config {
    const char* sPath;
    const char* sNamespace;
    bool bCreateIfMissing;
} xllm_memory_config;

typedef struct xllm_memory_record_input {
    xllm_memory_scope eScope;
    xllm_memory_kind eKind;
    xllm_memory_trust eTrust;
    xllm_memory_sensitivity eSensitivity;
    const char* sRecordId;
    const char* sTitle;
    const char* sSourceUri;
    const char* sText;
    const char* sActor;
    const char* sReason;
    int32_t iPriority;
    int64_t iCreatedAtUnix;
    int64_t iUpdatedAtUnix;
    int64_t iExpiresAtUnix;
    bool bReplaceExisting;
} xllm_memory_record_input;

typedef struct xllm_memory_receipt {
    xllm_memory_action eAction;
    uint64_t uStoreRevision;
    uint64_t uRecordRevision;
    uint64_t uPreviousContentHash;
    uint64_t uContentHash;
    int64_t iUpdatedAtUnix;
    char sRecordId[128];
} xllm_memory_receipt;

typedef struct xllm_memory_search_options {
    const char* sQuery;
    xllm_memory_scope eScope;
    xllm_memory_kind eKind;
    xllm_memory_sensitivity eMaximumSensitivity;
    uint32_t uMaxHits;
    size_t iMaxTotalBytes;
    double fMinScore;
    bool bIncludeExpired;
    int64_t iNowUnix;
} xllm_memory_search_options;

typedef struct xllm_memory_hit {
    xllm_memory_scope eScope;
    xllm_memory_kind eKind;
    xllm_memory_trust eTrust;
    xllm_memory_sensitivity eSensitivity;
    char* sRecordId;
    char* sTitle;
    char* sSourceUri;
    char* sText;
    char* sActor;
    char* sReason;
    int32_t iPriority;
    int64_t iCreatedAtUnix;
    int64_t iUpdatedAtUnix;
    int64_t iExpiresAtUnix;
    uint64_t uRecordRevision;
    uint64_t uContentHash;
    double fScore;
    bool bTextTruncated;
} xllm_memory_hit;

typedef struct xllm_memory_search_result {
    xllm_memory_hit* pHits;
    size_t iHitCount;
    uint64_t uStoreRevision;
} xllm_memory_search_result;

typedef struct xllm_memory_stats {
    uint64_t uStoreRevision;
    uint64_t uRecordCount;
    uint64_t uMemoryRecordCount;
    uint64_t uKnowledgeRecordCount;
    uint64_t uExpiredRecordCount;
    uint64_t uSensitiveRecordCount;
    uint64_t uTotalTextBytes;
} xllm_memory_stats;

void xllmMemoryConfigInit(xllm_memory_config* pConfig);
void xllmMemoryRecordInputInit(xllm_memory_record_input* pInput);
void xllmMemorySearchOptionsInit(xllm_memory_search_options* pOptions);
const char* xllmMemoryScopeName(xllm_memory_scope eScope);
const char* xllmMemoryKindName(xllm_memory_kind eKind);
const char* xllmMemoryTrustName(xllm_memory_trust eTrust);
const char* xllmMemorySensitivityName(xllm_memory_sensitivity eSensitivity);
const char* xllmMemoryActionName(xllm_memory_action eAction);

xllm_memory* xllmMemoryOpen(const xllm_memory_config* pConfig, xllm_error* pError);
void xllmMemoryClose(xllm_memory* pMemory);
const char* xllmMemoryNamespace(const xllm_memory* pMemory);
const char* xllmMemoryPath(const xllm_memory* pMemory);
bool xllmMemoryGetStats(const xllm_memory* pMemory, int64_t iNowUnix, xllm_memory_stats* pStats);

bool xllmMemoryPut(xllm_memory* pMemory, const xllm_memory_record_input* pInput,
    xllm_memory_receipt* pReceipt, xllm_error* pError);
bool xllmMemoryRemove(xllm_memory* pMemory, const char* sRecordId,
    xllm_memory_receipt* pReceipt, xllm_error* pError);

bool xllmMemorySearch(const xllm_memory* pMemory, const xllm_memory_search_options* pOptions,
    xllm_memory_search_result* pResult, xllm_error* pError);
void xllmMemorySearchResultUnit(xllm_memory_search_result* pResult);
bool xllmMemoryRenderContext(const xllm_memory* pMemory, const xllm_memory_search_result* pResult,
    size_t iMaxBytes, char** psContext, xllm_error* pError);
void xllmMemoryFree(void* pData);

#ifdef __cplusplus
}
#endif

#endif
