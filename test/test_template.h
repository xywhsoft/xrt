// 内部函数：__Test_Template_PrintResult
static void __Test_Template_PrintResult(const char* sLabel, int bPass)
{
	printf("  %s : %s\n", sLabel, bPass ? "PASS" : "FAIL");
}

typedef struct
{
	char* sPrefix;
} __Test_Template_PrefixData;

typedef struct
{
	xtetemplate hTemplate;
	const char* sName;
	char* sResult;
	size_t iRetSize;
} __Test_Template_ConcurrentCtx;


// 内部函数：__Test_Template_CopyArgText
static char* __Test_Template_CopyArgText(const char* sText, uint32 iSize)
{
	if ( sText == NULL ) {
		return NULL;
	}
	if ( (iSize >= 2u) && (((sText[0] == '\'') && (sText[iSize - 1u] == '\'')) || ((sText[0] == '"') && (sText[iSize - 1u] == '"'))) ) {
		return xrtCopyStr((str)&sText[1], iSize - 2u);
	}
	return xrtCopyStr((str)sText, iSize);
}


// 内部函数：__Test_Template_BufferWriter
static int __Test_Template_BufferWriter(void* pUserData, const char* sText, size_t iSize)
{
	return xrtBufferAppend((xbuffer)pUserData, (ptr)sText, (uint32)iSize, XBUF_BINARY) ? 1 : 0;
}


// 内部函数：__Test_Template_RenderWithError
static char* __Test_Template_RenderWithError(xtetemplate hTemplate, xvalue pCurrent, xvalue pGlobal, xdict pIncludeMap, size_t* pRetSize, XTE_Error* pError)
{
	XTE_RenderOptions tOptions = { 0 };
	XTE_Writer tWriter = { 0 };
	xbuffer_struct tBuf = { 0 };
	char chZero = 0;

	if ( pRetSize != NULL ) {
		*pRetSize = 0u;
	}

	xrtBufferInit(&tBuf, 0);
	tWriter.procWrite = __Test_Template_BufferWriter;
	tWriter.pUserData = &tBuf;
	tOptions.pCurrent = pCurrent;
	tOptions.pRoot = pCurrent;
	tOptions.pGlobal = pGlobal;
	tOptions.pIncludeMap = pIncludeMap;
	tOptions.pWriter = &tWriter;

	if ( !xteRenderEx(hTemplate, &tOptions, pError) ) {
		xrtBufferUnit(&tBuf);
		return NULL;
	}
	if ( !xrtBufferAppend(&tBuf, &chZero, 1, XBUF_BINARY) ) {
		xrtBufferUnit(&tBuf);
		return NULL;
	}
	if ( pRetSize != NULL ) {
		*pRetSize = tBuf.Length - 1u;
	}

	return tBuf.Buffer;
}


// 内部函数：__Test_Template_ConcurrentRenderProc
static uint32 __Test_Template_ConcurrentRenderProc(ptr pArg)
{
	__Test_Template_ConcurrentCtx* pCtx = (__Test_Template_ConcurrentCtx*)pArg;
	xvalue tblData = NULL;
	xvalue tblUser = NULL;

	if ( pCtx == NULL ) {
		return 1u;
	}

	tblData = xvoCreateTable();
	tblUser = xvoCreateTable();
	if ( (tblData == NULL) || (tblUser == NULL) ) {
		xvoUnref(tblUser);
		xvoUnref(tblData);
		return 2u;
	}

	xvoTableSetText(tblUser, "name", 0, (char*)pCtx->sName, 0, FALSE);
	xvoTableSetValue(tblData, "user", 0, tblUser, TRUE);
	pCtx->sResult = xteMake(pCtx->hTemplate, tblData, NULL, NULL, &pCtx->iRetSize);

	xvoUnref(tblData);
	return (pCtx->sResult != NULL) ? 0u : 3u;
}

#ifdef XTE_DEBUGMODE
// 内部函数：__Test_Template_DumpToText
static char* __Test_Template_DumpToText(xtetemplate hTemplate)
{
	XTE_Writer tWriter = { 0 };
	xbuffer_struct tBuf = { 0 };
	char chZero = 0;

	xrtBufferInit(&tBuf, 0);
	tWriter.procWrite = __Test_Template_BufferWriter;
	tWriter.pUserData = &tBuf;
	if ( !xteTemplateDump(hTemplate, &tWriter, 0) ) {
		xrtBufferUnit(&tBuf);
		return NULL;
	}
	if ( !xrtBufferAppend(&tBuf, &chZero, 1, XBUF_BINARY) ) {
		xrtBufferUnit(&tBuf);
		return NULL;
	}

	return tBuf.Buffer;
}
#endif

// 内部函数：__Test_Template_PrefixParse
static int __Test_Template_PrefixParse(XTE_StmtParseCtx* pCtx, void** ppData)
{
	const XTE_ArgItem* pArg = xteStmtParseRequireExprType(pCtx, 0, XTE_EXPR_TEXT, "test prefix requires text literal");
	const char* sRaw = NULL;
	__Test_Template_PrefixData* pData = xrtCalloc(1, sizeof(*pData));

	if ( pArg == NULL ) {
		xrtFree(pData);
		return 0;
	}
	sRaw = xteArgRawText(pCtx->pArgs, pArg);
	if ( (pData == NULL) || (sRaw == NULL) ) {
		xrtFree(pData);
		return 0;
	}

	pData->sPrefix = __Test_Template_CopyArgText(sRaw, pArg->iRawSize);
	if ( pData->sPrefix == NULL ) {
		xrtFree(pData);
		return 0;
	}

	ppData[0] = pData;
	return 1;
}


// 内部函数：__Test_Template_PathArgParse
static int __Test_Template_PathArgParse(XTE_StmtParseCtx* pCtx, void** ppData)
{
	const XTE_ArgItem* pArg = xteStmtParseRequireNamedExprType(pCtx, "value", 0, XTE_EXPR_PATH, "test patharg requires named path");
	const char* sRaw = NULL;
	__Test_Template_PrefixData* pData = xrtCalloc(1, sizeof(*pData));

	if ( pArg == NULL ) {
		xrtFree(pData);
		return 0;
	}
	sRaw = xteArgRawText(pCtx->pArgs, pArg);
	if ( (pData == NULL) || (sRaw == NULL) ) {
		xrtFree(pData);
		return 0;
	}

	pData->sPrefix = xrtCopyStr((str)sRaw, pArg->iRawSize);
	if ( pData->sPrefix == NULL ) {
		xrtFree(pData);
		return 0;
	}

	ppData[0] = pData;
	return 1;
}


