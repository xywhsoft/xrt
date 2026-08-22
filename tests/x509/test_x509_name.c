#include "../test.h"
#include "../fixtures/x509_name_vectors.h"
#include "../fixtures/x509_name_oracle.h"



/* 把固定 DER 数组转换为借用视图。 */
#define TEST_X509_NAME_VIEW(Name) \
	((xbytesview) { (Name), sizeof(Name) })



/* 验证 RFC 4518 大小写折叠、空白处理和 NFKC。 */
static void testX509NameStringPrep(void)
{
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_PRINTABLE_SPACE),
		TEST_X509_NAME_VIEW(X509_NAME_UTF8_SPACE)
	) == X509_VALUE, "X.509 Name insignificant-space comparison failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_UTF8_CAFE),
		TEST_X509_NAME_VIEW(X509_NAME_UTF8_CAFE_DECOMPOSED)
	) == X509_VALUE, "X.509 Name canonical composition comparison failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_UTF8_SHARP_S),
		TEST_X509_NAME_VIEW(X509_NAME_PRINTABLE_STRASSE)
	) == X509_VALUE, "X.509 Name multi-scalar case fold failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_UTF8_FULLWIDTH),
		TEST_X509_NAME_VIEW(X509_NAME_PRINTABLE_FOO)
	) == X509_VALUE, "X.509 Name compatibility normalization failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_UTF8_SOFT_HYPHEN),
		TEST_X509_NAME_VIEW(X509_NAME_PRINTABLE_FOO)
	) == X509_VALUE, "X.509 Name mapped-to-nothing character failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_UTF8_NBSP),
		TEST_X509_NAME_VIEW(X509_NAME_PRINTABLE_A_SPACE_B)
	) == X509_VALUE, "X.509 Name separator mapping failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_UTF8_CONTROL),
		TEST_X509_NAME_VIEW(X509_NAME_PRINTABLE_AB)
	) == X509_VALUE, "X.509 Name control removal failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_UTF8_TAB),
		TEST_X509_NAME_VIEW(X509_NAME_PRINTABLE_A_SPACE_B)
	) == X509_VALUE, "X.509 Name whitespace control mapping failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_UTF8_SPACE_MARK),
		TEST_X509_NAME_VIEW(X509_NAME_UTF8_MARK)
	) == X509_DONE,
		"X.509 Name discarded SPACE followed by a class-zero mark");
}



/* 验证 RFC 5280 要求和允许的 DirectoryString 编码。 */
static void testX509NameEncodings(void)
{
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_BMP_ANGSTROM),
		TEST_X509_NAME_VIEW(X509_NAME_UTF8_ANGSTROM_DECOMPOSED)
	) == X509_VALUE, "X.509 Name BMPString comparison failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_UNIVERSAL_ANGSTROM),
		TEST_X509_NAME_VIEW(X509_NAME_UTF8_ANGSTROM_DECOMPOSED)
	) == X509_VALUE, "X.509 Name UniversalString comparison failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_TELETEX_ASCII),
		TEST_X509_NAME_VIEW(X509_NAME_PRINTABLE_ALICE)
	) == X509_VALUE, "X.509 Name ASCII TeletexString comparison failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_TELETEX_NON_ASCII),
		TEST_X509_NAME_VIEW(X509_NAME_PRINTABLE_ALICE)
	) == X509_ERROR &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"implementation-defined TeletexString was silently accepted");
}



/* 验证已知 IA5 属性、RDN 集合和名称顺序。 */
static void testX509NameStructure(void)
{
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_DC_UPPER),
		TEST_X509_NAME_VIEW(X509_NAME_DC_LOWER)
	) == X509_VALUE, "X.509 domainComponent caseIgnoreIA5Match failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_EMAIL_UPPER),
		TEST_X509_NAME_VIEW(X509_NAME_EMAIL_LOWER)
	) == X509_VALUE, "X.509 emailAddress caseIgnoreIA5Match failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_DC_MAX),
		TEST_X509_NAME_VIEW(X509_NAME_DC_MAX)
	) == X509_VALUE, "maximum X.509 domainComponent label was rejected");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_EMAIL_MAX),
		TEST_X509_NAME_VIEW(X509_NAME_EMAIL_MAX)
	) == X509_VALUE, "maximum X.509 emailAddress was rejected");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_MULTI_LEFT),
		TEST_X509_NAME_VIEW(X509_NAME_MULTI_RIGHT)
	) == X509_VALUE, "X.509 multi-valued RDN set comparison failed");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_CHILD),
		TEST_X509_NAME_VIEW(X509_NAME_REVERSED)
	) == X509_DONE, "X.509 Name RDN order was ignored");
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_EMPTY),
		TEST_X509_NAME_VIEW(X509_NAME_EMPTY)
	) == X509_VALUE, "empty X.509 Names did not compare equal");
}



