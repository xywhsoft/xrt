#include "../test.h"



/* 验证 Content-Type、常用 Helper 和完整解析器能够严格往返。 */
static void testMultipartWriteRoundTrip(void)
{
	static const uint8 Binary[] = {
		UINT8_C(0x00), UINT8_C(0x41),
		UINT8_C(0x0D), UINT8_C(0x0A),
		UINT8_C(0xFF)
	};
	xmultipartboundary Boundary;
	xmultipartboundary ParsedBoundary;
	xmultiparterrorinfo Error;
	xmultipartpart Parts[2];
	char ContentType[128];
	uint8 Output[1024];
	char Name[32];
	char Filename[32];
	size_t iContentType;
	size_t iOffset = 0;
	size_t iCount = 0;
	size_t iSize;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("xrt:boundary"), &Boundary
	), "Multipart writer boundary init failed");
	testRequire(xrtMultipartContentTypeWrite(
		&Boundary, ContentType, sizeof(ContentType), &iContentType
	), "Multipart Content-Type write failed");
	testRequire(xrtMultipartBoundaryFromContentType(
		(xstrview){ ContentType, iContentType }, &ParsedBoundary
	) && (memcmp(
		&ParsedBoundary, &Boundary, sizeof(Boundary)
	) == 0), "Multipart Content-Type round trip mismatch");

	testRequire(xrtMultipartFieldWrite(
		&Boundary,
		XRT_STR_LITERAL("message"),
		(xbytesview){ (const uint8*)"hello", 5u },
		Output + iOffset,
		sizeof(Output) - iOffset,
		&iSize
	), "Multipart field write failed");
	iOffset += iSize;
	testRequire(xrtMultipartFileWrite(
		&Boundary,
		XRT_STR_LITERAL("upload"),
		XRT_STR_LITERAL("a\"b.txt"),
		XRT_STR_LITERAL("application/octet-stream"),
		(xbytesview){ Binary, sizeof(Binary) },
		Output + iOffset,
		sizeof(Output) - iOffset,
		&iSize
	), "Multipart file write failed");
	iOffset += iSize;
	testRequire(xrtMultipartCloseWrite(
		&Boundary,
		Output + iOffset,
		sizeof(Output) - iOffset,
		&iSize
	), "Multipart close write failed");
	iOffset += iSize;

	testRequire(xrtMultipartParse(
		(xbytesview){ Output, iOffset },
		&Boundary, Parts, 2, &iCount, NULL, &Error
	) && (iCount == 2), "Multipart writer output parse failed");
	testRequire(xrtMultipartPartNameWrite(
		&Parts[0], Name, sizeof(Name), &iSize
	) && (iSize == 7) &&
		(memcmp(Name, "message", 7) == 0) &&
		(Parts[0].Body.Size == 5) &&
		(memcmp(Parts[0].Body.Data, "hello", 5) == 0),
		"Multipart field round trip mismatch");
	testRequire(xrtMultipartPartFileNameWrite(
		&Parts[1], Filename, sizeof(Filename), &iSize
	) && (iSize == 7) &&
		(memcmp(Filename, "a\"b.txt", 7) == 0) &&
		(Parts[1].Body.Size == sizeof(Binary)) &&
		(memcmp(
			Parts[1].Body.Data, Binary, sizeof(Binary)
		) == 0), "Multipart file round trip mismatch");
}



/* 验证低级 Header、正文和尾部函数支持零复制式分段组合。 */
static void testMultipartWriteFragments(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_LITERAL("Content-Disposition"),
			XRT_STR_LITERAL("form-data; name=\"raw\"")
		},
		{
			XRT_STR_LITERAL("X-Meta"),
			XRT_STR_LITERAL("one")
		}
	};
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	xmultipartpart Part;
	uint8 Output[512];
	size_t iOffset = 0;
	size_t iRead = 0;
	size_t iSize;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("raw"), &Boundary
	), "Multipart fragment boundary init failed");
	testRequire(xrtMultipartPartHeadWrite(
		&Boundary, Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		Output + iOffset,
		sizeof(Output) - iOffset,
		&iSize
	), "Multipart Part Head write failed");
	iOffset += iSize;
	memcpy(Output + iOffset, "body", 4u);
	iOffset += 4u;
	testRequire(xrtMultipartPartEndWrite(
		Output + iOffset,
		sizeof(Output) - iOffset,
		&iSize
	), "Multipart Part End write failed");
	iOffset += iSize;
	testRequire(xrtMultipartCloseWrite(
		&Boundary,
		Output + iOffset,
		sizeof(Output) - iOffset,
		&iSize
	), "Multipart fragment close failed");
	iOffset += iSize;

	testRequire(xrtMultipartNext(
		(xbytesview){ Output, iOffset },
		&Boundary, &iRead, &Part, &Error
	) == XHTTP_NEXT_ITEM &&
		(Part.HeaderCount == 2) &&
		(Part.Body.Size == 4) &&
		(memcmp(Part.Body.Data, "body", 4) == 0),
		"Multipart fragments did not form a Part");
}



