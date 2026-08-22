#include "../test.h"



/* 验证按 Unicode 标量反转、二进制零标量和非法输入。 */
int main(void)
{
	static const char arrBinary[] = {
		'A', 0, (char)0xE4, (char)0xBD, (char)0xA0
	};
	static const char arrBinaryExpected[] = {
		(char)0xE4, (char)0xBD, (char)0xA0, 0, 'A'
	};
	static const char arrInvalid[] = {
		(char)0xF0, (char)0x80, (char)0x80, (char)0x80
	};
	static const char arrFilterInput[] = {
		'A', 0, (char)0xE4, (char)0xBD, (char)0xA0, 'B'
	};
	static const char arrFilterSet[] = {
		0, (char)0xE4, (char)0xBD, (char)0xA0
	};
	char arrBefore[64];
	char arrOutput[64];
	char arrText[64];
	xstrview Range;
	size_t iSize = 99;
	str sText;

	testRequire(xrtUtf8Range(XRT_STR_LITERAL("A你😀B"), -2, 1, &Range) &&
		(Range.Size == sizeof("😀") - 1u) &&
		(memcmp(Range.Data, "😀", Range.Size) == 0),
		"negative Unicode range mismatch");
	testRequire(xrtUtf8Range(XRT_STR_LITERAL("A你😀B"), INT64_MIN, -1,
		&Range) && (Range.Size == sizeof("A你😀B") - 1u),
		"minimum signed Unicode range mismatch");
	testRequire(xrtUtf8Range(XRT_STR_LITERAL("A你😀B"), 99, 1, &Range) &&
		(Range.Size == 0), "clamped Unicode range mismatch");
	sText = xrtUtf8Substr(XRT_STR_LITERAL("A你😀B"), 1, 2);
	testRequire((sText != NULL) && (strcmp(sText, "你😀") == 0),
		"allocated Unicode substring mismatch");
	xrtFree(sText);

	testRequire(xrtUtf8Find(XRT_STR_LITERAL("甲A乙a甲"),
		XRT_STR_LITERAL("甲"), 1) == 4,
		"Unicode forward search index mismatch");
	testRequire(xrtUtf8CaseFind(XRT_STR_LITERAL("甲A乙a甲"),
		XRT_STR_LITERAL("A"), 2) == 3,
		"Unicode case search index mismatch");
	testRequire(xrtUtf8RFind(XRT_STR_LITERAL("甲A乙a甲"),
		XRT_STR_LITERAL("甲")) == 4,
		"Unicode reverse search index mismatch");
	testRequire(xrtUtf8CaseRFind(XRT_STR_LITERAL("甲A乙a甲"),
		XRT_STR_LITERAL("A")) == 3,
		"Unicode reverse case search index mismatch");
	testRequire(xrtUtf8Find(XRT_STR_LITERAL("甲"), XRT_STR_LITERAL(""), 1) == 1,
		"empty Unicode search mismatch");
	testRequire(xrtUtf8ContainsAny(XRT_STR_LITERAL("A你B"),
		XRT_STR_LITERAL("好你")), "Unicode contains-any mismatch");
	testRequire(!xrtUtf8ContainsAny(XRT_STR_LITERAL("A你B"),
		XRT_STR_LITERAL("好")), "Unicode contains-any false positive");

	testRequire(xrtUtf8TrimLeftSet(XRT_STR_LITERAL("你你A好你"),
		XRT_STR_LITERAL("你好"), &Range) &&
		(Range.Size == sizeof("A好你") - 1u) &&
		(memcmp(Range.Data, "A好你", Range.Size) == 0),
		"Unicode left trim mismatch");
	testRequire(xrtUtf8TrimRightSet(XRT_STR_LITERAL("你A好你"),
		XRT_STR_LITERAL("你好"), &Range) &&
		(Range.Size == sizeof("你A") - 1u) &&
		(memcmp(Range.Data, "你A", Range.Size) == 0),
		"Unicode right trim mismatch");
	testRequire(xrtUtf8TrimSet(XRT_STR_LITERAL("你你A好你"),
		XRT_STR_LITERAL("你好"), &Range) &&
		(Range.Size == 1) && (Range.Data[0] == 'A'),
		"Unicode two-sided trim mismatch");
	testRequire(xrtUtf8TrimRightSet(XRT_STR_LITERAL("你好"),
		XRT_STR_LITERAL("你好"), &Range) && (Range.Size == 0),
		"all-member Unicode right trim mismatch");

	sText = xrtUtf8Insert(XRT_STR_LITERAL("A你B"), -1,
		XRT_STR_LITERAL("😀"));
	testRequire((sText != NULL) && (strcmp(sText, "A你😀B") == 0),
		"Unicode insert mismatch");
	xrtFree(sText);
	sText = xrtUtf8Remove(XRT_STR_LITERAL("A你😀B"), -3, 2);
	testRequire((sText != NULL) && (strcmp(sText, "AB") == 0),
		"Unicode remove mismatch");
	xrtFree(sText);

	sText = xrtUtf8PadLeft(XRT_STR_LITERAL("你"), 5,
		XRT_STR_LITERAL("好😀"));
	testRequire((sText != NULL) && (strcmp(sText, "好😀好😀你") == 0),
		"Unicode left pad mismatch");
	xrtFree(sText);
	sText = xrtUtf8PadRight(XRT_STR_LITERAL("你"), 4,
		XRT_STR_LITERAL("好😀"));
	testRequire((sText != NULL) && (strcmp(sText, "你好😀好") == 0),
		"Unicode partial right pad mismatch");
	xrtFree(sText);
	sText = xrtUtf8PadCenter(XRT_STR_LITERAL("你"), 5,
		XRT_STR_LITERAL("好😀"));
	testRequire((sText != NULL) && (strcmp(sText, "好😀你好😀") == 0),
		"Unicode center pad mismatch");
	xrtFree(sText);
	sText = xrtUtf8PadLeft(XRT_STR_LITERAL("你"), 3,
		(xstrview){ NULL, 0 });
	testRequire((sText != NULL) && (strcmp(sText, "  你") == 0),
		"Unicode default pad mismatch");
	xrtFree(sText);

	sText = xrtUtf8Reverse(XRT_STR_LITERAL("A你😀Z"));
	testRequire((sText != NULL) && (strcmp(sText, "Z😀你A") == 0),
		"Unicode scalar reverse mismatch");
	xrtFree(sText);

	sText = xrtUtf8Reverse((xstrview){ arrBinary, sizeof(arrBinary) });
	testRequire((sText != NULL) &&
		(memcmp(sText, arrBinaryExpected, sizeof(arrBinaryExpected)) == 0) &&
		(sText[sizeof(arrBinaryExpected)] == 0), "embedded-null scalar reverse mismatch");
	xrtFree(sText);

	testRequire(xrtUtf8ReverseTo(XRT_STR_LITERAL("A你😀Z"), arrOutput,
		sizeof(arrOutput)) && (strcmp(arrOutput, "Z😀你A") == 0),
		"Unicode scalar reverse target mismatch");
	memcpy(arrText, "A你😀Z", sizeof("A你😀Z"));
	testRequire(xrtUtf8ReverseTo((xstrview){ arrText, sizeof("A你😀Z") - 1u },
		arrText, sizeof(arrText)) && (strcmp(arrText, "Z😀你A") == 0),
		"Unicode scalar reverse in-place mismatch");

	sText = xrtUtf8Reverse((xstrview){ NULL, 0 });
	testRequire((sText != NULL) && (sText[0] == 0), "empty reverse ownership mismatch");
	xrtFree(sText);

	testRequire(xrtUtf8FilterTo(
		(xstrview){ arrFilterInput, sizeof(arrFilterInput) },
		(xstrview){ arrFilterSet, sizeof(arrFilterSet) },
		NULL, 0, &iSize) && (iSize == 2),
		"Unicode scalar filter query mismatch");
	testRequire(xrtUtf8FilterTo(
		(xstrview){ arrFilterInput, sizeof(arrFilterInput) },
		(xstrview){ arrFilterSet, sizeof(arrFilterSet) },
		arrOutput, sizeof(arrOutput), &iSize) && (iSize == 2) &&
		(memcmp(arrOutput, "AB", 3) == 0),
		"Unicode scalar binary filter mismatch");
	memcpy(arrText, "A你B好你C", sizeof("A你B好你C"));
	testRequire(xrtUtf8FilterTo(
		(xstrview){ arrText, sizeof("A你B好你C") - 1u },
		XRT_STR_LITERAL("你C"), arrText, sizeof(arrText), &iSize) &&
		(strcmp(arrText, "AB好") == 0),
		"Unicode scalar filter in-place mismatch");
	sText = xrtUtf8Filter(XRT_STR_LITERAL("A你B好你C"),
		XRT_STR_LITERAL("你C"));
	testRequire((sText != NULL) && (strcmp(sText, "AB好") == 0),
		"allocated Unicode scalar filter mismatch");
	xrtFree(sText);

	memset(arrOutput, 0xA5, sizeof(arrOutput));
	memcpy(arrBefore, arrOutput, sizeof(arrOutput));
	iSize = 99;
	testRequire(!xrtUtf8FilterTo(XRT_STR_LITERAL("A你B"),
		XRT_STR_LITERAL("你"), arrOutput, 2, &iSize) &&
		(iSize == 2) && (memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"short Unicode filter target changed output");
	xrtClearError();

	xrtClearError();
	testRequire(xrtUtf8Reverse((xstrview){ arrInvalid, sizeof(arrInvalid) }) == NULL,
		"invalid UTF-8 reverse must fail");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtErrorCode(xrtGetError()) == XUTF_ERROR_INVALID),
		"invalid UTF-8 reverse error mismatch");
	xrtClearError();
	testRequire(xrtUtf8Filter(XRT_STR_LITERAL("text"),
		(xstrview){ arrInvalid, sizeof(arrInvalid) }) == NULL,
		"invalid UTF-8 filter set must fail");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtErrorCode(xrtGetError()) == XUTF_ERROR_INVALID),
		"invalid UTF-8 filter set error mismatch");
	xrtClearError();
	testRequire(!xrtUtf8Range(XRT_STR_LITERAL("text"), 0, 1, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null Unicode range output must fail");
	xrtClearError();
	testRequire(xrtUtf8Find(XRT_STR_LITERAL("text"),
		(xstrview){ arrInvalid, sizeof(arrInvalid) }, 0) == XRT_NPOS &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"invalid Unicode search part must fail");
	xrtClearError();
	testRequire(xrtUtf8PadLeft(XRT_STR_LITERAL("text"), 8,
		(xstrview){ arrInvalid, sizeof(arrInvalid) }) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"invalid Unicode pad text must fail");
	xrtClearError();
	printf("[PASS] unicode-text\n");
	return 0;
}
