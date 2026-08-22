#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_POP3_AUTH
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* POP3 认证应携带 SASL Base64，但不隐式增加 TLS 闭包。 */
int main(void)
{
	#if !defined(XMAIL_FEATURE_POP3_AUTH) || \
		!defined(XMAIL_FEATURE_POP3_CLIENT) || \
		!defined(XRT_FEATURE_CODEC_BASE64)
		#error "XMAIL_MODULE_POP3_AUTH dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
		#error "XMAIL_MODULE_POP3_AUTH retained an unrelated heavy closure"
	#endif

	return 0;
}
