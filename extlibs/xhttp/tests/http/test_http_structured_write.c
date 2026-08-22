#include "../test.h"

#include <xrt/http_structured.h>



/* 查询长度后写出并比较精确线路字节。 */
static void testStructuredWriteBareValue(
	const xhttpstructuredvalue* pValue,
	xstrview Expected
)
{
	char arrOutput[256];
	size_t iSize;

	testRequire(
		xrtHttpStructuredBareWrite(
			pValue, NULL, 0, &iSize
		) && (iSize == Expected.Size),
		"structured bare write size mismatch"
	);
	testRequire(
		xrtHttpStructuredBareWrite(
			pValue, arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == Expected.Size) &&
		((iSize == 0) ||
		 (memcmp(arrOutput, Expected.Data, iSize) == 0)),
		"structured bare write bytes mismatch"
	);
}



/* 验证八种裸值都产生规范线路形式。 */
static void testStructuredWriteBare(void)
{
	xhttpstructuredvalue Value;
	static const unsigned char Binary[] = { 'a', 'b', 'c', 'd' };
	static const char Display[] = {
		'T', 'h', 'i', 's', ' ', (char)0xC3u, (char)0xBCu, 's'
	};

	memset(&Value, 0, sizeof(Value));
	Value.Type = XHTTP_STRUCTURED_INTEGER;
	Value.Number = -42;
	testStructuredWriteBareValue(&Value, XRT_STR_LITERAL("-42"));

	Value.Type = XHTTP_STRUCTURED_DECIMAL;
	Value.Number = -12340;
	testStructuredWriteBareValue(&Value, XRT_STR_LITERAL("-12.34"));
	Value.Number = 4000;
	testStructuredWriteBareValue(&Value, XRT_STR_LITERAL("4.0"));

	Value.Type = XHTTP_STRUCTURED_STRING;
	Value.Number = 0;
	Value.Data = XRT_STR_LITERAL("a\\b\"c");
	testStructuredWriteBareValue(
		&Value, XRT_STR_LITERAL("\"a\\\\b\\\"c\"")
	);

	Value.Type = XHTTP_STRUCTURED_TOKEN;
	Value.Data = XRT_STR_LITERAL("HTTP/1.1:ok");
	testStructuredWriteBareValue(
		&Value, XRT_STR_LITERAL("HTTP/1.1:ok")
	);

	Value.Type = XHTTP_STRUCTURED_BYTES;
	Value.Data = (xstrview){
		(const char*)Binary, sizeof(Binary)
	};
	testStructuredWriteBareValue(
		&Value, XRT_STR_LITERAL(":YWJjZA==:")
	);

	Value.Type = XHTTP_STRUCTURED_BOOLEAN;
	Value.Data = (xstrview){ NULL, 0 };
	Value.Number = 0;
	testStructuredWriteBareValue(&Value, XRT_STR_LITERAL("?0"));

	Value.Type = XHTTP_STRUCTURED_DATE;
	Value.Number = INT64_C(1659578233);
	testStructuredWriteBareValue(
		&Value, XRT_STR_LITERAL("@1659578233")
	);

	Value.Type = XHTTP_STRUCTURED_DISPLAY;
	Value.Number = 0;
	Value.Data = (xstrview){ Display, sizeof(Display) };
	testStructuredWriteBareValue(
		&Value, XRT_STR_LITERAL("%\"This %c3%bcs\"")
	);
}



/* 验证 Item 参数省略 true，并保持参数顺序。 */
static void testStructuredWriteItem(void)
{
	xhttpstructuredparameterentry Parameters[3];
	xhttpstructureditemvalue Item;
	char arrOutput[128];
	size_t iSize;

	memset(Parameters, 0, sizeof(Parameters));
	Parameters[0].Key = XRT_STR_LITERAL("a");
	Parameters[0].Value.Type = XHTTP_STRUCTURED_BOOLEAN;
	Parameters[0].Value.Number = 1;
	Parameters[1].Key = XRT_STR_LITERAL("b");
	Parameters[1].Value.Type = XHTTP_STRUCTURED_BOOLEAN;
	Parameters[1].Value.Number = 0;
	Parameters[2].Key = XRT_STR_LITERAL("name");
	Parameters[2].Value.Type = XHTTP_STRUCTURED_STRING;
	Parameters[2].Value.Data = XRT_STR_LITERAL("x");

	memset(&Item, 0, sizeof(Item));
	Item.Bare.Type = XHTTP_STRUCTURED_INTEGER;
	Item.Bare.Number = 1;
	Item.Parameters = Parameters;
	Item.ParameterCount = 3u;
	testRequire(
		xrtHttpStructuredItemWrite(
			&Item, arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == sizeof("1;a;b=?0;name=\"x\"") - 1u) &&
		(memcmp(
			arrOutput, "1;a;b=?0;name=\"x\"", iSize
		) == 0),
		"structured Item write mismatch"
	);
}



