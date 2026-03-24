static void __Test_Template_PrintResult(const char* sLabel, int bPass)
{
	printf("  %s : %s\n", sLabel, bPass ? "PASS" : "FAIL");
}

typedef struct
{
	char* sPrefix;
} __Test_Template_PrefixData;

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

static int __Test_Template_PrefixParse(XTE_StmtParseCtx* pCtx, void** ppData)
{
	const XTE_ArgItem* pArg = xteArgAt(pCtx->pArgs, 0);
	const char* sRaw = pArg ? xteTemplateGetString(pCtx->hTemplate, pArg->iRawOff) : NULL;
	__Test_Template_PrefixData* pData = xrtCalloc(1, sizeof(*pData));

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

static void __Test_Template_PrefixFree(void* pData)
{
	__Test_Template_PrefixData* pPrefix = (__Test_Template_PrefixData*)pData;

	if ( pPrefix == NULL ) {
		return;
	}

	xrtFree(pPrefix->sPrefix);
	xrtFree(pPrefix);
}

static XTE_Flow __Test_Template_UpperRender(XTE_StmtRenderCtx* pCtx)
{
	char* sText = xteEvalArgText(pCtx->pRender, xteArgAt(pCtx->pArgs, 0));
	char* sUpper = xrtUCase(sText, 0, FALSE);
	XTE_Flow iRet = XTE_FLOW_ERROR;

	if ( sUpper && xteStmtWrite(pCtx, sUpper, 0) ) {
		iRet = XTE_FLOW_OK;
	}

	xrtFree(sUpper);
	xrtFree(sText);
	return iRet;
}

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

static XTE_Flow __Test_Template_WithRender(XTE_StmtRenderCtx* pCtx)
{
	xvalue pScope = xteEvalArgValue(pCtx->pRender, xteArgAt(pCtx->pArgs, 0));
	int bOk = 0;

	bOk = xteStmtRenderBodyWithScope(pCtx, NULL, pScope);
	xvoUnref(pScope);
	return bOk ? XTE_FLOW_OK : XTE_FLOW_ERROR;
}

static XTE_Flow __Test_Template_RepeatRender(XTE_StmtRenderCtx* pCtx)
{
	int64 iCount = 0;
	uint32 i = 0;

	if ( !xteEvalArgInt(pCtx->pRender, xteArgAt(pCtx->pArgs, 0), &iCount) ) {
		return XTE_FLOW_ERROR;
	}

	for ( i = 0; i < (uint32)((iCount < 0) ? 0 : iCount); i++ ) {
		if ( pCtx->pBody && pCtx->pBody->iCount ) {
			if ( !xteStmtRenderBody(pCtx) ) {
				return XTE_FLOW_ERROR;
			}
		} else {
			char* sText = NULL;

			if ( xteArgCount(pCtx->pArgs) < 2u ) {
				return XTE_FLOW_ERROR;
			}
			sText = xteEvalArgText(pCtx->pRender, xteArgAt(pCtx->pArgs, 1));
			if ( (sText == NULL) || !xteStmtWrite(pCtx, sText, 0) ) {
				xrtFree(sText);
				return XTE_FLOW_ERROR;
			}
			xrtFree(sText);
		}
	}

	return XTE_FLOW_OK;
}

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

static XTE_Flow __Test_Template_LiteralRender(XTE_StmtRenderCtx* pCtx)
{
	if ( pCtx->sRawBody == NULL ) {
		return XTE_FLOW_OK;
	}
	return xteStmtWrite(pCtx, pCtx->sRawBody, pCtx->iRawBodySize) ? XTE_FLOW_OK : XTE_FLOW_ERROR;
}

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

static int __Test_Template_SurroundCall(XTE_FuncCtx* pCtx, xvalue* ppRet)
{
	const XTE_ArgItem* pValueArg = xteFindNamedArg(pCtx->pArgs, "value", 0);
	const XTE_ArgItem* pPrefixArg = xteFindNamedArg(pCtx->pArgs, "prefix", 0);
	const XTE_ArgItem* pSuffixArg = xteFindNamedArg(pCtx->pArgs, "suffix", 0);
	char* sValue = NULL;
	char* sPrefix = NULL;
	char* sSuffix = NULL;
	xbuffer_struct tBuf = { 0 };

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
	static const XTE_StatementDef tLiteral = {
		.sName = "literal",
		.iFlags = XTE_STMT_BLOCK | XTE_STMT_RAW_BODY,
		.iMinArgs = 0,
		.iMaxArgs = 0,
		.procRender = __Test_Template_LiteralRender
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

	xteRegisterStatement(hEngine, &tUpper);
	xteRegisterStatement(hEngine, &tWrap);
	xteRegisterStatement(hEngine, &tRepeat);
	xteRegisterStatement(hEngine, &tWith);
	xteRegisterStatement(hEngine, &tPrefix);
	xteRegisterStatement(hEngine, &tLiteral);
	xteRegisterFunction(hEngine, &tConcat);
	xteRegisterFunction(hEngine, &tSurround);
}


// Template 库测试
void Test_Template(xrtGlobalData* xCore)
{
	xteengine hEngine = NULL;
	xtetemplate hTemplate = NULL;
	xvalue tblData = NULL;
	xvalue tblUser = NULL;
	xvalue tblUsers = NULL;
	char* sResult = NULL;
	size_t iRetSize = 0;
	XTE_ParseOptions tParseOptions = { 0 };

	(void)xCore;

	printf("\n\n\n------------------------------------\n\n Template 库测试 :\n\n");

	hEngine = xteCreateEngine();
	__Test_Template_Register(hEngine);

	tblData = xvoCreateTable();
	tblUser = xvoCreateTable();
	tblUsers = xvoCreateArray();
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

	printf("\n--- 函数调用 ---\n");
	hTemplate = xteParseEx(hEngine,
		"{@concat:'Hi ':user.name}|{@surround:value=user.name:prefix='[':suffix=']'}",
		0,
		NULL,
		NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("render functions",
		(sResult != NULL) && (strcmp(sResult, "Hi Alice|[Alice]") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);

	printf("\n--- 自定义块 / 混合块 ---\n");
	hTemplate = xteParseEx(hEngine,
		"{#prefix:'Hi '}{#with:user}{$name}{#end}{#end}|{#wrap:tag='section'}Body{#end}|{#repeat:2}Z{#end}",
		0,
		NULL,
		NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("render custom statements",
		(sResult != NULL) && (strcmp(sResult, "Hi Alice|<section>Body</section>|ZZ") == 0));
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

	#ifdef XTE_ENABLE_FILE
	printf("\n--- 预编译文件 ---\n");
	hTemplate = xteParseEx(hEngine,
		"File {?user.name = 'Alice':OK:NO}|{#if:(count >= 1000000) and active}YES{#else}NO{#end}|{@surround:value=user.name:prefix='[':suffix=']'}",
		0,
		NULL,
		NULL);
	__Test_Template_PrintResult("save compiled template",
		xteTemplateSaveFile(hTemplate, "test_template_cache.xteb", 0, NULL));
	xteDestroyTemplate(hTemplate);
	hTemplate = xteTemplateLoadFile(hEngine, "test_template_cache.xteb", 0, NULL);
	sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
	__Test_Template_PrintResult("load compiled template",
		(sResult != NULL) && (strcmp(sResult, "File OK|YES|[Alice]") == 0));
	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	xrtFileDelete("test_template_cache.xteb");
	#endif

	#ifdef XTE_DEBUGMODE
	printf("\n--- DEBUG DUMP ---\n");
	hTemplate = xteParseEx(hEngine, "{#wrap}X{#end}", 0, NULL, NULL);
	__Test_Template_PrintResult("dump console", xteTemplateDumpConsole(hTemplate, 0));
	xteDestroyTemplate(hTemplate);
	#endif

	xvoUnref(tblData);
	xteDestroyEngine(hEngine);
	printf("\nTemplate 测试完成\n");
}
