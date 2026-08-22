#include "../test.h"



/* 扩展夹具记录作用域和总调用次数，不参与注册表所有权。 */
typedef struct testextensionstate {
	size_t Calls;
	const xvalue* Root;
	const xvalue* Global;
} testextensionstate;



/* 比较两个不依赖零结尾的字符串视图。 */
static bool testExtensionViewEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 要求当前错误属于模板域并具有指定代码。 */
static void testExtensionError(xtemplateerror Code, cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, sMessage);
	testRequire(
		(xrtErrorDomain(pError) != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.template") == 0) &&
		(xrtErrorCode(pError) == (int32)Code),
		sMessage
	);
}



/* 丢弃输出以便只验证回调、预算和错误路径。 */
static bool testExtensionDiscard(ptr pUserData, xstrview Text)
{
	(void)pUserData;
	(void)Text;
	return true;
}



/* 使用注册表和显式预算编译模板。 */
static xtemplate* testExtensionCompile(
	const xtemplateregistry* pRegistry,
	xstrview Source
)
{
	xtemplateconfig Config;

	xrtTemplateConfigInit(&Config);
	Config.Registry = pRegistry;
	return xrtTemplateCompileConfig(Source, &Config);
}



/* 读取一个参数并要求它是字符串。 */
static bool testExtensionStringArgument(
	xtemplatecall* pCall,
	size_t iIndex,
	xtemplatevalue* pValue
)
{
	xtemplateargview Argument;

	return xrtTemplateCallArgument(pCall, iIndex, &Argument) &&
		xrtTemplateCallEval(pCall, &Argument, pValue) &&
		(pValue->Type == XVALUE_STRING);
}



/* 函数扩展输出首个字符串参数和可选 suffix 命名参数。 */
static bool testExtensionEcho(xtemplatecall* pCall)
{
	testextensionstate* pState =
		(testextensionstate*)xrtTemplateCallData(pCall);
	xtemplateargview Suffix;
	xtemplateargview Missing;
	xtemplatevalue Value;
	xtemplatevalue Tail;

	if ( (pState == NULL) ||
		 !testExtensionViewEqual(
			xrtTemplateCallName(pCall),
			XRT_STR_LITERAL("echo")
		 ) ||
		 (xrtTemplateCallArgumentCount(pCall) != 2u) ||
		 (xrtTemplateCallRoot(pCall) != pState->Root) ||
		 (xrtTemplateCallCurrent(pCall) != pState->Root) ||
		 (xrtTemplateCallGlobal(pCall) != pState->Global) ||
		 !testExtensionStringArgument(pCall, 0u, &Value) ||
		 !xrtTemplateCallFind(
			pCall,
			XRT_STR_LITERAL("suffix"),
			&Suffix
		 ) ||
		 !xrtTemplateCallEval(pCall, &Suffix, &Tail) ||
		 (Tail.Type != XVALUE_STRING) ||
		 xrtTemplateCallFind(
			pCall,
			(xstrview){ NULL, 0 },
			&Missing
		 ) ) {
		return false;
	}
	pState->Calls++;
	return xrtTemplateCallWrite(pCall, Value.Text) &&
		xrtTemplateCallWrite(pCall, Tail.Text);
}



/* 行内语句扩展写出一个固定标记。 */
static bool testExtensionMark(xtemplatecall* pCall)
{
	testextensionstate* pState =
		(testextensionstate*)xrtTemplateCallData(pCall);

	if ( (pState == NULL) ||
		 (xrtTemplateCallArgumentCount(pCall) != 0u) ) {
		return false;
	}
	pState->Calls++;
	return xrtTemplateCallWrite(pCall, XRT_STR_LITERAL("M"));
}



/* 解析块扩展按整数参数重复渲染主体。 */
static bool testExtensionRepeat(xtemplatecall* pCall)
{
	testextensionstate* pState =
		(testextensionstate*)xrtTemplateCallData(pCall);
	xtemplateargview Argument;
	xtemplatevalue Value;

	if ( (pState == NULL) ||
		 !xrtTemplateCallArgument(pCall, 0u, &Argument) ||
		 !xrtTemplateCallEval(pCall, &Argument, &Value) ||
		 (Value.Type != XVALUE_INT) ||
		 (Value.Integer < 0) || (Value.Integer > 16) ) {
		return false;
	}
	pState->Calls++;
	for ( int64 i = 0; i < Value.Integer; i++ ) {
		if ( !xrtTemplateCallRender(pCall) ) {
			return false;
		}
	}
	return true;
}



