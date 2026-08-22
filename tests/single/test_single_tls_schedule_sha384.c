#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的 SHA-384 transcript。 */
int main(void)
{
	static const uint8 Expected[] =
		"\x0d\xab\x11\x7c\xf0\x0c\x04\x43\xd0\x18\xeb\x60\x85\xff\x55\x7f"
		"\x60\x97\xe7\xae\xa1\x6c\x6a\xf9\x3c\x68\x38\x7c\xc6\x34\x00\xb5"
		"\x13\x06\x75\xa8\xc1\xbf\xd1\x1e\x7d\x32\x77\x2a\x2b\x7d\x64\x59";
	xtlstranscript Transcript;
	uint8 Digest[48];

	if ( !__xrtTlsTranscriptInit(
		&Transcript, XCRYPTO_HASH_SHA384
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