// 内部函数：__Test_Template_ParseFailParse
static int __Test_Template_ParseFailParse(XTE_StmtParseCtx* pCtx, void** ppData)
{
	(void)ppData;
	return xteStmtParseSetError(pCtx, XTE_ERROR_PARSE, "test parse callback failed");
}


// 内部函数：__Test_Template_PrefixFree
static void __Test_Template_PrefixFree(void* pData)
{
	__Test_Template_PrefixData* pPrefix = (__Test_Template_PrefixData*)pData;

	if ( pPrefix == NULL ) {
		return;
	}

	xrtFree(pPrefix->sPrefix);
	xrtFree(pPrefix);
}


// 内部函数：__Test_Template_UpperRender
static XTE_Flow __Test_Template_UpperRender(XTE_StmtRenderCtx* pCtx)
{
	const XTE_ArgItem* pArg = xteStmtRequireArg(pCtx, 0, "test upper arg is required");
	char* sText = NULL;
	char* sUpper = NULL;
	XTE_Flow iRet = XTE_FLOW_ERROR;

	if ( pArg == NULL ) {
		return XTE_FLOW_ERROR;
	}
	sText = xteEvalArgText(pCtx->pRender, pArg);
	sUpper = xrtUCase(sText, 0, FALSE);
	if ( sUpper && xteStmtWrite(pCtx, sUpper, 0) ) {
		iRet = XTE_FLOW_OK;
	}

	xrtFree(sUpper);
	xrtFree(sText);
	return iRet;
}


// 内部函数：__Test_Template_WrapRender
static XTE_Flow __Test_Template_WrapRender(XTE_StmtRenderCtx* pCtx)
{
	const XTE_ArgItem* pTagArg = xteFindNamedArg(pCtx->pArgs, "tag", 0);
	char* sTag = pTagArg ? xteEvalArgText(pCtx->pRender, pTagArg) : (char*)xrtCopyStr("div", 0);

	if ( sTag == NULL ) {
		return XTE_FLOW_ERROR;
	}
	if ( !xteStmtWrite(pCtx, "<", 1) ) {
		xrtFree(sTag);
		return XTE_FLOW_ERROR;
	}
	if ( !xteStmtWrite(pCtx, sTag, 0) ) {
		xrtFree(sTag);
		return XTE_FLOW_ERROR;
	}
	if ( !xteStmtWrite(pCtx, ">", 1) ) {
		xrtFree(sTag);
		return XTE_FLOW_ERROR;
	}
	if ( !xteStmtRenderBody(pCtx) ) {
		xrtFree(sTag);
		return XTE_FLOW_ERROR;
	}
	if ( !xteStmtWrite(pCtx, "</", 2) ) {
		xrtFree(sTag);
		return XTE_FLOW_ERROR;
	}
	if ( !xteStmtWrite(pCtx, sTag, 0) ) {
		xrtFree(sTag);
		return XTE_FLOW_ERROR;
	}
	if ( !xteStmtWrite(pCtx, ">", 1) ) {
		xrtFree(sTag);
		return XTE_FLOW_ERROR;
	}

	xrtFree(sTag);
	return XTE_FLOW_OK;
}


// 内部函数：__Test_Template_WithRender
static XTE_Flow __Test_Template_WithRender(XTE_StmtRenderCtx* pCtx)
{
	const XTE_ArgItem* pArg = xteStmtRequireArg(pCtx, 0, "test with scope is required");
	xvalue pScope = NULL;
	int bOk = 0;

	if ( pArg == NULL ) {
		return XTE_FLOW_ERROR;
	}
	pScope = xteEvalArgValue(pCtx->pRender, pArg);
	bOk = xteStmtRenderBodyWithScope(pCtx, NULL, pScope);
	xvoUnref(pScope);
	return bOk ? XTE_FLOW_OK : XTE_FLOW_ERROR;
}


// 内部函数：__Test_Template_RepeatRender
static XTE_Flow __Test_Template_RepeatRender(XTE_StmtRenderCtx* pCtx)
{
	const XTE_ArgItem* pCountArg = xteStmtRequireArg(pCtx, 0, "test repeat count is required");
	int64 iCount = 0;
	uint32 i = 0;

	if ( pCountArg == NULL ) {
		return XTE_FLOW_ERROR;
	}
	if ( !xteEvalArgInt(pCtx->pRender, pCountArg, &iCount) ) {
		return XTE_FLOW_ERROR;
	}

	for ( i = 0; i < (uint32)((iCount < 0) ? 0 : iCount); i++ ) {
		if ( pCtx->pBody && pCtx->pBody->iCount ) {
			if ( !xteStmtRenderBody(pCtx) ) {
				return XTE_FLOW_ERROR;
			}
		} else {
			const XTE_ArgItem* pTextArg = NULL;
			char* sText = NULL;

			pTextArg = xteStmtRequireArg(pCtx, 1, "test repeat inline text is required");
			if ( pTextArg == NULL ) {
				return XTE_FLOW_ERROR;
			}
			sText = xteEvalArgText(pCtx->pRender, pTextArg);
			if ( (sText == NULL) || !xteStmtWrite(pCtx, sText, 0) ) {
				xrtFree(sText);
				return XTE_FLOW_ERROR;
			}
			xrtFree(sText);
		}
	}

	return XTE_FLOW_OK;
}


// 内部函数：__Test_Template_PrefixRender
static XTE_Flow __Test_Template_PrefixRender(XTE_StmtRenderCtx* pCtx)
{
	__Test_Template_PrefixData* pData = (__Test_Template_PrefixData*)pCtx->pData;

	if ( (pData == NULL) || (pData->sPrefix == NULL) ) {
		return XTE_FLOW_ERROR;
	}
	if ( !xteStmtWrite(pCtx, pData->sPrefix, 0) ) {
		return XTE_FLOW_ERROR;
	}
	return xteStmtRenderBody(pCtx) ? XTE_FLOW_OK : XTE_FLOW_ERROR;
}


// 内部函数：__Test_Template_PathArgRender
static XTE_Flow __Test_Template_PathArgRender(XTE_StmtRenderCtx* pCtx)
{
	__Test_Template_PrefixData* pData = (__Test_Template_PrefixData*)pCtx->pData;

	if ( (pData == NULL) || (pData->sPrefix == NULL) ) {
		return xteStmtSetError(pCtx, XTE_ERROR_RENDER, "test patharg render data missing");
	}
	return xteStmtWrite(pCtx, pData->sPrefix, 0) ? XTE_FLOW_OK : XTE_FLOW_ERROR;
}


