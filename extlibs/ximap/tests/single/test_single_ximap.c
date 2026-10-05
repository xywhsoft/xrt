#ifdef XIMAP_MODULE_XIMAP
	#undef XIMAP_MODULE_XIMAP
#endif
#define XIMAP_MODULE_XIMAP
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



/* ximap 产品伞单头应点亮自身全部协议与基座闭包。 */
int main(void)
{
	ximapresponseview Response;

	#if (!defined(XIMAP_FEATURE_IMAP) || \
	!defined(XIMAP_FEATURE_IMAP_DATA) || \
	!defined(XIMAP_FEATURE_IMAP_BODY) || \
	!defined(XIMAP_FEATURE_IMAP_CLIENT) || \
	!defined(XIMAP_FEATURE_IMAP_CLIENT_TLS) || \
	!defined(XIMAP_FEATURE_IMAP_AUTH) || \
	!defined(XIMAP_FEATURE_IMAP_COMMAND) || \
	!defined(XIMAP_FEATURE_IMAP_MESSAGE) || \
	!defined(XIMAP_FEATURE_IMAP_APPEND) || \
	!defined(XIMAP_FEATURE_IMAP_COMPRESS) || \
	!defined(XMAIL_FEATURE_MAIL_WIRE) || \
	!defined(XMAIL_FEATURE_MAIL_NET) || \
	!defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
	!defined(XMAIL_FEATURE_MAIL_TREE))
		#error "XIMAP_MODULE_XIMAP dependency closure is incomplete"
	#endif

	return xrtImapResponseParse(
		XRT_STR_LITERAL("* 23 FETCH (BODY[] {4096}"),
		&Response
	) ? 0 : 1;
}
