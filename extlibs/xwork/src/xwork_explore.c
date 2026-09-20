/*
 * xwork_explore.c — 探索三件（ls / glob / grep）
 *
 * 设计出发点：不假设系统装有 ls/rg/fd/git 等外部程序。内置实现全部
 * 进程内完成（xrtDirOpen / 自研 glob / xrtRegex），输出结构化、无区域
 * 设置差异，且随 xwork 本体编译自动覆盖所有目标架构。
 *
 * agent 配置决定模式（XWORK_EXPLORE_INTERNAL / EXTERNAL）：
 *   INTERNAL  — 始终用进程内实现（默认，零外部依赖）。
 *   EXTERNAL  — 委托外部程序（ls/fd/rg 约定）；程序缺失或超时则
 *               静默回落进程内实现（优雅降级）。
 * 路径一律经 xwork__resolve_path 沙箱（与 read/write/edit 同规则）。
 */

/* --------------- 共用：忽略目录与遍历 --------------- */

static const char* const c_xwork_ignore_dirs[] = {
    ".git", ".svn", ".hg", "node_modules", "__pycache__",
    ".venv", "venv", ".tox", ".mypy_cache", ".pytest_cache",
    "target", ".idea", ".vscode", NULL
};

static bool xwork__explore_ignore_dir(const char* sName)
{
    int i;
    for ( i = 0; c_xwork_ignore_dirs[i] != NULL; i++ )
        if ( strcmp(sName, c_xwork_ignore_dirs[i]) == 0 ) return true;
    return false;
}

static const char* const c_xwork_binary_exts[] = {
    ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp", ".ico", ".icns",
    ".zip", ".7z", ".gz", ".xz", ".bz2", ".tar", ".rar",
    ".exe", ".dll", ".so", ".dylib", ".a", ".lib", ".obj", ".o",
    ".pdf", ".woff", ".woff2", ".ttf", ".otf", ".eot",
    ".mp3", ".mp4", ".avi", ".mov", ".wav", ".flac", ".ogg", ".webm",
    ".db", ".sqlite", ".sqlite3", ".pyc", ".pyd", ".class", ".wasm", NULL
};

static bool xwork__explore_binary_ext(const char* sName)
{
    const char* pDot = strrchr(sName, '.');
    int i;
    size_t iLen;
    if ( pDot == NULL ) return false;
    iLen = strlen(pDot);
    for ( i = 0; c_xwork_binary_exts[i] != NULL; i++ ) {
        size_t iExt = strlen(c_xwork_binary_exts[i]);
        if ( iExt != iLen ) continue;
#if defined(_WIN32)
        if ( _stricmp(pDot, c_xwork_binary_exts[i]) == 0 ) return true;
#else
        if ( strcmp(pDot, c_xwork_binary_exts[i]) == 0 ) return true;
#endif
    }
    return false;
}

static char* xwork__explore_join(const char* sDir, const char* sName)
{
    size_t iLen = strlen(sDir) + 1u + strlen(sName) + 1u;
    char* p = (char*)malloc(iLen);
    if ( !p ) return NULL;
    snprintf(p, iLen, "%s/%s", sDir, sName);
    return p;
}

/* --------------- 外部程序委托（EXTERNAL 模式） --------------- */

/*
 * 同步执行外部只读程序（ls/fd/rg），等待至多 20s，回收 stdout。
 * 成功返回 true 且 pOutput 已设置；程序缺失/超时返回 false（调用方回落内置实现）。
 */
