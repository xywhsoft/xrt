#ifdef XIMAP_MODULE_XIMAP
	#undef XIMAP_MODULE_XIMAP
#endif
#define XIMAP_MODULE_IMAP_APPEND
#define XIMAP_IMPLEMENTATION
#include "../../include/ximap/features.h"
#include "../../../xmail/include/xmail/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#define XMAIL_IMPLEMENTATION
#include "../../../../single/extlibs/xmail.h"
#include "../../../../single/extlibs/ximap.h"



/* IMAP APPEND 单头应只闭包客户端和协议基础。 */
int main(void)
{
	ximapappendconfig Config;

	#if !defined(XIMAP_FEATURE_IMAP_APPEND) || \
		!defined(XIMAP_FEATURE_IMAP_CLIENT)
		#error "XIMAP_MODULE_IMAP_APPEND dependency closure mismatch"
	#endif
	#if defined(XIMAP_FEATURE_IMAP_COMMAND) || \
		defined(XIMAP_FEATURE_IMAP_AUTH) || \
		defined(XIMAP_FEATURE_IMAP_CLIENT_TLS)
		#error "XIMAP_MODULE_IMAP_APPEND retained unrelated features"
	#endif

	xrtImapAppendConfigInit(&Config);
	return Config.Literal == XIMAP_LITERAL_AUTO ? 0 : 1;
}
