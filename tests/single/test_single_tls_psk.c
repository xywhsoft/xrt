#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须同时提供 PSK 严格 parser 与 writer。 */
int main(void)
{
	uint8 Buffer[96];
	uint8 Binder[32] = { 0 };
	uint8 Modes[] = { XTLS_PSK_DHE_KE };
	xtlspsk Psk = {
		XRT_BYTES_LITERAL("ticket"), UINT32_C(7),
		(xbytesview) { Binder, sizeof(Binder) }
	};
	xtlswriter Writer;
	xtlsextensioncursor Extensions;
	xtlsextension Extension;
	xtlspskcursor Cursor;

	return xrtTlsWriterInit(&Writer, Buffer, sizeof(Buffer)) &&
		xrtTlsWriterPskModes(&Writer, Modes, 1u) &&
		xrtTlsWriterClientPsks(&Writer, &Psk, 1u) &&
		xrtTlsExtensionsInit(&Extensions, xrtTlsWriterData(&Writer)) &&
		(xrtTlsExtensionsRead(&Extensions, &Extension) == XTLS_ITEM_VALUE) &&
		(xrtTlsExtensionsRead(&Extensions, &Extension) == XTLS_ITEM_VALUE) &&
		xrtTlsClientPsks(Extension.Data, &Cursor) ? 0 : 1;
}