// 内部函数：__Test_Template_LiteralRender
static XTE_Flow __Test_Template_LiteralRender(XTE_StmtRenderCtx* pCtx)
{
	if ( pCtx->sRawBody == NULL ) {
		return XTE_FLOW_OK;
	}
	return xteStmtWrite(pCtx, pCtx->sRawBody, pCtx->iRawBodySize) ? XTE_FLOW_OK : XTE_FLOW_ERROR;
}


// 内部函数：__Test_Template_StrictStmtRender
static XTE_Flow __Test_Template_StrictStmtRender(XTE_StmtRenderCtx* pCtx)
{
	char* sText = xteStmtRequireNamedTextStrict(pCtx, "text", 0, "test strict stmt text must stay text");
	xbuffer_struct tBuf = { 0 };
	char sCount[64] = { 0 };
	int64 iCount = 0;
	int bEnabled = 0;

	if ( sText == NULL ) {
		return XTE_FLOW_ERROR;
	}
	if ( !xteStmtRequireNamedIntStrict(pCtx, "count", 0, &iCount, "test strict stmt count must stay int") ) {
		xrtFree(sText);
		return XTE_FLOW_ERROR;
	}
	if ( !xteStmtRequireNamedBoolStrict(pCtx, "enabled", 0, &bEnabled, "test strict stmt enabled must stay bool") ) {
		xrtFree(sText);
		return XTE_FLOW_ERROR;
	}

	snprintf(sCount, sizeof(sCount), "%lld", (long long)iCount);
	xrtBufferInit(&tBuf, 0);
	if ( !xrtBufferAppend(&tBuf, sText, 0, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, "|", 1, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, sCount, 0, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, "|", 1, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, bEnabled ? "true" : "false", bEnabled ? 4 : 5, XBUF_ANSI) ) {
		xrtFree(sText);
		xrtBufferUnit(&tBuf);
		return XTE_FLOW_ERROR;
	}

	xrtFree(sText);
	if ( !xteStmtWrite(pCtx, tBuf.Buffer, tBuf.Length) ) {
		xrtBufferUnit(&tBuf);
		return XTE_FLOW_ERROR;
	}
	xrtBufferUnit(&tBuf);
	return XTE_FLOW_OK;
}


// 内部函数：__Test_Template_StrictStmtPosRender
static XTE_Flow __Test_Template_StrictStmtPosRender(XTE_StmtRenderCtx* pCtx)
{
	char* sText = xteStmtRequireTextStrict(pCtx, 0, "test strict stmt pos text must stay text");
	xbuffer_struct tBuf = { 0 };
	char sCount[64] = { 0 };
	int64 iCount = 0;
	int bEnabled = 0;

	if ( sText == NULL ) {
		return XTE_FLOW_ERROR;
	}
	if ( !xteStmtRequireIntStrict(pCtx, 1, &iCount, "test strict stmt pos count must stay int") ) {
		xrtFree(sText);
		return XTE_FLOW_ERROR;
	}
	if ( !xteStmtRequireBoolStrict(pCtx, 2, &bEnabled, "test strict stmt pos enabled must stay bool") ) {
		xrtFree(sText);
		return XTE_FLOW_ERROR;
	}

	snprintf(sCount, sizeof(sCount), "%lld", (long long)iCount);
	xrtBufferInit(&tBuf, 0);
	if ( !xrtBufferAppend(&tBuf, sText, 0, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, "/", 1, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, sCount, 0, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, "/", 1, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, bEnabled ? "true" : "false", bEnabled ? 4 : 5, XBUF_ANSI) ) {
		xrtFree(sText);
		xrtBufferUnit(&tBuf);
		return XTE_FLOW_ERROR;
	}

	xrtFree(sText);
	if ( !xteStmtWrite(pCtx, tBuf.Buffer, tBuf.Length) ) {
		xrtBufferUnit(&tBuf);
		return XTE_FLOW_ERROR;
	}
	xrtBufferUnit(&tBuf);
	return XTE_FLOW_OK;
}


// 内部函数：__Test_Template_FailRender
static XTE_Flow __Test_Template_FailRender(XTE_StmtRenderCtx* pCtx)
{
	return xteStmtSetError(pCtx, XTE_ERROR_RENDER, "test statement render failed");
}


// 内部函数：__Test_Template_ConcatCall
static int __Test_Template_ConcatCall(XTE_FuncCtx* pCtx, xvalue* ppRet)
{
	xbuffer_struct tBuf = { 0 };
	uint32 i = 0;

	xrtBufferInit(&tBuf, 0);
	for ( i = 0; i < xteArgCount(pCtx->pArgs); i++ ) {
		char* sText = xteEvalArgText(pCtx->pRender, xteArgAt(pCtx->pArgs, i));

		if ( (sText == NULL) || !xrtBufferAppend(&tBuf, sText, 0, XBUF_ANSI) ) {
			xrtFree(sText);
			xrtBufferUnit(&tBuf);
			return 0;
		}
		xrtFree(sText);
	}

	ppRet[0] = xvoCreateText(tBuf.Buffer, tBuf.Length, FALSE);
	xrtBufferUnit(&tBuf);
	return (ppRet[0] != NULL);
}


// 内部函数：__Test_Template_SurroundCall
static int __Test_Template_SurroundCall(XTE_FuncCtx* pCtx, xvalue* ppRet)
{
	const XTE_ArgItem* pValueArg = xteFuncRequireNamedArg(pCtx, "value", 0, "test surround value is required");
	const XTE_ArgItem* pPrefixArg = xteFindNamedArg(pCtx->pArgs, "prefix", 0);
	const XTE_ArgItem* pSuffixArg = xteFindNamedArg(pCtx->pArgs, "suffix", 0);
	char* sValue = NULL;
	char* sPrefix = NULL;
	char* sSuffix = NULL;
	xbuffer_struct tBuf = { 0 };

	if ( pValueArg == NULL ) {
		return 0;
	}
	sValue = pValueArg ? xteEvalArgText(pCtx->pRender, pValueArg) : (char*)xrtCopyStr("", 0);
	sPrefix = pPrefixArg ? xteEvalArgText(pCtx->pRender, pPrefixArg) : (char*)xrtCopyStr("", 0);
	sSuffix = pSuffixArg ? xteEvalArgText(pCtx->pRender, pSuffixArg) : (char*)xrtCopyStr("", 0);

	if ( (sValue == NULL) || (sPrefix == NULL) || (sSuffix == NULL) ) {
		xrtFree(sValue);
		xrtFree(sPrefix);
		xrtFree(sSuffix);
		return 0;
	}

	xrtBufferInit(&tBuf, 0);
	if ( !xrtBufferAppend(&tBuf, sPrefix, 0, XBUF_ANSI) || !xrtBufferAppend(&tBuf, sValue, 0, XBUF_ANSI) || !xrtBufferAppend(&tBuf, sSuffix, 0, XBUF_ANSI) ) {
		xrtFree(sValue);
		xrtFree(sPrefix);
		xrtFree(sSuffix);
		xrtBufferUnit(&tBuf);
		return 0;
	}

	xrtFree(sValue);
	xrtFree(sPrefix);
	xrtFree(sSuffix);
	ppRet[0] = xvoCreateText(tBuf.Buffer, tBuf.Length, FALSE);
	xrtBufferUnit(&tBuf);
	return (ppRet[0] != NULL);
}


