#ifdef XPOP3_MODULE_XPOP3
	#undef XPOP3_MODULE_XPOP3
#endif
#define XPOP3_MODULE_POP3_MESSAGE
#define XPOP3_IMPLEMENTATION
#include "../../single/xpop3.h"



/* POP3 消息桥单头必须保留流式客户端、MIME 树和连续缓冲。 */
int main(void)
{
	#if !defined(XPOP3_FEATURE_POP3_MESSAGE) || \
		!defined(XPOP3_FEATURE_POP3_CLIENT) || \
		!defined(XMAIL_FEATURE_MAIL_TREE) || \
		!defined(XRT_FEATURE_BUFFER)
		#error "XPOP3_MODULE_POP3_MESSAGE dependency closure is incomplete"
	#endif
	#if defined(XPOP3_FEATURE_POP3_AUTH) || \
		defined(XPOP3_FEATURE_POP3_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_IMAP_CLIENT) || \
		defined(XMAIL_FEATURE_SMTP_CLIENT)
		#error "XPOP3_MODULE_POP3_MESSAGE retained unrelated features"
	#endif

	return xrtPop3ClientRetrBytes(NULL, 1u, 1024u, NULL, 0, NULL) == NULL ?
		0 : 1;
}
