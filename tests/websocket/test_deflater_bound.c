#include "../test.h"



/* 输出计数器不保存数据，用于验证任意压缩配置的线路长度上界。 */
typedef struct test_ws_deflater_count {
	size_t Size;
} test_ws_deflater_count;



/* 累计 Deflater 交付的线路字节。 */
static bool testWsDeflaterCountOutput(
	xbytesview Data,
	ptr pData
)
{
	test_ws_deflater_count* pCount =
		(test_ws_deflater_count*)pData;

	if ( Data.Size > (SIZE_MAX - pCount->Size) ) {
		return false;
	}
	pCount->Size += Data.Size;
	return true;
}



/* 生成确定性的不可压缩输入，确保输出上界面对最差数据仍然成立。 */
static void testWsDeflaterFill(
	uint8* pData,
	size_t iSize
)
{
	uint32 iState = UINT32_C(0x6d2b79f5);
	size_t i;

	for ( i = 0; i < iSize; i++ ) {
		iState ^= iState << 13u;
		iState ^= iState >> 17u;
		iState ^= iState << 5u;
		pData[i] = (uint8)iState;
	}
}



/* 验证一次中间刷新和最终刷新都不会越过公开的硬上界。 */
static void testWsDeflaterBoundCase(
	const xwsdeflaterconfig* pConfig,
	xbytesview Input
)
{
	test_ws_deflater_count Count;
	xwsdeflater* pDeflater;
	size_t iBefore;
	size_t iBound;

	memset(&Count, 0, sizeof(Count));
	pDeflater = xrtWsDeflaterCreate(pConfig);
	testRequire(
		(pDeflater != NULL) &&
		xrtWsDeflaterBound(Input.Size, &iBound) &&
		xrtWsDeflaterBegin(pDeflater, true),
		"WebSocket Deflater bound fixture failed"
	);

	iBefore = Count.Size;
	testRequire(
		xrtWsDeflaterWrite(
			pDeflater,
			Input,
			testWsDeflaterCountOutput,
			&Count
		) &&
		xrtWsDeflaterFlush(
			pDeflater,
			testWsDeflaterCountOutput,
			&Count
		) &&
		((Count.Size - iBefore) <= iBound),
		"WebSocket Deflater intermediate output exceeded bound"
	);

	iBefore = Count.Size;
	testRequire(
		xrtWsDeflaterWrite(
			pDeflater,
			Input,
			testWsDeflaterCountOutput,
			&Count
		) &&
		xrtWsDeflaterEnd(
			pDeflater,
			testWsDeflaterCountOutput,
			&Count
		) &&
		((Count.Size - iBefore) <= iBound),
		"WebSocket Deflater final output exceeded bound"
	);
	xrtWsDeflaterDestroy(pDeflater);
}



/* 穷举公开压缩级别、策略和窗口端点，并覆盖关键长度边界。 */
int main(void)
{
	static const size_t Sizes[] = {
		0u, 1u, 7u, 63u, 255u,
		1024u, 16384u, 65535u, 131071u
	};
	static const uint8 WindowBits[] = {
		XWS_DEFLATE_WINDOW_MIN,
		XWS_DEFLATE_WINDOW_MAX
	};
	static uint8 Data[131071];
	xwsdeflaterconfig Config;
	size_t iSize;
	size_t iWindow;
	int32 iLevel;
	int32 iStrategy;

	testWsDeflaterFill(Data, sizeof(Data));
	for ( iWindow = 0;
		iWindow < (sizeof(WindowBits) / sizeof(WindowBits[0]));
		iWindow++ ) {
		for ( iLevel = 0; iLevel <= 10; iLevel++ ) {
			for ( iStrategy = XDEFLATE_STRATEGY_DEFAULT;
				iStrategy <= XDEFLATE_STRATEGY_FIXED;
				iStrategy++ ) {
				xrtWsDeflaterConfigInit(&Config);
				Config.Level = iLevel;
				Config.Strategy =
					(xdeflatestrategy)iStrategy;
				Config.WindowBits = WindowBits[iWindow];
				for ( iSize = 0;
					iSize < (sizeof(Sizes) / sizeof(Sizes[0]));
					iSize++ ) {
					testWsDeflaterBoundCase(
						&Config,
						(xbytesview){
							Data,
							Sizes[iSize]
						}
					);
				}
			}
		}
	}

	printf("[PASS] websocket_deflater_bound\n");
	return 0;
}
