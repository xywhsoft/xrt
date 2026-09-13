#ifdef XIMAP_MODULE_XIMAP
	#undef XIMAP_MODULE_XIMAP
#endif
#define XIMAP_MODULE_IMAP_CLIENT
#define XIMAP_IMPLEMENTATION
#include "../../single/ximap.h"



/* IMAP 明文客户端单头不应携带 TLS。 */
int main(void)
{
	ximapclientconfig Config;

	#if !defined(XIMAP_FEATURE_IMAP_CLIENT) || \
		!defined(XIMAP_FEATURE_IMAP) || \
		!defined(XMAIL_FEATURE_MAIL_NET)
		#error "XIMAP_MODULE_IMAP_CLIENT dependency closure is incomplete"
	#endif
	#if defined(XIMAP_FEATURE_IMAP_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
		defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
		#error "XIMAP_MODULE_IMAP_CLIENT unexpectedly retained TLS"
	#endif

	xrtImapClientConfigInit(&Config);
	return (Config.Net.Port == 143u) &&
		(Config.CommandLineLimit == XIMAP_COMMAND_LINE_DEFAULT) ? 0 : 1;
}
