typedef struct xwork_walk_entry {
    const char* sAbsolutePath;
    const char* sRelativePath;
    const char* sName;
    bool bDirectory;
    uint64_t uSize;
    uint32_t uDepth;
} xwork_walk_entry;

typedef bool (*xwork_walk_fn)(void* pUserData, const xwork_walk_entry* pEntry);

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

typedef struct xwork_walk_context {
    xwork_agent* pAgent;
    xwork_walk_fn OnEntry;
    void* pUserData;
    bool bRecursive;
    bool bStopped;
    uint32_t uMaxDepth;
} xwork_walk_context;

static bool xwork__is_skipped_directory(const char* sName)
{
    return strcmp(sName, ".") == 0 || strcmp(sName, "..") == 0 ||
           strcmp(sName, ".git") == 0 || strcmp(sName, ".xcode") == 0;
}

static bool xwork__wildcard_match(const char* sPattern, const char* sText)
{
    const char* sStar = NULL;
    const char* sRetry = NULL;
    if ( !sPattern || !sPattern[0] || strcmp(sPattern, "*") == 0 ) return true;
    while ( *sText ) {
        if ( *sPattern == '?' || *sPattern == *sText ) { ++sPattern; ++sText; continue; }
        if ( *sPattern == '*' ) { sStar = sPattern++; sRetry = sText; continue; }
        if ( sStar ) { sPattern = sStar + 1; sText = ++sRetry; continue; }
        return false;
    }
    while ( *sPattern == '*' ) ++sPattern;
    return *sPattern == '\0';
}

static bool xwork__looks_binary(const unsigned char* pData, size_t iSize)
{
    size_t i;
    size_t iCheck = iSize < 8192u ? iSize : 8192u;
    for ( i = 0u; i < iCheck; ++i ) if ( pData[i] == 0u ) return true;
    return false;
}

static bool xwork__walk_directory(xwork_walk_context* pContext, const char* sDirectory, uint32_t uDepth)
{
#if defined(_WIN32)
    WIN32_FIND_DATAW tData;
    HANDLE hFind;
    char* sPattern = xrtPathJoin(sDirectory, "*");
    uint16* sPatternWide;
    if ( !sPattern ) return false;
    sPatternWide = xrtUtf8To16(sPattern, NULL);
    xrtFree(sPattern);
    if ( !sPatternWide ) return false;
    hFind = FindFirstFileW((LPCWSTR)sPatternWide, &tData);
    xrtFree(sPatternWide);
    if ( hFind == INVALID_HANDLE_VALUE ) return false;
    do {
        char* sPath;
        char* sRelative;
        char* sName;
        xwork_walk_entry tEntry;
        bool bDir = (tData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        sName = xrtUtf16To8((const uint16*)tData.cFileName, NULL);
        if ( !sName ) { FindClose(hFind); return false; }
        if ( xwork__is_skipped_directory(sName) ) { xrtFree(sName); continue; }
        sPath = xrtPathJoin(sDirectory, sName);
        if ( !sPath ) { xrtFree(sName); FindClose(hFind); return false; }
        sRelative = xwork__relative_path(pContext->pAgent, sPath);
        if ( !sRelative ) { xrtFree(sName); xrtFree(sPath); FindClose(hFind); return false; }
        memset(&tEntry, 0, sizeof(tEntry));
        tEntry.sAbsolutePath = sPath;
        tEntry.sRelativePath = sRelative;
        tEntry.sName = sName;
        tEntry.bDirectory = bDir;
        tEntry.uSize = ((uint64_t)tData.nFileSizeHigh << 32u) | tData.nFileSizeLow;
        tEntry.uDepth = uDepth;
        if ( !pContext->OnEntry(pContext->pUserData, &tEntry) ) pContext->bStopped = true;
        if ( !pContext->bStopped && bDir && pContext->bRecursive && uDepth < pContext->uMaxDepth ) {
            (void)xwork__walk_directory(pContext, sPath, uDepth + 1u);
        }
        free(sRelative);
        xrtFree(sPath);
        xrtFree(sName);
        if ( pContext->bStopped ) break;
    } while ( FindNextFileW(hFind, &tData) );
    FindClose(hFind);
#else
    DIR* pDir = opendir(sDirectory);
    struct dirent* pItem;
    if ( !pDir ) return false;
    while ( !pContext->bStopped && (pItem = readdir(pDir)) != NULL ) {
        char* sPath;
        char* sRelative;
        struct stat tStat;
        xwork_walk_entry tEntry;
        bool bDir;
        if ( xwork__is_skipped_directory(pItem->d_name) ) continue;
        sPath = xrtPathJoin(sDirectory, pItem->d_name);
        if ( !sPath ) { closedir(pDir); return false; }
        if ( stat(sPath, &tStat) != 0 ) { xrtFree(sPath); continue; }
        bDir = S_ISDIR(tStat.st_mode);
        sRelative = xwork__relative_path(pContext->pAgent, sPath);
        if ( !sRelative ) { xrtFree(sPath); closedir(pDir); return false; }
        memset(&tEntry, 0, sizeof(tEntry));
        tEntry.sAbsolutePath = sPath;
        tEntry.sRelativePath = sRelative;
        tEntry.sName = pItem->d_name;
        tEntry.bDirectory = bDir;
        tEntry.uSize = bDir ? 0u : (uint64_t)tStat.st_size;
        tEntry.uDepth = uDepth;
        if ( !pContext->OnEntry(pContext->pUserData, &tEntry) ) pContext->bStopped = true;
        if ( !pContext->bStopped && bDir && pContext->bRecursive && uDepth < pContext->uMaxDepth ) {
            (void)xwork__walk_directory(pContext, sPath, uDepth + 1u);
        }
        free(sRelative);
        xrtFree(sPath);
    }
    closedir(pDir);
#endif
    return true;
}

static bool xwork__walk(
    xwork_agent* pAgent,
    const char* sRoot,
    bool bRecursive,
    uint32_t uMaxDepth,
    xwork_walk_fn OnEntry,
    void* pUserData
)
{
    xwork_walk_context tContext;
    memset(&tContext, 0, sizeof(tContext));
    tContext.pAgent = pAgent;
    tContext.OnEntry = OnEntry;
    tContext.pUserData = pUserData;
    tContext.bRecursive = bRecursive;
    tContext.uMaxDepth = uMaxDepth;
    return xwork__walk_directory(&tContext, sRoot, 1u);
}

static xwork_result xwork__tool_fail(xwork_tool_output* pOutput, const char* sMessage)
{
    if ( !xworkToolOutputSet(pOutput, false, sMessage) ) return XWORK_RESULT_ERROR;
    return XWORK_RESULT_OK;
}

static xwork_result xwork__tool_read_file(
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
    if ( !xrtFileExists((str)sResolved) ) { eResult = xwork__tool_fail(pOutput, "file does not exist"); goto cleanup; }
    {
        uint64_t uSize = 0u;
        if ( !xwork__path_size(sResolved, &uSize) || uSize > 64u * 1024u * 1024u ) {
            eResult = xwork__tool_fail(pOutput, "file is larger than the 64 MiB read limit"); goto cleanup;
        }
    }
    pData = (unsigned char*)xrtFileReadAll(sResolved, &iSize);
    if ( !pData && iSize ) { eResult = xwork__tool_fail(pOutput, "failed to read file"); goto cleanup; }
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
    if ( uEmitted == uMaxLines && i < iSize && !xwork__buf_appendf(&tOutput, "[more lines available; continue at start_line=%llu]\n", (unsigned long long)uLine) ) goto oom;
    if ( uStartLine >= uLine && i >= iSize && !xwork__buf_append_cstr(&tOutput, "[start_line is beyond end of file]\n") ) goto oom;
    if ( !xworkToolOutputSet(pOutput, true, tOutput.pData ? tOutput.pData : "") ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build read_file output");
cleanup:
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolved);
    if ( pData && iSize ) xrtFree(pData);
    xwork__buf_unit(&tOutput);
    return eResult;
}

typedef struct xwork_list_context {
    xwork_buf tOutput;
    const char* sPattern;
    uint64_t uLimit;
    uint64_t uCount;
} xwork_list_context;

static bool xwork__list_entry(void* pUserData, const xwork_walk_entry* pEntry)
{
    xwork_list_context* pContext = (xwork_list_context*)pUserData;
    if ( pContext->uCount >= pContext->uLimit ) return false;
    if ( pContext->sPattern && pContext->sPattern[0] &&
         !xwork__wildcard_match(pContext->sPattern, pEntry->sName) &&
         !xwork__wildcard_match(pContext->sPattern, pEntry->sRelativePath) ) return true;
    if ( pEntry->bDirectory ) {
        if ( !xwork__buf_appendf(&pContext->tOutput, "%s/\n", pEntry->sRelativePath) ) return false;
    } else if ( !xwork__buf_appendf(
            &pContext->tOutput,
            "%s\t%llu bytes\n",
            pEntry->sRelativePath,
            (unsigned long long)pEntry->uSize
         ) ) return false;
    ++pContext->uCount;
    return pContext->uCount < pContext->uLimit;
}

static xwork_result xwork__tool_list_files(
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
    const char* sPattern;
    char* sResolved = NULL;
    bool bValid;
    bool bRecursive;
    uint64_t uLimit;
    uint64_t uDepth;
    xwork_list_context tList;
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    memset(&tList, 0, sizeof(tList));
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPath = xwork__json_text(tArgs, "path");
    if ( !sPath || !sPath[0] ) sPath = ".";
    sPattern = xwork__json_text(tArgs, "pattern");
    bRecursive = xwork__json_bool(tArgs, "recursive", false, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "recursive must be boolean"); goto cleanup; }
    uLimit = xwork__json_u64(tArgs, "max_results", 500u, &bValid);
    if ( !bValid || uLimit == 0u || uLimit > 10000u ) { eResult = xwork__tool_fail(pOutput, "max_results must be between 1 and 10000"); goto cleanup; }
    uDepth = xwork__json_u64(tArgs, "max_depth", 12u, &bValid);
    if ( !bValid || uDepth == 0u || uDepth > 64u ) { eResult = xwork__tool_fail(pOutput, "max_depth must be between 1 and 64"); goto cleanup; }
    sResolved = xwork__resolve_path(pAgent, sPath, pError);
    if ( !sResolved ) { eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage : "path denied"); goto cleanup; }
    if ( !xrtDirExists((str)sResolved) ) { eResult = xwork__tool_fail(pOutput, "directory does not exist"); goto cleanup; }
    tList.sPattern = sPattern;
    tList.uLimit = uLimit;
    if ( !xwork__buf_appendf(&tList.tOutput, "listing: %s (recursive=%s)\n", sPath, bRecursive ? "true" : "false") ) goto oom;
    if ( !xwork__walk(pAgent, sResolved, bRecursive, (uint32_t)uDepth, xwork__list_entry, &tList) && !tList.tOutput.pData ) {
        eResult = xwork__tool_fail(pOutput, "failed to scan directory"); goto cleanup;
    }
    if ( tList.uCount >= tList.uLimit && !xwork__buf_append_cstr(&tList.tOutput, "[result limit reached; narrow the path or pattern]\n") ) goto oom;
    if ( tList.uCount == 0u && !xwork__buf_append_cstr(&tList.tOutput, "[no matching entries]\n") ) goto oom;
    if ( !xworkToolOutputSet(pOutput, true, tList.tOutput.pData) ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build list_files output");
cleanup:
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolved);
    xwork__buf_unit(&tList.tOutput);
    return eResult;
}

