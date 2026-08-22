#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件可完成 TLS 1.3 ChaCha20-Poly1305 记录往返。 */
int main(void)
{
	uint8 KeyData[32] = { 0 };
	uint8 Iv[12] = { 0 };
	uint8 Encoded[64];
	uint8 Plain[16];
	size_t Written = 0;
	xtlsrecordtype Type = XTLS_RECORD_ALERT;
	xtlsrecord Record;
	xtlsrecordkey Send = { 0 };
	xtlsrecordkey Receive = { 0 };

	if ( !__xrtTlsRecordKeyInit(
		&Send, XTLS_VERSION_13, XTLS_CHACHA20_POLY1305_SHA256,
		(xbytesview) { KeyData, sizeof(KeyData) },
		(xbytesview) { Iv, sizeof(Iv) }
	) || !__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_APPLICATION_DATA, XRT_BYTES_LITERAL("hello"),
		0, Encoded, sizeof(Encoded), &Written
	) || (xrtTlsRecordParse(
		(xbytesview) { Encoded, Written }, &Record, NULL
	) != XTLS_OK) || !__xrtTlsRecordKeyInit(
		&Receive, XTLS_VERSION_13, XTLS_CHACHA20_POLY1305_SHA256,
		(xbytesview) { KeyData, sizeof(KeyData) },
		(xbytesview) { Iv, sizeof(Iv) }
	) || !__xrtTlsRecordOpen(
		&Receive, &Record, Plain, sizeof(Plain), &Type, &Written
	) || (Type != XTLS_RECORD_APPLICATION_DATA) || (Written != 5u) ||
		(memcmp(Plain, "hello", 5u) != 0) ) {
		return 1;
	}
	return 0;
}
