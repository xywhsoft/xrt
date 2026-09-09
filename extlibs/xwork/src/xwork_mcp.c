#include "xwork_internal.h"

/* Stable MCP baseline implemented here: protocol revision 2025-06-18 over
 * stdio. The transport is deliberately independent of shell parsing: callers
 * provide a program plus argv, and every wire message occupies one UTF-8 line. */

#define XWORK_MCP_PROTOCOL_DEFAULT "2025-06-18"
#define XWORK_MCP_TIMEOUT_DEFAULT 30000u
#define XWORK_MCP_MESSAGE_DEFAULT (4u * 1024u * 1024u)
#define XWORK_MCP_TOOLS_DEFAULT 1024u
#define XWORK_MCP_WIRE_NAME_MAX 64u

typedef struct xwork_mcp_tool_proxy {
    struct xwork_mcp_client* pClient;
    char* sRemoteName;
    char* sWireName;
    char* sDescription;
    char* sParametersJson;
    xwork_tool_effect eEffect;
} xwork_mcp_tool_proxy;

struct xwork_mcp_client {
    char* sServerName;
    char* sProgram;
    char** psArguments;
    size_t iArgumentCount;
    char* sWorkingDirectory;
    char* sRequestedProtocolVersion;
    char* sNegotiatedProtocolVersion;
    char* sToolSource;
    uint32_t uRequestTimeoutMs;
    size_t iMaxMessageBytes;
    size_t iMaxTools;
    xwork_tool_effect eDefaultToolEffect;
    bool bTrustReadOnlyAnnotations;
    bool bConnected;
    bool bServerSupportsToolListChanges;
    xcancel* pCancel;
    uint64_t uDeadline;
    xprocess* pProcess;
    xwork_process_capture* pCapture;
    uint64_t uStdoutOffset;
    uint64_t uNextRequestId;
    uint64_t uRequestsCompleted;
    xwork_buf tReadBuffer;
    xwork_mcp_tool_proxy* pTools;
    size_t iToolCount;
    size_t iToolCap;
    volatile long iBusy;
};