// 内部函数：__Test_Template_RequireCall
static int __Test_Template_RequireCall(XTE_FuncCtx* pCtx, xvalue* ppRet)
{
	const XTE_ArgItem* pValueArg = xteFuncRequireNamedArg(pCtx, "value", 0, "test function value is required");
	xvalue pValue = NULL;

	if ( pValueArg == NULL ) {
		return 0;
	}

	pValue = xteEvalArgValue(pCtx->pRender, pValueArg);
	if ( (pValue == NULL) || (pValue->Type == XVO_DT_NULL) ) {
		xvoUnref(pValue);
		return xteFuncSetError(pCtx, XTE_ERROR_RENDER, "test function value is required");
	}

	ppRet[0] = pValue;
	return 1;
}


// 内部函数：__Test_Template_StrictTypesCall
static int __Test_Template_StrictTypesCall(XTE_FuncCtx* pCtx, xvalue* ppRet)
{
	char* sText = xteFuncRequireNamedTextStrict(pCtx, "text", 0, "test strict text must stay text");
	xbuffer_struct tBuf = { 0 };
	char sCount[64] = { 0 };
	int64 iCount = 0;
	int bEnabled = 0;

	if ( sText == NULL ) {
		return 0;
	}
	if ( !xteFuncRequireNamedIntStrict(pCtx, "count", 0, &iCount, "test strict count must stay int") ) {
		xrtFree(sText);
		return 0;
	}
	if ( !xteFuncRequireNamedBoolStrict(pCtx, "enabled", 0, &bEnabled, "test strict enabled must stay bool") ) {
		xrtFree(sText);
		return 0;
	}

	snprintf(sCount, sizeof(sCount), "%lld", (long long)iCount);
	xrtBufferInit(&tBuf, 0);
	if ( !xrtBufferAppend(&tBuf, sText, 0, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, "|", 1, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, sCount, 0, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, "|", 1, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, bEnabled ? "true" : "false", bEnabled ? 4 : 5, XBUF_ANSI) ) {
		xrtFree(sText);
		xrtBufferUnit(&tBuf);
		return 0;
	}

	xrtFree(sText);
	ppRet[0] = xvoCreateText(tBuf.Buffer, tBuf.Length, FALSE);
	xrtBufferUnit(&tBuf);
	return (ppRet[0] != NULL);
}


// 内部函数：__Test_Template_StrictPosCall
static int __Test_Template_StrictPosCall(XTE_FuncCtx* pCtx, xvalue* ppRet)
{
	char* sText = xteFuncRequireTextStrict(pCtx, 0, "test strict pos text must stay text");
	xbuffer_struct tBuf = { 0 };
	char sCount[64] = { 0 };
	int64 iCount = 0;
	int bEnabled = 0;

	if ( sText == NULL ) {
		return 0;
	}
	if ( !xteFuncRequireIntStrict(pCtx, 1, &iCount, "test strict pos count must stay int") ) {
		xrtFree(sText);
		return 0;
	}
	if ( !xteFuncRequireBoolStrict(pCtx, 2, &bEnabled, "test strict pos enabled must stay bool") ) {
		xrtFree(sText);
		return 0;
	}

	snprintf(sCount, sizeof(sCount), "%lld", (long long)iCount);
	xrtBufferInit(&tBuf, 0);
	if ( !xrtBufferAppend(&tBuf, sText, 0, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, "/", 1, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, sCount, 0, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, "/", 1, XBUF_ANSI)
		|| !xrtBufferAppend(&tBuf, bEnabled ? "true" : "false", bEnabled ? 4 : 5, XBUF_ANSI) ) {
		xrtFree(sText);
		xrtBufferUnit(&tBuf);
		return 0;
	}

	xrtFree(sText);
	ppRet[0] = xvoCreateText(tBuf.Buffer, tBuf.Length, FALSE);
	xrtBufferUnit(&tBuf);
	return (ppRet[0] != NULL);
}


