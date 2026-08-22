#include "../test_allocator.h"



/* 验证 codec、原地解析和字段写出热路径完全不分配。 */
int main(void)
{
	static const xformfield Source[] = {
		{ XRT_BYTES_INIT("name"), XRT_BYTES_INIT("alice bob") }
	};
	char Text[64];
	uint8 Data[32];
	xformfield Fields[2];
	size_t iOffset = 0;
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"form failure allocator install failed");
	testRequire(xrtFormEncode(
		"a b", 3, Text, sizeof(Text), &iSize
	) && (strcmp(Text, "a+b") == 0),
		"form direct encode unexpectedly allocated");
	testRequire(xrtFormDecode(
		(xstrview){ Text, iSize }, Data, sizeof(Data), &iSize
	) && (iSize == 3) && (memcmp(Data, "a b", 3) == 0),
		"form direct decode unexpectedly allocated");
	testRequire(xrtFormWrite(
		Source, 1, Text, sizeof(Text), &iSize
	), "form field write unexpectedly allocated");
	testRequire(xrtFormParse(
		Text, iSize, Fields, 2, &iSize, NULL
	) && (iSize == 1), "form in-place parse unexpectedly allocated");
	testRequire(xrtFormFind(
		XRT_STR_LITERAL("a+b=x+y"), XRT_BYTES_LITERAL("a b"),
		&iOffset, Data, sizeof(Data), &iSize
	) == XFORM_FIND_FOUND && (iSize == 3) &&
		(memcmp(Data, "x y", 3) == 0),
		"form direct lookup unexpectedly allocated");
	printf("[PASS] form_urlencoded_noalloc\n");
	return 0;
}
