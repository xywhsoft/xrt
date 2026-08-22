#include "../test.h"



/* 写入测试夹具使用的 16 位网络序整数。 */
static void testTlsPskWrite16(uint8* pData, uint16 iValue)
{
	pData[0] = (uint8)(iValue >> 8u);
	pData[1] = (uint8)iValue;
}



/* 写入测试夹具使用的 32 位网络序整数。 */
static void testTlsPskWrite32(uint8* pData, uint32 iValue)
{
	pData[0] = (uint8)(iValue >> 24u);
	pData[1] = (uint8)(iValue >> 16u);
	pData[2] = (uint8)(iValue >> 8u);
	pData[3] = (uint8)iValue;
}



/* 手工构造两项 OfferedPsks，避免 parser 由同一 writer 自证。 */
static size_t testTlsPskFixture(uint8* pData, size_t iCapacity)
{
	size_t iOffset = 0;

	testRequire(iCapacity >= 102u, "TLS PSK fixture buffer is too small");
	testTlsPskWrite16(pData + iOffset, 16u);
	iOffset += 2u;
	testTlsPskWrite16(pData + iOffset, 3u);
	memcpy(pData + iOffset + 2u, "one", 3u);
	testTlsPskWrite32(pData + iOffset + 5u, UINT32_C(0x01020304));
	iOffset += 9u;
	testTlsPskWrite16(pData + iOffset, 1u);
	pData[iOffset + 2u] = 0xA5;
	testTlsPskWrite32(pData + iOffset + 3u, UINT32_C(0xF0E0D0C0));
	iOffset += 7u;
	testTlsPskWrite16(pData + iOffset, 82u);
	iOffset += 2u;
	pData[iOffset++] = 32u;
	memset(pData + iOffset, 0x11, 32u);
	iOffset += 32u;
	pData[iOffset++] = 48u;
	memset(pData + iOffset, 0x22, 48u);
	iOffset += 48u;
	return iOffset;
}



/* 严格 parser 必须同步发布每项 identity、年龄和 binder。 */
static void testTlsPskParse(void)
{
	uint8 Data[102];
	uint8 ModesData[] = { 2u, XTLS_PSK_DHE_KE, XTLS_PSK_KE };
	uint8 ServerData[] = { 0x12, 0x34 };
	xtlspskcursor Cursor;
	xtlspsk Psk;
	xbytesview Modes;
	uint16 iSelected;
	size_t iSize = testTlsPskFixture(Data, sizeof(Data));

	testRequire((iSize == sizeof(Data)) && xrtTlsPskModes(
		(xbytesview) { ModesData, sizeof(ModesData) }, &Modes
	) && (Modes.Size == 2u) && (Modes.Data[0] == XTLS_PSK_DHE_KE),
		"TLS PSK mode parsing failed");
	testRequire(xrtTlsClientPsks(
		(xbytesview) { Data, sizeof(Data) }, &Cursor
	), "TLS OfferedPsks parsing failed");
	testRequire((xrtTlsPsksRead(&Cursor, &Psk) == XTLS_ITEM_VALUE) &&
		(Psk.Identity.Size == 3u) &&
		(memcmp(Psk.Identity.Data, "one", 3u) == 0) &&
		(Psk.ObfuscatedAge == UINT32_C(0x01020304)) &&
		(Psk.Binder.Size == 32u) && (Psk.Binder.Data[0] == 0x11),
		"TLS first PSK entry mismatch");
	testRequire((xrtTlsPsksRead(&Cursor, &Psk) == XTLS_ITEM_VALUE) &&
		(Psk.Identity.Size == 1u) && (Psk.Identity.Data[0] == 0xA5) &&
		(Psk.ObfuscatedAge == UINT32_C(0xF0E0D0C0)) &&
		(Psk.Binder.Size == 48u) && (Psk.Binder.Data[47] == 0x22) &&
		(xrtTlsPsksRead(&Cursor, &Psk) == XTLS_ITEM_DONE),
		"TLS second PSK entry or completion mismatch");
	testRequire(xrtTlsServerPsk(
		(xbytesview) { ServerData, sizeof(ServerData) }, &iSelected
	) && (iSelected == UINT16_C(0x1234)),
		"TLS server PSK selection parsing failed");
}



/* 畸形向量必须失败且不得覆盖调用方游标。 */
static void testTlsPskReject(void)
{
	uint8 Data[102];
	uint8 DuplicateModes[] = { 2u, 1u, 1u };
	xtlspskcursor Cursor;
	xtlspskcursor Before;
	xtlspsk Psk;
	xtlspsk BeforePsk;
	xbytesview Modes;

	(void)testTlsPskFixture(Data, sizeof(Data));
	memset(&Cursor, 0xA5, sizeof(Cursor));
	Before = Cursor;
	testTlsPskWrite16(Data, 6u);
	testRequire(!xrtTlsClientPsks(
		(xbytesview) { Data, sizeof(Data) }, &Cursor
	) && (memcmp(&Cursor, &Before, sizeof(Cursor)) == 0),
		"TLS PSK parser accepted a short identities vector or changed output");
	(void)testTlsPskFixture(Data, sizeof(Data));
	Data[20] = 31u;
	testRequire(!xrtTlsClientPsks(
		(xbytesview) { Data, sizeof(Data) }, &Cursor
	), "TLS PSK parser accepted a short binder");
	testRequire(!xrtTlsPskModes(
		(xbytesview) { DuplicateModes, sizeof(DuplicateModes) }, &Modes
	), "TLS PSK parser accepted duplicate modes");
	testRequire(!xrtTlsServerPsk(
		(xbytesview) { Data, 1u }, (uint16*)&Data[4]
	), "TLS server PSK parser accepted a truncated selection");
	(void)testTlsPskFixture(Data, sizeof(Data));
	testRequire(xrtTlsClientPsks(
		(xbytesview) { Data, sizeof(Data) }, &Cursor
	), "TLS PSK tampered-cursor setup failed");
	Cursor.BinderOffset = Cursor.Binders.Size;
	memset(&Psk, 0x5A, sizeof(Psk));
	BeforePsk = Psk;
	testRequire((xrtTlsPsksRead(&Cursor, &Psk) == XTLS_ITEM_ERROR) &&
		(memcmp(&Psk, &BeforePsk, sizeof(Psk)) == 0),
		"TLS PSK reader accepted uneven cursor vectors");
}



/* 执行 PSK 线路解析回归。 */
int main(void)
{
	testTlsPskParse();
	testTlsPskReject();
	return 0;
}
