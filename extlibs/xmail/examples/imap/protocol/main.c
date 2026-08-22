#include <stdio.h>
#include <xmail.h>



/* 解析服务器响应并构建一条 IMAP 命令。 */
int main(void)
{
	ximapresponseview Response;
	char arrCommand[64];
	size_t iSize;

	if ( !xrtImapResponseParse(
		XRT_STR_LITERAL("A001 OK authenticated"),
		&Response
	) || !xrtImapCommandWrite(
		XRT_STR_LITERAL("A002"),
		XRT_STR_LITERAL("SELECT"),
		XRT_STR_LITERAL("\"INBOX\""),
		0,
		arrCommand,
		sizeof(arrCommand),
		&iSize
	) ) {
		return 1;
	}
	printf("status=%d command=%.*s",
		(int)Response.Status, (int)iSize, arrCommand);
	return 0;
}