/* 验证 Distinguished Name 子树按前导 RDN 序列匹配。 */
static void testX509NameSubtree(void)
{
	testRequire(xrtX509NameWithin(
		TEST_X509_NAME_VIEW(X509_NAME_CHILD),
		TEST_X509_NAME_VIEW(X509_NAME_BASE)
	) == X509_VALUE, "X.509 Name subtree prefix comparison failed");
	testRequire(xrtX509NameWithin(
		TEST_X509_NAME_VIEW(X509_NAME_BASE),
		TEST_X509_NAME_VIEW(X509_NAME_CHILD)
	) == X509_DONE, "short X.509 Name was accepted within a longer base");
	testRequire(xrtX509NameWithin(
		TEST_X509_NAME_VIEW(X509_NAME_REVERSED),
		TEST_X509_NAME_VIEW(X509_NAME_BASE)
	) == X509_DONE, "X.509 Name subtree ignored RDN order");
}



/* 验证畸形编码和 RFC 4518 禁止字符返回协议错误。 */
static void testX509NameErrors(void)
{
	static const uint8* const ppInvalid[] = {
		X509_NAME_UTF8_PRIVATE,
		X509_NAME_UTF8_UNASSIGNED,
		X509_NAME_BAD_UTF8,
		X509_NAME_BAD_BMP,
		X509_NAME_BAD_UNIVERSAL,
		X509_NAME_BAD_PRINTABLE,
		X509_NAME_DIRECTORY_EMPTY,
		X509_NAME_DC_EMPTY,
		X509_NAME_DC_DOT,
		X509_NAME_DC_HYPHEN_FIRST,
		X509_NAME_DC_HYPHEN_LAST,
		X509_NAME_DC_UNDERSCORE,
		X509_NAME_DC_TOO_LONG,
		X509_NAME_EMAIL_EMPTY,
		X509_NAME_EMAIL_TOO_LONG
	};
	static const size_t InvalidSizes[] = {
		sizeof(X509_NAME_UTF8_PRIVATE),
		sizeof(X509_NAME_UTF8_UNASSIGNED),
		sizeof(X509_NAME_BAD_UTF8),
		sizeof(X509_NAME_BAD_BMP),
		sizeof(X509_NAME_BAD_UNIVERSAL),
		sizeof(X509_NAME_BAD_PRINTABLE),
		sizeof(X509_NAME_DIRECTORY_EMPTY),
		sizeof(X509_NAME_DC_EMPTY),
		sizeof(X509_NAME_DC_DOT),
		sizeof(X509_NAME_DC_HYPHEN_FIRST),
		sizeof(X509_NAME_DC_HYPHEN_LAST),
		sizeof(X509_NAME_DC_UNDERSCORE),
		sizeof(X509_NAME_DC_TOO_LONG),
		sizeof(X509_NAME_EMAIL_EMPTY),
		sizeof(X509_NAME_EMAIL_TOO_LONG)
	};

	for ( size_t i = 0; i < (sizeof(ppInvalid) / sizeof(ppInvalid[0])); i++ ) {
		xbytesview Invalid = { ppInvalid[i], InvalidSizes[i] };

		testRequire(xrtX509NameEqual(
			Invalid, TEST_X509_NAME_VIEW(X509_NAME_PRINTABLE_ALICE)
		) == X509_ERROR &&
			(xrtErrorCode(xrtGetError()) == X509_ERROR_NAME),
			"invalid X.509 Name value was accepted");
	}
	testRequire(xrtX509NameEqual(
		TEST_X509_NAME_VIEW(X509_NAME_BAD_UTF8),
		TEST_X509_NAME_VIEW(X509_NAME_DC_LOWER)
	) == X509_ERROR &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_NAME),
		"mismatched attribute OID hid an invalid X.509 Name value");
	testRequire(xrtX509NameEqual(
		(xbytesview) { NULL, 1u },
		TEST_X509_NAME_VIEW(X509_NAME_PRINTABLE_ALICE)
	) == X509_ERROR && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid X.509 Name view was accepted");
}



/* 对照 Unicode 3.2 oracle 验证广覆盖 case-fold 和 NFKC 向量。 */
static void testX509NameOracle(void)
{
	for ( size_t i = 0; i < (sizeof(X509_NAME_ORACLE_VECTORS) /
		sizeof(X509_NAME_ORACLE_VECTORS[0])); i++ ) {
		const xrt_test_x509_name_oracle* pVector =
			&X509_NAME_ORACLE_VECTORS[i];
		xbytesview Left = {
			X509_NAME_ORACLE_DATA + pVector->LeftOffset,
			pVector->LeftSize
		};
		xbytesview Right = {
			X509_NAME_ORACLE_DATA + pVector->RightOffset,
			pVector->RightSize
		};

		testRequire(xrtX509NameEqual(Left, Right) == X509_VALUE,
			"X.509 Name differed from Unicode 3.2 oracle");
	}
}



/* 执行 X.509 Name 规范匹配测试。 */
int main(void)
{
	testX509NameStringPrep();
	testX509NameEncodings();
	testX509NameStructure();
	testX509NameSubtree();
	testX509NameErrors();
	testX509NameOracle();
	printf("[PASS] x509_name\n");
	return 0;
}
