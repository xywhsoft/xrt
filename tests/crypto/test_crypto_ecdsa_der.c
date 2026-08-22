#include "../test.h"



/* 验证短长度 DER 的规范编码、长度查询和往返解码。 */
static void testEcdsaDerShort(void)
{
	static const uint8 Expected[] = {
		0x30, 0x07, 0x02, 0x02, 0x00, 0x80, 0x02, 0x01, 0x7F
	};
	uint8 Raw[64] = { 0 };
	uint8 Der[80];
	uint8 Decoded[64];
	size_t iSize = 0;

	Raw[31] = 0x80;
	Raw[63] = 0x7F;
	testRequire(xrtEcdsaDerEncode(Raw, 32, NULL, 0, &iSize) &&
		(iSize == sizeof(Expected)), "ECDSA DER query mismatch");
	testRequire(xrtEcdsaDerEncode(Raw, 32, Der, sizeof(Der), &iSize) &&
		(iSize == sizeof(Expected)) &&
		xrtConstTimeEqual(Der, Expected, sizeof(Expected)),
		"ECDSA DER short encoding mismatch");
	testRequire(xrtEcdsaDerDecode(Der, iSize, Decoded, 32) &&
		xrtConstTimeEqual(Decoded, Raw, sizeof(Raw)),
		"ECDSA DER short round trip mismatch");
}



/* 验证 P-521 宽度所需的 0x81 长长度形式，给后续曲线保留表示能力。 */
static void testEcdsaDerLong(void)
{
	uint8 Raw[132];
	uint8 Der[141];
	uint8 Decoded[132];
	size_t iSize = 0;

	memset(Raw, 0xFF, sizeof(Raw));
	testRequire(xrtEcdsaDerEncode(Raw, 66, Der, sizeof(Der), &iSize) &&
		(iSize == sizeof(Der)) && (Der[0] == 0x30) &&
		(Der[1] == 0x81) && (Der[2] == 0x8A) &&
		(Der[3] == 0x02) && (Der[4] == 0x43) && (Der[5] == 0),
		"ECDSA DER long encoding mismatch");
	testRequire(xrtEcdsaDerDecode(Der, iSize, Decoded, 66) &&
		xrtConstTimeEqual(Decoded, Raw, sizeof(Raw)),
		"ECDSA DER long round trip mismatch");
}



/* 验证编解码通过局部快照支持任意输入输出重叠。 */
static void testEcdsaDerOverlap(void)
{
	uint8 Buffer[192] = { 0 };
	uint8 Expected[64] = { 0 };
	size_t iSize = 0;

	Expected[31] = 1;
	Expected[63] = 2;
	memcpy(Buffer + 16, Expected, sizeof(Expected));
	testRequire(xrtEcdsaDerEncode(
		Buffer + 16, 32, Buffer, sizeof(Buffer), &iSize
	), "ECDSA DER overlapping encode failed");
	testRequire(xrtEcdsaDerDecode(Buffer, iSize, Buffer + 4, 32) &&
		xrtConstTimeEqual(Buffer + 4, Expected, sizeof(Expected)),
		"ECDSA DER overlapping decode failed");
}



/* 验证严格解码拒绝负数、冗余零、非最短长度、尾随数据和越界整数。 */
static void testEcdsaDerRejectsMalformed(void)
{
	static const uint8 Cases[][12] = {
		{ 0x30, 0x06, 0x02, 0x01, 0x80, 0x02, 0x01, 0x01 },
		{ 0x30, 0x07, 0x02, 0x02, 0x00, 0x01, 0x02, 0x01, 0x01 },
		{ 0x30, 0x81, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x01 },
		{ 0x30, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x01, 0x00 },
		{ 0x30, 0x07, 0x02, 0x02, 0x01, 0x00, 0x02, 0x01, 0x01 }
	};
	static const size_t Sizes[] = { 8, 9, 9, 9, 9 };
	uint8 Output[2] = { 0xA5, 0x5A };

	for ( size_t i = 0; i < sizeof(Sizes) / sizeof(Sizes[0]); i++ ) {
		xrtClearError();
		testRequire(!xrtEcdsaDerDecode(Cases[i], Sizes[i], Output, 1) &&
			(Output[0] == 0xA5) && (Output[1] == 0x5A) &&
			(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
			(xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_SIGNATURE) &&
			(strcmp(xrtErrorOperation(xrtGetError()),
				"ecdsa-der-decode") == 0),
			"ECDSA DER malformed input contract mismatch");
	}
}



/* 验证参数与容量失败不会发布半个编码结果。 */
static void testEcdsaDerFailureAtomicity(void)
{
	uint8 Raw[64] = { 0 };
	uint8 Output[16];
	uint8 Before[16];
	size_t iSize = 0;

	Raw[31] = 1;
	Raw[63] = 2;
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	xrtClearError();
	testRequire(!xrtEcdsaDerEncode(Raw, 32, Output, 4, &iSize) &&
		(iSize == 8) && xrtConstTimeEqual(Output, Before, sizeof(Output)) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"ECDSA DER capacity failure was not atomic");
	xrtClearError();
	testRequire(!xrtEcdsaDerEncode(NULL, 32, Output, sizeof(Output), &iSize) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ECDSA DER null input contract mismatch");
	xrtClearError();
	testRequire(!xrtEcdsaDerEncode(
		Raw, 32, Output, sizeof(Output), (size_t*)Raw
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ECDSA DER overlapping size output was accepted");
}



int main(void)
{
	testEcdsaDerShort();
	testEcdsaDerLong();
	testEcdsaDerOverlap();
	testEcdsaDerRejectsMalformed();
	testEcdsaDerFailureAtomicity();
	printf("[PASS] crypto_ecdsa_der\n");
	return 0;
}
