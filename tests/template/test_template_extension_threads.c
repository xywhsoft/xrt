#include "../test.h"
#include "../test_thread.h"



#define TEST_TEMPLATE_EXTENSION_THREADS 8u
#define TEST_TEMPLATE_EXTENSION_ROUNDS 2000u



/* 并发夹具只借用发布后不再变化的模板和值。 */
typedef struct testextensionconcurrent {
	const xtemplate* Template;
	const xvalue* Data;
} testextensionconcurrent;



/* 无状态函数扩展输出求值后的字符串参数。 */
static bool testExtensionConcurrentEcho(xtemplatecall* pCall)
{
	xtemplateargview Argument;
	xtemplatevalue Value;

	return xrtTemplateCallArgument(pCall, 0u, &Argument) &&
		xrtTemplateCallEval(pCall, &Argument, &Value) &&
		(Value.Type == XVALUE_STRING) &&
		xrtTemplateCallWrite(pCall, Value.Text);
}



/* 无状态块扩展重复渲染主体，预算保留在线程局部渲染上下文中。 */
static bool testExtensionConcurrentRepeat(xtemplatecall* pCall)
{
	xtemplateargview Argument;
	xtemplatevalue Value;

	if ( !xrtTemplateCallArgument(pCall, 0u, &Argument) ||
		 !xrtTemplateCallEval(pCall, &Argument, &Value) ||
		 (Value.Type != XVALUE_INT) ) {
		return false;
	}
	for ( int64 i = 0; i < Value.Integer; i++ ) {
		if ( !xrtTemplateCallRender(pCall) ) {
			return false;
		}
	}
	return true;
}



/* 反复并发渲染同一模板、注册定义和只读数据。 */
static int testExtensionConcurrentRun(ptr pData)
{
	testextensionconcurrent* pContext =
		(testextensionconcurrent*)pData;
	xtemplaterenderconfig Config;

	xrtTemplateRenderConfigInit(&Config);
	Config.Root = pContext->Data;
	Config.Current = pContext->Data;
	for ( size_t i = 0; i < TEST_TEMPLATE_EXTENSION_ROUNDS; i++ ) {
		xstrbuf Output;

		xrtStrBufInit(&Output);
		if ( !xrtTemplateRenderTo(
			pContext->Template,
			&Config,
			&Output
		) || (Output.Size != 20u) ||
			 (memcmp(Output.Data, "Alice-[Alice][Alice]", 20u) != 0) ) {
			xrtStrBufFree(&Output);
			return 1;
		}
		xrtStrBufFree(&Output);
	}
	return 0;
}



/* 验证不可变注册表和模板可在线程间共享，渲染状态完全隔离。 */
int main(void)
{
	xtemplateextension arrExtensions[] = {
		{ XRT_STR_LITERAL("echo"), XTEMPLATE_EXTENSION_FUNCTION,
			1u, 1u, testExtensionConcurrentEcho, NULL, NULL },
		{ XRT_STR_LITERAL("repeat"), XTEMPLATE_EXTENSION_BLOCK,
			1u, 1u, testExtensionConcurrentRepeat, NULL, NULL }
	};
	xtemplateregistry* pRegistry = xrtTemplateRegistryCreate(
		arrExtensions,
		sizeof(arrExtensions) / sizeof(arrExtensions[0])
	);
	xtemplateconfig Compile;
	xtemplate* pTemplate;
	xvalue* pData = xrtValueObject();
	testextensionconcurrent Context;
	testthread arrThreads[TEST_TEMPLATE_EXTENSION_THREADS];

	testRequire(
		(pRegistry != NULL) && (pData != NULL),
		"concurrent extension fixture allocation failed"
	);
	xrtTemplateConfigInit(&Compile);
	Compile.Registry = pRegistry;
	pTemplate = xrtTemplateCompileConfig(
		XRT_STR_LITERAL("{@echo:name}-{#repeat:2}[{$name}]{#end}"),
		&Compile
	);
	testRequire(pTemplate != NULL, "concurrent extension compile failed");
	xrtTemplateRegistryRelease(pRegistry);
	testRequire(
		xrtValueObjectSetNew(
			pData,
			XRT_STR_LITERAL("name"),
			xrtValueString(XRT_STR_LITERAL("Alice"))
		),
		"concurrent extension data setup failed"
	);
	Context.Template = pTemplate;
	Context.Data = pData;
	for ( size_t i = 0; i < TEST_TEMPLATE_EXTENSION_THREADS; i++ ) {
		arrThreads[i].Proc = testExtensionConcurrentRun;
		arrThreads[i].Data = &Context;
	}
	testThreadsStart(arrThreads, TEST_TEMPLATE_EXTENSION_THREADS);
	testThreadsJoin(arrThreads, TEST_TEMPLATE_EXTENSION_THREADS);
	for ( size_t i = 0; i < TEST_TEMPLATE_EXTENSION_THREADS; i++ ) {
		testRequire(
			arrThreads[i].Result == 0,
			"concurrent template extension render failed"
		);
	}
	xrtValueRelease(pData);
	xrtTemplateRelease(pTemplate);
	printf("[PASS] template extension threads\n");
	return 0;
}
