#ifdef XIMAP_MODULE_XIMAP
	#undef XIMAP_MODULE_XIMAP
#endif
#define XIMAP_MODULE_IMAP
#define XIMAP_IMPLEMENTATION
#include "../../single/ximap.h"



/* IMAP 协议单头只依赖公共邮件线路原语。 */
int main(void)
{
	ximapresponseview Response;
	char arrCommand[32];
	size_t iSize;

	#if !defined(XIMAP_FEATURE_IMAP) || !defined(XMAIL_FEATURE_MAIL_WIRE)
		#error "XIMAP_MODULE_IMAP dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NET) || defined(XRT_FEATURE_TLS_STREAM) || \
		defined(XMAIL_FEATURE_MAIL_MESSAGE)
		#error "IMAP protocol unexpectedly enabled client dependencies"
	#endif

	return xrtImapResponseParse(
		XRT_STR_LITERAL("A1 OK done"),
		&Response
	) && xrtImapCommandWrite(
		XRT_STR_LITERAL("A2"),
		XRT_STR_LITERAL("NOOP"),
		XRT_STR_LITERAL(""),
		0,
		arrCommand,
		sizeof(arrCommand),
		&iSize
	) && (Response.Status == XIMAP_STATUS_OK) && (iSize == 9u) ? 0 : 1;
}