static bool xwork__explore_external(xwork_agent* pAgent, const char* const* pArgv,
    size_t iArgc, xwork_tool_output* pOutput, bool* pbFallback)
{
    xprocessconfig tConfig;
    xprocess* pProc = NULL;
    char* sBody = NULL;
    size_t iCap = 256u * 1024u, iLen = 0;
    xwaitresult eWait;
    char aProg[64];

    *pbFallback = true;
    snprintf(aProg, sizeof(aProg), "%s", pArgv[0]);
    xrtProcessConfigInit(&tConfig);
    tConfig.Target = XPROCESS_EXEC;
    tConfig.Program = pArgv[0];
    tConfig.Arg0 = pArgv[0];
    tConfig.Args = pArgv;
    tConfig.ArgCount = iArgc;
    tConfig.WorkDir = pAgent->sWorkspaceRoot[0] ? pAgent->sWorkspaceRoot : ".";
    tConfig.InheritEnv = true;
    tConfig.HideWindow = true;
    tConfig.Stdin.Mode = XPROCESS_IO_NULL;
    tConfig.Stdout.Mode = XPROCESS_IO_PIPE;
    tConfig.Stderr.Mode = XPROCESS_IO_NULL;

    pProc = xrtProcessSpawn(&tConfig);
    if ( pProc == NULL ) return false;   /* 程序缺失：回落 */
    sBody = (char*)malloc(iCap);
    if ( !sBody ) { xrtProcessDestroy(pProc); return false; }
    for ( ; ; ) {
        int64_t iN = xrtProcessRead(pProc, XPROCESS_STDOUT, sBody + iLen, iCap - iLen - 1u);
        if ( iN <= 0 ) break;
        iLen += (size_t)iN;
        if ( iLen + 1u >= iCap ) {
            char* pNew;
            if ( iCap >= 1024u * 1024u ) break;
            iCap *= 2u;
            pNew = (char*)realloc(sBody, iCap);
            if ( !pNew ) break;
            sBody = pNew;
        }
    }
    eWait = xrtProcessWaitFor(pProc, 20u * 1000u * 1000u);
    xrtProcessDestroy(pProc);
    if ( eWait != XWAIT_OK ) { free(sBody); return false; }   /* 超时回落 */
    if ( iLen == 0 ) { free(sBody); return false; }
    sBody[iLen] = 0;
    if ( memchr(sBody, 0, iLen) != NULL ) { free(sBody); return false; }
    if ( !xrtUtf8Valid((xstrview){ sBody, iLen }, NULL) ) { free(sBody); return false; }

    {
        xwork_buf tOut = {0};
        bool bOk;
        if ( !xwork__buf_appendf(&tOut, "[%s]\n", aProg) ) { xwork__buf_unit(&tOut); free(sBody); return false; }
        if ( !xwork__buf_append(&tOut, sBody, iLen) ) { xwork__buf_unit(&tOut); free(sBody); return false; }
        bOk = xworkToolOutputSet(pOutput, true, tOut.pData ? tOut.pData : "");
        xwork__buf_unit(&tOut);
        *pbFallback = false;
        free(sBody);
        return bOk;
    }
}

/* --------------- ls --------------- */

/* 目录条目列表（ls 工具与 read 的目录回退共用；追加进 pOut） */
bool xwork__list_directory(const char* sDir, bool bLong, bool bAll, xwork_buf* pOut)
{
    const size_t iMaxEntries = 2000u;
    xdir hDir = xrtDirOpen(sDir, XDIR_STAT);
    xdirentry tEntry;
    size_t n = 0u;

    if ( hDir == NULL ) return false;
    while ( xrtDirNext(hDir, &tEntry) == XDIR_NEXT_ITEM ) {
        const char* sName = tEntry.Name.Data;
        bool bDir = tEntry.Info.Type == XFILE_TYPE_DIRECTORY;
        if ( !bAll && sName[0] == '.' ) continue;
        if ( n >= iMaxEntries ) {
            if ( !xwork__buf_appendf(pOut, "... truncated at %u entries\n", (unsigned)iMaxEntries) ) {
                xrtDirClose(hDir);
                return false;
            }
            break;
        }
        if ( bLong ) {
            if ( !xwork__buf_appendf(pOut, "%s %10llu %10llu %s%s\n",
                    bDir ? "d" : "f",
                    (unsigned long long)(bDir ? 0u : tEntry.Info.Size),
                    (unsigned long long)((uint64_t)tEntry.Info.Modified / 1000000u),
                    sName, bDir ? "/" : "") ) {
                xrtDirClose(hDir);
                return false;
            }
        } else {
            if ( !xwork__buf_appendf(pOut, "%s%s\n", sName, bDir ? "/" : "") ) {
                xrtDirClose(hDir);
                return false;
            }
        }
        n++;
    }
    xrtDirClose(hDir);
    if ( n == 0 && !xwork__buf_append_cstr(pOut, "(empty directory)\n") ) return false;
    return true;
}

