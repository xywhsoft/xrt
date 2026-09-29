/*
 * xwork_python.c — python 三态工具（设计文档"便利封装二件"之一）
 *
 * 同步态（默认）：与一个持久 REPL 进程（python -u -i -q）交互；
 *   每次调用把用户代码 base64 包裹成单行喂入 stdin，随后注入哨兵 print；
 *   读线程聚合 stdout，调用方等哨兵出现即回收本次输出。
 *   超时/崩溃由 harness 机械收走：REPL 复位并在下次调用重生。
 * reset 态：销毁并重建 REPL（返回 state: reset）。
 * background 态：全新独立解释器（-u -c），复用 spawn 任务表，
 *   与普通进程同权接受 poll/wait/stop，完成事件照常轮次边界注入；
 *   不继承 REPL 状态（设计文档铁律）。
 *
 * 解释器路径由 agent 配置 sPythonPath 指定（agent 程序可探测/切换版本）。
 * REPL 状态生命周期 = agent 生命周期；host 每回合重建 agent 时状态随之
 * 复位（跨回合持久是宿主侧决策，见设计文档）。
 */

#define XWORK_PY_BUF_MAX     (1024u * 1024u)   /* stdout 累积上限（保尾） */
#define XWORK_PY_SLICE_MS    200u

/* --------------- REPL 生命周期 --------------- */

static void xwork__py_buf_reset(xwork_agent* pAgent)
{
    free(pAgent->pPyBuf);
    pAgent->pPyBuf = NULL;
    pAgent->iPyLen = 0;
    pAgent->iPyCap = 0;
}

static void xwork__py_kill_locked(xwork_agent* pAgent)
{
    if ( pAgent->pPyProc != NULL ) {
        if ( xwork__process_running(pAgent->pPyProc) ) {
            (void)xrtProcessKillTree(pAgent->pPyProc);
            (void)xrtProcessWait(pAgent->pPyProc);
        }
        xrtProcessDestroy(pAgent->pPyProc);
        pAgent->pPyProc = NULL;
    }
    xwork__py_buf_reset(pAgent);
    pAgent->bPyEof = false;
}

static int32_t xwork__py_reader_proc(ptr pArg)
{
    xwork_agent* pAgent = (xwork_agent*)pArg;
    char aChunk[8192];

    for ( ; ; ) {
        int64_t iN = xrtProcessRead(pAgent->pPyProc, XPROCESS_STDOUT, aChunk, sizeof(aChunk));
        if ( iN <= 0 ) {
            xrtMutexLock(pAgent->pPyLock);
            pAgent->bPyEof = true;
            xrtCondBroadcast(pAgent->pPyCond);
            xrtMutexUnlock(pAgent->pPyLock);
            return 0;
        }
        xrtMutexLock(pAgent->pPyLock);
        {
            size_t iNew = pAgent->iPyLen + (size_t)iN;
            if ( iNew + 1u > pAgent->iPyCap ) {
                size_t iNewCap = pAgent->iPyCap ? pAgent->iPyCap * 2u : 8192u;
                char* pNew;
                if ( iNewCap > XWORK_PY_BUF_MAX ) {
                    /* 保尾：丢弃前半，保留后半 */
                    size_t iKeep = pAgent->iPyLen > XWORK_PY_BUF_MAX / 2u
                        ? XWORK_PY_BUF_MAX / 2u : pAgent->iPyLen;
                    memmove(pAgent->pPyBuf, pAgent->pPyBuf + pAgent->iPyLen - iKeep, iKeep);
                    pAgent->iPyLen = iKeep;
                    iNew = pAgent->iPyLen + (size_t)iN;
                }
                pNew = (char*)realloc(pAgent->pPyBuf, iNewCap);
                if ( !pNew ) { xrtMutexUnlock(pAgent->pPyLock); return 0; }
                pAgent->pPyBuf = pNew;
                pAgent->iPyCap = iNewCap;
            }
            memcpy(pAgent->pPyBuf + pAgent->iPyLen, aChunk, (size_t)iN);
            pAgent->iPyLen += (size_t)iN;
            pAgent->pPyBuf[pAgent->iPyLen] = 0;
            xrtCondBroadcast(pAgent->pPyCond);
        }
        xrtMutexUnlock(pAgent->pPyLock);
    }
    return 0;
}

