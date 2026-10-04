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

/* 按 AWS SigV4 公式独立计算的 Route53 派生密钥固定向量。 */
static void testAwsSigningKey(void)
{
	static const uint8 Expected[32] = {
		0x88, 0x6c, 0x84, 0x4a, 0x71, 0xd6, 0x55, 0x04,
		0x13, 0xda, 0xae, 0xf6, 0xb8, 0x3a, 0xdd, 0xe8,
		0x18, 0x53, 0x27, 0xc9, 0xf0, 0x37, 0x94, 0x22,
		0x25, 0x05, 0x8c, 0x67, 0x93, 0xc0, 0x37, 0xef
	};
	char sTooLong[256];
	uint8 Key[32];
	testRequire(xacmeAwsSigningKey(
		"wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY",
		"20130524", "us-east-1", Key) &&
		memcmp(Key, Expected, sizeof(Key)) == 0,
		"aws Route53 SigV4 signing key vector mismatch");
	memset(sTooLong, 's', sizeof(sTooLong) - 1u);
	sTooLong[sizeof(sTooLong) - 1u] = '\0';
	xrtClearError();
	testRequire(!xacmeAwsSigningKey(sTooLong, "20130524",
		"us-east-1", Key) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"aws Route53 must reject a truncated signing secret");
	xrtClearError();
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

static void testAwsChangeAcknowledgment(void)
{
	static const char* Valid[] = {
		"<ChangeResourceRecordSetsResponse><ChangeInfo><Id>/change/1</Id><Status>PENDING</Status><SubmittedAt>2026-10-01T00:00:00Z</SubmittedAt></ChangeInfo></ChangeResourceRecordSetsResponse>",
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<ChangeResourceRecordSetsResponse xmlns=\"https://route53.amazonaws.com/doc/2013-04-01/\"><ChangeInfo><Comment>a &amp; b</Comment><Id>/change/1</Id><Status>INSYNC</Status><SubmittedAt>2026-10-01T00:00:00.123Z</SubmittedAt></ChangeInfo></ChangeResourceRecordSetsResponse>\n",
		"<ChangeResourceRecordSetsResponse><ChangeInfo><Comment/><Id>/change/1</Id><Status>PENDING</Status><SubmittedAt>2026-10-01T00:00:00Z</SubmittedAt></ChangeInfo></ChangeResourceRecordSetsResponse>"
	};
	static const char* Invalid[] = {
		NULL, "", "<ErrorResponse><Code>AccessDenied</Code></ErrorResponse>",
		"<?xml version=\"1.0\"",
		"<ChangeResourceRecordSetsResponse><ChangeInfo><Id>1</Id><Status>PENDING</Status></ChangeInfo></ChangeResourceRecordSetsResponse>",
		"<ChangeResourceRecordSetsResponse><ChangeInfo><Id>1</Id><Status>FAILED</Status><SubmittedAt>x</SubmittedAt></ChangeInfo></ChangeResourceRecordSetsResponse>",
		"<ChangeResourceRecordSetsResponse><ChangeInfo><Id></Id><Status>PENDING</Status><SubmittedAt>x</SubmittedAt></ChangeInfo></ChangeResourceRecordSetsResponse>",
		"<ChangeResourceRecordSetsResponse><ChangeInfo><Id>1</Id><Status>PENDING</Status><SubmittedAt></SubmittedAt></ChangeInfo></ChangeResourceRecordSetsResponse>",
		"<ChangeResourceRecordSetsResponse><ChangeInfo><Id>1</Id><Status>PENDING</Status><SubmittedAt>x</SubmittedAt></ChangeInfo>",
		"<ChangeResourceRecordSetsResponse><ChangeInfo><Id>1</Id><Status>PENDING</Status><SubmittedAt>x</SubmittedAt></ChangeInfo></ChangeResourceRecordSetsResponse>extra",
		"<ChangeResourceRecordSetsResponse><ChangeInfo><Id>1</Id><Id>2</Id><Status>PENDING</Status><SubmittedAt>x</SubmittedAt></ChangeInfo></ChangeResourceRecordSetsResponse>"
	};
	for(size_t i = 0u; i < sizeof(Valid) / sizeof(Valid[0]); i++)
		testRequire(xacmeAwsChangeAccepted(Valid[i]), "valid Route53 change acknowledgment rejected");
	for(size_t i = 0u; i < sizeof(Invalid) / sizeof(Invalid[0]); i++)
		testRequire(!xacmeAwsChangeAccepted(Invalid[i]), "invalid Route53 change acknowledgment accepted");
}

static void testAwsErrorEnvelope(void)
{
	static const struct { cstr sXml; cstr sCode; } Valid[] = {
		{ "<ErrorResponse><Error><Code>InvalidChangeBatch</Code></Error></ErrorResponse>", "InvalidChangeBatch" },
		{ "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<ErrorResponse xmlns=\"https://route53.amazonaws.com/doc/2013-04-01/\"><Error><Type>Sender</Type><Code>Throttling</Code><Message>Rate &amp; &#101;xceeded</Message></Error><RequestId>x</RequestId></ErrorResponse>\n", "Throttling" },
		{ "<?xml version='1.0' encoding='UTF-8'?><!--before--><r:ErrorResponse xmlns:r='http://route53.amazonaws.com/doc/2013-04-01/'><r:Error><r:Message/><r:Code>PriorRequestNotComplete</r:Code><r:Type /></r:Error><r:RequestId /></r:ErrorResponse><!--after-->", "PriorRequestNotComplete" },
		{ "<ErrorResponse ><Error><Code>Invalid&#x43;hange<!-- split -->Batch</Code><Messages><Message>a &lt; b</Message><Message><![CDATA[raw <&>]]></Message></Messages></Error></ErrorResponse>", "InvalidChangeBatch" },
		{ "\xef\xbb\xbf<?xml version=\"1.0\"?><ErrorResponse><Error><Code><![CDATA[AccessDenied]]></Code><Message>\xe4\xb8\xad\xf0\x9f\x98\x80</Message></Error></ErrorResponse>", "AccessDenied" },
		{ "<ErrorResponse><Error><Code>InvalidInput</Code><Message>InvalidChangeBatch</Message></Error></ErrorResponse>", "InvalidInput" },
		{ "<ErrorResponse><Error><Code>InvalidChangeBatchExtra</Code><Messages /></Error></ErrorResponse>", "InvalidChangeBatchExtra" }
	};
	static const cstr Invalid[] = {
		"", "<ErrorResponse><Code>InvalidChangeBatch</Code></ErrorResponse>",
		"<ErrorResponse><Error><Code /></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code></Code></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code> InvalidChangeBatch</Code></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><Code>InvalidInput</Code></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code></Error><Error><Code>InvalidInput</Code></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code></Error></ErrorResponse>x",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><Message>&bogus;</Message></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><Message>&#0;</Message></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><Message>&#xD800;</Message></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><Message>&#x110000;</Message></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><Message>&#;</Message></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><Message>&#xG;</Message></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><Message>\xc0\xaf</Message></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><Message>\xed\xa0\x80</Message></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><Message>\x01</Message></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><!--bad--comment--></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><Message>]]></Message></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><Message><Code>InvalidInput</Code></Message></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code><Message/><Message/></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>InvalidChangeBatch</Code></Error><RequestId/><RequestId/></ErrorResponse>",
		"<ErrorResponse xmlns='urn:other'><Error><Code>InvalidChangeBatch</Code></Error></ErrorResponse>",
		"<r:ErrorResponse><r:Error><r:Code>InvalidChangeBatch</r:Code></r:Error></r:ErrorResponse>",
		"<r:ErrorResponse xmlns:r='https://route53.amazonaws.com/doc/2013-04-01/'><Error><Code>InvalidChangeBatch</Code></Error></r:ErrorResponse>",
		"<!DOCTYPE ErrorResponse><ErrorResponse><Error><Code>InvalidChangeBatch</Code></Error></ErrorResponse>",
		"<?xml version=\"1.0\" encoding=\"UTF-16\"?><ErrorResponse><Error><Code>InvalidChangeBatch</Code></Error></ErrorResponse>",
		"<ErrorResponse><Error><Code>AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA</Code></Error></ErrorResponse>"
	};
	char Code[64];
	for(size_t i = 0u; i < sizeof(Valid) / sizeof(Valid[0]); i++) {
		size_t iSize = strlen(Valid[i].sXml);
		testRequire(xacmeAwsErrorCode((xstrview){ Valid[i].sXml, iSize }, Code) &&
			strcmp(Code, Valid[i].sCode) == 0, "valid Route53 error envelope rejected or code misread");
		/* Every truncated input is bounded; trailing XML whitespace/comments
		 * can themselves form a complete document, so only test before root close. */
		const char* pClose = strstr(Valid[i].sXml, "</ErrorResponse>");
		if(pClose != NULL) for(size_t n = 0u; n < (size_t)(pClose - Valid[i].sXml) + 16u; n++) {
			strcpy(Code, "stale");
			testRequire(!xacmeAwsErrorCode((xstrview){ Valid[i].sXml, n }, Code) && Code[0] == '\0',
				"truncated Route53 error accepted or stale output retained");
		}
	}
	for(size_t i = 0u; i < sizeof(Invalid) / sizeof(Invalid[0]); i++) {
		strcpy(Code, "stale");
		testRequire(!xacmeAwsErrorCode((xstrview){ Invalid[i], strlen(Invalid[i]) }, Code) && Code[0] == '\0',
			"invalid Route53 error envelope accepted or stale output retained");
	}
	{
		const char Nul[] = "<ErrorResponse><Error><Code>InvalidChangeBatch</Code></Error></ErrorResponse>\0x";
		testRequire(!xacmeAwsErrorCode((xstrview){ Nul, sizeof(Nul) - 1u }, Code) &&
			!xacmeAwsErrorCode((xstrview){ NULL, 1u }, Code) && !xacmeAwsErrorCode((xstrview){ "x", 4194305u }, Code) &&
			!xacmeAwsErrorCode((xstrview){ "x", 1u }, NULL), "error parser argument/size boundary failed");
	}
}

int main(void)
{
	testAwsErrorEnvelope();
	testAwsChangeAcknowledgment();
	testAwsSigningKey();
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
