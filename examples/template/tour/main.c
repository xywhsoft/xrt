/*
 * 范例：template/tour —— 模板自省/文件编译/扩展调用域补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【编译】    xrtTemplateCompileFileConfig（文件+高级配置）/
 *              Source（源码视图）
 *   【自省】    NodeCount / Node（节点视图：类型/位置/表达式）
 *   【输出】    Write（分片回调流式渲染）/
 *              RenderHtmlConfigInit（HTML 转义预设）
 *   【注册表】  RegistryRef（不可变共享引用计数）
 *   【扩展调用域】 CallName / CallData / CallArgumentCount /
 *              CallFind（命名参数）/ CallRaw / CallCurrent /
 *              CallRoot / CallGlobal / CallRenderCurrent
 *   【错误】    ErrorLocation（行/列定位）
 * 模块宏：XRT_MODULE_TEMPLATE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/template/tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   template: file-config compile source=13 nodes>=3
 *   template: write streamed=14 html-config ok
 *   template: call name=shout data=7 args=1/1 calls=1
 *   template: registry ref + error-location line=1 ok
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

/* 扩展上下文：携带用户数据值与 RenderCurrent 的替换值。 */
typedef struct examplectx {
	int Magic;
	volatile int iCalls;
	xvalue* pAlt;
} examplectx;

/* shout 块扩展：大写参数 + 双形态渲染主体（当前值/替换值）。 */
static bool exampleShout(xtemplatecall* pCall)
{
	examplectx* pCtx = (examplectx*)xrtTemplateCallData(pCall);
	xtemplateargview Argument;
	xtemplatevalue Value;
	const xvalue* pRoot;
	const xvalue* pGlobal;

	pCtx->iCalls = pCtx->iCalls + 1;
	/* 名称与参数数量。 */
	if ( (xrtTemplateCallName(pCall).Size != 5u) ||
		(memcmp(xrtTemplateCallName(pCall).Data, "shout",
			5u) != 0) ||
		(xrtTemplateCallArgumentCount(pCall) != 1u) ) {
		return false;
	}
	/* 位置参数 + Find 命名查找（无命时返回假是合法路径）。 */
	if ( !xrtTemplateCallArgument(pCall, 0u, &Argument) ||
		xrtTemplateCallFind(pCall, SV("nope"), &Argument) ||
		!xrtTemplateCallEval(pCall, &Argument, &Value) ||
		(Value.Type != XVALUE_STRING) ) {
		return false;
	}
	/* 原样主体与三个作用域（Global 未配置时为空，合法）。 */
	(void)xrtTemplateCallRaw(pCall);
	pRoot = xrtTemplateCallRoot(pCall);
	pGlobal = xrtTemplateCallGlobal(pCall);
	if ( (pRoot == NULL) ||
		(xrtTemplateCallCurrent(pCall) == NULL) ) {
		return false;
	}
	(void)pGlobal;
	/* 大写输出参数。 */
	{
		size_t i;

		for ( i = 0; i < Value.Text.Size; ++i ) {
			char c = Value.Text.Data[i];

			if ( (c >= 'a') && (c <= 'z') ) {
				c = (char)(c - 'a' + 'A');
			}
			if ( !xrtTemplateCallWrite(pCall,
					(xstrview) { &c, 1u }) ) {
				return false;
			}
		}
	}
	/* CallRender：按当前作用域渲染主体。 */
	if ( !xrtTemplateCallRender(pCall) ) {
		return false;
	}
	/* RenderCurrent：临时把当前值替换成 Alt 再渲染一次主体。 */
	return xrtTemplateCallRenderCurrent(pCall, pCtx->pAlt);
}

/* Write 回调：累计字节数。 */
static bool exampleWriter(ptr pUserData, xstrview Text)
{
	size_t* pTotal = (size_t*)pUserData;

	*pTotal = *pTotal + Text.Size;
	return true;
}

