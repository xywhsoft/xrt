#include "../test.h"

#include "../../src/internal/xacme_dnstxt.h"

#include <stdlib.h>
#include <string.h>

/*
	challtestsrv 门控：XACME_CHALL_DNS = 127.0.0.1:8053 时跑实查
	（DNS TXT），XACME_CHALL_HTTP = http://127.0.0.1:8055 用于铺值。
	未设置时只跑错误路径。
*/

int main(void)
{
	xacmedns Dns;
	const char* sDns = getenv("XACME_CHALL_DNS");

	testRequire(xacmeDnsInit(&Dns, NULL), "acme dns init failed");

	if((sDns != NULL) && (sDns[0] != '\0'))
	{
		char sRecords[4][XACME_TXT_RECORD_MAX];
		size_t iCount = 0u;

		/* 已铺的探针值应可查到。 */
		if(!xacmeDnsTxtQuery(
			&Dns, "127.0.0.1", 8053u, "_acme-challenge.probe.test",
			sRecords, 4u, &iCount))
		{
			const xerror* pE = xrtGetError();
			printf("[diag] query failed kind=%d code=%d msg=%s\n",
				xrtErrorKind(pE), xrtErrorCode(pE),
				xrtErrorMessage(pE) ? xrtErrorMessage(pE) : "?");
			testRequire(false, "acme dns txt query failed");
		}
		testRequire(iCount >= 1u, "acme dns txt query no records");
		if(iCount >= 1u)
		{
			testRequire(
				strcmp(sRecords[0], "probe123") == 0,
				"acme dns txt query value mismatch"
			);
		}

		/* 不存在的主机：零记录成功。 */
		iCount = 99u;
		testRequire(
			xacmeDnsTxtQuery(
				&Dns, "127.0.0.1", 8053u,
				"_acme-challenge.absent.invalid", sRecords, 4u, &iCount),
			"acme dns txt nx query failed"
		);
		testRequire(iCount == 0u, "acme dns txt nx count mismatch");

		/* Wait：期望值已存在应立即命中。 */
		testRequire(
			xacmeDnsTxtWait(
				&Dns, "127.0.0.1", 8053u, "_acme-challenge.probe.test",
				"probe123", 3000u),
			"acme dns txt wait existing failed"
		);
		/* Wait：不存在的值应超时。 */
		testRequire(
			!xacmeDnsTxtWait(
				&Dns, "127.0.0.1", 8053u, "_acme-challenge.probe.test",
				"absent-value", 2500u) &&
				(xrtErrorKind(xrtGetError()) == XERR_TIMEOUT),
			"acme dns txt wait absent mismatch"
		);
	}
	else
	{
		printf("[SKIP] acme dns txt live (XACME_CHALL_DNS unset)\n");
	}

	/* 错误路径。 */
	xrtClearError();
	testRequire(
		!xacmeDnsInit(NULL, NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme dns init null mismatch"
	);
	xrtClearError();
	testRequire(
		!xacmeDnsTxtQuery(
			&Dns, "not-an-ip", 53u, "a.b", NULL, 0u, NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme dns txt bad resolver mismatch"
	);

	xacmeDnsUnit(&Dns);
	printf("[PASS] acme dns txt probe\n");
	return 0;
}
