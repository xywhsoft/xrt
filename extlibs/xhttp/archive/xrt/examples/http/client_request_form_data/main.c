#include <stdio.h>
#include <xrt.h>



/* 用 FormData 一步设置请求正文，并取得可用于日志或签名的随机 boundary。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/upload")
	);
	xformdata* pForm = xrtFormDataCreate(NULL);
	xmultipartboundary Boundary;
	xstrview Filename = XRT_STR_LITERAL("note.txt");
	bool bPass = (pRequest != NULL) && (pForm != NULL) &&
		xrtFormDataAppendText(
			pForm,
			XRT_STR_LITERAL("title"),
			XRT_STR_LITERAL("demo")
		) && xrtFormDataAppendBytes(
			pForm,
			XRT_STR_LITERAL("file"),
			XRT_BYTES_LITERAL("hello"),
			&Filename,
			XRT_STR_LITERAL("text/plain")
		) && xrtHttpRequestSetFormDataRandom(
			pRequest,
			pForm,
			&Boundary
		);

	if ( bPass ) {
		printf(
			"boundary=%.*s body=%llu\n",
			(int)Boundary.Size,
			Boundary.Data,
			(unsigned long long)xrtHttpBodyLength(
				xrtHttpRequestBody(pRequest)
			)
		);
	}
	xrtFormDataDestroy(pForm);
	xrtHttpRequestDestroy(pRequest);
	return bPass ? 0 : 1;
}
