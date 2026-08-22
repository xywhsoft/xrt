#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_MESSAGE
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 邮件消息单头只闭合字段和传输解码依赖。 */
int main(void)
{
	xmailmessageview Message;
	xmailtransfer Transfer;
	unsigned char arrBody[8];
	size_t iSize;

	#if !defined(XMAIL_FEATURE_MAIL_MESSAGE) || \
		!defined(XMAIL_FEATURE_MAIL_HEADER) || \
		!defined(XMAIL_FEATURE_MAIL_CODEC)
		#error "XMAIL_MODULE_MAIL_MESSAGE dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_STRING) || defined(XMAIL_FEATURE_MAIL_PARAM) || \
		defined(XMAIL_FEATURE_MAIL_MULTIPART)
		#error "mail message unexpectedly enabled unrelated features"
	#endif

	if ( !xrtMailMessageParse(
		XRT_STR_LITERAL("Content-Transfer-Encoding: base64\r\n\r\naGk="),
		0,
		0,
		&Message
	) || !xrtMailMessageTransfer(&Message, &Transfer) ) {
		return 1;
	}
	return xrtMailMessageBodyWrite(
		&Message,
		Transfer,
		0,
		arrBody,
		sizeof(arrBody),
		&iSize
	) && (iSize == 2u) && (memcmp(arrBody, "hi", 2u) == 0) ? 0 : 2;
}
