#include "../test.h"



/* 外部模板夹具按显式长度名称返回一个新的模板引用。 */
typedef struct testtemplateresolver {
	xstrview FirstName;
	xtemplate* First;
	xstrview SecondName;
	xtemplate* Second;
	size_t Calls;
	bool Fail;
} testtemplateresolver;



/* 比较两个无零结尾依赖的模板名称。 */
static bool testTemplateNameEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 实现外部模板解析契约，并验证渲染器接管返回的引用。 */
static bool testTemplateResolve(
	ptr pUserData,
	xstrview Name,
	xtemplate** pTemplate
)
{
	testtemplateresolver* pResolver =
		(testtemplateresolver*)pUserData;

	pResolver->Calls++;
	if ( pResolver->Fail ) {
		xerror* pError = xrtErrorCreate(
			XERR_IO,
			"test.template.resolve",
			7,
			"template resolver fixture failed"
		);

		xrtSetError(pError);
		xrtErrorFree(pError);
		*pTemplate = pResolver->First != NULL
			? xrtTemplateRef(pResolver->First) : NULL;
		return false;
	}
	if ( testTemplateNameEqual(Name, pResolver->FirstName) ) {
		*pTemplate = xrtTemplateRef(pResolver->First);
	} else if ( testTemplateNameEqual(Name, pResolver->SecondName) ) {
		*pTemplate = xrtTemplateRef(pResolver->Second);
	}
	return true;
}



/* 丢弃输出以便只验证错误和生命周期路径。 */
static bool testTemplateDiscard(ptr pUserData, xstrview Text)
{
	(void)pUserData;
	(void)Text;
	return true;
}



/* 要求当前错误属于模板域并具有指定代码。 */
static void testTemplateComposeError(
	xtemplateerror Code,
	cstr sMessage
)
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



/* 使用显式配置渲染并要求完整字节结果一致。 */
static void testTemplateComposeRender(
	const xtemplate* pTemplate,
	const xtemplaterenderconfig* pConfig,
	xstrview Expected,
	cstr sMessage
)
{
	xstrbuf Output;
	bool bRendered;

	xrtStrBufInit(&Output);
	bRendered = xrtTemplateRenderTo(pTemplate, pConfig, &Output);
	if ( !bRendered ) {
		const xerror* pError = xrtGetError();

		fprintf(
			stderr,
			"template render failed: code=%d operation=%s message=%s\n",
			pError != NULL ? (int)xrtErrorCode(pError) : 0,
			(pError != NULL) && (xrtErrorOperation(pError) != NULL)
				? xrtErrorOperation(pError) : "",
			(pError != NULL) && (xrtErrorMessage(pError) != NULL)
				? xrtErrorMessage(pError) : ""
		);
	}
	testRequire(bRendered, sMessage);
	if ( (Output.Size != Expected.Size) ||
		 (memcmp(Output.Data, Expected.Data, Expected.Size) != 0) ) {
		fprintf(
			stderr,
			"template output mismatch: actual=[%.*s] expected=[%.*s]\n",
			(int)Output.Size,
			Output.Data,
			(int)Expected.Size,
			Expected.Data
		);
	}
	testRequire(
		(Output.Size == Expected.Size) &&
		(memcmp(Output.Data, Expected.Data, Expected.Size) == 0),
		sMessage
	);
	xrtStrBufFree(&Output);
}



