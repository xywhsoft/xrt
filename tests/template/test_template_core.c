#include "../test.h"



/* 要求当前错误属于稳定模板错误域和指定代码。 */
static void testTemplateError(xtemplateerror Code, cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, sMessage);
	testRequire(
		(xrtErrorDomain(pError) != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.template") == 0),
		sMessage
	);
	testRequire(xrtErrorCode(pError) == (int32)Code, sMessage);
}



/* 验证公开模板结构不随控制、组合或扩展裁剪改变布局。 */
static void testTemplatePublicLayout(void)
{
	struct {
		uint64 Before;
		xtemplateconfig Config;
		uint64 After;
	} Compile;
	struct {
		uint64 Before;
		xtemplaterenderconfig Config;
		uint64 After;
	} Render;
	xtemplatenodeview Node;
	bool bCompileControl;
	bool bCompileExtension;
	bool bRenderControl;
	bool bRenderCompose;

	memset(&Compile, 0xA5, sizeof(Compile));
	Compile.Before = UINT64_C(0x1122334455667788);
	Compile.After = UINT64_C(0x8877665544332211);
	xrtTemplateConfigInit(&Compile.Config);
	testRequire(
		(Compile.Before == UINT64_C(0x1122334455667788)) &&
		(Compile.After == UINT64_C(0x8877665544332211)),
		"template compile config initializer changed adjacent memory"
	);
	testRequire(
		Compile.Config.Registry == NULL,
		"template compile registry default was not cleared"
	);
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
		bCompileControl = (Compile.Config.MaxExpressions != 0u) &&
			(Compile.Config.MaxBlockDepth != 0u) &&
			(Compile.Config.MaxExpressionDepth != 0u);
	#else
		bCompileControl = (Compile.Config.MaxExpressions == 0u) &&
			(Compile.Config.MaxBlockDepth == 0u) &&
			(Compile.Config.MaxExpressionDepth == 0u);
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)
		bCompileExtension = (Compile.Config.MaxArguments != 0u) &&
			(Compile.Config.MaxCallArguments != 0u);
	#else
		bCompileExtension = (Compile.Config.MaxArguments == 0u) &&
			(Compile.Config.MaxCallArguments == 0u);
	#endif
	testRequire(
		bCompileControl && bCompileExtension,
		"trimmed template compile fields were not retained and cleared"
	);

	memset(&Render, 0xA5, sizeof(Render));
	Render.Before = UINT64_C(0x123456789ABCDEF0);
	Render.After = UINT64_C(0x0FEDCBA987654321);
	xrtTemplateRenderConfigInit(&Render.Config);
	testRequire(
		(Render.Before == UINT64_C(0x123456789ABCDEF0)) &&
		(Render.After == UINT64_C(0x0FEDCBA987654321)),
		"template render config initializer changed adjacent memory"
	);
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
		bRenderControl = (Render.Config.MaxDepth != 0u) &&
			(Render.Config.MaxLoopIterations != 0u);
	#else
		bRenderControl = (Render.Config.MaxDepth == 0u) &&
			(Render.Config.MaxLoopIterations == 0u);
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
		bRenderCompose = Render.Config.MaxIncludeDepth != 0u;
	#else
		bRenderCompose = Render.Config.MaxIncludeDepth == 0u;
	#endif
	testRequire(
		bRenderControl && bRenderCompose &&
		(Render.Config.Resolve == NULL) &&
		(Render.Config.ResolveData == NULL),
		"trimmed template render fields were not retained and cleared"
	);

	memset(&Node, 0, sizeof(Node));
	Node.Name = XRT_STR_LITERAL("stable");
	testRequire(
		(Node.Name.Size == 6u) &&
		(XTEMPLATE_NODE_EXTENSION > XTEMPLATE_NODE_RAW),
		"template node view or node enum is not feature invariant"
	);
}



