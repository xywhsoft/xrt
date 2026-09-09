#ifndef XLLM_SESSION_INTERNAL_H
#define XLLM_SESSION_INTERNAL_H

#include "../xllm-session.h"
#include "../xllm-xrt.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct xllm_session_entry {
    uint64_t uSequence;
    uint64_t uTurn;
    uint64_t uEstimatedTokens;
    uint32_t uFlags;
    xllm_message tMessage;
} xllm_session_entry;

typedef struct xllm_session_buf {
    char* pData;
    size_t iLen;
    size_t iCap;
} xllm_session_buf;

struct xllm_session {
    xllm_session_config tConfig;
    xllm_session_entry* pEntries;
    size_t iEntryCount;
    size_t iEntryCap;
    uint64_t uNextSequence;
    uint64_t uCurrentTurn;
    uint64_t uCompactedThrough;
    uint64_t uCompactionCount;
    uint64_t uJournalSequence;
    char* sSummary;
    char* sJournalPath;
};

struct xllm_compaction {
    xllm_session* pSession;
    uint64_t uBaseCompactedThrough;
    uint64_t uThroughSequence;
    uint64_t uEstimatedTokens;
    char* sPrompt;
    bool bCommitted;
};

char* xllm_session__strdup(const char* sText);
bool xllm_session__message_clone(xllm_message* pDst, const xllm_message* pSrc);
bool xllm_session__buf_append(xllm_session_buf* pBuf, const void* pData, size_t iLen);
bool xllm_session__buf_cstr(xllm_session_buf* pBuf, const char* sText);
bool xllm_session__buf_char(xllm_session_buf* pBuf, char ch);
bool xllm_session__buf_u64(xllm_session_buf* pBuf, uint64_t uValue);
bool xllm_session__json_string(xllm_session_buf* pBuf, const char* sText);
char* xllm_session__buf_detach(xllm_session_buf* pBuf);
void xllm_session__buf_unit(xllm_session_buf* pBuf);
void xllm_session__error(xllm_error* pError, xllm_error_code eCode, const char* sMessage);
uint64_t xllm_session__input_budget(const xllm_session* pSession);
bool xllm_session__entry_is_active(const xllm_session* pSession, const xllm_session_entry* pEntry);
bool xllm_session__should_prune_tool(const xllm_session* pSession, const xllm_session_entry* pEntry);
uint32_t xllm_session__pending_tool_calls(const xllm_session* pSession);
bool xllm_session__journal_append_turn(xllm_session* pSession, uint64_t uTurn);
bool xllm_session__journal_append_entry(xllm_session* pSession, const xllm_session_entry* pEntry);
bool xllm_session__journal_append_compaction(xllm_session* pSession, uint64_t uThroughSequence,
    uint64_t uCompactionCount, const char* sSummary);
bool xllm_session__write_entry(xllm_session_buf* pJson, const xllm_session_entry* pEntry);
xvalue* xllm_session__json_get(xvalue* pObject, const char* sKey);
const char* xllm_session__json_text(xvalue* pObject, const char* sKey);
uint64_t xllm_session__json_u64(xvalue* pObject, const char* sKey, uint64_t uDefault);
double xllm_session__json_double(xvalue* pObject, const char* sKey, double fDefault);
bool xllm_session__load_message(xllm_message* pMessage, xvalue* pEntry);

#endif
