#include "../test.h"
#include "../fixtures/x509_profile_vectors.h"



/* 构造只含一个 SAN 项的受控 Extensions DER。 */
static void testX509IdentityExtensions(
	xx509cert* pCert,
	const uint8* pExtensions,
	size_t iSize
)
{
	pCert->Extensions.Data = pExtensions;
	pCert->Extensions.Size = iSize;
}



/* 验证 RFC 9525 DNS 大小写、根点、通配符和严格标签规则。 */
static void testX509IdentityDns(void)
{
	static const char LongLabel[] =
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		".example";
	static const char EmbeddedZero[] = { 'a', 0, 'b', '.', 't', 'e', 's', 't' };
	static const char NonAscii[] = { (char)0xC3, (char)0xA9, '.', 't', 'e', 's', 't' };
	static const xstrview InvalidPatterns[] = {
		XRT_STR_INIT("foo*.example.test"),
		XRT_STR_INIT("*foo.example.test"),
		XRT_STR_INIT("foo.*.example.test"),
		XRT_STR_INIT("*.*.example.test"),
		XRT_STR_INIT("**.example.test"),
		XRT_STR_INIT("-foo.example.test"),
		XRT_STR_INIT("foo-.example.test"),
		XRT_STR_INIT("foo..example.test"),
		XRT_STR_INIT(".example.test"),
		XRT_STR_INIT("example.test.."),
		XRT_STR_INIT("*"),
		{ LongLabel, sizeof(LongLabel) - 1u },
		{ EmbeddedZero, sizeof(EmbeddedZero) },
		{ NonAscii, sizeof(NonAscii) }
	};

	testRequire(xrtX509MatchDns(
		XRT_STR_LITERAL("api.example.test"),
		XRT_STR_LITERAL("API.Example.Test")
	) == X509_VALUE, "case-insensitive DNS identity match failed");
	testRequire(xrtX509MatchDns(
		XRT_STR_LITERAL("api.example.test."),
		XRT_STR_LITERAL("api.example.test")
	) == X509_VALUE, "root-dot DNS identity match failed");
	testRequire(xrtX509MatchDns(
		XRT_STR_LITERAL("api.example.test"),
		XRT_STR_LITERAL("other.example.test")
	) == X509_DONE, "different DNS identity matched");

	testRequire(xrtX509MatchDns(
		XRT_STR_LITERAL("*.example.test"),
		XRT_STR_LITERAL("api.example.test")
	) == X509_VALUE, "one-label wildcard identity match failed");
	testRequire(xrtX509MatchDns(
		XRT_STR_LITERAL("*.example.test"),
		XRT_STR_LITERAL("a.b.example.test")
	) == X509_DONE, "wildcard matched more than one label");
	testRequire(xrtX509MatchDns(
		XRT_STR_LITERAL("*.example.test"),
		XRT_STR_LITERAL("example.test")
	) == X509_DONE, "wildcard matched an empty label");
	testRequire(xrtX509MatchDns(
		XRT_STR_LITERAL("*.com"), XRT_STR_LITERAL("example.com")
	) == X509_VALUE, "RFC wildcard was incorrectly coupled to a suffix list");

	for ( size_t i = 0; i < (sizeof(InvalidPatterns) /
		sizeof(InvalidPatterns[0])); i++ ) {
		testRequire((xrtX509MatchDns(
			InvalidPatterns[i], XRT_STR_LITERAL("foo.example.test")
		) == X509_ERROR) &&
			(xrtErrorCode(xrtGetError()) == X509_ERROR_DNS_NAME),
			"invalid DNS identity pattern was accepted");
	}
	testRequire(xrtX509MatchDns(
		XRT_STR_LITERAL("api.example.test"),
		XRT_STR_LITERAL("*.example.test")
	) == X509_ERROR, "wildcard reference identity was accepted");
}