/* 构造覆盖对象、数组、数字和时间的共享渲染数据。 */
static xvalue* testTemplateData(void)
{
	xvalue* pRoot = xrtValueObject();
	xvalue* pUser = xrtValueObject();
	xvalue* pItems = xrtValueArray();
	xvalue* pItem = xrtValueObject();
	xtime Time;

	testRequire(
		(pRoot != NULL) && (pUser != NULL) &&
		(pItems != NULL) && (pItem != NULL),
		"template test value allocation failed"
	);
	testRequire(
		xrtValueObjectSetNew(
			pUser,
			XRT_STR_LITERAL("name"),
			xrtValueString(XRT_STR_LITERAL("xrt"))
		),
		"template user name setup failed"
	);
	testRequire(
		xrtValueObjectSetNew(
			pItem,
			XRT_STR_LITERAL("name"),
			xrtValueString(XRT_STR_LITERAL("last"))
		),
		"template array item setup failed"
	);
	testRequire(
		xrtValueArrayAppendNew(pItems, pItem),
		"template array setup failed"
	);
	testRequire(
		xrtValueObjectSetNew(pRoot, XRT_STR_LITERAL("user"), pUser) &&
		xrtValueObjectSetNew(pRoot, XRT_STR_LITERAL("items"), pItems) &&
		xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("count"),
			xrtValueInt(42)
		) &&
		xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("active"),
			xrtValueBool(true)
		),
		"template root setup failed"
	);
	testRequire(
		xrtDateTime(2024, 2, 3, 4, 5, 6, 0, &Time) &&
		xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("when"),
			xrtValueTime(Time)
		),
		"template time setup failed"
	);
	return pRoot;
}



/* 验证编译元数据、路径预编译和全部核心输出类型。 */
static void testTemplateCompileAndRender(void)
{
	xstrview Source = XRT_STR_LITERAL(
		"Hello {$user.name}; {%count:04d}; {&when:%F}; "
		"{$items[-1].name}; {$active}; escaped={{$count}"
	);
	xtemplate* pTemplate = xrtTemplateCompile(Source);
	xvalue* pData = testTemplateData();
	xtemplatenodeview Node;
	str sResult;
	size_t iSize;

	testRequire(pTemplate != NULL, "template compile failed");
	testRequire(
		xrtStrEqual(xrtTemplateSource(pTemplate), Source),
		"template source metadata mismatch"
	);
	testRequire(
		xrtTemplateNodeCount(pTemplate) >= 6u,
		"template node count is incomplete"
	);
	testRequire(
		xrtTemplateNode(pTemplate, 1u, &Node) &&
		(Node.Type == XTEMPLATE_NODE_OUTPUT) &&
		xrtStrEqual(Node.Expression, XRT_STR_LITERAL("user.name")),
		"template output node metadata mismatch"
	);
	sResult = xrtTemplateRender(pTemplate, pData, &iSize);
	testRequire(sResult != NULL, "template render failed");
	testRequire(
		(iSize == strlen(sResult)) &&
		(strcmp(
			sResult,
			"Hello xrt; 0042; 2024-02-03; last; true; escaped={$count}"
		) == 0),
		"template render output mismatch"
	);
	xrtFree(sResult);
	xrtValueRelease(pData);
	xrtTemplateRelease(xrtTemplateRef(pTemplate));
	xrtTemplateRelease(pTemplate);
}



