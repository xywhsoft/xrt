#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_IMAP_CLIENT
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* IMAP 明文客户端单头不应携带 TLS。 */
int main(void)
{
	ximapclientconfig Config;

	#if !defined(XMAIL_FEATURE_IMAP_CLIENT) || \
		!defined(XMAIL_FEATURE_IMAP) || \
		!defined(XMAIL_FEATURE_MAIL_NET)
		#error "XMAIL_MODULE_IMAP_CLIENT dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_IMAP_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
		#error "XMAIL_MODULE_IMAP_CLIENT unexpectedly retained TLS"
	#endif

	xrtImapClientConfigInit(&Config);
	return (Config.Net.Port == 143u) &&
		(Config.CommandLineLimit == XIMAP_COMMAND_LINE_DEFAULT) ? 0 : 1;
}
