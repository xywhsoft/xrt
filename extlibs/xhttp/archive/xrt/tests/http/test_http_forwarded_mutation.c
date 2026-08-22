#include "../test.h"

#include <xrt/http_forwarded.h>



/* 生成可复现的轻量伪随机序列。 */
static uint32 testForwardedRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13;
	iValue ^= iValue >> 17;
	iValue ^= iValue << 5;
	*pState = iValue;
	return iValue;
}



/* 比较参数解码后的语义值。 */
static bool testForwardedParamEqual(
	const xhttpparam* pParam,
	xstrview Value
)
{
	char sDecoded[128];
	size_t iSize;

	return (Value.Size <= sizeof(sDecoded)) &&
		xrtHttpParamValueWrite(
			pParam, sDecoded, sizeof(sDecoded), &iSize
		) && (iSize == Value.Size) &&
		((iSize == 0) ||
		 (memcmp(sDecoded, Value.Data, iSize) == 0));
}



/* 统计标准参数存在位。 */
static size_t testForwardedFlagCount(uint32 iFlags)
{
	size_t iCount = 0;

	while ( iFlags != 0 ) {
		iCount += (size_t)(iFlags & 1u);
		iFlags >>= 1;
	}
	return iCount;
}



/* 验证解析结果完整保留扩展参数的名称和语义值。 */
static bool testForwardedExtensionsEqual(
	const xhttpforwarded* pForwarded,
	const xhttpforwardedvalue* pExpected
)
{
	xhttpparam Pair;
	size_t iOffset = 0;
	size_t iFound = 0;

	for ( ;; ) {
		xhttpnext Next = xrtHttpForwardedPairNext(
			pForwarded->Element, &iOffset, &Pair
		);
		size_t i;

		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			return iFound == pExpected->ExtensionCount;
		}
		for ( i = 0; i < pExpected->ExtensionCount; i++ ) {
			const xhttpforwardedpairvalue* pExtension =
				&pExpected->Extensions[i];

			if ( xrtHttpTokenEqual(
				Pair.Name, pExtension->Name
			) ) {
				if ( !testForwardedParamEqual(
					&Pair, pExtension->Value
				) ) {
					return false;
				}
				iFound++;
				break;
			}
		}
	}
}



