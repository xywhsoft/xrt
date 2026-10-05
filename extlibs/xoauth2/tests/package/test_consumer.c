#define XOAUTH2_MODULE_XOAUTH2
#include "xoauth2.h"
#include <string.h>

int test_oauth2_package(void)
{
	xoauth2client client = {0};
	size_t pending = SIZE_MAX;
	char* url;
	xoauth2UseGithub(&client, "package-client", "package-secret", "https://app.example/callback");
	url = xoauth2BeginLogin(&client);
	if ( url == NULL ) { xoauth2ClientUnit(&client); return 1; }
	if ( (strstr(url, "client_id=package-client") == NULL) || (strstr(url, "state=") == NULL) ) {
		xrtFree(url); xoauth2ClientUnit(&client); return 2;
	}
	xrtFree(url);
	xoauth2ClientUnit(&client);
	if ( !xoauth2HttpXrtCleanupPending(0u, &pending) || (pending != 0u) ) return 3;
	return 0;
}

#if !defined(AUTH_PACKAGE_COMPOSED)
int main(void) { return test_oauth2_package(); }
#endif
