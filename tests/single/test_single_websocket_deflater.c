#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



typedef struct test_single_ws_deflater {
	uint8 Data[64];
	size_t Size;
} test_single_ws_deflater;



/* 收集单头 Deflater 的线路输出。 */
static bool testSingleWsDeflaterOutput(
	xbytesview Data,
	ptr pData
)
{
	test_single_ws_deflater* pOutput =
		(test_single_ws_deflater*)pData;

	if ( Data.Size > (sizeof(pOutput->Data) - pOutput->Size) ) {
		return false;
	}
	memcpy(
		pOutput->Data + pOutput->Size,
		Data.Data,
		Data.Size
	);
	pOutput->Size += Data.Size;
	return true;
}



/* 单头发布必须保留流式 permessage-deflate 发送运行时。 */
int main(void)
{
	test_single_ws_deflater Output;
	xwsdeflaterconfig Config;
	xwsdeflater* pDeflater;

	memset(&Output, 0, sizeof(Output));
	xrtWsDeflaterConfigInit(&Config);
	Config.NoContextTakeover = true;
	pDeflater = xrtWsDeflaterCreate(&Config);
	if ( (pDeflater == NULL) ||
		!xrtWsDeflaterBegin(pDeflater, true) ||
		!xrtWsDeflaterWrite(
			pDeflater,
			XRT_BYTES_LITERAL("Hello permessage-deflate"),
			testSingleWsDeflaterOutput,
			&Output
		) ||
		!xrtWsDeflaterEnd(
			pDeflater,
			testSingleWsDeflaterOutput,
			&Output
		) ||
		(Output.Size == 0) ||
		((Output.Size >= 4u) &&
		 (memcmp(
			Output.Data + Output.Size - 4u,
			"\x00\x00\xff\xff",
			4u
		  ) == 0)) ) {
		xrtWsDeflaterDestroy(pDeflater);
		return 1;
	}
	xrtWsDeflaterDestroy(pDeflater);
	return 0;
}
