#ifndef XLLM_SESSION_H
#define XLLM_SESSION_H

/* TCC hosts mount this header in a flat VFS (/xs/xllm-session.h); native
 * builds use the on-disk sibling tree. Both spellings resolve to the same
 * xllm.h so vendored copies stay byte-identical to this file. */
#if defined(__TINYC__)
#include <xllm.h>
#else
#include "../xllm/xllm.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xllm_session xllm_session;
typedef struct xllm_compaction xllm_compaction;

#define XLLM_SESSION_ENTRY_PINNED     0x00000001u
#define XLLM_SESSION_ENTRY_SYNTHETIC  0x00000002u

#define XLLM_SESSION_DEFAULT_CONTEXT_WINDOW_TOKENS 204800ull
#define XLLM_SESSION_DEFAULT_MAX_OUTPUT_TOKENS     131072u

/* Legacy (durable style) summary sections; kept for hosts that pin them. */
#define XLLM_COMPACTION_SECTION_OBJECTIVE          (1u << 0)
#define XLLM_COMPACTION_SECTION_CONSTRAINTS        (1u << 1)
#define XLLM_COMPACTION_SECTION_ARCHITECTURE       (1u << 2)
#define XLLM_COMPACTION_SECTION_COMPLETED          (1u << 3)
#define XLLM_COMPACTION_SECTION_REPOSITORY_STATE   (1u << 4)
#define XLLM_COMPACTION_SECTION_VERIFICATION       (1u << 5)
#define XLLM_COMPACTION_SECTION_OPEN_ISSUES        (1u << 6)
#define XLLM_COMPACTION_SECTION_NEXT_ACTIONS       (1u << 7)
#define XLLM_COMPACTION_SECTION_ALL                0x000000ffu

/* Default (coding style, Pi) summary sections. */
#define XLLM_COMPACTION_SECTION_GOAL                (1u << 8)
#define XLLM_COMPACTION_SECTION_PREFERENCES         (1u << 9)
#define XLLM_COMPACTION_SECTION_PROGRESS            (1u << 10)
#define XLLM_COMPACTION_SECTION_KEY_DECISIONS       (1u << 11)
#define XLLM_COMPACTION_SECTION_NEXT_STEPS          (1u << 12)
#define XLLM_COMPACTION_SECTION_CRITICAL_CONTEXT    (1u << 13)
#define XLLM_COMPACTION_SECTION_PI_ALL              0x00003f00u

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
    /* --- v3 additions (appended; ConfigInit assigns defaults) --- */
    uint32_t uKeepRecentTokens;      /* tail-window cut budget; 0 = derived min(20000, window/4) */
    uint32_t uSummaryMaxBytes;       /* summary byte cap (quality gate); 0 = 32768, clamped to window */
    uint32_t uUserMessageCapBytes;   /* user message byte cap at Add time; 0 = unlimited */
    uint32_t uToolResultCapBytes;    /* tool result byte cap at Add time; 0 = unlimited */
    uint32_t uToolResultTotalCapBytes; /* per-turn cumulative tool result cap; 0 = unlimited */
    uint64_t uJournalMaxBytes;       /* journal replay budget; 0 = 64 MiB */
    const char* sSummaryStyle;       /* NULL/"coding" = Pi coding, "general", "durable" (v2 8-section); borrowed */
    const char* sSnapshotPath;       /* optional default snapshot path for the easy layer; borrowed */
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
    /* --- v3 additions --- */
    uint64_t uFillExact;            /* last observed prompt+output tokens; UINT64_MAX when unknown */
    uint64_t uIncrementMax;         /* worst-case next-turn growth envelope (tokens) */
    uint64_t uCachedInputTokens;    /* last observed provider cache hit (observation only) */
    uint64_t uSummaryTokensExact;   /* current summary cost: meta-call output tokens */
    uint32_t uSummaryGeneration;    /* summary generation; +1 per compaction or L2 truncation */
    uint32_t uAutoCompactStreak;    /* consecutive auto-compactions without a user entry */
    bool bFillExactValid;           /* false until the first real call reports usage (or after restore) */
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
    /* --- v3 additions (the decision fields; token figures stay informational) --- */
    size_t uSummaryBytes;
    size_t uMaximumSummaryBytes;
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

/* Rolling summary object (Pi CompactionEntry with exact meta-call usage). */
typedef struct xllm_session_summary {
    const char* sText;              /* borrowed from the session */
    uint64_t uThroughSequence;
    uint32_t uGeneration;
    uint64_t uPromptTokensAtBirth;
    uint64_t uOutputTokensAtBirth;
} xllm_session_summary;

/* ------------------------------------------------------------------ */
/* Compaction strategy table (D10): per-stage NULL = built-in default. */
/* ------------------------------------------------------------------ */

typedef enum xllm_compact_decision {
    XLLM_COMPACT_NO = 0,
    XLLM_COMPACT_YES
} xllm_compact_decision;

