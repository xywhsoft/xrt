#include "../test.h"
#include "../test_thread.h"



#define TEST_TEMPLATE_THREADS 8u
#define TEST_TEMPLATE_ROUNDS 2000u



/* 并发解析器只借用发布后不再变化的外部模板。 */
typedef struct testtemplateconcurrent {
	const xtemplate* Root;
	xtemplate* External;
	const xvalue* Data;
} testtemplateconcurrent;



/* 每次调用返回一个独立引用，渲染器负责在 include 完成后释放。 */
static bool testTemplateConcurrentResolve(
	ptr pUserData,
	xstrview Name,
	xtemplate** pTemplate
)
{
	testtemplateconcurrent* pContext =
		(testtemplateconcurrent*)pUserData;

	if ( (Name.Size == 8u) &&
		 (memcmp(Name.Data, "external", 8u) == 0) ) {
		*pTemplate = xrtTemplateRef(pContext->External);
	}
	return true;
}



/* 反复渲染同一不可变模板并验证线程局部配置和错误状态互不污染。 */
static int testTemplateConcurrentRun(ptr pData)
{
	testtemplateconcurrent* pContext =
		(testtemplateconcurrent*)pData;
	xtemplaterenderconfig Config;

	xrtTemplateRenderConfigInit(&Config);
	Config.Root = pContext->Data;
	Config.Current = pContext->Data;
	Config.Resolve = testTemplateConcurrentResolve;
	Config.ResolveData = pContext;
	for ( size_t i = 0; i < TEST_TEMPLATE_ROUNDS; i++ ) {
		xstrbuf Output;

		xrtStrBufInit(&Output);
		if ( !xrtTemplateRenderTo(pContext->Root, &Config, &Output) ||
			 (Output.Size != 16u) ||
			 (memcmp(Output.Data, "[Alice]-external", 16u) != 0) ) {
			xrtStrBufFree(&Output);
			return 1;
		}
		xrtStrBufFree(&Output);
	}
	return 0;
}



/* 验证本地定义索引、外部引用和只读作用域都支持并发复用。 */
int main(void)
{
	xtemplate* pRoot = xrtTemplateCompile(XRT_STR_LITERAL(
		"{#define:'local'}[{$name}]{#end}"
		"{#include:'local'}-{#include:'external'}"
	));
	xtemplate* pExternal = xrtTemplateCompile(
		XRT_STR_LITERAL("external")
	);
	xvalue* pData = xrtValueObject();
	testtemplateconcurrent Context;
	testthread arrThreads[TEST_TEMPLATE_THREADS];

	testRequire(
		(pRoot != NULL) && (pExternal != NULL) && (pData != NULL),
		"concurrent template fixture allocation failed"
	);
	testRequire(
		xrtValueObjectSetNew(
			pData,
			XRT_STR_LITERAL("name"),
			xrtValueString(XRT_STR_LITERAL("Alice"))
		),
		"concurrent template data setup failed"
	);
	Context.Root = pRoot;
	Context.External = pExternal;
	Context.Data = pData;
	for ( size_t i = 0; i < TEST_TEMPLATE_THREADS; i++ ) {
		arrThreads[i].Proc = testTemplateConcurrentRun;
		arrThreads[i].Data = &Context;
	}
	testThreadsStart(arrThreads, TEST_TEMPLATE_THREADS);
	testThreadsJoin(arrThreads, TEST_TEMPLATE_THREADS);
	for ( size_t i = 0; i < TEST_TEMPLATE_THREADS; i++ ) {
		testRequire(
			arrThreads[i].Result == 0,
			"concurrent template render failed"
		);
	}
	xrtValueRelease(pData);
	xrtTemplateRelease(pExternal);
	xrtTemplateRelease(pRoot);
	printf("[PASS] template compose threads\n");
	return 0;
}
