#ifdef XSMTP_MODULE_XSMTP
	#undef XSMTP_MODULE_XSMTP
#endif
#define XSMTP_MODULE_SMTP_CLIENT
#define XSMTP_IMPLEMENTATION
#include "../../single/xsmtp.h"



/* SMTP 明文客户端单头不应携带 TLS 闭包。 */
int main(void)
{
	xsmtpclientconfig Config;

	#if !defined(XSMTP_FEATURE_SMTP_CLIENT) || \
		!defined(XSMTP_FEATURE_SMTP) || \
		!defined(XMAIL_FEATURE_MAIL_NET)
		#error "XSMTP_MODULE_SMTP_CLIENT dependency closure is incomplete"
	#endif
	#if defined(XSMTP_FEATURE_SMTP_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
		#error "XSMTP_MODULE_SMTP_CLIENT unexpectedly retained TLS"
	#endif

	xrtSmtpClientConfigInit(&Config);
	return (Config.Net.Port == 25u) &&
		(Config.Net.Security == XMAIL_SECURITY_PLAIN) ? 0 : 1;
}