static bool xwork__py_write_line(xwork_agent* pAgent, const char* sLine)
{
    size_t iLen = strlen(sLine), iOff = 0;
    while ( iOff < iLen ) {
        int64_t iN = xrtProcessWrite(pAgent->pPyProc, sLine + iOff, iLen - iOff);
        if ( iN <= 0 ) return false;
        iOff += (size_t)iN;
    }
    return xrtProcessWrite(pAgent->pPyProc, "\n", 1) == 1;
}

static bool xwork__py_spawn_locked(xwork_agent* pAgent, xwork_error* pError)
{
    xprocessconfig tConfig;
    const char* aArgv[3];

    xrtProcessConfigInit(&tConfig);
    tConfig.Target = XPROCESS_EXEC;
    tConfig.Program = pAgent->sPythonPath;
    tConfig.Arg0 = pAgent->sPythonPath;
    aArgv[0] = "-u";
    aArgv[1] = "-i";
    aArgv[2] = "-q";
    tConfig.Args = aArgv;
    tConfig.ArgCount = 3u;
    tConfig.InheritEnv = true;
    tConfig.HideWindow = true;
    tConfig.Stdin.Mode = XPROCESS_IO_PIPE;
    tConfig.Stdout.Mode = XPROCESS_IO_PIPE;
    tConfig.Stderr.Mode = XPROCESS_IO_NULL;   /* python 级 stderr 已重定向 stdout */

    pAgent->pPyProc = xrtProcessSpawn(&tConfig);
    if ( pAgent->pPyProc == NULL ) {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "failed to spawn python (check sPythonPath)");
        return false;
    }
    pAgent->uPySeq = 0;
    pAgent->bPyEof = false;
    pAgent->pPyReader = xrtThreadCreate(xwork__py_reader_proc, pAgent, 0u);
    if ( pAgent->pPyReader == NULL ) {
        xrtProcessDestroy(pAgent->pPyProc);
        pAgent->pPyProc = NULL;
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to start python reader thread");
        return false;
    }
    /* 引导：python 级 stderr → stdout（traceback 也走哨兵通道） */
    if ( !xwork__py_write_line(pAgent, "import sys; sys.stderr = sys.stdout") ||
         !xwork__py_write_line(pAgent, "print(\"MDODONE0\\x02\")") ) {
        xwork__py_kill_locked(pAgent);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "python interpreter did not respond to bootstrap");
        return false;
    }
    {   /* 等 bootstrap 哨兵，见到后清缓冲：首次调用输出不被引导输出污染 */
        uint64_t uBootDeadline = xrtClock() + 10u * 1000u * 1000u;
        for ( ; ; ) {
            if ( pAgent->pPyBuf != NULL && strstr(pAgent->pPyBuf, "MDODONE0") != NULL ) {
                xwork__py_buf_reset(pAgent);
                break;
            }
            if ( pAgent->bPyEof || xrtClock() >= uBootDeadline ) {
                xwork__py_kill_locked(pAgent);
                xwork__set_error(pError, XWORK_ERROR_CONTEXT, "python interpreter did not respond to bootstrap");
                return false;
            }
            xrtCondWaitFor(pAgent->pPyCond, pAgent->pPyLock, XWORK_PY_SLICE_MS * 1000u);
        }
    }
    return true;
}