/* 验证自定义标记、嵌入零文本和空模板结果。 */
static void testTemplateBinaryAndMarkers(void)
{
	const char arrSource[] = { 'A', '\0', 'B' };
	xtemplateconfig Config;
	xtemplate* pTemplate;
	str sResult;
	size_t iSize;

	pTemplate = xrtTemplateCompile(
		(xstrview){ arrSource, sizeof(arrSource) }
	);
	testRequire(pTemplate != NULL, "binary template compile failed");
	sResult = xrtTemplateRender(pTemplate, NULL, &iSize);
	testRequire(
		(sResult != NULL) && (iSize == sizeof(arrSource)) &&
		(memcmp(sResult, arrSource, sizeof(arrSource)) == 0) &&
		(sResult[iSize] == 0),
		"binary template render mismatch"
	);
	xrtFree(sResult);
	xrtTemplateRelease(pTemplate);

	xrtTemplateConfigInit(&Config);
	Config.Open = XRT_STR_LITERAL("[[");
	Config.Close = XRT_STR_LITERAL("]]");
	pTemplate = xrtTemplateCompileConfig(
		XRT_STR_LITERAL("[[[[literal [[$name]]"),
		&Config
	);
	testRequire(pTemplate != NULL, "custom template marker compile failed");
	xrtTemplateRelease(pTemplate);

	pTemplate = xrtTemplateCompile((xstrview){ NULL, 0 });
	testRequire(pTemplate != NULL, "empty template compile failed");
	sResult = xrtTemplateRender(pTemplate, NULL, &iSize);
	testRequire(
		(sResult != NULL) && (iSize == 0u) && (sResult[0] == 0),
		"empty template did not return an owned empty string"
	);
	xrtFree(sResult);
	xrtTemplateRelease(pTemplate);
}



/* 验证语法失败携带精确模板源码位置。 */
static void testTemplateSyntax(void)
{
	xtemplate* pTemplate;
	xtemplatelocation Location;

	xrtClearError();
	pTemplate = xrtTemplateCompile(XRT_STR_LITERAL("line\n{$user..name}"));
	testRequire(pTemplate == NULL, "invalid template path was accepted");
	testTemplateError(
		XTEMPLATE_ERROR_SYNTAX,
		"template syntax error metadata mismatch"
	);
	testRequire(
		xrtTemplateErrorLocation(xrtGetError(), &Location) &&
		(Location.Line == 2u) && (Location.Column >= 3u),
		"template syntax location mismatch"
	);

	xrtClearError();
	pTemplate = xrtTemplateCompile(XRT_STR_LITERAL("{$missing"));
	testRequire(pTemplate == NULL, "unclosed template tag was accepted");
	testTemplateError(
		XTEMPLATE_ERROR_SYNTAX,
		"unclosed template error mismatch"
	);
}



/* 验证未定义策略和编译、输出、步骤预算均为硬限制。 */
static void testTemplateLimits(void)
{
	xtemplateconfig Compile;
	xtemplaterenderconfig Render;
	xtemplate* pTemplate;
	xvalue* pData = testTemplateData();
	xstrbuf Output;

	xrtTemplateConfigInit(&Compile);
	Compile.MaxSourceBytes = 2u;
	xrtClearError();
	pTemplate = xrtTemplateCompileConfig(XRT_STR_LITERAL("abc"), &Compile);
	testRequire(pTemplate == NULL, "template source limit was ignored");
	testTemplateError(
		XTEMPLATE_ERROR_LIMIT,
		"template source limit error mismatch"
	);

	xrtTemplateConfigInit(&Compile);
	Compile.MaxPathDepth = 1u;
	xrtClearError();
	pTemplate = xrtTemplateCompileConfig(
		XRT_STR_LITERAL("{$user.name}"),
		&Compile
	);
	testRequire(pTemplate == NULL, "template path depth limit was ignored");
	testTemplateError(
		XTEMPLATE_ERROR_LIMIT,
		"template path depth error mismatch"
	);

	pTemplate = xrtTemplateCompile(XRT_STR_LITERAL("prefix {$missing}"));
	testRequire(pTemplate != NULL, "template limit fixture compile failed");
	xrtTemplateRenderConfigInit(&Render);
	Render.Root = pData;
	Render.Current = pData;
	Render.Flags = XTEMPLATE_STRICT_UNDEFINED;
	xrtClearError();
	testRequire(
		!xrtTemplateWrite(pTemplate, &Render, NULL, NULL),
		"null template writer was accepted"
	);
	xrtClearError();
	xrtStrBufInit(&Output);
	Render.MaxOutputBytes = 3u;
	testRequire(
		!xrtTemplateRenderTo(pTemplate, &Render, &Output) &&
		(Output.Size == 0u),
		"template output limit was not transactional"
	);
	testTemplateError(
		XTEMPLATE_ERROR_LIMIT,
		"template output limit error mismatch"
	);
	xrtClearError();
	Render.MaxOutputBytes = XTEMPLATE_OUTPUT_DEFAULT;
	Render.MaxSteps = 2u;
	testRequire(
		!xrtTemplateRenderTo(pTemplate, &Render, &Output) &&
		(Output.Size == 0u),
		"template step limit was ignored"
	);
	testTemplateError(
		XTEMPLATE_ERROR_LIMIT,
		"template step limit error mismatch"
	);
	xrtClearError();
	Render.MaxSteps = XTEMPLATE_STEPS_DEFAULT;
	testRequire(
		!xrtTemplateRenderTo(pTemplate, &Render, &Output),
		"strict undefined template path was accepted"
	);
	testTemplateError(
		XTEMPLATE_ERROR_UNDEFINED,
		"strict undefined error mismatch"
	);
	xrtStrBufFree(&Output);
	xrtTemplateRelease(pTemplate);
	xrtValueRelease(pData);
}