static xwork_result xwork__tool_ls(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    const char* sPath = NULL;
    bool bAll = false, bLong = false, bValid = false;
    char* sResolved = NULL;
    xwork_buf tOut = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;

    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPath = xwork__json_text(tArgs, "path");
    if ( !sPath || !sPath[0] ) sPath = ".";
    bAll = xwork__json_bool(tArgs, "all", false, &bValid);
    bLong = xwork__json_bool(tArgs, "long", false, &bValid);

    sResolved = xwork__resolve_path(pAgent, sPath, pError);
    if ( !sResolved ) { eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage : "path denied"); goto cleanup; }
    if ( !xrtDirExists((str)sResolved) ) { eResult = xwork__tool_fail(pOutput, "path does not exist"); goto cleanup; }
    if ( !xwork__list_directory(sResolved, bLong, bAll, &tOut) ) goto oom;
    if ( !xworkToolOutputSet(pOutput, true, tOut.pData ? tOut.pData : "") ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "out of memory building ls output");
cleanup:
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolved);
    xwork__buf_unit(&tOut);
    return eResult;
}

/* --------------- glob --------------- */

/*
 * 模式语义（fd/gitignore 风格）：
 *   模式不含 '/'  — 匹配任意深度的文件名（如 "*.c"）
 *   模式含 '/'    — 匹配相对路径；"**" 跨任意层级，"*" 段内任意字符
 * 大小写：Windows 不敏感，其余平台敏感（跟随平台文件系统惯例）。
 */
static bool xwork__glob_seg(const char* pPat, const char* pStr, bool bFold)
{
    /* 段内匹配：'*' 任意字符列，'?' 单字符，其余字面（大小写可折叠） */
    if ( *pPat == 0 ) return *pStr == 0;
    if ( *pPat == '*' ) {
        const char* p = pStr;
        for ( ; ; ) {
            if ( xwork__glob_seg(pPat + 1, p, bFold) ) return true;
            if ( *p == 0 ) return false;
            p++;
        }
    }
    if ( *pStr == 0 ) return false;
    {
        char a = *pPat, b = *pStr;
        if ( a != '?' ) {
            if ( bFold ) {
                if ( a >= 'A' && a <= 'Z' ) a += 32;
                if ( b >= 'A' && b <= 'Z' ) b += 32;
            }
            if ( a != b ) return false;
        }
    }
    return xwork__glob_seg(pPat + 1, pStr + 1, bFold);
}

static bool xwork__glob_match(const char* sPattern, const char* sRel, bool bFold)
{
    char aPat[512], aRel[512];
    const char* pSlash = strchr(sPattern, '/');
    if ( pSlash == NULL ) {
        /* 无 '/'：匹配 basename（任意深度） */
        const char* pBase = strrchr(sRel, '/');
        return xwork__glob_seg(sPattern, pBase ? pBase + 1 : sRel, bFold);
    }
    if ( strncmp(sPattern, "**/", 3) == 0 ) {
        /* 模式以 ** 开头：在任意深度匹配剩余模式（前缀或更深层） */
        size_t i;
        snprintf(aPat, sizeof(aPat), "%s", sPattern + 3);
        snprintf(aRel, sizeof(aRel), "%s", sRel);
        if ( xwork__glob_seg(aPat, aRel, bFold) ) return true;
        for ( i = 0; aRel[i]; i++ ) {
            if ( aRel[i] == '/' ) {
                if ( xwork__glob_seg(aPat, aRel + i + 1, bFold) ) return true;
            }
        }
        return false;
    }
    snprintf(aPat, sizeof(aPat), "%s", sPattern);
    snprintf(aRel, sizeof(aRel), "%s", sRel);
    return xwork__glob_seg(aPat, aRel, bFold);
}