/* 验证本地定义前向引用、动态名称、控制节点和本地优先级。 */
static void testTemplateLocalDefinitions(void)
{
	static const char sSource[] =
		"A{#include:part}{#define:'a\\:b'}wrong{#end}"
		"{#define:'row'}{#if:active}{$name}{#else}off{#end}{#end}"
		"{#include:'row'}B";
	xvalue* pRoot = xrtValueObject();
	xtemplate* pTemplate = xrtTemplateCompile(
		(xstrview){ sSource, sizeof(sSource) - 1u }
	);
	testtemplateresolver Resolver;
	xtemplaterenderconfig Config;
	xtemplatenodeview Node;

	testRequire(
		(pRoot != NULL) && (pTemplate != NULL),
		"local template definition fixture failed"
	);
	testRequire(
		xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("part"),
			xrtValueString(XRT_STR_LITERAL("row"))
		) && xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("active"),
			xrtValueBool(true)
		) && xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("name"),
			xrtValueString(XRT_STR_LITERAL("Alice"))
		),
		"local template definition data failed"
	);
	memset(&Resolver, 0, sizeof(Resolver));
	Resolver.FirstName = XRT_STR_LITERAL("row");
	Resolver.First = xrtTemplateCompile(XRT_STR_LITERAL("external"));
	testRequire(
		Resolver.First != NULL,
		"local precedence external fixture failed"
	);
	xrtTemplateRenderConfigInit(&Config);
	Config.Root = pRoot;
	Config.Current = pRoot;
	Config.Resolve = testTemplateResolve;
	Config.ResolveData = &Resolver;
	testTemplateComposeRender(
		pTemplate,
		&Config,
		XRT_STR_LITERAL("AAliceAliceB"),
		"local template definition render mismatch"
	);
	testRequire(
		Resolver.Calls == 0,
		"external resolver bypassed a local definition"
	);
	testRequire(
		xrtTemplateNode(pTemplate, 2u, &Node) &&
		(Node.Type == XTEMPLATE_NODE_DEFINE) &&
		testTemplateNameEqual(Node.Name, XRT_STR_LITERAL("a:b")),
		"template definition node view mismatch"
	);
	xrtTemplateRelease(Resolver.First);
	xrtTemplateRelease(pTemplate);
	xrtValueRelease(pRoot);
}



/* 验证 raw 块完整保留模板语法字节而不执行其中内容。 */
static void testTemplateRaw(void)
{
	static const char sExpected[] =
		"A{$name}{#if:true}X{{#end}B";
	xtemplate* pTemplate = xrtTemplateCompile(XRT_STR_LITERAL(
		"A{#raw}{$name}{#if:true}X{{#end}{#end}B"
	));
	xtemplaterenderconfig Config;

	testRequire(pTemplate != NULL, "template raw compile failed");
	xrtTemplateRenderConfigInit(&Config);
	testTemplateComposeRender(
		pTemplate,
		&Config,
		(xstrview){ sExpected, sizeof(sExpected) - 1u },
		"template raw render mismatch"
	);
	xrtTemplateRelease(pTemplate);
}



/* 验证外部模板解析、当前作用域传递以及缺失和回调失败错误。 */
static void testTemplateExternal(void)
{
	xtemplate* pRoot = xrtTemplateCompile(
		XRT_STR_LITERAL("A{#include:'page'}B")
	);
	xtemplate* pPage = xrtTemplateCompile(
		XRT_STR_LITERAL("[{$name}]")
	);
	xvalue* pData = xrtValueObject();
	testtemplateresolver Resolver;
	xtemplaterenderconfig Config;

	testRequire(
		(pRoot != NULL) && (pPage != NULL) && (pData != NULL),
		"external template fixture failed"
	);
	testRequire(
		xrtValueObjectSetNew(
			pData,
			XRT_STR_LITERAL("name"),
			xrtValueString(XRT_STR_LITERAL("Alice"))
		),
		"external template data failed"
	);
	memset(&Resolver, 0, sizeof(Resolver));
	Resolver.FirstName = XRT_STR_LITERAL("page");
	Resolver.First = pPage;
	xrtTemplateRenderConfigInit(&Config);
	Config.Root = pData;
	Config.Current = pData;
	Config.Resolve = testTemplateResolve;
	Config.ResolveData = &Resolver;
	testTemplateComposeRender(
		pRoot,
		&Config,
		XRT_STR_LITERAL("A[Alice]B"),
		"external template render mismatch"
	);
	testRequire(Resolver.Calls == 1u, "external resolver call mismatch");

	xrtClearError();
	Resolver.FirstName = XRT_STR_LITERAL("other");
	testRequire(
		!xrtTemplateWrite(pRoot, &Config, testTemplateDiscard, NULL),
		"missing external template was accepted"
	);
	testTemplateComposeError(
		XTEMPLATE_ERROR_INCLUDE,
		"missing external template error mismatch"
	);

	xrtClearError();
	Resolver.Fail = true;
	testRequire(
		!xrtTemplateWrite(pRoot, &Config, testTemplateDiscard, NULL),
		"failing external resolver was accepted"
	);
	testTemplateComposeError(
		XTEMPLATE_ERROR_CALLBACK,
		"external resolver callback error mismatch"
	);
	testRequire(
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(xrtGetError())),
			"test.template.resolve"
		) == 0),
		"external resolver cause chain was not preserved"
	);
	xrtTemplateRelease(pPage);
	xrtTemplateRelease(pRoot);
	xrtValueRelease(pData);
}



