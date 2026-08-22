#include "../test.h"



/* 验证 Content-Disposition 的严格参数和文件名优先级。 */
static void testMimeContentDisposition(void)
{
	xcontentdisposition Disposition;
	union {
		xcontentdisposition Disposition;
		size_t Size;
	} Shared;
	xerror* pPrior;
	char Text[192];
	char FileName[64];
	size_t iSize;
	str sBuilt;

	testRequire(xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"attachment; filename=\"fallback.txt\"; "
			"filename*=UTF-8''%E4%B8%AD%E6%96%87.txt"
		), &Disposition
	), "Content-Disposition parse failed");
	testRequire((Disposition.Flags &
		XCONTENT_DISPOSITION_FILENAME) != 0 &&
		(Disposition.Flags &
		 XCONTENT_DISPOSITION_FILENAME_EXT) != 0,
		"Content-Disposition common parameters missing");
	testRequire(xrtHttpContentDispositionFileNameWrite(
		&Disposition, FileName, sizeof(FileName), &iSize
	) && (iSize == 10) &&
		(memcmp(FileName,
		 "\xE4\xB8\xAD\xE6\x96\x87.txt", 10) == 0),
		"Content-Disposition UTF-8 filename mismatch");
	testRequire(xrtHttpContentDispositionWrite(
		&Disposition, Text, sizeof(Text), &iSize
	) && (iSize == 76),
		"Content-Disposition write failed");
	Shared.Disposition = Disposition;
	testRequire(!xrtHttpContentDispositionWrite(
		&Shared.Disposition, NULL, 0, &Shared.Size
	), "Content-Disposition size output overlapped metadata");
	xrtClearError();
	sBuilt = xrtHttpContentDispositionFileNameBuild(
		&Disposition, &iSize
	);
	testRequire((sBuilt != NULL) && (iSize == 10) &&
		(memcmp(sBuilt,
		 "\xE4\xB8\xAD\xE6\x96\x87.txt", 10) == 0),
		"Content-Disposition filename build mismatch");
	xrtFree(sBuilt);

	testRequire(xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"attachment; filename=\"fallback.txt\"; "
			"filename*=ISO-8859-1''caf%E9.txt"
		), &Disposition
	), "Content-Disposition fallback value parse failed");
	pPrior = xrtErrorCreate(
		XERR_STATE, "test", 1, "preserved error"
	);
	testRequire(pPrior != NULL,
		"Content-Disposition prior error creation failed");
	xrtSetError(pPrior);
	testRequire(xrtHttpContentDispositionFileNameWrite(
		&Disposition, FileName, sizeof(FileName), &iSize
	) && (iSize == 12) &&
		(memcmp(FileName, "fallback.txt", 12) == 0) &&
		(xrtGetError() == pPrior),
		"Content-Disposition unsupported charset fallback failed");
	xrtClearError();
	xrtErrorFree(pPrior);
	testRequire(!xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(" \t"), &Disposition
	), "Content-Disposition accepted empty input");
	xrtClearError();
	testRequire(!xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"attachment; filename=a; FILENAME=b"
		), &Disposition
	), "Content-Disposition accepted duplicate filename");
	xrtClearError();
	testRequire(!xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"attachment; filename*=\"UTF-8''a.txt\""
		), &Disposition
	), "Content-Disposition accepted quoted filename*");
	xrtClearError();
}



