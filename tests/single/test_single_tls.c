#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件中的 TLS 记录编解码。 */
int main(void)
{
	uint8 Buffer[32];
	xtlsrecord Record;
	const xtlscipherinfo* pInfo;

	pInfo = xrtTlsCipherInfo(XTLS_AES_128_GCM_SHA256);
	if ( (pInfo == NULL) || (pInfo->KeySize != 16u) ||
		(pInfo->Version != XTLS_VERSION_13) || !xrtTlsRecordEncode(
		XTLS_RECORD_HANDSHAKE,
		UINT16_C(0x0303),
		XRT_BYTES_LITERAL("hello"),
		Buffer,
		sizeof(Buffer)
	) || (xrtTlsRecordParse(
		(xbytesview) { Buffer, 10 }, &Record, NULL
	) != XTLS_OK) || (Record.Payload.Size != 5u) ) {
		return 1;
	}
	printf("[PASS] single-tls\n");
	return 0;
}