/* 验证 List、Inner List 和外层参数的规范组合。 */
static void testStructuredWriteList(void)
{
	xhttpstructuredparameterentry InnerParameter;
	xhttpstructureditemvalue Inner[2];
	xhttpstructuredmembervalue Members[2];
	char arrOutput[128];
	size_t iSize;

	memset(&InnerParameter, 0, sizeof(InnerParameter));
	InnerParameter.Key = XRT_STR_LITERAL("lvl");
	InnerParameter.Value.Type = XHTTP_STRUCTURED_INTEGER;
	InnerParameter.Value.Number = 5;
	memset(Inner, 0, sizeof(Inner));
	Inner[0].Bare.Type = XHTTP_STRUCTURED_STRING;
	Inner[0].Bare.Data = XRT_STR_LITERAL("foo");
	Inner[1].Bare.Type = XHTTP_STRUCTURED_TOKEN;
	Inner[1].Bare.Data = XRT_STR_LITERAL("bar");
	memset(Members, 0, sizeof(Members));
	Members[0].Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Members[0].Item.Bare.Type = XHTTP_STRUCTURED_TOKEN;
	Members[0].Item.Bare.Data = XRT_STR_LITERAL("first");
	Members[1].Kind = XHTTP_STRUCTURED_MEMBER_INNER_LIST;
	Members[1].Inner = Inner;
	Members[1].InnerCount = 2u;
	Members[1].Parameters = &InnerParameter;
	Members[1].ParameterCount = 1u;

	testRequire(
		xrtHttpStructuredListWrite(
			Members, 2, arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == sizeof("first, (\"foo\" bar);lvl=5") - 1u) &&
		(memcmp(
			arrOutput, "first, (\"foo\" bar);lvl=5", iSize
		) == 0) && xrtHttpStructuredListValid(
			(xstrview){ arrOutput, iSize }
		),
		"structured List write mismatch"
	);
}



/* 验证 Dictionary 省略 true、保留参数并生成可回读值。 */
static void testStructuredWriteDictionary(void)
{
	xhttpstructuredparameterentry Flag;
	xhttpstructureddictionaryentry Entries[3];
	char arrOutput[128];
	size_t iSize;

	memset(&Flag, 0, sizeof(Flag));
	Flag.Key = XRT_STR_LITERAL("fast");
	Flag.Value.Type = XHTTP_STRUCTURED_BOOLEAN;
	Flag.Value.Number = 1;
	memset(Entries, 0, sizeof(Entries));
	Entries[0].Key = XRT_STR_LITERAL("u");
	Entries[0].Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Entries[0].Member.Item.Bare.Type = XHTTP_STRUCTURED_INTEGER;
	Entries[0].Member.Item.Bare.Number = 3;
	Entries[1].Key = XRT_STR_LITERAL("i");
	Entries[1].Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Entries[1].Member.Item.Bare.Type = XHTTP_STRUCTURED_BOOLEAN;
	Entries[1].Member.Item.Bare.Number = 1;
	Entries[1].Member.Item.Parameters = &Flag;
	Entries[1].Member.Item.ParameterCount = 1u;
	Entries[2].Key = XRT_STR_LITERAL("label");
	Entries[2].Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Entries[2].Member.Item.Bare.Type = XHTTP_STRUCTURED_STRING;
	Entries[2].Member.Item.Bare.Data = XRT_STR_LITERAL("work");

	testRequire(
		xrtHttpStructuredDictionaryWrite(
			Entries, 3, arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == sizeof("u=3, i;fast, label=\"work\"") - 1u) &&
		(memcmp(
			arrOutput, "u=3, i;fast, label=\"work\"", iSize
		) == 0) && xrtHttpStructuredDictionaryValid(
			(xstrview){ arrOutput, iSize }
		),
		"structured Dictionary write mismatch"
	);
}