typedef struct xwork_search_context {
    xwork_buf tOutput;
    const char* sQuery;
    const char* sPattern;
    uint64_t uLimit;
    uint64_t uMatches;
    uint64_t uFiles;
} xwork_search_context;

static bool xwork__search_entry(void* pUserData, const xwork_walk_entry* pEntry)
{
    xwork_search_context* pContext = (xwork_search_context*)pUserData;
    unsigned char* pData;
    size_t iSize = 0u;
    size_t i = 0u;
    uint64_t uLine = 1u;
    if ( pEntry->bDirectory ) return true;
    if ( pContext->uMatches >= pContext->uLimit ) return false;
    if ( pContext->sPattern && pContext->sPattern[0] &&
         !xwork__wildcard_match(pContext->sPattern, pEntry->sName) &&
         !xwork__wildcard_match(pContext->sPattern, pEntry->sRelativePath) ) return true;
    if ( pEntry->uSize > 4u * 1024u * 1024u ) return true;
    pData = (unsigned char*)xrtFileReadAll(pEntry->sAbsolutePath, &iSize);
    if ( !pData || xwork__looks_binary(pData, iSize) ) { if ( pData ) xrtFree(pData); return true; }
    ++pContext->uFiles;
    while ( i < iSize && pContext->uMatches < pContext->uLimit ) {
        size_t iStart = i;
        size_t iLen;
        char* sLine;
        while ( i < iSize && pData[i] != '\n' ) ++i;
        iLen = i - iStart;
        if ( iLen && pData[iStart + iLen - 1u] == '\r' ) --iLen;
        sLine = (char*)malloc(iLen + 1u);
        if ( !sLine ) { if ( iSize ) xrtFree(pData); return false; }
        memcpy(sLine, pData + iStart, iLen);
        sLine[iLen] = '\0';
        if ( strstr(sLine, pContext->sQuery) ) {
            size_t iShown = iLen < 1200u ? iLen : 1200u;
            if ( !xwork__buf_appendf(&pContext->tOutput, "%s:%llu: ", pEntry->sRelativePath, (unsigned long long)uLine) ||
                 !xwork__buf_append(&pContext->tOutput, sLine, iShown) ||
                 !xwork__buf_append_char(&pContext->tOutput, '\n') ) {
                free(sLine); if ( iSize ) xrtFree(pData); return false;
            }
            ++pContext->uMatches;
        }
        free(sLine);
        if ( i < iSize ) ++i;
        ++uLine;
    }
    if ( iSize ) xrtFree(pData);
    return pContext->uMatches < pContext->uLimit;
}

