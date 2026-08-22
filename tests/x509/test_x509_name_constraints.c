#include "../test.h"
#include "../fixtures/x509_name_constraints_vectors.h"
#include "../fixtures/x509_name_vectors.h"
#include "../fixtures/x509_profile_vectors.h"



/* 构造一个只借用调用方内容的 GeneralName。 */
static xx509genname testGeneralName(
	xx509gennametype Type,
	const void* pData,
	size_t iSize
)
{
	xx509genname Name;

	memset(&Name, 0, sizeof(Name));
	Name.Type = Type;
	Name.Value.Data = (const uint8*)pData;
	Name.Value.Size = iSize;
	return Name;
}



/* 验证顶层、子树游标、distance 与证书扩展读取契约。 */
static void testNameConstraintsParse(void)
{
	xx509nameconstraints Constraints;
	xx509nameconstraints Before;
	xx509subtreecursor Cursor;
	xx509subtree Subtree;
	xx509subtree BeforeSubtree;
	xx509cert Certificate;
	xx509result Result;

	testRequire(xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_MIXED,
			sizeof(X509_NAME_CONSTRAINTS_MIXED)
		}, &Constraints
	) && Constraints.HasPermitted && Constraints.HasExcluded,
		"valid mixed NameConstraints parse failed");
	Cursor = Constraints.Permitted;
	testRequire((xrtX509SubtreeRead(&Cursor, &Subtree) == X509_VALUE) &&
		(Subtree.Base.Type == X509_NAME_DNS) &&
		(Subtree.Base.Value.Size == 12u) &&
		!Subtree.HasMinimum && !Subtree.HasMaximum,
		"permitted DNS GeneralSubtree mismatch");
	testRequire((xrtX509SubtreeRead(&Cursor, &Subtree) == X509_VALUE) &&
		(Subtree.Base.Type == X509_NAME_EMAIL) &&
		(xrtX509SubtreeRead(&Cursor, &Subtree) == X509_DONE),
		"permitted email GeneralSubtree traversal mismatch");
	Cursor = Constraints.Excluded;
	testRequire((xrtX509SubtreeRead(&Cursor, &Subtree) == X509_VALUE) &&
		(Subtree.Base.Type == X509_NAME_IP) &&
		(Subtree.Base.Value.Size == 8u),
		"excluded IP GeneralSubtree mismatch");
	memset(&Subtree, 0xA5, sizeof(Subtree));
	BeforeSubtree = Subtree;
	testRequire((xrtX509SubtreeRead(&Cursor, &Subtree) == X509_DONE) &&
		(memcmp(&Subtree, &BeforeSubtree, sizeof(Subtree)) == 0),
		"terminal GeneralSubtree read changed output");

	memset(&Constraints, 0xA5, sizeof(Constraints));
	Before = Constraints;
	testRequire(!xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_EMPTY,
			sizeof(X509_NAME_CONSTRAINTS_EMPTY)
		}, &Constraints
	) && (memcmp(&Constraints, &Before, sizeof(Constraints)) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_NAME_CONSTRAINTS),
		"empty NameConstraints changed failed output");
	testRequire(!xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_EMPTY_PERMITTED,
			sizeof(X509_NAME_CONSTRAINTS_EMPTY_PERMITTED)
		}, &Constraints
	), "empty GeneralSubtrees was accepted");
	testRequire(!xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_DUPLICATE,
			sizeof(X509_NAME_CONSTRAINTS_DUPLICATE)
		}, &Constraints
	), "duplicate permittedSubtrees was accepted");
	testRequire(!xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_BAD_ORDER,
			sizeof(X509_NAME_CONSTRAINTS_BAD_ORDER)
		}, &Constraints
	), "out-of-order NameConstraints fields were accepted");
	testRequire(!xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_MINIMUM_ZERO,
			sizeof(X509_NAME_CONSTRAINTS_MINIMUM_ZERO)
		}, &Constraints
	), "explicit DEFAULT minimum was accepted");

	testRequire(xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Certificate
	), "NameConstraints extension fixture parse failed");
	Certificate.Extensions = (xbytesview) {
		X509_NAME_CONSTRAINTS_EXTENSION,
		sizeof(X509_NAME_CONSTRAINTS_EXTENSION)
	};
	Result = xrtX509NameConstraints(&Certificate, &Constraints);
	testRequire((Result == X509_VALUE) && Constraints.HasPermitted,
		"critical NameConstraints extension read failed");
	Certificate.Extensions = (xbytesview) {
		X509_NAME_CONSTRAINTS_NONCRITICAL_EXTENSION,
		sizeof(X509_NAME_CONSTRAINTS_NONCRITICAL_EXTENSION)
	};
	testRequire((xrtX509NameConstraints(
		&Certificate, &Constraints
	) == X509_ERROR) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_NAME_CONSTRAINTS),
		"non-critical NameConstraints extension was accepted");
}



