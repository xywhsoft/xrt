#include "xllm_internal.h"

/* Independent SSE framing limits: the HTTP body cap does not cover the
 * framing buffers, so a peer that never terminates a line or an event block
 * must not be able to grow them without bound. */
#define XLLM_SSE_LINE_LIMIT  (1024u * 1024u)
#define XLLM_SSE_EVENT_LIMIT (16u * 1024u * 1024u)

/* SSE framing: split the byte stream into lines, collect field values into
 * per-event buffers, and dispatch the assembled field set to the active
 * dialect when a blank line closes an event. Arbitrary fragmentation is
 * safe; a final unterminated line is flushed by xllm__sse_finish. */

static bool xllm__sse_dispatch(xllm_call* pCall)
{
    bool bOk = true;
    if ( pCall->tEventData.iLen ) {
        xllm_sse_fields tFields;
        tFields.tEvent.Data = pCall->bHaveEventName && pCall->tEventName.iLen
            ? pCall->tEventName.pData : NULL;
        tFields.tEvent.Size = pCall->bHaveEventName ? pCall->tEventName.iLen : 0u;
        tFields.tData.Data = pCall->tEventData.pData;
        tFields.tData.Size = pCall->tEventData.iLen;
        tFields.tId.Data = NULL;
        tFields.tId.Size = 0u;
        bOk = pCall->pDialect->DecodeSseEvent(pCall, &tFields);
    }
    pCall->tEventData.iLen = 0u;
    if ( pCall->tEventData.pData ) { pCall->tEventData.pData[0] = '\0'; }
    pCall->tEventName.iLen = 0u;
    pCall->bHaveEventName = false;
    return bOk;
}

static bool xllm__sse_process_line(xllm_call* pCall, const char* sLine, size_t iLen)
{
    const char* sValue;
    size_t iValueLen;
    bool bData;
    if ( iLen && sLine[iLen - 1u] == '\r' ) { --iLen; }
    if ( iLen == 0u ) { return xllm__sse_dispatch(pCall); }
    if ( sLine[0] == ':' ) { return true; }
    if ( iLen >= 5u && memcmp(sLine, "data:", 5u) == 0 ) {
        sValue = sLine + 5u;
        iValueLen = iLen - 5u;
        bData = true;
    } else if ( iLen >= 6u && memcmp(sLine, "event:", 6u) == 0 ) {
        sValue = sLine + 6u;
        iValueLen = iLen - 6u;
        bData = false;
    } else if ( iLen >= 3u && memcmp(sLine, "id:", 3u) == 0 ) {
        /* Collected by the framing contract but unused by shipped dialects. */
        return true;
    } else if ( iLen >= 6u && memcmp(sLine, "retry:", 6u) == 0 ) {
        return true;
    } else {
        return true;
    }
    if ( iValueLen && *sValue == ' ' ) { ++sValue; --iValueLen; }
    if ( bData ) {
        if ( pCall->tEventData.iLen > XLLM_SSE_EVENT_LIMIT - iValueLen - 1u ) {
            xllm__error_set(&pCall->tError, XLLM_ERROR_PROTOCOL,
                "provider event exceeds the SSE event size limit");
            return false;
        }
        if ( pCall->tEventData.iLen && !xllm__buf_append_char(&pCall->tEventData, '\n') ) return false;
        return xllm__buf_append(&pCall->tEventData, sValue, iValueLen);
    }
    if ( pCall->tEventName.iLen + iValueLen > XLLM_SSE_LINE_LIMIT ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PROTOCOL,
            "provider event name exceeds the SSE line size limit");
        return false;
    }
    if ( pCall->tEventName.iLen && !xllm__buf_append_char(&pCall->tEventName, '\n') ) return false;
    pCall->bHaveEventName = true;
    return xllm__buf_append(&pCall->tEventName, sValue, iValueLen);
}

bool xllm__sse_feed(xllm_call* pCall, const void* pData, size_t iLen)
{
    const unsigned char* p = (const unsigned char*)pData;
    size_t iStart = 0u;
    size_t i;
    if ( !pCall || !pCall->pDialect || (!pData && iLen) ) { return false; }
    for ( i = 0u; i < iLen; ++i ) {
        if ( p[i] != '\n' ) { continue; }
        if ( i > iStart && !xllm__buf_append(&pCall->tLine, p + iStart, i - iStart) ) goto oom;
        if ( !xllm__sse_process_line(pCall, pCall->tLine.pData ? pCall->tLine.pData : "", pCall->tLine.iLen) ) return false;
        pCall->tLine.iLen = 0u;
        if ( pCall->tLine.pData ) { pCall->tLine.pData[0] = '\0'; }
        iStart = i + 1u;
    }
    if ( iStart < iLen ) {
        if ( pCall->tLine.iLen + (iLen - iStart) > XLLM_SSE_LINE_LIMIT ) {
            xllm__error_set(&pCall->tError, XLLM_ERROR_PROTOCOL,
                "provider event exceeds the SSE line size limit");
            return false;
        }
        if ( !xllm__buf_append(&pCall->tLine, p + iStart, iLen - iStart) ) goto oom;
    }
    return true;
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to buffer provider event stream");
    return false;
}

bool xllm__sse_finish(xllm_call* pCall)
{
    if ( !pCall ) { return false; }
    if ( pCall->tLine.iLen ) {
        if ( !xllm__sse_process_line(pCall, pCall->tLine.pData, pCall->tLine.iLen) ) return false;
        pCall->tLine.iLen = 0u;
    }
    if ( pCall->tEventData.iLen && !xllm__sse_dispatch(pCall) ) return false;
    return true;
}

void xllm__sse_reset(xllm_call* pCall)
{
    if ( !pCall ) { return; }
    pCall->tEventData.iLen = 0u;
    pCall->tEventName.iLen = 0u;
    pCall->tLine.iLen = 0u;
    pCall->bHaveEventName = false;
}
