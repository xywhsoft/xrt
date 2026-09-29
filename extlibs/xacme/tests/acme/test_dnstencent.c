#include "../test.h"

#include "../../src/internal/xacme_dnstxt.h"
#include "../../src/internal/xacme_dns_tencent_internal.h"
#include <xrt/acme_dns_tencent.h>

#include <stdlib.h>
#include <string.h>

/*
	腾讯云 DNSPod 真实联测（环境门控）：
	  XACME_TENCENT_ID / XACME_TENCENT_KEY —— SecretId/SecretKey
	  XACME_TENCENT_FQDN —— TXT 属主（默认 _acme-challenge.test.xxrpa.com）
	流程：构造 → Add（TC3 签名 + zone 试探）→ 公共 resolver 确认 → Remove。
*/

int main(void)
{
	const char* sId = getenv("XACME_TENCENT_ID");
	const char* sKey = getenv("XACME_TENCENT_KEY");
	const char* sFqdnEnv = getenv("XACME_TENCENT_FQDN");
	cstr sFqdn = ((sFqdnEnv != NULL) && (sFqdnEnv[0] != '\0')) ?
		sFqdnEnv : "_acme-challenge.test.xxrpa.com";
	const char* sValue = "xacme-live-probe-20260915";
	xacmednstencentconfig Config;
	xacmednsprovider Provider;

	testRequire(xacmeDnsTencentResponseSuccess(XRT_STR_LITERAL(
		"{\"Response\":{\"RequestId\":\"probe\"}}")),
		"acme dns_tencent successful response mismatch");
	testRequire(!xacmeDnsTencentResponseSuccess(XRT_STR_LITERAL(
		"{\"Response\":{\"Error\":{\"Code\":"
		"\"InvalidParameter.RecordIdInvalid\"},\"RequestId\":\"probe\"}}")),
		"acme dns_tencent must reject API error inside HTTP success");
	testRequire(!xacmeDnsTencentResponseSuccess(XRT_STR_LITERAL("{")),
		"acme dns_tencent must reject malformed response");

	/* 参数错误语义（常跑）。 */
	{
		xacmednstencentconfig Bad;
		xacmednsprovider BadProvider;
		xrtAcmeDnsTencentConfigInit(&Bad);
		Bad.sSecretId = "id-only";
		xrtClearError();
		testRequire(
			!xrtAcmeDnsTencent(&Bad, NULL, &BadProvider) &&
				(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"acme dns_tencent missing key mismatch"
		);
	}
	{
		char sTooLong[256];
		xacmednstencentconfig Bad;
		xacmednsprovider BadProvider;
		memset(sTooLong, 'x', sizeof(sTooLong) - 1u);
		sTooLong[sizeof(sTooLong) - 1u] = '\0';
		xrtAcmeDnsTencentConfigInit(&Bad);
		Bad.sSecretId = sTooLong;
		Bad.sSecretKey = "key";
		xrtClearError();
		testRequire(!xrtAcmeDnsTencent(&Bad, NULL, &BadProvider) &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"acme dns_tencent must reject truncated secret id");
		Bad.sSecretId = "id";
		Bad.sSecretKey = sTooLong;
		xrtClearError();
		testRequire(!xrtAcmeDnsTencent(&Bad, NULL, &BadProvider) &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"acme dns_tencent must reject truncated secret key");
		Bad.sSecretKey = "key";
		Bad.sEndpoint = sTooLong;
		xrtClearError();
		testRequire(!xrtAcmeDnsTencent(&Bad, NULL, &BadProvider) &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"acme dns_tencent must reject truncated endpoint");
	}

	if((sId == NULL) || (sId[0] == '\0') || (sKey == NULL) ||
		(sKey[0] == '\0'))
	{
		printf("[SKIP] acme dns_tencent live (XACME_TENCENT_ID unset)\n");
		return 0;
	}

	xrtAcmeDnsTencentConfigInit(&Config);
	Config.sSecretId = sId;
	Config.sSecretKey = sKey;
	testRequire(
		xrtAcmeDnsTencent(&Config, NULL, &Provider),
		"acme dns_tencent construct failed"
	);

	if(!Provider.Add(&Provider, (xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sValue, strlen(sValue) }))
	{
		xrtAcmeDnsTencentProviderUnit(&Provider);
		testRequire(false, "acme dns_tencent add failed");
	}

	{
		xacmedns Probe;
		testRequire(
			xacmeDnsInit(&Probe, NULL),
			"acme dns_tencent probe init failed"
		);
		testRequire(
			xacmeDnsTxtWait(&Probe, "119.29.29.29", 53u, sFqdn, sValue,
				60000u),
			"acme dns_tencent txt not visible"
		);
		xacmeDnsUnit(&Probe);
	}
	printf("[tencent] TXT added and visible via resolver\n");

	testRequire(
		Provider.Remove(&Provider,
			(xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sValue, strlen(sValue) }),
		"acme dns_tencent remove failed"
	);

	xrtAcmeDnsTencentProviderUnit(&Provider);
	printf("[PASS] acme dns_tencent live\n");
	return 0;
}
