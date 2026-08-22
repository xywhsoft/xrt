#include "../test.h"



/* 验证构建器增长、自引用、二进制内容和所有权转移。 */
int main(void)
{
	xstrbuf tBuffer;
	xstrview Alias;
	str sTaken;
	bool bAlias;
	size_t iOffset;

	xrtStrBufInit(&tBuffer);
	testRequire(xrtStrBufValid(&tBuffer), "empty buffer must be valid");
	testRequire(xrtStrBufAppend(&tBuffer, XRT_STR_LITERAL("abc")), "initial append failed");
	Alias = xrtStrSlice(xrtStrBufView(&tBuffer), 1, 2);
	testRequire(xrtStrBufAlias(&tBuffer, Alias, &bAlias, &iOffset) &&
		bAlias && (iOffset == 1), "buffer alias inspection mismatch");
	testRequire(xrtStrBufAlias(
		&tBuffer,
		XRT_STR_LITERAL("external"),
		&bAlias,
		&iOffset
	) && !bAlias && (iOffset == 0), "external view must not be reported as an alias");
	xrtClearError();
	testRequire(!xrtStrBufAlias(
		&tBuffer,
		xrtStrViewN(tBuffer.Data + 2u, 2u),
		&bAlias,
		&iOffset
	), "alias extending beyond buffer content must fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"invalid buffer alias error mismatch");
	xrtClearError();
	testRequire(xrtStrBufAppend(&tBuffer, Alias), "self append failed");
	testRequire((tBuffer.Size == 5) && (memcmp(tBuffer.Data, "abcbc", 5) == 0) &&
		(tBuffer.Data[5] == 0), "self append result mismatch");

	Alias = xrtStrSlice(xrtStrBufView(&tBuffer), 0, tBuffer.Size);
	testRequire(xrtStrBufAppendRepeat(&tBuffer, Alias, 20), "self repeat growth failed");
	testRequire((tBuffer.Size == 105) && (tBuffer.Data[tBuffer.Size] == 0),
		"self repeat size mismatch");
	for ( size_t i = 0; i < 21u; i++ ) {
		testRequire(memcmp(tBuffer.Data + (i * 5u), "abcbc", 5) == 0,
			"self repeat content mismatch");
	}
	testRequire(xrtStrBufAppendByte(&tBuffer, 0), "embedded null append failed");
	testRequire((tBuffer.Size == 106) && (tBuffer.Data[105] == 0) && (tBuffer.Data[106] == 0),
		"embedded null invariant mismatch");

	testRequire(xrtStrBufResize(&tBuffer, 110), "buffer expansion failed");
	testRequire((tBuffer.Data[106] == 0) && (tBuffer.Data[109] == 0) && (tBuffer.Data[110] == 0),
		"expanded bytes were not cleared");
	testRequire(xrtStrBufResize(&tBuffer, 3), "buffer shrink failed");
	testRequire((tBuffer.Size == 3) && (strcmp(tBuffer.Data, "abc") == 0), "buffer shrink mismatch");

	sTaken = xrtStrBufTake(&tBuffer);
	testRequire((sTaken != NULL) && (strcmp(sTaken, "abc") == 0), "buffer take mismatch");
	testRequire((tBuffer.Data == NULL) && (tBuffer.Size == 0) && (tBuffer.Capacity == 0),
		"buffer take did not reset state");
	xrtFree(sTaken);
	sTaken = xrtStrBufTake(&tBuffer);
	testRequire((sTaken != NULL) && (sTaken[0] == 0), "empty buffer take ownership mismatch");
	xrtFree(sTaken);

	xrtClearError();
	testRequire(!xrtStrBufReserve(&tBuffer, SIZE_MAX), "maximum reserve must fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "reserve overflow error mismatch");
	xrtClearError();
	xrtStrBufFree(&tBuffer);
	printf("[PASS] string-buffer\n");
	return 0;
}
