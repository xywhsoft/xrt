#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_SMTP_SUBMIT
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* SMTP 提交单头必须只携带会话、Compose 及其公共依赖。 */
int main(void)
{
	xmailmessage Message;

	#if !defined(XMAIL_FEATURE_SMTP_SUBMIT) || \
		!defined(XMAIL_FEATURE_SMTP_CLIENT) || \
		!defined(XMAIL_FEATURE_MAIL_COMPOSE)
		#error "XMAIL_MODULE_SMTP_SUBMIT dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_SMTP_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_SMTP_AUTH) || \
		defined(XMAIL_FEATURE_POP3_CLIENT) || \
		defined(XMAIL_FEATURE_IMAP_CLIENT)
		#error "XMAIL_MODULE_SMTP_SUBMIT retained unrelated transport features"
	#endif

	xrtMailMessageInit(&Message);
	return xrtSmtpSubmit(NULL, &Message, 0, NULL) ? 1 : 0;
}