/* Writer 测试状态记录实际收到的分片和故障模式。 */
typedef struct testtemplatewriter {
	xstrbuf Output;
	bool Fail;
	bool SetError;
} testtemplatewriter;



/* 按测试状态写入分片，并可模拟带错误或无错误拒绝。 */
static bool testTemplateWriteCallback(ptr pUserData, xstrview Text)
{
	testtemplatewriter* pWriter = (testtemplatewriter*)pUserData;

	if ( pWriter->SetError ) {
		xerror* pError = xrtErrorCreate(
			XERR_IO,
			"test.writer",
			77,
			"writer failure"
		);

		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	if ( pWriter->Fail ) {
		return false;
	}
	return xrtStrBufAppend(&pWriter->Output, Text);
}



/* 验证 writer 错误隔离、错误透传和无错误拒绝补全。 */
static void testTemplateWriterErrors(void)
{
	xtemplate* pTemplate = xrtTemplateCompile(XRT_STR_LITERAL("text"));
	xtemplaterenderconfig Render;
	testtemplatewriter Writer;
	xerror* pPrevious;

	testRequire(pTemplate != NULL, "template writer fixture compile failed");
	xrtTemplateRenderConfigInit(&Render);
	memset(&Writer, 0, sizeof(Writer));
	xrtStrBufInit(&Writer.Output);
	pPrevious = xrtErrorCreate(XERR_VALUE, "test.previous", 9, "previous");
	testRequire(pPrevious != NULL, "previous error allocation failed");
	xrtSetError(pPrevious);
	Writer.SetError = true;
	testRequire(
		xrtTemplateWrite(
			pTemplate,
			&Render,
			testTemplateWriteCallback,
			&Writer
		),
		"successful template writer failed"
	);
	testRequire(
		xrtGetError() == pPrevious,
		"successful writer did not restore the previous error"
	);
	xrtClearError();
	xrtErrorFree(pPrevious);

	Writer.Fail = true;
	xrtClearError();
	testRequire(
		!xrtTemplateWrite(
			pTemplate,
			&Render,
			testTemplateWriteCallback,
			&Writer
		),
		"failing template writer was accepted"
	);
	testRequire(
		(xrtErrorDomain(xrtGetError()) != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "test.writer") == 0),
		"writer callback error was not preserved"
	);

	Writer.SetError = false;
	xrtClearError();
	testRequire(
		!xrtTemplateWrite(
			pTemplate,
			&Render,
			testTemplateWriteCallback,
			&Writer
		),
		"silent writer rejection was accepted"
	);
	testTemplateError(
		XTEMPLATE_ERROR_WRITE,
		"silent writer rejection error mismatch"
	);
	xrtClearError();
	xrtStrBufFree(&Writer.Output);
	xrtTemplateRelease(pTemplate);
}



/* 运行模板核心层全部契约和边界测试。 */
int main(void)
{
	testTemplatePublicLayout();
	testTemplateCompileAndRender();
	testTemplateBinaryAndMarkers();
	testTemplateSyntax();
	testTemplateLimits();
	testTemplateWriterErrors();
	printf("[PASS] template core\n");
	return 0;
}
