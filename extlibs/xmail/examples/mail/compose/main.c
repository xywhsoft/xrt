#include <stdio.h>
#include <xmail.h>



/* 使用一行式 Compose 构建带 UTF-8 主题的文本邮件。 */
int main(void)
{
	xmailmessage Message;
	xmailaddress To;
	str sOutput;
	size_t iSize;

	xrtMailMessageInit(&Message);
	Message.From = (xmailaddress){
		XRT_STR_LITERAL("Sender"),
		XRT_STR_LITERAL("sender@example.com")
	};
	To = (xmailaddress){
		XRT_STR_LITERAL("Receiver"),
		XRT_STR_LITERAL("receiver@example.net")
	};
	Message.To = &To;
	Message.ToCount = 1u;
	Message.Subject = XRT_STR_LITERAL("示例邮件");
	Message.Text = XRT_STR_LITERAL("hello from xmail");

	sOutput = xrtMailCompose(&Message, &iSize);
	if ( sOutput == NULL ) {
		return 1;
	}
	fwrite(sOutput, 1u, iSize, stdout);
	xrtFree(sOutput);
	return 0;
}
