#ifdef XPOP3_MODULE_XPOP3
	#undef XPOP3_MODULE_XPOP3
#endif
#define XPOP3_MODULE_XPOP3
#define XPOP3_IMPLEMENTATION
#include "../../single/xpop3.h"



/* xpop3 产品伞单头应点亮自身全部协议与基座闭包。 */
int main(void)
{
	xpop3replyview Reply;

	#if (!defined(XPOP3_FEATURE_POP3) || \
	!defined(XPOP3_FEATURE_POP3_CLIENT) || \
	!defined(XPOP3_FEATURE_POP3_CLIENT_TLS) || \
	!defined(XPOP3_FEATURE_POP3_AUTH) || \
	!defined(XPOP3_FEATURE_POP3_MESSAGE) || \
	!defined(XMAIL_FEATURE_MAIL_WIRE) || \
	!defined(XMAIL_FEATURE_MAIL_NET) || \
	!defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
	!defined(XMAIL_FEATURE_MAIL_TREE))
		#error "XPOP3_MODULE_XPOP3 dependency closure is incomplete"
	#endif

	return xrtPop3ReplyParse(
		XRT_STR_LITERAL("+OK mailbox ready"),
		&Reply
	) ? 0 : 1;
}
