#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 示例解析器只借用模板，命中时返回一个由渲染器接管的新引用。 */
static bool exampleTemplateResolve(
	ptr pUserData,
	xstrview Name,
	xtemplate** pTemplate
)
{
	xtemplate* pExternal = (xtemplate*)pUserData;
	static const char sName[] = "suffix";

	if ( (pTemplate == NULL) || (pExternal == NULL) ) {
		return false;
	}
	if ( (Name.Size == (sizeof(sName) - 1u)) &&
		(memcmp(Name.Data, sName, Name.Size) == 0) ) {
		*pTemplate = xrtTemplateRef(pExternal);
	}
	return true;
}



/* 展示前向本地定义、外部 include 与共享数据作用域。 */
int main(void)
{
	int iResult = 1;
	xtemplate* pTemplate = NULL;
	xtemplate* pExternal = NULL;
	xvalue* pRoot = NULL;
	xvalue* pUsers = NULL;
	xtemplaterenderconfig Render;
	xstrbuf Output;

	/* 根模板的本地定义允许前向引用，外部名称交给 Resolver。 */
	pTemplate = xrtTemplateCompile(XRT_STR_LITERAL(
		"Users: {#foreach:users}{#include:'user'}"
		"{?loop.last::,}{#end}{#include:'suffix'}"
		"{#define:'user'}{$name}{#end}"
	));
	pExternal = xrtTemplateCompile(XRT_STR_LITERAL(" (external)"));
	pRoot = xrtValueObject();
	pUsers = xrtValueArray();
	if ( (pTemplate == NULL) || (pExternal == NULL) ||
		(pRoot == NULL) || (pUsers == NULL) ) {
		goto cleanup;
	}

	/* 构造两个对象项，并把数组所有权转移给根值。 */
	for ( size_t i = 0; i < 2u; i++ ) {
		xvalue* pUser = xrtValueObject();

		if ( pUser == NULL ) {
			goto cleanup;
		}
		if ( !xrtValueObjectSetNew(
			pUser,
			XRT_STR_LITERAL("name"),
			xrtValueString(i == 0
				? XRT_STR_LITERAL("Alice")
				: XRT_STR_LITERAL("Bob"))
		) ) {
			xrtValueRelease(pUser);
			goto cleanup;
		}
		if ( !xrtValueArrayAppendNew(pUsers, pUser) ) {
			goto cleanup;
		}
	}
	if ( !xrtValueObjectSetTake(
		pRoot,
		XRT_STR_LITERAL("users"),
		&pUsers
	) ) {
		goto cleanup;
	}

	/* 每次渲染独立配置外部解析器，并事务写入字符串构建器。 */
	xrtTemplateRenderConfigInit(&Render);
	Render.Root = pRoot;
	Render.Current = pRoot;
	Render.Resolve = exampleTemplateResolve;
	Render.ResolveData = pExternal;
	xrtStrBufInit(&Output);
	if ( !xrtTemplateRenderTo(pTemplate, &Render, &Output) ) {
		xrtStrBufFree(&Output);
		goto cleanup;
	}
	printf("%.*s\n", (int)Output.Size, Output.Data);
	xrtStrBufFree(&Output);
	iResult = 0;

cleanup:
	/* 模板、数据和未转移数组引用都由调用方显式释放。 */
	xrtValueRelease(pUsers);
	xrtValueRelease(pRoot);
	xrtTemplateRelease(pExternal);
	xrtTemplateRelease(pTemplate);
	return iResult;
}