typedef struct xllm_compaction_plan {
    uint64_t uThroughSequence;      /* complete-turn candidates = (previous through, this value] */
    uint64_t uPrefixThroughSequence; /* split-turn prefix upper bound; 0 = no split (default).
                                      * Set when the retained-window-start turn alone exceeds the
                                      * keep-recent budget: entries (uThroughSequence, this value]
                                      * are that turn's prefix, summarized separately. */
    uint32_t uReserved[3];
} xllm_compaction_plan;

struct xllm_session_stats;

typedef struct xllm_compaction_ops {
    /* Trigger ruling; threshold auto path only (overflow/manual bypass it). */
    xllm_compact_decision (*pShouldCompact)(xllm_session*,
        const xllm_session_stats* pStats, void* pUserData);
    /* Cut point: pick the candidate range ending sequence (pair-complete). */
    bool (*pPlan)(xllm_session*, uint64_t uPrevThrough,
        xllm_compaction_plan* pPlan, void* pUserData);
    /* Serialize candidates (uFrom exclusive, uTo inclusive) into a NUL-terminated
     * malloc'd text; the session frees it with free(). */
    bool (*pSerialize)(xllm_session*, uint64_t uFrom, uint64_t uTo,
        char** psText, void* pUserData);
    /* Build the summarizer prompt from the previous summary and serialized
     * candidates; malloc'd result. */
    bool (*pBuildPrompt)(xllm_session*, const char* sPrevSummary,
        const char* sCandidates, char** psPrompt, void* pUserData);
    /* Meta call: produce a malloc'd summary from the prompt; report usage for
     * exact accounting. Default (easy layer) = bound client; NULL with no
     * bound client means the auto path is unavailable (drive it manually). */
    bool (*pSummarize)(xllm_session*, const char* sPrompt,
        char** psSummary, xllm_usage* pUsageOut, void* pUserData);
    /* Quality gate; fills *pQuality and returns acceptance in bAccepted. */
    bool (*pEvaluate)(xllm_session*, const char* sSummary,
        xllm_compaction_quality* pQuality, void* pUserData);
    /* Read-only commit notification. */
    void (*pOnCommitted)(xllm_session*, const xllm_session_summary*, void* pUserData);
    void* pUserData;
    uint32_t uReserved[4];
} xllm_compaction_ops;

const xllm_compaction_ops* xllmSessionDefaultCompactionOps(void);
bool xllmSessionSetCompactionOps(xllm_session*, const xllm_compaction_ops* pOps /* NULL = default */);

/* ------------------------------------------------------------------ */
/* Render and event hooks (D11). Borrowed; not persisted; fork inherits. */
/* ------------------------------------------------------------------ */

typedef enum xllm_render_action {
    XLLM_RENDER_KEEP = 0,
    XLLM_RENDER_MODIFIED,
    XLLM_RENDER_SKIP
} xllm_render_action;

typedef enum xllm_session_event_type {
    XLLM_SESSION_EVENT_TURN_BEGIN = 1,
    XLLM_SESSION_EVENT_TURN_END,
    XLLM_SESSION_EVENT_ENTRY_ADDED,
    XLLM_SESSION_EVENT_FILL_UPDATED,
    XLLM_SESSION_EVENT_PRESSURE_CHANGED,
    XLLM_SESSION_EVENT_COMPACT_PREPARE,
    XLLM_SESSION_EVENT_COMPACT_PLAN,
    XLLM_SESSION_EVENT_COMPACT_PROMPT,
    XLLM_SESSION_EVENT_COMPACT_SUMMARY,
    XLLM_SESSION_EVENT_COMPACT_EVALUATE,
    XLLM_SESSION_EVENT_COMPACT_COMMIT,
    XLLM_SESSION_EVENT_COMPACT_ABORT,
    XLLM_SESSION_EVENT_LADDER_TRUNCATE,
    XLLM_SESSION_EVENT_JOURNAL_RECORD,
    XLLM_SESSION_EVENT_CHECKPOINT_SAVED,
    XLLM_SESSION_EVENT_SESSION_RECOVERED,
    XLLM_SESSION_EVENT_SESSION_FORKED
} xllm_session_event_type;

typedef struct xllm_session_event {
    xllm_session_event_type eType;
    uint64_t uTurn;
    uint64_t uSeqFrom;
    uint64_t uSeqTo;
    const xllm_session_stats* pStats;   /* stack snapshot; valid only during the callback */
    const char* sText;                  /* optional: summary preview / stage label */
} xllm_session_event;

