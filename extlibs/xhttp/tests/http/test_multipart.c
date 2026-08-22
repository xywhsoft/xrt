#include "../test.h"



/* 验证 Content-Type boundary 的拥有型解析和 RFC 2046 字符边界。 */
static void testMultipartBoundary(void)
{
	xmultipartboundary Boundary;
	xstrview View;

	testRequire(xrtMultipartBoundaryFromContentType(
		XRT_STR_LITERAL(
			"multipart/form-data; boundary=\"AaB\\:03x\""
		), &Boundary
	), "Multipart quoted boundary parse failed");
	View = xrtMultipartBoundaryView(&Boundary);
	testRequire((View.Size == 7) &&
		(memcmp(View.Data, "AaB:03x", 7) == 0),
		"Multipart quoted boundary decode mismatch");
	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("abc def"), &Boundary
	), "Multipart boundary rejected legal inner space");
	testRequire(!xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("abc "), &Boundary
	) && !xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("abc@def"), &Boundary
	), "Multipart boundary accepted illegal syntax");
	xrtClearError();
}



/* 验证固定描述符、迭代输出和数组输出支持未对齐存储。 */
static void testMultipartMemoryContracts(void)
{
	static const char BodyText[] =
		"--b\r\n"
		"Content-Disposition: form-data; name=x\r\n"
		"\r\n"
		"value\r\n"
		"--b--\r\n";
	xbytesview Body = {
		(const uint8*)BodyText,
		sizeof(BodyText) - 1u
	};
	uint8 BoundaryStorage[sizeof(xmultipartboundary) + 2u];
	uint8 LimitsStorage[sizeof(xmultipartlimits) + 2u];
	uint8 OffsetStorage[sizeof(size_t) + 2u];
	uint8 PartStorage[sizeof(xmultipartpart) + 2u];
	uint8 ErrorStorage[sizeof(xmultiparterrorinfo) + 2u];
	uint8 CountStorage[sizeof(size_t) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	xmultipartboundary Boundary;
	xmultipartlimits Limits;
	xmultipartpart Part;
	xmultiparterrorinfo Error;
	xstrview View;
	size_t iValue = 0;
	char sName[8];

	memset(BoundaryStorage, 0xA5, sizeof(BoundaryStorage));
	memset(LimitsStorage, 0xA5, sizeof(LimitsStorage));
	xrtMultipartLimitsInit(
		(xmultipartlimits*)(void*)(LimitsStorage + 1u)
	);
	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("b"),
		(xmultipartboundary*)(void*)(BoundaryStorage + 1u)
	), "Multipart boundary did not support unaligned output");
	memcpy(&Boundary, BoundaryStorage + 1u, sizeof(Boundary));
	memcpy(&Limits, LimitsStorage + 1u, sizeof(Limits));
	View = xrtMultipartBoundaryView(
		(const xmultipartboundary*)(const void*)(BoundaryStorage + 1u)
	);
	testRequire(
		(BoundaryStorage[0] == 0xA5) &&
		(BoundaryStorage[sizeof(BoundaryStorage) - 1u] == 0xA5) &&
		(LimitsStorage[0] == 0xA5) &&
		(LimitsStorage[sizeof(LimitsStorage) - 1u] == 0xA5) &&
		(Boundary.Size == 1u) &&
		(View.Size == 1u) && (View.Data[0] == 'b') &&
		(Limits.MaxParts == 1024u) &&
		(Limits.MaxHeaders == 64u),
		"Multipart fixed descriptor initialization corrupted storage"
	);

	memset(OffsetStorage, 0xA5, sizeof(OffsetStorage));
	memset(PartStorage, 0xA5, sizeof(PartStorage));
	memset(ErrorStorage, 0xA5, sizeof(ErrorStorage));
	memcpy(OffsetStorage + 1u, &iValue, sizeof(iValue));
	testRequire(xrtMultipartNext(
		Body,
		(const xmultipartboundary*)(const void*)(BoundaryStorage + 1u),
		(size_t*)(void*)(OffsetStorage + 1u),
		(xmultipartpart*)(void*)(PartStorage + 1u),
		(xmultiparterrorinfo*)(void*)(ErrorStorage + 1u)
	) == XHTTP_NEXT_ITEM,
		"Multipart iterator did not support unaligned descriptors");
	memcpy(&iValue, OffsetStorage + 1u, sizeof(iValue));
	memcpy(&Part, PartStorage + 1u, sizeof(Part));
	memcpy(&Error, ErrorStorage + 1u, sizeof(Error));
	testRequire((iValue != 0) &&
		(Part.Body.Size == 5u) &&
		(Error.Code == XMULTIPART_ERROR_NONE) &&
		xrtMultipartFormPartValid(
			(const xmultipartpart*)(const void*)(PartStorage + 1u)
		), "Multipart iterator unaligned result mismatch");
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	testRequire(xrtMultipartPartNameWrite(
		(const xmultipartpart*)(const void*)(PartStorage + 1u),
		sName,
		sizeof(sName),
		(size_t*)(void*)(SizeStorage + 1u)
	), "Multipart Part writer did not support unaligned descriptors");
	memcpy(&iValue, SizeStorage + 1u, sizeof(iValue));
	testRequire((iValue == 1u) && (sName[0] == 'x') &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"Multipart Part writer unaligned result mismatch");

	testRequire(xrtMultipartValidate(
		Body,
		(const xmultipartboundary*)(const void*)(BoundaryStorage + 1u),
		(const xmultipartlimits*)(const void*)(LimitsStorage + 1u),
		(xmultiparterrorinfo*)(void*)(ErrorStorage + 1u)
	), "Multipart validation did not support unaligned descriptors");
	memset(PartStorage, 0xA5, sizeof(PartStorage));
	memset(CountStorage, 0xA5, sizeof(CountStorage));
	testRequire(xrtMultipartParse(
		Body,
		(const xmultipartboundary*)(const void*)(BoundaryStorage + 1u),
		(xmultipartpart*)(void*)(PartStorage + 1u),
		1u,
		(size_t*)(void*)(CountStorage + 1u),
		(const xmultipartlimits*)(const void*)(LimitsStorage + 1u),
		(xmultiparterrorinfo*)(void*)(ErrorStorage + 1u)
	), "Multipart array parse did not support unaligned outputs");
	memcpy(&iValue, CountStorage + 1u, sizeof(iValue));
	memcpy(&Part, PartStorage + 1u, sizeof(Part));
	testRequire((iValue == 1u) && (Part.Body.Size == 5u) &&
		(PartStorage[0] == 0xA5) &&
		(PartStorage[sizeof(PartStorage) - 1u] == 0xA5) &&
		(CountStorage[0] == 0xA5) &&
		(CountStorage[sizeof(CountStorage) - 1u] == 0xA5),
		"Multipart array parse corrupted unaligned output guards");

	xrtMultipartLimitsInit((xmultipartlimits*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"Multipart limits init accepted wrapping output");
	xrtClearError();
	testRequire(!xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("b"),
		(xmultipartboundary*)(uintptr_t)(UINTPTR_MAX - 1u)
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Multipart boundary parse accepted wrapping output");
	xrtClearError();
	testRequire(xrtMultipartNext(
		Body,
		&Boundary,
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u),
		&Part,
		NULL
	) == XHTTP_NEXT_ERROR,
		"Multipart iterator accepted wrapping offset");
	xrtClearError();
	testRequire(!xrtMultipartValidate(
		Body,
		&Boundary,
		(const xmultipartlimits*)(uintptr_t)(UINTPTR_MAX - 1u),
		NULL
	), "Multipart validation accepted wrapping limits");
	xrtClearError();
	testRequire(!xrtMultipartParse(
		Body,
		&Boundary,
		NULL,
		0,
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u),
		NULL,
		NULL
	), "Multipart parse accepted wrapping count output");
	xrtClearError();
}



