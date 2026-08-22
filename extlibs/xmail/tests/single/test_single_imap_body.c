#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_IMAP_BODY
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* IMAP BODYSTRUCTURE 单头闭包只保留协议与数据层。 */
int main(void)
{
	ximapbodyview Body;

	#if !defined(XMAIL_FEATURE_IMAP_BODY) || \
		!defined(XMAIL_FEATURE_IMAP_DATA) || \
		!defined(XMAIL_FEATURE_IMAP)
		#error "XMAIL_MODULE_IMAP_BODY dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_IMAP_CLIENT) || \
		defined(XMAIL_FEATURE_MAIL_NET) || \
		defined(XRT_FEATURE_NET)
		#error "XMAIL_MODULE_IMAP_BODY unexpectedly retained networking"
	#endif

	return xrtImapBodyParse(
		XRT_STR_LITERAL(
			"(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1)"
		),
		&Body
	) && (Body.Kind == XIMAP_BODY_TEXT) ? 0 : 1;
}
