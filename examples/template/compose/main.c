/*
 * 范例：template/compose —— 组合：本地 define、外部 include 与渲染配置
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTemplateRef            增加模板引用（include 解析器返回新引用）
 *   xrtTemplateRenderConfigInit + Render.Root/Current/Resolve
 *                             渲染配置：数据根/当前作用域/外部解析器
 *   xrtTemplateRenderTo       渲染到 xstrbuf（增量追加，可复用缓冲）
 * 模板语法：
 *   {#include:'user'}  引入模板（本地 define 或 Resolver 外部模板）
 *   {#define:'user'}...{#end}  本地定义（允许前向引用）
 * 模块宏：XRT_MODULE_TEMPLATE（依赖 VALUE/STRING）
 * 编译（单头形态，Windows，仓库根目录运行）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/template/compose/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   Users: Alice,Bob (external)
 *
 * include 的两级查找：
 *   1. 本地 {#define}（编译期已知，支持在定义点之前引用）；
 *   2. Resolve 回调（运行期外部来源——文件、缓存、网络皆可）。
 * Resolver 契约：命中时 *pTemplate 必须给出"新引用"
 *   （本例 xrtTemplateRef +1），渲染器用完自行释放；
 *   未命中返回 true 且不写指针，让查找继续/报错。
 */

#include <stdio.h>
#include <string.h>

#include <xrt.h>



/*
 * 外部模板解析器：只认识名字 "suffix"。
 * 借用 pUserData 携带外部模板；命中则交出新引用。
 */
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



int main(void)
{
	int iResult = 1;
	xtemplate* pTemplate = NULL;
	xtemplate* pExternal = NULL;
	xvalue* pRoot = NULL;
	xvalue* pUsers = NULL;
	xtemplaterenderconfig Render;
	xstrbuf Output;

	/*
	 * 根模板：foreach 里 include 'user'（先使用），
	 * 之后才 {#define:'user'} —— 本地定义允许前向引用；
	 * 结尾 include 'suffix' 交给外部 Resolver。
	 */
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

	/* 数据：两个 {name:...} 对象组成的数组，移交根对象。 */
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

	/*
	 * 渲染配置：Root/Current 都指向根数据（include 的模板
	 * 共享同一作用域，{$name} 直接看到循环项字段）；
	 * Resolve 挂外部解析器。
	 */
	xrtTemplateRenderConfigInit(&Render);
	Render.Root = pRoot;
	Render.Current = pRoot;
	Render.Resolve = exampleTemplateResolve;
	Render.ResolveData = pExternal;

	/* RenderTo 追加到构建器：多次渲染可复用同一缓冲拼大输出。 */
	xrtStrBufInit(&Output);
	if ( !xrtTemplateRenderTo(pTemplate, &Render, &Output) ) {
		xrtStrBufFree(&Output);
		goto cleanup;
	}
	printf("%.*s\n", (int)Output.Size, Output.Data);
	xrtStrBufFree(&Output);
	iResult = 0;

cleanup:
	/* 未转移的数组引用与全部模板逐个释放。 */
	xrtValueRelease(pUsers);
	xrtValueRelease(pRoot);
	xrtTemplateRelease(pExternal);
	xrtTemplateRelease(pTemplate);
	return iResult;
}
