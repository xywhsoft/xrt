#include <stdio.h>
#include <xmail.h>



/* 解析原始邮件并按报文声明解码正文。 */
int main(void)
{
	xmailmessageview Message;
	xmailtransfer Transfer;
	bytes pBody;
	size_t iSize;

	if ( !xrtMailMessageParse(
		XRT_STR_LITERAL(
			"Subject: xmail message\r\n"
			"Content-Transfer-Encoding: base64\r\n"
			"\r\n"
			"aGVsbG8=\r\n"
		),
		0,
		0,
		&Message
	) || !xrtMailMessageTransfer(&Message, &Transfer) ) {
		return 1;
	}
	pBody = xrtMailMessageBody(&Message, Transfer, 0, &iSize);
	if ( pBody == NULL ) {
		return 2;
	}
	printf("headers=%zu body=%.*s\n", Message.HeaderCount, (int)iSize, pBody);
	xrtFree(pBody);
	return 0;
}