/* 验证查询、容量失败原子性、非法字段和重叠输入边界。 */
/* 验证标准 Part 头支持空字段名、可选文件名和媒体类型。 */
static void testMultipartWriteFormHead(void)
{
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	xmultipartpart Part;
	xstrview Filename = XRT_STR_LITERAL("a\"b.txt");
	uint8 Output[512];
	char Name[1];
	char File[32];
	size_t iOffset = 0;
	size_t iRead = 0;
	size_t iSize;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("head"), &Boundary
	), "Multipart form Head boundary init failed");
	testRequire(xrtMultipartFormHeadWrite(
		&Boundary,
		(xstrview){ NULL, 0 },
		&Filename,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		Output,
		sizeof(Output),
		&iSize
	), "Multipart form Head write failed");
	iOffset += iSize;
	memcpy(Output + iOffset, "body", 4u);
	iOffset += 4u;
	testRequire(xrtMultipartPartEndWrite(
		Output + iOffset,
		sizeof(Output) - iOffset,
		&iSize
	), "Multipart form Head Part end failed");
	iOffset += iSize;
	testRequire(xrtMultipartCloseWrite(
		&Boundary,
		Output + iOffset,
		sizeof(Output) - iOffset,
		&iSize
	), "Multipart form Head close failed");
	iOffset += iSize;
	testRequire((xrtMultipartNext(
		(xbytesview){ Output, iOffset },
		&Boundary,
		&iRead,
		&Part,
		&Error
	) == XHTTP_NEXT_ITEM) &&
		xrtMultipartPartNameWrite(
			&Part, Name, sizeof(Name), &iSize
		) && (iSize == 0) &&
		xrtMultipartPartFileNameWrite(
			&Part, File, sizeof(File), &iSize
		) && (iSize == 7) &&
		(memcmp(File, "a\"b.txt", 7) == 0) &&
		xrtHttpMediaTypeEqual(
			&Part.ContentType,
			XRT_STR_LITERAL("text"),
			XRT_STR_LITERAL("plain")
		) && (Part.Body.Size == 4) &&
		(memcmp(Part.Body.Data, "body", 4) == 0),
		"Multipart form Head round trip mismatch");
}



/* 验证查询、容量失败原子性、非法字段和重叠输入边界。 */
static void testMultipartWriteFailure(void)
{
	xmultipartboundary Boundary;
	xhttpfield Invalid = {
		XRT_STR_LITERAL("X-Test"),
		XRT_STR_LITERAL("one\r\ntwo")
	};
	uint8 Output[256];
	uint8 Before[256];
	size_t iRequired;
	size_t iSize;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("failure"), &Boundary
	), "Multipart failure boundary init failed");
	testRequire(xrtMultipartFieldWrite(
		&Boundary,
		XRT_STR_LITERAL("field"),
		(xbytesview){ (const uint8*)"value", 5u },
		NULL, 0, &iRequired
	), "Multipart field size query failed");
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	iSize = 0;
	testRequire(!xrtMultipartFieldWrite(
		&Boundary,
		XRT_STR_LITERAL("field"),
		(xbytesview){ (const uint8*)"value", 5u },
		Output, iRequired - 1u, &iSize
	) && (iSize == iRequired) &&
		(memcmp(Output, Before, sizeof(Output)) == 0),
		"Multipart short output was not failure atomic");
	xrtClearError();

	testRequire(!xrtMultipartPartHeadWrite(
		&Boundary, &Invalid, 1,
		Output, sizeof(Output), &iSize
	), "Multipart writer accepted field injection");
	xrtClearError();

	memcpy(Output, "value", 5u);
	iSize = 123u;
	testRequire(!xrtMultipartFieldWrite(
		&Boundary,
		XRT_STR_LITERAL("field"),
		(xbytesview){ Output, 5u },
		Output, 1u, &iSize
	) && (iSize == 123u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Multipart writer did not prioritize overlapping input");
	xrtClearError();
}



int main(void)
{
	testMultipartWriteRoundTrip();
	testMultipartWriteFragments();
	testMultipartWriteFormHead();
	testMultipartWriteFailure();
	printf("[PASS] multipart_write\n");
	return 0;
}
