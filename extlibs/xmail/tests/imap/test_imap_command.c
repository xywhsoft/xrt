#include "../test.h"



/* 验证邮箱摘要可从数字响应和响应码增量更新。 */
static void testImapMailboxInfo(void)
{
	ximapmailboxinfo Info;
	ximapresponseview Response;

	xrtImapMailboxInfoInit(&Info);
	testRequire(xrtImapResponseParse(
		XRT_STR_LITERAL("* 23 EXISTS"),
		&Response
	) && (xrtImapMailboxInfoUpdate(&Response, &Info) == XMAIL_NEXT_ITEM),
		"IMAP mailbox EXISTS update failed");
	testRequire(xrtImapResponseParse(
		XRT_STR_LITERAL("* OK [UIDVALIDITY 42] stable"),
		&Response
	) && (xrtImapMailboxInfoUpdate(&Response, &Info) == XMAIL_NEXT_ITEM),
		"IMAP mailbox UIDVALIDITY update failed");
	testRequire(xrtImapResponseParse(
		XRT_STR_LITERAL("A2 OK [READ-WRITE] selected"),
		&Response
	) && (xrtImapMailboxInfoUpdate(&Response, &Info) == XMAIL_NEXT_ITEM) &&
		(Info.Exists == UINT64_C(23)) &&
		(Info.UidValidity == UINT64_C(42)) && !Info.ReadOnly &&
		((Info.Present & (XIMAP_MAILBOX_EXISTS |
		 XIMAP_MAILBOX_UID_VALIDITY | XIMAP_MAILBOX_ACCESS)) ==
		 (XIMAP_MAILBOX_EXISTS | XIMAP_MAILBOX_UID_VALIDITY |
		 XIMAP_MAILBOX_ACCESS)),
		"IMAP mailbox summary mismatch");
	testRequire(xrtImapResponseParse(
		XRT_STR_LITERAL("* 4 EXPUNGE"),
		&Response
	) && (xrtImapMailboxInfoUpdate(&Response, &Info) == XMAIL_NEXT_ITEM) &&
		(Info.Exists == UINT64_C(22)),
		"IMAP mailbox EXPUNGE update failed");

	xrtClearError();
	testRequire(xrtImapResponseParse(
		XRT_STR_LITERAL("* OK [UIDNEXT invalid] malformed"),
		&Response
	) && (xrtImapMailboxInfoUpdate(&Response, &Info) == XMAIL_NEXT_ERROR) &&
		(Info.UidNext == 0) &&
		((Info.Present & XIMAP_MAILBOX_UID_NEXT) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL),
		"malformed IMAP mailbox number was accepted");

	xrtClearError();
	testRequire(xrtImapResponseParse(
		XRT_STR_LITERAL("* OK [READ-ONLY invalid] malformed"),
		&Response
	) && (xrtImapMailboxInfoUpdate(&Response, &Info) == XMAIL_NEXT_ERROR) &&
		!Info.ReadOnly &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL),
		"malformed IMAP mailbox access code was accepted");
}



/* 运行 IMAP 命令层无网络单元测试。 */
int main(void)
{
	testImapMailboxInfo();
	return 0;
}