typedef struct {
    xwork_buf* pOut;
    size_t nFound;
    size_t iMax;
    char aPattern[512];
    bool bFold;
    bool bTruncated;
} xwork__glob_walk;

static void xwork__glob_walk_dir(xwork__glob_walk* pW, const char* sAbsDir,
    char* sRel, size_t iRelLen, int iDepth)
{
    xdir hDir;
    xdirentry tEntry;

    if ( pW->bTruncated || iDepth >= 16 ) return;
    hDir = xrtDirOpen(sAbsDir, 0);
    if ( hDir == NULL ) return;
    while ( xrtDirNext(hDir, &tEntry) == XDIR_NEXT_ITEM ) {
        const char* sName = tEntry.Name.Data;
        bool bDir = tEntry.Info.Type == XFILE_TYPE_DIRECTORY;
        char* pChild;

        if ( pW->bTruncated ) break;
        if ( sName[0] == '.' && strcmp(sName, "..") != 0 && strcmp(sName, ".") != 0 ) {
            /* 隐藏条目：跳过目录；文件交给模式判断（显式点模式仍可命中） */
            if ( bDir || pW->aPattern[0] != '.' ) continue;
        }
        if ( bDir && xwork__explore_ignore_dir(sName) ) continue;
        {
            size_t n = (size_t)snprintf(sRel + iRelLen, 512 - iRelLen, "%s%s", sName, bDir ? "/" : "");
            if ( !bDir && xwork__glob_match(pW->aPattern, sRel, pW->bFold) ) {
                char* sLine = xwork__strdup(sRel);
                if ( !sLine ) break;
                if ( pW->nFound >= pW->iMax ) {
                    if ( !xwork__buf_append_cstr(pW->pOut, "... truncated\n") ) { free(sLine); }
                    pW->bTruncated = true;
                    free(sLine);
                    break;
                }
                if ( !xwork__buf_append_cstr(pW->pOut, sLine) ||
                     !xwork__buf_append_cstr(pW->pOut, "\n") ) { free(sLine); break; }
                pW->nFound++;
                free(sLine);
            }
            if ( bDir && !pW->bTruncated ) {
                pChild = xwork__explore_join(sAbsDir, sName);
                if ( pChild ) {
                    xwork__glob_walk_dir(pW, pChild, sRel, iRelLen + n, iDepth + 1);
                    free(pChild);
                }
                sRel[iRelLen] = 0;
            }
        }
    }
    xrtDirClose(hDir);
}

static xwork_result xwork__tool_glob(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    const char* sPattern;
    const char* sPath = NULL;
    char* sResolved = NULL;
    uint64_t uMax;
    bool bValid = false;
    char* sRel = NULL;
    xwork__glob_walk tW;
    xwork_buf tOut = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;

    (void)pContext;
    memset(&tW, 0, sizeof(tW));
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPattern = xwork__json_text(tArgs, "pattern");
    if ( !sPattern || !sPattern[0] ) { eResult = xwork__tool_fail(pOutput, "pattern is required"); goto cleanup; }
    if ( strlen(sPattern) >= sizeof(tW.aPattern) ) { eResult = xwork__tool_fail(pOutput, "pattern too long"); goto cleanup; }
    sPath = xwork__json_text(tArgs, "path");
    if ( !sPath || !sPath[0] ) sPath = ".";
    uMax = xwork__json_u64(tArgs, "max_results", 200u, &bValid);
    if ( !bValid || uMax == 0 || uMax > 1000u ) uMax = 200u;

    sResolved = xwork__resolve_path(pAgent, sPath, pError);
    if ( !sResolved ) { eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage : "path denied"); goto cleanup; }
    if ( !xrtDirExists((str)sResolved) ) { eResult = xwork__tool_fail(pOutput, "path does not exist"); goto cleanup; }

    snprintf(tW.aPattern, sizeof(tW.aPattern), "%s", sPattern);
