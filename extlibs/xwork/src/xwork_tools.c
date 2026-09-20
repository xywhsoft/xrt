static bool xwork__path_size(const char* sPath, uint64_t* pSize)
{
    xfileinfo tInfo;
    if ( pSize ) *pSize = 0u;
    if ( !sPath || !xrtPathStat(sPath, true, &tInfo) ) return false;
    if ( pSize ) *pSize = tInfo.Size;
    return true;
}

static bool xwork__buf_append_escaped_bytes(xwork_buf* pBuf, const unsigned char* pData, size_t iSize)
{
    size_t i;
    for ( i = 0u; i < iSize; ++i ) {
        unsigned char c = pData[i];
        if ( c == '\n' || c == '\r' || c == '\t' || (c >= 0x20u && c <= 0x7Eu) ) {
            if ( !xwork__buf_append_char(pBuf, (char)c) ) return false;
        } else if ( !xwork__buf_appendf(pBuf, "\\x%02X", (unsigned int)c) ) {
            return false;
        }
    }
    return true;
}

/* Process pipes are byte streams. Tool results, however, are JSON text and
 * therefore must be valid UTF-8 before they reach the model provider. */
static bool xwork__buf_append_process_text(xwork_buf* pBuf, const void* pData, size_t iSize)
{
    const unsigned char* pBytes = (const unsigned char*)pData;
    if ( iSize == 0u ) return true;
    if ( !pData ) return false;
    if ( memchr(pData, 0, iSize) == NULL &&
         xrtUtf8Valid((xstrview){ (const char*)pData, iSize }, NULL) ) {
        return xwork__buf_append(pBuf, pData, iSize);
    }
#if defined(_WIN32)
    if ( memchr(pData, 0, iSize) == NULL ) {
        int iWide = MultiByteToWideChar(GetOEMCP(), 0, (const char*)pData, (int)iSize, NULL, 0);
        wchar_t* pWide = iWide > 0 ? (wchar_t*)malloc(((size_t)iWide + 1u) * sizeof(wchar_t)) : NULL;
        int iUtf8 = pWide ? MultiByteToWideChar(GetOEMCP(), 0, (const char*)pData,
            (int)iSize, pWide, iWide) : 0;
        char* sConverted = NULL;
        if ( iUtf8 > 0 ) {
            int iBytes = WideCharToMultiByte(CP_UTF8, 0, pWide, iWide, NULL, 0, NULL, NULL);
            sConverted = iBytes > 0 ? (char*)malloc((size_t)iBytes + 1u) : NULL;
            if ( sConverted && WideCharToMultiByte(CP_UTF8, 0, pWide, iWide,
                    sConverted, iBytes, NULL, NULL) > 0 ) sConverted[iBytes] = '\0';
        }
        free(pWide);
        if ( sConverted && sConverted[0] &&
             xrtUtf8Valid((xstrview){ sConverted, strlen(sConverted) }, NULL) ) {
            bool bOk = xwork__buf_append_cstr(pBuf, sConverted);
            free(sConverted);
            return bOk;
        }
        free(sConverted);
    }
#endif
    return xwork__buf_append_escaped_bytes(pBuf, pBytes, iSize);
}




static bool xwork__looks_binary(const unsigned char* pData, size_t iSize)
{
    size_t i;
    size_t iCheck = iSize < 8192u ? iSize : 8192u;
    for ( i = 0u; i < iCheck; ++i ) if ( pData[i] == 0u ) return true;
    return false;
}

/* Image passthrough: magic sniff decides; the extension is not trusted. */
static const char* xwork__image_mime(const unsigned char* pData, size_t iSize)
{
    if ( iSize >= 3u && pData[0] == 0xFF && pData[1] == 0xD8 && pData[2] == 0xFF ) {
        return "image/jpeg";
    }
    if ( iSize >= 8u && pData[0] == 0x89 && pData[1] == 'P' && pData[2] == 'N' && pData[3] == 'G' && pData[4] == 0x0D && pData[5] == 0x0A && pData[6] == 0x1A && pData[7] == 0x0A ) {
        return "image/png";
    }
    if ( iSize >= 6u && (memcmp(pData, "GIF87a", 6u) == 0 ||
                         memcmp(pData, "GIF89a", 6u) == 0) ) {
        return "image/gif";
    }
    if ( iSize >= 12u && memcmp(pData, "RIFF", 4u) == 0 &&
         memcmp(pData + 8u, "WEBP", 4u) == 0 ) {
        return "image/webp";
    }
    if ( iSize >= 2u && pData[0] == 'B' && pData[1] == 'M' ) {
        return "image/bmp";
    }
    return NULL;
}



xwork_result xwork__tool_fail(xwork_tool_output* pOutput, const char* sMessage)
{
    if ( !xworkToolOutputSet(pOutput, false, sMessage) ) return XWORK_RESULT_ERROR;
    return XWORK_RESULT_OK;
}

static xwork_result xwork__tool_read(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xvalue* tArgs = NULL;
    const char* sPath;
    char* sResolved = NULL;
    unsigned char* pData = NULL;
    size_t iSize = 0u;
    uint64_t uStartLine;
    uint64_t uMaxLines;
    bool bValid;
    uint64_t uLine = 1u;
    uint64_t uEmitted = 0u;
    size_t i = 0u;
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    (void)pContext;
    tArgs = xwork__json_parse_object(sArgumentsJson);
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPath = xwork__json_text(tArgs, "path");
    uStartLine = xwork__json_u64(tArgs, "start_line", 1u, &bValid);
    if ( !bValid || uStartLine == 0u ) { eResult = xwork__tool_fail(pOutput, "invalid start_line"); goto cleanup; }
    uMaxLines = xwork__json_u64(tArgs, "max_lines", 400u, &bValid);
    if ( !bValid || uMaxLines == 0u || uMaxLines > 10000u ) { eResult = xwork__tool_fail(pOutput, "max_lines must be between 1 and 10000"); goto cleanup; }
    if ( !sPath || !sPath[0] ) { eResult = xwork__tool_fail(pOutput, "path is required"); goto cleanup; }
    sResolved = xwork__resolve_path(pAgent, sPath, pError);
    if ( !sResolved ) { eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage : "path denied"); goto cleanup; }
    /* 目录回退：read 的意图是「给我这个路径的内容」，目录的内容即条目列表。
     * 显式告知这是目录（非文件内容），防模型把列表当正文处理。 */
    if ( xrtDirExists((str)sResolved) ) {
        if ( !xwork__buf_appendf(&tOutput,
                "dir: %s — this path is a directory, NOT a file; what follows is its entry listing (use ls to list directories directly):\n",
                sPath) ||
             !xwork__list_directory(sResolved, false, false, &tOutput) ||
             !xworkToolOutputSet(pOutput, true, tOutput.pData ? tOutput.pData : "") ) goto oom;
        eResult = XWORK_RESULT_OK;
        goto cleanup;
    }
    if ( !xrtFileExists((str)sResolved) ) { eResult = xwork__tool_fail(pOutput, "file does not exist"); goto cleanup; }
    {
        uint64_t uSize = 0u;
        if ( !xwork__path_size(sResolved, &uSize) || uSize > 64u * 1024u * 1024u ) {
            eResult = xwork__tool_fail(pOutput, "file is larger than the 64 MiB read limit"); goto cleanup;
        }
    }
    pData = (unsigned char*)xrtFileReadAll(sResolved, &iSize);
    if ( !pData && iSize ) { eResult = xwork__tool_fail(pOutput, "failed to read file"); goto cleanup; }
    {
        const char* sMime = xwork__image_mime(pData, iSize);
        if ( sMime ) {
            if ( !xwork__buf_appendf(&tOutput, "image: %s\nsize: %zu bytes\nmime: %s\nattached for viewing",
                    sPath, iSize, sMime) ||
                 !xworkToolOutputSet(pOutput, true, tOutput.pData) ||
                 !xworkToolOutputSetImage(pOutput, pData, iSize, sMime) ) goto oom;
            eResult = XWORK_RESULT_OK;
            goto cleanup;
        }
    }
    if ( xwork__looks_binary(pData, iSize) ) { eResult = xwork__tool_fail(pOutput, "file appears to be binary"); goto cleanup; }
    if ( !xwork__buf_appendf(&tOutput, "file: %s (%zu bytes)\n", sPath, iSize) ) goto oom;
    while ( i < iSize && uEmitted < uMaxLines ) {
        size_t iStart = i;
        size_t iLen;
        while ( i < iSize && pData[i] != '\n' ) ++i;
        iLen = i - iStart;
        if ( iLen && pData[iStart + iLen - 1u] == '\r' ) --iLen;
        if ( uLine >= uStartLine ) {
            if ( !xwork__buf_appendf(&tOutput, "%6llu | ", (unsigned long long)uLine) ||
                 !xwork__buf_append(&tOutput, pData + iStart, iLen) ||
                 !xwork__buf_append_char(&tOutput, '\n') ) goto oom;
            ++uEmitted;
        }
        if ( i < iSize ) ++i;
        ++uLine;
    }
    if ( uEmitted == uMaxLines && i < iSize && !xwork__buf_appendf(&tOutput, "[truncated: more lines remain; continue with start_line=%llu]\n", (unsigned long long)uLine) ) goto oom;
    if ( uStartLine >= uLine && i >= iSize && !xwork__buf_append_cstr(&tOutput, "[start_line is beyond end of file]\n") ) goto oom;
    if ( i >= iSize && !xwork__buf_appendf(&tOutput, "[complete: end of file at line %llu]\n", (unsigned long long)(uLine - 1u)) ) goto oom;
    if ( !xworkToolOutputSet(pOutput, true, tOutput.pData ? tOutput.pData : "") ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build read_file output");
cleanup:
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolved);
    if ( pData ) xrtFree(pData);   /* xrt returns a freeable buffer even when empty */
    xwork__buf_unit(&tOutput);
    return eResult;
}







static bool xwork__write_bytes(const char* sPath, const char* sContent, bool bAppend)
{
    size_t iLen = strlen(sContent);
    if ( iLen == 0u ) {
        FILE* pFile = fopen(sPath, bAppend ? "ab" : "wb");
        if ( !pFile ) return false;
        fclose(pFile);
        return true;
    }
    return bAppend
        ? xrtFileAppend(sPath, (xbytesview){ (const uint8*)sContent, iLen })
        : xrtFileWriteAtomic(sPath, (xbytesview){ (const uint8*)sContent, iLen });
}

static bool xwork__write_atomic_bytes(const char* sPath, const char* sContent, size_t iLen)
{
    if ( !sPath || (!sContent && iLen) ) return false;
    return xrtFileWriteAtomic(sPath,
        (xbytesview){ (const uint8*)sContent, iLen });
}

/* ------------------------------------------------------------------ */
/* EOL discipline: the model works in LF space; storage converts per   */
/* the agent policy. AUTO keeps each file's dominant ending.           */
/* ------------------------------------------------------------------ */