/* 验证完整正文迭代保留 preamble、epilogue、二进制正文和 Part 元数据边界。 */
static void testMultipartParse(void)
{
	static const char BodyText[] =
		"ignored preamble\r\n"
		"--AaB03x\r\n"
		"Content-Disposition: form-data; name=\"field\"\r\n"
		"X-Test: one\r\n"
		"\r\n"
		"value\r\n"
		"--AaB03x \t\r\n"
		"Content-Disposition: form-data; name=\"file\"; "
			"filename=\"fallback.txt\"; "
			"filename*=UTF-8''%E4%B8%AD%E6%96%87.txt\r\n"
		"Content-Type: text/plain; charset=UTF-8\r\n"
		"\r\n"
		"hello\r\nworld\r\n"
		"--AaB03x--\t\r\n"
		"ignored epilogue";
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	xmultipartpart Parts[2];
	xmultipartpart Before[2];
	xmultipartpart Part;
	xhttpnext Next;
	char Value[64];
	size_t iOffset = 0;
	size_t iCount;
	size_t iSize;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("AaB03x"), &Boundary
	), "Multipart boundary init failed");
	Next = xrtMultipartNext(
		(xbytesview){
			(const uint8*)BodyText, sizeof(BodyText) - 1u
		}, &Boundary, &iOffset, &Part, &Error
	);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		(Part.HeaderCount == 2) &&
		xrtMultipartFormPartValid(&Part) &&
		xrtMultipartPartNameWrite(
			&Part, Value, sizeof(Value), &iSize
		) && (iSize == 5) &&
		(memcmp(Value, "field", 5) == 0) &&
		(Part.Body.Size == 5) &&
		(memcmp(Part.Body.Data, "value", 5) == 0),
		"Multipart first Part mismatch");
	Next = xrtMultipartNext(
		(xbytesview){
			(const uint8*)BodyText, sizeof(BodyText) - 1u
		}, &Boundary, &iOffset, &Part, &Error
	);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		(Part.HeaderCount == 2) &&
		((Part.Flags & XMULTIPART_PART_CONTENT_TYPE) != 0) &&
		xrtHttpMediaTypeEqual(
			&Part.ContentType,
			XRT_STR_LITERAL("text"),
			XRT_STR_LITERAL("plain")
		) && xrtMultipartPartFileNameWrite(
			&Part, Value, sizeof(Value), &iSize
		) && (iSize == 10) &&
		(memcmp(
			Value, "\xE4\xB8\xAD\xE6\x96\x87.txt", 10
		) == 0) &&
		(Part.Body.Size == 12) &&
		(memcmp(Part.Body.Data, "hello\r\nworld", 12) == 0),
		"Multipart file Part mismatch");
	testRequire(xrtMultipartNext(
		(xbytesview){
			(const uint8*)BodyText, sizeof(BodyText) - 1u
		}, &Boundary, &iOffset, &Part, &Error
	) == XHTTP_NEXT_END &&
		(iOffset == (sizeof(BodyText) - 1u)),
		"Multipart closing boundary or epilogue mismatch");

	testRequire(xrtMultipartParse(
		(xbytesview){
			(const uint8*)BodyText, sizeof(BodyText) - 1u
		}, &Boundary, NULL, 0, &iCount, NULL, &Error
	) && (iCount == 2), "Multipart count query mismatch");
	memset(Parts, 0, sizeof(Parts));
	testRequire(xrtMultipartParse(
		(xbytesview){
			(const uint8*)BodyText, sizeof(BodyText) - 1u
		}, &Boundary, Parts, 2, &iCount, NULL, &Error
	), "Multipart array parse failed");
	testRequire(iCount == 2, "Multipart array count mismatch");
	testRequire(
		Parts[0].Body.Size == 5,
		"Multipart first body size mismatch"
	);
	testRequire(
		Parts[1].Body.Size == 12,
		"Multipart second body size mismatch"
	);

	memset(Parts, 0xA5, sizeof(Parts));
	memcpy(Before, Parts, sizeof(Parts));
	iCount = 0;
	testRequire(!xrtMultipartParse(
		(xbytesview){
			(const uint8*)BodyText, sizeof(BodyText) - 1u
		}, &Boundary, Parts, 1, &iCount, NULL, &Error
	) && (iCount == 2) &&
		(memcmp(Parts, Before, sizeof(Parts)) == 0),
		"Multipart capacity failure modified output");
	xrtClearError();
}



