#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_COMPOSE
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 高层 Compose 单头必须闭包内容原语，但不拉入邮件网络协议。 */
int main(void)
{
	xmailmessage Message;
	xmailaddress To;
	str sOutput;

	#if !defined(XMAIL_FEATURE_MAIL_COMPOSE) || \
		!defined(XMAIL_FEATURE_MAIL_BUILD) || \
		!defined(XMAIL_FEATURE_MAIL_CODEC) || \
		!defined(XMAIL_FEATURE_MAIL_DATE) || \
		!defined(XMAIL_FEATURE_MAIL_ID) || \
		!defined(XMAIL_FEATURE_MAIL_PARAM)
		#error "XMAIL_MODULE_MAIL_COMPOSE dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_NET) || defined(XMAIL_FEATURE_SMTP)
		#error "mail compose unexpectedly enabled transport modules"
	#endif

	xrtMailMessageInit(&Message);
	Message.From = (xmailaddress){
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL("from@example.com")
	};
	To = (xmailaddress){
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL("to@example.net")
	};
	Message.To = &To;
	Message.ToCount = 1u;
	Message.Subject = XRT_STR_LITERAL("single");
	Message.Text = XRT_STR_LITERAL("body");
	Message.Date = XRT_STR_LITERAL("Tue, 12 May 2026 10:00:00 +0000");
	Message.MessageId = XRT_STR_LITERAL("<single@example.com>");
	sOutput = xrtMailCompose(&Message, NULL);
	if ( sOutput == NULL ) {
		return 1;
	}
	if ( (strstr(sOutput, "Subject: single\r\n") == NULL) ||
		 (strstr(sOutput, "Content-Type: text/plain; charset=UTF-8") == NULL) ) {
		xrtFree(sOutput);
		return 2;
	}
	xrtFree(sOutput);
	return 0;
}
