/*
 * 范例：template/extension —— 块扩展：用 C 代码实现自定义模板标签
 * ----------------------------------------------------------------
 * 演示 API：
 *   xtemplateextension / XTEMPLATE_EXTENSION_BLOCK
 *                        扩展描述符（名字/种类/参数与主体约束/回调）
 *   xrtTemplateRegistryCreate / Release
 *                        不可变扩展注册表（可被多个模板共享）
 *   xrtTemplateConfigInit + Config.Registry
 *                        编译配置挂注册表
 *   xrtTemplateCompileConfig   带配置编译
 *   回调侧工具：
 *   xrtTemplateCallArgument  取第 N 个参数（视图）
 *   xrtTemplateCallEval      求值参数为模板值（路径/字面量皆可）
 *   xrtTemplateCallWrite     向输出写片段
 *   xrtTemplateCallRender    渲染块主体（{#link:...}主体...{#end}）
 * 模块宏：XRT_MODULE_TEMPLATE（依赖 VALUE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/template/extension/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   <a href="/docs">Documentation</a>
 *
 * 模板：{#link:url}{$label}{#end}
 *   参数 url 是数据路径 → CallEval 求值为 "/docs"；
 *   主体 {$label} 由 CallRender 渲染 → "Documentation"；
 *   回调自由拼接前后缀，得到完整 <a> 标签。
 * 扩展的种类还有 INLINE（无主体）——见 template.h。
 */

#include <stdio.h>

#include <xrt.h>



/*
 * "link" 块扩展回调：参数 0 = href，主体夹在 <a> 与 </a> 之间。
 * 返回 false 表示扩展失败并终止渲染（参数类型错在本例即失败）。
 */
static bool exampleTemplateLink(xtemplatecall* pCall)
{
	xtemplateargview Argument;
	xtemplatevalue Value;

	if ( !xrtTemplateCallArgument(pCall, 0u, &Argument) ||
		 !xrtTemplateCallEval(pCall, &Argument, &Value) ||
		 (Value.Type != XVALUE_STRING) ) {
		return false;
	}
	return xrtTemplateCallWrite(pCall, XRT_STR_LITERAL("<a href=\"")) &&
		xrtTemplateCallWrite(pCall, Value.Text) &&
		xrtTemplateCallWrite(pCall, XRT_STR_LITERAL("\">")) &&
		xrtTemplateCallRender(pCall) &&
		xrtTemplateCallWrite(pCall, XRT_STR_LITERAL("</a>"));
}



int main(void)
{
	/* 扩展描述符：名字 link、块级、参数恰好 1 个、必须带主体。 */
	xtemplateextension Extension = {
		XRT_STR_LITERAL("link"),
		XTEMPLATE_EXTENSION_BLOCK,
		1u,
		1u,
		exampleTemplateLink,
		NULL,
		NULL
	};
	xtemplateregistry* pRegistry = xrtTemplateRegistryCreate(&Extension, 1u);
	xtemplateconfig Config;
	xtemplate* pTemplate;
	xvalue* pData = xrtValueObject();
	str sOutput;

	if ( (pRegistry == NULL) || (pData == NULL) ) {
		return 1;
	}

	/* 编译配置挂上注册表：模板里的 {#link:...} 由此可解析。 */
	xrtTemplateConfigInit(&Config);
	Config.Registry = pRegistry;
	pTemplate = xrtTemplateCompileConfig(
		XRT_STR_LITERAL("{#link:url}{$label}{#end}"),
		&Config
	);
	xrtTemplateRegistryRelease(pRegistry);
	if ( (pTemplate == NULL) || !xrtValueObjectSetNew(
		pData,
		XRT_STR_LITERAL("url"),
		xrtValueString(XRT_STR_LITERAL("/docs"))
	) || !xrtValueObjectSetNew(
		pData,
		XRT_STR_LITERAL("label"),
		xrtValueString(XRT_STR_LITERAL("Documentation"))
	) ) {
		return 2;
	}
	sOutput = xrtTemplateRender(pTemplate, pData, NULL);
	if ( sOutput == NULL ) {
		return 3;
	}
	printf("%s\n", sOutput);
	xrtFree(sOutput);
	xrtValueRelease(pData);
	xrtTemplateRelease(pTemplate);
	return 0;
}
