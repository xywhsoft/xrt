#include "../test.h"

#include <xrt/http_cache_store.h>



/* 使用给定 Key、字段和正文创建普通完整响应记录。 */
static xhttpcacherecord* testHttpCacheRecordCreate(
	const xhttpcachekey* pKey,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttpcachepart* pParts,
	size_t iPartCount,
	uint64 iLength
)
{
	xhttpcacherecordinput Input;

	testRequire(
		xrtHttpCacheRecordInputInit(
			&Input, pKey, XHTTP_STATUS_OK
		),
		"HTTP cache record input init failed"
	);
	Input.Flags = XHTTP_CACHE_RECORD_HAS_LENGTH |
		XHTTP_CACHE_RECORD_COMPLETE;
	Input.Reason = XRT_STR_LITERAL("OK");
	Input.Fields = pFields;
	Input.FieldCount = iFieldCount;
	Input.Parts = pParts;
	Input.PartCount = iPartCount;
	Input.Length = iLength;
	Input.ResponseTime = INT64_C(1000000);
	Input.RequestClock = 100;
	Input.ResponseClock = 200;
	return xrtHttpCacheRecordCreate(&Input);
}



/* 验证紧凑副本、Vary 选择、正文片段和协议 Entry 视图。 */
static void testHttpCacheRecordPacked(void)
{
	char Method[] = "GET";
	char URI[] = "https://example.test/data";
	char Partition[] = "site-a";
	char Encoding[] = " gzip ";
	char Language[] = "en";
	char BodyA[] = "hello";
	char BodyB[] = "world";
	xhttpfield RequestFields[] = {
		{
			{ "Accept-Encoding", 15 },
			{ Encoding, 6 }
		},
		{
			{ "Accept-Language", 15 },
			{ Language, 2 }
		}
	};
	xhttpfield ResponseFields[] = {
		{
			XRT_STR_INIT("Vary"),
			XRT_STR_INIT("Accept-Encoding, Accept-Language")
		},
		{
			XRT_STR_INIT("Vary"),
			XRT_STR_INIT("accept-encoding")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Thu, 01 Jan 1970 00:00:02 GMT")
		},
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=60")
		}
	};
	xhttpcachepart Parts[] = {
		{ 0, { (cbytes)BodyA, 5 } },
		{ 5, { (cbytes)BodyB, 5 } }
	};
	xhttpcachekey Key;
	xhttpcachekey Query;
	xhttpcacherecord* pRecord;
	xhttpcacherecord* pHeld;
	xhttpcacheentry Entry;
	const xhttpcachepart* pPart;

	testRequire(
		xrtHttpCacheKeyInit(
			&Key,
			(xstrview){ Method, 3 },
			(xstrview){ URI, strlen(URI) }
		),
		"HTTP cache record key init failed"
	);
	Key.Partition = (xstrview){
		Partition, strlen(Partition)
	};
	Key.Fields = RequestFields;
	Key.FieldCount = sizeof(RequestFields) /
		sizeof(RequestFields[0]);
	pRecord = testHttpCacheRecordCreate(
		&Key,
		ResponseFields,
		sizeof(ResponseFields) / sizeof(ResponseFields[0]),
		Parts,
		sizeof(Parts) / sizeof(Parts[0]),
		10
	);
	testRequire(pRecord != NULL,
		"HTTP cache packed record create failed");

	memset(Method, 'X', 3);
	memset(URI, 'X', strlen(URI));
	memset(Partition, 'X', strlen(Partition));
	memset(Encoding, 'X', 6);
	memset(Language, 'X', 2);
	memset(BodyA, 'X', 5);
	memset(BodyB, 'X', 5);
	testRequire(
		xrtHttpCacheRecordCharge(pRecord) >
			sizeof(xhttpcacherecordinput) &&
		xrtHttpCacheRecordVersion(pRecord) ==
			XHTTP_VERSION_1_1 &&
		xrtHttpCacheRecordStatus(pRecord) ==
			XHTTP_STATUS_OK &&
		xrtHttpCacheRecordBodyBytes(pRecord) == 10 &&
		xrtHttpCacheRecordLength(pRecord) == 10 &&
		xrtHttpCacheRecordPartCount(pRecord) == 2 &&
		xrtHttpCacheRecordVaryCount(pRecord) == 2,
		"HTTP cache packed record metadata mismatch"
	);
	pPart = xrtHttpCacheRecordPartAt(pRecord, 0);
	testRequire(
		(pPart != NULL) &&
		(pPart->Offset == 0) &&
		(pPart->Data.Size == 5) &&
		(memcmp(pPart->Data.Data, "hello", 5) == 0),
		"HTTP cache packed record did not own body"
	);
	testRequire(
		xrtHttpFieldNameEqual(
			xrtHttpCacheRecordVaryAt(pRecord, 0),
			XRT_STR_LITERAL("Accept-Encoding")
		) &&
		xrtHttpFieldNameEqual(
			xrtHttpCacheRecordVaryAt(pRecord, 1),
			XRT_STR_LITERAL("Accept-Language")
		),
		"HTTP cache packed record Vary deduplication failed"
	);
	testRequire(
		xrtHttpCacheRecordEntry(pRecord, &Entry) &&
		(Entry.Fields ==
		 xrtHttpCacheRecordFieldAt(pRecord, 0)) &&
		(Entry.FieldCount == 4) &&
		(Entry.Flags == 0),
		"HTTP cache packed record Entry view mismatch"
	);

	testRequire(
		xrtHttpCacheKeyInit(
			&Query,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("https://example.test/data")
		),
		"HTTP cache record query init failed"
	);
	Query.Partition = XRT_STR_LITERAL("site-a");
	{
		static const xhttpfield QueryFields[] = {
			{
				XRT_STR_INIT("accept-encoding"),
				XRT_STR_INIT("gzip")
			},
			{
				XRT_STR_INIT("ACCEPT-LANGUAGE"),
				XRT_STR_INIT("en")
			}
		};

		Query.Fields = QueryFields;
		Query.FieldCount = sizeof(QueryFields) /
			sizeof(QueryFields[0]);
		testRequire(
			xrtHttpCacheRecordMatches(pRecord, &Query),
			"HTTP cache record Vary OWS match failed"
		);
	}
	Query.FieldCount = 1;
	testRequire(
		!xrtHttpCacheRecordMatches(pRecord, &Query),
		"HTTP cache record matched absent Vary field"
	);
	Query.FieldCount = 0;
	testRequire(
		!xrtHttpCacheRecordMatches(pRecord, &Query),
		"HTTP cache record matched empty request selector"
	);
	Query.Partition = XRT_STR_LITERAL("site-b");
	testRequire(
		!xrtHttpCacheRecordMatches(pRecord, &Query),
		"HTTP cache record ignored partition"
	);

	pHeld = xrtHttpCacheRecordRetain(pRecord);
	testRequire(pHeld == pRecord,
		"HTTP cache record retain failed");
	xrtHttpCacheRecordRelease(pHeld);
	xrtHttpCacheRecordRelease(pRecord);
}



