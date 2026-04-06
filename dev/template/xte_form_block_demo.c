#ifndef XRT_INTERNAL_TEST_ENV
	#define XRT_INTERNAL_TEST_ENV
#endif

#include "../../xrt.c"

#include <stdio.h>
#include <string.h>

typedef struct DemoBuf_Struct {
	char* sText;
	size_t iLen;
	size_t iCap;
} DemoBuf;

typedef struct DemoCounters_Struct {
	uint32 iCompileParseCount;
	uint32 iCompileGenerateCount;
	uint32 iCompileRenderCount;
	uint32 iRuntimeParseCount;
	uint32 iRuntimeGenerateCount;
	uint32 iRuntimeRenderCount;
} DemoCounters;

typedef struct DemoCompiledForm_Struct {
	char* sHtml;
	size_t iHtmlSize;
} DemoCompiledForm;

typedef struct DemoRuntimeForm_Struct {
	char* sJson;
	size_t iJsonSize;
} DemoRuntimeForm;

static DemoCounters g_tDemoCounters;

#define DEMO_FORM_ERROR_ALLOC 1001

static void DemoBuf_Init(DemoBuf* pBuf)
{
	memset(pBuf, 0, sizeof(*pBuf));
}

static void DemoBuf_Unit(DemoBuf* pBuf)
{
	if ( pBuf->sText != NULL ) {
		xrtFree(pBuf->sText);
	}
	memset(pBuf, 0, sizeof(*pBuf));
}

static bool DemoBuf_Ensure(DemoBuf* pBuf, size_t iNeedExtra)
{
	size_t iNeed;
	size_t iNewCap;
	char* sNewText;
	if ( pBuf == NULL ) {
		return FALSE;
	}
	iNeed = pBuf->iLen + iNeedExtra + 1u;
	if ( iNeed <= pBuf->iCap ) {
		return TRUE;
	}
	iNewCap = (pBuf->iCap != 0u) ? pBuf->iCap : 256u;
	while ( iNewCap < iNeed ) {
		iNewCap *= 2u;
	}
	sNewText = (char*)xrtRealloc(pBuf->sText, iNewCap);
	if ( sNewText == NULL ) {
		return FALSE;
	}
	pBuf->sText = sNewText;
	pBuf->iCap = iNewCap;
	return TRUE;
}

static bool DemoBuf_AppendN(DemoBuf* pBuf, const char* sText, size_t iSize)
{
	if ( sText == NULL ) {
		return TRUE;
	}
	if ( !DemoBuf_Ensure(pBuf, iSize) ) {
		return FALSE;
	}
	memcpy(pBuf->sText + pBuf->iLen, sText, iSize);
	pBuf->iLen += iSize;
	pBuf->sText[pBuf->iLen] = 0;
	return TRUE;
}

static bool DemoBuf_Append(DemoBuf* pBuf, const char* sText)
{
	if ( sText == NULL ) {
		return TRUE;
	}
	return DemoBuf_AppendN(pBuf, sText, strlen(sText));
}

static bool DemoBuf_AppendChar(DemoBuf* pBuf, char ch)
{
	return DemoBuf_AppendN(pBuf, &ch, 1u);
}

static bool DemoBuf_AppendEscaped(DemoBuf* pBuf, const char* sText)
{
	const unsigned char* p = (const unsigned char*)sText;
	if ( p == NULL ) {
		return TRUE;
	}
	while ( *p != 0u ) {
		switch ( *p ) {
			case '&':
				if ( !DemoBuf_Append(pBuf, "&amp;") ) {
					return FALSE;
				}
				break;
			case '<':
				if ( !DemoBuf_Append(pBuf, "&lt;") ) {
					return FALSE;
				}
				break;
			case '>':
				if ( !DemoBuf_Append(pBuf, "&gt;") ) {
					return FALSE;
				}
				break;
			case '"':
				if ( !DemoBuf_Append(pBuf, "&quot;") ) {
					return FALSE;
				}
				break;
			default:
				if ( !DemoBuf_AppendChar(pBuf, (char)(*p)) ) {
					return FALSE;
				}
				break;
		}
		++p;
	}
	return TRUE;
}