static bool xwork__file_prefers_crlf(const char* sData, size_t iSize)
{
    size_t iCrlf = 0u;
    size_t iLf = 0u;
    size_t i;
    for ( i = 0u; i < iSize; ++i ) {
        if ( sData[i] == '\n' ) {
            if ( i > 0u && sData[i - 1u] == '\r' ) ++iCrlf;
            else ++iLf;
        }
    }
    return iCrlf > iLf;
}

/* Strip CR from CRLF pairs; returns a malloc'd LF-normalized copy. */
static char* xwork__normalize_to_lf(const char* sData, size_t iSize, size_t* piOut)
{
    char* sOut = (char*)malloc(iSize + 1u);
    size_t i;
    size_t n = 0u;
    if ( !sOut ) return NULL;
    for ( i = 0u; i < iSize; ++i ) {
        if ( sData[i] == '\r' && i + 1u < iSize && sData[i + 1u] == '\n' ) continue;
        sOut[n++] = sData[i];
    }
    sOut[n] = '\0';
    if ( piOut ) *piOut = n;
    return sOut;
}

/* Convert LF to the storage ending; returns a malloc'd copy. */
static char* xwork__apply_storage_eol(const char* sLf, size_t iLen,
    const xwork_agent* pAgent, bool bExistingPrefersCrlf, size_t* piOut)
{
    bool bCrlf;
    char* sOut;
    size_t i;
    size_t n = 0u;
    if ( !pAgent || pAgent->eEolPolicy == XWORK_EOL_PRESERVE ) {
        bCrlf = false;   /* PRESERVE callers pass already-raw text */
    } else if ( pAgent->eEolPolicy == XWORK_EOL_FORCE_LF ) {
        bCrlf = false;
    } else if ( pAgent->eEolPolicy == XWORK_EOL_FORCE_CRLF ) {
        bCrlf = true;
    } else {
        bCrlf = bExistingPrefersCrlf;
    }
    if ( !bCrlf ) {
        sOut = (char*)malloc(iLen + 1u);
        if ( !sOut ) return NULL;
        memcpy(sOut, sLf, iLen);
        sOut[iLen] = '\0';
        if ( piOut ) *piOut = iLen;
        return sOut;
    }
    {
        size_t iLf = 0u;
        for ( i = 0u; i < iLen; ++i ) {
            if ( sLf[i] == '\n' ) ++iLf;
        }
        sOut = (char*)malloc(iLen + iLf + 1u);
        if ( !sOut ) return NULL;
        for ( i = 0u; i < iLen; ++i ) {
            if ( sLf[i] == '\n' ) sOut[n++] = '\r';
            sOut[n++] = sLf[i];
        }
        sOut[n] = '\0';
        if ( piOut ) *piOut = n;
        return sOut;
    }
}

/* Find occurrences of a needle (memmem-free, Windows portable). */
static const char* xwork__find_bytes(const char* sHay, size_t iHay,
    const char* sNeedle, size_t iNeedle, size_t iFrom)
{
    if ( iNeedle == 0u || iHay < iNeedle ) return NULL;
    for ( ; iFrom + iNeedle <= iHay; ++iFrom ) {
        if ( sHay[iFrom] == sNeedle[0] &&
             memcmp(sHay + iFrom, sNeedle, iNeedle) == 0 ) {
            return sHay + iFrom;
        }
    }
    return NULL;
}


static xwork_result xwork__tool_write(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    const char* sPath;
    const char* sContent;
    const char* sMode;
    char* sResolved = NULL;
    char* sStored = NULL;
    char* sExisting = NULL;
    bool bAppend = false;
    bool bCreate = false;
    bool bCreatedDirs = false;
    size_t iStoredLen = 0u;
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPath = xwork__json_text(tArgs, "path");
    sContent = xwork__json_text(tArgs, "content");
    sMode = xwork__json_text(tArgs, "mode");
    if ( !sMode || !sMode[0] ) sMode = "overwrite";
    if ( !sPath || !sPath[0] || !sContent ) { eResult = xwork__tool_fail(pOutput, "path and content are required"); goto cleanup; }
    if ( strcmp(sMode, "append") == 0 ) bAppend = true;
    else if ( strcmp(sMode, "create") == 0 ) bCreate = true;
    else if ( strcmp(sMode, "overwrite") != 0 ) { eResult = xwork__tool_fail(pOutput, "mode must be overwrite, append, or create"); goto cleanup; }
    sResolved = xwork__resolve_path(pAgent, sPath, pError);
    if ( !sResolved ) { eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage : "path denied"); goto cleanup; }
    if ( bCreate && xrtPathExists((str)sResolved) ) { eResult = xwork__tool_fail(pOutput, "create conflict: target already exists"); goto cleanup; }
    /* Parents are always created; the success message reports it so the
     * model notices when it invented structure. */
    bCreatedDirs = !xwork__parent_exists(sResolved);
    if ( !xwork__ensure_parent(sResolved) ) { eResult = xwork__tool_fail(pOutput, "failed to create parent directories"); goto cleanup; }
    /* EOL discipline: the model's text is normalized to LF first, then
     * storage converts per policy (AUTO keeps the file's dominant ending). */
    {
        size_t iExisting = 0u;
        char* sLf = NULL;
        if ( pAgent->eEolPolicy == XWORK_EOL_AUTO && (bAppend || !bCreate) ) {
            sExisting = (char*)xrtFileReadAll(sResolved, &iExisting);
        }
        if ( pAgent->eEolPolicy == XWORK_EOL_PRESERVE ) {
            sStored = xwork__strdup(sContent);
            if ( sStored ) { iStoredLen = strlen(sStored); }
            if ( !sStored ) goto oom;
        } else {
            sLf = xwork__normalize_to_lf(sContent, strlen(sContent), NULL);
            if ( !sLf ) goto oom;
            sStored = xwork__apply_storage_eol(sLf, strlen(sLf), pAgent,
                sExisting ? xwork__file_prefers_crlf(sExisting, iExisting) : false, &iStoredLen);
            free(sLf);
            if ( !sStored ) goto oom;
        }
    }
    if ( !(bAppend ? xwork__write_bytes(sResolved, sStored, true)
                  : xwork__write_atomic_bytes(sResolved, sStored, iStoredLen)) ) {
        eResult = xwork__tool_fail(pOutput, "failed to write file");
        goto cleanup;
    }
    if ( !xwork__buf_appendf(&tOutput, "wrote %zu bytes to %s (mode=%s)%s",
            iStoredLen, sPath, sMode,
            bCreatedDirs ? " (created parent directories)" : "") ||
         !xworkToolOutputSet(pOutput, true, tOutput.pData) ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build write_file output");
cleanup:
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolved);
    free(sStored);
    if ( sExisting ) xrtFree(sExisting);
    xwork__buf_unit(&tOutput);
    return eResult;
}

/* pi-style batch edit: every old_text is matched against the original
 * file (not against earlier edits' output); one atomic write applies the
 * whole batch. 0 or ambiguous matches return candidate context lines for
 * self-correction instead of a bare error. */
#define XWORK_EDIT_MAX_EDITS 64u

typedef struct xwork_edit_span {
    size_t iStart;
    size_t iLen;
    size_t iNew;
    size_t iNewLen;
} xwork_edit_span;

static int xwork__span_cmp(const void* pA, const void* pB)
{
    const xwork_edit_span* pSA = (const xwork_edit_span*)pA;
    const xwork_edit_span* pSB = (const xwork_edit_span*)pB;
    if ( pSA->iStart < pSB->iStart ) return -1;
    if ( pSA->iStart > pSB->iStart ) return 1;
    return 0;
}

/* Append numbered candidate lines around a byte offset (self-correction). */
static bool xwork__append_edit_candidates(xwork_buf* pOut, const char* sLf,
    size_t iLen, size_t iFrom, size_t iCount, const char* sNeedle)
{
    size_t iLine = 1u;
    size_t i;
    size_t iLastStart = 0u;
    size_t iHits = 0u;
    size_t iShown = 0u;
    /* count lines and find candidate regions: lines containing sNeedle. */
    if ( !xwork__buf_appendf(pOut, "candidates:\n") ) return false;
    for ( i = 0u; i <= iLen && iShown < iCount; ++i ) {
        bool bEnd = i == iLen;
        if ( !bEnd && sLf[i] != '\n' ) continue;
        if ( sNeedle ) {
            size_t n = i - iLastStart + (bEnd ? 0u : 1u);
            const char* pLine = sLf + iLastStart;
            size_t iNeedle = strlen(sNeedle);
            bool bHit = false;
            size_t k;
            for ( k = 0u; k + iNeedle <= n; ++k ) {
                if ( pLine[k] == sNeedle[0] && memcmp(pLine + k, sNeedle, iNeedle) == 0 ) {
                    bHit = true;
                    break;
                }
            }
            if ( bHit ) { ++iHits; }
            if ( bHit && iHits >= iFrom ) {
                if ( !xwork__buf_appendf(pOut, "%6zu | ", iLine) ) return false;
                if ( !xwork__buf_append(pOut, pLine, n && pLine[n - 1u] == '\n' ? n - 1u : n) ||
                     !xwork__buf_append_char(pOut, '\n') ) return false;
                ++iShown;
            }
        } else if ( iLine <= iCount ) {
            size_t n = i - iLastStart + (bEnd ? 0u : 1u);
            const char* pLine = sLf + iLastStart;
            if ( !xwork__buf_appendf(pOut, "%6zu | ", iLine) ) return false;
            if ( !xwork__buf_append(pOut, pLine, n && pLine[n - 1u] == '\n' ? n - 1u : n) ||
                 !xwork__buf_append_char(pOut, '\n') ) return false;
            ++iShown;
        }
        ++iLine;
        iLastStart = i + 1u;
    }
    if ( iShown == 0u ) {
        if ( !xwork__buf_appendf(pOut, "(no candidate lines)\n") ) return false;
    }
    return true;
}