/* 验证 DNS、email、URI、IP 与 directoryName 的公开匹配规则。 */
static void testNameConstraintsMatching(void)
{
	static const char DnsName[] = "Api.Example.Test";
	static const char DnsBase[] = "example.test";
	static const char DnsOther[] = "example.org";
	static const char Email[] = "Alice@Sub.Example.Test";
	static const char EmailExact[] = "Alice@sub.example.test";
	static const char EmailLocalMismatch[] = "alice@sub.example.test";
	static const char EmailHost[] = "sub.example.test";
	static const char EmailTree[] = ".example.test";
	static const char EmailQuoted[] = "\"a@b\"@example.test";
	static const char EmailQuotedBase[] = "\"a@b\"@EXAMPLE.TEST";
	static const char EmailBadDots[] = "a..b@example.test";
	static const char Uri[] = "https://user@api.example.test:443/v1";
	static const char UriExact[] = "api.example.test";
	static const char UriTree[] = ".example.test";
	static const char BadUri[] = "https://a@b@example.test/";
	static const uint8 Ip[] = { 192, 0, 2, 42 };
	static const uint8 IpBase[] = {
		192, 0, 2, 0, 255, 255, 255, 0
	};
	xx509genname Name;
	xx509genname Base;

	Name = testGeneralName(X509_NAME_DNS, DnsName, sizeof(DnsName) - 1u);
	Base = testGeneralName(X509_NAME_DNS, DnsBase, sizeof(DnsBase) - 1u);
	testRequire(xrtX509GeneralNameWithin(&Name, &Base) == X509_VALUE,
		"DNS subtree and case-insensitive match failed");
	Base.Value = (xbytesview) {
		(const uint8*)DnsOther, sizeof(DnsOther) - 1u
	};
	testRequire(xrtX509GeneralNameWithin(&Name, &Base) == X509_DONE,
		"unrelated DNS subtree matched");

	Name = testGeneralName(X509_NAME_EMAIL, Email, sizeof(Email) - 1u);
	Base = testGeneralName(
		X509_NAME_EMAIL, EmailExact, sizeof(EmailExact) - 1u
	);
	testRequire(xrtX509GeneralNameWithin(&Name, &Base) == X509_VALUE,
		"exact mailbox constraint failed");
	Base.Value = (xbytesview) {
		(const uint8*)EmailLocalMismatch, sizeof(EmailLocalMismatch) - 1u
	};
	testRequire(xrtX509GeneralNameWithin(&Name, &Base) == X509_DONE,
		"mailbox local-part was compared case-insensitively");
	Base.Value = (xbytesview) {
		(const uint8*)EmailHost, sizeof(EmailHost) - 1u
	};
	testRequire(xrtX509GeneralNameWithin(&Name, &Base) == X509_VALUE,
		"exact email host constraint failed");
	Base.Value = (xbytesview) {
		(const uint8*)EmailTree, sizeof(EmailTree) - 1u
	};
	testRequire(xrtX509GeneralNameWithin(&Name, &Base) == X509_VALUE,
		"email subdomain constraint failed");
	Name.Value = (xbytesview) {
		(const uint8*)EmailQuoted, sizeof(EmailQuoted) - 1u
	};
	Base.Value = (xbytesview) {
		(const uint8*)EmailQuotedBase, sizeof(EmailQuotedBase) - 1u
	};
	testRequire(xrtX509GeneralNameWithin(&Name, &Base) == X509_VALUE,
		"quoted SMTP local-part containing at-sign failed");
	Name.Value = (xbytesview) {
		(const uint8*)EmailBadDots, sizeof(EmailBadDots) - 1u
	};
	testRequire(xrtX509GeneralNameWithin(&Name, &Base) == X509_ERROR,
		"invalid SMTP Dot-string was accepted");

	Name = testGeneralName(X509_NAME_URI, Uri, sizeof(Uri) - 1u);
	Base = testGeneralName(X509_NAME_URI, UriExact, sizeof(UriExact) - 1u);
	testRequire(xrtX509GeneralNameWithin(&Name, &Base) == X509_VALUE,
		"URI DNS authority exact match failed");
	Base.Value = (xbytesview) {
		(const uint8*)UriTree, sizeof(UriTree) - 1u
	};
	testRequire(xrtX509GeneralNameWithin(&Name, &Base) == X509_VALUE,
		"URI DNS authority subtree match failed");
	Name.Value = (xbytesview) {
		(const uint8*)BadUri, sizeof(BadUri) - 1u
	};
	testRequire((xrtX509GeneralNameWithin(&Name, &Base) == X509_ERROR) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_NAME_CONSTRAINTS),
		"URI with repeated raw userinfo delimiter was accepted");

	Name = testGeneralName(X509_NAME_IP, Ip, sizeof(Ip));
	Base = testGeneralName(X509_NAME_IP, IpBase, sizeof(IpBase));
	testRequire(xrtX509GeneralNameWithin(&Name, &Base) == X509_VALUE,
		"IPv4 address-and-mask constraint failed");
	Name.Value = (xbytesview) {
		(const uint8*)"\xC0\x00\x03\x01", 4u
	};
	testRequire(xrtX509GeneralNameWithin(&Name, &Base) == X509_DONE,
		"IPv4 address outside mask matched");

	Name = testGeneralName(
		X509_NAME_DIRECTORY, X509_NAME_CHILD, sizeof(X509_NAME_CHILD)
	);
	Base = testGeneralName(
		X509_NAME_DIRECTORY, X509_NAME_BASE, sizeof(X509_NAME_BASE)
	);
	testRequire(xrtX509GeneralNameWithin(&Name, &Base) == X509_VALUE,
		"directoryName subtree match failed");
	Name.Type = X509_NAME_REGISTERED_ID;
	Base.Type = X509_NAME_REGISTERED_ID;
	testRequire((xrtX509GeneralNameWithin(&Name, &Base) == X509_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"unsupported GeneralName constraint was not rejected explicitly");
}