/* 解析块扩展把当前作用域替换成参数值后渲染主体。 */
static bool testExtensionWith(xtemplatecall* pCall)
{
	testextensionstate* pState =
		(testextensionstate*)xrtTemplateCallData(pCall);
	xtemplateargview Argument;
	xtemplatevalue Value;

	if ( (pState == NULL) ||
		 !xrtTemplateCallArgument(pCall, 0u, &Argument) ||
		 !xrtTemplateCallEval(pCall, &Argument, &Value) ||
		 (Value.Value == NULL) ) {
		return false;
	}
	pState->Calls++;
	return xrtTemplateCallRenderCurrent(pCall, Value.Value);
}



/* 原样块扩展把未经解析的主体直接写到共享 writer。 */
static bool testExtensionRaw(xtemplatecall* pCall)
{
	testextensionstate* pState =
		(testextensionstate*)xrtTemplateCallData(pCall);

	if ( pState == NULL ) {
		return false;
	}
	pState->Calls++;
	return xrtTemplateCallWrite(pCall, xrtTemplateCallRaw(pCall));
}



/* 失败扩展设置可追踪原因，模板层应包装而不吞掉它。 */
static bool testExtensionFail(xtemplatecall* pCall)
{
	xerror* pError = xrtErrorCreate(
		XERR_IO,
		"test.template.extension",
		17,
		"extension fixture failed"
	);

	(void)pCall;
	xrtSetError(pError);
	xrtErrorFree(pError);
	return false;
}



/* 使用描述项数据中的文本验证函数与语句可以同名。 */
static bool testExtensionWriteData(xtemplatecall* pCall)
{
	return xrtTemplateCallWrite(
		pCall,
		xrtStrView((cstr)xrtTemplateCallData(pCall))
	);
}



/* 记录注册表最终释放时对每个成功接管项执行一次析构。 */
static void testExtensionDrop(ptr pData)
{
	size_t* pCount = (size_t*)pData;

	(*pCount)++;
}



