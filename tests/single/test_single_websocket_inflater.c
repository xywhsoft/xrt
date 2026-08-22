#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



typedef struct test_single_ws_inflater {
	char Data[32];
	size_t Size;
} test_single_ws_inflater;



/* 收集单头 Inflater 的语义输出。 */
static bool testSingleWsInflaterOutput(
	xbytesview Data,
	ptr pData
)
{
	test_single_ws_inflater* pOutput =
		(test_single_ws_inflater*)pData;

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



/* 单头发布必须保留流式 permessage-deflate 接收运行时。 */
int main(void)
{
	static const uint8 Encoded[] = {
		0xF2, 0x48, 0xCD, 0xC9, 0xC9, 0x57, 0x28, 0x48,
		0x2D, 0xCA, 0x4D, 0x2D, 0x2E, 0x4E, 0x4C, 0x4F,
		0xD5, 0x4D, 0x49, 0x4D, 0xCB, 0x49, 0x2C, 0x49,
		0x05, 0x00
	};
	test_single_ws_inflater Output;
	xwsinflater* pInflater;

	memset(&Output, 0, sizeof(Output));
	pInflater = xrtWsInflaterCreate(NULL);
	if ( (pInflater == NULL) ||
		!xrtWsInflaterBegin(pInflater, true) ||
		!xrtWsInflaterWrite(
			pInflater,
			(xbytesview){ Encoded, sizeof(Encoded) },
			testSingleWsInflaterOutput,
			&Output
		) ||
		!xrtWsInflaterEnd(
			pInflater,
			testSingleWsInflaterOutput,
			&Output
		) ||
		(Output.Size != 24u) ||
		(memcmp(
			Output.Data,
			"Hello permessage-deflate",
			Output.Size
		 ) != 0) ) {
		xrtWsInflaterDestroy(pInflater);
		return 1;
	}
	xrtWsInflaterDestroy(pInflater);
	return 0;
}
