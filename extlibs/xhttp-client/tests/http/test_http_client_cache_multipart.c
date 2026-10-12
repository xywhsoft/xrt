#include "../test.h"
#include "../../src/internal/xrt_http_client_runtime.h"



/* 测试分配器只在武装后拒绝一次指定尺寸的分配。 */
typedef struct test_http_client_cache_multipart_allocator {
	size_t Min;
	size_t Max;
	bool Armed;
	bool Failed;
} test_http_client_cache_multipart_allocator;



/* 转发普通分配，并为片段描述数组提供确定的 OOM 故障点。 */
static ptr testHttpClientCacheMultipartAlloc(
	ptr pData,
	size_t iSize
)
{
	test_http_client_cache_multipart_allocator* pState =
		(test_http_client_cache_multipart_allocator*)pData;

	if ( pState->Armed && !pState->Failed &&
		(iSize >= pState->Min) &&
		(iSize <= pState->Max) ) {
		pState->Failed = true;
		return NULL;
	}
	return malloc(iSize);
}



/* 转发普通重分配，并与初始分配共享同一个故障点。 */
static ptr testHttpClientCacheMultipartRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_http_client_cache_multipart_allocator* pState =
		(test_http_client_cache_multipart_allocator*)pData;

	if ( pState->Armed && !pState->Failed &&
		(iSize >= pState->Min) &&
		(iSize <= pState->Max) ) {
		pState->Failed = true;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放测试分配器产生的原始内存。 */
static void testHttpClientCacheMultipartFree(
	ptr pData,
	ptr pMemory
)
{
	(void)pData;
	free(pMemory);
}



/* 构造借用外层响应字段的完整 206 片段输入。 */
static xhttpcachefragmentinput
testHttpClientCacheMultipartBase(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		},
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=3600")
		}
	};
	xhttpcachefragmentinput Input;

	xrtHttpCacheFragmentInputInit(&Input);
	Input.Method = XRT_STR_LITERAL("GET");
	Input.Fields = Fields;
	Input.FieldCount =
		sizeof(Fields) / sizeof(Fields[0]);
	Input.ResponseTime = 100;
	Input.Status = XHTTP_STATUS_PARTIAL_CONTENT;
	Input.Flags =
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
		XHTTP_CACHE_FRAGMENT_BODY_COMPLETE;
	return Input;
}



/* 运行规划器并返回其拥有型结果。 */
static __xrt_http_client_cache_multipart_decision
testHttpClientCacheMultipartPlan(
	xstrview ContentType,
	xstrview Body,
	size_t iMaxParts,
	__xrt_http_client_cache_multipart* pPlan
)
{
	xhttpcachefragmentinput Input =
		testHttpClientCacheMultipartBase();

	return __xrtHttpClientCacheMultipartPlan(
		&Input,
		ContentType,
		(xbytesview){
			(const uint8*)Body.Data,
			Body.Size
		},
		iMaxParts,
		pPlan
	);
}



/* 验证乱序 Part 会按表示偏移排序并保留表示元数据。 */
static void testHttpClientCacheMultipartValid(void)
{
	static const xstrview Body = XRT_STR_INIT(
		"--b\r\n"
		"Content-Range: bytes 8-9/10\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 2\r\n"
		"\r\n"
		"89\r\n"
		"--b\r\n"
		"Content-Range: bytes 0-1/10\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"01\r\n"
		"--b--\r\n"
	);
	__xrt_http_client_cache_multipart Plan;

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			Body,
			8,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_STORE,
		"HTTP client cache multipart valid plan failed"
	);
	testRequire(
		(Plan.PartCount == 2) &&
		(Plan.Parts[0].Offset == 0) &&
		(Plan.Parts[0].Data.Size == 2) &&
		(memcmp(Plan.Parts[0].Data.Data, "01", 2) == 0) &&
		(Plan.Parts[1].Offset == 8) &&
		(Plan.Parts[1].Data.Size == 2) &&
		(memcmp(Plan.Parts[1].Data.Data, "89", 2) == 0) &&
		(Plan.Length == 10) &&
		((Plan.Flags &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_LENGTH) != 0) &&
		((Plan.Flags &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_CONTENT_TYPE) != 0) &&
		((Plan.Flags &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_COMPLETE) == 0) &&
		(Plan.ContentType.Size == 10) &&
		(memcmp(Plan.ContentType.Data, "text/plain", 10) == 0),
		"HTTP client cache multipart valid plan mismatch"
	);
	__xrtHttpClientCacheMultipartUnit(&Plan);
}