/* 验证证书 SAN 中的 DNS-ID、IPv4/IPv6 IP-ID 和类型隔离。 */
static void testX509IdentityHost(void)
{
	static const uint8 OnlyDns[] = {
		0x30, 0x1D, 0x30, 0x1B, 0x06, 0x03, 0x55, 0x1D, 0x11,
		0x04, 0x14, 0x30, 0x12, 0x82, 0x10,
		'a', 'p', 'i', '.', 'e', 'x', 'a', 'm',
		'p', 'l', 'e', '.', 't', 'e', 's', 't'
	};
	static const uint8 WildcardDns[] = {
		0x30, 0x1D, 0x30, 0x1B, 0x06, 0x03, 0x55, 0x1D, 0x11,
		0x04, 0x14, 0x30, 0x12, 0x82, 0x10,
		'*', '.', 'x', '.', 'e', 'x', 'a', 'm',
		'p', 'l', 'e', '.', 't', 'e', 's', 't'
	};
	static const uint8 InvalidDns[] = {
		0x30, 0x1D, 0x30, 0x1B, 0x06, 0x03, 0x55, 0x1D, 0x11,
		0x04, 0x14, 0x30, 0x12, 0x82, 0x10,
		'b', 'a', 'd', '*', 'e', 'x', 'a', 'm',
		'p', 'l', 'e', '.', 't', 'e', 's', 't'
	};
	static const uint8 OnlyIPv4[] = {
		0x30, 0x11, 0x30, 0x0F, 0x06, 0x03, 0x55, 0x1D, 0x11,
		0x04, 0x08, 0x30, 0x06, 0x87, 0x04, 127, 0, 0, 1
	};
	static const uint8 OnlyIPv6[] = {
		0x30, 0x1D, 0x30, 0x1B, 0x06, 0x03, 0x55, 0x1D, 0x11,
		0x04, 0x14, 0x30, 0x12, 0x87, 0x10,
		0x20, 0x01, 0x0D, 0xB8, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 1
	};
	xx509cert Cert;
	xx509genname Name;
	xx509genname BeforeName;

	testRequire(xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Cert
	), "X.509 identity fixture parse failed");
	testRequire((xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("API.Example.Test."), &Name
	) == X509_VALUE) && (Name.Type == X509_NAME_DNS) &&
		(Name.Value.Size == 16u), "certificate DNS-ID match failed");
	testRequire((xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("127.0.0.1"), &Name
	) == X509_VALUE) && (Name.Type == X509_NAME_IP) &&
		(Name.Value.Size == 4u), "certificate IPv4 IP-ID match failed");
	testRequire(xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("api.example.test"), NULL
	) == X509_VALUE, "certificate identity match with no output failed");

	memset(&Name, 0xA5, sizeof(Name));
	BeforeName = Name;
	testRequire((xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("other.example.test"), &Name
	) == X509_DONE) && (memcmp(
		&Name, &BeforeName, sizeof(Name)
	) == 0), "identity mismatch changed output");

	testX509IdentityExtensions(&Cert, WildcardDns, sizeof(WildcardDns));
	testRequire(xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("api.x.example.test"), &Name
	) == X509_VALUE, "certificate wildcard DNS-ID match failed");
	testRequire(xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("a.b.x.example.test"), &Name
	) == X509_DONE, "certificate wildcard matched multiple labels");

	testX509IdentityExtensions(&Cert, InvalidDns, sizeof(InvalidDns));
	memset(&Name, 0xA5, sizeof(Name));
	BeforeName = Name;
	testRequire((xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("badxexample.test"), &Name
	) == X509_DONE) && (memcmp(
		&Name, &BeforeName, sizeof(Name)
	) == 0), "invalid presented DNS-ID was not ignored");

	testX509IdentityExtensions(&Cert, OnlyIPv6, sizeof(OnlyIPv6));
	testRequire((xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("2001:db8::1"), &Name
	) == X509_VALUE) && (Name.Type == X509_NAME_IP),
		"certificate IPv6 IP-ID match failed");
	testRequire(xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("[2001:db8::1]"), &Name
	) == X509_VALUE, "bracketed IPv6 identity match failed");
	testRequire(xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("2001:db8::2"), &Name
	) == X509_DONE, "different IPv6 identity matched");

	testX509IdentityExtensions(&Cert, OnlyDns, sizeof(OnlyDns));
	testRequire(xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("127.0.0.1"), &Name
	) == X509_DONE, "IP reference matched a DNS-ID");
	testX509IdentityExtensions(&Cert, OnlyIPv4, sizeof(OnlyIPv4));
	testRequire(xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("api.example.test"), &Name
	) == X509_DONE, "DNS reference matched an IP-ID");

	memset(&Cert.Extensions, 0, sizeof(Cert.Extensions));
	memset(&Name, 0xA5, sizeof(Name));
	BeforeName = Name;
	testRequire((xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("api.example.test"), &Name
	) == X509_DONE) && (memcmp(
		&Name, &BeforeName, sizeof(Name)
	) == 0), "X.509 identity fell back to Common Name");
}



/* 验证无效引用身份和畸形 SAN 的结构化错误与失败原子性。 */
static void testX509IdentityErrors(void)
{
	static const xstrview InvalidHosts[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("[example.test]"),
		XRT_STR_INIT("[2001:db8::1"),
		XRT_STR_INIT("2001:db8::1%1"),
		XRT_STR_INIT("127.0.0.01"),
		XRT_STR_INIT("256.0.0.1"),
		XRT_STR_INIT("foo..example.test")
	};
	xx509cert Cert;
	xx509genname Name;
	xx509genname BeforeName;

	testRequire(xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Cert
	), "X.509 identity error fixture parse failed");
	for ( size_t i = 0; i < (sizeof(InvalidHosts) /
		sizeof(InvalidHosts[0])); i++ ) {
		memset(&Name, 0xA5, sizeof(Name));
		BeforeName = Name;
		testRequire((xrtX509MatchHost(
			&Cert, InvalidHosts[i], &Name
		) == X509_ERROR) && (memcmp(
			&Name, &BeforeName, sizeof(Name)
		) == 0) &&
			(xrtErrorCode(xrtGetError()) == X509_ERROR_IDENTITY),
			"invalid reference identity was accepted or changed output");
	}

	testRequire(xrtX509Parse(
		X509_PROFILE_BAD_SAN, sizeof(X509_PROFILE_BAD_SAN), &Cert
	), "malformed SAN identity fixture failed base parse");
	memset(&Name, 0xA5, sizeof(Name));
	BeforeName = Name;
	testRequire((xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("api.example.test"), &Name
	) == X509_ERROR) && (memcmp(
		&Name, &BeforeName, sizeof(Name)
	) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_IDENTITY) &&
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(xrtErrorCode(xrtErrorCause(xrtGetError())) ==
		 X509_ERROR_GENERAL_NAME),
		"malformed SAN did not preserve identity error cause");
}



/* 执行 X.509 服务身份匹配、隔离和错误边界测试。 */
int main(void)
{
	testX509IdentityDns();
	testX509IdentityHost();
	testX509IdentityErrors();
	printf("[PASS] x509_identity\n");
	return 0;
}
