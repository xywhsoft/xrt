#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_IMAP_CLIENT_TLS
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* IMAP TLS 客户端单头只保留基础会话和经过验证的 TLS 传输闭包。 */
int main(void)
{
	ximapclientconfig Config;

	#if !defined(XMAIL_FEATURE_IMAP_CLIENT_TLS) || \
		!defined(XMAIL_FEATURE_IMAP_CLIENT) || \
		!defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		!defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE) || \
		!defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
		#error "XMAIL_MODULE_IMAP_CLIENT_TLS dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_IMAP_AUTH) || \
		defined(XMAIL_FEATURE_IMAP_COMMAND) || \
		defined(XMAIL_FEATURE_IMAP_APPEND) || \
		defined(XMAIL_FEATURE_IMAP_COMPRESS) || \
		defined(XMAIL_FEATURE_SMTP_CLIENT) || \
		defined(XMAIL_FEATURE_POP3_CLIENT)
		#error "XMAIL_MODULE_IMAP_CLIENT_TLS retained unrelated mail features"
	#endif

	xrtImapClientConfigInit(&Config);
	Config.Net.Security = XMAIL_SECURITY_STARTTLS;
	return (Config.Net.Port == 143u) &&
		(Config.Net.Security == XMAIL_SECURITY_STARTTLS) ? 0 : 1;
}
