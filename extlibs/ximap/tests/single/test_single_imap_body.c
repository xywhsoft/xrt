#ifdef XIMAP_MODULE_XIMAP
	#undef XIMAP_MODULE_XIMAP
#endif
#define XIMAP_MODULE_IMAP_BODY
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



/* IMAP BODYSTRUCTURE 单头闭包只保留协议与数据层。 */
int main(void)
{
	ximapbodyview Body;

	#if !defined(XIMAP_FEATURE_IMAP_BODY) || \
		!defined(XIMAP_FEATURE_IMAP_DATA) || \
		!defined(XIMAP_FEATURE_IMAP)
		#error "XIMAP_MODULE_IMAP_BODY dependency closure is incomplete"
	#endif
	#if defined(XIMAP_FEATURE_IMAP_CLIENT) || \
		defined(XMAIL_FEATURE_MAIL_NET) || \
		defined(XRT_FEATURE_NET)
		#error "XIMAP_MODULE_IMAP_BODY unexpectedly retained networking"
	#endif

	return xrtImapBodyParse(
		XRT_STR_LITERAL(
			"(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1)"
		),
		&Body
	) && (Body.Kind == XIMAP_BODY_TEXT) ? 0 : 1;
}
