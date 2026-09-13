#ifndef XLLM_MEMORY_INTERNAL_H
#define XLLM_MEMORY_INTERNAL_H

#include "../xllm-memory.h"
#include "../xllm-memory-xrt.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct xllm_memory_record_internal {
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
} xllm_memory_record_internal;

typedef struct xllm_memory_buf {
    char* pData;
    size_t iLen;
    size_t iCap;
} xllm_memory_buf;

struct xllm_memory {
    char* sPath;
    char* sNamespace;
    xllm_memory_record_internal* pRecords;
    size_t iRecordCount;
    size_t iRecordCap;
    uint64_t uStoreRevision;
};

char* xllm_memory__strdup(const char* sText);
char* xllm_memory__strndup(const char* sText, size_t iLen);
void xllm_memory__error(xllm_error* pError, xllm_error_code eCode, const char* sMessage);
int64_t xllm_memory__now_unix(void);
bool xllm_memory__buf_append(xllm_memory_buf* pBuf, const void* pData, size_t iLen);
bool xllm_memory__buf_cstr(xllm_memory_buf* pBuf, const char* sText);
bool xllm_memory__buf_char(xllm_memory_buf* pBuf, char ch);
bool xllm_memory__buf_u64(xllm_memory_buf* pBuf, uint64_t uValue);
bool xllm_memory__buf_i64(xllm_memory_buf* pBuf, int64_t iValue);
bool xllm_memory__buf_double(xllm_memory_buf* pBuf, double fValue);
bool xllm_memory__json_string(xllm_memory_buf* pBuf, const char* sText);
char* xllm_memory__buf_detach(xllm_memory_buf* pBuf);
void xllm_memory__buf_unit(xllm_memory_buf* pBuf);
const xllm_memory_record_internal* xllm_memory__find(const xllm_memory* pMemory,
    const char* sRecordId, size_t* pIndex);

#endif
