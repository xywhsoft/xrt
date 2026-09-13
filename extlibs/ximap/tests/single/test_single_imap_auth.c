#ifdef XIMAP_MODULE_XIMAP
	#undef XIMAP_MODULE_XIMAP
#endif
#define XIMAP_MODULE_IMAP_AUTH
#define XIMAP_IMPLEMENTATION
#include "../../single/ximap.h"



/* IMAP 认证单头应带入客户端与 Base64，但不隐式带入 TLS。 */
int main(void)
{
	ximapauthconfig Config;

	#if !defined(XIMAP_FEATURE_IMAP_AUTH) || \
		!defined(XIMAP_FEATURE_IMAP_CLIENT) || \
		!defined(XRT_FEATURE_CODEC_BASE64)
		#error "XIMAP_MODULE_IMAP_AUTH dependency closure is incomplete"
	#endif
	#if defined(XIMAP_FEATURE_IMAP_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_MAIL_NET_TLS)
		#error "XIMAP_MODULE_IMAP_AUTH unexpectedly retained TLS"
	#endif

	xrtImapAuthConfigInit(&Config);
	return (Config.Method == XIMAP_AUTH_PLAIN) &&
		Config.InitialResponse && !Config.AllowPlaintext ? 0 : 1;
}
