#ifdef XIMAP_MODULE_XIMAP
	#undef XIMAP_MODULE_XIMAP
#endif
#define XIMAP_MODULE_IMAP_COMMAND
#define XIMAP_IMPLEMENTATION
#include "../../single/ximap.h"



/* IMAP 命令层单头应带入客户端，不隐式带入 TLS 和认证。 */
int main(void)
{
	ximapmailboxinfo Info;

	#if !defined(XIMAP_FEATURE_IMAP_COMMAND) || \
		!defined(XIMAP_FEATURE_IMAP_CLIENT)
		#error "XIMAP_MODULE_IMAP_COMMAND dependency closure mismatch"
	#endif
	#if defined(XIMAP_FEATURE_IMAP_CLIENT_TLS) || \
		defined(XIMAP_FEATURE_IMAP_AUTH)
		#error "XIMAP_MODULE_IMAP_COMMAND retained unrelated features"
	#endif

	xrtImapMailboxInfoInit(&Info);
	return Info.Present == 0 ? 0 : 1;
}