/* 验证本地递归、跨模板递归与 include 深度都被硬限制。 */
static void testTemplateCyclesAndDepth(void)
{
	xtemplate* pLocal = xrtTemplateCompile(XRT_STR_LITERAL(
		"{#define:'a'}{#include:'a'}{#end}{#include:'a'}"
	));
	xtemplate* pA = xrtTemplateCompile(XRT_STR_LITERAL("{#include:'b'}"));
	xtemplate* pB = xrtTemplateCompile(XRT_STR_LITERAL("{#include:'a'}"));
	testtemplateresolver Resolver;
	xtemplaterenderconfig Config;

	testRequire(
		(pLocal != NULL) && (pA != NULL) && (pB != NULL),
		"template cycle fixtures failed"
	);
	xrtTemplateRenderConfigInit(&Config);
	xrtClearError();
	testRequire(
		!xrtTemplateWrite(pLocal, &Config, testTemplateDiscard, NULL),
		"local template include cycle was accepted"
	);
	testTemplateComposeError(
		XTEMPLATE_ERROR_CYCLE,
		"local template cycle error mismatch"
	);

	memset(&Resolver, 0, sizeof(Resolver));
	Resolver.FirstName = XRT_STR_LITERAL("a");
	Resolver.First = pA;
	Resolver.SecondName = XRT_STR_LITERAL("b");
	Resolver.Second = pB;
	Config.Resolve = testTemplateResolve;
	Config.ResolveData = &Resolver;
	xrtClearError();
	testRequire(
		!xrtTemplateWrite(pA, &Config, testTemplateDiscard, NULL),
		"external template include cycle was accepted"
	);
	testTemplateComposeError(
		XTEMPLATE_ERROR_CYCLE,
		"external template cycle error mismatch"
	);

	Config.MaxIncludeDepth = 1u;
	xrtClearError();
	testRequire(
		!xrtTemplateWrite(pA, &Config, testTemplateDiscard, NULL),
		"template include depth limit was ignored"
	);
	testTemplateComposeError(
		XTEMPLATE_ERROR_LIMIT,
		"template include depth error mismatch"
	);
	xrtTemplateRelease(pB);
	xrtTemplateRelease(pA);
	xrtTemplateRelease(pLocal);
}



/* 验证定义语法和重复名称在编译期稳定失败。 */
static void testTemplateComposeSyntax(void)
{
	const cstr arrInvalid[] = {
		"{#define:name}x{#end}",
		"{#define:''}x{#end}",
		"{#define:'a'}x{#end}{#define:'a'}y{#end}",
		"{#define:'a'}x{#else}y{#end}",
		"{#include:}",
		"{#raw}x"
	};

	for ( size_t i = 0; i <
		(sizeof(arrInvalid) / sizeof(arrInvalid[0])); i++ ) {
		xtemplate* pTemplate;

		xrtClearError();
		pTemplate = xrtTemplateCompile(xrtStrView(arrInvalid[i]));
		testRequire(
			pTemplate == NULL,
			"invalid template composition syntax was accepted"
		);
		testTemplateComposeError(
			XTEMPLATE_ERROR_SYNTAX,
			"template composition syntax error mismatch"
		);
	}
}



/* 运行模板组合层的功能、所有权和边界测试。 */
int main(void)
{
	testTemplateLocalDefinitions();
	testTemplateRaw();
	testTemplateExternal();
	testTemplateCyclesAndDepth();
	testTemplateComposeSyntax();
	printf("[PASS] template compose\n");
	return 0;
}
