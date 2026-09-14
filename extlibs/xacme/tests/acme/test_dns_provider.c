#include "../test.h"

#include <string.h>

static bool testDnsStubAdd(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	(void)pProvider;
	(void)sFqdn;
	(void)sTxt;
	return true;
}

static bool testDnsStubRemove(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	(void)pProvider;
	(void)sFqdn;
	(void)sTxt;
	return true;
}

/* provider 最小契约与线程错误语义。 */
int main(void)
{
	xacmednsprovider Provider;

	xrtClearError();
	testRequire(
		!xrtAcmeDnsProviderValidate(NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
			(xrtErrorCode(xrtGetError()) ==
			 XACME_DNS_ERROR_ARGUMENT) &&
			(strcmp(xrtErrorDomain(xrtGetError()), "xrt.acme.dns") == 0),
		"acme dns provider validate null mismatch"
	);

	memset(&Provider, 0, sizeof(Provider));
	xrtClearError();
	testRequire(
		!xrtAcmeDnsProviderValidate(&Provider) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme dns provider validate empty mismatch"
	);

	Provider.sId = "custom";
	Provider.Add = testDnsStubAdd;
	xrtClearError();
	testRequire(
		!xrtAcmeDnsProviderValidate(&Provider) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme dns provider validate missing remove mismatch"
	);

	Provider.Remove = testDnsStubRemove;
	testRequire(
		xrtAcmeDnsProviderValidate(&Provider),
		"acme dns provider validate full mismatch"
	);

	printf("[PASS] acme dns provider contract\n");
	return 0;
}