static xwork_result xwork__tool_edit(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    const char* sPath;
    char* sResolved = NULL;
    char* sRaw = NULL;
    bool bRawFromXrt = false;
    char* sLf = NULL;
    char* sStored = NULL;
    char** psNew = NULL;
    bool* pbAll = NULL;
    xwork_edit_span* pSpans = NULL;
    size_t iSpanCount = 0u;
    size_t iSpanCap = 0u;
    size_t iRaw = 0u;
    size_t iLf = 0u;
    size_t iEditCount = 0u;
    size_t i;
    xvalue* tEdits;
    bool bPrefersCrlf;
    xwork_buf tNext = {0};
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPath = xwork__json_text(tArgs, "path");
    if ( !sPath || !sPath[0] ) { eResult = xwork__tool_fail(pOutput, "path is required"); goto cleanup; }
    tEdits = xwork__json_get(tArgs, "edits");
    if ( !tEdits || xrtValueType(tEdits) != XVALUE_ARRAY ||
         (iEditCount = xrtValueCount(tEdits)) == 0u || iEditCount > XWORK_EDIT_MAX_EDITS ) {
        eResult = xwork__tool_fail(pOutput, "edits must be an array of 1-64 objects");
        goto cleanup;
    }
    psNew = (char**)calloc(iEditCount, sizeof(char*));
    pbAll = (bool*)calloc(iEditCount, sizeof(bool));
    if ( !psNew || !pbAll ) goto oom;
    for ( i = 0u; i < iEditCount; ++i ) {
        xvalue* tEdit = xrtValueArrayGet(tEdits, i);
        const char* sNew = xwork__json_text(tEdit, "new_text");
        bool bValid;
        if ( !tEdit || !xwork__json_text(tEdit, "old_text") ||
             !xwork__json_text(tEdit, "old_text")[0] || !sNew ) {
            eResult = xwork__tool_fail(pOutput, "each edit needs non-empty old_text and new_text");
            goto cleanup;
        }
        pbAll[i] = xwork__json_bool(tEdit, "replace_all", false, &bValid);
        if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "replace_all must be boolean"); goto cleanup; }
        psNew[i] = xwork__strdup(sNew);
        if ( !psNew[i] ) goto oom;
    }
    sResolved = xwork__resolve_path(pAgent, sPath, pError);
    if ( !sResolved ) { eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage : "path denied"); goto cleanup; }
    if ( !xrtFileExists((str)sResolved) ) { eResult = xwork__tool_fail(pOutput, "file does not exist"); goto cleanup; }
    sRaw = (char*)xrtFileReadAll(sResolved, &iRaw);
    if ( !sRaw && iRaw ) { eResult = xwork__tool_fail(pOutput, "failed to read file"); goto cleanup; }
    bRawFromXrt = sRaw != NULL;   /* xrt returns a freeable buffer even when empty */
    if ( !sRaw ) sRaw = xwork__strdup("");
    if ( memchr(sRaw, 0, iRaw) != NULL ) {
        eResult = xwork__tool_fail(pOutput, "binary file; edit supports UTF-8 text only");
        goto cleanup;
    }
    bPrefersCrlf = xwork__file_prefers_crlf(sRaw, iRaw);
    if ( pAgent->eEolPolicy == XWORK_EOL_PRESERVE ) {
        sLf = xwork__strdup(sRaw);
        iLf = iRaw;
    } else {
        sLf = xwork__normalize_to_lf(sRaw, iRaw, &iLf);
    }
    if ( !sLf ) goto oom;

    /* Collect spans against the ORIGINAL LF text. */
    iSpanCap = iEditCount * 2u;
    pSpans = (xwork_edit_span*)malloc(iSpanCap * sizeof(*pSpans));
    if ( !pSpans ) goto oom;
    for ( i = 0u; i < iEditCount; ++i ) {
        xvalue* tEdit = xrtValueArrayGet(tEdits, i);
        const char* sOld = xwork__json_text(tEdit, "old_text");
        size_t iOld = strlen(sOld);
        size_t iFrom = 0u;
        size_t iMatches = 0u;
        const char* pMatch;
        while ( (pMatch = xwork__find_bytes(sLf, iLf, sOld, iOld, iFrom)) != NULL ) {
            /* replace_all keeps every span; single edits keep only the first
             * (later matches still count toward the ambiguity report). */
            if ( pbAll[i] || iMatches == 0u ) {
                if ( iSpanCount == iSpanCap ) {
                    xwork_edit_span* pNewSpans;
                    iSpanCap *= 2u;
                    pNewSpans = (xwork_edit_span*)realloc(pSpans, iSpanCap * sizeof(*pSpans));
                    if ( !pNewSpans ) goto oom;
                    pSpans = pNewSpans;
                }
                pSpans[iSpanCount].iStart = (size_t)(pMatch - sLf);
                pSpans[iSpanCount].iLen = iOld;
                pSpans[iSpanCount].iNew = i;
                pSpans[iSpanCount].iNewLen = strlen(psNew[i]);
                ++iSpanCount;
            }
            ++iMatches;
            iFrom = (size_t)(pMatch - sLf) + iOld;
        }
        if ( iMatches == 0u ) {
            if ( !xwork__buf_appendf(&tOutput,
                    "edit %zu failed: old_text was not found (0 matches).\n", i) ||
                 !xwork__append_edit_candidates(&tOutput, sLf, iLf, 1u, 15u, NULL) ) goto oom;
            eResult = xwork__tool_fail(pOutput, tOutput.pData ? tOutput.pData : "old_text was not found");
            goto cleanup;
        }
        if ( iMatches > 1u && !pbAll[i] ) {
            if ( !xwork__buf_appendf(&tOutput,
                    "edit %zu failed: old_text occurs %zu times; add context or set replace_all.\n",
                    i, iMatches) ||
                 !xwork__append_edit_candidates(&tOutput, sLf, iLf, 1u, 10u, sOld) ) goto oom;
            eResult = xwork__tool_fail(pOutput, tOutput.pData ? tOutput.pData : "old_text is ambiguous");
            goto cleanup;
        }
    }

    /* Sort, reject overlaps, splice one atomic result. */
    qsort(pSpans, iSpanCount, sizeof(*pSpans), xwork__span_cmp);
    for ( i = 1u; i < iSpanCount; ++i ) {
        if ( pSpans[i].iStart < pSpans[i - 1u].iStart + pSpans[i - 1u].iLen ) {
            eResult = xwork__tool_fail(pOutput, "edits overlap; merge them into one edit");
            goto cleanup;
        }
    }
    {
        size_t iCursor = 0u;
        for ( i = 0u; i < iSpanCount; ++i ) {
            if ( !xwork__buf_append(&tNext, sLf + iCursor, pSpans[i].iStart - iCursor) ||
                 !xwork__buf_append(&tNext, psNew[pSpans[i].iNew], pSpans[i].iNewLen) ) goto oom;
            iCursor = pSpans[i].iStart + pSpans[i].iLen;
        }
        if ( !xwork__buf_append(&tNext, sLf + iCursor, iLf - iCursor) ) goto oom;
    }
    if ( pAgent->eEolPolicy == XWORK_EOL_PRESERVE ) {
        sStored = xwork__strdup(tNext.pData ? tNext.pData : "");
    } else {
        sStored = xwork__apply_storage_eol(tNext.pData ? tNext.pData : "", tNext.iLen,
            pAgent, bPrefersCrlf, NULL);
    }
    if ( !sStored ) goto oom;
    if ( !xwork__write_atomic_bytes(sResolved, sStored, strlen(sStored)) ) {
        eResult = xwork__tool_fail(pOutput, "failed to write edited file");
        goto cleanup;
    }
    if ( !xwork__buf_appendf(&tOutput, "applied %zu edit%s (%zu replacement%s) to %s (%zu -> %zu bytes)",
            iEditCount, iEditCount == 1u ? "" : "s",
            iSpanCount, iSpanCount == 1u ? "" : "s",
            sPath, iRaw, strlen(sStored)) ||
         !xworkToolOutputSet(pOutput, true, tOutput.pData) ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to apply batch edit");
cleanup:
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolved);
    if ( bRawFromXrt ) { if ( sRaw ) xrtFree(sRaw); }
    else { free(sRaw); }
    free(sLf);
    free(sStored);
    if ( psNew ) { for ( i = 0u; i < iEditCount; ++i ) free(psNew[i]); free(psNew); }
    free(pbAll);
    free(pSpans);
    xwork__buf_unit(&tNext);
    xwork__buf_unit(&tOutput);
    return eResult;
}


static bool xwork__process_running(const xprocess* pProcess)
{
    return pProcess && xrtProcessState(pProcess) == XPROCESS_RUNNING;
}

static int32 xwork__process_capture_thread(void* pData)
{
    xwork_process_capture_stream* pStream = (xwork_process_capture_stream*)pData;
    uint8_t pChunk[4096];
    for ( ;; ) {
        int64 iRead = xrtProcessRead(pStream->pOwner->pProcess,
            pStream->eStream, pChunk, sizeof(pChunk));
        if ( iRead <= 0 ) break;
        if ( !xrtMutexLock(pStream->pOwner->pLock) ) break;
        if ( (size_t)iRead >= pStream->iLimit ) {
            size_t iKeep = pStream->iLimit;
            pStream->uBaseOffset += pStream->tData.iLen + (uint64_t)iRead - iKeep;
            pStream->tData.iLen = 0u;
            (void)xwork__buf_append(&pStream->tData,
                pChunk + (size_t)iRead - iKeep, iKeep);
        } else {
            size_t iDrop = pStream->tData.iLen + (size_t)iRead > pStream->iLimit
                ? pStream->tData.iLen + (size_t)iRead - pStream->iLimit : 0u;
            if ( iDrop ) {
                memmove(pStream->tData.pData, pStream->tData.pData + iDrop,
                    pStream->tData.iLen - iDrop);
                pStream->tData.iLen -= iDrop;
                pStream->uBaseOffset += iDrop;
            }
            (void)xwork__buf_append(&pStream->tData, pChunk, (size_t)iRead);
        }
        (void)xrtMutexUnlock(pStream->pOwner->pLock);
    }
    if ( xrtMutexLock(pStream->pOwner->pLock) ) {
        pStream->bDone = true;
        (void)xrtMutexUnlock(pStream->pOwner->pLock);
    }
    return 0;
}

static xwork_process_capture* xwork__process_capture_create(
    xprocess* pProcess,
    size_t iLimit,
    bool bCaptureStderr
)
{
    xwork_process_capture* pCapture = (xwork_process_capture*)calloc(1u, sizeof(*pCapture));
    if ( !pCapture ) return NULL;
    pCapture->pProcess = pProcess;
    pCapture->pLock = xrtMutexCreate();
    pCapture->tStdout.pOwner = pCapture;
    pCapture->tStdout.eStream = XPROCESS_STDOUT;
    pCapture->tStdout.iLimit = iLimit;
    pCapture->tStderr.pOwner = pCapture;
    pCapture->tStderr.eStream = XPROCESS_STDERR;
    pCapture->tStderr.iLimit = iLimit;
    if ( !pCapture->pLock ) goto fail;
    pCapture->tStdout.pThread = xrtThreadCreate(
        xwork__process_capture_thread, &pCapture->tStdout, 0u);
    if ( !pCapture->tStdout.pThread ) goto fail;
    if ( bCaptureStderr ) {
        pCapture->tStderr.pThread = xrtThreadCreate(
            xwork__process_capture_thread, &pCapture->tStderr, 0u);
        if ( !pCapture->tStderr.pThread ) goto fail;
    } else {
        pCapture->tStderr.bDone = true;
    }
    return pCapture;
fail:
    if ( pCapture->tStdout.pThread ) {
        (void)xrtProcessClose(pProcess, XPROCESS_STDOUT);
        (void)xrtThreadWait(pCapture->tStdout.pThread);
        xrtThreadDestroy(pCapture->tStdout.pThread);
    }
    if ( pCapture->pLock ) (void)xrtMutexDestroy(pCapture->pLock);
    free(pCapture);
    return NULL;
}

static void xwork__process_capture_destroy(xwork_process_capture* pCapture)
{
    if ( !pCapture ) return;
    if ( pCapture->tStdout.pThread ) {
        (void)xrtThreadWait(pCapture->tStdout.pThread);
        xrtThreadDestroy(pCapture->tStdout.pThread);
    }
    if ( pCapture->tStderr.pThread ) {
        (void)xrtThreadWait(pCapture->tStderr.pThread);
        xrtThreadDestroy(pCapture->tStderr.pThread);
    }
    xwork__buf_unit(&pCapture->tStdout.tData);
    xwork__buf_unit(&pCapture->tStderr.tData);
    if ( pCapture->pLock ) (void)xrtMutexDestroy(pCapture->pLock);
    free(pCapture);
}

