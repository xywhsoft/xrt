#include "../test.h"



/* PSK writer 必须生成公共 parser 可严格消费的完整扩展。 */
static void testTlsPskWriteRoundTrip(void)
{
	uint8 Buffer[256];
	uint8 Binder1[32];
	uint8 Binder2[48];
	uint8 Modes[] = { XTLS_PSK_DHE_KE };
	xtlspsk Psks[2];
	xtlswriter Writer;
	xtlsextensioncursor Extensions;
	xtlsextension Extension;
	xtlspskcursor Cursor;
	xtlspsk Parsed;
	xbytesview ParsedModes;

	memset(Binder1, 0x31, sizeof(Binder1));
	memset(Binder2, 0x42, sizeof(Binder2));
	Psks[0] = (xtlspsk) {
		XRT_BYTES_LITERAL("ticket-one"), UINT32_C(0x01020304),
		(xbytesview) { Binder1, sizeof(Binder1) }
	};
	Psks[1] = (xtlspsk) {
		XRT_BYTES_LITERAL("two"), UINT32_C(0xA0B0C0D0),
		(xbytesview) { Binder2, sizeof(Binder2) }
	};
	testRequire(xrtTlsWriterInit(&Writer, Buffer, sizeof(Buffer)) &&
		xrtTlsWriterPskModes(&Writer, Modes, 1u) &&
		xrtTlsWriterClientPsks(&Writer, Psks, 2u) &&
		xrtTlsExtensionsInit(&Extensions, xrtTlsWriterData(&Writer)) &&
		(xrtTlsExtensionsRead(&Extensions, &Extension) == XTLS_ITEM_VALUE) &&
		(Extension.Type == XTLS_EXTENSION_PSK_KEY_EXCHANGE_MODES) &&
		xrtTlsPskModes(Extension.Data, &ParsedModes) &&
		(ParsedModes.Size == 1u) &&
		(xrtTlsExtensionsRead(&Extensions, &Extension) == XTLS_ITEM_VALUE) &&
		(Extension.Type == XTLS_EXTENSION_PRE_SHARED_KEY) &&
		xrtTlsClientPsks(Extension.Data, &Cursor) &&
		(xrtTlsPsksRead(&Cursor, &Parsed) == XTLS_ITEM_VALUE) &&
		(Parsed.Identity.Size == sizeof("ticket-one") - 1u) &&
		(Parsed.Binder.Size == sizeof(Binder1)) &&
		(xrtTlsPsksRead(&Cursor, &Parsed) == XTLS_ITEM_VALUE) &&
		(Parsed.ObfuscatedAge == UINT32_C(0xA0B0C0D0)) &&
		(Parsed.Binder.Size == sizeof(Binder2)) &&
		(xrtTlsPsksRead(&Cursor, &Parsed) == XTLS_ITEM_DONE) &&
		(xrtTlsExtensionsRead(&Extensions, &Extension) == XTLS_ITEM_DONE),
		"TLS PSK writer round trip failed");
}



/* PSK writer 的容量、重复和重叠失败必须保持逻辑长度不变。 */
static void testTlsPskWriteAtomicity(void)
{
	uint8 Buffer[96];
	uint8 Binder[32] = { 0 };
	uint8 DuplicateModes[] = { 1u, 1u };
	xtlspsk Psk = {
		XRT_BYTES_LITERAL("ticket"), 0,
		(xbytesview) { Binder, sizeof(Binder) }
	};
	xtlswriter Writer;
	size_t iSize;

	testRequire(xrtTlsWriterInit(&Writer, Buffer, sizeof(Buffer)) &&
		xrtTlsWriterExtension(
			&Writer, XTLS_EXTENSION_PADDING,
			(xbytesview) { NULL, 0 }
		), "TLS PSK atomicity setup failed");
	iSize = Writer.Size;
	testRequire(!xrtTlsWriterPskModes(
		&Writer, DuplicateModes, sizeof(DuplicateModes)
	) && (Writer.Size == iSize),
		"TLS PSK writer accepted duplicate modes or changed its size");
	testRequire(xrtTlsWriterReset(&Writer),
		"TLS PSK writer reset failed");
	Psk.Identity = (xbytesview) { Buffer, 6u };
	testRequire(!xrtTlsWriterClientPsks(&Writer, &Psk, 1u) &&
		(Writer.Size == 0),
		"TLS PSK writer accepted overlapping identity input");
	Psk.Identity = XRT_BYTES_LITERAL("ticket");
	testRequire(xrtTlsWriterInit(&Writer, Buffer, 8u) &&
		!xrtTlsWriterClientPsks(&Writer, &Psk, 1u) &&
		(Writer.Size == 0),
		"TLS PSK writer modified an undersized destination");
}



/* ServerHello writer 必须只编码一个 16 位选择索引。 */
static void testTlsServerPskWrite(void)
{
	uint8 Buffer[16];
	xtlswriter Writer;
	xtlsextension Extension;
	uint16 iSelected;

	testRequire(xrtTlsWriterInit(&Writer, Buffer, sizeof(Buffer)) &&
		xrtTlsWriterServerPsk(&Writer, UINT16_C(0xBEEF)) &&
		(xrtTlsExtensionParse(
			xrtTlsWriterData(&Writer), &Extension, NULL
		) == XTLS_OK) &&
		(Extension.Type == XTLS_EXTENSION_PRE_SHARED_KEY) &&
		xrtTlsServerPsk(Extension.Data, &iSelected) &&
		(iSelected == UINT16_C(0xBEEF)),
		"TLS server PSK writer round trip failed");
}



/* 执行 PSK writer 回归。 */
int main(void)
{
	testTlsPskWriteRoundTrip();
	testTlsPskWriteAtomicity();
	testTlsServerPskWrite();
	return 0;
}
