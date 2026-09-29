#include "../test.h"

#include "../../src/internal/xacme_dnstxt.h"
#include "../../src/internal/xacme_dns_aws_internal.h"
#include <xrt/acme_dns_aws.h>

#include <stdlib.h>
#include <string.h>

/*
	AWS Route53 真实联测（环境门控）：
	  XACME_AWS_KEY / XACME_AWS_SECRET —— AccessKeyId/SecretAccessKey
	  XACME_AWS_REGION —— 可选（默认 us-east-1）
	  XACME_AWS_FQDN —— TXT 属主（默认 _acme-challenge.test.xxrpa.com）
   流程：构造 → Add（SigV4 + 记录集合并）→
   公共 resolver 确认 → Remove（精确移除）。
*/

static size_t countText(cstr sText, cstr sNeedle)
{
	size_t iCount = 0u;
	size_t iStep = strlen(sNeedle);
	while((sText = strstr(sText, sNeedle)) != NULL)
	{
		iCount++;
		sText += iStep;
	}
	return iCount;
}

static void testOfflineZones(void)
{
	static const char* sMixed =
		"<ListHostedZonesByNameResponse xmlns=\"https://route53.amazonaws.com/"
		"doc/2013-04-01/\"><HostedZones>"
		"<HostedZone><Id>/hostedzone/ZPRIVATE</Id>"
		"<Name>example.com.</Name><Config><PrivateZone>true</PrivateZone>"
		"</Config></HostedZone>"
		"<HostedZone><Id>/hostedzone/ZPUBLIC</Id>"
		"<Name>example.com.</Name><Config><PrivateZone>false</PrivateZone>"
		"</Config></HostedZone></HostedZones>"
		"<IsTruncated>false</IsTruncated>"
		"</ListHostedZonesByNameResponse>";
	static const char* sEmpty =
		"<ListHostedZonesByNameResponse><HostedZones/>"
		"<IsTruncated>false</IsTruncated>"
		"</ListHostedZonesByNameResponse>";
	static const char* sAmbiguous =
		"<ListHostedZonesByNameResponse><HostedZones>"
		"<HostedZone><Id>ZPUBLIC1</Id><Name>example.com.</Name>"
		"<Config><PrivateZone>false</PrivateZone></Config></HostedZone>"
		"<HostedZone><Id>ZPUBLIC2</Id><Name>example.com.</Name>"
		"<Config><PrivateZone>false</PrivateZone></Config></HostedZone>"
		"</HostedZones><IsTruncated>false</IsTruncated>"
		"</ListHostedZonesByNameResponse>";
	static const char* sTruncated =
		"<ListHostedZonesByNameResponse><HostedZones>"
		"<HostedZone><Id>ZPUBLIC</Id><Name>example.com.</Name>"
		"<Config><PrivateZone>false</PrivateZone></Config></HostedZone>"
		"</HostedZones><IsTruncated>true</IsTruncated>"
		"<NextDNSName>example.com</NextDNSName>"
		"</ListHostedZonesByNameResponse>";
	static const char* sPublicWithoutConfig =
		"<ListHostedZonesByNameResponse><HostedZones>"
		"<HostedZone><Id>ZPLAIN</Id><Name>example.com.</Name>"
		"</HostedZone></HostedZones><IsTruncated>false</IsTruncated>"
		"</ListHostedZonesByNameResponse>";
	char sId[64];
	testRequire(xacmeAwsSelectPublicZone(sMixed, "example.com",
		sId, sizeof(sId)) == 1 && (strcmp(sId, "ZPUBLIC") == 0),
		"aws zone discovery must skip a same-name private zone");
	testRequire(xacmeAwsSelectPublicZone(sEmpty, "example.com",
		sId, sizeof(sId)) == 0,
		"aws empty zone listing must allow parent search");
	testRequire(xacmeAwsSelectPublicZone(sPublicWithoutConfig,
		"example.com", sId, sizeof(sId)) == 1 &&
		(strcmp(sId, "ZPLAIN") == 0),
		"aws omitted zone config denotes a public zone");
	testRequire(xacmeAwsSelectPublicZone(sAmbiguous, "example.com",
		sId, sizeof(sId)) < 0,
		"aws ambiguous public zone must fail closed");
	testRequire(xacmeAwsSelectPublicZone(sTruncated, "example.com",
		sId, sizeof(sId)) < 0,
		"aws truncated same-name zone page must fail closed");
}