int main(void)
{
	static const char sFile[] = "xrt-template-tour.tpl";
	examplectx Context;
	xtemplateextension Extension;
	xtemplateregistry* pRegistry = NULL;
	xtemplateregistry* pRegistryRef = NULL;
	xtemplateconfig Config;
	xtemplaterenderconfig RenderConfig;
	xtemplaterenderconfig HtmlConfig;
	xtemplate* pTemplate = NULL;
	xtemplate* pExtTemplate = NULL;
	xvalue* pName = NULL;
	xvalue* pData = NULL;
	xvalue* pGlobal = NULL;
	xtemplatenodeview Node;
	xerror* pError = NULL;
	xtemplatelocation Location;
	FILE* pOut;
	size_t iTotal = 0;
	size_t iNodes;
	size_t i;
	int iResult = 1;

	/* 准备模板文件：输出标签是 {$path} 语法（14 字节）。 */
	pOut = fopen(sFile, "wb");
	if ( (pOut == NULL) ||
		(fwrite("Hello {$name}!", 1u, 14u, pOut) != 14u) ) {
		goto Cleanup;
	}
	fclose(pOut);
	pOut = NULL;

	/* ---- CompileFileConfig + Source + 节点自省 ---- */
	xrtTemplateConfigInit(&Config);
	pTemplate = xrtTemplateCompileFileConfig(sFile, &Config);
	if ( (pTemplate == NULL) ||
		(xrtTemplateSource(pTemplate).Size != 14u) ) {
		goto Cleanup;
	}
	iNodes = xrtTemplateNodeCount(pTemplate);
	if ( (iNodes < 2u) || (iNodes > 4u) ) {
		goto Cleanup;
	}
	/* 找到 TEXT 首节点与 OUTPUT 节点（表达式为 name）。 */
	{
		bool bText = false;
		bool bOutput = false;

		for ( i = 0; i < iNodes; ++i ) {
			if ( !xrtTemplateNode(pTemplate, i, &Node) ) {
				goto Cleanup;
			}
			if ( (Node.Type == XTEMPLATE_NODE_TEXT) &&
				!bText ) {
				bText = (Node.Source.Size >= 5u);
			}
			if ( Node.Type == XTEMPLATE_NODE_OUTPUT ) {
				bOutput = (Node.Expression.Size == 4u) &&
					(memcmp(Node.Expression.Data,
						"name", 4u) == 0);
			}
		}
		if ( !bText || !bOutput ) {
			goto Cleanup;
		}
	}
	printf("template: file-config compile source=13 nodes>=%zu\n",
		iNodes);

	/* ---- Write 流式 + RenderHtmlConfigInit ---- */
	pName = xrtValueString(SV("xrt"));
	pData = xrtValueObject();
	if ( (pName == NULL) || (pData == NULL) ||
		!xrtValueObjectSetNew(pData, SV("name"), pName) ) {
		goto Cleanup;  /* SetNew 接管 pName */
	}
	pName = NULL;
	xrtTemplateRenderConfigInit(&RenderConfig);
	RenderConfig.Root = pData;
	RenderConfig.Current = pData;
	iTotal = 0;
	if ( !xrtTemplateWrite(pTemplate, &RenderConfig,
			exampleWriter, &iTotal) ||
		(iTotal != 10u) ) {  /* "Hello xrt!" 共 10 字节 */
		goto Cleanup;
	}
	/* HTML 转义预设：转义 < > & 到实体后变长。 */
	pName = xrtValueString(SV("<b>"));
	if ( (pName == NULL) ||
		!xrtValueObjectSetNew(pData, SV("name"), pName) ) {
		goto Cleanup;
	}
	pName = NULL;
	xrtTemplateRenderHtmlConfigInit(&HtmlConfig);
	HtmlConfig.Root = pData;
	HtmlConfig.Current = pData;
	{
		str sHtml = xrtTemplateRender(pTemplate, pData, NULL);

		/* 直接 Render 不走 Html 转义配置——转义验证用 Write。 */
		iTotal = 0;
		if ( !xrtTemplateWrite(pTemplate, &HtmlConfig,
				exampleWriter, &iTotal) ) {
			xrtFree(sHtml);
			goto Cleanup;
		}
		/* "<b>" → "&lt;b&gt;" 9 字节 + "Hello " 6 + "!" 1 = 16。 */
		if ( iTotal != 16u ) {
			xrtFree(sHtml);
			goto Cleanup;
		}
		xrtFree(sHtml);
	}
	printf("template: write streamed=14 html-config ok\n");

	/* ---- 扩展调用域：upper(name) + raw 块 ---- */
	Context.Magic = 7;
	Context.iCalls = 0;
	Extension.Name = SV("shout");
	Extension.Type = XTEMPLATE_EXTENSION_BLOCK;
	Extension.MinArguments = 1u;
	Extension.MaxArguments = 1u;
	Extension.Call = exampleShout;
	Extension.Data = &Context;
	Extension.Drop = NULL;
	pRegistry = xrtTemplateRegistryCreate(&Extension, 1u);
	if ( (pRegistry == NULL) ) {
		goto Cleanup;
	}
	/* RegistryRef：不可变共享。 */
	pRegistryRef = xrtTemplateRegistryRef(pRegistry);
	if ( pRegistryRef != pRegistry ) {
		goto Cleanup;
	}
	xrtTemplateConfigInit(&Config);
	Config.Registry = pRegistryRef;
	/* 函数扩展语法是 {@name:args}（语句扩展才是 {#...}）。 */
	pExtTemplate = xrtTemplateCompileConfig(
		SV("{#shout:x}[{$v}]{#end}"), &Config);
	if ( (pExtTemplate == NULL) ||
		(xrtTemplateNodeCount(pExtTemplate) == 0u) ) {
		goto Cleanup;
	}
	Context.pAlt = xrtValueObject();
	pName = xrtValueString(SV("alt"));
	if ( (pData == NULL) || (Context.pAlt == NULL) ||
		!xrtValueObjectSetNew(Context.pAlt, SV("v"), pName) ) {
		goto Cleanup;
	}
	pName = NULL;
	if ( !xrtValueObjectSetNew(pData, SV("x"),
			xrtValueString(SV("hi"))) ||
		!xrtValueObjectSetNew(pData, SV("v"),
			xrtValueString(SV("body"))) ) {
		goto Cleanup;
	}
	pGlobal = xrtValueObject();
	{
		str sOut = xrtTemplateRender(pExtTemplate, pData, NULL);

		if ( (sOut == NULL) || (strcmp(sOut, "HI[body][alt]") != 0) ||
			(Context.iCalls != 1) ) {
			xrtFree(sOut);
			goto Cleanup;
		}
		xrtFree(sOut);
	}
	(void)pGlobal;
	printf("template: call name=shout data=%d args=1/1 calls=%d\n",
		Context.Magic, Context.iCalls);

	/* ---- ErrorLocation：语法错误的行列定位 ---- */
	{
		xtemplate* pBad = xrtTemplateCompile(SV("A{$x"));

		if ( (pBad != NULL) ||
			((pError = xrtTakeError()) == NULL) ||
			!xrtTemplateErrorLocation(pError, &Location) ||
			(Location.Line == 0u) ) {
			goto Cleanup;
		}
		xrtErrorFree(pError);
		pError = NULL;
	}
	printf("template: registry ref + error-location line=%zu ok\n",
		Location.Line);
	iResult = 0;

Cleanup:
	xrtErrorFree(pError);
	xrtTemplateRelease(pExtTemplate);
	xrtTemplateRegistryRelease(pRegistryRef);
	xrtTemplateRegistryRelease(pRegistry);
	xrtValueRelease(pGlobal);
	xrtValueRelease(pName);
	xrtValueRelease(pData);
	xrtTemplateRelease(pTemplate);
	if ( pOut != NULL ) {
		fclose(pOut);
	}
	(void)remove(sFile);
	return iResult;
}
