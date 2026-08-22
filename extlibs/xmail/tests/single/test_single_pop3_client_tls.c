#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_POP3_CLIENT_TLS
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* POP3 TLS 客户端单头应保留 STLS 和验证后的 TLS 拨号闭包。 */
int main(void)
{
	xpop3clientconfig Config;

	#if !defined(XMAIL_FEATURE_POP3_CLIENT_TLS) || \
		!defined(XMAIL_FEATURE_POP3_CLIENT) || \
		!defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		!defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE) || \
		!defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
		#error "XMAIL_MODULE_POP3_CLIENT_TLS dependency closure is incomplete"
	#endif

	xrtPop3ClientConfigInit(&Config);
	Config.Net.Security = XMAIL_SECURITY_STARTTLS;
	return Config.Net.Security == XMAIL_SECURITY_STARTTLS ? 0 : 1;
}
