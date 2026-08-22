#include "../test.h"

#include <xrt/http_decode.h>



/* 扫描双层 gzip 解码器的全部创建分配点并验证失败后没有活动对象。 */
int main(void)
{
	static const xhttpfield GzipTwice[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip, gzip")
		}
	};
	xhttpdecode* pDecode;
	bool bFailure = false;
	bool bSuccess = false;

	for ( uint64 i = 0; i < 128u; i++ ) {
		bool bTriggered;

		xrtClearError();
		testRequire(
			xrtMemDebugFailAfter(i),
			"HTTP decoder OOM injection setup failed"
		);
		pDecode = xrtHttpDecodeCreate(GzipTwice, 1, NULL);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pDecode == NULL ) {
			bFailure = true;
			testRequire(
				bTriggered &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"HTTP decoder OOM error mismatch"
			);
			testMemoryDebugDrain(
				"HTTP decoder OOM leaked a partial decode chain"
			);
			continue;
		}
		bSuccess = true;
		testRequire(
			!bTriggered &&
			(xrtHttpDecodeMode(pDecode) == XHTTP_DECODE_CONTENT),
			"HTTP decoder OOM scan success mismatch"
		);
		xrtHttpDecodeDestroy(pDecode);
		break;
	}
	testRequire(
		bFailure && bSuccess,
		"HTTP decoder OOM scan missed failure or success"
	);
	testMemoryDebugDrain("HTTP decoder OOM final state leaked storage");
	puts("[PASS] http_decode_oom");
	return 0;
}
