#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的 SHA-256 transcript 与 Expand-Label。 */
int main(void)
{
	static const uint8 Expected[] =
		"\x50\x99\xc1\x96\x27\x70\x8a\xd2\xd0\xa2\x20\xca\x11\x51\x8a\xcb"
		"\x97\x28\xf7\xdc\xa1\xe4\x18\xac\xef\xbe\x3e\x88\xeb\xf1\xa9\x51";
	xtlstranscript Transcript;
	uint8 Digest[32];

	if ( !__xrtTlsTranscriptInit(
		&Transcript, XCRYPTO_HASH_SHA256
	) || !__xrtTlsTranscriptUpdate(
		&Transcript, XRT_BYTES_LITERAL("client-hello-wire")
	) || !__xrtTlsTranscriptUpdate(
		&Transcript, XRT_BYTES_LITERAL("server-hello-wire")
	) || !__xrtTlsTranscriptDigest(
		&Transcript, Digest, sizeof(Digest)
	) || (memcmp(Digest, Expected, sizeof(Digest)) != 0) ) {
		return 1;
	}
	__xrtTlsTranscriptClear(&Transcript);
	return 0;
}