/* 验证容量、重复 key、输入重叠和未对齐描述符边界。 */
static void testStructuredWriteEdges(void)
{
	xhttpstructureddictionaryentry Duplicate[2];
	xhttpstructuredmembervalue Member;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(Member) + 1u];
	} Storage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(size_t) + 1u];
	} SizeStorage;
	size_t* pUnalignedSize =
		(size_t*)(SizeStorage.Bytes + 1u);
	char arrOutput[32];
	char arrBefore[32];
	size_t iSize;

	memset(&Member, 0, sizeof(Member));
	Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Member.Item.Bare.Type = XHTTP_STRUCTURED_STRING;
	Member.Item.Bare.Data = XRT_STR_LITERAL("abc");
	memcpy(Storage.Bytes + 1u, &Member, sizeof(Member));
	testRequire(
		xrtHttpStructuredListWrite(
			(const xhttpstructuredmembervalue*)(Storage.Bytes + 1u),
			1, arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == 5u) &&
		(memcmp(arrOutput, "\"abc\"", 5u) == 0),
		"structured writer rejected unaligned descriptors"
	);
	testRequire(
		xrtHttpStructuredListWrite(
			&Member, 1, arrOutput, sizeof(arrOutput),
			pUnalignedSize
		),
		"structured writer rejected unaligned length output"
	);
	memcpy(&iSize, pUnalignedSize, sizeof(iSize));
	testRequire(
		(iSize == 5u) && (memcmp(arrOutput, "\"abc\"", 5u) == 0),
		"structured writer unaligned length mismatch"
	);

	memset(arrOutput, 0xA5, sizeof(arrOutput));
	memcpy(arrBefore, arrOutput, sizeof(arrOutput));
	iSize = 99u;
	testRequire(
		!xrtHttpStructuredListWrite(
			&Member, 1, arrOutput, 4u, &iSize
		) && (iSize == 5u) &&
		(memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0),
		"structured writer capacity failure was not atomic"
	);
	xrtClearError();

	memset(Duplicate, 0, sizeof(Duplicate));
	Duplicate[0].Key = XRT_STR_LITERAL("a");
	Duplicate[1].Key = XRT_STR_LITERAL("a");
	Duplicate[0].Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Duplicate[1].Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Duplicate[0].Member.Item.Bare.Type = XHTTP_STRUCTURED_BOOLEAN;
	Duplicate[1].Member.Item.Bare.Type = XHTTP_STRUCTURED_BOOLEAN;
	Duplicate[0].Member.Item.Bare.Number = 1;
	Duplicate[1].Member.Item.Bare.Number = 1;
	iSize = 99u;
	testRequire(
		!xrtHttpStructuredDictionaryWrite(
			Duplicate, 2, arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == 99u),
		"structured writer accepted duplicate Dictionary keys"
	);
	xrtClearError();

	Member.Item.Bare.Data = (xstrview){ arrOutput, 3u };
	iSize = 99u;
	testRequire(
		!xrtHttpStructuredListWrite(
			&Member, 1, arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == 99u),
		"structured writer accepted output overlap"
	);
	xrtClearError();

	Member.Item.Bare.Data = XRT_STR_LITERAL("abc");
	iSize = 99u;
	testRequire(
		!xrtHttpStructuredBareWrite(
			(const xhttpstructuredvalue*)(uintptr_t)(
				UINTPTR_MAX - 1u
			), NULL, 0, &iSize
		) && (iSize == 99u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"structured writer accepted wrapped root descriptor"
	);
	xrtClearError();

	Member.Item.Bare.Data = (xstrview){
		(const char*)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	iSize = 99u;
	testRequire(
		!xrtHttpStructuredListWrite(
			&Member, 1, NULL, 0, &iSize
		) && (iSize == 99u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"structured writer accepted wrapped nested data"
	);
	xrtClearError();

	Member.Item.Bare.Data = XRT_STR_LITERAL("abc");
	testRequire(
		!xrtHttpStructuredListWrite(
			&Member, 1, NULL, 0,
			&Member.Item.ParameterCount
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"structured writer accepted descriptor and size alias"
	);
	xrtClearError();

	memset(&Member, 0, sizeof(Member));
	Member.Kind = XHTTP_STRUCTURED_MEMBER_INNER_LIST;
	Member.Item.Bare.Data.Data = "";
	iSize = 99u;
	testRequire(
		!xrtHttpStructuredListWrite(
			&Member, 1, NULL, 0, &iSize
		) && (iSize == 99u),
		"structured writer accepted unused Inner Item data"
	);
	xrtClearError();

	memset(&Member, 0, sizeof(Member));
	Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Member.Item.Bare.Type = XHTTP_STRUCTURED_STRING;
	Member.Item.Bare.Data = XRT_STR_LITERAL("abc");
	iSize = 99u;
	testRequire(
		!xrtHttpStructuredListWrite(
			&Member, 1,
			(void*)(uintptr_t)(UINTPTR_MAX - 1u), 4u,
			&iSize
		) && (iSize == 99u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"structured writer accepted wrapped output"
	);
	xrtClearError();
}



/* 运行 RFC 9651 规范序列化测试。 */
int main(void)
{
	testStructuredWriteBare();
	testStructuredWriteItem();
	testStructuredWriteList();
	testStructuredWriteDictionary();
	testStructuredWriteEdges();
	printf("[PASS] http_structured_write\n");
	return 0;
}