/* 验证扩展参数、权威视图和文件名失败原子性。 */
static void testContentDispositionHardening(void)
{
	xcontentdisposition Disposition;
	xcontentdisposition Empty = { 0 };
	xhttpparam Param;
	xerror* pPrior;
	char Text[128];
	char Output[64];
	char Guard[64];
	size_t iSize;

	/* 所有星号扩展参数都必须使用未加引号的 RFC 8187 ext-value。 */
	testRequire(xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"attachment; title*=UTF-8'en'hello%20world"
		),
		&Disposition
	), "Content-Disposition extension parameter parse failed");
	testRequire(xrtHttpContentDispositionParam(
		&Disposition, XRT_STR_LITERAL("TITLE*"), &Param
	) == XHTTP_NEXT_ITEM &&
		(Param.Value.Size == 22u),
		"Content-Disposition extension parameter lookup failed");
	testRequire(!xrtHttpContentDispositionParse(
		XRT_STR_LITERAL("attachment; title*=plain"),
		&Disposition
	), "Content-Disposition accepted malformed extension value");
	xrtClearError();
	memset(&Disposition, 0xA5, sizeof(Disposition));
	testRequire(!xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"attachment; title*=\"UTF-8''plain\""
		),
		&Disposition
	) && (memcmp(
		&Disposition, &Empty, sizeof(Disposition)
	) == 0), "Content-Disposition failure did not clear output");
	xrtClearError();

	/* 无效 UTF-8 filename* 被忽略，并在不污染线程错误时回退 filename。 */
	testRequire(xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"attachment; filename=\"fallback.txt\"; "
			"filename*=UTF-8''bad%C3%28.txt"
		),
		&Disposition
	), "Content-Disposition invalid UTF-8 syntax parse failed");
	pPrior = xrtErrorCreate(
		XERR_STATE, "test", 2, "preserved UTF-8 fallback error"
	);
	testRequire(pPrior != NULL,
		"Content-Disposition UTF-8 prior error creation failed");
	xrtSetError(pPrior);
	testRequire(xrtHttpContentDispositionFileNameWrite(
		&Disposition, Output, sizeof(Output), &iSize
	) && (iSize == 12u) &&
		(memcmp(Output, "fallback.txt", 12u) == 0) &&
		(xrtGetError() == pPrior),
		"Content-Disposition invalid UTF-8 fallback failed");
	xrtClearError();
	xrtErrorFree(pPrior);

	memset(Output, 0x5A, sizeof(Output));
	memcpy(Guard, Output, sizeof(Output));
	iSize = 777u;
	testRequire(xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"attachment; filename*=UTF-8''bad%C3%28.txt"
		),
		&Disposition
	) && !xrtHttpContentDispositionFileNameWrite(
		&Disposition, Output, sizeof(Output), &iSize
	) && (iSize == 777u) &&
		(memcmp(Output, Guard, sizeof(Output)) == 0),
		"Content-Disposition invalid UTF-8 failure was not atomic");
	xrtClearError();

	/* 派生缓存不是权威输入，读取和写出必须从原始视图重建。 */
	testRequire(xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"attachment; filename=\"fallback.txt\"; "
			"filename*=UTF-8''%E4%B8%AD%E6%96%87.txt"
		),
		&Disposition
	), "Content-Disposition cache source parse failed");
	Disposition.Flags = XCONTENT_DISPOSITION_NONE;
	memset(&Disposition.Name, 0xA5, sizeof(Disposition.Name));
	memset(&Disposition.FileName, 0xA5, sizeof(Disposition.FileName));
	memset(
		&Disposition.FileNameExt, 0xA5,
		sizeof(Disposition.FileNameExt)
	);
	testRequire(xrtHttpContentDispositionFileNameWrite(
		&Disposition, Output, sizeof(Output), &iSize
	) && (iSize == 10u) &&
		(memcmp(
			Output, "\xE4\xB8\xAD\xE6\x96\x87.txt", 10u
		) == 0),
		"Content-Disposition trusted stale derived cache");
	testRequire(xrtHttpContentDispositionWrite(
		&Disposition, Text, sizeof(Text), &iSize
	) && (iSize == 76u),
		"Content-Disposition stale cache blocked raw write");

	/* 调用方可以只提供 Type 与 Parameters，常用缓存由操作重新推导。 */
	memset(&Disposition, 0, sizeof(Disposition));
	Disposition.Type = XRT_STR_LITERAL("attachment");
	Disposition.Parameters = XRT_STR_LITERAL(
		"filename=\"manual.txt\""
	);
	testRequire(xrtHttpContentDispositionFileNameWrite(
		&Disposition, Output, sizeof(Output), &iSize
	) && (iSize == 10u) &&
		(memcmp(Output, "manual.txt", 10u) == 0),
		"Content-Disposition raw descriptor filename failed");
	testRequire(xrtHttpContentDispositionWrite(
		&Disposition, Text, sizeof(Text), &iSize
	) && (iSize == 33u) &&
		(memcmp(
			Text, "attachment; filename=\"manual.txt\"", 33u
		) == 0),
		"Content-Disposition raw descriptor write failed");

	/* 短缓冲和非法描述符不得留下部分输出。 */
	memset(Output, 0x5A, sizeof(Output));
	memcpy(Guard, Output, sizeof(Output));
	iSize = 0;
	testRequire(!xrtHttpContentDispositionFileNameWrite(
		&Disposition, Output, 2u, &iSize
	) && (iSize == 10u) &&
		(memcmp(Output, Guard, sizeof(Output)) == 0),
		"Content-Disposition short filename write was not atomic");
	xrtClearError();
	Disposition.Parameters = XRT_STR_LITERAL("title*=plain");
	iSize = 999u;
	testRequire(!xrtHttpContentDispositionWrite(
		&Disposition, Output, sizeof(Output), &iSize
	) && (iSize == 999u) &&
		(memcmp(Output, Guard, sizeof(Output)) == 0),
		"Content-Disposition invalid descriptor write was not atomic");
	xrtClearError();

	/* 输出不能覆盖仍被借用的原始字段文本。 */
	memcpy(
		Text, "attachment; filename=overlap.txt", 32u
	);
	testRequire(xrtHttpContentDispositionParse(
		(xstrview){ Text, 32u }, &Disposition
	), "Content-Disposition overlap source parse failed");
	iSize = 0;
	testRequire(!xrtHttpContentDispositionFileNameWrite(
		&Disposition, Text + 21u, 4u, &iSize
	), "Content-Disposition accepted overlapping filename output");
	xrtClearError();
}



/* 执行 Content-Disposition 测试。 */
int main(void)
{
	testMimeContentDisposition();
	testContentDispositionHardening();
	printf("[PASS] http_content_disposition\n");
	return 0;
}
