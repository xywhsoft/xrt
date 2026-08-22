#include <stdio.h>

#include <xrt.h>



/* 编码并解析一条可由自定义可靠传输承载的 TLS 记录。 */
int main(void)
{
	uint8 Buffer[64];
	xtlsrecord Record;
	const xtlscipherinfo* pInfo;

	pInfo = xrtTlsCipherInfo(XTLS_AES_128_GCM_SHA256);
	if ( pInfo == NULL ) {
		return 1;
	}

	if ( !xrtTlsRecordEncode(
		XTLS_RECORD_APPLICATION_DATA,
		UINT16_C(0x0303),
		XRT_BYTES_LITERAL("example"),
		Buffer,
		sizeof(Buffer)
	) ) {
		return 1;
	}
	if ( xrtTlsRecordParse(
		(xbytesview) { Buffer, 12 }, &Record, NULL
	) != XTLS_OK ) {
		return 1;
	}
	printf(
		"type=%s legacy=0x%04x payload=%zu cipher=%s key=%u\n",
		xrtTlsRecordName(Record.Type),
		(unsigned)Record.LegacyVersion,
		Record.Payload.Size,
		xrtTlsCipherName(pInfo->Cipher),
		(unsigned)pInfo->KeySize
	);
	return 0;
}