static bool xwork__mcp_try_lock(xwork_mcp_client* pClient)
{
#if defined(_MSC_VER)
    return _InterlockedCompareExchange(&pClient->iBusy, 1, 0) == 0;
#elif defined(__GNUC__) || defined(__clang__)
    long iExpected = 0;
    return __atomic_compare_exchange_n(&pClient->iBusy, &iExpected, 1, false,
        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    if ( pClient->iBusy ) return false;
    pClient->iBusy = 1;
    return true;
#endif
}

static void xwork__mcp_unlock(xwork_mcp_client* pClient)
{
    xwork__atomic_store(&pClient->iBusy, 0);
}

static bool xwork__mcp_effect_valid(xwork_tool_effect eEffect)
{
    return eEffect >= XWORK_TOOL_EFFECT_READ_ONLY && eEffect <= XWORK_TOOL_EFFECT_PROCESS;
}

static void xwork__mcp_proxy_unit(xwork_mcp_tool_proxy* pTool)
{
    if ( !pTool ) return;
    free(pTool->sRemoteName);
    free(pTool->sWireName);
    free(pTool->sDescription);
    free(pTool->sParametersJson);
    memset(pTool, 0, sizeof(*pTool));
}

static void xwork__mcp_tools_unit(xwork_mcp_tool_proxy* pTools, size_t iCount)
{
    size_t i;
    for ( i = 0u; i < iCount; ++i ) xwork__mcp_proxy_unit(&pTools[i]);
    free(pTools);
}

static void xwork__mcp_process_unit(xwork_mcp_client* pClient)
{
    if ( !pClient || !pClient->pProcess ) return;
    (void)xrtProcessClose(pClient->pProcess, XPROCESS_STDIN);
    if ( xrtProcessWaitFor(pClient->pProcess, UINT64_C(200000)) == XWAIT_TIMEOUT ) {
        (void)xrtProcessKillTree(pClient->pProcess);
        (void)xrtProcessWait(pClient->pProcess);
    }
    xwork__process_capture_destroy(pClient->pCapture);
    pClient->pCapture = NULL;
    xrtProcessDestroy(pClient->pProcess);
    pClient->pProcess = NULL;
    pClient->bConnected = false;
}

static void xwork__mcp_set_context_error(xwork_error* pError, xwork_operation_status eStatus)
{
    if ( eStatus == XWORK_OPERATION_TIMED_OUT ) {
        xwork__set_error(pError, XWORK_ERROR_TIMEOUT, "MCP request deadline exceeded");
    } else {
        xwork__set_error(pError, XWORK_ERROR_CANCELLED, "MCP request cancelled");
    }
}

static xwork_operation_status xwork__mcp_context_status(
    const xwork_mcp_client* pClient,
    const xcancel* pOperationCancel,
    uint64_t uOperationDeadline
)
{
    if ( pOperationCancel && xrtCancelRequested(pOperationCancel) )
        return XWORK_OPERATION_CANCELLED;
    if ( pClient && pClient->pCancel && xrtCancelRequested(pClient->pCancel) )
        return XWORK_OPERATION_CANCELLED;
    if ( uOperationDeadline != XRT_DEADLINE_NEVER && xrtDeadlineExpired(uOperationDeadline) )
        return XWORK_OPERATION_TIMED_OUT;
    if ( pClient && pClient->uDeadline != XRT_DEADLINE_NEVER &&
         xrtDeadlineExpired(pClient->uDeadline) ) return XWORK_OPERATION_TIMED_OUT;
    return XWORK_OPERATION_ACTIVE;
}

static bool xwork__mcp_write_all(xwork_mcp_client* pClient, const char* sData, size_t iSize)
{
    size_t iOffset = 0u;
    while ( iOffset < iSize ) {
        int64_t iWritten = xrtProcessWrite(pClient->pProcess, sData + iOffset, iSize - iOffset);
        if ( iWritten <= 0 ) return false;
        iOffset += (size_t)iWritten;
    }
    return true;
}

static bool xwork__mcp_send_notification(
    xwork_mcp_client* pClient,
    const char* sMethod,
    const char* sParamsJson
)
{
    xwork_buf tWire = {0};
    bool bOk = xwork__buf_append_cstr(&tWire, "{\"jsonrpc\":\"2.0\",\"method\":") &&
        xwork__json_string(&tWire, sMethod) &&
        xwork__buf_append_cstr(&tWire, ",\"params\":") &&
        xwork__buf_append_cstr(&tWire, sParamsJson ? sParamsJson : "{}") &&
        xwork__buf_append_cstr(&tWire, "}\n") &&
        xwork__mcp_write_all(pClient, tWire.pData, tWire.iLen);
    xwork__buf_unit(&tWire);
    return bOk;
}

static char* xwork__mcp_take_line(xwork_mcp_client* pClient)
{
    size_t i;
    size_t iLineLen;
    size_t iConsumed;
    char* sLine;
    for ( i = 0u; i < pClient->tReadBuffer.iLen; ++i ) {
        if ( pClient->tReadBuffer.pData[i] == '\n' ) break;
    }
    if ( i == pClient->tReadBuffer.iLen ) return NULL;
    iLineLen = i;
    if ( iLineLen && pClient->tReadBuffer.pData[iLineLen - 1u] == '\r' ) --iLineLen;
    sLine = (char*)malloc(iLineLen + 1u);
    if ( !sLine ) return NULL;
    memcpy(sLine, pClient->tReadBuffer.pData, iLineLen);
    sLine[iLineLen] = '\0';
    iConsumed = i + 1u;
    memmove(pClient->tReadBuffer.pData,
        pClient->tReadBuffer.pData + iConsumed,
        pClient->tReadBuffer.iLen - iConsumed);
    pClient->tReadBuffer.iLen -= iConsumed;
    if ( pClient->tReadBuffer.pData ) pClient->tReadBuffer.pData[pClient->tReadBuffer.iLen] = '\0';
    return sLine;
}

static char* xwork__mcp_stderr_tail(xwork_mcp_client* pClient)
{
    size_t iSize = 0u;
    uint64_t uBase = 0u;
    uint64_t uNext = 0u;
    char* sData = (char*)xwork__process_capture_since(
        pClient->pCapture, true, 0u, SIZE_MAX, &iSize, &uBase, &uNext);
    char* sTail;
    size_t iStart = iSize > 2048u ? iSize - 2048u : 0u;
    if ( !sData || !iSize ) { free(sData); return NULL; }
    sTail = (char*)malloc(iSize - iStart + 1u);
    if ( sTail ) {
        memcpy(sTail, sData + iStart, iSize - iStart);
        sTail[iSize - iStart] = '\0';
    }
    free(sData);
    return sTail;
}

static char* xwork__mcp_read_message(
    xwork_mcp_client* pClient,
    const xcancel* pOperationCancel,
    uint64_t uOperationDeadline,
    uint64_t uDeadlineMs,
    xwork_error* pError
)
{
    for ( ;; ) {
        char* sLine = xwork__mcp_take_line(pClient);
        xwork_operation_status eContextStatus;
        if ( sLine ) {
            if ( !sLine[0] ) {
                free(sLine);
                xwork__set_error(pError, XWORK_ERROR_TOOL,
                    "MCP stdout contained an empty non-message line");
                return NULL;
            }
            return sLine;
        }
        if ( pClient->tReadBuffer.iLen > pClient->iMaxMessageBytes ) {
            xwork__set_error(pError, XWORK_ERROR_TOOL, "MCP message exceeds configured byte limit");
            return NULL;
        }
        {
            size_t iSize = 0u;
            char* sChunk;
            uint64_t uBaseOffset = 0u;
            uint64_t uNextOffset = pClient->uStdoutOffset;
            sChunk = (char*)xwork__process_capture_since(
                pClient->pCapture, false,
                pClient->uStdoutOffset,
                pClient->iMaxMessageBytes + 1u,
                &iSize,
                &uBaseOffset,
                &uNextOffset
            );
            if ( uBaseOffset > pClient->uStdoutOffset ) {
                free(sChunk);
                xwork__set_error(pError, XWORK_ERROR_IO, "MCP stdout capture was truncated");
                return NULL;
            }
            pClient->uStdoutOffset = uNextOffset;
            if ( sChunk && iSize ) {
                bool bAppended = xwork__buf_append(&pClient->tReadBuffer, sChunk, iSize);
                free(sChunk);
                if ( !bAppended ) {
                    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to buffer MCP response");
                    return NULL;
                }
                continue;
            }
            free(sChunk);
        }
        eContextStatus = xwork__mcp_context_status(
            pClient, pOperationCancel, uOperationDeadline);
        if ( eContextStatus != XWORK_OPERATION_ACTIVE ) {
            xwork__mcp_set_context_error(pError, eContextStatus);
            return NULL;
        }
        if ( xrtClock() / UINT64_C(1000) >= uDeadlineMs ) {
            xwork__set_error(pError, XWORK_ERROR_TIMEOUT, "MCP request timed out");
            return NULL;
        }
        if ( !xwork__process_running(pClient->pProcess) ) {
            char* sTail = xwork__mcp_stderr_tail(pClient);
            if ( sTail && sTail[0] ) {
                snprintf(pError->sMessage, sizeof(pError->sMessage),
                    "MCP server exited before responding: %.900s", sTail);
                pError->eCode = XWORK_ERROR_IO;
            } else {
                xwork__set_error(pError, XWORK_ERROR_IO, "MCP server exited before responding");
            }
            free(sTail);
            return NULL;
        }
        xrtSleep(5u);
    }
}

static void xwork__mcp_cancel_request(xwork_mcp_client* pClient, uint64_t uRequestId)
{
    char sParams[160];
    snprintf(sParams, sizeof(sParams),
        "{\"requestId\":%llu,\"reason\":\"client operation cancelled\"}",
        (unsigned long long)uRequestId);
    (void)xwork__mcp_send_notification(pClient, "notifications/cancelled", sParams);
}

static xvalue* xwork__mcp_request(
    xwork_mcp_client* pClient,
    const char* sMethod,
    const char* sParamsJson,
    const xcancel* pOperationCancel,
    uint64_t uOperationDeadline,
    bool bCancellable,
    xwork_error* pError
)
{
    xwork_buf tWire = {0};
    uint64_t uRequestId = ++pClient->uNextRequestId;
    uint64_t uStartedMs = xrtClock() / UINT64_C(1000);
    uint64_t uDeadlineMs = uStartedMs + pClient->uRequestTimeoutMs;
    xwork_operation_status eContextStatus = xwork__mcp_context_status(
        pClient, pOperationCancel, uOperationDeadline);
    if ( eContextStatus != XWORK_OPERATION_ACTIVE ) {
        xwork__mcp_set_context_error(pError, eContextStatus);
        return NULL;
    }
    if ( !xwork__buf_appendf(&tWire, "{\"jsonrpc\":\"2.0\",\"id\":%llu,\"method\":",
            (unsigned long long)uRequestId) ||
         !xwork__json_string(&tWire, sMethod) ||
         !xwork__buf_append_cstr(&tWire, ",\"params\":") ||
         !xwork__buf_append_cstr(&tWire, sParamsJson ? sParamsJson : "{}") ||
         !xwork__buf_append_cstr(&tWire, "}\n") ) {
        xwork__buf_unit(&tWire);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build MCP request");
        return NULL;
    }
    if ( !xwork__mcp_write_all(pClient, tWire.pData, tWire.iLen) ) {
        xwork__buf_unit(&tWire);
        xwork__set_error(pError, XWORK_ERROR_IO, "failed to write MCP request");
        return NULL;
    }
    xwork__buf_unit(&tWire);
    for ( ;; ) {
        char* sMessage = xwork__mcp_read_message(
            pClient, pOperationCancel, uOperationDeadline, uDeadlineMs, pError);
        xvalue* tRoot;
        xvalue* tId;
        xvalue* tError;
        int64 iId = -1;
        const char* sJsonRpc;
        if ( !sMessage ) {
            if ( bCancellable && (pError->eCode == XWORK_ERROR_CANCELLED ||
                                  pError->eCode == XWORK_ERROR_TIMEOUT) ) {
                xwork__mcp_cancel_request(pClient, uRequestId);
            }
            return NULL;
        }
        tRoot = xrtJsonParse((xstrview){ sMessage, strlen(sMessage) });
        free(sMessage);
        if ( !tRoot || xrtValueType(tRoot) != XVALUE_OBJECT ) {
            if ( tRoot ) xrtValueRelease(tRoot);
            xwork__set_error(pError, XWORK_ERROR_TOOL,
                "MCP stdout contained a non-object JSON-RPC message");
            return NULL;
        }
        sJsonRpc = xwork__json_text(tRoot, "jsonrpc");
        if ( !sJsonRpc || strcmp(sJsonRpc, "2.0") != 0 ) {
            xrtValueRelease(tRoot);
            xwork__set_error(pError, XWORK_ERROR_TOOL, "MCP response has an invalid jsonrpc version");
            return NULL;
        }
        tId = xwork__json_get(tRoot, "id");
        if ( !tId ) {
            /* Notifications are asynchronous and may legally interleave a
             * response. list_changed is reflected in the next explicit refresh. */
            xrtValueRelease(tRoot);
            continue;
        }
        if ( !xrtValueGetInt(tId, &iId) || iId < 0 ||
             (uint64_t)iId != uRequestId ) {
            xrtValueRelease(tRoot);
            continue;
        }
        tError = xwork__json_get(tRoot, "error");
        if ( tError ) {
            const char* sRemoteMessage = xwork__json_text(tError, "message");
            snprintf(pError->sMessage, sizeof(pError->sMessage),
                "MCP protocol error: %.900s", sRemoteMessage ? sRemoteMessage : "unknown error");
            pError->eCode = XWORK_ERROR_TOOL;
            xrtValueRelease(tRoot);
            return NULL;
        }
        if ( !xwork__json_get(tRoot, "result") ) {
            xrtValueRelease(tRoot);
            xwork__set_error(pError, XWORK_ERROR_TOOL, "MCP response has neither result nor error");
            return NULL;
        }
        ++pClient->uRequestsCompleted;
        return tRoot;
    }
}

static uint32_t xwork__mcp_name_hash(const char* sText)
{
    uint32_t uHash = 2166136261u;
    const unsigned char* p = (const unsigned char*)sText;
    while ( p && *p ) { uHash ^= *p++; uHash *= 16777619u; }
    return uHash;
}

static void xwork__mcp_append_name_part(char* sOut, size_t iCap, size_t* piLen, const char* sText)
{
    const unsigned char* p = (const unsigned char*)sText;
    while ( p && *p && *piLen + 1u < iCap ) {
        unsigned char ch = *p++;
        sOut[(*piLen)++] = (char)(((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == '_' || ch == '-') ? ch : '_');
    }
    sOut[*piLen] = '\0';
}

static char* xwork__mcp_wire_name(const char* sServer, const char* sRemote, uint32_t uCollisionSalt)
{
    char sName[XWORK_MCP_WIRE_NAME_MAX + 1u];
    char sHash[16];
    size_t iLen = 0u;
    memset(sName, 0, sizeof(sName));
    memcpy(sName, "mcp__", 5u);
    iLen = 5u;
    xwork__mcp_append_name_part(sName, 24u, &iLen, sServer);
    if ( iLen + 2u < sizeof(sName) ) { sName[iLen++] = '_'; sName[iLen++] = '_'; sName[iLen] = '\0'; }
    xwork__mcp_append_name_part(sName,
        uCollisionSalt ? sizeof(sName) - 11u : sizeof(sName), &iLen, sRemote);
    if ( uCollisionSalt ) {
        snprintf(sHash, sizeof(sHash), "__%08x", uCollisionSalt);
        xwork__mcp_append_name_part(sName, sizeof(sName), &iLen, sHash);
    }
    return xwork__strdup(sName);
}

static bool xwork__mcp_proxy_name_exists(
    const xwork_mcp_tool_proxy* pTools,
    size_t iCount,
    const char* sWireName
)
{
    size_t i;
    for ( i = 0u; i < iCount; ++i ) {
        if ( strcmp(pTools[i].sWireName, sWireName) == 0 ) return true;
    }
    return false;
}

static bool xwork__mcp_proxy_remote_exists(
    const xwork_mcp_tool_proxy* pTools,
    size_t iCount,
    const char* sRemoteName
)
{
    size_t i;
    for ( i = 0u; i < iCount; ++i ) {
        if ( strcmp(pTools[i].sRemoteName, sRemoteName) == 0 ) return true;
    }
    return false;
}

static bool xwork__mcp_append_proxy(
    xwork_mcp_client* pClient,
    xwork_mcp_tool_proxy** ppTools,
    size_t* piCount,
    size_t* piCap,
    xvalue* tTool,
    xwork_error* pError
)
{
    const char* sRemoteName = xwork__json_text(tTool, "name");
    const char* sDescription = xwork__json_text(tTool, "description");
    xvalue* tSchema = xwork__json_get(tTool, "inputSchema");
    xvalue* tAnnotations = xwork__json_get(tTool, "annotations");
    xwork_mcp_tool_proxy* pProxy;
    xwork_mcp_tool_proxy* pNew;
    char* sSchema = NULL;
    size_t iSchema = 0u;
    bool bReadOnly = false;
    bool bValid = true;
    uint32_t uSalt = 0u;
    if ( !sRemoteName || !sRemoteName[0] || !tSchema ||
         xrtValueType(tSchema) != XVALUE_OBJECT ) {
        xwork__set_error(pError, XWORK_ERROR_TOOL,
            "MCP tools/list returned a tool without a name or object inputSchema");
        return false;
    }
    if ( xwork__mcp_proxy_remote_exists(*ppTools, *piCount, sRemoteName) ) {
        xwork__set_error(pError, XWORK_ERROR_TOOL, "MCP tools/list returned duplicate tool names");
        return false;
    }
    if ( *piCount >= pClient->iMaxTools ) {
        xwork__set_error(pError, XWORK_ERROR_TOOL, "MCP tool count exceeds configured limit");
        return false;
    }
    if ( *piCount == *piCap ) {
        size_t iCap = *piCap ? *piCap * 2u : 8u;
        if ( iCap > pClient->iMaxTools ) iCap = pClient->iMaxTools;
        pNew = (xwork_mcp_tool_proxy*)realloc(*ppTools, iCap * sizeof(*pNew));
        if ( !pNew ) {
            xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to grow MCP tool list");
            return false;
        }
        memset(pNew + *piCap, 0, (iCap - *piCap) * sizeof(*pNew));
        *ppTools = pNew;
        *piCap = iCap;
    }
    pProxy = &(*ppTools)[*piCount];
    memset(pProxy, 0, sizeof(*pProxy));
    pProxy->pClient = pClient;
    pProxy->sRemoteName = xwork__strdup(sRemoteName);
    pProxy->sDescription = xwork__strdup(sDescription ? sDescription : "MCP tool");
    sSchema = xrtJsonStringify(tSchema, NULL, &iSchema);
    if ( sSchema ) pProxy->sParametersJson = xwork__strdup(sSchema);
    if ( sSchema ) xrtFree(sSchema);
    pProxy->sWireName = xwork__mcp_wire_name(pClient->sServerName, sRemoteName, 0u);
    if ( pProxy->sWireName && xwork__mcp_proxy_name_exists(*ppTools, *piCount, pProxy->sWireName) ) {
        free(pProxy->sWireName);
        uSalt = xwork__mcp_name_hash(sRemoteName);
        pProxy->sWireName = xwork__mcp_wire_name(pClient->sServerName, sRemoteName, uSalt);
    }
    if ( pClient->bTrustReadOnlyAnnotations && tAnnotations &&
         xrtValueType(tAnnotations) == XVALUE_OBJECT ) {
        bReadOnly = xwork__json_bool(tAnnotations, "readOnlyHint", false, &bValid);
    }
    pProxy->eEffect = bValid && bReadOnly
        ? XWORK_TOOL_EFFECT_READ_ONLY : pClient->eDefaultToolEffect;
    if ( !pProxy->sRemoteName || !pProxy->sWireName || !pProxy->sDescription ||
         !pProxy->sParametersJson ) {
        xwork__mcp_proxy_unit(pProxy);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy MCP tool definition");
        return false;
    }
    ++*piCount;
    return true;
}

static bool xwork__mcp_discover_tools(
    xwork_mcp_client* pClient,
    xwork_mcp_tool_proxy** ppTools,
    size_t* piCount,
    size_t* piCap,
    xwork_error* pError
)
{
    char* sCursor = NULL;
    uint32_t uPages = 0u;
    for ( ;; ) {
        xwork_buf tParams = {0};
        xvalue* tRoot = NULL;
        xvalue* tResult;
        xvalue* tTools;
        const char* sNextCursor;
        uint32_t i;
        bool bOk;
        if ( sCursor ) {
            bOk = xwork__buf_append_cstr(&tParams, "{\"cursor\":") &&
                xwork__json_string(&tParams, sCursor) && xwork__buf_append_char(&tParams, '}');
        } else {
            bOk = xwork__buf_append_cstr(&tParams, "{}");
        }
        if ( !bOk ) {
            xwork__buf_unit(&tParams);
            free(sCursor);
            xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build tools/list request");
            return false;
        }
        tRoot = xwork__mcp_request(pClient, "tools/list", tParams.pData,
            NULL, XRT_DEADLINE_NEVER, true, pError);
        xwork__buf_unit(&tParams);
        if ( !tRoot ) { free(sCursor); return false; }
        tResult = xwork__json_get(tRoot, "result");
        tTools = xwork__json_get(tResult, "tools");
        if ( !tResult || xrtValueType(tResult) != XVALUE_OBJECT || !tTools ||
             xrtValueType(tTools) != XVALUE_ARRAY ) {
            xrtValueRelease(tRoot);
            free(sCursor);
            xwork__set_error(pError, XWORK_ERROR_TOOL, "MCP tools/list result has no tools array");
            return false;
        }
        for ( i = 0u; i < xrtValueCount(tTools); ++i ) {
            xvalue* tTool = xrtValueArrayGet(tTools, i);
            if ( !tTool || xrtValueType(tTool) != XVALUE_OBJECT ||
                 !xwork__mcp_append_proxy(pClient, ppTools, piCount, piCap, tTool, pError) ) {
                xrtValueRelease(tRoot);
                free(sCursor);
                return false;
            }
        }
        sNextCursor = xwork__json_text(tResult, "nextCursor");
        if ( !sNextCursor || !sNextCursor[0] ) {
            xrtValueRelease(tRoot);
            free(sCursor);
            return true;
        }
        if ( sCursor && strcmp(sCursor, sNextCursor) == 0 ) {
            xrtValueRelease(tRoot);
            free(sCursor);
            xwork__set_error(pError, XWORK_ERROR_TOOL, "MCP tools/list repeated its pagination cursor");
            return false;
        }
        {
            char* sCopy = xwork__strdup(sNextCursor);
            xrtValueRelease(tRoot);
            if ( !sCopy ) {
                free(sCursor);
                xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy MCP cursor");
                return false;
            }
            free(sCursor);
            sCursor = sCopy;
        }
        if ( ++uPages >= 64u ) {
            free(sCursor);
            xwork__set_error(pError, XWORK_ERROR_TOOL, "MCP tools/list exceeded pagination limit");
            return false;
        }
    }
}

static xwork_result xwork__mcp_tool_execute(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_mcp_tool_proxy* pProxy = (xwork_mcp_tool_proxy*)pUserData;
    xcancel* pOperationCancel = pContext && pContext->pAgent
        ? pContext->pAgent->pCancel : NULL;
    uint64_t uOperationDeadline = pContext && pContext->pAgent
        ? pContext->pAgent->uDeadline : XRT_DEADLINE_NEVER;
    return xworkMcpClientCallTool(
        pProxy->pClient, pProxy->sRemoteName, sArgumentsJson,
        pOperationCancel, uOperationDeadline, pOutput, pError);
}

void xworkMcpStdioConfigInit(xwork_mcp_stdio_config* pConfig)
{
    if ( !pConfig ) return;
    memset(pConfig, 0, sizeof(*pConfig));
    pConfig->sProtocolVersion = XWORK_MCP_PROTOCOL_DEFAULT;
    pConfig->uRequestTimeoutMs = XWORK_MCP_TIMEOUT_DEFAULT;
    pConfig->iMaxMessageBytes = XWORK_MCP_MESSAGE_DEFAULT;
    pConfig->iMaxTools = XWORK_MCP_TOOLS_DEFAULT;
    pConfig->eDefaultToolEffect = XWORK_TOOL_EFFECT_PROCESS;
    pConfig->uDeadline = XRT_DEADLINE_NEVER;
}

xwork_mcp_client* xworkMcpClientCreate(
    const xwork_mcp_stdio_config* pConfig,
    xwork_error* pError
)
{
    xwork_mcp_client* pClient;
    size_t i;
    xworkErrorInit(pError);
    if ( !pConfig || !pConfig->sServerName || !pConfig->sServerName[0] ||
         !pConfig->sProgram || !pConfig->sProgram[0] ||
         (pConfig->iArgumentCount && !pConfig->psArguments) ||
         pConfig->iArgumentCount > UINT32_MAX ||
         !xwork__mcp_effect_valid(pConfig->eDefaultToolEffect) ) {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "invalid MCP stdio configuration");
        return NULL;
    }
    pClient = (xwork_mcp_client*)calloc(1u, sizeof(*pClient));
    if ( !pClient ) {
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to allocate MCP client");
        return NULL;
    }
    pClient->sServerName = xwork__strdup(pConfig->sServerName);
    pClient->sProgram = xwork__strdup(pConfig->sProgram);
    pClient->sWorkingDirectory = pConfig->sWorkingDirectory && pConfig->sWorkingDirectory[0]
        ? xwork__strdup(pConfig->sWorkingDirectory) : NULL;
    pClient->sRequestedProtocolVersion = xwork__strdup(
        pConfig->sProtocolVersion && pConfig->sProtocolVersion[0]
            ? pConfig->sProtocolVersion : XWORK_MCP_PROTOCOL_DEFAULT);
    {
        xwork_buf tSource = {0};
        if ( xwork__buf_append_cstr(&tSource, "mcp:") &&
             xwork__buf_append_cstr(&tSource, pConfig->sServerName) ) {
            pClient->sToolSource = xwork__buf_detach(&tSource);
        }
        xwork__buf_unit(&tSource);
    }
    if ( pConfig->iArgumentCount ) {
        pClient->psArguments = (char**)calloc(pConfig->iArgumentCount, sizeof(char*));
    }
    pClient->iArgumentCount = pConfig->iArgumentCount;
    for ( i = 0u; i < pClient->iArgumentCount && pClient->psArguments; ++i ) {
        pClient->psArguments[i] = xwork__strdup(pConfig->psArguments[i] ? pConfig->psArguments[i] : "");
        if ( !pClient->psArguments[i] ) break;
    }
    pClient->uRequestTimeoutMs = pConfig->uRequestTimeoutMs
        ? pConfig->uRequestTimeoutMs : XWORK_MCP_TIMEOUT_DEFAULT;
    pClient->iMaxMessageBytes = pConfig->iMaxMessageBytes
        ? pConfig->iMaxMessageBytes : XWORK_MCP_MESSAGE_DEFAULT;
    pClient->iMaxTools = pConfig->iMaxTools ? pConfig->iMaxTools : XWORK_MCP_TOOLS_DEFAULT;
    pClient->eDefaultToolEffect = pConfig->eDefaultToolEffect;
    pClient->bTrustReadOnlyAnnotations = pConfig->bTrustReadOnlyAnnotations;
    pClient->pCancel = pConfig->pCancel;
    pClient->uDeadline = pConfig->uDeadline;
    if ( !pClient->sServerName || !pClient->sProgram || !pClient->sRequestedProtocolVersion ||
         !pClient->sToolSource ||
         (pConfig->sWorkingDirectory && pConfig->sWorkingDirectory[0] && !pClient->sWorkingDirectory) ||
         (pClient->iArgumentCount && (!pClient->psArguments || i != pClient->iArgumentCount)) ||
         pClient->iMaxMessageBytes < 1024u || pClient->iMaxTools == 0u ) {
        xworkMcpClientDestroy(pClient);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy MCP configuration");
        return NULL;
    }
    return pClient;
}

bool xworkMcpClientConnect(xwork_mcp_client* pClient, xwork_error* pError)
{
    xprocessconfig tConfig;
    xvalue* tRoot = NULL;
    xvalue* tResult;
    xvalue* tCapabilities;
    xvalue* tToolsCapability;
    const char* sProtocol;
    bool bValid = true;
    xwork_buf tParams = {0};
    bool bOk = false;
    xworkErrorInit(pError);
    if ( !pClient ) {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "MCP client is null");
        return false;
    }
    if ( !xwork__mcp_try_lock(pClient) ) {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "MCP client already has an active request");
        return false;
    }
    if ( pClient->bConnected ) { bOk = true; goto cleanup; }
    xwork__mcp_process_unit(pClient);
    pClient->uStdoutOffset = 0u;
    pClient->uNextRequestId = 0u;
    pClient->tReadBuffer.iLen = 0u;
    xrtProcessConfigInit(&tConfig);
    tConfig.Program = pClient->sProgram;
    tConfig.Args = (const cstr*)pClient->psArguments;
    tConfig.ArgCount = pClient->iArgumentCount;
    tConfig.WorkDir = pClient->sWorkingDirectory;
    tConfig.HideWindow = true;
    tConfig.NewGroup = true;
    tConfig.Stdin.Mode = XPROCESS_IO_PIPE;
    tConfig.Stdout.Mode = XPROCESS_IO_PIPE;
    tConfig.Stderr.Mode = XPROCESS_IO_PIPE;
    pClient->pProcess = xrtProcessSpawn(&tConfig);
    if ( !pClient->pProcess ) {
        xwork__set_error(pError, XWORK_ERROR_IO, "failed to start MCP stdio server");
        goto cleanup;
    }
    pClient->pCapture = xwork__process_capture_create(pClient->pProcess,
        pClient->iMaxMessageBytes <= SIZE_MAX / 4u
            ? pClient->iMaxMessageBytes * 4u : pClient->iMaxMessageBytes,
        true);
    if ( !pClient->pCapture ) {
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
            "failed to create MCP output capture");
        goto cleanup;
    }
    if ( !xwork__buf_append_cstr(&tParams, "{\"protocolVersion\":") ||
         !xwork__json_string(&tParams, pClient->sRequestedProtocolVersion) ||
         !xwork__buf_append_cstr(&tParams,
            ",\"capabilities\":{},\"clientInfo\":{\"name\":\"xwork\",\"version\":\"2.3.0\"}}") ) {
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build MCP initialize request");
        goto cleanup;
    }
    tRoot = xwork__mcp_request(pClient, "initialize", tParams.pData,
        NULL, XRT_DEADLINE_NEVER, false, pError);
    if ( !tRoot ) goto cleanup;
    tResult = xwork__json_get(tRoot, "result");
    sProtocol = xwork__json_text(tResult, "protocolVersion");
    tCapabilities = xwork__json_get(tResult, "capabilities");
    tToolsCapability = xwork__json_get(tCapabilities, "tools");
    if ( !tResult || xrtValueType(tResult) != XVALUE_OBJECT || !sProtocol || !sProtocol[0] ||
         !tCapabilities || xrtValueType(tCapabilities) != XVALUE_OBJECT ||
         !tToolsCapability || xrtValueType(tToolsCapability) != XVALUE_OBJECT ) {
        xwork__set_error(pError, XWORK_ERROR_TOOL,
            "MCP initialize response lacks protocolVersion or tools capability");
        goto cleanup;
    }
    if ( !xwork__replace(&pClient->sNegotiatedProtocolVersion, sProtocol) ) {
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy negotiated MCP version");
        goto cleanup;
    }
    pClient->bServerSupportsToolListChanges = xwork__json_bool(
        tToolsCapability, "listChanged", false, &bValid);
    if ( !bValid ) {
        xwork__set_error(pError, XWORK_ERROR_TOOL, "MCP tools.listChanged must be boolean");
        goto cleanup;
    }
    if ( !xwork__mcp_send_notification(pClient, "notifications/initialized", "{}") ) {
        xwork__set_error(pError, XWORK_ERROR_IO, "failed to send MCP initialized notification");
        goto cleanup;
    }
    pClient->bConnected = true;
    bOk = true;

