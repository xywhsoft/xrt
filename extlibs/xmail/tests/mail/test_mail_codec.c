#include "../test.h"



/* 验证 Quoted-Printable 文本规范化、二进制解码和错误原子性。 */
static void testMailQuotedPrintable(void)
{
	static const uint8 arrInput[] = {
		'l', 'i', 'n', 'e', ' ', '\n', '=', 0
	};
	static const char sExpected[] = "line=20\r\n=3D=00";
	static const uint8 arrDecoded[] = {
		'l', 'i', 'n', 'e', ' ', '\r', '\n', '=', 0
	};
	char arrOutput[128];
	uint8 arrInPlace[64] = "line=20\r\n=3D=00";
	size_t iSize = 0;
	str sEncoded;
	bytes pDecoded;

	testRequire(xrtMailQpWrite(
		arrInput,
		sizeof(arrInput),
		0,
		XMAIL_QP_TEXT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, sExpected) == 0),
		"mail quoted-printable text output mismatch");
	testRequire(xrtMailQpWrite(
		"aaaaa",
		5u,
		4u,
		XMAIL_QP_TEXT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "aaa=\r\naa") == 0),
		"mail quoted-printable line wrapping mismatch");

	testRequire(xrtMailQpDecodeWrite(
		testMailView((cstr)arrInPlace),
		0,
		arrInPlace,
		sizeof(arrInPlace),
		&iSize
	) && (iSize == sizeof(arrDecoded)) &&
		(memcmp(arrInPlace, arrDecoded, sizeof(arrDecoded)) == 0),
		"mail quoted-printable in-place decode mismatch");

	memcpy(arrOutput, "keep", 5u);
	xrtClearError();
	testRequire(!xrtMailQpDecodeWrite(
		XRT_STR_LITERAL("=GG"),
		0,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (memcmp(arrOutput, "keep", 5u) == 0),
		"invalid quoted-printable modified output");
	testRequire(xrtErrorFind(
		xrtGetError(),
		"xrt.mail",
		XMAIL_ERROR_ENCODING
	) != NULL, "quoted-printable error metadata mismatch");

	sEncoded = xrtMailQp(arrInput, sizeof(arrInput), 0, XMAIL_QP_TEXT, &iSize);
	testRequire((sEncoded != NULL) && (strcmp(sEncoded, sExpected) == 0),
		"allocated quoted-printable mismatch");
	pDecoded = xrtMailQpDecode(testMailView(sEncoded), 0, &iSize);
	testRequire((pDecoded != NULL) && (iSize == sizeof(arrDecoded)) &&
		(memcmp(pDecoded, arrDecoded, sizeof(arrDecoded)) == 0),
		"allocated quoted-printable decode mismatch");
	xrtFree(pDecoded);
	xrtFree(sEncoded);
}



/* 验证 MIME Base64 行包装和空白容忍解码。 */
static void testMailBase64(void)
{
	char arrOutput[128];
	uint8 arrDecoded[16];
	size_t iSize = 0;
	str sEncoded;
	bytes pDecoded;

	testRequire(xrtMailBase64Write(
		"hello",
		5u,
		0,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == 10u) && (strcmp(arrOutput, "aGVsbG8=\r\n") == 0),
		"mail Base64 output mismatch");
	testRequire(xrtMailBase64DecodeWrite(
		XRT_STR_LITERAL("aGVs\r\nbG8=\r\n"),
		arrDecoded,
		sizeof(arrDecoded),
		&iSize
	) && (iSize == 5u) && (memcmp(arrDecoded, "hello", 5u) == 0),
		"mail Base64 whitespace decode mismatch");

	sEncoded = xrtMailBase64("hello", 5u, 4u, &iSize);
	testRequire((sEncoded != NULL) &&
		(strcmp(sEncoded, "aGVs\r\nbG8=\r\n") == 0),
		"allocated MIME Base64 wrapping mismatch");
	pDecoded = xrtMailBase64Decode(testMailView(sEncoded), NULL);
	testRequire((pDecoded != NULL) && (memcmp(pDecoded, "hello", 5u) == 0),
		"allocated MIME Base64 decode mismatch");
	xrtFree(pDecoded);
	xrtFree(sEncoded);

	xrtClearError();
	testRequire(!xrtMailBase64Write(
		"x",
		1u,
		5u,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	), "invalid MIME Base64 line width was accepted");
	testRequire(xrtErrorFind(
		xrtGetError(),
		"xrt.mail",
		XMAIL_ERROR_CONFIG
	) != NULL, "MIME Base64 config error mismatch");
}



/* 运行邮件传输编码测试。 */
int main(void)
{
	testMailQuotedPrintable();
	testMailBase64();
	return 0;
}