#if defined(_WIN32)
    tW.bFold = true;
#else
    tW.bFold = false;
#endif
    tW.iMax = (size_t)uMax;
    tW.pOut = &tOut;
    sRel = (char*)calloc(1u, 512u);
    if ( !sRel ) goto oom;
    xwork__glob_walk_dir(&tW, sResolved, sRel, 0, 0);
    free(sRel);
    sRel = NULL;
    if ( tW.nFound == 0 && !xwork__buf_append_cstr(&tOut, "(no matches)\n") ) goto oom;
    if ( !xworkToolOutputSet(pOutput, true, tOut.pData ? tOut.pData : "") ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    free(sRel);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "out of memory building glob output");
cleanup:
    free(sRel);
    xwork__buf_unit(&tOut);
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolved);
    return eResult;
}

/* --------------- grep --------------- */

static const char* xwork__casestrstr(const char* sHay, const char* sNeedle)
{
    size_t i;
    if ( !sNeedle[0] ) return sHay;
    for ( i = 0; sHay[i]; i++ ) {
        size_t j;
        for ( j = 0; sNeedle[j]; j++ ) {
            char a = sHay[i + j], b = sNeedle[j];
            if ( a == 0 ) return NULL;
            if ( a >= 'A' && a <= 'Z' ) a += 32;
            if ( b >= 'A' && b <= 'Z' ) b += 32;
            if ( a != b ) break;
        }
        if ( !sNeedle[j] ) return sHay + i;
    }
    return NULL;
}

static bool xwork__line_match(xregex* pRegex, const char* sLine,
    const char* sLiteral, bool bIgnoreCase)
{
    if ( pRegex ) return xrtRegexTest(pRegex, (xstrview){ sLine, strlen(sLine) }) == XREGEX_MATCH;
    return bIgnoreCase ? (xwork__casestrstr(sLine, sLiteral) != NULL)
                       : (strstr(sLine, sLiteral) != NULL);
}

typedef struct {
    xwork_agent* pAgent;
    xwork_buf* pOut;
    xregex* pRegex;
    const char* sLiteral;
    bool bIgnoreCase;
    size_t nFound;
    size_t iMax;
    size_t nSkipped;
    bool bTruncated;
} xwork__grep_ctx;

