#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_TREE
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* MIME 树裁剪入口只拉入解析所需的内容层闭包。 */
int main(void)
{
	static const char sMessage[] =
		"Content-Type: text/plain\r\n\r\nbody";
	xmailtree Tree;

	#if !defined(XMAIL_FEATURE_MAIL_TREE) || \
		!defined(XMAIL_FEATURE_MAIL_MESSAGE) || \
		!defined(XMAIL_FEATURE_MAIL_MULTIPART) || \
		!defined(XMAIL_FEATURE_MAIL_PARAM)
		#error "XMAIL_MODULE_MAIL_TREE dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_BUILD) || defined(XMAIL_FEATURE_SMTP)
		#error "mail tree unexpectedly enabled builders or transports"
	#endif

	if ( !xrtMailTreeParse(XRT_STR_LITERAL(sMessage), NULL, &Tree) ) {
		return 1;
	}
	if ( (Tree.Root == NULL) || (Tree.Root->Data.Size != 4u) ||
		 (memcmp(Tree.Root->Data.Data, "body", 4u) != 0) ) {
		xrtMailTreeFree(&Tree);
		return 2;
	}
	xrtMailTreeFree(&Tree);
	return 0;
}
