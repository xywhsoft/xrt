#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_HEADER
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 邮件字段裁剪入口必须保留零分配游标。 */
int main(void)
{
	xmailheadercursor Cursor;
	xmailheaderview Header;

	#if !defined(XMAIL_FEATURE_MAIL_HEADER) || !defined(XMAIL_FEATURE_MAIL_CORE)
		#error "XMAIL_MODULE_MAIL_HEADER dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_STRING) || defined(XMAIL_FEATURE_MAIL_CODEC)
		#error "mail header unexpectedly enabled unrelated features"
	#endif

	if ( !xrtMailHeaderCursorInit(
		&Cursor,
		XRT_STR_LITERAL("X-Test: ok\r\n\r\n")
	) ) {
		return 1;
	}
	return xrtMailHeaderNext(&Cursor, &Header) == XMAIL_NEXT_ITEM ? 0 : 2;
}
