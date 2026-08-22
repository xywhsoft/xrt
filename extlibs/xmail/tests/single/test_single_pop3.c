#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_POP3
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* POP3 协议单头只依赖公共邮件线路原语。 */
int main(void)
{
	xpop3stat Stat;
	char arrCommand[16];
	size_t iSize;

	#if !defined(XMAIL_FEATURE_POP3) || !defined(XMAIL_FEATURE_MAIL_WIRE)
		#error "XMAIL_MODULE_POP3 dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NET) || defined(XRT_FEATURE_TLS_STREAM) || \
		defined(XMAIL_FEATURE_MAIL_MESSAGE)
		#error "POP3 protocol unexpectedly enabled client dependencies"
	#endif

	return xrtPop3StatParse(XRT_STR_LITERAL("+OK 2 128"), &Stat) &&
		xrtPop3CommandWrite(
			XRT_STR_LITERAL("QUIT"),
			XRT_STR_LITERAL(""),
			arrCommand,
			sizeof(arrCommand),
			&iSize
		) && (Stat.Messages == 2u) && (iSize == 6u) ? 0 : 1;
}
