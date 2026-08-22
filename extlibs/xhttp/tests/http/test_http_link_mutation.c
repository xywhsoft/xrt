#include "../test.h"

#include <xrt/http_link.h>



/* 生成可复现的轻量伪随机序列。 */
static uint32 testLinkRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13;
	iValue ^= iValue >> 17;
	iValue ^= iValue << 5;
	*pState = iValue;
	return iValue;
}



/* 按字节比较借用视图。 */
static bool testLinkMutationViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 返回关系列表中的第一项。 */
static xstrview testLinkFirstRelation(xstrview Relations)
{
	cstr sSpace = (cstr)memchr(
		Relations.Data, ' ', Relations.Size
	);

	return (xstrview){
		Relations.Data,
		(sSpace == NULL) ? Relations.Size :
		(size_t)(sSpace - Relations.Data)
	};
}



/* 验证随机生产的合法 Link 列表可以逐元素还原。 */
static void testLinkRoundtrip(void)
{
	static const xstrview Targets[] = {
		XRT_STR_INIT("/next"),
		XRT_STR_INIT("../previous?q=1#part"),
		XRT_STR_INIT("https://example.test/a,b"),
		XRT_STR_INIT("//cdn.example.test/item")
	};
	static const xstrview Relations[] = {
		XRT_STR_INIT("next"),
		XRT_STR_INIT("alternate stylesheet"),
		XRT_STR_INIT("https://example.test/relation"),
		XRT_STR_INIT("prev last")
	};
	xhttplinkvalue Links[4];
	xhttplinkparamvalue Params[4][5];
	char sField[2048];
	uint32 iState = UINT32_C(0x8288A11E);
	size_t iRound;

	for ( iRound = 0; iRound < 5000; iRound++ ) {
		xhttplinkcursor Cursor;
		xhttplink Parsed;
		size_t iCount = (testLinkRandom(&iState) % 4u) + 1u;
		size_t iSize;
		size_t i;

		memset(Links, 0, sizeof(Links));
		memset(Params, 0, sizeof(Params));
		for ( i = 0; i < iCount; i++ ) {
			uint32 iMask = testLinkRandom(&iState) & 0x1Fu;
			size_t iParam = 0;

			Links[i].Target = Targets[
				testLinkRandom(&iState) %
				(sizeof(Targets) / sizeof(Targets[0]))
			];
			Links[i].Relations = Relations[
				testLinkRandom(&iState) %
				(sizeof(Relations) / sizeof(Relations[0]))
			];
			Links[i].Parameters = Params[i];
			if ( (iMask & 0x01u) != 0 ) {
				Params[i][iParam++] = (xhttplinkparamvalue){
					XRT_STR_LITERAL("anchor"),
					XRT_STR_LITERAL("#section"),
					XHTTP_PARAM_HAS_VALUE
				};
			}
			if ( (iMask & 0x02u) != 0 ) {
				Params[i][iParam++] = (xhttplinkparamvalue){
					XRT_STR_LITERAL("hreflang"),
					XRT_STR_LITERAL("en-US"),
					XHTTP_PARAM_HAS_VALUE
				};
			}
			if ( (iMask & 0x04u) != 0 ) {
				Params[i][iParam++] = (xhttplinkparamvalue){
					XRT_STR_LITERAL("title"),
					XRT_STR_LITERAL("a \\\"title\\\""),
					XHTTP_PARAM_HAS_VALUE |
					XHTTP_PARAM_QUOTED
				};
			}
			if ( (iMask & 0x08u) != 0 ) {
				Params[i][iParam++] = (xhttplinkparamvalue){
					XRT_STR_LITERAL("title*"),
					XRT_STR_LITERAL(
						"UTF-8'en'encoded%20title"
					),
					XHTTP_PARAM_HAS_VALUE
				};
			}
			if ( (iMask & 0x10u) != 0 ) {
				Params[i][iParam++] = (xhttplinkparamvalue){
					XRT_STR_LITERAL("type"),
					XRT_STR_LITERAL("application/json"),
					XHTTP_PARAM_HAS_VALUE |
					XHTTP_PARAM_QUOTED
				};
			}
			Links[i].ParameterCount = iParam;
		}
		testRequire(
			xrtHttpLinkWrite(
				Links, iCount, sField,
				sizeof(sField), &iSize
			) && xrtHttpLinkValid(
				(xstrview){ sField, iSize }
			),
			"Link mutation writer output was invalid"
		);
		xrtHttpLinkCursorInit(&Cursor);
		for ( i = 0; i < iCount; i++ ) {
			testRequire(
				(xrtHttpLinkNext(
					(xstrview){ sField, iSize },
					&Cursor, &Parsed
				) == XHTTP_NEXT_ITEM) &&
				testLinkMutationViewEqual(
					Parsed.Target, Links[i].Target
				) && (Parsed.ParamCount ==
				 (Links[i].ParameterCount + 1u)) &&
				(xrtHttpLinkRelationFind(
					&Parsed,
					testLinkFirstRelation(
						Links[i].Relations
					)
				) == XHTTP_NEXT_ITEM),
				"Link mutation roundtrip mismatch"
			);
		}
		testRequire(
			xrtHttpLinkNext(
				(xstrview){ sField, iSize },
				&Cursor, &Parsed
			) == XHTTP_NEXT_END,
			"Link mutation list did not end"
		);
	}
}



