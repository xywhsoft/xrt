#ifdef XSMTP_MODULE_XSMTP
	#undef XSMTP_MODULE_XSMTP
#endif
#define XSMTP_MODULE_SMTP_AUTH
#define XSMTP_IMPLEMENTATION
#include "../../single/xsmtp.h"



/* SMTP 认证单头只增加 Base64，不强制带入 TLS。 */
int main(void)
{
	xsmtpauthconfig Config;

	#if !defined(XSMTP_FEATURE_SMTP_AUTH) || \
		!defined(XSMTP_FEATURE_SMTP_CLIENT) || \
		!defined(XRT_FEATURE_CODEC_BASE64)
		#error "XSMTP_MODULE_SMTP_AUTH dependency closure is incomplete"
	#endif
	#if defined(XSMTP_FEATURE_SMTP_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
		#error "XSMTP_MODULE_SMTP_AUTH unexpectedly retained TLS"
	#endif

	xrtSmtpAuthConfigInit(&Config);
	return (Config.Method == XSMTP_AUTH_PLAIN) &&
		Config.InitialResponse && !Config.AllowPlaintext ? 0 : 1;
}
