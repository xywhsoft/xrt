#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



static const uint8 TestGzip[] = {
	0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0xFF, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x57,
	0x48, 0xCE, 0xCF, 0x2D, 0x28, 0x4A, 0x2D, 0x2E,
	0x4E, 0x4D, 0x51, 0x28, 0xCF, 0x2F, 0xCA, 0x49,
	0x01, 0x00, 0xA1, 0x2D, 0x94, 0x53, 0x16, 0x00,
	0x00, 0x00
};



typedef struct test_output {
	char Data[32];
	size_t Size;
} test_output;



/* 收集单头文件解码结果。 */
static bool testOutput(xbytesview Data, ptr pData)
{
	test_output* pOutput = (test_output*)pData;

	if ( Data.Size > (sizeof(pOutput->Data) - pOutput->Size) ) {
		return false;
	}
	memcpy(pOutput->Data + pOutput->Size, Data.Data, Data.Size);
	pOutput->Size += Data.Size;
	return true;
}



/* 验证单头文件保留流式 gzip 自动解码。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip")
		}
	};
	test_output Output;
	xhttpdecode* pDecode;
	bool bPass;

	memset(&Output, 0, sizeof(Output));
	pDecode = xrtHttpDecodeCreate(Fields, 1, NULL);
	bPass = (pDecode != NULL) && xrtHttpDecodeWrite(
		pDecode,
		(xbytesview){ TestGzip, sizeof(TestGzip) },
		true,
		testOutput,
		&Output
	) && xrtHttpDecodeDone(pDecode) &&
		(Output.Size == 22u) &&
		(memcmp(Output.Data, "hello compressed world", 22u) == 0);
	xrtHttpDecodeDestroy(pDecode);
	printf("%s single-http-decode\n", bPass ? "[PASS]" : "[FAIL]");
	return bPass ? 0 : 1;
}
