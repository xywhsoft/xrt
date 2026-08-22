#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_IMAP_APPEND
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* IMAP APPEND 单头应只闭包客户端和协议基础。 */
int main(void)
{
	ximapappendconfig Config;

	#if !defined(XMAIL_FEATURE_IMAP_APPEND) || \
		!defined(XMAIL_FEATURE_IMAP_CLIENT)
		#error "XMAIL_MODULE_IMAP_APPEND dependency closure mismatch"
	#endif
	#if defined(XMAIL_FEATURE_IMAP_COMMAND) || \
		defined(XMAIL_FEATURE_IMAP_AUTH) || \
		defined(XMAIL_FEATURE_IMAP_CLIENT_TLS)
		#error "XMAIL_MODULE_IMAP_APPEND retained unrelated features"
	#endif

	xrtImapAppendConfigInit(&Config);
	return Config.Literal == XIMAP_LITERAL_AUTO ? 0 : 1;
}