/* 验证四种扩展模式、参数视图、作用域和模板持有注册表引用。 */
static void testExtensionModes(void)
{
	testextensionstate State = { 0, NULL, NULL };
	xtemplateextension arrExtensions[] = {
		{ XRT_STR_LITERAL("echo"), XTEMPLATE_EXTENSION_FUNCTION,
			1u, 2u, testExtensionEcho, &State, NULL },
		{ XRT_STR_LITERAL("mark"), XTEMPLATE_EXTENSION_STATEMENT,
			0u, 0u, testExtensionMark, &State, NULL },
		{ XRT_STR_LITERAL("repeat"), XTEMPLATE_EXTENSION_BLOCK,
			1u, 1u, testExtensionRepeat, &State, NULL },
		{ XRT_STR_LITERAL("with"), XTEMPLATE_EXTENSION_BLOCK,
			1u, 1u, testExtensionWith, &State, NULL },
		{ XRT_STR_LITERAL("literal"), XTEMPLATE_EXTENSION_RAW_BLOCK,
			0u, 0u, testExtensionRaw, &State, NULL }
	};
	xtemplateregistry* pRegistry = xrtTemplateRegistryCreate(
		arrExtensions,
		sizeof(arrExtensions) / sizeof(arrExtensions[0])
	);
	xtemplate* pTemplate;
	xvalue* pRoot = xrtValueObject();
	xvalue* pUser = xrtValueObject();
	xvalue* pGlobal = xrtValueString(XRT_STR_LITERAL("global"));
	xtemplaterenderconfig Render;
	size_t iExtensions = 0;

	testRequire(
		(pRegistry != NULL) && (pRoot != NULL) &&
		(pUser != NULL) && (pGlobal != NULL),
		"template extension mode fixture allocation failed"
	);
	testRequire(
		xrtValueObjectSetNew(
			pUser,
			XRT_STR_LITERAL("name"),
			xrtValueString(XRT_STR_LITERAL("Alice"))
		) && xrtValueObjectSetTake(
			pRoot,
			XRT_STR_LITERAL("user"),
			&pUser
		),
		"template extension mode data setup failed"
	);
	State.Root = pRoot;
	State.Global = pGlobal;
	pTemplate = testExtensionCompile(
		pRegistry,
		XRT_STR_LITERAL(
			"{@echo:user.name:suffix='!'}|{#mark}|"
			"{#repeat:2}[{$user.name}]{#end}|"
			"{#with:user}{$name}{#end}|"
			"{#literal}{$ignored}{#end}"
		)
	);
	testRequire(pTemplate != NULL, "template extension mode compile failed");
	xrtTemplateRegistryRelease(pRegistry);
	xrtTemplateRenderConfigInit(&Render);
	Render.Root = pRoot;
	Render.Current = pRoot;
	Render.Global = pGlobal;
	{
		static const char sExpected[] =
			"Alice!|M|[Alice][Alice]|Alice|{$ignored}";
		xstrbuf Output;

		xrtStrBufInit(&Output);
		testRequire(
			xrtTemplateRenderTo(pTemplate, &Render, &Output),
			"template extension mode render failed"
		);
		testRequire(
			(Output.Size == (sizeof(sExpected) - 1u)) &&
			(memcmp(
				Output.Data,
				sExpected,
				sizeof(sExpected) - 1u
			 ) == 0),
			"template extension mode output mismatch"
		);
		xrtStrBufFree(&Output);
	}
	testRequire(State.Calls == 5u, "template extension call count mismatch");
	for ( size_t i = 0; i < xrtTemplateNodeCount(pTemplate); i++ ) {
		xtemplatenodeview Node;

		testRequire(
			xrtTemplateNode(pTemplate, i, &Node),
			"template extension node view failed"
		);
		if ( Node.Type == XTEMPLATE_NODE_EXTENSION ) {
			iExtensions++;
			testRequire(
				Node.Name.Size != 0,
				"template extension node name is empty"
			);
		}
	}
	testRequire(iExtensions == 5u, "template extension node count mismatch");
	xrtTemplateRelease(pTemplate);
	xrtValueRelease(pGlobal);
	xrtValueRelease(pRoot);
}



/* 验证注册表名称空间、不可变引用和成功或失败时的数据所有权。 */
static void testExtensionRegistry(void)
{
	size_t iOwnedDrops = 0;
	size_t iFailedDropsA = 0;
	size_t iFailedDropsB = 0;
	xtemplateextension Owned = {
		XRT_STR_LITERAL("owned"), XTEMPLATE_EXTENSION_FUNCTION,
		0u, 0u, testExtensionWriteData, &iOwnedDrops, testExtensionDrop
	};
	xtemplateextension arrSame[] = {
		{ XRT_STR_LITERAL("same"), XTEMPLATE_EXTENSION_FUNCTION,
			0u, 0u, testExtensionWriteData, "F", NULL },
		{ XRT_STR_LITERAL("same"), XTEMPLATE_EXTENSION_STATEMENT,
			0u, 0u, testExtensionWriteData, "S", NULL }
	};
	xtemplateextension arrDuplicate[] = {
		{ XRT_STR_LITERAL("dup"), XTEMPLATE_EXTENSION_FUNCTION,
			0u, 0u, testExtensionWriteData,
			&iFailedDropsA, testExtensionDrop },
		{ XRT_STR_LITERAL("dup"), XTEMPLATE_EXTENSION_FUNCTION,
			0u, 0u, testExtensionWriteData,
			&iFailedDropsB, testExtensionDrop }
	};
	xtemplateregistry* pRegistry = xrtTemplateRegistryCreate(&Owned, 1u);
	xtemplateregistry* pSame;
	xtemplate* pTemplate;
	str sOutput;

	testRequire(pRegistry != NULL, "owned template registry creation failed");
	testRequire(
		xrtTemplateRegistryRef(pRegistry) == pRegistry,
		"template registry retain failed"
	);
	xrtTemplateRegistryRelease(pRegistry);
	testRequire(iOwnedDrops == 0u, "retained registry dropped data early");
	xrtTemplateRegistryRelease(pRegistry);
	testRequire(iOwnedDrops == 1u, "registry did not drop owned data once");

	pSame = xrtTemplateRegistryCreate(arrSame, 2u);
	testRequire(pSame != NULL, "function and statement names could not coexist");
	pTemplate = testExtensionCompile(
		pSame,
		XRT_STR_LITERAL("{@same}{#same}")
	);
	testRequire(pTemplate != NULL, "same-name extensions did not compile");
	sOutput = xrtTemplateRender(pTemplate, NULL, NULL);
	testRequire(
		(sOutput != NULL) && (strcmp(sOutput, "FS") == 0),
		"same-name extension dispatch mismatch"
	);
	xrtFree(sOutput);
	xrtTemplateRelease(pTemplate);
	xrtTemplateRegistryRelease(pSame);

	xrtClearError();
	testRequire(
		xrtTemplateRegistryCreate(arrDuplicate, 2u) == NULL,
		"duplicate extension name was accepted"
	);
	testExtensionError(
		XTEMPLATE_ERROR_CONFIG,
		"duplicate extension registry error mismatch"
	);
	testRequire(
		(iFailedDropsA == 0u) && (iFailedDropsB == 0u),
		"failed registry creation took caller data ownership"
	);
}