cleanup:
    if ( tRoot ) xrtValueRelease(tRoot);
    xwork__buf_unit(&tParams);
    if ( !bOk ) xwork__mcp_process_unit(pClient);
    xwork__mcp_unlock(pClient);
    return bOk;
}

bool xworkMcpClientRefreshTools(
    xwork_mcp_client* pClient,
    xwork_agent* pAgent,
    xwork_error* pError
)
{
    xwork_mcp_tool_proxy* pNewTools = NULL;
    size_t iNewCount = 0u;
    size_t iNewCap = 0u;
    size_t i;
    size_t iRemoved = 0u;
    bool bOk = false;
    xworkErrorInit(pError);
    if ( !pClient || !pAgent || !pClient->bConnected ) {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
            "connected MCP client and agent are required");
        return false;
    }
    if ( !xwork__mcp_try_lock(pClient) ) {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "MCP client already has an active request");
        return false;
    }
    if ( pAgent->bRunning ) {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
            "MCP tools cannot refresh while the agent is running");
        goto cleanup;
    }
    if ( !xwork__mcp_discover_tools(
            pClient, &pNewTools, &iNewCount, &iNewCap, pError) ) goto cleanup;
    for ( i = 0u; i < iNewCount; ++i ) {
        size_t j;
        for ( j = 0u; j < pAgent->iToolCount; ++j ) {
            xwork_tool_entry* pExisting = &pAgent->pTools[j];
            if ( strcmp(pExisting->sName, pNewTools[i].sWireName) == 0 &&
                 strcmp(pExisting->sSource, pClient->sToolSource) != 0 ) {
                xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                    "MCP tool wire name collides with another registry source");
                goto cleanup;
            }
        }
    }
    if ( !xworkAgentUnregisterToolsBySource(
            pAgent, pClient->sToolSource, &iRemoved, pError) ) goto cleanup;
    (void)iRemoved;
    xwork__mcp_tools_unit(pClient->pTools, pClient->iToolCount);
    pClient->pTools = pNewTools;
    pClient->iToolCount = iNewCount;
    pClient->iToolCap = iNewCap;
    pNewTools = NULL;
    iNewCount = 0u;
    for ( i = 0u; i < pClient->iToolCount; ++i ) {
        xwork_tool_definition tDefinition;
        memset(&tDefinition, 0, sizeof(tDefinition));
        tDefinition.sName = pClient->pTools[i].sWireName;
        tDefinition.sDescription = pClient->pTools[i].sDescription;
        tDefinition.sParametersJson = pClient->pTools[i].sParametersJson;
        tDefinition.bStrict = false;
        tDefinition.eEffect = pClient->pTools[i].eEffect;
        tDefinition.OnExecute = xwork__mcp_tool_execute;
        tDefinition.pUserData = &pClient->pTools[i];
        tDefinition.sSource = pClient->sToolSource;
        if ( !xworkAgentRegisterTool(pAgent, &tDefinition, pError) ) {
            size_t iRollback = 0u;
            (void)xworkAgentUnregisterToolsBySource(
                pAgent, pClient->sToolSource, &iRollback, NULL);
            goto cleanup;
        }
    }
    bOk = true;