// 内部函数：__Test_Template_Register
static void __Test_Template_Register(xteengine hEngine)
{
	static const XTE_StatementDef tUpper = {
		.sName = "upper",
		.iFlags = XTE_STMT_INLINE,
		.iMinArgs = 1,
		.iMaxArgs = 1,
		.procRender = __Test_Template_UpperRender
	};
	static const XTE_StatementDef tWrap = {
		.sName = "wrap",
		.iFlags = XTE_STMT_BLOCK | XTE_STMT_ALLOW_NAMED_ARGS,
		.iMinArgs = 0,
		.iMaxArgs = 4,
		.procRender = __Test_Template_WrapRender
	};
	static const XTE_StatementDef tRepeat = {
		.sName = "repeat",
		.iFlags = XTE_STMT_HYBRID,
		.iMinArgs = 1,
		.iMaxArgs = 2,
		.procRender = __Test_Template_RepeatRender
	};
	static const XTE_StatementDef tWith = {
		.sName = "with",
		.iFlags = XTE_STMT_BLOCK,
		.iMinArgs = 1,
		.iMaxArgs = 1,
		.procRender = __Test_Template_WithRender
	};
	static const XTE_StatementDef tPrefix = {
		.sName = "prefix",
		.iFlags = XTE_STMT_BLOCK,
		.iMinArgs = 1,
		.iMaxArgs = 1,
		.procParse = __Test_Template_PrefixParse,
		.procRender = __Test_Template_PrefixRender,
		.procFreeData = __Test_Template_PrefixFree
	};
	static const XTE_StatementDef tPathArg = {
		.sName = "patharg",
		.iFlags = XTE_STMT_INLINE | XTE_STMT_ALLOW_NAMED_ARGS,
		.iMinArgs = 1,
		.iMaxArgs = 1,
		.procParse = __Test_Template_PathArgParse,
		.procRender = __Test_Template_PathArgRender,
		.procFreeData = __Test_Template_PrefixFree
	};
	static const XTE_StatementDef tLiteral = {
		.sName = "literal",
		.iFlags = XTE_STMT_BLOCK | XTE_STMT_RAW_BODY,
		.iMinArgs = 0,
		.iMaxArgs = 0,
		.procRender = __Test_Template_LiteralRender
	};
	static const XTE_StatementDef tFail = {
		.sName = "fail",
		.iFlags = XTE_STMT_INLINE,
		.iMinArgs = 0,
		.iMaxArgs = 0,
		.procRender = __Test_Template_FailRender
	};
	static const XTE_StatementDef tStrictStmt = {
		.sName = "strict_stmt",
		.iFlags = XTE_STMT_INLINE | XTE_STMT_ALLOW_NAMED_ARGS,
		.iMinArgs = 3,
		.iMaxArgs = 6,
		.procRender = __Test_Template_StrictStmtRender
	};
	static const XTE_StatementDef tStrictStmtPos = {
		.sName = "strict_stmt_pos",
		.iFlags = XTE_STMT_INLINE,
		.iMinArgs = 3,
		.iMaxArgs = 3,
		.procRender = __Test_Template_StrictStmtPosRender
	};
	static const XTE_StatementDef tParseFail = {
		.sName = "parsefail",
		.iFlags = XTE_STMT_INLINE,
		.iMinArgs = 0,
		.iMaxArgs = 0,
		.procParse = __Test_Template_ParseFailParse
	};
	static const XTE_FunctionDef tConcat = {
		.sName = "concat",
		.iMinArgs = 1,
		.iMaxArgs = 8,
		.procCall = __Test_Template_ConcatCall
	};
	static const XTE_FunctionDef tSurround = {
		.sName = "surround",
		.iMinArgs = 1,
		.iMaxArgs = 8,
		.procCall = __Test_Template_SurroundCall
	};
	static const XTE_FunctionDef tRequire = {
		.sName = "require",
		.iMinArgs = 0,
		.iMaxArgs = 4,
		.procCall = __Test_Template_RequireCall
	};
	static const XTE_FunctionDef tStrictTypes = {
		.sName = "strict_types",
		.iMinArgs = 3,
		.iMaxArgs = 6,
		.procCall = __Test_Template_StrictTypesCall
	};
	static const XTE_FunctionDef tStrictPos = {
		.sName = "strict_pos",
		.iMinArgs = 3,
		.iMaxArgs = 3,
		.procCall = __Test_Template_StrictPosCall
	};

	xteRegisterStatement(hEngine, &tUpper);
	xteRegisterStatement(hEngine, &tWrap);
	xteRegisterStatement(hEngine, &tRepeat);
	xteRegisterStatement(hEngine, &tWith);
	xteRegisterStatement(hEngine, &tPrefix);
	xteRegisterStatement(hEngine, &tPathArg);
	xteRegisterStatement(hEngine, &tLiteral);
	xteRegisterStatement(hEngine, &tFail);
	xteRegisterStatement(hEngine, &tStrictStmt);
	xteRegisterStatement(hEngine, &tStrictStmtPos);
	xteRegisterStatement(hEngine, &tParseFail);
	xteRegisterFunction(hEngine, &tConcat);
	xteRegisterFunction(hEngine, &tSurround);
	xteRegisterFunction(hEngine, &tRequire);
	xteRegisterFunction(hEngine, &tStrictTypes);
	xteRegisterFunction(hEngine, &tStrictPos);
}


