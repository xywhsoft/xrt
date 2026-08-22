#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的握手和扩展 framing。 */
int main(void)
{
	uint8 Buffer[32];
	xtlshandshake Handshake;
	xtlsextension Extension;

	if ( !xrtTlsHandshakeEncode(
		XTLS_HANDSHAKE_CLIENT_HELLO,
		XRT_BYTES_LITERAL("hello"), Buffer, sizeof(Buffer)
	) || (xrtTlsHandshakeParse(
		(xbytesview) { Buffer, 9u }, &Handshake, NULL
	) != XTLS_OK) || (Handshake.Body.Size != 5u) ||
		 !xrtTlsExtensionEncode(
			XTLS_EXTENSION_SUPPORTED_VERSIONS,
			XRT_BYTES_LITERAL("\x03\x04"), Buffer, sizeof(Buffer)
		) || (xrtTlsExtensionParse(
			(xbytesview) { Buffer, 6u }, &Extension, NULL
		) != XTLS_OK) || (Extension.Data.Size != 2u) ) {
		return 1;
	}
	return 0;
}
