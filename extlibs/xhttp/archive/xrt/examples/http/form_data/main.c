#include <xrt.h>

#include <stdio.h>



/* 构建一个包含文本和文件字段的流式 FormData 正文。 */
int main(void)
{
	xformdata* pForm = xrtFormDataCreate(NULL);
	xmultipartboundary Boundary;
	xstrview Filename = XRT_STR_LITERAL("note.txt");
	xhttpbody* pBody;
	char ContentType[128];
	size_t iSize;

	if ( (pForm == NULL) || !xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("----xrt-example"), &Boundary
	) || !xrtFormDataAppendText(
		pForm, XRT_STR_LITERAL("title"), XRT_STR_LITERAL("demo")
	) || !xrtFormDataAppendBytes(
		pForm,
		XRT_STR_LITERAL("file"),
		(xbytesview){ (const uint8*)"hello", 5 },
		&Filename,
		XRT_STR_LITERAL("text/plain")
	) ) {
		xrtFormDataDestroy(pForm);
		return 1;
	}
	pBody = xrtFormDataBody(pForm, &Boundary);
	if ( (pBody == NULL) || !xrtMultipartContentTypeWrite(
		&Boundary,
		ContentType,
		sizeof(ContentType),
		&iSize
	) ) {
		xrtHttpBodyDestroy(pBody);
		xrtFormDataDestroy(pForm);
		return 1;
	}
	printf("%.*s\n", (int)iSize, ContentType);
	printf("parts=%u body=%llu\n",
		(unsigned)xrtFormDataCount(pForm),
		(unsigned long long)xrtHttpBodyLength(pBody));
	xrtHttpBodyDestroy(pBody);
	xrtFormDataDestroy(pForm);
	return 0;
}