void xwork__python_unit(xwork_agent* pAgent)
{
    if ( pAgent->pPyLock == NULL ) return;
    xrtMutexLock(pAgent->pPyLock);
    if ( pAgent->pPyProc != NULL ) {
        if ( xwork__process_running(pAgent->pPyProc) ) {
            (void)xrtProcessKillTree(pAgent->pPyProc);
            (void)xrtProcessWait(pAgent->pPyProc);
        }
        xrtProcessDestroy(pAgent->pPyProc);
        pAgent->pPyProc = NULL;
    }
    xwork__py_buf_reset(pAgent);
    xrtMutexUnlock(pAgent->pPyLock);
    if ( pAgent->pPyReader != NULL ) {
        xrtThreadWaitFor(pAgent->pPyReader, 500u * 1000u);
        pAgent->pPyReader = NULL;
    }
    if ( pAgent->pPyLock != NULL ) { xrtMutexDestroy(pAgent->pPyLock); pAgent->pPyLock = NULL; }
    if ( pAgent->pPyCond != NULL ) { xrtCondDestroy(pAgent->pPyCond); pAgent->pPyCond = NULL; }
}

/* --------------- 辅助 --------------- */

static void xwork__py_json_escape(xwork_buf* pBuf, const char* s)
{
    size_t i;
    for ( i = 0; s[i] != 0; i++ ) {
        unsigned char c = (unsigned char)s[i];
        char aEsc[8];
        if ( c == '"' || c == '\\' ) {
            aEsc[0] = '\\'; aEsc[1] = (char)c; aEsc[2] = 0;
            xwork__buf_append_cstr(pBuf, aEsc);
        } else if ( c < 0x20 ) {
            snprintf(aEsc, sizeof(aEsc), "\\u%04x", c);
            xwork__buf_append_cstr(pBuf, aEsc);
        } else {
            xwork__buf_append(pBuf, s + i, 1);
        }
    }
}

/* 标准字母表 base64（无换行）；调用方 free */
static char* xwork__py_b64(const unsigned char* pData, size_t iLen)
{
    static const char sB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t iOut = ((iLen + 2u) / 3u) * 4u;
    char* p = (char*)malloc(iOut + 1u);
    size_t i, o = 0;
    if ( !p ) return NULL;
    for ( i = 0; i + 2u < iLen; i += 3u ) {
        uint32_t v = ((uint32_t)pData[i] << 16) | ((uint32_t)pData[i + 1u] << 8) | pData[i + 2u];
        p[o++] = sB64[(v >> 18) & 63u];
        p[o++] = sB64[(v >> 12) & 63u];
        p[o++] = sB64[(v >> 6) & 63u];
        p[o++] = sB64[v & 63u];
    }
    if ( i < iLen ) {
        uint32_t v = (uint32_t)pData[i] << 16;
        if ( i + 1u < iLen ) v |= (uint32_t)pData[i + 1u] << 8;
        p[o++] = sB64[(v >> 18) & 63u];
        p[o++] = sB64[(v >> 12) & 63u];
        p[o++] = (i + 1u < iLen) ? sB64[(v >> 6) & 63u] : '=';
        p[o++] = '=';
    }
    p[o] = 0;
    return p;
}

/* --------------- 工具实现 --------------- */

