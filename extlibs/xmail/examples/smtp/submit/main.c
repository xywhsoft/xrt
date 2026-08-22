#include <xmail.h>



/* 使用消息描述自动建立 envelope，并直接流式提交 MIME 内容。 */
bool submitMessage(xsmtpclient* pClient, xdeadline iDeadline)
{
	xmailmessage Message;
	xmailaddress To;

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
	Message.Subject = XRT_STR_LITERAL("xmail submit");
	Message.Text = XRT_STR_LITERAL("message body");
	return xrtSmtpSubmit(pClient, &Message, iDeadline, NULL);
}



/* 示例由宿主建立、认证并提供处于 READY 状态的 SMTP Client。 */
int main(void)
{
	return 0;
}
