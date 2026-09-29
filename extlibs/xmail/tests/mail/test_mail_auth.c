#include "../../src/internal/xrt_mail_auth.h"
#include "../test.h"


static void testAuthEncoding(
	char* sEncoded,
	size_t iEncoded,
	cstr sExpected
)
{
	testRequire(sEncoded != NULL, "mail auth encoding failed");
	testRequire(iEncoded == strlen(sExpected) &&
		strcmp(sEncoded, sExpected) == 0,
		"mail auth encoded payload mismatch");
	__xrtMailAuthFree(sEncoded, iEncoded + 1u);
}


int main(void)
{
	size_t iEncoded = 0u;
	char* sEncoded;

	testRequire(__xrtMailAuthFieldValid(testMailView("user"), true),
		"mail auth rejects valid field");
	testRequire(__xrtMailAuthFieldValid(
		testMailViewN("a\1b", 3u), false),
		"mail auth must allow SOH in mechanisms without delimiter");
	testRequire(!__xrtMailAuthFieldValid(
		testMailViewN("a\1b", 3u), true),
		"mail auth must reject SOH delimiter");
	testRequire(!__xrtMailAuthFieldValid(
		testMailViewN("a\0b", 3u), false) &&
		!__xrtMailAuthFieldValid(testMailView("a\rb"), false) &&
		!__xrtMailAuthFieldValid(testMailView("a\nb"), false) &&
		!__xrtMailAuthFieldValid(testMailViewN(NULL, 1u), false),
		"mail auth must reject binary or line-breaking fields");

	sEncoded = __xrtMailAuthPlain(
		testMailView("auth"), testMailView("user"),
		testMailView("p,a=s"), &iEncoded);
	testAuthEncoding(sEncoded, iEncoded, "YXV0aAB1c2VyAHAsYT1z");

	sEncoded = __xrtMailAuthXoauth2(
		testMailView("user"), testMailView("abc"), &iEncoded);
	testAuthEncoding(sEncoded, iEncoded,
		"dXNlcj11c2VyAWF1dGg9QmVhcmVyIGFiYwEB");

	sEncoded = __xrtMailAuthOauthBearer(
		testMailView("a,b=c"), testMailView("abc"), &iEncoded);
	testAuthEncoding(sEncoded, iEncoded,
		"bixhPWE9MkNiPTNEYywBYXV0aD1CZWFyZXIgYWJjAQE=");

	xrtClearError();
	testRequire(__xrtMailAuthEncode("x", 1u, NULL) == NULL &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"mail auth encoder must reject missing size output");

	printf("[PASS] mail auth payloads\n");
	return 0;
}