static xwork_result xwork__tool_python(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    xwork_agent* pAgent = pContext ? (xwork_agent*)pContext->pAgent : (xwork_agent*)pUserData;
    xvalue* tArgs = xwork__json_parse_object(sArgumentsJson);
    const char* sCodeRaw;
    char* sCode = NULL;
    bool bReset = false, bBackground = false, bValid = false;
    uint64_t uTimeout;
    if ( !tArgs ) return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sCodeRaw = xwork__json_text(tArgs, "code");
    if ( !sCodeRaw || !sCodeRaw[0] ) {
        xrtValueRelease(tArgs);
        return xwork__tool_fail(pOutput, "code is required");
    }
    sCode = xwork__strdup(sCodeRaw);
    if ( !sCode ) { xrtValueRelease(tArgs); return xwork__tool_fail(pOutput, "out of memory"); }
    bReset = xwork__json_bool(tArgs, "reset", false, &bValid);
    bBackground = xwork__json_bool(tArgs, "background", false, &bValid);
    uTimeout = xwork__json_u64(tArgs, "timeout_ms", 120000u, &bValid);
    if ( !bValid || uTimeout < 1000u || uTimeout > 600000u ) uTimeout = 120000u;

    /* ---- background：全新独立解释器，复用 spawn 任务表 ---- */
    if ( bBackground ) {
        xwork_buf tSynth = {0};
        char* sJson = NULL;
        if ( !xwork__buf_append_cstr(&tSynth, "{\"argv\":[\"") ) goto bg_oom;
        xwork__py_json_escape(&tSynth, pAgent->sPythonPath);
        xwork__buf_append_cstr(&tSynth, "\",\"-u\",\"-c\",\"");
        xwork__py_json_escape(&tSynth, sCode);
        xwork__buf_append_cstr(&tSynth, "\"],\"max_capture_bytes\":1048576}");
        sJson = tSynth.pData;
        tSynth.pData = NULL;   /* 所有权移交 */
        xwork__buf_unit(&tSynth);
        (void)xwork__tool_spawn(pAgent, pContext, sJson ? sJson : "{}", pOutput, pError);
        free(sJson);
        free(sCode);
        xrtValueRelease(tArgs);
        return XWORK_RESULT_OK;
    bg_oom:
        xwork__buf_unit(&tSynth);
        free(sCode);
        xrtValueRelease(tArgs);
        return xwork__tool_fail(pOutput, "out of memory");
    }

    /* ---- reset：显式复位 REPL ---- */
    if ( bReset ) {
        xrtMutexLock(pAgent->pPyLock);
        xwork__py_kill_locked(pAgent);
        xrtMutexUnlock(pAgent->pPyLock);
        free(sCode);
        xrtValueRelease(tArgs);
        if ( !xworkToolOutputSet(pOutput, true,
                "{\"state\":\"reset\",\"note\":\"interpreter cleared; next call starts fresh\"}") )
            return XWORK_RESULT_ERROR;
        return XWORK_RESULT_OK;
    }

    /* ---- 同步：持久 REPL（惰性创建；崩溃/超时后下次调用重生） ---- */
    xrtMutexLock(pAgent->pPyLock);
    if ( pAgent->pPyProc == NULL || pAgent->bPyEof ) {
        xwork__py_kill_locked(pAgent);
        if ( !xwork__py_spawn_locked(pAgent, pError) ) {
            xrtMutexUnlock(pAgent->pPyLock);
            free(sCode);
            xrtValueRelease(tArgs);
            return xwork__tool_fail(pOutput, "python interpreter failed to start (check sPythonPath)");
        }
    }

    /* base64 包裹用户代码 + 哨兵（序列号防旧哨兵串扰） */
    {
        uint32_t uSeq = ++pAgent->uPySeq;
        char aSentinel[32];
        char* sB64 = xwork__py_b64((const unsigned char*)sCode, strlen(sCode));
        char* sLineA;
        char sLineB[48];
        size_t iMark = pAgent->iPyLen;
        char* pHit = NULL;
        uint64_t uDeadline;

        snprintf(aSentinel, sizeof(aSentinel), "MDODONE%u", (unsigned)uSeq);
        if ( !sB64 ) {
            xrtMutexUnlock(pAgent->pPyLock);
            free(sCode); xrtValueRelease(tArgs);
            return xwork__tool_fail(pOutput, "out of memory");
        }
        sLineA = (char*)malloc(strlen(sB64) + 96u);
        if ( !sLineA ) {
            free(sB64); xrtMutexUnlock(pAgent->pPyLock);
            free(sCode); xrtValueRelease(tArgs);
            return xwork__tool_fail(pOutput, "out of memory");
        }
        snprintf(sLineA, strlen(sB64) + 96u,
            "import base64 as _b; exec(compile(_b.b64decode(\"%s\").decode(\"utf-8\"), \"<mdo>\", \"exec\"))", sB64);
        snprintf(sLineB, sizeof(sLineB), "print(\"%s\\x02\")", aSentinel);

        if ( !xwork__py_write_line(pAgent, sLineA) || !xwork__py_write_line(pAgent, sLineB) ) {
            xwork__py_kill_locked(pAgent);
            xrtMutexUnlock(pAgent->pPyLock);
            free(sLineA); free(sB64); free(sCode); xrtValueRelease(tArgs);
            return xwork__tool_fail(pOutput, "python interpreter pipe broke; state was reset");
        }

        /* 等哨兵（只认本次序列号；deadline 由 harness 机械收走） */
        uDeadline = xrtClock() + uTimeout * 1000u;
        for ( ; ; ) {
            if ( pAgent->pPyBuf != NULL && iMark <= pAgent->iPyLen )
                pHit = strstr(pAgent->pPyBuf + iMark, aSentinel);
            if ( pHit != NULL ) break;
            if ( pAgent->bPyEof ) {
                xwork__py_kill_locked(pAgent);
                xrtMutexUnlock(pAgent->pPyLock);
                free(sLineA); free(sB64); free(sCode); xrtValueRelease(tArgs);
                return xwork__tool_fail(pOutput, "python interpreter exited during execution; state was reset");
            }
            if ( xrtClock() >= uDeadline ) {
                xwork__py_kill_locked(pAgent);
                xrtMutexUnlock(pAgent->pPyLock);
                free(sLineA); free(sB64); free(sCode); xrtValueRelease(tArgs);
                return xwork__tool_fail(pOutput,
                    "timeout: code did not finish; interpreter was reset (state lost)");
            }
            xrtCondWaitFor(pAgent->pPyCond, pAgent->pPyLock, XWORK_PY_SLICE_MS * 1000u);
        }

        /* 哨兵前即本次输出（iMark 后算起；Windows \r 折叠） */
        {
            size_t iOutLen = (size_t)(pHit - (pAgent->pPyBuf + iMark));
            char* sResult = (char*)malloc(iOutLen + 1u);
            if ( !sResult ) {
                xrtMutexUnlock(pAgent->pPyLock);
                free(sLineA); free(sB64); free(sCode); xrtValueRelease(tArgs);
                return xwork__tool_fail(pOutput, "out of memory");
            }
            memcpy(sResult, pAgent->pPyBuf + iMark, iOutLen);
            sResult[iOutLen] = 0;
            {
                char* r = sResult; char* w = sResult;
                while ( *r ) { if ( *r != '\r' ) *w++ = *r; r++; }
                *w = 0;
            }
            xrtMutexUnlock(pAgent->pPyLock);
            free(sLineA); free(sB64); free(sCode); xrtValueRelease(tArgs);
            if ( !xworkToolOutputSet(pOutput, true, sResult) ) {
                free(sResult);
                return XWORK_RESULT_ERROR;
            }
            free(sResult);
            return XWORK_RESULT_OK;
        }
    }
}

