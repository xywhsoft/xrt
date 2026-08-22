#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_NET
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 基础邮件网络单头只带入明文 TCP 拨号闭包。 */
int main(void)
{
	xmailnetconfig Config;

	#if !defined(XMAIL_FEATURE_MAIL_NET) || \
		!defined(XRT_FEATURE_NET_TCP_DIAL_SYNC)
		#error "XMAIL_MODULE_MAIL_NET dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE) || \
		defined(XRT_FEATURE_TLS_STREAM_FUTURE) || \
		defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
		#error "XMAIL_MODULE_MAIL_NET unexpectedly retained TLS"
	#endif

	xrtMailNetConfigInit(&Config);
	return (Config.Security == XMAIL_SECURITY_PLAIN) &&
		(Config.LineLimit == XMAIL_WIRE_LINE_DEFAULT) &&
		(Config.ReadChunk == XMAIL_NET_READ_CHUNK_DEFAULT) &&
		(Config.WriteChunk == XMAIL_NET_WRITE_CHUNK_DEFAULT) ? 0 : 1;
}
