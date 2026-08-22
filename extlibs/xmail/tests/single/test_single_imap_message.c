#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_IMAP_MESSAGE
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* IMAP 消息桥单头必须保留命令、数据、MIME 树和连续缓冲。 */
int main(void)
{
	#if !defined(XMAIL_FEATURE_IMAP_MESSAGE) || \
		!defined(XMAIL_FEATURE_IMAP_COMMAND) || \
		!defined(XMAIL_FEATURE_IMAP_DATA) || \
		!defined(XMAIL_FEATURE_MAIL_TREE) || \
		!defined(XRT_FEATURE_BUFFER)
		#error "XMAIL_MODULE_IMAP_MESSAGE dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_IMAP_AUTH) || \
		defined(XMAIL_FEATURE_IMAP_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_IMAP_APPEND) || \
		defined(XMAIL_FEATURE_IMAP_COMPRESS) || \
		defined(XMAIL_FEATURE_SMTP_CLIENT) || \
		defined(XMAIL_FEATURE_POP3_CLIENT)
		#error "XMAIL_MODULE_IMAP_MESSAGE retained unrelated features"
	#endif

	return xrtImapClientBodyBytes(
		NULL,
		1u,
		XRT_STR_LITERAL(""),
		false,
		true,
		1024u,
		NULL,
		0,
		NULL
	) == NULL ? 0 : 1;
}