static void* xwork__process_capture_since(
    xwork_process_capture* pCapture,
    bool bStderr,
    uint64_t uOffset,
    size_t iMaxBytes,
    size_t* pSize,
    uint64_t* pBaseOffset,
    uint64_t* pNextOffset
)
{
    xwork_process_capture_stream* pStream;
    uint64_t uAvailableEnd;
    size_t iStart;
    size_t iCopy;
    uint8_t* pCopy = NULL;
    if ( pSize ) *pSize = 0u;
    if ( !pCapture || !xrtMutexLock(pCapture->pLock) ) return NULL;
    pStream = bStderr ? &pCapture->tStderr : &pCapture->tStdout;
    uAvailableEnd = pStream->uBaseOffset + pStream->tData.iLen;
    if ( pBaseOffset ) *pBaseOffset = pStream->uBaseOffset;
    if ( uOffset < pStream->uBaseOffset ) uOffset = pStream->uBaseOffset;
    if ( uOffset > uAvailableEnd ) uOffset = uAvailableEnd;
    iStart = (size_t)(uOffset - pStream->uBaseOffset);
    iCopy = pStream->tData.iLen - iStart;
    if ( iCopy > iMaxBytes ) iCopy = iMaxBytes;
    if ( iCopy ) {
        pCopy = (uint8_t*)malloc(iCopy);
        if ( pCopy ) memcpy(pCopy, pStream->tData.pData + iStart, iCopy);
        else iCopy = 0u;
    }
    if ( pSize ) *pSize = iCopy;
    if ( pNextOffset ) *pNextOffset = uOffset + iCopy;
    (void)xrtMutexUnlock(pCapture->pLock);
    return pCopy;
}

static void xwork__process_entry_close(xwork_process_entry* pEntry)
{
    if ( !pEntry ) return;
    if ( pEntry->eKind == XWORK_TASK_AGENT ) {
        if ( pEntry->pChildCancel && !pEntry->bDone ) {
            (void)xrtCancelRequest(pEntry->pChildCancel);
        }
        if ( pEntry->pThread ) {
            /* The cancel propagates into the delegate's model calls and the
             * loop-top checks; an unbounded join is safe because every wait
             * in the composition honors the token or a deadline. */
            (void)xrtThreadWait(pEntry->pThread);
            xrtThreadDestroy(pEntry->pThread);
        }
        if ( pEntry->pChildCancel ) {
            xrtCancelDestroy(pEntry->pChildCancel);
        }
        if ( pEntry->pStateLock ) {
            (void)xrtMutexLock(pEntry->pStateLock);
            free(pEntry->sResult);
            pEntry->sResult = NULL;
            (void)xrtMutexUnlock(pEntry->pStateLock);
            (void)xrtMutexDestroy(pEntry->pStateLock);
        }
        free(pEntry->sCommand);
        free(pEntry->sNotify);
        memset(pEntry, 0, sizeof(*pEntry));
        return;
    }
    if ( pEntry->pProcess ) {
        if ( xwork__process_running(pEntry->pProcess) ) {
            (void)xrtProcessKillTree(pEntry->pProcess);
            if ( xrtProcessWaitFor(pEntry->pProcess, UINT64_C(3000000)) != XWAIT_OK ) {
                (void)xrtProcessKill(pEntry->pProcess);
                (void)xrtProcessWait(pEntry->pProcess);
            }
        }
        xwork__process_capture_destroy(pEntry->pCapture);
        xrtProcessDestroy(pEntry->pProcess);
    }
    free(pEntry->sCommand);
    free(pEntry->sNotify);
    memset(pEntry, 0, sizeof(*pEntry));
}

bool xwork__task_running(xwork_process_entry* pEntry)
{
    if ( !pEntry ) return false;
    if ( pEntry->eKind == XWORK_TASK_AGENT ) {
        bool bRunning = true;
        if ( pEntry->pStateLock ) {
            (void)xrtMutexLock(pEntry->pStateLock);
            bRunning = !pEntry->bDone;
            (void)xrtMutexUnlock(pEntry->pStateLock);
        }
        return bRunning;
    }
    return xwork__process_running(pEntry->pProcess);
}

static bool xwork__task_entry_running(xwork_process_entry* pEntry)
{
    return xwork__task_running(pEntry);
}

xwork_process_entry* xwork__task_add(xwork_agent* pAgent, xwork_task_kind eKind)
{
    xwork_process_entry* pEntry = xwork__process_add(pAgent);
    if ( pEntry ) pEntry->eKind = eKind;
    return pEntry;
}

/* Wait up to uWaitMs for a task of either kind to finish. */
static bool xwork__wait_task(xwork_agent* pAgent, xwork_process_entry* pEntry, uint64_t uWaitMs)
{
    uint64_t uDeadline = xrtDeadlineAfter(uWaitMs * UINT64_C(1000));
    while ( xwork__task_entry_running(pEntry) ) {
        if ( xrtDeadlineExpired(uDeadline) ) return false;
        if ( xwork__is_cancelled(pAgent) ) return false;
        xrtSleep(5u);
    }
    return true;
}

static void xwork__process_remove(xwork_agent* pAgent, size_t iIndex)
{
    if ( !pAgent || iIndex >= pAgent->iProcessCount ) return;
    xwork__process_entry_close(&pAgent->pProcesses[iIndex]);
    if ( iIndex + 1u < pAgent->iProcessCount ) {
        pAgent->pProcesses[iIndex] = pAgent->pProcesses[pAgent->iProcessCount - 1u];
        memset(&pAgent->pProcesses[pAgent->iProcessCount - 1u], 0, sizeof(*pAgent->pProcesses));
    }
    --pAgent->iProcessCount;
}

void xwork__processes_unit(xwork_agent* pAgent)
{
    if ( !pAgent ) return;
    while ( pAgent->iProcessCount ) xwork__process_remove(pAgent, pAgent->iProcessCount - 1u);
    free(pAgent->pProcesses);
    pAgent->pProcesses = NULL;
    pAgent->iProcessCap = 0u;
}

static xwork_process_entry* xwork__process_find(xwork_agent* pAgent, uint64_t uId, size_t* piIndex)
{
    size_t i;
    if ( piIndex ) *piIndex = (size_t)-1;
    if ( !pAgent || !uId ) return NULL;
    for ( i = 0u; i < pAgent->iProcessCount; ++i ) {
        if ( pAgent->pProcesses[i].uId == uId ) {
            if ( piIndex ) *piIndex = i;
            return &pAgent->pProcesses[i];
        }
    }
    return NULL;
}

xwork_process_entry* xwork__process_add(xwork_agent* pAgent)
{
    xwork_process_entry* pNew;
    size_t i;
    size_t iCap;
    for ( i = pAgent->iProcessCount; i > 0u && pAgent->iProcessCount >= pAgent->uMaxManagedProcesses; --i ) {
        xwork_process_entry* pCandidate = &pAgent->pProcesses[i - 1u];
        /* Only reclaim finished tasks whose completion notice was consumed;
         * an unclaimed notice is still owed to the host/model. */
        if ( !xwork__task_entry_running(pCandidate) && pCandidate->bNoticeTaken ) {
            xwork__process_remove(pAgent, i - 1u);
        }
    }
    if ( pAgent->iProcessCount >= pAgent->uMaxManagedProcesses ) return NULL;
    if ( pAgent->iProcessCount == pAgent->iProcessCap ) {
        iCap = pAgent->iProcessCap ? pAgent->iProcessCap * 2u : 4u;
        if ( iCap > pAgent->uMaxManagedProcesses ) iCap = pAgent->uMaxManagedProcesses;
        pNew = (xwork_process_entry*)realloc(pAgent->pProcesses, iCap * sizeof(*pNew));
        if ( !pNew ) return NULL;
        memset(pNew + pAgent->iProcessCap, 0, (iCap - pAgent->iProcessCap) * sizeof(*pNew));
        pAgent->pProcesses = pNew;
        pAgent->iProcessCap = iCap;
    }
    pNew = &pAgent->pProcesses[pAgent->iProcessCount++];
    memset(pNew, 0, sizeof(*pNew));
    pNew->uId = ++pAgent->uNextProcessId;
    if ( pNew->uId == 0u ) pNew->uId = ++pAgent->uNextProcessId;
    pNew->eKind = XWORK_TASK_PROCESS;
    pNew->uStartedUs = xrtClock();
    return pNew;
}

static bool xwork__append_process_stream(
    xwork_buf* pOutput,
    xwork_process_entry* pEntry,
    bool bStderr,
    size_t iMaxBytes
)
{
    uint64_t* puOffset = bStderr ? &pEntry->uStderrOffset : &pEntry->uStdoutOffset;
    uint64_t uRequested = *puOffset;
    void* pData;
    size_t iSize = 0u;
    uint64_t uBaseOffset = 0u;
    uint64_t uNextOffset = uRequested;
    pData = xwork__process_capture_since(pEntry->pCapture, bStderr,
        uRequested, iMaxBytes, &iSize, &uBaseOffset, &uNextOffset);
    if ( uNextOffset > *puOffset ) *puOffset = uNextOffset;
    if ( uBaseOffset > uRequested &&
         !xwork__buf_appendf(pOutput, "[%s output before offset %llu was dropped by the capture limit]\n",
            bStderr ? "stderr" : "stdout", (unsigned long long)uBaseOffset) ) goto fail;
    if ( iSize ) {
        if ( !xwork__buf_appendf(pOutput, "--- %s ---\n", bStderr ? "stderr" : "stdout") ||
             !xwork__buf_append_process_text(pOutput, pData, iSize) ||
             !xwork__buf_append_char(pOutput, '\n') ) goto fail;
    }
    free(pData);
    return true;
fail:
    free(pData);
    return false;
}