static void xwork__grep_file(xwork__grep_ctx* pG, const char* sAbsFile, const char* sRel)
{
    size_t iSize = 0;
    bytes pData;
    const char* pLine;
    const char* pEnd;
    size_t iLineNo = 0;

    if ( pG->bTruncated || xwork__explore_binary_ext(sRel) ) return;
    pData = xrtFileReadAll(sAbsFile, &iSize);
    if ( pData == NULL || iSize == 0 ) { xrtFree(pData); return; }
    if ( iSize > 1024u * 1024u || memchr(pData, 0, iSize) != NULL ) {
        xrtFree(pData);
        pG->nSkipped++;
        return;
    }
    pLine = (const char*)pData;
    pEnd = pLine + iSize;
    while ( pLine < pEnd && !pG->bTruncated ) {
        const char* pNL = memchr(pLine, '\n', (size_t)(pEnd - pLine));
        size_t iLen = pNL ? (size_t)(pNL - pLine) : (size_t)(pEnd - pLine);
        char* sCopy;
        iLineNo++;
        if ( iLen > 400u ) iLen = 400u;   /* 行展示截断 */
        sCopy = xwork__strdup(pLine);
        if ( !sCopy ) break;
        sCopy[iLen] = 0;
        {
            size_t iR = strlen(sCopy);
            while ( iR && (sCopy[iR - 1] == '\r' || sCopy[iR - 1] == ' ' || sCopy[iR - 1] == '\t') ) sCopy[--iR] = 0;
        }
        if ( xwork__line_match(pG->pRegex, sCopy, pG->sLiteral, pG->bIgnoreCase) ) {
            char* sShown = sCopy;
            if ( pG->nFound >= pG->iMax ) {
                if ( !xwork__buf_appendf(pG->pOut, "... truncated at %u matches\n", (unsigned)pG->iMax) )
                    { free(sCopy); break; }
                pG->bTruncated = true;
                free(sCopy);
                break;
            }
            pG->nFound++;
            if ( !xwork__buf_appendf(pG->pOut, "%s:%llu: ", sRel, (unsigned long long)iLineNo) ||
                 !xwork__buf_append_cstr(pG->pOut, sShown) ||
                 !xwork__buf_append_cstr(pG->pOut, "\n") ) { free(sCopy); break; }
        }
        free(sCopy);
        pLine = pNL ? pNL + 1 : pEnd;
    }
    xrtFree(pData);
}

static void xwork__grep_walk(xwork__grep_ctx* pG, const char* sAbsDir,
    char* sRel, size_t iRelLen, int iDepth)
{
    xdir hDir;
    xdirentry tEntry;

    if ( pG->bTruncated || iDepth >= 16 ) return;
    hDir = xrtDirOpen(sAbsDir, 0);
    if ( hDir == NULL ) return;
    while ( xrtDirNext(hDir, &tEntry) == XDIR_NEXT_ITEM ) {
        const char* sName = tEntry.Name.Data;
        bool bDir = tEntry.Info.Type == XFILE_TYPE_DIRECTORY;
        char* pChild;

        if ( pG->bTruncated ) break;
        if ( sName[0] == '.' ) continue;
        if ( bDir && xwork__explore_ignore_dir(sName) ) continue;
        if ( bDir ) {
            size_t n = (size_t)snprintf(sRel + iRelLen, 512 - iRelLen, "%s/", sName);
            pChild = xwork__explore_join(sAbsDir, sName);
            if ( pChild ) {
                xwork__grep_walk(pG, pChild, sRel, iRelLen + n, iDepth + 1);
                free(pChild);
            }
            sRel[iRelLen] = 0;
        } else {
            size_t n = (size_t)snprintf(sRel + iRelLen, 512 - iRelLen, "%s", sName);
            pChild = xwork__explore_join(sAbsDir, sName);
            if ( pChild ) {
                xwork__grep_file(pG, pChild, sRel);
                free(pChild);
            }
            sRel[iRelLen] = 0;
            (void)n;
        }
    }
    xrtDirClose(hDir);
}

