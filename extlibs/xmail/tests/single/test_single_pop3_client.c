#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_POP3_CLIENT
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* POP3 明文客户端单头不应携带 TLS。 */
int main(void)
{
	xpop3clientconfig Config;

	#if !defined(XMAIL_FEATURE_POP3_CLIENT) || \
		!defined(XMAIL_FEATURE_POP3) || \
		!defined(XMAIL_FEATURE_MAIL_NET)
		#error "XMAIL_MODULE_POP3_CLIENT dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_POP3_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
		#error "XMAIL_MODULE_POP3_CLIENT unexpectedly retained TLS"
	#endif

	xrtPop3ClientConfigInit(&Config);
	return (Config.Net.Port == 110u) && Config.ReadCapabilities ? 0 : 1;
}