static xwork_result xwork__tool_search_text(
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
    const char* sQuery;
    char* sResolved = NULL;
    bool bValid;
    uint64_t uLimit;
    uint64_t uDepth;
    xwork_search_context tSearch;
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    memset(&tSearch, 0, sizeof(tSearch));
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPath = xwork__json_text(tArgs, "path");
    if ( !sPath || !sPath[0] ) sPath = ".";
    sQuery = xwork__json_text(tArgs, "query");
    if ( !sQuery || !sQuery[0] ) { eResult = xwork__tool_fail(pOutput, "query is required"); goto cleanup; }
    uLimit = xwork__json_u64(tArgs, "max_results", 200u, &bValid);
    if ( !bValid || uLimit == 0u || uLimit > 5000u ) { eResult = xwork__tool_fail(pOutput, "max_results must be between 1 and 5000"); goto cleanup; }
    uDepth = xwork__json_u64(tArgs, "max_depth", 24u, &bValid);
    if ( !bValid || uDepth == 0u || uDepth > 64u ) { eResult = xwork__tool_fail(pOutput, "max_depth must be between 1 and 64"); goto cleanup; }
    sResolved = xwork__resolve_path(pAgent, sPath, pError);
    if ( !sResolved ) { eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage : "path denied"); goto cleanup; }
    if ( !xrtDirExists((str)sResolved) ) { eResult = xwork__tool_fail(pOutput, "search path must be a directory"); goto cleanup; }
    tSearch.sQuery = sQuery;
    tSearch.sPattern = xwork__json_text(tArgs, "pattern");
    tSearch.uLimit = uLimit;
    if ( !xwork__buf_appendf(&tSearch.tOutput, "search: %s in %s\n", sQuery, sPath) ) goto oom;
    (void)xwork__walk(pAgent, sResolved, true, (uint32_t)uDepth, xwork__search_entry, &tSearch);
    if ( tSearch.uMatches == 0u && !xwork__buf_appendf(&tSearch.tOutput, "[no matches in %llu text files]\n", (unsigned long long)tSearch.uFiles) ) goto oom;
    if ( tSearch.uMatches >= tSearch.uLimit && !xwork__buf_append_cstr(&tSearch.tOutput, "[result limit reached; narrow the query, path, or pattern]\n") ) goto oom;
    if ( !xworkToolOutputSet(pOutput, true, tSearch.tOutput.pData) ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build search_text output");
cleanup:
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolved);
    xwork__buf_unit(&tSearch.tOutput);
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

static xwork_result xwork__tool_write_file(
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
    bool bValid;
    bool bCreateDirs;
    bool bAppend = false;
    bool bCreate = false;
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
    bCreateDirs = xwork__json_bool(tArgs, "create_dirs", true, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "create_dirs must be boolean"); goto cleanup; }
    sResolved = xwork__resolve_path(pAgent, sPath, pError);
    if ( !sResolved ) { eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage : "path denied"); goto cleanup; }
    if ( bCreate && xrtPathExists((str)sResolved) ) { eResult = xwork__tool_fail(pOutput, "create conflict: target already exists"); goto cleanup; }
    if ( bCreateDirs && !xwork__ensure_parent(sResolved) ) { eResult = xwork__tool_fail(pOutput, "failed to create parent directories"); goto cleanup; }
    if ( !(bAppend ? xwork__write_bytes(sResolved, sContent, true)
                  : xwork__write_atomic_bytes(sResolved, sContent, strlen(sContent))) ) {
        eResult = xwork__tool_fail(pOutput, "failed to write file");
        goto cleanup;
    }
    if ( !xwork__buf_appendf(&tOutput, "wrote %zu bytes to %s (mode=%s)", strlen(sContent), sPath, sMode) ||
         !xworkToolOutputSet(pOutput, true, tOutput.pData) ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build write_file output");
cleanup:
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolved);
    xwork__buf_unit(&tOutput);
    return eResult;
}

static xwork_result xwork__tool_replace_text(
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
    const char* sOld;
    const char* sNew;
    char* sResolved = NULL;
    char* sCurrent = NULL;
    size_t iCurrent = 0u;
    bool bValid;
    bool bAll;
    const char* pScan;
    const char* pMatch;
    size_t iOld;
    size_t iNew;
    uint64_t uMatches = 0u;
    xwork_buf tNext = {0};
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPath = xwork__json_text(tArgs, "path");
    sOld = xwork__json_text(tArgs, "old_text");
    sNew = xwork__json_text(tArgs, "new_text");
    if ( !sPath || !sPath[0] || !sOld || !sOld[0] || !sNew ) { eResult = xwork__tool_fail(pOutput, "path, non-empty old_text, and new_text are required"); goto cleanup; }
    bAll = xwork__json_bool(tArgs, "replace_all", false, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "replace_all must be boolean"); goto cleanup; }
    sResolved = xwork__resolve_path(pAgent, sPath, pError);
    if ( !sResolved ) { eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage : "path denied"); goto cleanup; }
    if ( !xrtFileExists((str)sResolved) ) { eResult = xwork__tool_fail(pOutput, "file does not exist"); goto cleanup; }
    sCurrent = (char*)xrtFileReadAll(sResolved, &iCurrent);
    if ( !sCurrent && iCurrent ) { eResult = xwork__tool_fail(pOutput, "failed to read file"); goto cleanup; }
    iOld = strlen(sOld);
    iNew = strlen(sNew);
    pScan = sCurrent ? sCurrent : "";
    while ( (pMatch = strstr(pScan, sOld)) != NULL ) {
        if ( !xwork__buf_append(&tNext, pScan, (size_t)(pMatch - pScan)) ||
             !xwork__buf_append(&tNext, sNew, iNew) ) goto oom;
        ++uMatches;
        pScan = pMatch + iOld;
        if ( !bAll ) break;
    }
    if ( uMatches == 0u ) { eResult = xwork__tool_fail(pOutput, "replace conflict: old_text was not found"); goto cleanup; }
    if ( !bAll && strstr(pScan, sOld) != NULL ) { eResult = xwork__tool_fail(pOutput, "replace conflict: old_text occurs more than once; add more context or set replace_all=true"); goto cleanup; }
    if ( !xwork__buf_append_cstr(&tNext, pScan) ) goto oom;
    if ( !xwork__write_atomic_bytes(sResolved, tNext.pData, tNext.iLen) ) { eResult = xwork__tool_fail(pOutput, "failed to write replaced file"); goto cleanup; }
    if ( !xwork__buf_appendf(&tOutput, "replaced %llu occurrence%s in %s (%zu -> %zu bytes)",
            (unsigned long long)uMatches, uMatches == 1u ? "" : "s", sPath, iCurrent, tNext.iLen) ||
         !xworkToolOutputSet(pOutput, true, tOutput.pData) ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to apply text replacement");
cleanup:
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolved);
    if ( sCurrent && iCurrent ) xrtFree(sCurrent);
    xwork__buf_unit(&tNext);
    xwork__buf_unit(&tOutput);
    return eResult;
}

