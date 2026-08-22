#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 构建 JSON 与 HTML 共享的应用数据，不让输出格式反向控制模型。 */
static xvalue* exampleDashboardModel(void)
{
	xvalue* pModel = xrtValueObject();

	if ( (pModel == NULL) || !xrtValueObjectSetNew(
		pModel,
		XRT_STR_LITERAL("title"),
		xrtValueString(XRT_STR_LITERAL("XRT Dashboard"))
	) || !xrtValueObjectSetNew(
		pModel,
		XRT_STR_LITERAL("service"),
		xrtValueString(XRT_STR_LITERAL("xrt"))
	) || !xrtValueObjectSetNew(
		pModel,
		XRT_STR_LITERAL("requests"),
		xrtValueInt(42)
	) ) {
		xrtValueRelease(pModel);
		return NULL;
	}
	return pModel;
}



/* 把已经生成的正文复制进可选 Reply 构建器。 */
static xhttpreply* exampleDashboardReply(
	xstrview ContentType,
	cstr sBody,
	size_t iBodySize
)
{
	xhttpreply* pReply = xrtHttpReplyCreate(
		XHTTP_STATUS_OK
	);

	if ( (pReply == NULL) || !xrtHttpReplySetBytes(
		pReply,
		(xbytesview){ (cbytes)sBody, iBodySize },
		ContentType
	) ) {
		xrtHttpReplyDestroy(pReply);
		return NULL;
	}
	return pReply;
}



/* 输出两条 Reply 的媒体类型和正文长度。 */
static bool exampleDashboardPrint(
	cstr sName,
	const xhttpreply* pReply
)
{
	const xhttpfield* pType = xrtHttpReplyHeader(
		pReply,
		XRT_STR_LITERAL("Content-Type")
	);

	if ( pType == NULL ) {
		return false;
	}
	printf(
		"%s: %.*s, %llu bytes\n",
		sName,
		(int)pType->Value.Size,
		pType->Value.Data,
		(unsigned long long)xrtHttpBodyLength(
			xrtHttpReplyBody(pReply)
		)
	);
	return true;
}



/* 用同一份 Value 分别生成 JSON API 与 HTML 页面响应。 */
int main(void)
{
	xtemplate* pTemplate = xrtTemplateCompile(
		XRT_STR_LITERAL(
			"<h1>{$title}</h1><p>{$service}: {%requests:,d}</p>"
		)
	);
	xvalue* pModel = exampleDashboardModel();
	xhttpreply* pJsonReply = NULL;
	xhttpreply* pHtmlReply = NULL;
	str sJson = NULL;
	str sHtml = NULL;
	size_t iJsonSize = 0;
	int iResult = 1;

	if ( (pTemplate == NULL) || (pModel == NULL) ) {
		goto Cleanup;
	}
	sJson = xrtJsonStringify(pModel, false, &iJsonSize);
	sHtml = xrtTemplateRender(pTemplate, pModel, NULL);
	if ( (sJson == NULL) || (sHtml == NULL) ) {
		goto Cleanup;
	}
	pJsonReply = exampleDashboardReply(
		XRT_STR_LITERAL("application/json; charset=utf-8"),
		sJson,
		iJsonSize
	);
	pHtmlReply = exampleDashboardReply(
		XRT_STR_LITERAL("text/html; charset=utf-8"),
		sHtml,
		strlen(sHtml)
	);
	if ( (pJsonReply == NULL) || (pHtmlReply == NULL) ||
		!exampleDashboardPrint("api", pJsonReply) ||
		!exampleDashboardPrint("page", pHtmlReply) ) {
		goto Cleanup;
	}
	iResult = 0;

Cleanup:
	xrtHttpReplyDestroy(pHtmlReply);
	xrtHttpReplyDestroy(pJsonReply);
	xrtFree(sHtml);
	xrtFree(sJson);
	xrtValueRelease(pModel);
	xrtTemplateRelease(pTemplate);
	return iResult;
}