/* 验证关闭边界、Header 和 false boundary candidate 的严格失败语义。 */
static void testMultipartInvalid(void)
{
	static const char FalseCandidate[] =
		"--b\r\n"
		"Content-Disposition: form-data; name=x\r\n"
		"\r\n"
		"one\r\n--bX\r\ntwo\r\n"
		"--b--\r\n";
	static const xstrview Invalid[] = {
		XRT_STR_INIT(
			"--b\r\nA: 1\nB: 2\r\n\r\nx\r\n--b--\r\n"
		),
		XRT_STR_INIT(
			"--b\r\nContent-Type: text/plain\r\n"
			"Content-Type: text/html\r\n\r\nx\r\n--b--\r\n"
		),
		XRT_STR_INIT(
			"--b\r\n\r\nx\r\n--b\r\n"
		),
		XRT_STR_INIT("--b--\r\n")
	};
	xmultipartboundary Boundary;
	xmultipartlimits Limits;
	xmultiparterrorinfo Error;
	xmultipartpart Part;
	size_t iCount = SIZE_MAX;
	size_t iOffset;
	size_t i;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("b"), &Boundary
	), "Multipart invalid-test boundary init failed");
	iOffset = 0;
	testRequire(xrtMultipartNext(
		(xbytesview){
			(const uint8*)FalseCandidate,
			sizeof(FalseCandidate) - 1u
		}, &Boundary, &iOffset, &Part, &Error
	) == XHTTP_NEXT_ITEM &&
		(Part.Body.Size == 14) &&
		(memcmp(Part.Body.Data, "one\r\n--bX\r\ntwo", 14) == 0),
		"Multipart false boundary candidate split the body");

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtMultipartValidate(
			(xbytesview){
				(const uint8*)Invalid[i].Data, Invalid[i].Size
			}, &Boundary, NULL, &Error
		), "Multipart accepted malformed body");
		xrtClearError();
	}
	testRequire(xrtMultipartValidateCount(
		(xbytesview){
			(const uint8*)Invalid[3].Data,
			Invalid[3].Size
		},
		&Boundary,
		NULL,
		XMULTIPART_VALIDATE_ALLOW_EMPTY,
		&iCount,
		&Error
	) && (iCount == 0u) && (Error.Code == XMULTIPART_ERROR_NONE),
		"Multipart empty-body validation contract mismatch");
	testRequire(!xrtMultipartValidateCount(
		(xbytesview){
			(const uint8*)Invalid[3].Data,
			Invalid[3].Size
		},
		&Boundary,
		NULL,
		UINT32_C(0x80000000),
		&iCount,
		&Error
	), "Multipart validation accepted unknown flags");
	xrtClearError();

	xrtMultipartLimitsInit(&Limits);
	Limits.MaxParts = 0;
	testRequire(!xrtMultipartValidate(
		(xbytesview){
			(const uint8*)FalseCandidate,
			sizeof(FalseCandidate) - 1u
		}, &Boundary, &Limits, &Error
	) && (Error.Code == XMULTIPART_ERROR_PARTS_LIMIT),
		"Multipart Part limit was not enforced");
	xrtClearError();
}



/* 执行 Multipart 整包协议测试。 */
int main(void)
{
	testMultipartBoundary();
	testMultipartMemoryContracts();
	testMultipartParse();
	testMultipartInvalid();
	printf("[PASS] multipart\n");
	return 0;
}