/* 验证 Subject、SAN、历史 emailAddress 和非默认 distance 的证书语义。 */
static void testNameConstraintsCertificate(void)
{
	xx509cert Certificate;
	xx509nameconstraints Constraints;

	testRequire(xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Certificate
	) && xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_DNS,
			sizeof(X509_NAME_CONSTRAINTS_DNS)
		}, &Constraints
	) && xrtX509NameConstraintsCheck(&Constraints, &Certificate),
		"permitted DNS certificate check failed");
	testRequire(xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_LOOPBACK,
			sizeof(X509_NAME_CONSTRAINTS_LOOPBACK)
		}, &Constraints
	) && !xrtX509NameConstraintsCheck(&Constraints, &Certificate) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_NAME_CONSTRAINTS),
		"excluded SAN IP address was accepted");

	Certificate.Subject = (xbytesview) {
		X509_NAME_EMAIL_UPPER, sizeof(X509_NAME_EMAIL_UPPER)
	};
	Certificate.Extensions = (xbytesview) { NULL, 0 };
	testRequire(xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_EMAIL,
			sizeof(X509_NAME_CONSTRAINTS_EMAIL)
		}, &Constraints
	) && xrtX509NameConstraintsCheck(&Constraints, &Certificate),
		"legacy Subject emailAddress constraint failed");
	Certificate.Extensions = (xbytesview) {
		X509_NAME_CONSTRAINTS_SAN_ALLOWED,
		sizeof(X509_NAME_CONSTRAINTS_SAN_ALLOWED)
	};
	testRequire(xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_EXCLUDE_EMAIL,
			sizeof(X509_NAME_CONSTRAINTS_EXCLUDE_EMAIL)
		}, &Constraints
	) && xrtX509NameConstraintsCheck(&Constraints, &Certificate),
		"Subject emailAddress was constrained despite SAN presence");

	Certificate.Subject = (xbytesview) {
		X509_NAME_CHILD, sizeof(X509_NAME_CHILD)
	};
	Certificate.Extensions = (xbytesview) {
		X509_NAME_CONSTRAINTS_SAN_ALLOWED,
		sizeof(X509_NAME_CONSTRAINTS_SAN_ALLOWED)
	};
	testRequire(xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_MINIMUM_ONE,
			sizeof(X509_NAME_CONSTRAINTS_MINIMUM_ONE)
		}, &Constraints
	) && !xrtX509NameConstraintsCheck(&Constraints, &Certificate) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"non-default GeneralSubtree distance was silently ignored");

	memset(&Constraints, 0, sizeof(Constraints));
	Constraints.HasPermitted = true;
	testRequire(!xrtX509NameConstraintsCheck(&Constraints, &Certificate) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_NAME_CONSTRAINTS),
		"forged empty GeneralSubtrees cursor was accepted");
}



/* 执行 NameConstraints 解析、匹配和证书语义测试。 */
int main(void)
{
	testNameConstraintsParse();
	testNameConstraintsMatching();
	testNameConstraintsCertificate();
	printf("[PASS] x509_name_constraints\n");
	return 0;
}
