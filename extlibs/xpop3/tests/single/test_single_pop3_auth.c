#ifdef XPOP3_MODULE_XPOP3
	#undef XPOP3_MODULE_XPOP3
#endif
#define XPOP3_MODULE_POP3_AUTH
#define XPOP3_IMPLEMENTATION
#include "../../single/xpop3.h"



/* POP3 认证应携带 SASL Base64，但不隐式增加 TLS 闭包。 */
int main(void)
{
	#if !defined(XPOP3_FEATURE_POP3_AUTH) || \
		!defined(XPOP3_FEATURE_POP3_CLIENT) || \
		!defined(XRT_FEATURE_CODEC_BASE64)
		#error "XPOP3_MODULE_POP3_AUTH dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
		#error "XPOP3_MODULE_POP3_AUTH retained an unrelated heavy closure"
	#endif

	return 0;
}