// Template 库测试
void Test_Template(xrtGlobalData* xCore)
{
	xteengine hEngine = NULL;
	xteengine hLoadEngine = NULL;
	xtetemplate hTemplate = NULL;
	xvalue tblData = NULL;
	xvalue tblUser = NULL;
	xvalue tblUsers = NULL;
	xvalue tblPairs = NULL;
	char* sResult = NULL;
	size_t iRetSize = 0;
	XTE_ParseOptions tParseOptions = { 0 };
	XTE_Error tError = { 0 };

	(void)xCore;

	printf("\n\n\n------------------------------------\n\n Template 库测试 :\n\n");

	hEngine = xteCreateEngine();
	__Test_Template_Register(hEngine);

	tblData = xvoCreateTable();
	tblUser = xvoCreateTable();
	tblUsers = xvoCreateArray();
	tblPairs = xvoCreateTable();
	xvoTableSetText(tblUser, "name", 0, "Alice", 0, FALSE);
	xvoTableSetText(tblUser, "city", 0, "beijing", 0, FALSE);
	xvoTableSetValue(tblData, "user", 0, tblUser, TRUE);
	{
		xvalue tblUser1 = xvoCreateTable();
		xvalue tblUser2 = xvoCreateTable();
		xvalue tblUser3 = xvoCreateTable();

		xvoTableSetText(tblUser1, "name", 0, "Alice", 0, FALSE);
		xvoTableSetBool(tblUser1, "skip", 0, FALSE);
		xvoTableSetBool(tblUser1, "stop", 0, FALSE);
		xvoArrayAppendValue(tblUsers, tblUser1, TRUE);

		xvoTableSetText(tblUser2, "name", 0, "Bob", 0, FALSE);
		xvoTableSetBool(tblUser2, "skip", 0, FALSE);
		xvoTableSetBool(tblUser2, "stop", 0, TRUE);
		xvoArrayAppendValue(tblUsers, tblUser2, TRUE);

		xvoTableSetText(tblUser3, "name", 0, "Cat", 0, FALSE);
		xvoTableSetBool(tblUser3, "skip", 0, TRUE);
		xvoTableSetBool(tblUser3, "stop", 0, FALSE);
		xvoArrayAppendValue(tblUsers, tblUser3, TRUE);
	}
	xvoTableSetValue(tblData, "users", 0, tblUsers, TRUE);
	xvoTableSetText(tblPairs, "first", 0, "A", 0, FALSE);
	xvoTableSetText(tblPairs, "second", 0, "B", 0, FALSE);
	xvoTableSetValue(tblData, "pairs", 0, tblPairs, TRUE);
	xvoTableSetInt(tblData, "count", 0, 1234567);
	xvoTableSetBool(tblData, "active", 0, TRUE);
	xvoTableSetTimeSerial(tblData, "created", 0, 2026, 3, 24, 11, 22, 33);

	printf("--- 基础渲染与访问器 ---\n");
	hTemplate = xteParseEx(hEngine,
		"Hello {$user.name}! {?active:ON:OFF} {%count:,} {#upper:user.city}",
		0,
		NULL,
		NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("parse template", hTemplate != NULL);
	__Test_Template_PrintResult("render basic output", (sResult != NULL) && (strcmp(sResult, "Hello Alice! ON 1,234,567 BEIJING") == 0));
	__Test_Template_PrintResult("node count", xteTemplateGetNodeCount(hTemplate) > 0u);
	__Test_Template_PrintResult("expr count", xteTemplateGetExprCount(hTemplate) > 0u);
	__Test_Template_PrintResult("arg count", xteTemplateGetArgCount(hTemplate) > 0u);
	__Test_Template_PrintResult("root span", xteTemplateGetRootSpan(hTemplate).iCount > 0u);
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	printf("\n--- 自定义定界符 ---\n");
	tParseOptions.sBracket = "<>";
	hTemplate = xteParseEx(hEngine, "<$user.name>", 0, &tParseOptions, NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("bracket <>", (sResult != NULL) && (strcmp(sResult, "Alice") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	tParseOptions.sBracket = "{{}}";
	hTemplate = xteParseEx(hEngine, "{{$user.name}}", 0, &tParseOptions, NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("bracket {{}}", (sResult != NULL) && (strcmp(sResult, "Alice") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	tParseOptions.sBracket = "(())";
	hTemplate = xteParseEx(hEngine, "(($user.name))", 0, &tParseOptions, NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("bracket (())", (sResult != NULL) && (strcmp(sResult, "Alice") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	memset(&tParseOptions, 0, sizeof(tParseOptions));

	printf("\n--- 路径解析 ---\n");
	__Test_Template_PrintResult("resolve user.name",
		strcmp(xvoGetText(xteResolvePath("user.name", 0, tblData, tblData, NULL, NULL)), "Alice") == 0);
	__Test_Template_PrintResult("resolve items missing",
		xteResolvePath("user.missing", 0, tblData, tblData, NULL, NULL)->Type == XVO_DT_NULL);

	printf("\n--- 内建语句 ---\n");
	hTemplate = xteParseEx(hEngine,
		"{#if:active}ON{#else}OFF{#end}|"
		"{#if:false}A{#elseif:active}B{#else}C{#end}|"
		"{#for:1:3:1}{%__index__}{#end}|"
		"{#foreach:users}{#if:skip}{#continue}{#end}{$name}|{#if:stop}{#break}{#end}{#end}",
		0,
		NULL,
		NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("render builtin statements",
		(sResult != NULL) && (strcmp(sResult, "ON|B|123|Alice|Bob|") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	printf("\n--- 循环边界 ---\n");
	hTemplate = xteParseEx(hEngine,
		"{#for:3:1:-1}{%__index__}{#end}|{#foreach:pairs}{$__key__}={$__value__};{#end}",
		0,
		NULL,
		NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("render loop edges",
		(sResult != NULL)
		&& (strncmp(sResult, "321|", 4) == 0)
		&& (strstr(sResult, "first=A;") != NULL)
		&& (strstr(sResult, "second=B;") != NULL));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	printf("\n--- 条件表达式 ---\n");
	hTemplate = xteParseEx(hEngine,
		"{#if:(count >= 1000000) and active}A{#else}B{#end}|"
		"{#if:not false}C{#else}D{#end}|"
		"{?user.name = 'Alice':OK:NO}|"
		"{#if:(count < 10) or (user.name = 'Alice')}E{#else}F{#end}",
		0,
		NULL,
		NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("render bool expressions",
		(sResult != NULL) && (strcmp(sResult, "A|C|OK|E") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	printf("\n--- Include ---\n");
	{
		xdict tblInclude = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
		xtetemplate hCard = xteParseEx(hEngine, "[{$user.name}]", 0, NULL, NULL);

		xrtDictSetPtr(tblInclude, "card", 0, hCard, NULL);
		hTemplate = xteParseEx(hEngine, "X{#include:'card'}Y", 0, NULL, NULL);
		sResult = xteMake(hTemplate, tblData, NULL, tblInclude, &iRetSize);
		__Test_Template_PrintResult("render include",
			(sResult != NULL) && (strcmp(sResult, "X[Alice]Y") == 0));
		xrtFree(sResult);
		xteDestroyTemplate(hTemplate);
		xteDestroyTemplate(hCard);
		xrtDictDestroy(tblInclude);
	}

	printf("\n--- Define / 子模板 ---\n");
	hTemplate = xteParseEx(hEngine,
		"A{#define:'card'}[{$user.name}]{#end}B{#include:'card'}C",
		0,
		NULL,
		NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("render local define",
		(sResult != NULL) && (strcmp(sResult, "AB[Alice]C") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	{
		xdict tblInclude = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
		xtetemplate hCard = xteParseEx(hEngine, "[EXT]", 0, NULL, NULL);

		xrtDictSetPtr(tblInclude, "card", 0, hCard, NULL);
		hTemplate = xteParseEx(hEngine,
			"A{#define:'card'}[LOCAL]{#end}B{#include:'card'}C",
			0,
			NULL,
			NULL);
		sResult = xteMake(hTemplate, tblData, NULL, tblInclude, &iRetSize);
		__Test_Template_PrintResult("render local define before include map",
			(sResult != NULL) && (strcmp(sResult, "AB[LOCAL]C") == 0));
		xrtFree(sResult);
		xteDestroyTemplate(hTemplate);
		xteDestroyTemplate(hCard);
		xrtDictDestroy(tblInclude);
	}

	printf("\n--- 函数调用 ---\n");
	hTemplate = xteParseEx(hEngine,
		"{@concat:'Hi ':user.name}|{@surround:value=user.name:prefix='[':suffix=']'}|{@strict_types:text=user.name:count=count:enabled=active}|{@strict_pos:user.name:count:active}",
		0,
		NULL,
		NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("render functions",
		(sResult != NULL) && (strcmp(sResult, "Hi Alice|[Alice]|Alice|1234567|true|Alice/1234567/true") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	printf("\n--- 语句 strict helper ---\n");
	hTemplate = xteParseEx(hEngine,
		"{#strict_stmt:text=user.name:count=count:enabled=active}{#strict_stmt_pos:user.name:count:active}",
		0,
		NULL,
		NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("render strict statements",
		(sResult != NULL) && (strcmp(sResult, "Alice|1234567|trueAlice/1234567/true") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	printf("\n--- 错误路径 ---\n");
	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{@missing}", 0, NULL, &tError);
	__Test_Template_PrintResult("parse missing function",
		(hTemplate == NULL) && (tError.iCode == XTE_ERROR_PARSE) && (strcmp(tError.sDesc, "template function is not registered") == 0));

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{#parsefail}", 0, NULL, &tError);
	__Test_Template_PrintResult("parse callback error",
		(hTemplate == NULL) && (tError.iCode == XTE_ERROR_PARSE) && (strcmp(tError.sDesc, "test parse callback failed") == 0));

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{#prefix:user.name}X{#end}", 0, NULL, &tError);
	__Test_Template_PrintResult("parse helper expr type",
		(hTemplate == NULL) && (tError.iCode == XTE_ERROR_PARSE) && (strcmp(tError.sDesc, "test prefix requires text literal") == 0));

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{#patharg:value='user.name'}", 0, NULL, &tError);
	__Test_Template_PrintResult("parse helper named expr type",
		(hTemplate == NULL) && (tError.iCode == XTE_ERROR_PARSE) && (strcmp(tError.sDesc, "test patharg requires named path") == 0));

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{#define:'card'}A{#end}{#define:'card'}B{#end}", 0, NULL, &tError);
	__Test_Template_PrintResult("parse duplicate define",
		(hTemplate == NULL) && (tError.iCode == XTE_ERROR_PARSE) && (strcmp(tError.sDesc, "template define name is duplicated") == 0));

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{#for:1:3:0}X{#end}", 0, NULL, NULL);
	sResult = __Test_Template_RenderWithError(hTemplate, tblData, NULL, NULL, &iRetSize, &tError);
	__Test_Template_PrintResult("for step zero error",
		(sResult == NULL) && (tError.iCode == XTE_ERROR_RENDER) && (strcmp(tError.sDesc, "template for step cannot be zero") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{#for:0:100001:1}X{#end}", 0, NULL, NULL);
	sResult = __Test_Template_RenderWithError(hTemplate, tblData, NULL, NULL, &iRetSize, &tError);
	__Test_Template_PrintResult("for max iteration error",
		(sResult == NULL) && (tError.iCode == XTE_ERROR_RENDER) && (strcmp(tError.sDesc, "template for exceeded max iterations") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{#break}", 0, NULL, NULL);
	sResult = __Test_Template_RenderWithError(hTemplate, tblData, NULL, NULL, &iRetSize, &tError);
	__Test_Template_PrintResult("break outside loop error",
		(sResult == NULL) && (tError.iCode == XTE_ERROR_RENDER) && (strcmp(tError.sDesc, "template break must be inside loop") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{#continue}", 0, NULL, NULL);
	sResult = __Test_Template_RenderWithError(hTemplate, tblData, NULL, NULL, &iRetSize, &tError);
	__Test_Template_PrintResult("continue outside loop error",
		(sResult == NULL) && (tError.iCode == XTE_ERROR_RENDER) && (strcmp(tError.sDesc, "template continue must be inside loop") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{@require:value=user.missing}", 0, NULL, NULL);
	sResult = __Test_Template_RenderWithError(hTemplate, tblData, NULL, NULL, &iRetSize, &tError);
	__Test_Template_PrintResult("function callback error",
		(sResult == NULL) && (tError.iCode == XTE_ERROR_RENDER) && (strcmp(tError.sDesc, "test function value is required") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{@strict_types:text=count:count=count:enabled=active}", 0, NULL, NULL);
	sResult = __Test_Template_RenderWithError(hTemplate, tblData, NULL, NULL, &iRetSize, &tError);
	__Test_Template_PrintResult("strict text type error",
		(sResult == NULL) && (tError.iCode == XTE_ERROR_RENDER) && (strcmp(tError.sDesc, "test strict text must stay text") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{@strict_types:text=user.name:count='3':enabled=active}", 0, NULL, NULL);
	sResult = __Test_Template_RenderWithError(hTemplate, tblData, NULL, NULL, &iRetSize, &tError);
	__Test_Template_PrintResult("strict int type error",
		(sResult == NULL) && (tError.iCode == XTE_ERROR_RENDER) && (strcmp(tError.sDesc, "test strict count must stay int") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{@strict_types:text=user.name:count=count:enabled='true'}", 0, NULL, NULL);
	sResult = __Test_Template_RenderWithError(hTemplate, tblData, NULL, NULL, &iRetSize, &tError);
	__Test_Template_PrintResult("strict bool type error",
		(sResult == NULL) && (tError.iCode == XTE_ERROR_RENDER) && (strcmp(tError.sDesc, "test strict enabled must stay bool") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{@strict_pos:user.name:'3':active}", 0, NULL, NULL);
	sResult = __Test_Template_RenderWithError(hTemplate, tblData, NULL, NULL, &iRetSize, &tError);
	__Test_Template_PrintResult("strict pos int type error",
		(sResult == NULL) && (tError.iCode == XTE_ERROR_RENDER) && (strcmp(tError.sDesc, "test strict pos count must stay int") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{#strict_stmt:text=count:count=count:enabled=active}", 0, NULL, NULL);
	sResult = __Test_Template_RenderWithError(hTemplate, tblData, NULL, NULL, &iRetSize, &tError);
	__Test_Template_PrintResult("strict stmt text type error",
		(sResult == NULL) && (tError.iCode == XTE_ERROR_RENDER) && (strcmp(tError.sDesc, "test strict stmt text must stay text") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{#strict_stmt_pos:user.name:'3':active}", 0, NULL, NULL);
	sResult = __Test_Template_RenderWithError(hTemplate, tblData, NULL, NULL, &iRetSize, &tError);
	__Test_Template_PrintResult("strict stmt pos int type error",
		(sResult == NULL) && (tError.iCode == XTE_ERROR_RENDER) && (strcmp(tError.sDesc, "test strict stmt pos count must stay int") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{#fail}", 0, NULL, NULL);
	sResult = __Test_Template_RenderWithError(hTemplate, tblData, NULL, NULL, &iRetSize, &tError);
	__Test_Template_PrintResult("statement callback error",
		(sResult == NULL) && (tError.iCode == XTE_ERROR_RENDER) && (strcmp(tError.sDesc, "test statement render failed") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	memset(&tError, 0, sizeof(tError));
	hTemplate = xteParseEx(hEngine, "{#repeat:2}", 0, NULL, NULL);
	sResult = __Test_Template_RenderWithError(hTemplate, tblData, NULL, NULL, &iRetSize, &tError);
	__Test_Template_PrintResult("statement require arg error",
		(sResult == NULL) && (tError.iCode == XTE_ERROR_RENDER) && (strcmp(tError.sDesc, "test repeat inline text is required") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	printf("\n--- 自定义块 / 混合块 ---\n");
	hTemplate = xteParseEx(hEngine,
		"{#prefix:'Hi '}{#with:user}{$name}{#end}{#end}|{#patharg:value=user.name}|{#wrap:tag='section'}Body{#end}|{#repeat:2}Z{#end}",
		0,
		NULL,
		NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("render custom statements",
		(sResult != NULL) && (strcmp(sResult, "Hi Alice|user.name|<section>Body</section>|ZZ") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	hTemplate = xteParseEx(hEngine,
		"{#repeat:3:'ha'}",
		0,
		NULL,
		NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("render hybrid inline",
		(sResult != NULL) && (strcmp(sResult, "hahaha") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	printf("\n--- RAW_BODY ---\n");
	hTemplate = xteParseEx(hEngine,
		"{#literal}{$user.name}{#end}",
		0,
		NULL,
		NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("render raw body",
		(sResult != NULL) && (strcmp(sResult, "{$user.name}") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	printf("\n--- SCRIPT ---\n");
	hTemplate = xteParseEx(hEngine,
		"{#script}{#if:active}{$user.name}{#end}{#end}",
		0,
		NULL,
		NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("render script raw body",
		(sResult != NULL) && (strcmp(sResult, "{#if:active}{$user.name}{#end}") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	printf("\n--- 并发渲染 ---\n");
	{
		__Test_Template_ConcurrentCtx tCtxA = { 0 };
		__Test_Template_ConcurrentCtx tCtxB = { 0 };
		xthread pThreadA = NULL;
		xthread pThreadB = NULL;

		hTemplate = xteParseEx(hEngine,
			"{#define:'card'}[{$user.name}]{#end}{#include:'card'}",
			0,
			NULL,
			NULL);

		tCtxA.hTemplate = hTemplate;
		tCtxA.sName = "Alice";
		tCtxB.hTemplate = hTemplate;
		tCtxB.sName = "Bob";

		pThreadA = xrtThreadCreate(__Test_Template_ConcurrentRenderProc, &tCtxA, 0);
		pThreadB = xrtThreadCreate(__Test_Template_ConcurrentRenderProc, &tCtxB, 0);
		if ( pThreadA ) {
			xrtThreadWait(pThreadA);
		}
		if ( pThreadB ) {
			xrtThreadWait(pThreadB);
		}

		__Test_Template_PrintResult("concurrent render",
			(pThreadA != NULL)
			&& (pThreadB != NULL)
			&& (xrtThreadGetExitCode(pThreadA) == 0u)
			&& (xrtThreadGetExitCode(pThreadB) == 0u)
			&& (tCtxA.sResult != NULL)
			&& (tCtxB.sResult != NULL)
			&& (strcmp(tCtxA.sResult, "[Alice]") == 0)
			&& (strcmp(tCtxB.sResult, "[Bob]") == 0));

		xrtFree(tCtxA.sResult);
		xrtFree(tCtxB.sResult);
		if ( pThreadA ) {
			xrtThreadDestroy(pThreadA);
		}
		if ( pThreadB ) {
			xrtThreadDestroy(pThreadB);
		}
		xteDestroyTemplate(hTemplate);
	}

	#ifdef XTE_ENABLE_FILE
	printf("\n--- 预编译文件 ---\n");
	hTemplate = xteParseEx(hEngine,
		"File {#define:'card'}[{$user.name}]{#end}{#include:'card'}|{#prefix:'Hi '}{#with:user}{$name}{#end}{#end}|{#patharg:value=user.name}|{#if:(count >= 1000000) and active}YES{#else}NO{#end}|{@surround:value=user.name:prefix='[':suffix=']'}",
		0,
		NULL,
		NULL);
	__Test_Template_PrintResult("save compiled template",
		xteTemplateSaveFile(hTemplate, "test_template_cache.xteb", 0, NULL));
	xteDestroyTemplate(hTemplate);
	hTemplate = xteTemplateLoadFile(hEngine, "test_template_cache.xteb", 0, NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("load compiled template",
		(sResult != NULL) && (strcmp(sResult, "File [Alice]|Hi Alice|user.name|YES|[Alice]") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	memset(&tError, 0, sizeof(tError));
	hLoadEngine = xteCreateEngine();
	hTemplate = xteTemplateLoadFile(hLoadEngine, "test_template_cache.xteb", 0, &tError);
	__Test_Template_PrintResult("load compiled template missing statement",
		(hTemplate == NULL) && (tError.iCode == XTE_ERROR_UNKNOWN_STATEMENT) && (strcmp(tError.sDesc, "template statement is not registered") == 0));
	xteDestroyTemplate(hTemplate);
	xteDestroyEngine(hLoadEngine);
	hLoadEngine = NULL;
	xrtFileDelete("test_template_cache.xteb");
	#endif

	#ifdef XTE_DEBUGMODE
	printf("\n--- DEBUG DUMP ---\n");
	hTemplate = xteParseEx(hEngine, "{#wrap}X{#end}", 0, NULL, NULL);
	__Test_Template_PrintResult("dump console", xteTemplateDumpConsole(hTemplate, 0));
	xteDestroyTemplate(hTemplate);

	hTemplate = xteParseEx(hEngine,
		"{$user.name}{?true:Y:N}{@surround:value=user.name:prefix='[':suffix=']':count=3}{#wrap:tag='section':enabled=true}X{#end}",
		0,
		NULL,
		NULL);
	sResult = __Test_Template_DumpToText(hTemplate);
	__Test_Template_PrintResult("dump args",
		(sResult != NULL)
		&& (strstr(sResult, "OUTPUT(PATH): user.name\n") != NULL)
		&& (strstr(sResult, "INLINE_BOOL(BOOL): true\n") != NULL)
		&& (strstr(sResult, "OUTPUT_FUNC: surround\n") != NULL)
		&& (strstr(sResult, "\tARG value: user.name\n") != NULL)
		&& (strstr(sResult, "\t\tEXPR PATH: user.name\n") != NULL)
		&& (strstr(sResult, "\tARG prefix: '['\n") != NULL)
		&& (strstr(sResult, "\t\tEXPR TEXT: [\n") != NULL)
		&& (strstr(sResult, "\tARG count: 3\n") != NULL)
		&& (strstr(sResult, "\t\tEXPR INT: 3\n") != NULL)
		&& (strstr(sResult, "STATEMENT: wrap\n") != NULL)
		&& (strstr(sResult, "\tARG tag: 'section'\n") != NULL)
		&& (strstr(sResult, "\tARG enabled: true\n") != NULL)
		&& (strstr(sResult, "\t\tEXPR BOOL: true\n") != NULL));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	#endif

	xvoUnref(tblData);
	xteDestroyEngine(hLoadEngine);
	xteDestroyEngine(hEngine);
	printf("\nTemplate 测试完成\n");
}