static bool xwork__append_process_status(
    xwork_agent* pAgent,
    xwork_buf* pOutput,
    xwork_process_entry* pEntry,
    uint32_t uWaitMs,
    size_t iMaxBytes
)
{
    bool bRunning;
    xprocessstatus tExit;
    if ( uWaitMs && xwork__task_entry_running(pEntry) ) {
        (void)xwork__wait_task(pAgent, pEntry, uWaitMs);
    }
    bRunning = xwork__task_entry_running(pEntry);
    if ( pEntry->eKind == XWORK_TASK_AGENT ) {
        char* sResult = NULL;
        bool bSuccess = false;
        if ( pEntry->pStateLock ) {
            (void)xrtMutexLock(pEntry->pStateLock);
            sResult = pEntry->sResult ? xwork__strdup(pEntry->sResult) : NULL;
            bSuccess = pEntry->bSuccess;
            (void)xrtMutexUnlock(pEntry->pStateLock);
        }
        if ( !xwork__buf_appendf(pOutput, "task_id: %llu\nstate: %s\ndelegation: %s\nsuccess: %s\n",
                (unsigned long long)pEntry->uId, bRunning ? "running" : "exited",
                pEntry->sCommand ? pEntry->sCommand : "",
                bRunning ? "-" : (bSuccess ? "true" : "false")) ) { free(sResult); return false; }
        if ( sResult ) {
            size_t iLen = strlen(sResult);
            if ( iLen > iMaxBytes ) iLen = iMaxBytes;
            if ( !xwork__buf_append_cstr(pOutput, "--- final report ---\n") ||
                 !xwork__buf_append(pOutput, sResult, iLen) ||
                 !xwork__buf_append_char(pOutput, '\n') ) { free(sResult); return false; }
        }
        free(sResult);
        return true;
    }
    if ( !xwork__buf_appendf(pOutput, "task_id: %llu\nstate: %s\ncommand: %s\n",
            (unsigned long long)pEntry->uId, bRunning ? "running" : "exited",
            pEntry->sCommand ? pEntry->sCommand : "") ) return false;
    if ( !xwork__append_process_stream(pOutput, pEntry, false, iMaxBytes) ||
         !xwork__append_process_stream(pOutput, pEntry, true, iMaxBytes) ) return false;
    if ( !bRunning ) {
        memset(&tExit, 0, sizeof(tExit));
        (void)xrtProcessStatus(pEntry->pProcess, &tExit);
        if ( !xwork__buf_appendf(pOutput, "exit_code: %d\nexit_kind: %d\nstop_reason: %d\n",
                tExit.Code, tExit.Kind, tExit.Stop) ) return false;
    } else if ( !xwork__buf_append_cstr(pOutput, "use poll to read more output\n") ) {
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Task-family helpers: argv/env parsing shared by exec and spawn.     */
/* ------------------------------------------------------------------ */

static void xwork__free_string_array(char** psItems, size_t iCount)
{
    size_t i;
    if ( !psItems ) return;
    for ( i = 0u; i < iCount; ++i ) free(psItems[i]);
    free(psItems);
}

/* Duplicate a JSON string array into owned C strings; 1..iMax entries.
 * Returns false for absent or malformed keys; nothing leaks on failure. */
static bool xwork__parse_string_array(xvalue* tArgs, const char* sKey,
    char*** ppsItems, size_t* piCount, size_t iMax)
{
    xvalue* tArray = xwork__json_get(tArgs, sKey);
    size_t i;
    *ppsItems = NULL;
    *piCount = 0u;
    if ( !tArray || xrtValueType(tArray) != XVALUE_ARRAY ) return false;
    *piCount = xrtValueCount(tArray);
    if ( *piCount == 0u || *piCount > iMax ) { *piCount = 0u; return false; }
    *ppsItems = (char**)calloc(*piCount, sizeof(char*));
    if ( !*ppsItems ) { *piCount = 0u; return false; }
    for ( i = 0u; i < *piCount; ++i ) {
        xstrview tText;
        xvalue* pItem = xrtValueArrayGet(tArray, i);
        if ( !pItem || !xrtValueGetString(pItem, &tText) || !tText.Data || !tText.Size ) {
            xwork__free_string_array(*ppsItems, *piCount);
            *ppsItems = NULL;
            *piCount = 0u;
            return false;
        }
        (*ppsItems)[i] = xwork__strndup(tText.Data, tText.Size);
        if ( !(*ppsItems)[i] ) {
            xwork__free_string_array(*ppsItems, *piCount);
            *ppsItems = NULL;
            *piCount = 0u;
            return false;
        }
    }
    return true;
}

/* Parse "K=V" entries and append the PAGER/TERM defaults; env storage is
 * owned by the caller, the xprocessenv array borrows it plus literals. */
static bool xwork__build_env(xvalue* tArgs, xprocessenv** ppEnv, size_t* piEnvCount,
    char*** ppsEnvStorage, size_t* piStorageCount)
{
    size_t iModel = 0u;
    size_t iTotal;
    xprocessenv* pEnv = NULL;
    char** psStorage = NULL;
    *ppEnv = NULL;
    *piEnvCount = 0u;
    *ppsEnvStorage = NULL;
    *piStorageCount = 0u;
    if ( xwork__json_get(tArgs, "env") ) {
        if ( !xwork__parse_string_array(tArgs, "env", &psStorage, &iModel, 128u) ) return false;
    }
    iTotal = iModel + 2u;
    pEnv = (xprocessenv*)calloc(iTotal, sizeof(*pEnv));
    if ( !pEnv ) { xwork__free_string_array(psStorage, iModel); return false; }
    {
        size_t n = 0u;
        size_t i;
        for ( i = 0u; i < iModel; ++i ) {
            char* sEq = strchr(psStorage[i], '=');
            if ( !sEq ) continue;   /* malformed entries are skipped */
            *sEq = '\0';
            pEnv[n].Name = psStorage[i];
            pEnv[n].Value = sEq + 1;
            ++n;
        }
        pEnv[n].Name = "PAGER"; pEnv[n].Value = "cat"; ++n;
        pEnv[n].Name = "TERM";  pEnv[n].Value = "dumb"; ++n;
        *piEnvCount = n;
    }
    *ppEnv = pEnv;
    *ppsEnvStorage = psStorage;
    *piStorageCount = iModel;
    return true;
}

/* argv[0] is the program; xprocessconfig.Args excludes it. */
static void xwork__apply_argv_config(xprocessconfig* pConfig, char** psArgv, size_t iArgvCount)
{
    pConfig->Target = XPROCESS_EXEC;
    pConfig->Program = psArgv[0];
    pConfig->Args = (const cstr*)(iArgvCount > 1u ? psArgv + 1 : NULL);
    pConfig->ArgCount = iArgvCount - 1u;
}

/* Space-joined preview for status display. */
static char* xwork__argv_preview(char** psArgv, size_t iArgvCount)
{
    xwork_buf tBuf = {0};
    size_t i;
    for ( i = 0u; i < iArgvCount; ++i ) {
        if ( i && !xwork__buf_append_char(&tBuf, ' ') ) { xwork__buf_unit(&tBuf); return NULL; }
        if ( !xwork__buf_append_cstr(&tBuf, psArgv[i]) ) { xwork__buf_unit(&tBuf); return NULL; }
        if ( tBuf.iLen > 2048u ) break;
    }
    return xwork__buf_detach(&tBuf);
}

static bool xwork__process_wait_all_ready(xwork_agent* pAgent,
    const uint64_t* puIds, size_t iCount, bool bAll, uint32_t uTimeoutMs)
{
    uint64_t uDeadline = xrtDeadlineAfter((uint64_t)uTimeoutMs * UINT64_C(1000));
    for ( ; ; ) {
        xwork_process_entry* pEntry;
        size_t iReady = 0u;
        size_t i;
        for ( i = 0u; i < iCount; ++i ) {
            pEntry = xwork__process_find(pAgent, puIds[i], NULL);
            if ( pEntry && !xwork__task_entry_running(pEntry) ) ++iReady;
        }
        if ( bAll ? iReady == iCount : iReady >  0u ) return true;
        if ( xrtDeadlineExpired(uDeadline) ) return false;
        if ( xwork__is_cancelled(pAgent) ) return false;
        xrtSleep(10u);
    }
}

size_t xworkAgentTakeTaskNotices(xwork_agent* pAgent,
    xwork_task_notice* pNotices, size_t iCapacity)
{
    size_t iTaken = 0u;
    size_t i;
    if ( !pAgent ) { return 0u; }
    for ( i = 0u; i < pAgent->iProcessCount && iTaken < iCapacity; ++i ) {
        xwork_process_entry* pEntry = &pAgent->pProcesses[i];
        if ( pEntry->bNoticeTaken || xwork__task_entry_running(pEntry) ) continue;
        if ( pEntry->uExitedUs == 0u ) pEntry->uExitedUs = xrtClock();
        pNotices[iTaken].uTaskId = pEntry->uId;
        pNotices[iTaken].eKind = pEntry->eKind;
        if ( pEntry->eKind == XWORK_TASK_AGENT ) {
            /* Agent tasks: success flag from the delegation thread; the
             * final report rides the preview slot (borrowed). */
            pNotices[iTaken].iExitCode = pEntry->bSuccess ? 0 : 1;
            pNotices[iTaken].bExitedCleanly = pEntry->bSuccess;
            pNotices[iTaken].sPreview = pEntry->sResult && pEntry->sResult[0]
                ? pEntry->sResult : pEntry->sCommand;
        } else {
            xprocessstatus tExit;
            memset(&tExit, 0, sizeof(tExit));
            (void)xrtProcessStatus(pEntry->pProcess, &tExit);
            pNotices[iTaken].iExitCode = tExit.Code;
            pNotices[iTaken].bExitedCleanly = tExit.Kind == XPROCESS_EXIT_CODE && tExit.Code == 0;
            pNotices[iTaken].sPreview = pEntry->sCommand;
        }
        pNotices[iTaken].sNotify = pEntry->sNotify;     /* borrowed */
        pEntry->bNoticeTaken = true;
        ++iTaken;
    }
    return iTaken;
}

bool xworkTaskWatchdog(xwork_agent* pAgent, xwork_watchdog_digest* pDigest)
{
    static const uint64_t uUncollectedNudgeMs = 60000u;
    uint64_t uNow = xrtClock();
    uint64_t uNextWakeUs = 0u;
    size_t i;
    if ( !pDigest ) { return false; }
    memset(pDigest, 0, sizeof(*pDigest));
    if ( !pAgent ) { return false; }
    for ( i = 0u; i < pAgent->iProcessCount; ++i ) {
        xwork_process_entry* pEntry = &pAgent->pProcesses[i];
        bool bRunning = xwork__task_entry_running(pEntry);
        if ( bRunning ) {
            ++pDigest->iRunningTasks;
            if ( pEntry->uRemindAfterMs != 0u ) {
                uint64_t uElapsedUs = uNow - pEntry->uStartedUs;
                uint64_t uRemindUs = pEntry->uRemindAfterMs * UINT64_C(1000);
                if ( uElapsedUs >= uRemindUs ) {
                    ++pDigest->iStalledTasks;
                    pDigest->bShouldWake = true;
                } else {
                    uint64_t uDue = uRemindUs - uElapsedUs;
                    if ( uNextWakeUs == 0u || uDue < uNextWakeUs ) uNextWakeUs = uDue;
                }
            }
        } else {
            if ( pEntry->uExitedUs == 0u ) pEntry->uExitedUs = uNow;
            if ( !pEntry->bNoticeTaken ) {
                ++pDigest->iUncollectedNotices;
                /* One gentle nudge per task; the model decides after that. */
                if ( !pEntry->bNudged && uNow - pEntry->uExitedUs >= uUncollectedNudgeMs * UINT64_C(1000) ) {
                    pEntry->bNudged = true;
                    pDigest->bShouldWake = true;
                } else if ( !pEntry->bNudged ) {
                    uint64_t uDue = uUncollectedNudgeMs * UINT64_C(1000) - (uNow - pEntry->uExitedUs);
                    if ( uNextWakeUs == 0u || uDue < uNextWakeUs ) uNextWakeUs = uDue;
                }
            }
        }
    }
    pDigest->uNextWakeMs = uNextWakeUs != 0u ? (uNextWakeUs + 999u) / 1000u : 0u;
    return true;
}

static xwork_result xwork__tool_spawn(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    const char* sCwd;
    const char* sNotify;
    char* sResolvedCwd = NULL;
    char** psArgv = NULL;
    char** psEnvStorage = NULL;
    xprocessenv* pEnv = NULL;
    size_t iArgvCount = 0u;
    size_t iEnvCount = 0u;
    size_t iEnvStorageCount = 0u;
    bool bValid;
    bool bMerge;
    uint64_t uCapture;
    uint64_t uRemindMs;
    xprocessconfig tConfig;
    xprocess* pProcess = NULL;
    xwork_process_entry* pEntry = NULL;
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sCwd = xwork__json_text(tArgs, "cwd");
    if ( !sCwd || !sCwd[0] ) sCwd = ".";
    if ( !xwork__parse_string_array(tArgs, "argv", &psArgv, &iArgvCount, 256u) || !psArgv ) {
        eResult = xwork__tool_fail(pOutput, "argv must be a non-empty array of 1-256 strings");
        goto cleanup;
    }
    uCapture = xwork__json_u64(tArgs, "max_capture_bytes", 1048576u, &bValid);
    if ( !bValid || uCapture < 1024u || uCapture > 64u * 1024u * 1024u ) {
        eResult = xwork__tool_fail(pOutput, "max_capture_bytes must be between 1024 and 67108864"); goto cleanup;
    }
    bMerge = xwork__json_bool(tArgs, "merge_stderr", false, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "merge_stderr must be boolean"); goto cleanup; }
    sNotify = xwork__json_text(tArgs, "notify");
    if ( sNotify && strlen(sNotify) > 500u ) {
        eResult = xwork__tool_fail(pOutput, "notify must be at most 500 characters"); goto cleanup;
    }
    uRemindMs = xwork__json_u64(tArgs, "remind_after_ms", 0u, &bValid);
    if ( !bValid || uRemindMs > 3600000u ) {
        eResult = xwork__tool_fail(pOutput, "remind_after_ms must be between 0 and 3600000"); goto cleanup;
    }
    sResolvedCwd = xwork__resolve_path(pAgent, sCwd, pError);
    if ( !sResolvedCwd ) { eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage : "cwd denied"); goto cleanup; }
    if ( !xrtDirExists((str)sResolvedCwd) ) { eResult = xwork__tool_fail(pOutput, "cwd does not exist"); goto cleanup; }
    if ( !xwork__build_env(tArgs, &pEnv, &iEnvCount, &psEnvStorage, &iEnvStorageCount) ) goto oom;
    pEntry = xwork__process_add(pAgent);
    if ( !pEntry ) { eResult = xwork__tool_fail(pOutput, "managed task limit reached; stop or release an existing task"); goto cleanup; }
    pEntry->sCommand = xwork__argv_preview(psArgv, iArgvCount);
    if ( !pEntry->sCommand ) goto oom;
    if ( sNotify && sNotify[0] ) {
        pEntry->sNotify = xwork__strdup(sNotify);
        if ( !pEntry->sNotify ) goto oom;
    }
    pEntry->uRemindAfterMs = uRemindMs;
    xrtProcessConfigInit(&tConfig);
    xwork__apply_argv_config(&tConfig, psArgv, iArgvCount);
    tConfig.WorkDir = sResolvedCwd;
    tConfig.InheritEnv = true;
    tConfig.Env = pEnv;
    tConfig.EnvCount = iEnvCount;
    tConfig.NewGroup = true;
    tConfig.HideWindow = true;
    tConfig.Stdin.Mode = XPROCESS_IO_PIPE;
    tConfig.Stdout.Mode = XPROCESS_IO_PIPE;
    tConfig.Stderr.Mode = bMerge ? XPROCESS_IO_MERGE : XPROCESS_IO_PIPE;
    pProcess = xrtProcessSpawn(&tConfig);
    if ( !pProcess ) {
        xwork__process_remove(pAgent, pAgent->iProcessCount - 1u);
        pEntry = NULL;
        eResult = xwork__tool_fail(pOutput, "failed to start task");
        goto cleanup;
    }
    pEntry->pProcess = pProcess;
    pEntry->pCapture = xwork__process_capture_create(pProcess, (size_t)uCapture, !bMerge);
    if ( !pEntry->pCapture ) {
        (void)xrtProcessKillTree(pProcess);
        (void)xrtProcessWait(pProcess);
        xwork__process_remove(pAgent, pAgent->iProcessCount - 1u);
        pEntry = NULL;
        pProcess = NULL;
        goto oom;
    }
    pProcess = NULL;
    if ( !xwork__buf_appendf(&tOutput, "task_id: %llu\nstate: running\ncommand: %s\n",
            (unsigned long long)pEntry->uId, pEntry->sCommand) ||
         !xwork__buf_append_cstr(&tOutput, "completion is announced at the next turn boundary\n") ||
         !xworkToolOutputSet(pOutput, true, tOutput.pData) ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    if ( pEntry ) xwork__process_remove(pAgent, pAgent->iProcessCount - 1u);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to create managed task");
cleanup:
    if ( pProcess ) {
        if ( xwork__process_running(pProcess) ) { (void)xrtProcessKillTree(pProcess); (void)xrtProcessWait(pProcess); }
        xrtProcessDestroy(pProcess);
    }
    xwork__free_string_array(psArgv, iArgvCount);
    xwork__free_string_array(psEnvStorage, iEnvStorageCount);
    free(pEnv);
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolvedCwd);
    xwork__buf_unit(&tOutput);
    return eResult;
}