/* 验证一致重叠被裁掉，完整覆盖被识别为一个完整表示。 */
static void testHttpClientCacheMultipartNormalize(void)
{
	static const xstrview Overlap = XRT_STR_INIT(
		"--b\r\n"
		"Content-Range: bytes 0-4/10\r\n"
		"\r\n"
		"01234\r\n"
		"--b\r\n"
		"Content-Range: bytes 3-7/10\r\n"
		"\r\n"
		"34567\r\n"
		"--b--\r\n"
	);
	static const xstrview Complete = XRT_STR_INIT(
		"--b\r\n"
		"Content-Range: bytes 5-9/10\r\n"
		"\r\n"
		"56789\r\n"
		"--b\r\n"
		"Content-Range: bytes 0-4/10\r\n"
		"\r\n"
		"01234\r\n"
		"--b--\r\n"
	);
	__xrt_http_client_cache_multipart Plan;

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			Overlap,
			8,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_STORE &&
		(Plan.PartCount == 2) &&
		(Plan.Parts[0].Offset == 0) &&
		(Plan.Parts[0].Data.Size == 5) &&
		(Plan.Parts[1].Offset == 5) &&
		(Plan.Parts[1].Data.Size == 3) &&
		(memcmp(Plan.Parts[1].Data.Data, "567", 3) == 0),
		"HTTP client cache multipart overlap normalization failed"
	);
	__xrtHttpClientCacheMultipartUnit(&Plan);

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			Complete,
			8,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_STORE &&
		(Plan.PartCount == 2) &&
		((Plan.Flags &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_COMPLETE) != 0),
		"HTTP client cache multipart complete coverage failed"
	);
	__xrtHttpClientCacheMultipartUnit(&Plan);
}



/* 验证语义冲突不会产生可缓存片段。 */
static void testHttpClientCacheMultipartConflicts(void)
{
	static const xstrview Overlap = XRT_STR_INIT(
		"--b\r\n"
		"Content-Range: bytes 0-4/10\r\n"
		"\r\n"
		"01234\r\n"
		"--b\r\n"
		"Content-Range: bytes 3-7/10\r\n"
		"\r\n"
		"XX567\r\n"
		"--b--\r\n"
	);
	static const xstrview Type = XRT_STR_INIT(
		"--b\r\n"
		"Content-Range: bytes 0-1/10\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"01\r\n"
		"--b\r\n"
		"Content-Range: bytes 8-9/10\r\n"
		"Content-Type: application/json\r\n"
		"\r\n"
		"89\r\n"
		"--b--\r\n"
	);
	static const xstrview Length = XRT_STR_INIT(
		"--b\r\n"
		"Content-Range: bytes 0-1/10\r\n"
		"\r\n"
		"01\r\n"
		"--b\r\n"
		"Content-Range: bytes 8-9/11\r\n"
		"\r\n"
		"89\r\n"
		"--b--\r\n"
	);
	static const xstrview OutsideKnownLength = XRT_STR_INIT(
		"--b\r\n"
		"Content-Range: bytes 10-11/*\r\n"
		"\r\n"
		"AB\r\n"
		"--b\r\n"
		"Content-Range: bytes 0-1/10\r\n"
		"\r\n"
		"01\r\n"
		"--b--\r\n"
	);
	__xrt_http_client_cache_multipart Plan;

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			Overlap,
			8,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP &&
		((Plan.Reasons &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_OVERLAP) != 0),
		"HTTP client cache multipart accepted conflicting overlap"
	);
	xrtClearError();

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			Type,
			8,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP &&
		((Plan.Reasons &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_CONTENT_TYPE) != 0),
		"HTTP client cache multipart accepted conflicting type"
	);
	xrtClearError();

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			Length,
			8,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP &&
		((Plan.Reasons &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_LENGTH) != 0),
		"HTTP client cache multipart accepted conflicting length"
	);
	xrtClearError();

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			OutsideKnownLength,
			8,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP &&
		((Plan.Reasons &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_LENGTH) != 0),
		"HTTP client cache multipart accepted out-of-bounds Part"
	);
	xrtClearError();
}



/* 验证 Part 线路长度、编码和 Content-Range 必须无歧义。 */
static void testHttpClientCacheMultipartPartMetadata(void)
{
	static const xstrview InvalidLength = XRT_STR_INIT(
		"--b\r\n"
		"Content-Range: bytes 0-1/10\r\n"
		"Content-Length: 3\r\n"
		"\r\n"
		"01\r\n"
		"--b--\r\n"
	);
	static const xstrview Encoded = XRT_STR_INIT(
		"--b\r\n"
		"Content-Range: bytes 0-1/10\r\n"
		"Content-Transfer-Encoding: base64\r\n"
		"\r\n"
		"MDE=\r\n"
		"--b--\r\n"
	);
	static const xstrview DuplicateRange = XRT_STR_INIT(
		"--b\r\n"
		"Content-Range: bytes 0-1/10\r\n"
		"Content-Range: bytes 8-9/10\r\n"
		"\r\n"
		"01\r\n"
		"--b--\r\n"
	);
	__xrt_http_client_cache_multipart Plan;

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			InvalidLength,
			8,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP &&
		((Plan.Reasons &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_LENGTH) != 0),
		"HTTP client cache multipart accepted bad Part length"
	);
	xrtClearError();

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			Encoded,
			8,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP &&
		((Plan.Reasons &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_ENCODING) != 0),
		"HTTP client cache multipart accepted encoded Part"
	);
	xrtClearError();

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			DuplicateRange,
			8,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP &&
		((Plan.Reasons &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_RANGE) != 0),
		"HTTP client cache multipart accepted duplicate range"
	);
	xrtClearError();
}