#define XWORK_PATCH_MAX_CHANGES 64u
#define XWORK_PATCH_MAX_FILE_BYTES (16u * 1024u * 1024u)

typedef enum xwork_patch_operation {
    XWORK_PATCH_CREATE = 1,
    XWORK_PATCH_REPLACE,
    XWORK_PATCH_DELETE
} xwork_patch_operation;

typedef struct xwork_patch_change {
    xwork_patch_operation eOperation;
    char* sPath;
    char* sResolved;
    char* sBefore;
    size_t iBefore;
    char* sAfter;
    size_t iAfter;
    uint64_t uMatches;
    bool bExisted;
    bool bApplied;
} xwork_patch_change;

static void xwork__patch_change_unit(xwork_patch_change* pChange)
{
    if ( !pChange ) return;
    free(pChange->sPath);
    free(pChange->sResolved);
    free(pChange->sBefore);
    free(pChange->sAfter);
    memset(pChange, 0, sizeof(*pChange));
}

static bool xwork__same_path(const char* sLeft, const char* sRight)
{
#if defined(_WIN32)
    return _stricmp(sLeft, sRight) == 0;
#else
    return strcmp(sLeft, sRight) == 0;
#endif
}

static bool xwork__copy_bytes(const char* pData, size_t iLen, char** ppCopy)
{
    char* pCopy;
    if ( !ppCopy || (!pData && iLen) ) return false;
    pCopy = (char*)malloc(iLen + 1u);
    if ( !pCopy ) return false;
    if ( iLen ) memcpy(pCopy, pData, iLen);
    pCopy[iLen] = '\0';
    *ppCopy = pCopy;
    return true;
}

/* Returns 1 on success, 0 for a deterministic replacement conflict, and -1
 * for allocation failure. The input is known UTF-8 text without embedded NUL. */
static int xwork__make_replacement(
    const char* sCurrent,
    const char* sOld,
    const char* sNew,
    bool bReplaceAll,
    char** ppNext,
    size_t* piNext,
    uint64_t* puMatches
)
{
    const char* pScan = sCurrent;
    const char* pMatch;
    size_t iOld = strlen(sOld);
    size_t iNew = strlen(sNew);
    uint64_t uMatches = 0u;
    xwork_buf tNext = {0};
    while ( (pMatch = strstr(pScan, sOld)) != NULL ) {
        if ( !xwork__buf_append(&tNext, pScan, (size_t)(pMatch - pScan)) ||
             !xwork__buf_append(&tNext, sNew, iNew) ) {
            xwork__buf_unit(&tNext);
            return -1;
        }
        ++uMatches;
        pScan = pMatch + iOld;
        if ( !bReplaceAll ) break;
    }
    if ( uMatches == 0u || (!bReplaceAll && strstr(pScan, sOld) != NULL) ) {
        xwork__buf_unit(&tNext);
        return 0;
    }
    if ( !xwork__buf_append_cstr(&tNext, pScan) ) {
        xwork__buf_unit(&tNext);
        return -1;
    }
    *piNext = tNext.iLen;
    *ppNext = xwork__buf_detach(&tNext);
    *puMatches = uMatches;
    return 1;
}

static bool xwork__patch_restore(xwork_patch_change* pChange)
{
    if ( !pChange->bApplied ) return true;
    if ( pChange->bExisted ) {
        if ( !xwork__ensure_parent(pChange->sResolved) ) return false;
        return xwork__write_atomic_bytes(pChange->sResolved, pChange->sBefore, pChange->iBefore);
    }
    if ( !xrtPathExists((str)pChange->sResolved) ) return true;
    return xrtFileDelete((str)pChange->sResolved);
}