static xwork_result xwork__tool_poll(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    uint64_t uId;
    uint64_t uWaitMs;
    uint64_t uMaxBytes;
    bool bValid;
    bool bRelease;
    size_t iIndex;
    xwork_process_entry* pEntry;
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    uId = xwork__json_u64(tArgs, "task_id", 0u, &bValid);
    if ( !bValid || !uId ) { eResult = xwork__tool_fail(pOutput, "positive task_id is required"); goto cleanup; }
    uWaitMs = xwork__json_u64(tArgs, "wait_ms", 0u, &bValid);
    if ( !bValid || uWaitMs > 30000u ) { eResult = xwork__tool_fail(pOutput, "wait_ms must be between 0 and 30000"); goto cleanup; }
    uMaxBytes = xwork__json_u64(tArgs, "max_bytes", 64u * 1024u, &bValid);
    if ( !bValid || uMaxBytes < 256u || uMaxBytes > 1024u * 1024u ) { eResult = xwork__tool_fail(pOutput, "max_bytes must be between 256 and 1048576"); goto cleanup; }
    bRelease = xwork__json_bool(tArgs, "release", false, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "release must be boolean"); goto cleanup; }
    pEntry = xwork__process_find(pAgent, uId, &iIndex);
    if ( !pEntry ) { eResult = xwork__tool_fail(pOutput, "unknown or released task_id"); goto cleanup; }
    if ( bRelease && xwork__task_entry_running(pEntry) ) {
        if ( uWaitMs ) (void)xwork__wait_task(pAgent, pEntry, uWaitMs);
        uWaitMs = 0u;
        if ( xwork__task_entry_running(pEntry) ) {
            eResult = xwork__tool_fail(pOutput, "cannot release a running task; stop it first");
            goto cleanup;
        }
    }
    if ( !xwork__append_process_status(pAgent, &tOutput, pEntry, (uint32_t)uWaitMs, (size_t)uMaxBytes) ) goto oom;
    if ( !xworkToolOutputSet(pOutput, true, tOutput.pData) ) goto oom;
    if ( bRelease ) xwork__process_remove(pAgent, iIndex);
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to report managed process status");
cleanup:
    if ( tArgs ) xrtValueRelease(tArgs);
    xwork__buf_unit(&tOutput);
    return eResult;
}

static xwork_result xwork__tool_stdin(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    uint64_t uId;
    const char* sInput;
    bool bValid;
    bool bNewline;
    bool bClose;
    xwork_process_entry* pEntry;
    xwork_buf tInput = {0};
    xwork_buf tOutput = {0};
    int64_t iWritten = 0;
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    uId = xwork__json_u64(tArgs, "task_id", 0u, &bValid);
    if ( !bValid || !uId ) { eResult = xwork__tool_fail(pOutput, "positive task_id is required"); goto cleanup; }
    sInput = xwork__json_text(tArgs, "input");
    bNewline = xwork__json_bool(tArgs, "append_newline", false, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "append_newline must be boolean"); goto cleanup; }
    bClose = xwork__json_bool(tArgs, "close_stdin", false, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "close_stdin must be boolean"); goto cleanup; }
    if ( !sInput && !bClose ) { eResult = xwork__tool_fail(pOutput, "input or close_stdin=true is required"); goto cleanup; }
    pEntry = xwork__process_find(pAgent, uId, NULL);
    if ( !pEntry ) { eResult = xwork__tool_fail(pOutput, "unknown or released task_id"); goto cleanup; }
    if ( pEntry->eKind != XWORK_TASK_PROCESS ) { eResult = xwork__tool_fail(pOutput, "stdin applies to process tasks only"); goto cleanup; }
    if ( !xwork__process_running(pEntry->pProcess) ) { eResult = xwork__tool_fail(pOutput, "process has already exited"); goto cleanup; }
    if ( pEntry->bStdinClosed ) { eResult = xwork__tool_fail(pOutput, "process stdin is already closed"); goto cleanup; }
    if ( sInput && (sInput[0] || bNewline) ) {
        if ( !xwork__buf_append_cstr(&tInput, sInput) || (bNewline && !xwork__buf_append_char(&tInput, '\n')) ) goto oom;
        iWritten = xrtProcessWrite(pEntry->pProcess, tInput.pData, tInput.iLen);
        if ( iWritten < 0 || (size_t)iWritten != tInput.iLen ) { eResult = xwork__tool_fail(pOutput, "failed to write complete input to process"); goto cleanup; }
    }
    if ( bClose ) {
        if ( !xrtProcessClose(pEntry->pProcess, XPROCESS_STDIN) ) { eResult = xwork__tool_fail(pOutput, "failed to close process stdin"); goto cleanup; }
        pEntry->bStdinClosed = true;
    }
    if ( !xwork__buf_appendf(&tOutput, "task_id: %llu\nwrote: %lld bytes\nstdin: %s\n",
            (unsigned long long)uId, (long long)iWritten, pEntry->bStdinClosed ? "closed" : "open") ||
         !xworkToolOutputSet(pOutput, true, tOutput.pData) ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to prepare managed process input");
cleanup:
    if ( tArgs ) xrtValueRelease(tArgs);
    xwork__buf_unit(&tInput);
    xwork__buf_unit(&tOutput);
    return eResult;
}

