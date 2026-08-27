#include "../test.h"



/* 要求当前错误属于模板域并具有指定代码。 */
static void testTemplateControlError(
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



/* 创建覆盖条件、数组遍历和对象遍历的输入数据。 */
static xvalue* testTemplateControlData(void)
{
	xvalue* pRoot = xrtValueObject();
	xvalue* pUser = xrtValueObject();
	xvalue* pUsers = xrtValueArray();
	xvalue* pPairs = xrtValueObject();
	const cstr arrNames[] = { "Alice", "Skip", "Bob", "After" };

	testRequire(
		(pRoot != NULL) && (pUser != NULL) &&
		(pUsers != NULL) && (pPairs != NULL),
		"template control fixture allocation failed"
	);
	testRequire(
		xrtValueObjectSetNew(
			pUser,
			XRT_STR_LITERAL("name"),
			xrtValueString(XRT_STR_LITERAL("Alice"))
		),
		"template control user setup failed"
	);
	for ( size_t i = 0; i < 4u; i++ ) {
		xvalue* pItem = xrtValueObject();

		testRequire(pItem != NULL, "template control item allocation failed");
		testRequire(
			xrtValueObjectSetNew(
				pItem,
				XRT_STR_LITERAL("name"),
				xrtValueString((xstrview){ arrNames[i], strlen(arrNames[i]) })
			) && xrtValueObjectSetNew(
				pItem,
				XRT_STR_LITERAL("skip"),
				xrtValueBool(i == 1u)
			) && xrtValueObjectSetNew(
				pItem,
				XRT_STR_LITERAL("stop"),
				xrtValueBool(i == 2u)
			) && xrtValueArrayAppendNew(pUsers, pItem),
			"template control item setup failed"
		);
	}
	testRequire(
		xrtValueObjectSetNew(
			pPairs,
			XRT_STR_LITERAL("first"),
			xrtValueString(XRT_STR_LITERAL("A"))
		) && xrtValueObjectSetNew(
			pPairs,
			XRT_STR_LITERAL("second"),
			xrtValueString(XRT_STR_LITERAL("B"))
		) && xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("active"),
			xrtValueBool(true)
		) && xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("big"),
			xrtValueInt(INT64_C(9007199254740993))
		) && xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("max"),
			xrtValueUInt(UINT64_MAX)
		) && xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("user"),
			pUser
		) && xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("users"),
			pUsers
		) && xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("pairs"),
			pPairs
		),
		"template control root setup failed"
	);
	return pRoot;
}



/* 编译并渲染模板，要求结果与完整显式长度文本一致。 */
static void testTemplateControlRender(
	xstrview Source,
	const xvalue* pData,
	xstrview Expected,
	cstr sMessage
)
{
	xtemplate* pTemplate = xrtTemplateCompile(Source);
	str sResult;
	size_t iSize;

	testRequire(pTemplate != NULL, sMessage);
	sResult = xrtTemplateRender(pTemplate, pData, &iSize);
	testRequire(sResult != NULL, sMessage);
	testRequire(
		(iSize == Expected.Size) &&
		(memcmp(sResult, Expected.Data, Expected.Size) == 0),
		sMessage
	);
	xrtFree(sResult);
	xrtTemplateRelease(pTemplate);
}



/* 验证条件分支、短路、括号、字符串和大整数精确比较。 */
static void testTemplateConditions(void)
{
	xvalue* pData = testTemplateControlData();

	testTemplateControlRender(
		XRT_STR_LITERAL(
			"{#if:(big = 9007199254740993) and active}A"
			"{#elseif:false}X{#else}B{#end}|"
			"{#if:big = 9007199254740992}X{#else}C{#end}|"
			"{#if:max > 9223372036854775807}U{#end}|"
			"{#if:max < 18446744073709551616.0}V{#end}|"
			"{#if:max = 18446744073709551615.0}X{#else}W{#end}|"
			"{#if:not false and (missing or true)}D{#end}|"
			"{?user.name = 'Alice':OK:NO}|"
			"{#if:true}O{#if:false}X{#else}I{#end}Z"
			"{#else}N{#end}|{?true:left\\:right:no}"
		),
		pData,
		XRT_STR_LITERAL("A|C|U|V|W|D|OK|OIZ|left:right"),
		"template condition render mismatch"
	);
	xrtValueRelease(pData);
}



/* 验证闭区间范围、自动方向、步长以及循环元数据。 */
static void testTemplateFor(void)
{
	testTemplateControlRender(
		XRT_STR_LITERAL(
			"{#for:1:3}{%loop.value}:{%loop.index}:"
			"{?loop.first:Y:N}:{?loop.last:Y:N};{#end}|"
			"{#for:3:1}{%loop.value}{#end}|"
			"{#for:1:5:2}{%loop.value}{#end}|"
			"{#for:1:5}{#if:loop.value = 2}{#continue}{#end}"
			"{#if:loop.value = 4}{#break}{#end}{%loop.value}{#end}"
		),
		NULL,
		XRT_STR_LITERAL(
			"1:0:Y:N;2:1:N:N;3:2:N:Y;|321|135|13"
		),
		"template for render mismatch"
	);
	testTemplateControlRender(
		XRT_STR_LITERAL("A{#for:3:1:1}X{#end}B"),
		NULL,
		XRT_STR_LITERAL("AB"),
		"template direction mismatch range was not empty"
	);
}



