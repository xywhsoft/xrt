#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_ID
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 标识裁剪入口只闭包安全随机源，不拉入时间或字段解析。 */
int main(void)
{
	char arrBoundary[64];
	size_t iSize = 0;

	#if !defined(XMAIL_FEATURE_MAIL_ID) || !defined(XRT_FEATURE_RANDOM_SECURE) || \
		!defined(XRT_FEATURE_UNICODE)
		#error "XMAIL_MODULE_MAIL_ID dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_DATE) || defined(XMAIL_FEATURE_MAIL_HEADER)
		#error "mail id unexpectedly enabled unrelated mail modules"
	#endif

	return xrtMailBoundaryWrite(
		arrBoundary,
		sizeof(arrBoundary),
		&iSize
	) ? 0 : 1;
}