/* 验证边界、Part 数量与非目标媒体类型的分支契约。 */
static void testHttpClientCacheMultipartLimits(void)
{
	static const xstrview TwoParts = XRT_STR_INIT(
		"--b\r\n"
		"Content-Range: bytes 0-1/10\r\n"
		"\r\n"
		"01\r\n"
		"--b\r\n"
		"Content-Range: bytes 8-9/10\r\n"
		"\r\n"
		"89\r\n"
		"--b--\r\n"
	);
	static const xstrview Unclosed = XRT_STR_INIT(
		"--b\r\n"
		"Content-Range: bytes 0-1/10\r\n"
		"\r\n"
		"01\r\n"
		"--b\r\n"
	);
	__xrt_http_client_cache_multipart Plan;

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL("application/octet-stream"),
			TwoParts,
			8,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_NONE &&
		(xrtGetError() == NULL),
		"HTTP client cache multipart non-target type failed"
	);

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL("multipart/byteranges"),
			TwoParts,
			8,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP &&
		((Plan.Reasons &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_BOUNDARY) != 0),
		"HTTP client cache multipart accepted missing boundary"
	);
	xrtClearError();

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			Unclosed,
			8,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP &&
		((Plan.Reasons &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_BODY) != 0),
		"HTTP client cache multipart accepted unclosed body"
	);
	xrtClearError();

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			TwoParts,
			1,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP &&
		((Plan.Reasons &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_PARTS) != 0) &&
		(xrtErrorIs(xrtGetError(), XERR_RANGE) != NULL),
		"HTTP client cache multipart Part limit failed"
	);
	xrtClearError();

	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			TwoParts,
			SIZE_MAX,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_STORE &&
		(Plan.PartCount == 2),
		"HTTP client cache multipart preallocated the logical limit"
	);
	__xrtHttpClientCacheMultipartUnit(&Plan);
}



/* 验证片段描述数组分配失败保持内存错误。 */
static void testHttpClientCacheMultipartOom(
	test_http_client_cache_multipart_allocator* pAllocator
)
{
	char sBody[8192];
	__xrt_http_client_cache_multipart Plan;
	size_t iSize = 0;
	size_t i;

	for ( i = 0; i < 65u; i++ ) {
		int iWritten = snprintf(
			sBody + iSize,
			sizeof(sBody) - iSize,
			"--b\r\n"
			"Content-Range: bytes %u-%u/65\r\n"
			"\r\n"
			"x\r\n",
			(uint32)i,
			(uint32)i
		);

		testRequire(
			(iWritten > 0) &&
			((size_t)iWritten <
			 (sizeof(sBody) - iSize)),
			"HTTP client cache multipart OOM body overflow"
		);
		iSize += (size_t)iWritten;
	}
	{
		int iWritten = snprintf(
			sBody + iSize,
			sizeof(sBody) - iSize,
			"--b--\r\n"
		);

		testRequire(
			(iWritten > 0) &&
			((size_t)iWritten <
			 (sizeof(sBody) - iSize)),
			"HTTP client cache multipart OOM close overflow"
		);
		iSize += (size_t)iWritten;
	}

	pAllocator->Armed = true;
	pAllocator->Failed = false;
	testRequire(
		testHttpClientCacheMultipartPlan(
			XRT_STR_LITERAL(
				"multipart/byteranges; boundary=b"
			),
			(xstrview){ sBody, iSize },
			65,
			&Plan
		) == __XRT_HTTP_CLIENT_CACHE_MULTIPART_ERROR &&
		pAllocator->Failed &&
		(xrtErrorIs(xrtGetError(), XERR_MEMORY) != NULL),
		"HTTP client cache multipart OOM contract failed"
	);
	pAllocator->Armed = false;
	xrtClearError();
}



/* 执行源站 multipart/byteranges 缓存分解测试。 */
int main(void)
{
	test_http_client_cache_multipart_allocator State = {
		1025u,
		2048u,
		false,
		false
	};
	xallocator Allocator = {
		&State,
		testHttpClientCacheMultipartAlloc,
		testHttpClientCacheMultipartRealloc,
		testHttpClientCacheMultipartFree
	};

	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP client cache multipart allocator install failed"
	);
	testHttpClientCacheMultipartValid();
	testHttpClientCacheMultipartNormalize();
	testHttpClientCacheMultipartConflicts();
	testHttpClientCacheMultipartPartMetadata();
	testHttpClientCacheMultipartLimits();
	testHttpClientCacheMultipartOom(&State);
	testMemoryDebugDrain(
		"HTTP client cache multipart leaked memory"
	);
	printf("[PASS] http client cache multipart\n");
	return 0;
}