cleanup:
    xwork__mcp_tools_unit(pNewTools, iNewCount);
    xwork__mcp_unlock(pClient);
    return bOk;
}

xwork_result xworkMcpClientCallTool(
    xwork_mcp_client* pClient,
    const char* sRemoteToolName,
    const char* sArgumentsJson,
    xcancel* pCancel,
    uint64_t uDeadline,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xvalue* tArguments = NULL;
    xvalue* tRoot = NULL;
    xvalue* tResult;
    xvalue* tContent;
    xvalue* tStructured;
    xwork_buf tParams = {0};
    xwork_buf tRendered = {0};
    bool bIsError = false;
    bool bValid = true;
    uint32_t i;
    xwork_result eResult = XWORK_RESULT_ERROR;
    char* sCompactArguments = NULL;
    size_t iCompactArguments = 0u;
    xworkErrorInit(pError);
    if ( !pClient || !pClient->bConnected || !sRemoteToolName || !sRemoteToolName[0] ||
         !sArgumentsJson || !pOutput ) {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "invalid MCP tool call");
        return XWORK_RESULT_ERROR;
    }
    tArguments = xwork__json_parse_object(sArgumentsJson);
    if ( !tArguments ) {
        return xworkToolOutputSet(pOutput, false,
            "invalid MCP arguments: expected a JSON object")
            ? XWORK_RESULT_OK : XWORK_RESULT_ERROR;
    }
    if ( !xwork__mcp_try_lock(pClient) ) {
        xrtValueRelease(tArguments);
        return xworkToolOutputSet(pOutput, false, "MCP client is busy")
            ? XWORK_RESULT_OK : XWORK_RESULT_ERROR;
    }
    sCompactArguments = xrtJsonStringify(tArguments, NULL, &iCompactArguments);
    if ( !sCompactArguments ) {
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
            "failed to normalize MCP tool arguments");
        goto cleanup;
    }
    if ( !xwork__buf_append_cstr(&tParams, "{\"name\":") ||
         !xwork__json_string(&tParams, sRemoteToolName) ||
         !xwork__buf_append_cstr(&tParams, ",\"arguments\":") ||
         !xwork__buf_append(&tParams, sCompactArguments, iCompactArguments) ||
         !xwork__buf_append_char(&tParams, '}') ) {
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build MCP tools/call request");
        goto cleanup;
    }
    tRoot = xwork__mcp_request(pClient, "tools/call", tParams.pData,
        pCancel, uDeadline, true, pError);
    if ( !tRoot ) {
        if ( pError->eCode == XWORK_ERROR_CANCELLED ) { eResult = XWORK_RESULT_CANCELLED; goto cleanup; }
        if ( pError->eCode == XWORK_ERROR_TIMEOUT ) { eResult = XWORK_RESULT_TIMEOUT; goto cleanup; }
        if ( !xworkToolOutputSet(pOutput, false,
                pError->sMessage[0] ? pError->sMessage : "MCP tool request failed") ) {
            xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to store MCP tool error");
            goto cleanup;
        }
        xworkErrorInit(pError);
        eResult = XWORK_RESULT_OK;
        goto cleanup;
    }
    tResult = xwork__json_get(tRoot, "result");
    tContent = xwork__json_get(tResult, "content");
    tStructured = xwork__json_get(tResult, "structuredContent");
    if ( !tResult || xrtValueType(tResult) != XVALUE_OBJECT || !tContent ||
         xrtValueType(tContent) != XVALUE_ARRAY ) {
        if ( !xworkToolOutputSet(pOutput, false,
                "MCP tools/call result has no content array") ) goto cleanup;
        eResult = XWORK_RESULT_OK;
        goto cleanup;
    }
    bIsError = xwork__json_bool(tResult, "isError", false, &bValid);
    if ( !bValid ) {
        if ( !xworkToolOutputSet(pOutput, false, "MCP tools/call isError must be boolean") ) goto cleanup;
        eResult = XWORK_RESULT_OK;
        goto cleanup;
    }
    for ( i = 0u; i < xrtValueCount(tContent); ++i ) {
        xvalue* tBlock = xrtValueArrayGet(tContent, i);
        const char* sType = xwork__json_text(tBlock, "type");
        const char* sText = xwork__json_text(tBlock, "text");
        if ( i && !xwork__buf_append_char(&tRendered, '\n') ) goto oom;
        if ( tBlock && xrtValueType(tBlock) == XVALUE_OBJECT &&
             sType && strcmp(sType, "text") == 0 && sText ) {
            if ( !xwork__buf_append_cstr(&tRendered, sText) ) goto oom;
        } else {
            size_t iJson = 0u;
            char* sJson = tBlock ? xrtJsonStringify(tBlock, NULL, &iJson) : NULL;
            if ( !sJson || !xwork__buf_append_cstr(&tRendered, "[MCP content] ") ||
                 !xwork__buf_append(&tRendered, sJson, iJson) ) {
                if ( sJson ) xrtFree(sJson);
                goto oom;
            }
            xrtFree(sJson);
        }
    }
    if ( tStructured ) {
        size_t iJson = 0u;
        char* sJson = xrtJsonStringify(tStructured, NULL, &iJson);
        if ( !sJson ||
             (tRendered.iLen && !xwork__buf_append_cstr(&tRendered, "\n")) ||
             !xwork__buf_append_cstr(&tRendered, "structured_content: ") ||
             !xwork__buf_append(&tRendered, sJson, iJson) ) {
            if ( sJson ) xrtFree(sJson);
            goto oom;
        }
        xrtFree(sJson);
    }
    if ( !tRendered.iLen && !xwork__buf_append_cstr(&tRendered, "MCP tool returned no content") ) goto oom;
    if ( !xworkToolOutputSet(pOutput, !bIsError, tRendered.pData) ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;

oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to render MCP tool result");

cleanup:
    if ( sCompactArguments ) xrtFree(sCompactArguments);
    if ( tRoot ) xrtValueRelease(tRoot);
    if ( tArguments ) xrtValueRelease(tArguments);
    xwork__buf_unit(&tParams);
    xwork__buf_unit(&tRendered);
    xwork__mcp_unlock(pClient);
    return eResult;
}