/* 验证数组当前作用域、对象键值以及 break 和 continue。 */
static void testTemplateForeach(void)
{
	xvalue* pData = testTemplateControlData();

	testTemplateControlRender(
		XRT_STR_LITERAL(
			"{#foreach:users}{#if:skip}{#continue}{#end}"
			"{$name}:{%loop.index}:{%loop.number}:"
			"{?loop.first:Y:N}:{?loop.last:Y:N};"
			"{#if:stop}{#break}{#end}{#end}|"
			"{#foreach:pairs}{$loop.key}={$loop.value};{#end}"
		),
		pData,
		XRT_STR_LITERAL(
			"Alice:0:1:Y:N;Bob:2:3:N:N;|first=A;second=B;"
		),
		"template foreach render mismatch"
	);
	xrtValueRelease(pData);
}



/* 验证非法结构、类型和循环边界都返回稳定模板错误。 */
static void testTemplateControlErrors(void)
{
	const cstr arrSyntax[] = {
		"{#if:true}x",
		"{#else}",
		"{#if:true}x{#else:no}y{#end}",
		"{#for:1}x{#end}",
		"{#foreach:}x{#end}",
		"{#if:(true}x{#end}"
	};

	for ( size_t i = 0; i < (sizeof(arrSyntax) / sizeof(arrSyntax[0])); i++ ) {
		xtemplate* pTemplate;

		xrtClearError();
		pTemplate = xrtTemplateCompile(xrtStrView(arrSyntax[i]));
		testRequire(pTemplate == NULL, "invalid template control syntax accepted");
		testTemplateControlError(
			XTEMPLATE_ERROR_SYNTAX,
			"template control syntax error mismatch"
		);
	}
	{
		const cstr arrRender[] = {
			"{#for:1:3:0}x{#end}",
			"{#foreach:true}x{#end}",
			"{#break}",
			"{#continue}"
		};

		for ( size_t i = 0; i <
			(sizeof(arrRender) / sizeof(arrRender[0])); i++ ) {
			xtemplate* pTemplate = xrtTemplateCompile(
				xrtStrView(arrRender[i])
			);
			str sResult;

			testRequire(pTemplate != NULL, "control error fixture compile failed");
			xrtClearError();
			sResult = xrtTemplateRender(pTemplate, NULL, NULL);
			testRequire(sResult == NULL, "invalid control render was accepted");
			testTemplateControlError(
				(i == 1u) ? XTEMPLATE_ERROR_TYPE :
					(i == 0u ? XTEMPLATE_ERROR_TYPE : XTEMPLATE_ERROR_SYNTAX),
				"template control render error mismatch"
			);
			xrtTemplateRelease(pTemplate);
		}
	}
}



/* 验证块、表达式、渲染深度和循环次数预算都是硬限制。 */
static void testTemplateControlLimits(void)
{
	xtemplateconfig Compile;
	xtemplaterenderconfig Render;
	xtemplate* pTemplate;
	xstrbuf Output;

	xrtTemplateConfigInit(&Compile);
	Compile.MaxBlockDepth = 1u;
	xrtClearError();
	pTemplate = xrtTemplateCompileConfig(
		XRT_STR_LITERAL("{#if:true}{#if:true}x{#end}{#end}"),
		&Compile
	);
	testRequire(pTemplate == NULL, "template block depth limit ignored");
	testTemplateControlError(
		XTEMPLATE_ERROR_LIMIT,
		"template block depth error mismatch"
	);

	xrtTemplateConfigInit(&Compile);
	Compile.MaxExpressionDepth = 1u;
	xrtClearError();
	pTemplate = xrtTemplateCompileConfig(
		XRT_STR_LITERAL("{#if:((true))}x{#end}"),
		&Compile
	);
	testRequire(pTemplate == NULL, "template expression depth limit ignored");
	testTemplateControlError(
		XTEMPLATE_ERROR_LIMIT,
		"template expression depth error mismatch"
	);

	pTemplate = xrtTemplateCompile(
		XRT_STR_LITERAL("{#for:1:4}x{#end}")
	);
	testRequire(pTemplate != NULL, "template loop limit fixture failed");
	xrtTemplateRenderConfigInit(&Render);
	Render.MaxLoopIterations = 3u;
	xrtStrBufInit(&Output);
	xrtClearError();
	testRequire(
		!xrtTemplateRenderTo(pTemplate, &Render, &Output) &&
		(Output.Size == 0u),
		"template loop limit was not transactional"
	);
	testTemplateControlError(
		XTEMPLATE_ERROR_LIMIT,
		"template loop limit error mismatch"
	);
	xrtStrBufFree(&Output);
	xrtTemplateRelease(pTemplate);

	pTemplate = xrtTemplateCompile(
		XRT_STR_LITERAL("{#if:true}x{#end}")
	);
	testRequire(pTemplate != NULL, "template render depth fixture failed");
	xrtTemplateRenderConfigInit(&Render);
	Render.MaxDepth = 1u;
	xrtStrBufInit(&Output);
	xrtClearError();
	testRequire(
		!xrtTemplateRenderTo(pTemplate, &Render, &Output) &&
		(Output.Size == 0u),
		"template render depth limit ignored"
	);
	testTemplateControlError(
		XTEMPLATE_ERROR_LIMIT,
		"template render depth error mismatch"
	);
	xrtStrBufFree(&Output);
	xrtTemplateRelease(pTemplate);
}



/* 运行模板控制层的语义、边界和预算测试。 */
int main(void)
{
	testTemplateConditions();
	testTemplateFor();
	testTemplateForeach();
	testTemplateControlErrors();
	testTemplateControlLimits();
	printf("[PASS] template control\n");
	return 0;
}
