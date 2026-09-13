#ifndef XLLM_SESSION_H
#define XLLM_SESSION_H

#include "../xllm/xllm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xllm_session xllm_session;
typedef struct xllm_compaction xllm_compaction;

#define XLLM_SESSION_ENTRY_PINNED     0x00000001u
#define XLLM_SESSION_ENTRY_SYNTHETIC  0x00000002u

#define XLLM_SESSION_DEFAULT_CONTEXT_WINDOW_TOKENS 204800ull
#define XLLM_SESSION_DEFAULT_MAX_OUTPUT_TOKENS     131072u

#define XLLM_COMPACTION_SECTION_OBJECTIVE          (1u << 0)
#define XLLM_COMPACTION_SECTION_CONSTRAINTS        (1u << 1)
#define XLLM_COMPACTION_SECTION_ARCHITECTURE       (1u << 2)
#define XLLM_COMPACTION_SECTION_COMPLETED          (1u << 3)
#define XLLM_COMPACTION_SECTION_REPOSITORY_STATE   (1u << 4)
#define XLLM_COMPACTION_SECTION_VERIFICATION       (1u << 5)
#define XLLM_COMPACTION_SECTION_OPEN_ISSUES        (1u << 6)
#define XLLM_COMPACTION_SECTION_NEXT_ACTIONS       (1u << 7)
#define XLLM_COMPACTION_SECTION_ALL                0x000000ffu

typedef struct xllm_session_config {
    uint64_t uContextWindowTokens;
    uint32_t uMaxOutputTokens;
    /* Minimum output room protected from input growth; 0 selects a dynamic default. */
    uint32_t uOutputReserveTokens;
    uint32_t uSafetyReserveTokens;
    uint32_t uRecentTurnsToKeep;
    uint32_t uToolPruneBytes;
    uint32_t uSummaryMaxTokens;
    uint32_t uSummaryMinTokens;
    uint32_t uCompactionRequiredSections;
    double fPruneTrigger;
    double fCompactTrigger;
} xllm_session_config;

typedef enum xllm_session_pressure {
    XLLM_SESSION_PRESSURE_NONE = 0,
    XLLM_SESSION_PRESSURE_PRUNE,
    XLLM_SESSION_PRESSURE_COMPACT,
    XLLM_SESSION_PRESSURE_OVERFLOW
} xllm_session_pressure;

typedef struct xllm_session_stats {
    uint64_t uContextWindowTokens;
    uint64_t uInputBudgetTokens;
    uint64_t uOutputReserveTokens;
    uint64_t uRawActiveTokens;
    uint64_t uRenderedActiveTokens;
    uint64_t uPruneThresholdTokens;
    uint64_t uCompactThresholdTokens;
    uint64_t uCompactedThroughSequence;
    uint64_t uCurrentTurn;
    uint64_t uEntryCount;
    uint64_t uCompactionCount;
    uint64_t uJournalSequence;
    uint32_t uNextMaxOutputTokens;
    uint32_t uPendingToolCalls;
    bool bJournalEnabled;
    xllm_session_pressure ePressure;
} xllm_session_stats;

typedef struct xllm_compaction_quality {
    uint32_t uRequiredSections;
    uint32_t uPresentSections;
    uint32_t uMissingSections;
    uint32_t uMinimumSummaryTokens;
    uint32_t uMaximumSummaryTokens;
    uint64_t uSourceTokens;
    uint64_t uSummaryTokens;
    bool bAccepted;
} xllm_compaction_quality;

/* Borrowed view into an unresolved assistant tool call. The strings remain
 * valid until the session is mutated or destroyed. */
typedef struct xllm_pending_tool_call {
    uint64_t uTurn;
    const char* sId;
    const char* sName;
    const char* sArgumentsJson;
} xllm_pending_tool_call;

typedef struct xllm_session_tail {
    uint64_t uTurn;
    xllm_role eRole;
    bool bHasMessage;
} xllm_session_tail;