static void testOfflineRecordSet(void)
{
	static const char* sExisting =
		"<?xml version=\"1.0\"?><ListResourceRecordSetsResponse>"
		"<ResourceRecordSets><ResourceRecordSet>"
		"<Name>_acme-challenge.example.com.</Name><Type>TXT</Type>"
		"<TTL>300</TTL><ResourceRecords>"
		"<ResourceRecord><Value>\"old-token\"</Value></ResourceRecord>"
		"<ResourceRecord><Value>\"SPF &amp; mail\"</Value></ResourceRecord>"
		"</ResourceRecords></ResourceRecordSet></ResourceRecordSets>"
		"<IsTruncated>false</IsTruncated></ListResourceRecordSetsResponse>";
	static const char* sEmpty =
		"<ListResourceRecordSetsResponse><ResourceRecordSets/>"
		"</ListResourceRecordSetsResponse>";
	static const char* sNoMatch =
		"<ListResourceRecordSetsResponse><ResourceRecordSets>"
		"<ResourceRecordSet><Name>later.example.com.</Name>"
		"<Type>TXT</Type></ResourceRecordSet>"
		"</ResourceRecordSets></ListResourceRecordSetsResponse>";
	static const char* sOne =
		"<ListResourceRecordSetsResponse><ResourceRecordSets>"
		"<ResourceRecordSet><Name>_acme-challenge.example.com.</Name>"
		"<Type>TXT</Type><TTL>42</TTL><ResourceRecords>"
		"<ResourceRecord><Value>&quot;old-token&quot;</Value></ResourceRecord>"
		"</ResourceRecords></ResourceRecordSet></ResourceRecordSets>"
		"</ListResourceRecordSetsResponse>";
	xacmeawstxtset Set;
	xbuffer Request;
	bool bChanged = false;
	testRequire(xacmeAwsParseTxtSet(sExisting,
		"_acme-challenge.example.com", &Set) && Set.bPresent &&
		(Set.iCount == 2u) && (strcmp(Set.sTtl, "300") == 0),
		"aws TXT list parse failed");
	xrtBufferInit(&Request);
	testRequire(xacmeAwsBuildTxtChange(&Set, "_acme-challenge.example.com",
		"new-token", true, &Request, &bChanged) && bChanged,
		"aws TXT add batch failed");
	testRequire(countText((cstr)Request.Data, "<Action>DELETE</Action>") == 1u &&
		countText((cstr)Request.Data, "<Action>CREATE</Action>") == 1u &&
		countText((cstr)Request.Data, "<TTL>300</TTL>") == 2u &&
		countText((cstr)Request.Data, "<Value>\"old-token\"</Value>") == 2u &&
		countText((cstr)Request.Data, "<Value>\"SPF &amp; mail\"</Value>") == 2u &&
		countText((cstr)Request.Data, "<Value>\"new-token\"</Value>") == 1u,
		"aws add must preserve existing TXT values and TTL");
	xrtBufferUnit(&Request);
	testRequire(xacmeAwsBuildTxtChange(&Set, "_acme-challenge.example.com",
		"old-token", false, &Request, &bChanged) && bChanged,
		"aws TXT remove batch failed");
	testRequire(countText((cstr)Request.Data, "<Value>\"old-token\"</Value>") == 1u &&
		countText((cstr)Request.Data, "<Value>\"SPF &amp; mail\"</Value>") == 2u,
		"aws remove must preserve unrelated TXT values");
	xrtBufferUnit(&Request);
	testRequire(xacmeAwsBuildTxtChange(&Set, "_acme-challenge.example.com",
		"absent", false, &Request, &bChanged) && !bChanged,
		"aws absent TXT removal must be idempotent");
	testRequire(xacmeAwsParseTxtSet(sNoMatch,
		"_acme-challenge.example.com", &Set) && !Set.bPresent,
		"aws nonmatching first record must be absent");
	testRequire(xacmeAwsBuildTxtChange(&Set, "_acme-challenge.example.com",
		"fresh-token", true, &Request, &bChanged) && bChanged &&
		countText((cstr)Request.Data, "<Action>CREATE</Action>") == 1u &&
		countText((cstr)Request.Data, "<Action>DELETE</Action>") == 0u &&
		countText((cstr)Request.Data, "<TTL>60</TTL>") == 1u,
		"aws absent TXT add must create one set");
	xrtBufferUnit(&Request);
	testRequire(xacmeAwsParseTxtSet(sOne,
		"_acme-challenge.example.com", &Set) && Set.bPresent,
		"aws single TXT parse failed");
	testRequire(xacmeAwsBuildTxtChange(&Set, "_acme-challenge.example.com",
		"old-token", false, &Request, &bChanged) && bChanged &&
		countText((cstr)Request.Data, "<Action>DELETE</Action>") == 1u &&
		countText((cstr)Request.Data, "<Action>CREATE</Action>") == 0u &&
		countText((cstr)Request.Data, "&quot;old-token&quot;") == 1u,
		"aws last TXT removal must delete the whole set");
	xrtBufferUnit(&Request);
	testRequire(xacmeAwsParseTxtSet(sEmpty,
		"_acme-challenge.example.com", &Set) && !Set.bPresent,
		"aws empty record list must be absent");
	testRequire(!xacmeAwsParseTxtSet(
		"<ListResourceRecordSetsResponse><ResourceRecordSets/>",
		"_acme-challenge.example.com", &Set),
		"aws truncated XML response must be rejected");
	testRequire(!xacmeAwsParseTxtSet(
		"<ListResourceRecordSetsResponse><ResourceRecordSets>"
		"<ResourceRecordSet><Name>_acme-challenge.example.com.</Name>"
		"<Type>TXT</Type><SetIdentifier>weighted</SetIdentifier>"
		"<TTL>60</TTL></ResourceRecordSet></ResourceRecordSets>"
		"</ListResourceRecordSetsResponse>",
		"_acme-challenge.example.com", &Set),
		"aws routing-policy set must be rejected safely");
	{
		char sLarge[4001];
		size_t i;
		memset(sLarge, 'x', 4000u);
		sLarge[0] = '"';
		sLarge[3999] = '"';
		sLarge[4000] = '\0';
		memset(&Set, 0, sizeof(Set));
		Set.bPresent = true;
		strcpy(Set.sTtl, "60");
		Set.iCount = 5u;
		for(i = 0u; i < Set.iCount; i++)
			Set.Values[i] = (xstrview){ sLarge, 4000u };
		testRequire(!xacmeAwsBuildTxtChange(&Set,
			"_acme-challenge.example.com", "new-token", true,
			&Request, &bChanged),
			"aws must reject a change beyond the batch value quota");
		Set.iCount = XACME_AWS_TXT_MAX_VALUES;
		for(i = 0u; i < Set.iCount; i++)
			Set.Values[i] = (xstrview){ "\"x\"", 3u };
		testRequire(!xacmeAwsBuildTxtChange(&Set,
			"_acme-challenge.example.com", "new-token", true,
			&Request, &bChanged),
			"aws must reject adding beyond the record-set count quota");
	}
}