/* 对照 URL parser 验证 quoted-pair 不会改变 URI-reference 接受集合。 */
static void testLinkEscapedUriParity(void)
{
	static const char Alphabet[] =
		"abcXYZ012:/?#[]@.%+-_~";
	static const char Prefix[] =
		"</x>; rel=next; anchor=\"";
	char sUri[32];
	char sField[96];
	uint32 iState = UINT32_C(0x55524921);
	size_t iCase;

	for ( iCase = 0; iCase < 5000; iCase++ ) {
		xurl Url;
		size_t iUri = testLinkRandom(&iState) % sizeof(sUri);
		size_t iEscape = (iUri == 0) ? 0 :
			(testLinkRandom(&iState) % iUri);
		size_t iField = sizeof(Prefix) - 1u;
		size_t i;
		bool bUrl;
		bool bLink;

		for ( i = 0; i < iUri; i++ ) {
			sUri[i] = Alphabet[
				testLinkRandom(&iState) %
				(sizeof(Alphabet) - 1u)
			];
		}
		memcpy(sField, Prefix, iField);
		for ( i = 0; i < iUri; i++ ) {
			if ( i == iEscape ) {
				sField[iField++] = '\\';
			}
			sField[iField++] = sUri[i];
		}
		sField[iField++] = '"';
		bUrl = xrtUrlParse((xstrview){ sUri, iUri }, &Url);
		xrtClearError();
		bLink = xrtHttpLinkValid(
			(xstrview){ sField, iField }
		);
		testRequire(
			bLink == bUrl,
			"Link escaped URI validation diverged from URL parser"
		);
		xrtClearError();
	}
}



/* 验证任意字节及其截断前缀的解析稳定性和失败原子性。 */
static void testLinkPrefixes(void)
{
	char sInput[80];
	uint32 iState = UINT32_C(0x4C494E4B);
	size_t iCase;

	for ( iCase = 0; iCase < 2500; iCase++ ) {
		size_t iSize =
			testLinkRandom(&iState) % sizeof(sInput);
		size_t i;

		for ( i = 0; i < iSize; i++ ) {
			sInput[i] = (char)testLinkRandom(&iState);
		}
		for ( i = 0; i <= iSize; i++ ) {
			xstrview Input = { sInput, i };
			xhttplinkcursor Cursor;
			xhttplinkcursor SavedCursor;
			xhttplink Link;
			xhttplink SavedLink;
			bool bValid = xrtHttpLinkValid(Input);

			xrtClearError();
			xrtHttpLinkCursorInit(&Cursor);
			SavedCursor = Cursor;
			memset(&Link, 0xA5, sizeof(Link));
			SavedLink = Link;
			if ( !bValid ) {
				testRequire(
					(xrtHttpLinkNext(
						Input, &Cursor, &Link
					) == XHTTP_NEXT_ERROR) &&
					(memcmp(
						&Cursor, &SavedCursor,
						sizeof(Cursor)
					) == 0) &&
					(memcmp(
						&Link, &SavedLink,
						sizeof(Link)
					) == 0),
					"Link mutation failure was not atomic"
				);
				xrtClearError();
				continue;
			}
			for ( ;; ) {
				xhttpnext Next = xrtHttpLinkNext(
					Input, &Cursor, &Link
				);

				testRequire(
					Next != XHTTP_NEXT_ERROR,
					"Link mutation valid input failed"
				);
				if ( Next == XHTTP_NEXT_END ) {
					break;
				}
			}
		}
	}
}



/* 执行 Link 确定性变异和截断测试。 */
int main(void)
{
	testLinkRoundtrip();
	testLinkEscapedUriParity();
	testLinkPrefixes();
	printf("[PASS] http_link_mutation\n");
	return 0;
}