/* 验证部分表示、完整覆盖、Vary 星号和非法范围边界。 */
static void testHttpCacheRecordBoundaries(void)
{
	static const uint8 Body[] = { 1, 2, 3 };
	static const xhttpfield Wildcard[] = {
		{
			XRT_STR_INIT("Vary"),
			XRT_STR_INIT("*")
		}
	};
	xhttpcachepart Part = { 2, { Body, sizeof(Body) } };
	xhttpcachekey Key;
	xhttpcacherecordinput Input;
	xhttpcacherecord* pRecord;
	xhttpcacheentry Entry;

	testRequire(
		xrtHttpCacheKeyInit(
			&Key,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("https://example.test/partial")
		) &&
		xrtHttpCacheRecordInputInit(
			&Input, &Key, XHTTP_STATUS_OK
		),
		"HTTP cache partial input init failed"
	);
	Input.Flags = XHTTP_CACHE_RECORD_HAS_LENGTH;
	Input.Parts = &Part;
	Input.PartCount = 1;
	Input.Length = 10;
	pRecord = xrtHttpCacheRecordCreate(&Input);
	testRequire(
		(pRecord != NULL) &&
		xrtHttpCacheRecordEntry(pRecord, &Entry) &&
		((Entry.Flags & XHTTP_CACHE_ENTRY_PARTIAL) != 0),
		"HTTP cache partial record was not marked partial"
	);
	xrtHttpCacheRecordRelease(pRecord);

	Input.Flags |= XHTTP_CACHE_RECORD_COMPLETE;
	testRequire(
		xrtHttpCacheRecordCreate(&Input) == NULL,
		"HTTP cache complete record accepted a leading gap"
	);
	xrtClearError();

	Input.Flags = XHTTP_CACHE_RECORD_HAS_LENGTH |
		XHTTP_CACHE_RECORD_COMPLETE;
	Input.Parts = NULL;
	Input.PartCount = 0;
	Input.Length = 0;
	pRecord = xrtHttpCacheRecordCreate(&Input);
	testRequire(
		(pRecord != NULL) &&
		((xrtHttpCacheRecordFlags(pRecord) &
		  XHTTP_CACHE_RECORD_COMPLETE) != 0),
		"HTTP cache empty complete record failed"
	);
	xrtHttpCacheRecordRelease(pRecord);

	Input.Fields = Wildcard;
	Input.FieldCount = 1;
	testRequire(
		xrtHttpCacheRecordCreate(&Input) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"HTTP cache record accepted Vary wildcard"
	);
	xrtClearError();

	Part.Offset = UINT64_MAX - 1u;
	Part.Data.Size = 3;
	Input.Fields = NULL;
	Input.FieldCount = 0;
	Input.Flags = 0;
	Input.Parts = &Part;
	Input.PartCount = 1;
	testRequire(
		xrtHttpCacheRecordCreate(&Input) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP cache record missed range overflow"
	);
	xrtClearError();
}



