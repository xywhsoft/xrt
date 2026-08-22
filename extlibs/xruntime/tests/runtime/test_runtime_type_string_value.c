#include "../test.h"



/* 验证拥有型 str 与动态字符串之间的独立双向转换。 */
int main(void)
{
	const xrttype* pType = xrtTypeString();
	xvalue* pSource = xrtValueString(XRT_STR_LITERAL("hello"));
	xvalue* pEmpty = xrtValueString(XRT_STR_LITERAL(""));
	xvalue* pResult;
	xvalue* pEmbedded;
	str sText;
	str sEmpty;
	xstrview Text;
	const char sEmbedded[] = { 'a', 0, 'b' };

	testRequire((pSource != NULL) && (pEmpty != NULL),
		"runtime string Value fixture failed");
	testRequire(
		xrtValueToTyped(pSource, pType, &sText, NULL) &&
		(sText != NULL) && (strcmp(sText, "hello") == 0),
		"runtime string Value decode failed"
	);
	pResult = xrtValueFromTyped(pType, &sText, NULL);
	testRequire(
		(pResult != NULL) &&
		xrtValueGetString(pResult, &Text) &&
		xrtStrEqual(Text, XRT_STR_LITERAL("hello")),
		"runtime string Value encode failed"
	);

	testRequire(
		xrtValueToTyped(pEmpty, pType, &sEmpty, NULL) &&
		(sEmpty != NULL) && (sEmpty[0] == 0),
		"runtime empty string Value decode failed"
	);
	xrtTypeDropValue(pType, &sEmpty);
	pEmbedded = xrtValueString(xrtStrViewN(sEmbedded, sizeof(sEmbedded)));
	testRequire(pEmbedded != NULL,
		"runtime embedded-zero string fixture failed");
	xrtClearError();
	testRequire(!xrtValueToTyped(pEmbedded, pType, &sEmpty, NULL),
		"embedded-zero string was decoded as str");
	testRequire(
		(sEmpty == NULL) &&
		(xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.typed-value") == 0) &&
		(xrtErrorCode(xrtGetError()) == XTYPED_VALUE_ERROR_CONVERT),
		"embedded-zero string conversion error mismatch"
	);

	xrtTypeDropValue(pType, &sText);
	xrtTypeDropValue(pType, &sEmpty);
	xrtValueRelease(pEmbedded);
	xrtValueRelease(pResult);
	xrtValueRelease(pEmpty);
	xrtValueRelease(pSource);
	xrtClearError();
	printf("[PASS] runtime string Value conversion\n");
	return 0;
}
