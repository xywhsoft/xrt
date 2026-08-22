#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_NET_TLS
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* TLS 邮件网络单头必须带齐 Future、验证器和密码闭包。 */
int main(void)
{
	xmailnetconfig Config;

	#if !defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		!defined(XMAIL_FEATURE_MAIL_NET) || \
		!defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE) || \
		!defined(XRT_FEATURE_TLS_STREAM_FUTURE) || \
		!defined(XRT_FEATURE_TLS_CLIENT_VERIFY) || \
		!defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || \
		!defined(XRT_FEATURE_TLS_SCHEDULE_SHA384) || \
		!defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || \
		!defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) || \
		!defined(XRT_FEATURE_TLS_RECORD_AES) || \
		!defined(XRT_FEATURE_TLS_RECORD_CHACHA)
		#error "XMAIL_MODULE_MAIL_NET_TLS dependency closure is incomplete"
	#endif

	xrtMailNetConfigInit(&Config);
	return (Config.Security == XMAIL_SECURITY_PLAIN) &&
		(Config.TlsTimeout == Config.Dial.Timeout) ? 0 : 1;
}