/* 创建覆盖语法、回调与渲染预算测试的共享注册表。 */
static xtemplateregistry* testExtensionBoundaryRegistry(
	testextensionstate* pState
)
{
	xtemplateextension arrExtensions[] = {
		{ XRT_STR_LITERAL("echo"), XTEMPLATE_EXTENSION_FUNCTION,
			1u, 2u, testExtensionEcho, pState, NULL },
		{ XRT_STR_LITERAL("repeat"), XTEMPLATE_EXTENSION_BLOCK,
			1u, 1u, testExtensionRepeat, pState, NULL },
		{ XRT_STR_LITERAL("fail"), XTEMPLATE_EXTENSION_FUNCTION,
			0u, 0u, testExtensionFail, pState, NULL }
	};

	return xrtTemplateRegistryCreate(
		arrExtensions,
		sizeof(arrExtensions) / sizeof(arrExtensions[0])
	);
}



/* 验证未知名称、参数语法、参数预算和块闭合在编译期失败。 */
static void testExtensionCompileBoundaries(void)
{
	testextensionstate State = { 0, NULL, NULL };
	xtemplateregistry* pRegistry = testExtensionBoundaryRegistry(&State);
	const cstr arrInvalid[] = {
		"{@echo:}",
		"{@echo:a=1:a=2}",
		"{@echo:1:2:3}",
		"{@bad-name}",
		"{#repeat:1}x"
	};

	testRequire(pRegistry != NULL, "template boundary registry failed");
	for ( size_t i = 0;
		i < (sizeof(arrInvalid) / sizeof(arrInvalid[0])); i++ ) {
		xtemplate* pTemplate;

		xrtClearError();
		pTemplate = testExtensionCompile(
			pRegistry,
			xrtStrView(arrInvalid[i])
		);
		testRequire(
			pTemplate == NULL,
			"invalid template extension syntax was accepted"
		);
		testExtensionError(
			XTEMPLATE_ERROR_SYNTAX,
			"template extension syntax error mismatch"
		);
	}
	{
		const cstr arrUndefined[] = { "{@missing}", "{#missing}" };

		for ( size_t i = 0; i < 2u; i++ ) {
			xrtClearError();
			testRequire(
				testExtensionCompile(
					pRegistry,
					xrtStrView(arrUndefined[i])
				) == NULL,
				"undefined template extension was accepted"
			);
			testExtensionError(
				XTEMPLATE_ERROR_UNDEFINED,
				"undefined template extension error mismatch"
			);
		}
	}
	{
		xtemplateconfig Config;

		xrtTemplateConfigInit(&Config);
		Config.Registry = pRegistry;
		Config.MaxArguments = 1u;
		xrtClearError();
		testRequire(
			xrtTemplateCompileConfig(
				XRT_STR_LITERAL("{@echo:'a'}{@echo:'b'}"),
				&Config
			) == NULL,
			"template total argument budget was ignored"
		);
		testExtensionError(
			XTEMPLATE_ERROR_LIMIT,
			"template total argument limit error mismatch"
		);
		Config.MaxArguments = XTEMPLATE_ARGUMENTS_DEFAULT;
		Config.MaxCallArguments = 1u;
		xrtClearError();
		testRequire(
			xrtTemplateCompileConfig(
				XRT_STR_LITERAL("{@echo:'a':suffix='b'}"),
				&Config
			) == NULL,
			"template call argument budget was ignored"
		);
		testExtensionError(
			XTEMPLATE_ERROR_LIMIT,
			"template call argument limit error mismatch"
		);
	}
	xrtTemplateRegistryRelease(pRegistry);
}



