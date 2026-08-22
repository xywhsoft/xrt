#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_SMTP_CLIENT
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* SMTP 明文客户端单头不应携带 TLS 闭包。 */
int main(void)
{
	xsmtpclientconfig Config;

	#if !defined(XMAIL_FEATURE_SMTP_CLIENT) || \
		!defined(XMAIL_FEATURE_SMTP) || \
		!defined(XMAIL_FEATURE_MAIL_NET)
		#error "XMAIL_MODULE_SMTP_CLIENT dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_SMTP_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
		#error "XMAIL_MODULE_SMTP_CLIENT unexpectedly retained TLS"
	#endif

	xrtSmtpClientConfigInit(&Config);
	return (Config.Net.Port == 25u) &&
		(Config.Net.Security == XMAIL_SECURITY_PLAIN) ? 0 : 1;
}
