#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_IMAP_COMMAND
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* IMAP 命令层单头应带入客户端，不隐式带入 TLS 和认证。 */
int main(void)
{
	ximapmailboxinfo Info;

	#if !defined(XMAIL_FEATURE_IMAP_COMMAND) || \
		!defined(XMAIL_FEATURE_IMAP_CLIENT)
		#error "XMAIL_MODULE_IMAP_COMMAND dependency closure mismatch"
	#endif
	#if defined(XMAIL_FEATURE_IMAP_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_IMAP_AUTH)
		#error "XMAIL_MODULE_IMAP_COMMAND retained unrelated features"
	#endif

	xrtImapMailboxInfoInit(&Info);
	return Info.Present == 0 ? 0 : 1;
}