static xwork_result xwork__tool_grep(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    const char* sPattern;
    const char* sPath = NULL;
    char* sResolved = NULL;
    bool bRegex = true, bIgnoreCase = false, bValid = false;
    uint64_t uMax;
    xregex* pRegex = NULL;
    char* sRel = NULL;
    xwork__grep_ctx tG;
    xwork_buf tOut = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;

    (void)pContext;
    memset(&tG, 0, sizeof(tG));
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPattern = xwork__json_text(tArgs, "pattern");
    if ( !sPattern || !sPattern[0] ) { eResult = xwork__tool_fail(pOutput, "pattern is required"); goto cleanup; }
    sPath = xwork__json_text(tArgs, "path");
    if ( !sPath || !sPath[0] ) sPath = ".";
    bRegex = xwork__json_bool(tArgs, "regex", true, &bValid);
    bIgnoreCase = xwork__json_bool(tArgs, "ignore_case", false, &bValid);
    uMax = xwork__json_u64(tArgs, "max_results", 100u, &bValid);
    if ( !bValid || uMax == 0 || uMax > 500u ) uMax = 100u;

    if ( bRegex ) {
        pRegex = xrtRegexCompile((xstrview){ sPattern, strlen(sPattern) });
        if ( pRegex == NULL ) { eResult = xwork__tool_fail(pOutput, "invalid regex pattern"); goto cleanup; }
    }

    sResolved = xwork__resolve_path(pAgent, sPath, pError);
    if ( !sResolved ) { eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage : "path denied"); goto cleanup; }

    tG.pAgent = pAgent;
    tG.pOut = &tOut;
    tG.pRegex = pRegex;
    tG.sLiteral = sPattern;
    tG.bIgnoreCase = bIgnoreCase;
    tG.iMax = (size_t)uMax;

    if ( xrtFileExists((str)sResolved) ) {
        /* 单文件：直接搜 */
        char* sCopy = xwork__strdup(sResolved);
        const char* pLeaf = strrchr(sResolved, '/');
        if ( !sCopy ) goto oom;
        xwork__grep_file(&tG, sCopy, pLeaf ? pLeaf + 1 : sCopy);
        free(sCopy);
    } else if ( xrtDirExists((str)sResolved) ) {
        sRel = (char*)calloc(1u, 512u);
        if ( !sRel ) goto oom;
        xwork__grep_walk(&tG, sResolved, sRel, 0, 0);
        free(sRel);
        sRel = NULL;
    } else {
        eResult = xwork__tool_fail(pOutput, "path does not exist");
        goto cleanup;
    }

    if ( tG.nFound == 0 && !xwork__buf_appendf(&tOut, "(no matches%s)\n",
            tG.nSkipped ? " — some binary/oversized files skipped" : "") ) goto oom;
    if ( !xworkToolOutputSet(pOutput, true, tOut.pData ? tOut.pData : "") ) goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    free(sRel);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "out of memory building grep output");
cleanup:
    free(sRel);
    if ( pRegex ) xrtRegexRelease(pRegex);
    xwork__buf_unit(&tOut);
    if ( tArgs ) xrtValueRelease(tArgs);
    free(sResolved);
    return eResult;
}

/* --------------- 模式分发：EXTERNAL 委托 / INTERNAL 回落 --------------- */

static xwork_result xwork__tool_ls_dispatch(
    void* pUserData, const xwork_tool_context* pContext,
    const char* sArgumentsJson, xwork_tool_output* pOutput, xwork_error* pError)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    if ( pAgent->bExploreExternal ) {
        xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
        const char* sPath = tArgs ? xwork__json_text(tArgs, "path") : NULL;
        const char* aArgv[4];
        bool bFallback = false;
        xwork_result eR;
        aArgv[0] = pAgent->sLsProgram;
        aArgv[1] = "-A";
        aArgv[2] = "-1";
        aArgv[3] = (sPath && sPath[0]) ? sPath : ".";
        eR = xwork__explore_external(pAgent, aArgv, 4u, pOutput, &bFallback);
        if ( tArgs ) xrtValueRelease(tArgs);
        if ( !bFallback ) return eR;
    }
    return xwork__tool_ls(pUserData, pContext, sArgumentsJson, pOutput, pError);
}