/* 验证初始化和查询保留最精确的错误种类。 */
static void testHttpCacheRecordErrors(void)
{
	xhttpcachekey Key;
	xhttpcacherecordinput Input;
	xhttpcacherecord* pOutput = (xhttpcacherecord*)(uintptr_t)1;
	xhttpcache* pCache;

	testRequire(
		!xrtHttpCacheKeyInit(
			&Key,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("https://example.test/bad uri")
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP cache key hid invalid URI error"
	);
	xrtClearError();

	testRequire(
		!xrtHttpCacheKeyInit(
			&Key,
			XRT_STR_LITERAL("GE T"),
			XRT_STR_LITERAL("https://example.test/data")
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP cache key hid invalid method error"
	);
	xrtClearError();

	testRequire(
		xrtHttpCacheKeyInit(
			&Key,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("https://example.test/data")
		),
		"HTTP cache valid key init failed"
	);
	testRequire(
		!xrtHttpCacheRecordInputInit(&Input, &Key, 199) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP cache input status did not report range"
	);
	xrtClearError();

	pCache = xrtHttpCacheCreate(NULL);
	testRequire(pCache != NULL,
		"HTTP cache error-test store create failed");
	Key.URI = XRT_STR_LITERAL("https://example.test/bad uri");
	testRequire(
		(xrtHttpCacheGet(
			pCache, &Key, &pOutput
		) == XHTTP_CACHE_LOOKUP_ERROR) &&
		(pOutput == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP cache query hid URI error or kept stale output"
	);
	xrtClearError();
	xrtHttpCacheRelease(pCache);
}



/* 验证缓存固定描述符支持未对齐存储、立即快照和回绕拒绝。 */
static void testHttpCacheRecordMemoryContracts(void)
{
	static const uint8 Body[] = { 1u, 2u, 3u };
	uint8 ConfigStorage[sizeof(xhttpcacheconfig) + 2u];
	uint8 KeyStorage[sizeof(xhttpcachekey) + 2u];
	uint8 InputStorage[sizeof(xhttpcacherecordinput) + 2u];
	uint8 PartStorage[sizeof(xhttpcachepart) + 2u];
	uint8 EntryStorage[sizeof(xhttpcacheentry) + 2u];
	xhttpcacheconfig Config;
	xhttpcachekey Key;
	xhttpcacherecordinput Input;
	xhttpcachepart Part = { 0, { Body, sizeof(Body) } };
	xhttpcacheentry Entry;
	xhttpcacherecord* pRecord;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memset(KeyStorage, 0xA5, sizeof(KeyStorage));
	memset(InputStorage, 0xA5, sizeof(InputStorage));
	memset(PartStorage, 0xA5, sizeof(PartStorage));
	memset(EntryStorage, 0xA5, sizeof(EntryStorage));
	xrtHttpCacheConfigInit(
		(xhttpcacheconfig*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire(
		(Config.InitialEntries == 64u) &&
		(Config.MaxEntries == XHTTP_CACHE_ENTRIES_DEFAULT) &&
		(ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5),
		"HTTP cache config init rejected unaligned storage"
	);
	testRequire(xrtHttpCacheKeyInit(
		(xhttpcachekey*)(void*)(KeyStorage + 1u),
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/memory")
	), "HTTP cache key init rejected unaligned storage");
	testRequire(xrtHttpCacheRecordInputInit(
		(xhttpcacherecordinput*)(void*)(InputStorage + 1u),
		(const xhttpcachekey*)(const void*)(KeyStorage + 1u),
		XHTTP_STATUS_OK
	), "HTTP cache record input rejected unaligned descriptors");
	memcpy(&Input, InputStorage + 1u, sizeof(Input));
	Input.Flags = XHTTP_CACHE_RECORD_HAS_LENGTH |
		XHTTP_CACHE_RECORD_COMPLETE;
	Input.Parts = (const xhttpcachepart*)(const void*)(
		PartStorage + 1u
	);
	Input.PartCount = 1u;
	Input.Length = sizeof(Body);
	memcpy(PartStorage + 1u, &Part, sizeof(Part));
	memcpy(InputStorage + 1u, &Input, sizeof(Input));
	memset(KeyStorage + 1u, 0, sizeof(Key));
	pRecord = xrtHttpCacheRecordCreate(
		(const xhttpcacherecordinput*)(const void*)(
			InputStorage + 1u
		)
	);
	memset(InputStorage + 1u, 0, sizeof(Input));
	memset(PartStorage + 1u, 0, sizeof(Part));
	testRequire(
		(pRecord != NULL) &&
		(xrtHttpCacheRecordBodyBytes(pRecord) == sizeof(Body)) &&
		(xrtHttpCacheRecordLength(pRecord) == sizeof(Body)),
		"HTTP cache record did not snapshot unaligned input"
	);
	testRequire(xrtHttpCacheRecordEntry(
		pRecord,
		(xhttpcacheentry*)(void*)(EntryStorage + 1u)
	), "HTTP cache record Entry rejected unaligned output");
	memcpy(&Entry, EntryStorage + 1u, sizeof(Entry));
	testRequire((Entry.Flags == 0) &&
		(EntryStorage[0] == 0xA5) &&
		(EntryStorage[sizeof(EntryStorage) - 1u] == 0xA5),
		"HTTP cache record Entry published invalid output");
	xrtHttpCacheRecordRelease(pRecord);

	xrtHttpCacheConfigInit(
		(xhttpcacheconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP cache config init accepted a wrapping output");
	xrtClearError();
	testRequire(!xrtHttpCacheKeyInit(
		(xhttpcachekey*)(uintptr_t)(UINTPTR_MAX - 1u),
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test")
	), "HTTP cache key init accepted a wrapping output");
	xrtClearError();
	testRequire(!xrtHttpCacheRecordInputInit(
		&Input,
		(const xhttpcachekey*)(uintptr_t)(UINTPTR_MAX - 1u),
		XHTTP_STATUS_OK
	), "HTTP cache record input accepted a wrapping key");
	xrtClearError();
	testRequire(xrtHttpCacheRecordCreate(
		(const xhttpcacherecordinput*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL, "HTTP cache record accepted a wrapping input");
	xrtClearError();
	testRequire(!xrtHttpCacheRecordEntry(
		(xhttpcacherecord*)(uintptr_t)1u,
		(xhttpcacheentry*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP cache record Entry accepted a wrapping output");
	xrtClearError();
}



/* 执行不可变 Record 的所有权与边界契约。 */
int main(void)
{
	testHttpCacheRecordPacked();
	testHttpCacheRecordBoundaries();
	testHttpCacheRecordErrors();
	testHttpCacheRecordMemoryContracts();
	printf("[PASS] HTTP cache store record\n");
	return 0;
}
