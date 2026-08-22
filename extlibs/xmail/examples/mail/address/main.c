#include <stdio.h>
#include <xmail.h>



/* 展示常见 mailbox 构建和地址列表遍历。 */
int main(void)
{
	const xmailaddress arrRecipients[] = {
		{ XRT_STR_INIT("中文用户"), XRT_STR_INIT("user@example.com") },
		{ XRT_STR_INIT(""), XRT_STR_INIT("next@example.net") }
	};
	xmailaddresscursor Cursor;
	xmailaddressview Address;
	xstrview Mailbox;
	size_t iMailboxSize;
	str sMailbox = xrtMailAddressList(
		arrRecipients,
		2u,
		XMAIL_WORD_BASE64,
		XMAIL_ADDRESS_DEFAULT,
		&iMailboxSize
	);

	if ( sMailbox == NULL ) {
		return 1;
	}
	printf("%s\n", sMailbox);
	Mailbox.Data = sMailbox;
	Mailbox.Size = iMailboxSize;
	if ( !xrtMailAddressCursorInit(
		&Cursor,
		Mailbox,
		XMAIL_ADDRESS_DEFAULT
	) || (xrtMailAddressNext(&Cursor, &Address) != XMAIL_NEXT_ITEM) ) {
		xrtFree(sMailbox);
		return 2;
	}
	printf("local=%.*s domain=%.*s\n",
		(int)Address.Local.Size,
		Address.Local.Data,
		(int)Address.Domain.Size,
		Address.Domain.Data);
	xrtFree(sMailbox);
	return 0;
}