static xwork_result xwork__tool_stop(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    uint64_t uId;
    uint64_t uWaitMs;
    const char* sMode;
    bool bValid;
    bool bRelease;
    bool bRequested = true;
    bool bRunning;
    size_t iIndex;
    xwork_process_entry* pEntry;
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    uId = xwork__json_u64(tArgs, "task_id", 0u, &bValid);
    if ( !bValid || !uId ) { eResult = xwork__tool_fail(pOutput, "positive task_id is required"); goto cleanup; }
    sMode = xwork__json_text(tArgs, "mode");
    if ( !sMode || !sMode[0] ) sMode = "terminate";
    if ( strcmp(sMode, "interrupt") != 0 && strcmp(sMode, "terminate") != 0 &&
         strcmp(sMode, "kill") != 0 && strcmp(sMode, "kill_tree") != 0 ) {
        eResult = xwork__tool_fail(pOutput, "mode must be interrupt, terminate, kill, or kill_tree"); goto cleanup;
    }
    uWaitMs = xwork__json_u64(tArgs, "wait_ms", 3000u, &bValid);
    if ( !bValid || uWaitMs > 30000u ) { eResult = xwork__tool_fail(pOutput, "wait_ms must be between 0 and 30000"); goto cleanup; }
    bRelease = xwork__json_bool(tArgs, "release", true, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "release must be boolean"); goto cleanup; }
    pEntry = xwork__process_find(pAgent, uId, &iIndex);
    if ( !pEntry ) { eResult = xwork__tool_fail(pOutput, "unknown or released task_id"); goto cleanup; }
    if ( xwork__task_entry_running(pEntry) ) {
        if ( pEntry->eKind == XWORK_TASK_AGENT ) {
            if ( pEntry->pChildCancel ) (void)xrtCancelRequest(pEntry->pChildCancel);
            bRequested = true;
        } else {
            if ( strcmp(sMode, "interrupt") == 0 ) bRequested = xrtProcessInterrupt(pEntry->pProcess);
            else if ( strcmp(sMode, "terminate") == 0 ) bRequested = xrtProcessTerminate(pEntry->pProcess);
            else if ( strcmp(sMode, "kill") == 0 ) bRequested = xrtProcessKill(pEntry->pProcess);
            else bRequested = xrtProcessKillTree(pEntry->pProcess);
        }
        if ( !bRequested ) { eResult = xwork__tool_fail(pOutput, "task stop request failed"); goto cleanup; }
    }
    if ( uWaitMs && xwork__task_entry_running(pEntry) ) {
        (void)xwork__wait_task(pAgent, pEntry, uWaitMs);
    }
    if ( !xwork__append_process_status(pAgent, &tOutput, pEntry, (uint32_t)uWaitMs, 64u * 1024u) ) goto oom;
    bRunning = xwork__task_entry_running(pEntry);
    if ( !xworkToolOutputSet(pOutput, !bRunning, tOutput.pData) ) goto oom;
    if ( bRelease && !bRunning ) xwork__process_remove(pAgent, iIndex);
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to stop or report managed process");
cleanup:
    if ( tArgs ) xrtValueRelease(tArgs);
    xwork__buf_unit(&tOutput);
    return eResult;
}

static bool xwork__exec_capture_scoped(
    xwork_agent* pAgent,
    const xprocessconfig* pConfig,
    xprocessresult* pResult,
    uint32_t uTimeoutMs,
    xwork_result* pScopeResult
)
{
    xprocessrunoptions tOptions;
    xdeadline uCommandDeadline;
    xwork_operation_status eStatus;
    if ( pScopeResult ) *pScopeResult = XWORK_RESULT_OK;
    if ( !pAgent || !pConfig || !pResult ) return false;
    memset(pResult, 0, sizeof(*pResult));
    eStatus = xwork__operation_status(pAgent);
    if ( eStatus != XWORK_OPERATION_ACTIVE ) {
        if ( pScopeResult ) *pScopeResult = eStatus == XWORK_OPERATION_TIMED_OUT
            ? XWORK_RESULT_TIMEOUT : XWORK_RESULT_CANCELLED;
        return true;
    }
    if ( !xrtProcessRunOptionsInit(&tOptions) ) return false;
    uCommandDeadline = xrtDeadlineAfter((uint64_t)uTimeoutMs * UINT64_C(1000));
    tOptions.Deadline = pAgent->uDeadline != XRT_DEADLINE_NEVER &&
        pAgent->uDeadline < uCommandDeadline ? pAgent->uDeadline : uCommandDeadline;
    tOptions.Cancel = pAgent->pCancel;
    tOptions.StdoutLimit = pAgent->iMaxCapturedCommandBytes;
    tOptions.StderrLimit = pAgent->iMaxCapturedCommandBytes;
    tOptions.Overflow = XPROCESS_OVERFLOW_KEEP_LAST;
    if ( !xrtProcessRun(pConfig, &tOptions, pResult) ) return false;
    if ( pResult->Wait == XWAIT_CANCELLED ) {
        if ( pScopeResult ) *pScopeResult = XWORK_RESULT_CANCELLED;
    } else if ( pResult->Wait == XWAIT_TIMEOUT &&
                pAgent->uDeadline != XRT_DEADLINE_NEVER &&
                xrtDeadlineExpired(pAgent->uDeadline) ) {
        if ( pScopeResult ) *pScopeResult = XWORK_RESULT_TIMEOUT;
    }
    return true;
}

static xwork_result xwork__tool_exec(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    const char* sCwd;
    char* sResolvedCwd = NULL;
    char** psArgv = NULL;
    char** psEnvStorage = NULL;
    xprocessenv* pEnv = NULL;
    size_t iArgvCount = 0u;
    size_t iEnvCount = 0u;
    size_t iEnvStorageCount = 0u;
    bool bValid;
    bool bMerge;
    bool bExpectedExit;
    uint64_t uTimeout;
    uint32_t i;
    uint32_t iExpectedCount = 0u;
    xvalue* tExpectedExitCodes;
    xprocessconfig tConfig;
    xprocessresult tProcess;
    xwork_result eScopeResult = XWORK_RESULT_OK;
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    memset(&tProcess, 0, sizeof(tProcess));
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sCwd = xwork__json_text(tArgs, "cwd");
    if ( !sCwd || !sCwd[0] ) sCwd = ".";
    if ( !xwork__parse_string_array(tArgs, "argv", &psArgv, &iArgvCount, 256u) || !psArgv ) {
        eResult = xwork__tool_fail(pOutput, "argv must be a non-empty array of 1-256 strings");
        goto cleanup;
    }
    uTimeout = xwork__json_u64(tArgs, "timeout_ms", pAgent->uCommandTimeoutMs, &bValid);
    if ( !bValid || uTimeout == 0u || uTimeout > 3600000u ) { eResult = xwork__tool_fail(pOutput, "timeout_ms must be between 1 and 3600000"); goto cleanup; }
    bMerge = xwork__json_bool(tArgs, "merge_stderr", true, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "merge_stderr must be boolean"); goto cleanup; }
    tExpectedExitCodes = xwork__json_get(tArgs, "expected_exit_codes");
    if ( tExpectedExitCodes ) {
        if ( xrtValueType(tExpectedExitCodes) != XVALUE_ARRAY ) {
            eResult = xwork__tool_fail(pOutput, "expected_exit_codes must be a non-empty array of integers"); goto cleanup;
        }
        iExpectedCount = xrtValueCount(tExpectedExitCodes);
        if ( iExpectedCount == 0u || iExpectedCount > 32u ) {
            eResult = xwork__tool_fail(pOutput, "expected_exit_codes must contain between 1 and 32 integers"); goto cleanup;
        }
        for ( i = 0u; i < iExpectedCount; ++i ) {
            xvalue* tCode = xrtValueArrayGet(tExpectedExitCodes, i);
            int64_t iCode;
            if ( !tCode || !xrtValueGetInt(tCode, &iCode) ) {
                eResult = xwork__tool_fail(pOutput, "expected_exit_codes must contain only integers"); goto cleanup;
            }
            if ( iCode < -2147483647LL - 1LL || iCode > 2147483647LL ) {
                eResult = xwork__tool_fail(pOutput, "expected_exit_codes values must fit in a signed 32-bit exit code"); goto cleanup;
            }
        }
    }
    sResolvedCwd = xwork__resolve_path(pAgent, sCwd, pError);
    if ( !sResolvedCwd ) { eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage : "cwd denied"); goto cleanup; }
    if ( !xrtDirExists((str)sResolvedCwd) ) { eResult = xwork__tool_fail(pOutput, "cwd does not exist"); goto cleanup; }
    if ( !xwork__build_env(tArgs, &pEnv, &iEnvCount, &psEnvStorage, &iEnvStorageCount) ) goto oom;
    xrtProcessConfigInit(&tConfig);
    xwork__apply_argv_config(&tConfig, psArgv, iArgvCount);
    tConfig.WorkDir = sResolvedCwd;
    tConfig.InheritEnv = true;
    tConfig.Env = pEnv;
    tConfig.EnvCount = iEnvCount;
    tConfig.NewGroup = true;
    tConfig.HideWindow = true;
    tConfig.Stdout.Mode = XPROCESS_IO_PIPE;
    tConfig.Stderr.Mode = bMerge ? XPROCESS_IO_MERGE : XPROCESS_IO_PIPE;
    tConfig.Stdin.Mode = XPROCESS_IO_NULL;
    if ( !xwork__exec_capture_scoped(pAgent, &tConfig, &tProcess, (uint32_t)uTimeout, &eScopeResult) ) {
        eResult = xwork__tool_fail(pOutput, "failed to start or capture command"); goto cleanup;
    }
    if ( eScopeResult == XWORK_RESULT_TIMEOUT ) {
        xwork__set_error(pError, XWORK_ERROR_TIMEOUT, "agent operation deadline was exceeded during command execution");
        eResult = XWORK_RESULT_TIMEOUT;
        goto cleanup;
    }
    if ( eScopeResult == XWORK_RESULT_CANCELLED ) {
        xwork__set_error(pError, XWORK_ERROR_CANCELLED, "agent operation was cancelled during command execution");
        eResult = XWORK_RESULT_CANCELLED;
        goto cleanup;
    }
    bExpectedExit = tProcess.Wait == XWAIT_OK &&
        tProcess.Status.Kind == XPROCESS_EXIT_CODE;
    if ( bExpectedExit ) {
        if ( tExpectedExitCodes ) {
            bExpectedExit = false;
            for ( i = 0u; i < iExpectedCount; ++i ) {
                int64 iExpected = 0;
                if ( xrtValueGetInt(xrtValueArrayGet(tExpectedExitCodes, i), &iExpected) &&
                     iExpected == (int64_t)tProcess.Status.Code ) {
                    bExpectedExit = true;
                    break;
                }
            }
        } else {
            bExpectedExit = tProcess.Status.Code == 0;
        }
    }
    {
        char* sPreview = xwork__argv_preview(psArgv, iArgvCount);
        bool bAppendOk = sPreview &&
            xwork__buf_appendf(&tOutput, "$ %s\nexit_code: %d\nexit_expected: %s\nduration_ms: %llu%s\n",
                sPreview,
                tProcess.Status.Code,
                bExpectedExit ? "true" : "false",
                (unsigned long long)(tProcess.Duration / UINT64_C(1000)),
                tProcess.Wait == XWAIT_TIMEOUT ? " (timed out)" : "");
        free(sPreview);
        if ( !bAppendOk ) goto oom;
    }
    if ( tProcess.StdoutSize ) {
        if ( !xwork__buf_append_cstr(&tOutput, "--- stdout ---\n") ||
             !xwork__buf_append_process_text(&tOutput, tProcess.Stdout, tProcess.StdoutSize) ||
             !xwork__buf_append_char(&tOutput, '\n') ) goto oom;
    }
    if ( tProcess.StderrSize ) {
        if ( !xwork__buf_append_cstr(&tOutput, "--- stderr ---\n") ||
             !xwork__buf_append_process_text(&tOutput, tProcess.Stderr, tProcess.StderrSize) ||
             !xwork__buf_append_char(&tOutput, '\n') ) goto oom;
    }
    if ( tProcess.StdoutTruncated || tProcess.StderrTruncated ) {
        if ( !xwork__buf_append_cstr(&tOutput, "[process capture was truncated by the configured capture limit]\n") ) goto oom;
    }
    if ( !xworkToolOutputSet(pOutput, bExpectedExit, tOutput.pData ? tOutput.pData : "") ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build exec output");
cleanup:
    xwork__free_string_array(psArgv, iArgvCount);
    xwork__free_string_array(psEnvStorage, iEnvStorageCount);
    free(pEnv);
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolvedCwd);
    xrtProcessResultUnit(&tProcess);
    xwork__buf_unit(&tOutput);
    return eResult;
}