/* 验证回调原因链、共享输出/步骤预算和循环控制边界。 */
static void testExtensionRenderBoundaries(void)
{
	testextensionstate State = { 0, NULL, NULL };
	xtemplateregistry* pRegistry = testExtensionBoundaryRegistry(&State);
	xtemplate* pFail = testExtensionCompile(
		pRegistry,
		XRT_STR_LITERAL("{@fail}")
	);
	xtemplate* pOutput = testExtensionCompile(
		pRegistry,
		XRT_STR_LITERAL("{@echo:'long':suffix='!'}")
	);
	xtemplate* pSteps = testExtensionCompile(
		pRegistry,
		XRT_STR_LITERAL("{#repeat:2}x{#end}")
	);
	xtemplate* pFlow = testExtensionCompile(
		pRegistry,
		XRT_STR_LITERAL("{#for:1:1}{#repeat:1}{#break}{#end}{#end}")
	);
	xtemplaterenderconfig Render;

	testRequire(
		(pFail != NULL) && (pOutput != NULL) &&
		(pSteps != NULL) && (pFlow != NULL),
		"template extension render boundary compile failed"
	);
	xrtTemplateRenderConfigInit(&Render);
	xrtClearError();
	testRequire(
		!xrtTemplateWrite(
			pFail,
			&Render,
			testExtensionDiscard,
			NULL
		),
		"failing template extension callback was accepted"
	);
	testExtensionError(
		XTEMPLATE_ERROR_CALLBACK,
		"template extension callback error mismatch"
	);
	testRequire(
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(xrtGetError())),
			"test.template.extension"
		) == 0),
		"template extension callback cause was not preserved"
	);

	Render.MaxOutputBytes = 2u;
	xrtClearError();
	testRequire(
		!xrtTemplateWrite(
			pOutput,
			&Render,
			testExtensionDiscard,
			NULL
		),
		"template extension output budget was ignored"
	);
	testExtensionError(
		XTEMPLATE_ERROR_LIMIT,
		"template extension output limit error mismatch"
	);

	xrtTemplateRenderConfigInit(&Render);
	Render.MaxSteps = 2u;
	xrtClearError();
	testRequire(
		!xrtTemplateWrite(
			pSteps,
			&Render,
			testExtensionDiscard,
			NULL
		),
		"template extension repeated body bypassed step budget"
	);
	testExtensionError(
		XTEMPLATE_ERROR_LIMIT,
		"template extension step limit error mismatch"
	);

	xrtTemplateRenderConfigInit(&Render);
	xrtClearError();
	testRequire(
		!xrtTemplateWrite(
			pFlow,
			&Render,
			testExtensionDiscard,
			NULL
		),
		"template loop control crossed an extension boundary"
	);
	testExtensionError(
		XTEMPLATE_ERROR_SYNTAX,
		"template extension flow boundary error mismatch"
	);
	xrtTemplateRelease(pFlow);
	xrtTemplateRelease(pSteps);
	xrtTemplateRelease(pOutput);
	xrtTemplateRelease(pFail);
	xrtTemplateRegistryRelease(pRegistry);
}



/* 运行模板扩展层的功能、所有权、错误和预算测试。 */
int main(void)
{
	testExtensionModes();
	testExtensionRegistry();
	testExtensionCompileBoundaries();
	testExtensionRenderBoundaries();
	printf("[PASS] template extension\n");
	return 0;
}