void xllmSessionConfigInit(xllm_session_config* pConfig);
uint64_t xllmSessionComputeSafetyReserve(uint64_t uContextWindowTokens);
uint32_t xllmSessionComputeOutputReserve(uint64_t uContextWindowTokens, uint32_t uMaxOutputTokens);
uint64_t xllmEstimateTextTokens(const char* sText);
uint64_t xllmEstimateMessageTokens(const xllm_message* pMessage);

xllm_session* xllmSessionCreate(const xllm_session_config* pConfig, xllm_error* pError);
xllm_session* xllmSessionFork(const xllm_session* pSession, xllm_error* pError);
void xllmSessionDestroy(xllm_session* pSession);
bool xllmSessionGetConfig(const xllm_session* pSession, xllm_session_config* pConfig);

uint64_t xllmSessionBeginTurn(xllm_session* pSession);
uint64_t xllmSessionCurrentTurn(const xllm_session* pSession);
bool xllmSessionAddMessage(xllm_session* pSession, uint64_t uTurn, const xllm_message* pMessage, uint32_t uFlags);
bool xllmSessionAddText(xllm_session* pSession, uint64_t uTurn, xllm_role eRole, const char* sContent, uint32_t uFlags);
bool xllmSessionAddAssistantResponse(xllm_session* pSession, uint64_t uTurn, const xllm_response* pResponse);
bool xllmSessionAddToolResult(xllm_session* pSession, uint64_t uTurn, const char* sToolCallId, const char* sContent);
bool xllmSessionGetTail(const xllm_session* pSession, xllm_session_tail* pTail);
size_t xllmSessionPendingToolCallCount(const xllm_session* pSession);
bool xllmSessionPendingToolCallAt(const xllm_session* pSession, size_t iIndex, xllm_pending_tool_call* pCall);

bool xllmSessionGetStats(const xllm_session* pSession, xllm_session_stats* pStats);
bool xllmSessionBuildRequest(const xllm_session* pSession, xllm_request* pRequest, xllm_error* pError);

/*
 * A compaction object is a transaction: prepare selects a safe prefix and
 * builds a summarizer prompt; commit advances the checkpoint only after a
 * valid summary was obtained. Destroying it without commit aborts safely.
 */
xllm_compaction* xllmSessionPrepareCompaction(xllm_session* pSession, bool bForce, xllm_error* pError);
const char* xllmCompactionPrompt(const xllm_compaction* pCompaction);
uint64_t xllmCompactionThroughSequence(const xllm_compaction* pCompaction);
uint64_t xllmCompactionEstimatedTokens(const xllm_compaction* pCompaction);
bool xllmCompactionEvaluateSummary(const xllm_compaction* pCompaction, const char* sSummary,
    xllm_compaction_quality* pQuality, xllm_error* pError);
bool xllmSessionCommitCompaction(xllm_session* pSession, xllm_compaction* pCompaction, const char* sSummary, xllm_error* pError);
void xllmCompactionDestroy(xllm_compaction* pCompaction);

bool xllmSessionSave(const xllm_session* pSession, const char* sPath, xllm_error* pError);
xllm_session* xllmSessionLoad(const char* sPath, xllm_error* pError);

/*
 * Journaling is single-writer. Enable it only on a new/recovered session.
 * Mutations are appended before they become visible in memory. A checkpoint
 * atomically writes the full state and then removes covered journal records.
 */
bool xllmSessionEnableJournal(xllm_session* pSession, const char* sJournalPath, xllm_error* pError);
void xllmSessionDisableJournal(xllm_session* pSession);
const char* xllmSessionJournalPath(const xllm_session* pSession);
bool xllmSessionCheckpoint(xllm_session* pSession, const char* sSnapshotPath, xllm_error* pError);
xllm_session* xllmSessionRecover(const char* sSnapshotPath, const char* sJournalPath,
    const xllm_session_config* pConfigIfNew, xllm_error* pError);

#ifdef __cplusplus
}
#endif

#endif