static char* DemoBuf_Detach(DemoBuf* pBuf, size_t* pRetSize)
{
	char* sRet;
	if ( pRetSize != NULL ) {
		*pRetSize = pBuf->iLen;
	}
	sRet = pBuf->sText;
	pBuf->sText = NULL;
	pBuf->iLen = 0u;
	pBuf->iCap = 0u;
	return sRet;
}

static const char* Demo_GetText(xvalue pTbl, const char* sKey, const char* sDefault)
{
	str sText = NULL;
	xvalue pVal = NULL;
	if ( pTbl == NULL ) {
		return sDefault;
	}
	pVal = xvoTableGetValue(pTbl, sKey, 0);
	if ( pVal == NULL || xvoIsNull(pVal) ) {
		return sDefault;
	}
	sText = xvoGetText(pVal);
	return (sText != NULL) ? (const char*)sText : sDefault;
}

static xvalue Demo_GetValue(xvalue pTbl, const char* sKey)
{
	if ( pTbl == NULL ) {
		return NULL;
	}
	return xvoTableGetValue(pTbl, sKey, 0);
}

static bool Demo_AppendOptionLabel(DemoBuf* pBuf, xvalue pOption)
{
	if ( pOption == NULL || xvoIsNull(pOption) ) {
		return DemoBuf_Append(pBuf, "option");
	}
	if ( xvoType(pOption) == XVO_DT_TEXT ) {
		return DemoBuf_AppendEscaped(pBuf, (const char*)xvoGetText(pOption));
	}
	if ( xvoType(pOption) == XVO_DT_TABLE ) {
		const char* sLabel = Demo_GetText(pOption, "label", NULL);
		const char* sValue = Demo_GetText(pOption, "value", NULL);
		if ( sLabel != NULL ) {
			return DemoBuf_AppendEscaped(pBuf, sLabel);
		}
		if ( sValue != NULL ) {
			return DemoBuf_AppendEscaped(pBuf, sValue);
		}
	}
	return DemoBuf_Append(pBuf, "option");
}

static bool Demo_AppendFieldHtml(DemoBuf* pBuf, xvalue pField)
{
	const char* sType = Demo_GetText(pField, "type", "text");
	const char* sName = Demo_GetText(pField, "name", "field");
	const char* sLabel = Demo_GetText(pField, "label", sName);
	xvalue pOptions = NULL;
	uint32 i = 0u;

	if ( !DemoBuf_Append(pBuf, "<div class=\"xte-form-field xte-form-field-") ) {
		return FALSE;
	}
	if ( !DemoBuf_AppendEscaped(pBuf, sType) ) {
		return FALSE;
	}
	if ( !DemoBuf_Append(pBuf, "\"><label>") ) {
		return FALSE;
	}
	if ( !DemoBuf_AppendEscaped(pBuf, sLabel) ) {
		return FALSE;
	}
	if ( !DemoBuf_Append(pBuf, "</label>") ) {
		return FALSE;
	}

	if ( strcmp(sType, "textarea") == 0 ) {
		if ( !DemoBuf_Append(pBuf, "<textarea name=\"") ||
			!DemoBuf_AppendEscaped(pBuf, sName) ||
			!DemoBuf_Append(pBuf, "\"></textarea>") ) {
			return FALSE;
		}
	} else if ( strcmp(sType, "switch") == 0 ) {
		if ( !DemoBuf_Append(pBuf, "<input type=\"checkbox\" name=\"") ||
			!DemoBuf_AppendEscaped(pBuf, sName) ||
			!DemoBuf_Append(pBuf, "\" />") ) {
			return FALSE;
		}
	} else if ( strcmp(sType, "select") == 0 ) {
		pOptions = Demo_GetValue(pField, "options");
		if ( !DemoBuf_Append(pBuf, "<select name=\"") ||
			!DemoBuf_AppendEscaped(pBuf, sName) ||
			!DemoBuf_Append(pBuf, "\">") ) {
			return FALSE;
		}
		if ( pOptions != NULL && xvoType(pOptions) == XVO_DT_ARRAY ) {
	for ( i = 0u; i < xvoArrayItemCount(pOptions); ++i ) {
		xvalue pItem = xvoArrayGetValue(pOptions, i);
				if ( !DemoBuf_Append(pBuf, "<option>") ||
					!Demo_AppendOptionLabel(pBuf, pItem) ||
					!DemoBuf_Append(pBuf, "</option>") ) {
					return FALSE;
				}
			}
		}
		if ( !DemoBuf_Append(pBuf, "</select>") ) {
			return FALSE;
		}
	} else {
		if ( !DemoBuf_Append(pBuf, "<input type=\"text\" name=\"") ||
			!DemoBuf_AppendEscaped(pBuf, sName) ||
			!DemoBuf_Append(pBuf, "\" />") ) {
			return FALSE;
		}
	}

	if ( !DemoBuf_Append(pBuf, "</div>") ) {
		return FALSE;
	}
	return TRUE;
}

