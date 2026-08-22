#include "../test.h"

#include <xrt/http_via.h>



/* 生成稳定伪随机字节。 */
static uint32 testViaRandom(uint32* pState)
{
	*pState = (*pState * UINT32_C(1664525)) +
		UINT32_C(1013904223);
	return *pState;
}



/* 随机普通注释必须完成 writer、parser、decoder 字节闭环。 */
int main(void)
{
	static const char Alphabet[] =
		"abc XYZ012()\\,.;";
	char sComment[64];
	char sWire[192];
	char sDecoded[64];
	xhttpviavalue Value;
	xhttpviacursor Cursor;
	xhttpvia Via;
	uint32 iState = UINT32_C(0x5A17C0DE);
	size_t iDecoded;
	size_t iWire;
	size_t iSize;
	size_t i;

	memset(&Value, 0, sizeof(Value));
	Value.ProtocolVersion = XRT_STR_LITERAL("1.1");
	Value.Pseudonym = XRT_STR_LITERAL("edge");
	Value.Flags = XHTTP_VIA_HAS_COMMENT;
	for ( i = 0; i < 6000u; i++ ) {
		iSize = (size_t)(testViaRandom(&iState) %
			(sizeof(sComment) + 1u));
		for ( iDecoded = 0; iDecoded < iSize; iDecoded++ ) {
			sComment[iDecoded] = Alphabet[
				testViaRandom(&iState) %
				(sizeof(Alphabet) - 1u)
			];
		}
		Value.Comment = (xstrview){ sComment, iSize };
		testRequire(
			xrtHttpViaElementWrite(
				&Value, sWire, sizeof(sWire), &iWire
			),
			"random Via comment write failed"
		);
		xrtHttpViaCursorInit(&Cursor);
		testRequire(
			(xrtHttpViaNext(
				(xstrview){ sWire, iWire }, &Cursor, &Via
			) == XHTTP_NEXT_ITEM) &&
			xrtHttpViaCommentDecode(
				Via.Comment,
				sDecoded,
				sizeof(sDecoded),
				&iDecoded
			) && (iDecoded == iSize) &&
			((iSize == 0) ||
			 (memcmp(sDecoded, sComment, iSize) == 0)),
			"random Via comment roundtrip mismatch"
		);
	}
	printf("[PASS] http_via_mutation\n");
	return 0;
}