static xwork_result xwork__tool_wait(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    const char* sMode;
    xvalue* tIds;
    bool bValid;
    bool bAll;
    uint64_t uTimeoutMs;
    uint64_t* puIds = NULL;
    size_t iIdCount = 0u;
    size_t i;
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    tIds = xwork__json_get(tArgs, "task_ids");
    if ( !tIds || xrtValueType(tIds) != XVALUE_ARRAY ||
         (iIdCount = xrtValueCount(tIds)) == 0u || iIdCount > 32u ) {
        eResult = xwork__tool_fail(pOutput, "task_ids must be an array of 1-32 integers");
        goto cleanup;
    }
    puIds = (uint64_t*)calloc(iIdCount, sizeof(*puIds));
    if ( !puIds ) goto oom;
    for ( i = 0u; i < iIdCount; ++i ) {
        int64_t iId = 0;
        xvalue* pItem = xrtValueArrayGet(tIds, i);
        if ( !pItem || !xrtValueGetInt(pItem, &iId) || iId < 1 ) {
            eResult = xwork__tool_fail(pOutput, "task_ids must contain positive integers");
            goto cleanup;
        }
        puIds[i] = (uint64_t)iId;
        if ( !xwork__process_find(pAgent, puIds[i], NULL) ) {
            xwork_buf tBad = {0};
            if ( xwork__buf_appendf(&tBad, "unknown or released task_id %llu",
                    (unsigned long long)puIds[i]) && tBad.pData ) {
                eResult = xwork__tool_fail(pOutput, tBad.pData);
            } else {
                eResult = xwork__tool_fail(pOutput, "unknown or released task_id");
            }
            xwork__buf_unit(&tBad);
            goto cleanup;
        }
    }
    sMode = xwork__json_text(tArgs, "mode");
    if ( !sMode || !sMode[0] ) sMode = "any";
    if ( strcmp(sMode, "any") != 0 && strcmp(sMode, "all") != 0 ) {
        eResult = xwork__tool_fail(pOutput, "mode must be any or all");
        goto cleanup;
    }
    bAll = strcmp(sMode, "all") == 0;
    uTimeoutMs = xwork__json_u64(tArgs, "timeout_ms", 120000u, &bValid);
    if ( !bValid || uTimeoutMs > 600000u ) {
        eResult = xwork__tool_fail(pOutput, "timeout_ms must be between 0 and 600000");
        goto cleanup;
    }
    (void)xwork__process_wait_all_ready(pAgent, puIds, iIdCount, bAll, (uint32_t)uTimeoutMs);
    for ( i = 0u; i < iIdCount; ++i ) {
        xwork_process_entry* pEntry = xwork__process_find(pAgent, puIds[i], NULL);
        if ( !pEntry ) continue;
        if ( !xwork__append_process_status(pAgent, &tOutput, pEntry, 0u, 4096u) ) goto oom;
    }
    if ( !xworkToolOutputSet(pOutput, true, tOutput.pData ? tOutput.pData : "") ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build wait output");
cleanup:
    free(puIds);
    if ( tArgs ) xrtValueRelease(tArgs);
    xwork__buf_unit(&tOutput);
    return eResult;
}

bool xworkAgentRegisterBuiltinReadOnlyTools(xwork_agent* pAgent, xwork_error* pError)
{
    static const xwork_tool_definition arrTools[] = {
        {
            "read",
            "Read workspace files. Text returns numbered lines with pagination; a directory path returns its entry listing with an explicit note that it is a directory (prefer ls); images (jpg/png/gif/webp/bmp) are attached for viewing. A trailing marker states whether you saw the whole file ([complete: end of file at line N]) or only part of it ([truncated: ... continue with start_line=N]). Oversized text output is truncated with the full copy spilled to an artifact.",
            "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"start_line\":{\"type\":\"integer\",\"minimum\":1},\"max_lines\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":10000}},\"required\":[\"path\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_read, NULL, NULL
        },
    };
    size_t i;
    xwork_tool_definition tTool;
    if ( !pAgent ) {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is null");
        return false;
    }
    for ( i = 0u; i < sizeof(arrTools) / sizeof(arrTools[0]); ++i ) {
        tTool = arrTools[i];
        tTool.pUserData = pAgent;
        tTool.sSource = "builtin";
        if ( !xworkAgentRegisterTool(pAgent, &tTool, pError) ) return false;
    }
    return true;
}

bool xworkAgentRegisterBuiltinTools(xwork_agent* pAgent, xwork_error* pError)
{
    static const xwork_tool_definition arrTools[] = {
        {
            "write",
            "Create, overwrite, or append a UTF-8 workspace file. Parent directories are created automatically and reported. Prefer edit for small changes.",
            "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"},\"mode\":{\"type\":\"string\",\"enum\":[\"overwrite\",\"create\",\"append\"]}},\"required\":[\"path\",\"content\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_WORKSPACE_WRITE, xwork__tool_write, NULL, NULL
        },
        {
            "edit",
            "Apply exact text edits to one file in a single atomic pass. Each old_text must match the original file uniquely (or set replace_all); 0 or multiple matches return candidate context lines for self-correction.",
            "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"edits\":{\"type\":\"array\",\"minItems\":1,\"maxItems\":64,\"items\":{\"type\":\"object\",\"properties\":{\"old_text\":{\"type\":\"string\"},\"new_text\":{\"type\":\"string\"},\"replace_all\":{\"type\":\"boolean\"}},\"required\":[\"old_text\",\"new_text\"],\"additionalProperties\":false}}},\"required\":[\"path\",\"edits\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_WORKSPACE_WRITE, xwork__tool_edit, NULL, NULL
        },
        {
            "spawn",
            "Start a background task and return task_id. argv is direct (no shell). Output lands in a bounded tail buffer; completion is announced at the next turn boundary.",
            "{\"type\":\"object\",\"properties\":{\"argv\":{\"type\":\"array\",\"minItems\":1,\"maxItems\":256,\"items\":{\"type\":\"string\"}},\"cwd\":{\"type\":\"string\"},\"env\":{\"type\":\"array\",\"maxItems\":128,\"items\":{\"type\":\"string\"}},\"max_capture_bytes\":{\"type\":\"integer\",\"minimum\":1024,\"maximum\":67108864},\"merge_stderr\":{\"type\":\"boolean\"},\"notify\":{\"type\":\"string\",\"maxLength\":500},\"remind_after_ms\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":3600000}},\"required\":[\"argv\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_PROCESS, xwork__tool_spawn, NULL, NULL
        },
        {
            "poll",
            "Read new incremental output from a task (process or subagent) and report its state. Set release=true only after it exits.",
            "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"integer\",\"minimum\":1},\"wait_ms\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":30000},\"max_bytes\":{\"type\":\"integer\",\"minimum\":256,\"maximum\":1048576},\"release\":{\"type\":\"boolean\"}},\"required\":[\"task_id\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_poll, NULL, NULL
        },
        {
            "wait",
            "Block until any (default) or all of the given tasks exit, returning their new output and exit status. An expired timeout returns early with still-running states.",
            "{\"type\":\"object\",\"properties\":{\"task_ids\":{\"type\":\"array\",\"minItems\":1,\"maxItems\":32,\"items\":{\"type\":\"integer\",\"minimum\":1}},\"mode\":{\"type\":\"string\",\"enum\":[\"any\",\"all\"]},\"timeout_ms\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":600000}},\"required\":[\"task_ids\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_wait, NULL, NULL
        },
        {
            "stdin",
            "Write text to a process task's stdin, optionally appending a newline and/or closing stdin.",
            "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"integer\",\"minimum\":1},\"input\":{\"type\":\"string\"},\"append_newline\":{\"type\":\"boolean\"},\"close_stdin\":{\"type\":\"boolean\"}},\"required\":[\"task_id\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_PROCESS, xwork__tool_stdin, NULL, NULL
        },
        {
            "stop",
            "Stop a task. Modes interrupt/terminate/kill/kill_tree apply to processes; a subagent is cancelled cooperatively. Success means the task actually stopped.",
            "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"integer\",\"minimum\":1},\"mode\":{\"type\":\"string\",\"enum\":[\"interrupt\",\"terminate\",\"kill\",\"kill_tree\"]},\"wait_ms\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":30000},\"release\":{\"type\":\"boolean\"}},\"required\":[\"task_id\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_PROCESS, xwork__tool_stop, NULL, NULL
        },
        {
            "exec",
            "Run one command to completion. argv is passed directly with no shell — no pipes or globs; chain work in the command's own tooling or use spawn. Nonzero exit fails unless listed in expected_exit_codes.",
            "{\"type\":\"object\",\"properties\":{\"argv\":{\"type\":\"array\",\"minItems\":1,\"maxItems\":256,\"items\":{\"type\":\"string\"}},\"cwd\":{\"type\":\"string\"},\"env\":{\"type\":\"array\",\"maxItems\":128,\"items\":{\"type\":\"string\"}},\"timeout_ms\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":3600000},\"merge_stderr\":{\"type\":\"boolean\"},\"expected_exit_codes\":{\"type\":\"array\",\"minItems\":1,\"maxItems\":32,\"items\":{\"type\":\"integer\",\"minimum\":-2147483648,\"maximum\":2147483647}}},\"required\":[\"argv\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_PROCESS, xwork__tool_exec, NULL, NULL
        }
    };
    size_t i;
    xwork_tool_definition tTool;
    if ( !pAgent ) {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is null");
        return false;
    }
    if ( !xworkAgentRegisterBuiltinReadOnlyTools(pAgent, pError) ) return false;
    for ( i = 0u; i < sizeof(arrTools) / sizeof(arrTools[0]); ++i ) {
        tTool = arrTools[i];
        tTool.pUserData = pAgent;
        tTool.sSource = "builtin";
        if ( !xworkAgentRegisterTool(pAgent, &tTool, pError) ) return false;
    }
    return true;
}