static bool Demo_AppendFieldsHtml(DemoBuf* pBuf, xvalue pFields)
{
	uint32 i = 0u;
	if ( pFields == NULL || xvoType(pFields) != XVO_DT_ARRAY ) {
		return TRUE;
	}
	for ( i = 0u; i < xvoArrayItemCount(pFields); ++i ) {
		xvalue pField = xvoArrayGetValue(pFields, i);
		if ( pField == NULL || xvoType(pField) != XVO_DT_TABLE ) {
			continue;
		}
		if ( !Demo_AppendFieldHtml(pBuf, pField) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static char* Demo_GenerateFormHtml(xvalue pConfig, size_t* pRetSize)
{
	DemoBuf tBuf;
	const char* sTitle = Demo_GetText(pConfig, "title", "Form");
	xvalue pGroups = Demo_GetValue(pConfig, "groups");
	xvalue pFields = Demo_GetValue(pConfig, "fields");
	uint32 i = 0u;

	DemoBuf_Init(&tBuf);

	if ( !DemoBuf_Append(&tBuf, "<div class=\"xte-form-block\">") ||
		!DemoBuf_Append(&tBuf, "<h3>") ||
		!DemoBuf_AppendEscaped(&tBuf, sTitle) ||
		!DemoBuf_Append(&tBuf, "</h3>") ) {
		DemoBuf_Unit(&tBuf);
		return NULL;
	}

	if ( pGroups != NULL && xvoType(pGroups) == XVO_DT_ARRAY ) {
		for ( i = 0u; i < xvoArrayItemCount(pGroups); ++i ) {
			xvalue pGroup = xvoArrayGetValue(pGroups, i);
			const char* sGroupTitle = NULL;
			if ( pGroup == NULL || xvoType(pGroup) != XVO_DT_TABLE ) {
				continue;
			}
			sGroupTitle = Demo_GetText(pGroup, "title", "Group");
			if ( !DemoBuf_Append(&tBuf, "<section class=\"xte-form-group\">") ||
				!DemoBuf_Append(&tBuf, "<h4>") ||
				!DemoBuf_AppendEscaped(&tBuf, sGroupTitle) ||
				!DemoBuf_Append(&tBuf, "</h4>") ) {
				DemoBuf_Unit(&tBuf);
				return NULL;
			}
			if ( !Demo_AppendFieldsHtml(&tBuf, Demo_GetValue(pGroup, "fields")) ||
				!DemoBuf_Append(&tBuf, "</section>") ) {
				DemoBuf_Unit(&tBuf);
				return NULL;
			}
		}
	} else {
		if ( !Demo_AppendFieldsHtml(&tBuf, pFields) ) {
			DemoBuf_Unit(&tBuf);
			return NULL;
		}
	}

	if ( !DemoBuf_Append(&tBuf, "</div>") ) {
		DemoBuf_Unit(&tBuf);
		return NULL;
	}
	return DemoBuf_Detach(&tBuf, pRetSize);
}

static int Demo_FormCompileParse(XTE_StmtParseCtx* pCtx, void** ppData)
{
	xvalue pConfig = NULL;
	DemoCompiledForm* pForm = NULL;
	size_t iHtmlSize = 0u;
	++g_tDemoCounters.iCompileParseCount;

	pConfig = xrtParseJSON((str)pCtx->sRawBody, pCtx->iRawBodySize);
	if ( pConfig == NULL ) {
		return xteStmtParseSetError(pCtx, 1, "form json parse failed");
	}

	pForm = (DemoCompiledForm*)xrtCalloc(1u, sizeof(*pForm));
	if ( pForm == NULL ) {
		xvoUnref(pConfig);
		return xteStmtParseSetError(pCtx, DEMO_FORM_ERROR_ALLOC, "compile form data alloc failed");
	}

	++g_tDemoCounters.iCompileGenerateCount;
	pForm->sHtml = Demo_GenerateFormHtml(pConfig, &iHtmlSize);
	pForm->iHtmlSize = iHtmlSize;
	xvoUnref(pConfig);
	if ( pForm->sHtml == NULL ) {
		xrtFree(pForm);
		return xteStmtParseSetError(pCtx, DEMO_FORM_ERROR_ALLOC, "compile form html failed");
	}

	*ppData = pForm;
	return 1;
}

static XTE_Flow Demo_FormCompileRender(XTE_StmtRenderCtx* pCtx)
{
	DemoCompiledForm* pForm = (DemoCompiledForm*)pCtx->pData;
	++g_tDemoCounters.iCompileRenderCount;
	if ( pForm == NULL || pForm->sHtml == NULL ) {
		return xteStmtSetError(pCtx, 1, "compile form data missing");
	}
	if ( !xteStmtWrite(pCtx, pForm->sHtml, pForm->iHtmlSize) ) {
		return xteStmtSetError(pCtx, 1, "compile form write failed");
	}
	return XTE_FLOW_OK;
}

static void Demo_FormCompileFree(void* pData)
{
	DemoCompiledForm* pForm = (DemoCompiledForm*)pData;
	if ( pForm == NULL ) {
		return;
	}
	if ( pForm->sHtml != NULL ) {
		xrtFree(pForm->sHtml);
	}
	xrtFree(pForm);
}

static int Demo_FormRuntimeParse(XTE_StmtParseCtx* pCtx, void** ppData)
{
	DemoRuntimeForm* pForm = NULL;
	++g_tDemoCounters.iRuntimeParseCount;

	pForm = (DemoRuntimeForm*)xrtCalloc(1u, sizeof(*pForm));
	if ( pForm == NULL ) {
		return xteStmtParseSetError(pCtx, DEMO_FORM_ERROR_ALLOC, "runtime form data alloc failed");
	}
	pForm->sJson = (char*)xrtCopyStr((str)pCtx->sRawBody, pCtx->iRawBodySize);
	pForm->iJsonSize = pCtx->iRawBodySize;
	if ( pForm->sJson == NULL ) {
		xrtFree(pForm);
		return xteStmtParseSetError(pCtx, DEMO_FORM_ERROR_ALLOC, "runtime form json copy failed");
	}

	*ppData = pForm;
	return 1;
}

static XTE_Flow Demo_FormRuntimeRender(XTE_StmtRenderCtx* pCtx)
{
	DemoRuntimeForm* pForm = (DemoRuntimeForm*)pCtx->pData;
	xvalue pConfig = NULL;
	char* sHtml = NULL;
	size_t iHtmlSize = 0u;
	++g_tDemoCounters.iRuntimeRenderCount;

	if ( pForm == NULL || pForm->sJson == NULL ) {
		return xteStmtSetError(pCtx, 1, "runtime form data missing");
	}

	pConfig = xrtParseJSON((str)pForm->sJson, pForm->iJsonSize);
	if ( pConfig == NULL ) {
		return xteStmtSetError(pCtx, 1, "runtime form json parse failed");
	}
	++g_tDemoCounters.iRuntimeGenerateCount;
	sHtml = Demo_GenerateFormHtml(pConfig, &iHtmlSize);
	xvoUnref(pConfig);
	if ( sHtml == NULL ) {
		return xteStmtSetError(pCtx, DEMO_FORM_ERROR_ALLOC, "runtime form html failed");
	}

	if ( !xteStmtWrite(pCtx, sHtml, iHtmlSize) ) {
		xrtFree(sHtml);
		return xteStmtSetError(pCtx, 1, "runtime form write failed");
	}
	xrtFree(sHtml);
	return XTE_FLOW_OK;
}

static void Demo_FormRuntimeFree(void* pData)
{
	DemoRuntimeForm* pForm = (DemoRuntimeForm*)pData;
	if ( pForm == NULL ) {
		return;
	}
	if ( pForm->sJson != NULL ) {
		xrtFree(pForm->sJson);
	}
	xrtFree(pForm);
}

static const char* g_sTemplateText =
	"<article>\n"
	"<h2>Compile-Time Form Block</h2>\n"
	"{{#form}}\n"
	"{\n"
	"  \"title\": \"Compile Form\",\n"
	"  \"groups\": [\n"
	"    {\n"
	"      \"title\": \"Basic\",\n"
	"      \"fields\": [\n"
	"        {\"name\": \"title\", \"type\": \"text\", \"label\": \"Title\"},\n"
	"        {\"name\": \"enabled\", \"type\": \"switch\", \"label\": \"Enabled\"},\n"
	"        {\"name\": \"mode\", \"type\": \"select\", \"label\": \"Mode\", \"options\": [\"md\", \"html\", \"code\"]}\n"
	"      ]\n"
	"    }\n"
	"  ]\n"
	"}\n"
	"{{#end}}\n"
	"<h2>Runtime Form Block</h2>\n"
	"{{#form_runtime}}\n"
	"{\n"
	"  \"title\": \"Runtime Form\",\n"
	"  \"fields\": [\n"
	"    {\"name\": \"summary\", \"type\": \"textarea\", \"label\": \"Summary\"},\n"
	"    {\"name\": \"editor\", \"type\": \"select\", \"label\": \"Editor\", \"options\": [\n"
	"      {\"label\": \"Markdown\", \"value\": \"md\"},\n"
	"      {\"label\": \"RichText\", \"value\": \"html\"}\n"
	"    ]}\n"
	"  ]\n"
	"}\n"
	"{{#end}}\n"
	"</article>\n";

static void Demo_PrintNodeStats(xtetemplate hTemplate)
{
	uint32 i = 0u;
	uint32 iText = 0u;
	uint32 iStatement = 0u;
	for ( i = 0u; i < xteTemplateGetNodeCount(hTemplate); ++i ) {
		const XTE_Node* pNode = xteTemplateGetNode(hTemplate, i);
		if ( pNode == NULL ) {
			continue;
		}
		if ( pNode->iType == XTE_NODE_TEXT ) {
			++iText;
		} else if ( pNode->iType == XTE_NODE_STATEMENT ) {
			++iStatement;
		}
	}
	printf("node stats: text=%u statement=%u total=%u\n", iText, iStatement, xteTemplateGetNodeCount(hTemplate));
}

int main(void)
{
	xteengine hEngine = NULL;
	xtetemplate hTemplate = NULL;
	XTE_ParseOptions tParseOptions = { 0 };
	XTE_Error tError = { 0 };
	size_t iOutSize = 0u;
	char* sOutput = NULL;
	int i = 0;
	const XTE_StatementDef tCompileStmt = {
		.sName = "form",
		.iFlags = XTE_STMT_BLOCK | XTE_STMT_RAW_BODY,
		.iMinArgs = 0,
		.iMaxArgs = 0,
		.pUserData = NULL,
		.procParse = Demo_FormCompileParse,
		.procRender = Demo_FormCompileRender,
		.procFreeData = Demo_FormCompileFree
	};
	const XTE_StatementDef tRuntimeStmt = {
		.sName = "form_runtime",
		.iFlags = XTE_STMT_BLOCK | XTE_STMT_RAW_BODY,
		.iMinArgs = 0,
		.iMaxArgs = 0,
		.pUserData = NULL,
		.procParse = Demo_FormRuntimeParse,
		.procRender = Demo_FormRuntimeRender,
		.procFreeData = Demo_FormRuntimeFree
	};

	xrtInit();

	hEngine = xteCreateEngine();
	if ( hEngine == NULL ) {
		fprintf(stderr, "xteCreateEngine failed\n");
		xrtUnit();
		return 1;
	}
	if ( !xteRegisterStatement(hEngine, &tCompileStmt) ) {
		fprintf(stderr, "register form statement failed\n");
		xteDestroyEngine(hEngine);
		xrtUnit();
		return 1;
	}
	if ( !xteRegisterStatement(hEngine, &tRuntimeStmt) ) {
		fprintf(stderr, "register form_runtime statement failed\n");
		xteDestroyEngine(hEngine);
		xrtUnit();
		return 1;
	}

	tParseOptions.sBracket = "{{}}";
	hTemplate = xteParseEx(hEngine, g_sTemplateText, 0u, &tParseOptions, &tError);
	if ( hTemplate == NULL ) {
		fprintf(stderr, "parse failed: code=%d desc=%s line=%u col=%u\n",
			tError.iCode,
			tError.sDesc ? tError.sDesc : "(null)",
			tError.iLine,
			tError.iColumn);
		xteDestroyEngine(hEngine);
		xrtUnit();
		return 1;
	}

	printf("== XTE form block demo ==\n");
	Demo_PrintNodeStats(hTemplate);
	printf("compile parse count   : %u\n", g_tDemoCounters.iCompileParseCount);
	printf("compile generate count: %u\n", g_tDemoCounters.iCompileGenerateCount);
	printf("runtime parse count   : %u\n", g_tDemoCounters.iRuntimeParseCount);
	printf("runtime generate count: %u\n", g_tDemoCounters.iRuntimeGenerateCount);

	for ( i = 1; i <= 3; ++i ) {
		iOutSize = 0u;
		sOutput = xteMake(hTemplate, NULL, NULL, NULL, &iOutSize);
		printf("\n-- render #%d --\n", i);
		if ( sOutput == NULL ) {
			fprintf(stderr, "render failed on #%d\n", i);
			xteDestroyTemplate(hTemplate);
			xteDestroyEngine(hEngine);
			xrtUnit();
			return 1;
		}
		printf("%s\n", sOutput);
		xrtFree(sOutput);
	}

	printf("\n== counters after 3 renders ==\n");
	printf("compile parse count   : %u\n", g_tDemoCounters.iCompileParseCount);
	printf("compile generate count: %u\n", g_tDemoCounters.iCompileGenerateCount);
	printf("compile render count  : %u\n", g_tDemoCounters.iCompileRenderCount);
	printf("runtime parse count   : %u\n", g_tDemoCounters.iRuntimeParseCount);
	printf("runtime generate count: %u\n", g_tDemoCounters.iRuntimeGenerateCount);
	printf("runtime render count  : %u\n", g_tDemoCounters.iRuntimeRenderCount);

	printf("\nconclusion:\n");
	printf("1. {{#form}} block syntax works through xte custom statements.\n");
	printf("2. Current public API supports compile-time pre-generation via procParse + pData.\n");
	printf("3. The parsed AST still keeps a statement node; it is not rewritten into XTE_NODE_TEXT.\n");
	printf("4. To support true text-node replacement, xte needs a public compile-time node rewrite helper.\n");

	xteDestroyTemplate(hTemplate);
	xteDestroyEngine(hEngine);
	xrtUnit();
	return 0;
}
