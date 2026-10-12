#include "http_client_response_fixture.h"



/* 验证多字段线路顺序、策略跳过、查询和代理选择。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Basic realm=\"basic\"")
		},
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT(
				"Digest realm=\"legacy\", nonce=\"m\", "
				"algorithm=MD5, qop=\"auth\""
			)
		},
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT(
				"Digest realm=\"api\", nonce=\"sha\", "
				"algorithm=SHA-256, qop=\"auth, auth-int\""
			)
		},
		{
			XRT_STR_INIT("Proxy-Authenticate"),
			XRT_STR_INIT(
				"Digest realm=\"proxy\", nonce=\"p\", "
				"algorithm=SHA-256, qop=\"auth\""
			)
		}
	};
	xhttpresponse* pResponse = testHttpResponseFixtureCreate(
		Fields, sizeof(Fields) / sizeof(Fields[0])
	);
	xhttpdigestpolicy Policy;
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
	char Output[32];
	size_t iSize;

	testRequire((xrtHttpResponseDigestChallengeChoose(
		pResponse,
		NULL,
		NULL,
		0,
		&iSize,
		&Challenge,
		&Choice
	) == XHTTP_NEXT_ITEM) && (iSize == 6u) &&
		(Challenge.Algorithm == XHTTP_DIGEST_ALGORITHM_SHA256) &&
		(Challenge.Realm.Size == 0) &&
		(Choice.Qop == XHTTP_DIGEST_QOP_AUTH),
		"HTTP response Digest choice query mismatch");
	testRequire((xrtHttpResponseDigestChallengeChoose(
		pResponse,
		NULL,
		Output,
		sizeof(Output),
		&iSize,
		&Challenge,
		&Choice
	) == XHTTP_NEXT_ITEM) &&
		testHttpResponseFixtureText(Challenge.Realm, "api") &&
		testHttpResponseFixtureText(Challenge.Nonce, "sha"),
		"HTTP response Digest choice decode mismatch");
	testRequire((xrtHttpResponseProxyDigestChallengeChoose(
		pResponse,
		NULL,
		Output,
		sizeof(Output),
		&iSize,
		&Challenge,
		&Choice
	) == XHTTP_NEXT_ITEM) &&
		testHttpResponseFixtureText(Challenge.Realm, "proxy"),
		"HTTP response proxy Digest choice mismatch");

	xrtHttpDigestPolicyInit(&Policy);
	Policy.Qops = XHTTP_DIGEST_QOPS_AUTH_INT;
	testRequire((xrtHttpResponseDigestChallengeChoose(
		pResponse,
		&Policy,
		Output,
		sizeof(Output),
		&iSize,
		&Challenge,
		&Choice
	) == XHTTP_NEXT_ITEM) &&
		(Choice.Qop == XHTTP_DIGEST_QOP_AUTH_INT),
		"HTTP response Digest auth-int choice mismatch");
	Policy.Algorithms = XHTTP_DIGEST_ALGORITHMS_MD5;
	testRequire(xrtHttpResponseDigestChallengeChoose(
		pResponse,
		&Policy,
		Output,
		sizeof(Output),
		&iSize,
		&Challenge,
		&Choice
	) == XHTTP_NEXT_END,
		"HTTP response Digest choice accepted unavailable MD5");

	/* 结构输出允许未对齐，发布仍必须保持逐字段一致。 */
	{
		uint8 SizeStorage[sizeof(size_t) + 1u];
		uint8 ChallengeStorage[sizeof(xhttpdigestchallenge) + 1u];
		uint8 ChoiceStorage[sizeof(xhttpdigestchoice) + 1u];
		size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);
		xhttpdigestchallenge* pChallenge =
			(xhttpdigestchallenge*)(void*)(ChallengeStorage + 1u);
		xhttpdigestchoice* pChoice =
			(xhttpdigestchoice*)(void*)(ChoiceStorage + 1u);

		testRequire(xrtHttpResponseDigestChallengeChoose(
			pResponse,
			NULL,
			Output,
			sizeof(Output),
			pSize,
			pChallenge,
			pChoice
		) == XHTTP_NEXT_ITEM,
			"HTTP response unaligned Digest choice failed");
		memcpy(&iSize, pSize, sizeof(iSize));
		memcpy(&Challenge, pChallenge, sizeof(Challenge));
		memcpy(&Choice, pChoice, sizeof(Choice));
		testRequire((iSize == 6u) &&
			testHttpResponseFixtureText(Challenge.Realm, "api") &&
			(Choice.Algorithm == XHTTP_DIGEST_ALGORITHM_SHA256),
			"HTTP response unaligned Digest choice mismatch");
	}

	testRequire(xrtHttpResponseDigestChallengeChoose(
		pResponse,
		NULL,
		Output,
		5u,
		&iSize,
		&Challenge,
		&Choice
	) == XHTTP_NEXT_ERROR,
		"HTTP response Digest choice accepted short output");
	testHttpResponseFixtureError(
		XERR_RANGE,
		XHTTP_RESPONSE_ERROR_AUTH,
		"HTTP response Digest choice short output error mismatch"
	);

	{
		const xhttpfield* pField = xrtHttpHeadersData(
			xrtHttpResponseHeaders(pResponse)
		);

		testRequire(xrtHttpResponseDigestChallengeChoose(
			pResponse,
			NULL,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge,
			(xhttpdigestchoice*)(void*)pField->Value.Data
		) == XHTTP_NEXT_ERROR,
			"HTTP response Digest choice accepted response alias");
		testHttpResponseFixtureError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_ARGUMENT,
			"HTTP response Digest choice alias error mismatch"
		);
	}
	{
		xstrview Reason = xrtHttpResponseReason(pResponse);

		testRequire(xrtHttpResponseDigestChallengeChoose(
			pResponse,
			NULL,
			(void*)Reason.Data,
			Reason.Size + 1u,
			&iSize,
			&Challenge,
			&Choice
		) == XHTTP_NEXT_ERROR,
			"HTTP response Digest choice accepted response text output");
		testHttpResponseFixtureError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_ARGUMENT,
			"HTTP response Digest choice text error mismatch"
		);
	}
	testRequire(xrtHttpResponseDigestChallengeChoose(
		pResponse,
		NULL,
		Output,
		sizeof(Output),
		(size_t*)(void*)xrtHttpResponseHeaders(pResponse),
		&Challenge,
		&Choice
	) == XHTTP_NEXT_ERROR,
		"HTTP response Digest choice accepted Header container output");
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP response Digest choice Header container error mismatch"
	);
	testRequire(xrtHttpResponseDigestChallengeChoose(
		pResponse,
		NULL,
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		4u,
		&iSize,
		&Challenge,
		&Choice
	) == XHTTP_NEXT_ERROR,
		"HTTP response Digest choice accepted wrapping output");
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP response Digest choice wrapping output error mismatch"
	);
	testRequire(
		xrtHttpResponseStatus(pResponse) == XHTTP_STATUS_OK,
		"HTTP response Digest choice alias damaged response"
	);
	xrtHttpResponseDestroy(pResponse);

	{
		static const xhttpfield Invalid[] = {
			{
				XRT_STR_INIT("WWW-Authenticate"),
				XRT_STR_INIT("Digest realm=\"api\", nonce=\"n\"")
			}
		};

		pResponse = testHttpResponseFixtureCreate(Invalid, 1u);
		testRequire(xrtHttpResponseDigestChallengeChoose(
			pResponse,
			NULL,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge,
			&Choice
		) == XHTTP_NEXT_ERROR,
			"HTTP response Digest choice skipped malformed candidate");
		testHttpResponseFixtureError(
			XERR_VALUE,
			XHTTP_RESPONSE_ERROR_AUTH,
			"HTTP response malformed Digest choice error mismatch"
		);
		xrtHttpResponseDestroy(pResponse);
	}
	puts("[PASS] HTTP client response Digest choice");
	return 0;
}