typedef struct xllm_session_hooks {
    /* Per-entry transform on a cloned work message. Same entry should map to
     * the same output (violations cost cache hits, not correctness). SKIPping
     * an entry that breaks tool-call pairing fails the render. */
    xllm_render_action (*pRenderMessage)(xllm_session*, uint64_t uSequence,
        uint64_t uTurn, uint32_t uEntryFlags, xllm_message* pWork, void* pUserData);
    /* Summary message construction; pWork arrives pre-filled with the default
     * user-bridge form. Return false to omit the summary this render. */
    bool (*pRenderSummary)(xllm_session*, const xllm_session_summary*,
        xllm_message* pWork, void* pUserData);
    /* Final request post-processing (ephemeral content allowed; append at the
     * end to protect prefix caching). Return false to fail the render. */
    bool (*pRenderComplete)(xllm_session*, xllm_request* pRequest, void* pUserData);
    /* Pure observation; return value ignored. */
    void (*pOnEvent)(xllm_session*, const xllm_session_event* pEvent, void* pUserData);
    void* pUserData;
    uint32_t uReserved[4];
} xllm_session_hooks;

bool xllmSessionSetHooks(xllm_session*, const xllm_session_hooks* pHooks /* NULL = remove */);

void xllmSessionConfigInit(xllm_session_config*);
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

/* Exact-feedback channel: records server usage and refreshes governance.
 * xllmSessionAddAssistantResponse calls this automatically. */
bool xllmSessionRecordUsage(xllm_session* pSession, const xllm_usage* pUsage);

bool xllmSessionGetTail(const xllm_session* pSession, xllm_session_tail* pTail);
size_t xllmSessionPendingToolCallCount(const xllm_session* pSession);
bool xllmSessionPendingToolCallAt(const xllm_session* pSession, size_t iIndex, xllm_pending_tool_call* pCall);

bool xllmSessionGetStats(const xllm_session* pSession, xllm_session_stats* pStats);
bool xllmSessionBuildRequest(const xllm_session* pSession, xllm_request* pRequest, xllm_error* pError);
bool xllmSessionGetSummary(const xllm_session* pSession, xllm_session_summary* pSummary);

/*
 * A compaction object is a transaction: prepare selects a safe prefix and
 * builds a summarizer prompt; commit advances the checkpoint only after a
 * valid summary was obtained. Destroying it without commit aborts safely.
 */
xllm_compaction* xllmSessionPrepareCompaction(xllm_session* pSession, bool bForce, xllm_error* pError);
const char* xllmCompactionPrompt(const xllm_compaction* pCompaction);
uint64_t xllmCompactionThroughSequence(const xllm_compaction* pCompaction);
uint64_t xllmCompactionEstimatedTokens(const xllm_compaction* pCompaction);
/* Record the meta-call usage before committing (exact summary accounting). */
bool xllmCompactionSetUsage(xllm_compaction* pCompaction, const xllm_usage* pUsage);
bool xllmCompactionEvaluateSummary(const xllm_compaction* pCompaction, const char* sSummary,
    xllm_compaction_quality* pQuality, xllm_error* pError);
bool xllmSessionCommitCompaction(xllm_session* pSession, xllm_compaction* pCompaction, const char* sSummary, xllm_error* pError);
void xllmCompactionDestroy(xllm_compaction* pCompaction);

/* Threshold auto-compaction consult: runs the full ops pipeline (meta call via
 * the bound client or a custom pSummarize) when due. */
bool xllmSessionMaybeCompact(xllm_session* pSession, bool* pbCompact, xllm_error* pError);

/* Overflow ladder: L1 full compaction via the current ops, then L2 structural
 * tail truncation (turn boundaries, pair-safe, floor keepRecent/2) journaling
 * a truncate event. L3 (per-message cap rejection) happens at Add time. */
bool xllmSessionOverflowLadder(xllm_session* pSession, xllm_error* pError);

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

/* ------------------------------------------------------------------ */
/* Easy layer (D1): optional bound client driving the call loop.       */
/* ------------------------------------------------------------------ */

xllm_session* xllmSessionCreateBound(const xllm_session_config* pConfig,
    xllm_client* pClient /* borrowed, must outlive the session */, xllm_error* pError);

/* Full convenience turn: begin turn, add user text (cap-checked), render,
 * call, add the assistant response (usage recorded), then MaybeCompact.
 * The response ownership moves to the caller. */
xllm_result xllmSessionSend(xllm_session* pSession, const char* sUserText,
    const xllm_stream_callbacks* pCallbacks /* optional */, xllm_response** ppResponse,
    xllm_error* pError);

/* Render and call only; nothing is appended to the ledger. */
xllm_result xllmSessionComplete(xllm_session* pSession,
    const xllm_stream_callbacks* pCallbacks /* optional */, xllm_response** ppResponse,
    xllm_error* pError);

/* Test seam: scriptable model call replacing the bound client (usage fully
 * controllable for offline governance lifecycle tests). */
typedef xllm_result (*xllm_test_call_proc)(void* pUserData, const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks, xllm_response** ppResponse, xllm_error* pError);
xllm_session* xllmSessionCreateForTest(const xllm_session_config* pConfig,
    xllm_test_call_proc pCall, void* pUserData, xllm_error* pError);

#ifdef __cplusplus
}
#endif

#endif
