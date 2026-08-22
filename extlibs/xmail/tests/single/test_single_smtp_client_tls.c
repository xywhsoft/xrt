#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_SMTP_CLIENT_TLS
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* SMTP TLS 客户端单头应保留 STARTTLS 与验证后的 TLS 拨号闭包。 */
int main(void)
{
	xsmtpclientconfig Config;

	#if !defined(XMAIL_FEATURE_SMTP_CLIENT_TLS) || \
		!defined(XMAIL_FEATURE_SMTP_CLIENT) || \
		!defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		!defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE) || \
		!defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
		#error "XMAIL_MODULE_SMTP_CLIENT_TLS dependency closure is incomplete"
	#endif

	xrtSmtpClientConfigInit(&Config);
	Config.Net.Security = XMAIL_SECURITY_STARTTLS;
	return Config.Net.Security == XMAIL_SECURITY_STARTTLS ? 0 : 1;
}