bool xwork__register_python_tool(xwork_agent* pAgent, xwork_error* pError)
{
    xwork_tool_definition tTool;

    memset(&tTool, 0, sizeof(tTool));
    tTool.sName = "python";
    tTool.sDescription =
        "Run python code. Sync (default): executes in a persistent interpreter, state (variables/imports) is kept between calls and printed output is returned. "
        "reset=true: clear the interpreter. background=true: run in a fresh independent interpreter and return task_id for poll/wait/stop (does not inherit state). "
        "Prefer this for text processing, computation, and multi-step transformations.";
    tTool.sParametersJson =
        "{\"type\":\"object\",\"properties\":{"
        "\"code\":{\"type\":\"string\",\"minLength\":1},"
        "\"reset\":{\"type\":\"boolean\"},"
        "\"background\":{\"type\":\"boolean\"},"
        "\"timeout_ms\":{\"type\":\"integer\",\"minimum\":1000,\"maximum\":600000}"
        "},\"required\":[\"code\"],\"additionalProperties\":false}";
    tTool.bStrict = true;
    tTool.eEffect = XWORK_TOOL_EFFECT_PROCESS;
    tTool.OnExecute = xwork__tool_python;
    tTool.pUserData = NULL;   /* 经 pContext->pAgent 取宿主 */
    tTool.sSource = "builtin-python";
    return xworkAgentRegisterTool(pAgent, &tTool, pError);
}
