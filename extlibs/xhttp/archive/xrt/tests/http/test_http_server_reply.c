#include "../test.h"



/* 比较借用文本视图与零结尾常量。 */
static bool testHttpReplyTextEqual(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 验证空 Reply 只按需创建字段、原因存储和正文。 */
static void testHttpReplyLazyState(void)
{
	xhttpreplyconfig Config;
	xhttpreply* pReply;

	xrtHttpReplyConfigInit(&Config);
	testRequire(
		(Config.Headers.InitialFields == 0) &&
		(Config.Headers.InitialBytes == 0) &&
		(Config.Trailers.InitialFields == 0) &&
		(Config.Trailers.InitialBytes == 0),
		"HTTP Reply default config reserved fixed field storage"
	);
	pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	testRequire(pReply != NULL,
		"HTTP Reply create failed");
	testRequire(
		(xrtHttpReplyStatus(pReply) == XHTTP_STATUS_OK) &&
		testHttpReplyTextEqual(
			xrtHttpReplyReason(pReply), "OK"
		) &&
		(xrtHttpReplyHeaders(pReply) == NULL) &&
		(xrtHttpReplyTrailers(pReply) == NULL) &&
		(xrtHttpReplyBody(pReply) == NULL),
		"empty HTTP Reply allocated or exposed unexpected state"
	);
	testRequire(xrtHttpReplyEditHeaders(pReply) != NULL,
		"HTTP Reply mutable Header access failed");
	testRequire(
		(xrtHttpReplyHeaders(pReply) != NULL) &&
		(xrtHttpReplyHeaderCount(pReply) == 0),
		"HTTP Reply mutable Header access changed logical fields"
	);
	xrtHttpReplyDestroy(pReply);
}



/* 验证 Reply 配置支持未对齐存储、立即快照并拒绝回绕地址。 */
static void testHttpReplyMemoryContracts(void)
{
	uint8 Storage[sizeof(xhttpreplyconfig) + 2u];
	xhttpreplyconfig Config;
	xhttpreply* pReply;

	memset(Storage, 0xA5, sizeof(Storage));
	xrtHttpReplyConfigInit((xhttpreplyconfig*)(void*)(Storage + 1u));
	memcpy(&Config, Storage + 1u, sizeof(Config));
	testRequire(
		(Storage[0] == 0xA5) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5) &&
		(Config.Headers.InitialFields == 0) &&
		(Config.Headers.InitialBytes == 0) &&
		(Config.Trailers.InitialFields == 0) &&
		(Config.Trailers.InitialBytes == 0),
		"HTTP Reply config init did not support unaligned storage"
	);
	Config.Headers.MaxFields = 1u;
	Config.Trailers.MaxFields = 1u;
	memcpy(Storage + 1u, &Config, sizeof(Config));
	pReply = xrtHttpReplyCreateWithConfig(
		XHTTP_STATUS_OK,
		(const xhttpreplyconfig*)(const void*)(Storage + 1u)
	);
	testRequire((pReply != NULL) &&
		(Storage[0] == 0xA5) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5),
		"HTTP Reply did not accept an unaligned config"
	);
	memset(Storage + 1u, 0, sizeof(Config));
	testRequire(xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("X-Test"),
		XRT_STR_LITERAL("header")
	) && xrtHttpReplyAddTrailer(
		pReply,
		XRT_STR_LITERAL("X-Test"),
		XRT_STR_LITERAL("trailer")
	), "HTTP Reply retained caller config storage");
	xrtHttpReplyDestroy(pReply);

	xrtHttpReplyConfigInit((xhttpreplyconfig*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP Reply config init accepted wrapping output");
	xrtClearError();
	testRequire((xrtHttpReplyCreateWithConfig(
		XHTTP_STATUS_OK,
		(const xhttpreplyconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Reply create accepted wrapping config");
	xrtClearError();
}



/* 验证状态、原因短语、重复字段和 Trailer 构建语义。 */
static void testHttpReplyFields(void)
{
	xhttpreply* pReply = xrtHttpReplyCreate(
		XHTTP_STATUS_CREATED
	);
	const xhttpfield* pField;

	testRequire(pReply != NULL,
		"HTTP Reply field fixture create failed");
	testRequire(xrtHttpReplySetStatus(
		pReply, XHTTP_STATUS_NOT_FOUND
	) && testHttpReplyTextEqual(
		xrtHttpReplyReason(pReply), "Not Found"
	), "HTTP Reply status did not restore standard reason");
	testRequire(xrtHttpReplySetReason(
		pReply, XRT_STR_LITERAL("Missing Resource")
	) && testHttpReplyTextEqual(
		xrtHttpReplyReason(pReply), "Missing Resource"
	), "HTTP Reply custom reason mismatch");
	testRequire(
		!xrtHttpReplySetReason(
			pReply, XRT_STR_LITERAL("bad\r\nreason")
		) && testHttpReplyTextEqual(
			xrtHttpReplyReason(pReply), "Missing Resource"
		), "HTTP Reply invalid reason changed visible state");
	xrtClearError();

	testRequire(
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Set-Cookie"),
			XRT_STR_LITERAL("a=1")
		) && xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("set-cookie"),
			XRT_STR_LITERAL("b=2")
		) && (xrtHttpReplyHeaderCount(pReply) == 2),
		"HTTP Reply did not preserve repeated Header fields"
	);
	testRequire(xrtHttpReplySetHeader(
		pReply,
		XRT_STR_LITERAL("SET-COOKIE"),
		XRT_STR_LITERAL("c=3")
	) && (xrtHttpReplyHeaderCount(pReply) == 1),
		"HTTP Reply Header Set did not fold duplicates");
	pField = xrtHttpReplyHeader(
		pReply, XRT_STR_LITERAL("set-cookie")
	);
	testRequire((pField != NULL) &&
		testHttpReplyTextEqual(pField->Value, "c=3") &&
		(xrtHttpReplyHeaderData(pReply) == pField) &&
		(xrtHttpReplyHeaderAt(pReply, 0) == pField),
		"HTTP Reply Header lookup mismatch");

	testRequire(
		xrtHttpReplyAddTrailer(
			pReply,
			XRT_STR_LITERAL("Digest"),
			XRT_STR_LITERAL("sha-256=:AA==:")
		) && (xrtHttpReplyTrailerCount(pReply) == 1) &&
		(xrtHttpReplyTrailerData(pReply) ==
			xrtHttpReplyTrailerAt(pReply, 0)) &&
		(xrtHttpReplyTrailerAt(pReply, 0) != NULL),
		"HTTP Reply Trailer construction failed"
	);
	testRequire(
		(xrtHttpReplyRemoveHeader(
			pReply, XRT_STR_LITERAL("Set-Cookie")
		) == 1) &&
		(xrtHttpReplyRemoveTrailer(
			pReply, XRT_STR_LITERAL("Digest")
		) == 1),
		"HTTP Reply field removal mismatch"
	);
	xrtHttpReplyDestroy(pReply);
	testRequire(
		(xrtHttpReplyHeaderData(NULL) == NULL) &&
		(xrtHttpReplyTrailerData(NULL) == NULL),
		"HTTP Reply field data accepted null Reply"
	);
}



/* 验证正文引用、字节便利入口和深克隆边界。 */
static void testHttpReplyBodyAndClone(void)
{
	xhttpreply* pReply = xrtHttpReplyCreate(
		XHTTP_STATUS_OK
	);
	xhttpreply* pClone;
	xhttpbody* pBody;
	const xhttpfield* pType;

	testRequire((pReply != NULL) &&
		xrtHttpReplySetBytes(
			pReply,
			(xbytesview){
				(cbytes)"{\"code\":200}",
				12
			},
			XRT_STR_LITERAL(
				"application/json; charset=utf-8"
			)
		), "HTTP Reply byte body setup failed");
	pType = xrtHttpReplyHeader(
		pReply, XRT_STR_LITERAL("Content-Type")
	);
	testRequire(
		(pType != NULL) &&
		testHttpReplyTextEqual(
			pType->Value,
			"application/json; charset=utf-8"
		) &&
		(xrtHttpBodyLength(
			xrtHttpReplyBody(pReply)
		) == 12),
		"HTTP Reply byte body metadata mismatch"
	);

	pBody = xrtHttpBodyBorrow(
		(xbytesview){ (cbytes)"borrowed", 8 }
	);
	testRequire((pBody != NULL) &&
		xrtHttpReplySetBody(pReply, pBody),
		"HTTP Reply body reference setup failed");
	xrtHttpBodyDestroy(pBody);
	testRequire(xrtHttpBodyLength(
		xrtHttpReplyBody(pReply)
	) == 8, "HTTP Reply did not retain its own body reference");

	testRequire(
		xrtHttpReplySetReason(
			pReply, XRT_STR_LITERAL("Custom")
		) && xrtHttpReplyAddTrailer(
			pReply,
			XRT_STR_LITERAL("X-End"),
			XRT_STR_LITERAL("yes")
		), "HTTP Reply clone fixture setup failed");
	pClone = xrtHttpReplyClone(pReply);
	testRequire(pClone != NULL,
		"HTTP Reply clone failed");
	testRequire(
		xrtHttpReplySetReason(
			pReply, XRT_STR_LITERAL("Changed")
		) && xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("text/plain")
		), "HTTP Reply source mutation failed");
	testRequire(
		testHttpReplyTextEqual(
			xrtHttpReplyReason(pClone), "Custom"
		) && testHttpReplyTextEqual(
			xrtHttpReplyHeader(
				pClone,
				XRT_STR_LITERAL("Content-Type")
			)->Value,
			"application/json; charset=utf-8"
		) && (xrtHttpReplyTrailerCount(pClone) == 1) &&
		(xrtHttpBodyLength(
			xrtHttpReplyBody(pClone)
		) == 8),
		"HTTP Reply clone shared mutable state or lost body"
	);
	xrtHttpReplyDestroy(pClone);
	xrtHttpReplyDestroy(pReply);
}



/* 验证状态范围和配置边界在创建前失败。 */
static void testHttpReplyContracts(void)
{
	xhttpreplyconfig Config;
	xhttpreply* pReply;

	testRequire(xrtHttpReplyCreate(99) == NULL,
		"HTTP Reply accepted a two-digit status");
	xrtClearError();
	xrtHttpReplyConfigInit(&Config);
	Config.Headers.InitialFields =
		Config.Headers.MaxFields + 1u;
	testRequire(xrtHttpReplyCreateWithConfig(
		XHTTP_STATUS_OK, &Config
	) == NULL, "HTTP Reply accepted invalid Header config");
	xrtClearError();
	pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	testRequire(pReply != NULL,
		"HTTP Reply lookup contract fixture failed");
	testRequire(
		(xrtHttpReplyHeader(
			pReply, XRT_STR_LITERAL("Bad Name")
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtHttpReplyHeaders(pReply) == NULL),
		"empty HTTP Reply Header lookup skipped name validation"
	);
	xrtClearError();
	testRequire(
		(xrtHttpReplyTrailer(
			pReply, (xstrview){ NULL, 1 }
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtHttpReplyTrailers(pReply) == NULL),
		"empty HTTP Reply Trailer lookup accepted an invalid view"
	);
	xrtClearError();
	testRequire(
		(xrtHttpReplyEditHeaders(pReply) != NULL) &&
		(xrtHttpReplyHeader(
			pReply, XRT_STR_LITERAL("Bad Name")
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"created HTTP Reply Header lookup changed validation semantics"
	);
	xrtClearError();
	xrtHttpReplyDestroy(pReply);
	testRequire(
		(xrtHttpReplyStatus(NULL) == 0) &&
		(xrtHttpReplyHeaderCount(NULL) == 0) &&
		(xrtHttpReplyTrailerCount(NULL) == 0) &&
		(xrtHttpReplyBody(NULL) == NULL),
		"HTTP Reply null queries returned non-empty state"
	);
	xrtHttpReplyDestroy(NULL);
}



/* 运行无 I/O 服务端 Reply 构建器测试。 */
int main(void)
{
	testHttpReplyLazyState();
	testHttpReplyMemoryContracts();
	testHttpReplyFields();
	testHttpReplyBodyAndClone();
	testHttpReplyContracts();
	printf("[PASS] HTTP server Reply\n");
	return 0;
}
