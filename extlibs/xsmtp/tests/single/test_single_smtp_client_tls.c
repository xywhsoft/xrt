#ifdef XSMTP_MODULE_XSMTP
	#undef XSMTP_MODULE_XSMTP
#endif
#define XSMTP_MODULE_SMTP_CLIENT_TLS
#define XSMTP_IMPLEMENTATION
#include "../../single/xsmtp.h"



/* SMTP TLS 客户端单头应保留 STARTTLS 与验证后的 TLS 拨号闭包。 */
int main(void)
{
	xsmtpclientconfig Config;

	#if !defined(XSMTP_FEATURE_SMTP_CLIENT_TLS) || \
		!defined(XSMTP_FEATURE_SMTP_CLIENT) || \
		!defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		!defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE) || \
		!defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
		#error "XSMTP_MODULE_SMTP_CLIENT_TLS dependency closure is incomplete"
	#endif

	xrtSmtpClientConfigInit(&Config);
	Config.Net.Security = XMAIL_SECURITY_STARTTLS;
	return Config.Net.Security == XMAIL_SECURITY_STARTTLS ? 0 : 1;
}
