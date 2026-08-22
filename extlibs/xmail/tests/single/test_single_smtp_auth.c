#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_SMTP_AUTH
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* SMTP 认证单头只增加 Base64，不强制带入 TLS。 */
int main(void)
{
	xsmtpauthconfig Config;

	#if !defined(XMAIL_FEATURE_SMTP_AUTH) || \
		!defined(XMAIL_FEATURE_SMTP_CLIENT) || \
		!defined(XRT_FEATURE_CODEC_BASE64)
		#error "XMAIL_MODULE_SMTP_AUTH dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_SMTP_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
		#error "XMAIL_MODULE_SMTP_AUTH unexpectedly retained TLS"
	#endif

	xrtSmtpAuthConfigInit(&Config);
	return (Config.Method == XSMTP_AUTH_PLAIN) &&
		Config.InitialResponse && !Config.AllowPlaintext ? 0 : 1;
}
