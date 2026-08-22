#include "../test.h"



/* 生成可重复的轻量伪随机序列。 */
static uint32 testSetCookieRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13;
	iValue ^= iValue >> 17;
	iValue ^= iValue << 5;
	*pState = iValue;
	return iValue;
}



/* 随机构建安全属性组合并通过宽松接收解析回环。 */
int main(void)
{
	static const char Alphabet[] =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
	uint32 iState = UINT32_C(0x5E7C00C1);
	size_t iRound;

	for ( iRound = 0; iRound < 6000; iRound++ ) {
		char Name[25];
		char Value[65];
		xsetcookie Input;
		xsetcookie Output;
		char Text[256];
		size_t iName = (testSetCookieRandom(&iState) % 24u) + 1u;
		size_t iValue = testSetCookieRandom(&iState) % 64u;
		size_t iSize;
		size_t i;

		for ( i = 0; i < iName; i++ ) {
			Name[i] = Alphabet[
				testSetCookieRandom(&iState) % (sizeof(Alphabet) - 1u)
			];
		}
		for ( i = 0; i < iValue; i++ ) {
			Value[i] = Alphabet[
				testSetCookieRandom(&iState) % (sizeof(Alphabet) - 1u)
			];
		}
		memset(&Input, 0, sizeof(Input));
		Input.Name = (xstrview){ Name, iName };
		Input.Value = (xstrview){ Value, iValue };
		if ( (testSetCookieRandom(&iState) & 1u) != 0 ) {
			Input.Flags |= XSET_COOKIE_HAS_PATH;
			Input.Path = XRT_STR_LITERAL("/");
		}
		if ( (testSetCookieRandom(&iState) & 1u) != 0 ) {
			Input.Flags |= XSET_COOKIE_SECURE;
		}
		if ( (testSetCookieRandom(&iState) & 1u) != 0 ) {
			Input.Flags |= XSET_COOKIE_HTTP_ONLY;
		}
		if ( (testSetCookieRandom(&iState) & 1u) != 0 ) {
			Input.Flags |= XSET_COOKIE_HAS_MAX_AGE;
			Input.MaxAge = (int64)(testSetCookieRandom(&iState) % 100000u);
		}
		if ( (testSetCookieRandom(&iState) & 1u) != 0 ) {
			Input.Flags |= XSET_COOKIE_HAS_SAME_SITE;
			Input.SameSite = ((Input.Flags & XSET_COOKIE_SECURE) != 0) ?
				(xcookiesamesite)((testSetCookieRandom(&iState) % 3u) + 1u) :
				(xcookiesamesite)((testSetCookieRandom(&iState) % 2u) + 1u);
		}
		if ( ((Input.Flags & XSET_COOKIE_SECURE) != 0) &&
			((testSetCookieRandom(&iState) & 1u) != 0) ) {
			Input.Flags |= XSET_COOKIE_PARTITIONED;
		}
		testRequire(xrtSetCookieWrite(
			&Input, Text, sizeof(Text), &iSize
		), "set-cookie mutation write failed");
		testRequire(xrtSetCookieParse(
			(xstrview){ Text, iSize }, &Output
		), "set-cookie mutation parse failed");
		testRequire((Output.Name.Size == Input.Name.Size) &&
			(memcmp(Output.Name.Data, Input.Name.Data, Input.Name.Size) == 0) &&
			(Output.Value.Size == Input.Value.Size) &&
			(memcmp(Output.Value.Data, Input.Value.Data,
			 Input.Value.Size) == 0),
			"set-cookie mutation pair mismatch");
		testRequire(((Output.Flags & XSET_COOKIE_HAS_MAX_AGE) ==
			(Input.Flags & XSET_COOKIE_HAS_MAX_AGE)) &&
			(((Output.Flags & XSET_COOKIE_HAS_MAX_AGE) == 0) ||
			 (Output.MaxAge == Input.MaxAge)),
			"set-cookie mutation Max-Age mismatch");
	}
	printf("[PASS] set_cookie_mutation\n");
	return 0;
}
