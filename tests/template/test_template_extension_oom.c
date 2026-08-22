#include "../test.h"

#include "../test_fault_allocator.h"



/* OOM 函数扩展求值字符串参数并直接写入共享输出。 */
static bool testTemplateExtensionOomEcho(xtemplatecall* pCall)
{
	xtemplateargview Argument;
	xtemplatevalue Value;

	return xrtTemplateCallArgument(pCall, 0u, &Argument) &&
		xrtTemplateCallEval(pCall, &Argument, &Value) &&
		(Value.Type == XVALUE_STRING) &&
		xrtTemplateCallWrite(pCall, Value.Text);
}



/* OOM 块扩展按有限次数重复渲染预编译主体。 */
static bool testTemplateExtensionOomRepeat(xtemplatecall* pCall)
{
	xtemplateargview Argument;
	xtemplatevalue Value;

	if ( !xrtTemplateCallArgument(pCall, 0u, &Argument) ||
		 !xrtTemplateCallEval(pCall, &Argument, &Value) ||
		 (Value.Type != XVALUE_INT) ||
		 (Value.Integer < 0) || (Value.Integer > 32) ) {
		return false;
	}
	for ( int64 i = 0; i < Value.Integer; i++ ) {
		if ( !xrtTemplateCallRender(pCall) ) {
			return false;
		}
	}
	return true;
}



/* 执行覆盖注册表、参数表达式、块回调和输出扩容的完整路径。 */
static bool testTemplateExtensionOomAttempt(void)
{
	static const char sPrefix[] =
		"{#repeat:16}[{@echo:name}]{#end}";
	xtemplateextension arrExtensions[] = {
		{ XRT_STR_LITERAL("echo"), XTEMPLATE_EXTENSION_FUNCTION,
			1u, 1u, testTemplateExtensionOomEcho, NULL, NULL },
		{ XRT_STR_LITERAL("repeat"), XTEMPLATE_EXTENSION_BLOCK,
			1u, 1u, testTemplateExtensionOomRepeat, NULL, NULL }
	};
	char arrSource[8192];
	xtemplateregistry* pRegistry = NULL;
	xtemplate* pTemplate = NULL;
	xvalue* pRoot = NULL;
	str sOutput = NULL;
	bool bComplete = false;

	memcpy(arrSource, sPrefix, sizeof(sPrefix) - 1u);
	memset(
		arrSource + sizeof(sPrefix) - 1u,
		'x',
		sizeof(arrSource) - (sizeof(sPrefix) - 1u)
	);
	pRegistry = xrtTemplateRegistryCreate(
		arrExtensions,
		sizeof(arrExtensions) / sizeof(arrExtensions[0])
	);
	if ( pRegistry == NULL ) {
		goto cleanup;
	}
	{
		xtemplateconfig Config;

		xrtTemplateConfigInit(&Config);
		Config.Registry = pRegistry;
		pTemplate = xrtTemplateCompileConfig(
			(xstrview){ arrSource, sizeof(arrSource) },
			&Config
		);
	}
	if ( pTemplate == NULL ) {
		goto cleanup;
	}
	pRoot = xrtValueObject();
	if ( (pRoot == NULL) || !xrtValueObjectSetNew(
		pRoot,
		XRT_STR_LITERAL("name"),
		xrtValueString(XRT_STR_LITERAL("Alice"))
	) ) {
		goto cleanup;
	}
	sOutput = xrtTemplateRender(pTemplate, pRoot, NULL);
	bComplete = sOutput != NULL;

cleanup:
	xrtFree(sOutput);
	xrtValueRelease(pRoot);
	xrtTemplateRelease(pTemplate);
	xrtTemplateRegistryRelease(pRegistry);
	xrtClearError();
	return bComplete;
}



/* 扫描扩展层稳定分配点并验证失败清理与后续恢复。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);
	size_t iBaseline;
	size_t iCalls;

	testRequire(
		xrtSetAllocator(&Allocator),
		"template extension OOM allocator install failed"
	);
	testRequire(
		testTemplateExtensionOomAttempt(),
		"template extension OOM warm-up failed"
	);
	testMemoryDebugDrain("template extension OOM memory debug reset failed");
	iBaseline = State.Live;

	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(
		testTemplateExtensionOomAttempt(),
		"template extension OOM baseline failed"
	);
	iCalls = State.Calls;
	testRequire(iCalls != 0, "template extension reached no allocation");
	testMemoryDebugDrain("template extension OOM baseline reset failed");
	testRequire(
		State.Live == iBaseline,
		"template extension OOM baseline leaked storage"
	);

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testTemplateExtensionOomAttempt(),
			"template extension unexpectedly survived injected OOM"
		);
		testRequire(
			State.Hit,
			"template extension OOM target was not reached"
		);
		testMemoryDebugDrain(
			"template extension OOM memory debug reset failed"
		);
		testRequire(
			State.Live == iBaseline,
			"template extension OOM path leaked storage"
		);
	}

	State.FailAt = SIZE_MAX;
	testRequire(
		testTemplateExtensionOomAttempt(),
		"template extension did not recover after OOM"
	);
	testMemoryDebugDrain("template extension OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "template extension recovery leaked storage");
	printf(
		"[PASS] template extension OOM (%zu allocation points)\n",
		iCalls
	);
	return 0;
}
