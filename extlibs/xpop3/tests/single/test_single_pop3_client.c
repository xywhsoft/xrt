#ifdef XPOP3_MODULE_XPOP3
	#undef XPOP3_MODULE_XPOP3
#endif
#define XPOP3_MODULE_POP3_CLIENT
#define XPOP3_IMPLEMENTATION
#include "../../single/xpop3.h"



/* POP3 明文客户端单头不应携带 TLS。 */
int main(void)
{
	xpop3clientconfig Config;

	#if !defined(XPOP3_FEATURE_POP3_CLIENT) || \
		!defined(XPOP3_FEATURE_POP3) || \
		!defined(XMAIL_FEATURE_MAIL_NET)
		#error "XPOP3_MODULE_POP3_CLIENT dependency closure is incomplete"
	#endif
	#if defined(XPOP3_FEATURE_POP3_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
		#error "XPOP3_MODULE_POP3_CLIENT unexpectedly retained TLS"
	#endif

	xrtPop3ClientConfigInit(&Config);
	return (Config.Net.Port == 110u) && Config.ReadCapabilities ? 0 : 1;
}
