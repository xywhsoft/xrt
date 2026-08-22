#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_POP3_MESSAGE
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* POP3 消息桥单头必须保留流式客户端、MIME 树和连续缓冲。 */
int main(void)
{
	#if !defined(XMAIL_FEATURE_POP3_MESSAGE) || \
		!defined(XMAIL_FEATURE_POP3_CLIENT) || \
		!defined(XMAIL_FEATURE_MAIL_TREE) || \
		!defined(XRT_FEATURE_BUFFER)
		#error "XMAIL_MODULE_POP3_MESSAGE dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_POP3_AUTH) || \
		defined(XMAIL_FEATURE_POP3_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_IMAP_CLIENT) || \
		defined(XMAIL_FEATURE_SMTP_CLIENT)
		#error "XMAIL_MODULE_POP3_MESSAGE retained unrelated features"
	#endif

	return xrtPop3ClientRetrBytes(NULL, 1u, 1024u, NULL, 0, NULL) == NULL ?
		0 : 1;
}
