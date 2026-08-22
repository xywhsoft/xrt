#include "../test_allocator.h"



/* MIME 分配型便捷函数在 OOM 下保持长度结果不变。 */
int main(void)
{
	xmediatype Type;
	xcontentdisposition Disposition;
	size_t iSize = 99;

	testRequire(xrtHttpMediaTypeParse(
		XRT_STR_LITERAL("application/json; charset=UTF-8"), &Type
	), "MIME OOM media type setup failed");
	testRequire(xrtHttpContentDispositionParse(
		XRT_STR_LITERAL("attachment; filename=\"a.txt\""),
		&Disposition
	), "MIME OOM disposition setup failed");
	testRequire(testInstallFailAllocator(),
		"MIME OOM allocator install failed");
	testRequire((xrtHttpMediaTypeBuild(
		&Type, &iSize
	) == NULL) && (iSize == 99) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"MIME media type build OOM was not atomic");
	xrtClearError();
	testRequire((xrtHttpContentDispositionBuild(
		&Disposition, &iSize
	) == NULL) && (iSize == 99) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"Content-Disposition build OOM was not atomic");
	xrtClearError();
	testRequire((xrtHttpContentDispositionFileNameBuild(
		&Disposition, &iSize
	) == NULL) && (iSize == 99) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"Content-Disposition filename OOM was not atomic");
	printf("[PASS] http_content_disposition_oom\n");
	return 0;
}

