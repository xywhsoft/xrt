/*
 * XRT Example - String to Number
 * XRT 范例 - 字符串转数字
 *
 * Description / 说明:
 *   EN: Demonstrates token parsing with xrtParseNum/xrtParseNumSkipSpace
 *       and convenience conversion with xrtStrTo*.
 *   CN: 演示 xrtParseNum/xrtParseNumSkipSpace 的 token 解析，以及
 *       xrtStrTo* 的便捷转换边界。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/jnum_str_to_num.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/jnum_str_to_num              (Linux, GCC)
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include <string.h>


static const char* GetJNumTypeName(jnum_type_t eType)
{
	switch ( eType ) {
	case JNUM_NULL:		return "JNUM_NULL";
	case JNUM_BOOL:		return "JNUM_BOOL";
	case JNUM_INT:		return "JNUM_INT";
	case JNUM_HEX:		return "JNUM_HEX";
	case JNUM_LINT:		return "JNUM_LINT";
	case JNUM_LHEX:		return "JNUM_LHEX";
	case JNUM_DOUBLE:	return "JNUM_DOUBLE";
	default:			return "UNKNOWN";
	}
}


static void PrintParsedValue(jnum_type_t eType, const jnum_value_t* pValue)
{
	switch ( eType ) {
	case JNUM_BOOL:
		printf("%s", pValue->vbool ? "true" : "false");
		break;
	case JNUM_INT:
		printf("%d", pValue->vint);
		break;
	case JNUM_HEX:
		printf("0x%X", pValue->vhex);
		break;
	case JNUM_LINT:
		printf("%lld", (long long)pValue->vlint);
		break;
	case JNUM_LHEX:
		printf("0x%llX", (unsigned long long)pValue->vlhex);
		break;
	case JNUM_DOUBLE:
		printf("%.17g", pValue->vdbl);
		break;
	default:
		printf("(null)");
		break;
	}
}


static void DemoParseNum(void)
{
	const char* arrTokens[] = {
		"42",
		"-42",
		"0xFF",
		"3.14",
		"1.23e5",
		"123abc",
		"0x",
		"18446744073709551615",
	};
	int i;

	printf("=== xrtParseNum (token parsing) ===\n");
	printf("=== xrtParseNum（token 解析） ===\n");

	for ( i = 0; i < (int)(sizeof(arrTokens) / sizeof(arrTokens[0])); i++ ) {
		jnum_type_t eType = JNUM_NULL;
		jnum_value_t sValue = {0};
		int iLen = xrtParseNum(arrTokens[i], &eType, &sValue);
		int bFull = (iLen == (int)strlen(arrTokens[i]));

		printf("  token = \"%s\"\n", arrTokens[i]);
		printf("    len  = %d\n", iLen);
		printf("    type = %s\n", GetJNumTypeName(eType));
		printf("    full = %s\n", bFull ? "yes" : "no");
		printf("    value = ");
		PrintParsedValue(eType, &sValue);
		printf("\n\n");
	}
}


static void DemoParseNumSkipSpace(void)
{
	const char* sToken = "   -17";
	jnum_type_t eType = JNUM_NULL;
	jnum_value_t sValue = {0};
	int iLen = xrtParseNumSkipSpace(sToken, &eType, &sValue);

	printf("=== xrtParseNumSkipSpace ===\n");
	printf("=== xrtParseNumSkipSpace ===\n");
	printf("  token = \"%s\"\n", sToken);
	printf("    len  = %d\n", iLen);
	printf("    type = %s\n", GetJNumTypeName(eType));
	printf("    value = ");
	PrintParsedValue(eType, &sValue);
	printf("\n\n");
}


static void DemoWrappers(void)
{
	const char* arrInputs[] = {
		"8080",
		"0xFF",
		"123abc",
		"abc",
		"  3.5",
	};
	int i;

	printf("=== xrtStrTo* wrappers ===\n");
	printf("=== xrtStrTo* 便捷转换 ===\n");

	for ( i = 0; i < (int)(sizeof(arrInputs) / sizeof(arrInputs[0])); i++ ) {
		printf("  input = \"%s\"\n", arrInputs[i]);
		printf("    I32 = %d\n", xrtStrToI32(arrInputs[i]));
		printf("    U64 = %llu\n", (unsigned long long)xrtStrToU64(arrInputs[i]));
		printf("    Num = %.17g\n", xrtStrToNum(arrInputs[i]));
		printf("\n");
	}
}


static void DemoConfigLine(void)
{
	char arrConfig[][32] = {
		"port=8080",
		"timeout=30000",
		"ratio=0.85",
		"mask=0xFF",
	};
	int i;

	printf("=== Config-like Input ===\n");
	printf("=== 类配置输入 ===\n");

	for ( i = 0; i < (int)(sizeof(arrConfig) / sizeof(arrConfig[0])); i++ ) {
		char* pEq = strchr(arrConfig[i], '=');

		if ( pEq == NULL ) {
			continue;
		}

		*pEq = '\0';

		{
			const char* sKey = arrConfig[i];
			const char* sToken = pEq + 1;
			jnum_type_t eType = JNUM_NULL;
			jnum_value_t sValue = {0};
			int iLen = xrtParseNum(sToken, &eType, &sValue);
			int bFull = (iLen == (int)strlen(sToken));

			printf("  %s = %s\n", sKey, sToken);
			printf("    type = %s\n", GetJNumTypeName(eType));
			printf("    full = %s\n", bFull ? "yes" : "no");
			printf("    value = ");
			PrintParsedValue(eType, &sValue);
			printf("\n\n");
		}
	}
}


int main(void)
{
	xrtInit();

	printf("XRT JNUM - String to Number Demo\n");
	printf("XRT JNUM - 字符串转数字演示\n");
	printf("================================\n\n");

	DemoParseNum();
	DemoParseNumSkipSpace();
	DemoWrappers();
	DemoConfigLine();

	xrtUnit();
	return 0;
}
