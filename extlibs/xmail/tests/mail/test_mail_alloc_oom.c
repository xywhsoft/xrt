#include "../test_fault_allocator.h"



/* 要求下一次分配失败且当前没有遗留底层块。 */
static void testMailFailNext(testfaultallocator* pState)
{
	pState->FailAt = pState->Calls + 1u;
	pState->Hit = false;
}



/* 检查一次失败确实命中目标且没有泄漏。 */
static void testMailRequireFailed(
	testfaultallocator* pState,
	cstr sMessage
)
{
	testRequire(pState->Hit, sMessage);
	xrtClearError();
	testMemoryDebugDrain("mail OOM memory debug reset failed");
	testRequire(pState->Live == 0, sMessage);
}



/* 覆盖全部一行式内容构建函数的最终分配和恢复路径。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	static const char sMessagePrefix[] =
		"Content-Transfer-Encoding: quoted-printable\r\n\r\n";
	xallocator Allocator = testFaultAllocator(&State);
	char arrRaw[2048];
	char arrBase64[2048];
	char arrFolded[1600];
	char arrWord[2048];
	char arrEncodedWords[1600];
	char arrName[1600];
	char arrParameter[2048];
	char arrMessage[2304];
	xmailaddress arrAddresses[2];
	xmailmessageview Message;
	size_t iSize = SIZE_MAX;
	str sText;
	bytes pData;

	memset(arrRaw, 'a', sizeof(arrRaw));
	arrRaw[700] = '\n';
	memset(arrBase64, 'A', sizeof(arrBase64));
	memset(arrWord, 'a', sizeof(arrWord));
	memset(arrName, 'n', sizeof(arrName));
	memcpy(arrParameter, "; filename=", 11u);
	memset(arrParameter + 11u, 'v', sizeof(arrParameter) - 11u);
	memcpy(
		arrMessage,
		sMessagePrefix,
		sizeof(sMessagePrefix) - 1u
	);
	memset(
		arrMessage + sizeof(sMessagePrefix) - 1u,
		'm',
		sizeof(arrMessage) - sizeof(sMessagePrefix) + 1u
	);
	arrWord[0] = '=';
	arrWord[1] = '?';
	{
		static const char sWord[] =
			"=?UTF-8?Q?aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa?= ";
		size_t iPosition = 0;

		while ( (iPosition + sizeof(sWord) - 1u) <= sizeof(arrEncodedWords) ) {
			memcpy(arrEncodedWords + iPosition, sWord, sizeof(sWord) - 1u);
			iPosition += sizeof(sWord) - 1u;
		}
		memset(arrEncodedWords + iPosition, ' ',
			sizeof(arrEncodedWords) - iPosition);
	}
	for ( size_t i = 0; i < sizeof(arrFolded); i++ ) {
		arrFolded[i] = (i & 1u) != 0 ? ' ' : 'a';
	}
	arrFolded[700] = '\r';
	arrFolded[701] = '\n';
	arrFolded[702] = ' ';
	testRequire(xrtSetAllocator(&Allocator),
		"mail OOM allocator install failed");

	testMailFailNext(&State);
	sText = xrtMailCrlf(testMailViewN(arrRaw, sizeof(arrRaw)), &iSize);
	testRequire(sText == NULL, "mail CRLF unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail CRLF OOM leaked storage");

	testMailFailNext(&State);
	sText = xrtMailQp(
		arrRaw,
		sizeof(arrRaw),
		0,
		XMAIL_QP_TEXT,
		&iSize
	);
	testRequire(sText == NULL, "mail QP unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail QP OOM leaked storage");

	testMailFailNext(&State);
	pData = xrtMailQpDecode(
		testMailViewN(arrRaw, sizeof(arrRaw)),
		0,
		&iSize
	);
	testRequire(pData == NULL, "mail QP decode unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail QP decode OOM leaked storage");

	testMailFailNext(&State);
	sText = xrtMailBase64(arrRaw, sizeof(arrRaw), 0, &iSize);
	testRequire(sText == NULL, "mail Base64 unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail Base64 OOM leaked storage");

	testMailFailNext(&State);
	pData = xrtMailBase64Decode(
		testMailViewN(arrBase64, sizeof(arrBase64)),
		&iSize
	);
	testRequire(pData == NULL, "mail Base64 decode unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail Base64 decode OOM leaked storage");

	testMailFailNext(&State);
	sText = xrtMailHeaderUnfold(
		testMailViewN(arrFolded, sizeof(arrFolded)),
		&iSize
	);
	testRequire(sText == NULL, "mail unfold unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail unfold OOM leaked storage");

	testMailFailNext(&State);
	sText = xrtMailHeader(
		XRT_STR_LITERAL("Subject"),
		testMailViewN(arrFolded, sizeof(arrFolded)),
		0,
		&iSize
	);
	testRequire(sText == NULL, "mail header unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail header OOM leaked storage");

	testMailFailNext(&State);
	sText = xrtMailWordEncode(
		testMailViewN(arrWord, sizeof(arrWord)),
		XMAIL_WORD_BASE64,
		&iSize
	);
	testRequire(sText == NULL, "mail word encode unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail word encode OOM leaked storage");

	testMailFailNext(&State);
	sText = xrtMailWordDecode(
		testMailViewN(arrEncodedWords, sizeof(arrEncodedWords)),
		XMAIL_WORD_STRICT,
		&iSize
	);
	testRequire(sText == NULL, "mail word decode unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail word decode OOM leaked storage");

	testMailFailNext(&State);
	sText = xrtMailAddress(
		testMailViewN(arrName, sizeof(arrName)),
		XRT_STR_LITERAL("user@example.com"),
		XMAIL_WORD_BASE64,
		XMAIL_ADDRESS_DEFAULT,
		&iSize
	);
	testRequire(sText == NULL, "mail address unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail address OOM leaked storage");

	arrAddresses[0] = (xmailaddress){
		XRT_STR_LITERAL("One"),
		XRT_STR_LITERAL("one@example.com")
	};
	arrAddresses[1] = (xmailaddress){
		testMailViewN(arrName, sizeof(arrName)),
		XRT_STR_LITERAL("two@example.com")
	};
	testMailFailNext(&State);
	sText = xrtMailAddressList(
		arrAddresses,
		2u,
		XMAIL_WORD_BASE64,
		XMAIL_ADDRESS_DEFAULT,
		&iSize
	);
	testRequire(sText == NULL, "mail address list unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail address list OOM leaked storage");

	testMailFailNext(&State);
	sText = xrtMailMessageId(
		testMailViewN(arrName, sizeof(arrName)),
		&iSize
	);
	testRequire(sText == NULL, "mail Message-ID unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail Message-ID OOM leaked storage");

	testMailFailNext(&State);
	sText = xrtMailParamFind(
		testMailViewN(arrParameter, sizeof(arrParameter)),
		XRT_STR_LITERAL("filename"),
		&iSize,
		NULL
	);
	testRequire(sText == NULL, "mail parameter unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail parameter OOM leaked storage");

	testMailFailNext(&State);
	sText = xrtMailParam(
		XRT_STR_LITERAL("filename"),
		testMailViewN(arrName, sizeof(arrName)),
		XMAIL_PARAM_ENCODING_AUTO,
		&iSize
	);
	testRequire(sText == NULL, "mail parameter build unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail parameter build OOM leaked storage");

	testRequire(xrtMailMessageParse(
		testMailViewN(arrMessage, sizeof(arrMessage)),
		0,
		0,
		&Message
	), "mail message OOM fixture parse failed");
	testMailFailNext(&State);
	pData = xrtMailMessageBody(
		&Message,
		XMAIL_TRANSFER_QUOTED_PRINTABLE,
		0,
		&iSize
	);
	testRequire(pData == NULL, "mail message body unexpectedly survived OOM");
	testMailRequireFailed(&State, "mail message body OOM leaked storage");

	State.FailAt = SIZE_MAX;
	sText = xrtMailHeader(
		XRT_STR_LITERAL("Subject"),
		testMailViewN(arrFolded, sizeof(arrFolded)),
		0,
		&iSize
	);
	testRequire((sText != NULL) && (iSize > 1024u),
		"mail helper did not recover after OOM");
	xrtFree(sText);
	testMemoryDebugDrain("mail OOM recovery memory debug reset failed");
	testRequire(State.Live == 0, "mail OOM recovery leaked storage");
	printf("[PASS] mail allocation OOM (16 helpers)\n");
	return 0;
}