/* 验证 writer 生成的字段可以完整还原各元素语义。 */
static void testForwardedRoundtrip(void)
{
	static const xstrview Nodes[] = {
		XRT_STR_INIT("192.0.2.1"),
		XRT_STR_INIT("[2001:db8::1]:443"),
		XRT_STR_INIT("unknown"),
		XRT_STR_INIT("unknown:65535"),
		XRT_STR_INIT("_edge"),
		XRT_STR_INIT("_edge:_private-port")
	};
	static const xstrview Hosts[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("example.com"),
		XRT_STR_INIT("example.com:443"),
		XRT_STR_INIT("[2001:db8::7]:8443"),
		XRT_STR_INIT("[v1.future]:"),
		XRT_STR_INIT(":12345678901234567890"),
		XRT_STR_INIT("node%2Eexample")
	};
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http"),
		XRT_STR_INIT("https"),
		XRT_STR_INIT("web+tls"),
		XRT_STR_INIT("x-custom.1")
	};
	static const xstrview ExtensionNames[] = {
		XRT_STR_INIT("trace"),
		XRT_STR_INIT("route-id")
	};
	static const xstrview ExtensionValues[] = {
		XRT_STR_INIT("alpha"),
		XRT_STR_INIT("edge,west"),
		XRT_STR_INIT("a\"b\\c"),
		XRT_STR_INIT(""),
		XRT_STR_INIT("with space")
	};
	xhttpforwardedvalue Elements[4];
	xhttpforwardedpairvalue Extensions[4][2];
	char sField[1024];
	uint32 iState = UINT32_C(0x46574444);
	size_t iRound;

	for ( iRound = 0; iRound < 6000; iRound++ ) {
		xhttpforwardedcursor Cursor;
		xhttpforwarded Forwarded;
		size_t iCount =
			(testForwardedRandom(&iState) % 4u) + 1u;
		size_t iSize;
		size_t i;

		memset(Elements, 0, sizeof(Elements));
		for ( i = 0; i < iCount; i++ ) {
			uint32 iFlags =
				testForwardedRandom(&iState) & 0x0Fu;
			size_t iExtensions =
				testForwardedRandom(&iState) % 3u;
			size_t j;

			if ( (iFlags == 0) && (iExtensions == 0) ) {
				iFlags = XHTTP_FORWARDED_HAS_FOR;
			}
			Elements[i].Flags = iFlags;
			Elements[i].For = Nodes[
				testForwardedRandom(&iState) %
				(sizeof(Nodes) / sizeof(Nodes[0]))
			];
			Elements[i].By = Nodes[
				testForwardedRandom(&iState) %
				(sizeof(Nodes) / sizeof(Nodes[0]))
			];
			Elements[i].Host = Hosts[
				testForwardedRandom(&iState) %
				(sizeof(Hosts) / sizeof(Hosts[0]))
			];
			Elements[i].Proto = Protocols[
				testForwardedRandom(&iState) %
				(sizeof(Protocols) / sizeof(Protocols[0]))
			];
			Elements[i].Extensions = Extensions[i];
			Elements[i].ExtensionCount = iExtensions;
			for ( j = 0; j < iExtensions; j++ ) {
				Extensions[i][j].Name = ExtensionNames[j];
				Extensions[i][j].Value = ExtensionValues[
					testForwardedRandom(&iState) %
					(sizeof(ExtensionValues) /
					 sizeof(ExtensionValues[0]))
				];
			}
			if ( (iFlags & XHTTP_FORWARDED_HAS_FOR) == 0 ) {
				Elements[i].For = (xstrview){ NULL, 0 };
			}
			if ( (iFlags & XHTTP_FORWARDED_HAS_BY) == 0 ) {
				Elements[i].By = (xstrview){ NULL, 0 };
			}
			if ( (iFlags & XHTTP_FORWARDED_HAS_HOST) == 0 ) {
				Elements[i].Host = (xstrview){ NULL, 0 };
			}
			if ( (iFlags & XHTTP_FORWARDED_HAS_PROTO) == 0 ) {
				Elements[i].Proto = (xstrview){ NULL, 0 };
			}
		}
		testRequire(
			xrtHttpForwardedWrite(
				Elements, iCount, sField,
				sizeof(sField), &iSize
			) && xrtHttpForwardedValid(
				(xstrview){ sField, iSize }
			),
			"Forwarded mutation writer output was invalid"
		);
		xrtHttpForwardedCursorInit(&Cursor);
		for ( i = 0; i < iCount; i++ ) {
			xhttpforwardedvalue* pExpected = &Elements[i];

			testRequire(
				xrtHttpForwardedNext(
					(xstrview){ sField, iSize },
					&Cursor, &Forwarded
				) == XHTTP_NEXT_ITEM,
				"Forwarded mutation element parse failed"
			);
			testRequire(
				(Forwarded.Flags == pExpected->Flags) &&
				(Forwarded.PairCount ==
				 (testForwardedFlagCount(pExpected->Flags) +
				  pExpected->ExtensionCount)) &&
				testForwardedExtensionsEqual(
					&Forwarded, pExpected
				),
				"Forwarded mutation element shape mismatch"
			);
			if ( (pExpected->Flags &
				XHTTP_FORWARDED_HAS_FOR) != 0 ) {
				testRequire(testForwardedParamEqual(
					&Forwarded.For, pExpected->For
				), "Forwarded mutation for mismatch");
			}
			if ( (pExpected->Flags &
				XHTTP_FORWARDED_HAS_BY) != 0 ) {
				testRequire(testForwardedParamEqual(
					&Forwarded.By, pExpected->By
				), "Forwarded mutation by mismatch");
			}
			if ( (pExpected->Flags &
				XHTTP_FORWARDED_HAS_HOST) != 0 ) {
				testRequire(testForwardedParamEqual(
					&Forwarded.Host, pExpected->Host
				), "Forwarded mutation host mismatch");
			}
			if ( (pExpected->Flags &
				XHTTP_FORWARDED_HAS_PROTO) != 0 ) {
				testRequire(testForwardedParamEqual(
					&Forwarded.Proto, pExpected->Proto
				), "Forwarded mutation proto mismatch");
			}
		}
		testRequire(
			xrtHttpForwardedNext(
				(xstrview){ sField, iSize },
				&Cursor, &Forwarded
			) == XHTTP_NEXT_END,
			"Forwarded mutation list did not end"
		);
	}
}



/* 验证任意字节及其截断前缀的解析结果稳定且失败原子。 */
static void testForwardedPrefixes(void)
{
	char sInput[72];
	uint32 iState = UINT32_C(0x7239A11E);
	size_t iCase;

	for ( iCase = 0; iCase < 2500; iCase++ ) {
		size_t iSize =
			testForwardedRandom(&iState) % sizeof(sInput);
		size_t i;

		for ( i = 0; i < iSize; i++ ) {
			sInput[i] = (char)testForwardedRandom(&iState);
		}
		for ( i = 0; i <= iSize; i++ ) {
			xstrview Input = { sInput, i };
			xhttpforwardedcursor Cursor;
			xhttpforwardedcursor SavedCursor;
			xhttpforwarded Forwarded;
			xhttpforwarded SavedForwarded;
			bool bValid = xrtHttpForwardedValid(Input);

			xrtClearError();
			xrtHttpForwardedCursorInit(&Cursor);
			SavedCursor = Cursor;
			memset(&Forwarded, 0xA5, sizeof(Forwarded));
			SavedForwarded = Forwarded;
			if ( !bValid ) {
				testRequire(
					(xrtHttpForwardedNext(
						Input, &Cursor, &Forwarded
					) == XHTTP_NEXT_ERROR) &&
					(memcmp(
						&Cursor, &SavedCursor,
						sizeof(Cursor)
					) == 0) &&
					(memcmp(
						&Forwarded, &SavedForwarded,
						sizeof(Forwarded)
					) == 0),
					"Forwarded mutation failure was not atomic"
				);
				xrtClearError();
				continue;
			}
			for ( ;; ) {
				xhttpnext Next = xrtHttpForwardedNext(
					Input, &Cursor, &Forwarded
				);

				testRequire(
					Next != XHTTP_NEXT_ERROR,
					"Forwarded mutation valid input failed"
				);
				if ( Next == XHTTP_NEXT_END ) {
					break;
				}
			}
		}
	}
}



/* 执行 Forwarded 确定性变异与截断测试。 */
int main(void)
{
	testForwardedRoundtrip();
	testForwardedPrefixes();
	printf("[PASS] http_forwarded_mutation\n");
	return 0;
}