static xwork_result xwork__tool_apply_patch(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    xvalue* tChanges;
    xwork_patch_change* pChanges = NULL;
    uint32_t iCount = 0u;
    uint32_t i;
    uint32_t j;
    uint32_t iApplied = 0u;
    bool bRollbackOk = true;
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    tChanges = xwork__json_get(tArgs, "changes");
    if ( !tChanges || xrtValueType(tChanges) != XVALUE_ARRAY ) {
        eResult = xwork__tool_fail(pOutput, "changes must be an array");
        goto cleanup;
    }
    iCount = xrtValueCount(tChanges);
    if ( iCount == 0u || iCount > XWORK_PATCH_MAX_CHANGES ) {
        eResult = xwork__tool_fail(pOutput, "changes must contain between 1 and 64 entries");
        goto cleanup;
    }
    pChanges = (xwork_patch_change*)calloc(iCount, sizeof(*pChanges));
    if ( !pChanges ) goto oom;

    /* Prepare the full transaction before touching the workspace. */
    for ( i = 0u; i < iCount; ++i ) {
        xvalue* tChange = xrtValueArrayGet(tChanges, i);
        const char* sPath;
        const char* sOperation;
        const char* sContent;
        const char* sOld;
        const char* sNew;
        bool bValid;
        bool bAll;
        void* pFile = NULL;
        size_t iFile = 0u;
        int iReplaceResult;
        xwork_patch_change* pChange = &pChanges[i];
        if ( !tChange || xrtValueType(tChange) != XVALUE_OBJECT ) {
            if ( !xwork__buf_appendf(&tOutput, "change %u must be an object", (unsigned int)(i + 1u)) ) goto oom;
            eResult = xwork__tool_fail(pOutput, tOutput.pData);
            goto cleanup;
        }
        sPath = xwork__json_text(tChange, "path");
        sOperation = xwork__json_text(tChange, "operation");
        if ( !sPath || !sPath[0] || !sOperation || !sOperation[0] ) {
            if ( !xwork__buf_appendf(&tOutput, "change %u requires path and operation", (unsigned int)(i + 1u)) ) goto oom;
            eResult = xwork__tool_fail(pOutput, tOutput.pData);
            goto cleanup;
        }
        pChange->sPath = xwork__strdup(sPath);
        pChange->sResolved = xwork__resolve_path(pAgent, sPath, pError);
        if ( !pChange->sPath || !pChange->sResolved ) {
            if ( !pChange->sPath ) goto oom;
            if ( !xwork__buf_appendf(&tOutput, "change %u (%s): %s", (unsigned int)(i + 1u), sPath,
                    pError && pError->sMessage[0] ? pError->sMessage : "path denied") ) goto oom;
            eResult = xwork__tool_fail(pOutput, tOutput.pData);
            goto cleanup;
        }
        for ( j = 0u; j < i; ++j ) {
            if ( xwork__same_path(pChange->sResolved, pChanges[j].sResolved) ) {
                if ( !xwork__buf_appendf(&tOutput, "change %u (%s): duplicate target in one transaction", (unsigned int)(i + 1u), sPath) ) goto oom;
                eResult = xwork__tool_fail(pOutput, tOutput.pData);
                goto cleanup;
            }
        }
        if ( strcmp(sOperation, "create") == 0 ) pChange->eOperation = XWORK_PATCH_CREATE;
        else if ( strcmp(sOperation, "replace") == 0 ) pChange->eOperation = XWORK_PATCH_REPLACE;
        else if ( strcmp(sOperation, "delete") == 0 ) pChange->eOperation = XWORK_PATCH_DELETE;
        else {
            if ( !xwork__buf_appendf(&tOutput, "change %u (%s): operation must be create, replace, or delete", (unsigned int)(i + 1u), sPath) ) goto oom;
            eResult = xwork__tool_fail(pOutput, tOutput.pData);
            goto cleanup;
        }
        pChange->bExisted = xrtPathExists((str)pChange->sResolved);
        if ( pChange->bExisted && !xrtFileExists((str)pChange->sResolved) ) {
            if ( !xwork__buf_appendf(&tOutput, "change %u (%s): target is not a regular file", (unsigned int)(i + 1u), sPath) ) goto oom;
            eResult = xwork__tool_fail(pOutput, tOutput.pData);
            goto cleanup;
        }
        if ( pChange->eOperation == XWORK_PATCH_CREATE && pChange->bExisted ) {
            if ( !xwork__buf_appendf(&tOutput, "change %u (%s): create conflict, target already exists", (unsigned int)(i + 1u), sPath) ) goto oom;
            eResult = xwork__tool_fail(pOutput, tOutput.pData);
            goto cleanup;
        }
        if ( pChange->eOperation != XWORK_PATCH_CREATE && !pChange->bExisted ) {
            if ( !xwork__buf_appendf(&tOutput, "change %u (%s): target does not exist", (unsigned int)(i + 1u), sPath) ) goto oom;
            eResult = xwork__tool_fail(pOutput, tOutput.pData);
            goto cleanup;
        }
        if ( pChange->bExisted ) {
            uint64_t uFileSize = 0u;
            if ( !xwork__path_size(pChange->sResolved, &uFileSize) ||
                 uFileSize > XWORK_PATCH_MAX_FILE_BYTES ) {
                if ( !xwork__buf_appendf(&tOutput, "change %u (%s): file exceeds the 16 MiB patch limit", (unsigned int)(i + 1u), sPath) ) goto oom;
                eResult = xwork__tool_fail(pOutput, tOutput.pData);
                goto cleanup;
            }
            pFile = xrtFileReadAll(pChange->sResolved, &iFile);
            if ( !pFile && iFile ) {
                if ( !xwork__buf_appendf(&tOutput, "change %u (%s): failed to read current file", (unsigned int)(i + 1u), sPath) ) goto oom;
                eResult = xwork__tool_fail(pOutput, tOutput.pData);
                goto cleanup;
            }
            if ( xwork__looks_binary((const unsigned char*)pFile, iFile) ||
                 (iFile && !xrtUtf8Valid((xstrview){ (const char*)pFile, iFile }, NULL)) ) {
                if ( pFile ) xrtFree(pFile);
                if ( !xwork__buf_appendf(&tOutput, "change %u (%s): target must be UTF-8 text", (unsigned int)(i + 1u), sPath) ) goto oom;
                eResult = xwork__tool_fail(pOutput, tOutput.pData);
                goto cleanup;
            }
            if ( !xwork__copy_bytes((const char*)pFile, iFile, &pChange->sBefore) ) {
                if ( pFile ) xrtFree(pFile);
                goto oom;
            }
            if ( pFile ) xrtFree(pFile);
            pChange->iBefore = iFile;
        }
        if ( pChange->eOperation == XWORK_PATCH_CREATE ) {
            sContent = xwork__json_text(tChange, "content");
            if ( !sContent ) {
                if ( !xwork__buf_appendf(&tOutput, "change %u (%s): create requires content", (unsigned int)(i + 1u), sPath) ) goto oom;
                eResult = xwork__tool_fail(pOutput, tOutput.pData);
                goto cleanup;
            }
            pChange->iAfter = strlen(sContent);
            if ( pChange->iAfter > XWORK_PATCH_MAX_FILE_BYTES ) {
                if ( !xwork__buf_appendf(&tOutput, "change %u (%s): content exceeds the 16 MiB patch limit", (unsigned int)(i + 1u), sPath) ) goto oom;
                eResult = xwork__tool_fail(pOutput, tOutput.pData);
                goto cleanup;
            }
            if ( !xwork__copy_bytes(sContent, pChange->iAfter, &pChange->sAfter) ) goto oom;
        } else if ( pChange->eOperation == XWORK_PATCH_REPLACE ) {
            sOld = xwork__json_text(tChange, "old_text");
            sNew = xwork__json_text(tChange, "new_text");
            bAll = xwork__json_bool(tChange, "replace_all", false, &bValid);
            if ( !sOld || !sOld[0] || !sNew || !bValid ) {
                if ( !xwork__buf_appendf(&tOutput, "change %u (%s): replace requires non-empty old_text, new_text, and optional boolean replace_all", (unsigned int)(i + 1u), sPath) ) goto oom;
                eResult = xwork__tool_fail(pOutput, tOutput.pData);
                goto cleanup;
            }
            iReplaceResult = xwork__make_replacement(pChange->sBefore, sOld, sNew, bAll,
                &pChange->sAfter, &pChange->iAfter, &pChange->uMatches);
            if ( iReplaceResult < 0 ) goto oom;
            if ( iReplaceResult == 0 ) {
                if ( !xwork__buf_appendf(&tOutput, "change %u (%s): replace conflict, old_text must occur exactly once unless replace_all=true", (unsigned int)(i + 1u), sPath) ) goto oom;
                eResult = xwork__tool_fail(pOutput, tOutput.pData);
                goto cleanup;
            }
            if ( pChange->iAfter > XWORK_PATCH_MAX_FILE_BYTES ) {
                if ( !xwork__buf_appendf(&tOutput, "change %u (%s): patched file exceeds the 16 MiB limit", (unsigned int)(i + 1u), sPath) ) goto oom;
                eResult = xwork__tool_fail(pOutput, tOutput.pData);
                goto cleanup;
            }
        }
    }

    for ( i = 0u; i < iCount; ++i ) {
        xwork_patch_change* pChange = &pChanges[i];
        bool bOk;
        if ( pChange->eOperation == XWORK_PATCH_DELETE ) {
            bOk = xrtFileDelete((str)pChange->sResolved);
        } else {
            bOk = xwork__ensure_parent(pChange->sResolved) &&
                  xwork__write_atomic_bytes(pChange->sResolved, pChange->sAfter, pChange->iAfter);
        }
        if ( !bOk ) {
            for ( j = i; j > 0u; --j ) {
                if ( !xwork__patch_restore(&pChanges[j - 1u]) ) bRollbackOk = false;
            }
            xwork__buf_unit(&tOutput);
            if ( !xwork__buf_appendf(&tOutput, "transaction failed at change %u (%s); rollback %s",
                    (unsigned int)(i + 1u), pChange->sPath, bRollbackOk ? "completed" : "was incomplete") ) goto oom;
            eResult = xwork__tool_fail(pOutput, tOutput.pData);
            goto cleanup;
        }
        pChange->bApplied = true;
        ++iApplied;
    }

    xwork__buf_unit(&tOutput);
    if ( !xwork__buf_appendf(&tOutput, "applied %u change%s transactionally:\n",
            (unsigned int)iApplied, iApplied == 1u ? "" : "s") ) goto oom;
    for ( i = 0u; i < iCount; ++i ) {
        const xwork_patch_change* pChange = &pChanges[i];
        const char* sName = pChange->eOperation == XWORK_PATCH_CREATE ? "create" :
                            pChange->eOperation == XWORK_PATCH_REPLACE ? "replace" : "delete";
        if ( !xwork__buf_appendf(&tOutput, "- %s %s (%zu -> %zu bytes%s)\n", sName,
                pChange->sPath, pChange->iBefore, pChange->iAfter,
                pChange->eOperation == XWORK_PATCH_REPLACE && pChange->uMatches > 1u ? ", multiple replacements" : "") ) goto oom;
    }
    if ( !xworkToolOutputSet(pOutput, true, tOutput.pData) ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    if ( iApplied ) {
        for ( j = iApplied; j > 0u; --j ) (void)xwork__patch_restore(&pChanges[j - 1u]);
    }
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to prepare or report apply_patch transaction");
cleanup:
    if ( pChanges ) {
        for ( i = 0u; i < iCount; ++i ) xwork__patch_change_unit(&pChanges[i]);
        free(pChanges);
    }
    if ( tArgs ) xrtValueRelease(tArgs);
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
    memset(pEntry, 0, sizeof(*pEntry));
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

static xwork_process_entry* xwork__process_add(xwork_agent* pAgent)
{
    xwork_process_entry* pNew;
    size_t i;
    size_t iCap;
    for ( i = pAgent->iProcessCount; i > 0u && pAgent->iProcessCount >= pAgent->uMaxManagedProcesses; --i ) {
        if ( !xwork__process_running(pAgent->pProcesses[i - 1u].pProcess) ) xwork__process_remove(pAgent, i - 1u);
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
    xwork_buf* pOutput,
    xwork_process_entry* pEntry,
    uint32_t uWaitMs,
    size_t iMaxBytes
)
{
    bool bRunning;
    xprocessstatus tExit;
    if ( uWaitMs && xwork__process_running(pEntry->pProcess) ) {
        (void)xrtProcessWaitFor(pEntry->pProcess, (uint64_t)uWaitMs * UINT64_C(1000));
    }
    bRunning = xwork__process_running(pEntry->pProcess);
    if ( !xwork__buf_appendf(pOutput, "process_id: %llu\nstate: %s\ncommand: %s\n",
            (unsigned long long)pEntry->uId, bRunning ? "running" : "exited",
            pEntry->sCommand ? pEntry->sCommand : "") ) return false;
    if ( !xwork__append_process_stream(pOutput, pEntry, false, iMaxBytes) ||
         !xwork__append_process_stream(pOutput, pEntry, true, iMaxBytes) ) return false;
    if ( !bRunning ) {
        memset(&tExit, 0, sizeof(tExit));
        (void)xrtProcessStatus(pEntry->pProcess, &tExit);
        if ( !xwork__buf_appendf(pOutput, "exit_code: %d\nexit_kind: %d\nstop_reason: %d\n",
                tExit.Code, tExit.Kind, tExit.Stop) ) return false;
    } else if ( !xwork__buf_append_cstr(pOutput, "use poll_process to read more output\n") ) {
        return false;
    }
    return true;
}

static xwork_result xwork__tool_start_process(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    const char* sCommand;
    const char* sCwd;
    char* sResolvedCwd = NULL;
    bool bValid;
    bool bMerge;
    uint64_t uWaitMs;
    uint64_t uCapture;
    xprocessconfig tConfig;
    xprocess* pProcess = NULL;
    xwork_process_entry* pEntry = NULL;
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sCommand = xwork__json_text(tArgs, "command");
    sCwd = xwork__json_text(tArgs, "cwd");
    if ( !sCwd || !sCwd[0] ) sCwd = ".";
    if ( !sCommand || !sCommand[0] ) { eResult = xwork__tool_fail(pOutput, "command is required"); goto cleanup; }
    uWaitMs = xwork__json_u64(tArgs, "wait_ms", 0u, &bValid);
    if ( !bValid || uWaitMs > 30000u ) { eResult = xwork__tool_fail(pOutput, "wait_ms must be between 0 and 30000"); goto cleanup; }
    uCapture = xwork__json_u64(tArgs, "max_capture_bytes", pAgent->iMaxCapturedCommandBytes, &bValid);
    if ( !bValid || uCapture < 1024u || uCapture > 64u * 1024u * 1024u ) {
        eResult = xwork__tool_fail(pOutput, "max_capture_bytes must be between 1024 and 67108864"); goto cleanup;
    }
    bMerge = xwork__json_bool(tArgs, "merge_stderr", false, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "merge_stderr must be boolean"); goto cleanup; }
    sResolvedCwd = xwork__resolve_path(pAgent, sCwd, pError);
    if ( !sResolvedCwd ) { eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage : "cwd denied"); goto cleanup; }
    if ( !xrtDirExists((str)sResolvedCwd) ) { eResult = xwork__tool_fail(pOutput, "cwd does not exist"); goto cleanup; }
    pEntry = xwork__process_add(pAgent);
    if ( !pEntry ) { eResult = xwork__tool_fail(pOutput, "managed process limit reached; stop or release an existing process"); goto cleanup; }
    pEntry->sCommand = xwork__strdup(sCommand);
    if ( !pEntry->sCommand ) goto oom;
    xrtProcessConfigInit(&tConfig);
    tConfig.Target = XPROCESS_SHELL;
    tConfig.Command = sCommand;
    tConfig.WorkDir = sResolvedCwd;
    tConfig.InheritEnv = true;
    tConfig.NewGroup = true;
    tConfig.HideWindow = true;
    tConfig.Stdin.Mode = XPROCESS_IO_PIPE;
    tConfig.Stdout.Mode = XPROCESS_IO_PIPE;
    tConfig.Stderr.Mode = bMerge ? XPROCESS_IO_MERGE : XPROCESS_IO_PIPE;
    pProcess = xrtProcessSpawn(&tConfig);
    if ( !pProcess ) {
        xwork__process_remove(pAgent, pAgent->iProcessCount - 1u);
        pEntry = NULL;
        eResult = xwork__tool_fail(pOutput, "failed to start process");
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
    if ( !xwork__append_process_status(&tOutput, pEntry, (uint32_t)uWaitMs, 64u * 1024u) ||
         !xworkToolOutputSet(pOutput, true, tOutput.pData) ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    if ( pEntry ) xwork__process_remove(pAgent, pAgent->iProcessCount - 1u);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to create managed process");
cleanup:
    if ( pProcess ) {
        if ( xwork__process_running(pProcess) ) { (void)xrtProcessKillTree(pProcess); (void)xrtProcessWait(pProcess); }
        xrtProcessDestroy(pProcess);
    }
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolvedCwd);
    xwork__buf_unit(&tOutput);
    return eResult;
}

static xwork_result xwork__tool_poll_process(
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
    uId = xwork__json_u64(tArgs, "process_id", 0u, &bValid);
    if ( !bValid || !uId ) { eResult = xwork__tool_fail(pOutput, "positive process_id is required"); goto cleanup; }
    uWaitMs = xwork__json_u64(tArgs, "wait_ms", 0u, &bValid);
    if ( !bValid || uWaitMs > 30000u ) { eResult = xwork__tool_fail(pOutput, "wait_ms must be between 0 and 30000"); goto cleanup; }
    uMaxBytes = xwork__json_u64(tArgs, "max_bytes", 64u * 1024u, &bValid);
    if ( !bValid || uMaxBytes < 256u || uMaxBytes > 1024u * 1024u ) { eResult = xwork__tool_fail(pOutput, "max_bytes must be between 256 and 1048576"); goto cleanup; }
    bRelease = xwork__json_bool(tArgs, "release", false, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "release must be boolean"); goto cleanup; }
    pEntry = xwork__process_find(pAgent, uId, &iIndex);
    if ( !pEntry ) { eResult = xwork__tool_fail(pOutput, "unknown or released process_id"); goto cleanup; }
    if ( bRelease && xwork__process_running(pEntry->pProcess) ) {
        if ( uWaitMs ) (void)xrtProcessWaitFor(pEntry->pProcess, uWaitMs * UINT64_C(1000));
        uWaitMs = 0u;
        if ( xwork__process_running(pEntry->pProcess) ) {
            eResult = xwork__tool_fail(pOutput, "cannot release a running process; stop it first");
            goto cleanup;
        }
    }
    if ( !xwork__append_process_status(&tOutput, pEntry, (uint32_t)uWaitMs, (size_t)uMaxBytes) ) goto oom;
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

static xwork_result xwork__tool_write_process(
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
    uId = xwork__json_u64(tArgs, "process_id", 0u, &bValid);
    if ( !bValid || !uId ) { eResult = xwork__tool_fail(pOutput, "positive process_id is required"); goto cleanup; }
    sInput = xwork__json_text(tArgs, "input");
    bNewline = xwork__json_bool(tArgs, "append_newline", false, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "append_newline must be boolean"); goto cleanup; }
    bClose = xwork__json_bool(tArgs, "close_stdin", false, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "close_stdin must be boolean"); goto cleanup; }
    if ( !sInput && !bClose ) { eResult = xwork__tool_fail(pOutput, "input or close_stdin=true is required"); goto cleanup; }
    pEntry = xwork__process_find(pAgent, uId, NULL);
    if ( !pEntry ) { eResult = xwork__tool_fail(pOutput, "unknown or released process_id"); goto cleanup; }
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
    if ( !xwork__buf_appendf(&tOutput, "process_id: %llu\nwrote: %lld bytes\nstdin: %s\n",
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

static xwork_result xwork__tool_stop_process(
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
    uId = xwork__json_u64(tArgs, "process_id", 0u, &bValid);
    if ( !bValid || !uId ) { eResult = xwork__tool_fail(pOutput, "positive process_id is required"); goto cleanup; }
    sMode = xwork__json_text(tArgs, "mode");
    if ( !sMode || !sMode[0] ) sMode = "interrupt";
    if ( strcmp(sMode, "interrupt") != 0 && strcmp(sMode, "terminate") != 0 &&
         strcmp(sMode, "kill") != 0 && strcmp(sMode, "kill_tree") != 0 ) {
        eResult = xwork__tool_fail(pOutput, "mode must be interrupt, terminate, kill, or kill_tree"); goto cleanup;
    }
    uWaitMs = xwork__json_u64(tArgs, "wait_ms", 3000u, &bValid);
    if ( !bValid || uWaitMs > 30000u ) { eResult = xwork__tool_fail(pOutput, "wait_ms must be between 0 and 30000"); goto cleanup; }
    bRelease = xwork__json_bool(tArgs, "release", true, &bValid);
    if ( !bValid ) { eResult = xwork__tool_fail(pOutput, "release must be boolean"); goto cleanup; }
    pEntry = xwork__process_find(pAgent, uId, &iIndex);
    if ( !pEntry ) { eResult = xwork__tool_fail(pOutput, "unknown or released process_id"); goto cleanup; }
    if ( xwork__process_running(pEntry->pProcess) ) {
        if ( strcmp(sMode, "interrupt") == 0 ) bRequested = xrtProcessInterrupt(pEntry->pProcess);
        else if ( strcmp(sMode, "terminate") == 0 ) bRequested = xrtProcessTerminate(pEntry->pProcess);
        else if ( strcmp(sMode, "kill") == 0 ) bRequested = xrtProcessKill(pEntry->pProcess);
        else bRequested = xrtProcessKillTree(pEntry->pProcess);
        if ( !bRequested ) { eResult = xwork__tool_fail(pOutput, "process stop request failed"); goto cleanup; }
    }
    if ( !xwork__append_process_status(&tOutput, pEntry, (uint32_t)uWaitMs, 64u * 1024u) ) goto oom;
    bRunning = xwork__process_running(pEntry->pProcess);
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

static xwork_result xwork__tool_exec_command(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    const char* sCommand;
    const char* sCwd;
    char* sResolvedCwd = NULL;
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
    sCommand = xwork__json_text(tArgs, "command");
    sCwd = xwork__json_text(tArgs, "cwd");
    if ( !sCwd || !sCwd[0] ) sCwd = ".";
    if ( !sCommand || !sCommand[0] ) { eResult = xwork__tool_fail(pOutput, "command is required"); goto cleanup; }
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
    xrtProcessConfigInit(&tConfig);
    tConfig.Target = XPROCESS_SHELL;
    tConfig.Command = sCommand;
    tConfig.WorkDir = sResolvedCwd;
    tConfig.InheritEnv = true;
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
    if ( !xwork__buf_appendf(&tOutput, "$ %s\nexit_code: %d\nexit_expected: %s\nduration_ms: %llu%s\n",
            sCommand,
            tProcess.Status.Code,
            bExpectedExit ? "true" : "false",
            (unsigned long long)(tProcess.Duration / UINT64_C(1000)),
            tProcess.Wait == XWAIT_TIMEOUT ? " (timed out)" : "") ) goto oom;
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
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build exec_command output");
cleanup:
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolvedCwd);
    xrtProcessResultUnit(&tProcess);
    xwork__buf_unit(&tOutput);
    return eResult;
}

bool xworkAgentRegisterBuiltinReadOnlyTools(xwork_agent* pAgent, xwork_error* pError)
{
    static const xwork_tool_definition arrTools[] = {
        {
            "read_file",
            "Read a UTF-8 text file from the workspace with stable line numbers. Use start_line to continue large files.",
            "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"start_line\":{\"type\":\"integer\",\"minimum\":1},\"max_lines\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":10000}},\"required\":[\"path\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_read_file, NULL, NULL
        },
        {
            "list_files",
            "List files and directories within the workspace. Recursive scans skip .git and .xcode.",
            "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"pattern\":{\"type\":\"string\"},\"recursive\":{\"type\":\"boolean\"},\"max_results\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":10000},\"max_depth\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":64}},\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_list_files, NULL, NULL
        },
        {
            "search_text",
            "Search literal text in workspace files. Use pattern such as *.c to narrow files; binary and files over 4 MiB are skipped.",
            "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"},\"path\":{\"type\":\"string\"},\"pattern\":{\"type\":\"string\"},\"max_results\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":5000},\"max_depth\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":64}},\"required\":[\"query\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_search_text, NULL, NULL
        }
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
            "write_file",
            "Create, overwrite, or append a UTF-8 file inside the workspace. Prefer replace_text for small edits to existing files.",
            "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"},\"mode\":{\"type\":\"string\",\"enum\":[\"overwrite\",\"create\",\"append\"]},\"create_dirs\":{\"type\":\"boolean\"}},\"required\":[\"path\",\"content\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_WORKSPACE_WRITE, xwork__tool_write_file, NULL, NULL
        },
        {
            "replace_text",
            "Replace an exact text block in one workspace file. By default the old text must occur exactly once; include surrounding context for safe edits.",
            "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"old_text\":{\"type\":\"string\"},\"new_text\":{\"type\":\"string\"},\"replace_all\":{\"type\":\"boolean\"}},\"required\":[\"path\",\"old_text\",\"new_text\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_WORKSPACE_WRITE, xwork__tool_replace_text, NULL, NULL
        },
        {
            "apply_patch",
            "Apply a validated multi-file UTF-8 text transaction. Every change is checked before writing; a failed write rolls back earlier changes.",
            "{\"type\":\"object\",\"properties\":{\"changes\":{\"type\":\"array\",\"minItems\":1,\"maxItems\":64,\"items\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"operation\":{\"type\":\"string\",\"enum\":[\"create\",\"replace\",\"delete\"]},\"content\":{\"type\":\"string\"},\"old_text\":{\"type\":\"string\"},\"new_text\":{\"type\":\"string\"},\"replace_all\":{\"type\":\"boolean\"}},\"required\":[\"path\",\"operation\"],\"additionalProperties\":false}}},\"required\":[\"changes\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_WORKSPACE_WRITE, xwork__tool_apply_patch, NULL, NULL
        },
        {
            "start_process",
            "Start a managed long-running shell process in the workspace. Returns a process_id for polling, stdin writes, and cleanup.",
            "{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"},\"cwd\":{\"type\":\"string\"},\"wait_ms\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":30000},\"max_capture_bytes\":{\"type\":\"integer\",\"minimum\":1024,\"maximum\":67108864},\"merge_stderr\":{\"type\":\"boolean\"}},\"required\":[\"command\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_PROCESS, xwork__tool_start_process, NULL, NULL
        },
        {
            "poll_process",
            "Incrementally read new stdout and stderr from a managed process and report its state. Set release=true only after it exits.",
            "{\"type\":\"object\",\"properties\":{\"process_id\":{\"type\":\"integer\",\"minimum\":1},\"wait_ms\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":30000},\"max_bytes\":{\"type\":\"integer\",\"minimum\":256,\"maximum\":1048576},\"release\":{\"type\":\"boolean\"}},\"required\":[\"process_id\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_poll_process, NULL, NULL
        },
        {
            "write_process",
            "Write UTF-8 text to a managed process stdin, optionally append a newline and/or close stdin.",
            "{\"type\":\"object\",\"properties\":{\"process_id\":{\"type\":\"integer\",\"minimum\":1},\"input\":{\"type\":\"string\"},\"append_newline\":{\"type\":\"boolean\"},\"close_stdin\":{\"type\":\"boolean\"}},\"required\":[\"process_id\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_PROCESS, xwork__tool_write_process, NULL, NULL
        },
        {
            "stop_process",
            "Stop a managed process using interrupt, terminate, kill, or kill_tree; returns final incremental output when it exits.",
            "{\"type\":\"object\",\"properties\":{\"process_id\":{\"type\":\"integer\",\"minimum\":1},\"mode\":{\"type\":\"string\",\"enum\":[\"interrupt\",\"terminate\",\"kill\",\"kill_tree\"]},\"wait_ms\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":30000},\"release\":{\"type\":\"boolean\"}},\"required\":[\"process_id\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_PROCESS, xwork__tool_stop_process, NULL, NULL
        },
        {
            "exec_command",
            "Run a non-interactive command in a workspace directory and capture stdout, stderr, exit code, duration, and timeout state. For negative tests, pass expected_exit_codes so an intentional nonzero exit is treated as success; the default is [0].",
            "{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"},\"cwd\":{\"type\":\"string\"},\"timeout_ms\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":3600000},\"merge_stderr\":{\"type\":\"boolean\"},\"expected_exit_codes\":{\"type\":\"array\",\"minItems\":1,\"maxItems\":32,\"items\":{\"type\":\"integer\",\"minimum\":-2147483648,\"maximum\":2147483647}}},\"required\":[\"command\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_PROCESS, xwork__tool_exec_command, NULL, NULL
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