int main(void)
{
	testOfflineZones();
	testOfflineRecordSet();
	const char* sKey = getenv("XACME_AWS_KEY");
	const char* sSecret = getenv("XACME_AWS_SECRET");
	const char* sRegion = getenv("XACME_AWS_REGION");
	const char* sFqdnEnv = getenv("XACME_AWS_FQDN");
	cstr sFqdn = ((sFqdnEnv != NULL) && (sFqdnEnv[0] != '\0')) ?
		sFqdnEnv : "_acme-challenge.test.xxrpa.com";
	const char* sValue = "xacme-live-probe-20260915";
	xacmednsawsconfig Config;
	xacmednsprovider Provider;

	/* 参数错误语义（常跑）。 */
	{
		xacmednsawsconfig Bad;
		xacmednsprovider BadProvider;
		xrtAcmeDnsAwsConfigInit(&Bad);
		xrtClearError();
		testRequire(
			!xrtAcmeDnsAws(&Bad, NULL, &BadProvider) &&
				(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"acme dns_aws missing credentials mismatch"
		);
	}
	{
		char sTooLong[256];
		xacmednsawsconfig Bad;
		xacmednsprovider BadProvider;
		memset(sTooLong, 'x', sizeof(sTooLong) - 1u);
		sTooLong[sizeof(sTooLong) - 1u] = '\0';
		xrtAcmeDnsAwsConfigInit(&Bad);
		Bad.sAccessKeyId = sTooLong;
		Bad.sSecretAccessKey = "key";
		xrtClearError();
		testRequire(!xrtAcmeDnsAws(&Bad, NULL, &BadProvider) &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"acme dns_aws must reject truncated access key id");
		Bad.sAccessKeyId = "id";
		Bad.sSecretAccessKey = sTooLong;
		xrtClearError();
		testRequire(!xrtAcmeDnsAws(&Bad, NULL, &BadProvider) &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"acme dns_aws must reject truncated secret key");
		Bad.sSecretAccessKey = "key";
		Bad.sRegion = sTooLong;
		xrtClearError();
		testRequire(!xrtAcmeDnsAws(&Bad, NULL, &BadProvider) &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"acme dns_aws must reject truncated region");
		Bad.sRegion = NULL;
		Bad.sEndpoint = sTooLong;
		xrtClearError();
			testRequire(!xrtAcmeDnsAws(&Bad, NULL, &BadProvider) &&
				xrtErrorKind(xrtGetError()) == XERR_RANGE,
				"acme dns_aws must reject truncated endpoint");
	}
	{
		xacmednsawsconfig Offline;
		xacmednsprovider OfflineProvider;
		xrtAcmeDnsAwsConfigInit(&Offline);
		Offline.sAccessKeyId = "offline-id";
		Offline.sSecretAccessKey = "offline-secret";
		testRequire(xrtAcmeDnsAws(&Offline, NULL, &OfflineProvider),
			"acme dns_aws offline construct failed");
		xrtClearError();
		testRequire(!OfflineProvider.Add(&OfflineProvider,
			(xstrview){ "bad<name.example.com", 20u },
			(xstrview){ "token", 5u }) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"acme dns_aws must reject invalid owner before network");
		xrtAcmeDnsAwsProviderUnit(&OfflineProvider);
	}

	if((sKey == NULL) || (sKey[0] == '\0') || (sSecret == NULL) ||
		(sSecret[0] == '\0'))
	{
		printf("[SKIP] acme dns_aws live (XACME_AWS_KEY unset)\n");
		return 0;
	}

	xrtAcmeDnsAwsConfigInit(&Config);
	Config.sAccessKeyId = sKey;
	Config.sSecretAccessKey = sSecret;
	Config.sRegion = sRegion;
	testRequire(
		xrtAcmeDnsAws(&Config, NULL, &Provider),
		"acme dns_aws construct failed"
	);

	if(!Provider.Add(&Provider, (xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sValue, strlen(sValue) }))
	{
		xrtAcmeDnsAwsProviderUnit(&Provider);
		testRequire(false, "acme dns_aws add failed");
	}

	{
		xacmedns Probe;
		testRequire(
			xacmeDnsInit(&Probe, NULL),
			"acme dns_aws probe init failed"
		);
		testRequire(
			xacmeDnsTxtWait(&Probe, "8.8.8.8", 53u, sFqdn, sValue, 60000u),
			"acme dns_aws txt not visible"
		);
		xacmeDnsUnit(&Probe);
	}
	printf("[aws] TXT added and visible via resolver\n");

	testRequire(
		Provider.Remove(&Provider,
			(xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sValue, strlen(sValue) }),
		"acme dns_aws remove failed"
	);

	xrtAcmeDnsAwsProviderUnit(&Provider);
	printf("[PASS] acme dns_aws live\n");
	return 0;
}
