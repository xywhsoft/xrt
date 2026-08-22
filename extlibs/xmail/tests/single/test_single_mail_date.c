#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_DATE
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 日期裁剪入口只闭包时间文本，不拉入随机数或 MIME 参数。 */
int main(void)
{
	size_t iSize = 0;

	#if !defined(XMAIL_FEATURE_MAIL_DATE) || !defined(XRT_FEATURE_TIME_TEXT)
		#error "XMAIL_MODULE_MAIL_DATE dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_ID) || defined(XMAIL_FEATURE_MAIL_PARAM)
		#error "mail date unexpectedly enabled unrelated mail modules"
	#endif

	return xrtMailDateWrite(xrtNow(), 0, NULL, 0, &iSize) &&
		(iSize != 0) ? 0 : 1;
}
