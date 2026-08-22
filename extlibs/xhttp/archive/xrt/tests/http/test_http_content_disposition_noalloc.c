#include "../test_allocator.h"



/* Content-Disposition 解析、文件名解码和直接写出必须保持零堆分配。 */
int main(void)
{
	xcontentdisposition Disposition;
	char Output[128];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"Content-Disposition failure allocator install failed");
	testRequire(xrtHttpContentDispositionParse(
		XRT_STR_LITERAL("attachment; filename=\"a.txt\""),
		&Disposition
	), "Content-Disposition parse allocated");
	testRequire(xrtHttpContentDispositionFileNameWrite(
		&Disposition, Output, sizeof(Output), &iSize
	), "Content-Disposition filename read allocated");
	testRequire(xrtHttpContentDispositionWrite(
		&Disposition, Output, sizeof(Output), &iSize
	), "Content-Disposition write allocated");

	/* UTF-8 校验跨 percent 字节流进行，不能分配完整解码副本。 */
	testRequire(xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"attachment; filename=fallback.txt; "
			"filename*=UTF-8''%E4%B8%AD%E6%96%87.txt"
		),
		&Disposition
	), "Content-Disposition UTF-8 parse allocated");
	testRequire(xrtHttpContentDispositionFileNameWrite(
		&Disposition, Output, sizeof(Output), &iSize
	) && (iSize == 10u) && (memcmp(
		Output, "\xE4\xB8\xAD\xE6\x96\x87.txt", 10u
	) == 0), "Content-Disposition UTF-8 filename read allocated");

	/* 非法 UTF-8 不得触发分配，且必须回退普通 filename。 */
	testRequire(xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"attachment; filename=fallback.txt; "
			"filename*=UTF-8''bad%C3%28.txt"
		),
		&Disposition
	), "Content-Disposition invalid UTF-8 parse allocated");
	testRequire(xrtHttpContentDispositionFileNameWrite(
		&Disposition, Output, sizeof(Output), &iSize
	) && (iSize == 12u) && (memcmp(
		Output, "fallback.txt", 12u
	) == 0), "Content-Disposition UTF-8 fallback allocated");
	printf("[PASS] http_content_disposition_noalloc\n");
	return 0;
}