static xwork_result xwork__tool_glob_dispatch(
    void* pUserData, const xwork_tool_context* pContext,
    const char* sArgumentsJson, xwork_tool_output* pOutput, xwork_error* pError)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    if ( pAgent->bExploreExternal && pAgent->sGlobProgram[0] ) {
        xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
        const char* sPattern = tArgs ? xwork__json_text(tArgs, "pattern") : NULL;
        const char* sPath = tArgs ? xwork__json_text(tArgs, "path") : NULL;
        const char* aArgv[4];
        bool bFallback = false;
        xwork_result eR;
        if ( sPattern && sPattern[0] ) {
            aArgv[0] = pAgent->sGlobProgram;
            aArgv[1] = "--glob";
            aArgv[2] = sPattern;
            aArgv[3] = (sPath && sPath[0]) ? sPath : ".";
            eR = xwork__explore_external(pAgent, aArgv, 4u, pOutput, &bFallback);
            if ( tArgs ) xrtValueRelease(tArgs);
            if ( !bFallback ) return eR;
        }
        if ( tArgs ) xrtValueRelease(tArgs);
    }
    return xwork__tool_glob(pUserData, pContext, sArgumentsJson, pOutput, pError);
}

static xwork_result xwork__tool_grep_dispatch(
    void* pUserData, const xwork_tool_context* pContext,
    const char* sArgumentsJson, xwork_tool_output* pOutput, xwork_error* pError)
{
    xwork_agent* pAgent = (xwork_agent*)pUserData;
    if ( pAgent->bExploreExternal && pAgent->sGrepProgram[0] ) {
        xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
        const char* sPattern = tArgs ? xwork__json_text(tArgs, "pattern") : NULL;
        const char* sPath = tArgs ? xwork__json_text(tArgs, "path") : NULL;
        const char* aArgv[5];
        bool bFallback = false;
        xwork_result eR;
        if ( sPattern && sPattern[0] ) {
            aArgv[0] = pAgent->sGrepProgram;
            aArgv[1] = "--line-number";
            aArgv[2] = "--no-heading";
            aArgv[3] = sPattern;
            aArgv[4] = (sPath && sPath[0]) ? sPath : ".";
            eR = xwork__explore_external(pAgent, aArgv, 5u, pOutput, &bFallback);
            if ( tArgs ) xrtValueRelease(tArgs);
            if ( !bFallback ) return eR;
        }
        if ( tArgs ) xrtValueRelease(tArgs);
    }
    return xwork__tool_grep(pUserData, pContext, sArgumentsJson, pOutput, pError);
}

/* --------------- 注册（模式分发 + 外部委托 + 内部回落） --------------- */


bool xwork__register_explore_tools(xwork_agent* pAgent, xwork_error* pError)
{
    static const xwork_tool_definition arrTools[] = {
        {
            "ls",
            "List a directory's entries (name, type; optional size/mtime). Use before reading to discover actual file names.",
            "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"all\":{\"type\":\"boolean\"},\"long\":{\"type\":\"boolean\"}},\"required\":[],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_ls_dispatch, NULL, NULL
        },
        {
            "glob",
            "Find files by glob pattern (e.g. \"*.c\" at any depth, \"**/*.h\", \"src/*.md\"). Prefer this over shell find; results are capped.",
            "{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\",\"minLength\":1},\"path\":{\"type\":\"string\"},\"max_results\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":1000}},\"required\":[\"pattern\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_glob_dispatch, NULL, NULL
        },
        {
            "grep",
            "Search text inside workspace files (regex or literal), returns file:line:text. Prefer this over shell grep; binary files are skipped, results are capped.",
            "{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\",\"minLength\":1},\"path\":{\"type\":\"string\"},\"regex\":{\"type\":\"boolean\"},\"ignore_case\":{\"type\":\"boolean\"},\"max_results\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":500}},\"required\":[\"pattern\"],\"additionalProperties\":false}",
            true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_grep_dispatch, NULL, NULL
        },
    };
    size_t i;
    for ( i = 0; i < sizeof(arrTools) / sizeof(arrTools[0]); i++ ) {
        xwork_tool_definition tTool = arrTools[i];
        tTool.pUserData = pAgent;   /* 工具经 pUserData 取 agent（与 builtin 同约定） */
        tTool.sSource = "builtin-explore";
        if ( !xworkAgentRegisterTool(pAgent, &tTool, pError) ) return false;
    }
    return true;
}
