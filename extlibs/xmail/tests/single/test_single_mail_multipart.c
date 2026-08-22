#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_MULTIPART
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* multipart 裁剪入口闭包 boundary 和字段语法，不拉入内容编码。 */
int main(void)
{
	xmailmultipartcursor Cursor;
	xmailmultipartview Part;

	#if !defined(XMAIL_FEATURE_MAIL_MULTIPART) || \
		!defined(XMAIL_FEATURE_MAIL_HEADER)
		#error "XMAIL_MODULE_MAIL_MULTIPART dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_ID) || defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XRT_FEATURE_STRING) || defined(XMAIL_FEATURE_MAIL_CODEC) || \
		defined(XMAIL_FEATURE_MAIL_PARAM)
		#error "mail multipart unexpectedly enabled unrelated modules"
	#endif

	if ( !xrtMailMultipartCursorInit(
		&Cursor,
		XRT_STR_LITERAL("--b--\r\n"),
		XRT_STR_LITERAL("b"),
		0
	) ) {
		return 1;
	}
	return xrtMailMultipartNext(&Cursor, &Part) == XMAIL_NEXT_END ? 0 : 2;
}
