#include <xrt/http_decode.h>

#include <stdio.h>



/* 把解码片段直接写到标准输出，实际网络代码可改为业务消费回调。 */
static bool printBody(xbytesview Data, ptr pData)
{
	FILE* pOutput = (FILE*)pData;

	return fwrite(Data.Data, 1, Data.Size, pOutput) == Data.Size;
}



/* 展示从 HTTP/1 Header 建立流式 gzip 解码器。 */
int main(void)
{
	static const uint8 Gzip[] = {
		0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0xFF, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x57,
		0x48, 0xCE, 0xCF, 0x2D, 0x28, 0x4A, 0x2D, 0x2E,
		0x4E, 0x4D, 0x51, 0x28, 0xCF, 0x2F, 0xCA, 0x49,
		0x01, 0x00, 0xA1, 0x2D, 0x94, 0x53, 0x16, 0x00,
		0x00, 0x00
	};
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip")
		}
	};
	xhttpdecode* pDecode = xrtHttpDecodeCreate(Fields, 1, NULL);
	bool bSuccess;

	if ( pDecode == NULL ) {
		return 1;
	}
	bSuccess = xrtHttpDecodeWrite(
		pDecode,
		(xbytesview){ Gzip, sizeof(Gzip) },
		true,
		printBody,
		stdout
	) && xrtHttpDecodeDone(pDecode);
	xrtHttpDecodeDestroy(pDecode);
	return bSuccess ? 0 : 1;
}