bool xworkMcpClientGetInfo(const xwork_mcp_client* pClient, xwork_mcp_info* pInfo)
{
    if ( !pClient || !pInfo ) return false;
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->sServerName = pClient->sServerName;
    pInfo->sProtocolVersion = pClient->sNegotiatedProtocolVersion
        ? pClient->sNegotiatedProtocolVersion : pClient->sRequestedProtocolVersion;
    pInfo->sToolSource = pClient->sToolSource;
    pInfo->iToolCount = pClient->iToolCount;
    pInfo->uRequestsCompleted = pClient->uRequestsCompleted;
    pInfo->bConnected = pClient->bConnected;
    pInfo->bServerSupportsToolListChanges = pClient->bServerSupportsToolListChanges;
    return true;
}

void xworkMcpClientDestroy(xwork_mcp_client* pClient)
{
    size_t i;
    if ( !pClient ) return;
    xwork__mcp_process_unit(pClient);
    xwork__mcp_tools_unit(pClient->pTools, pClient->iToolCount);
    for ( i = 0u; i < pClient->iArgumentCount; ++i ) free(pClient->psArguments[i]);
    free(pClient->psArguments);
    free(pClient->sServerName);
    free(pClient->sProgram);
    free(pClient->sWorkingDirectory);
    free(pClient->sRequestedProtocolVersion);
    free(pClient->sNegotiatedProtocolVersion);
    free(pClient->sToolSource);
    xwork__buf_unit(&pClient->tReadBuffer);
    free(pClient);
}
