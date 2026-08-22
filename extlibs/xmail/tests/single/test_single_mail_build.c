#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_BUILD
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



typedef struct singlemailsink {
	char Data[128];
	size_t Size;
} singlemailsink;



/* 收集单头构建结果。 */
static bool singleMailWrite(xbytesview Data, ptr pUserData)
{
	singlemailsink* pSink = (singlemailsink*)pUserData;

	if ( Data.Size > (sizeof(pSink->Data) - pSink->Size - 1u) ) {
		return false;
	}
	memcpy(pSink->Data + pSink->Size, Data.Data, Data.Size);
	pSink->Size += Data.Size;
	pSink->Data[pSink->Size] = 0;
	return true;
}



/* 构建模块必须闭包地址、字段和 multipart，但不拉入消息解析器。 */
int main(void)
{
	singlemailsink Sink = { 0 };
	xmailbuilder Builder;

	#if !defined(XMAIL_FEATURE_MAIL_BUILD) || \
		!defined(XMAIL_FEATURE_MAIL_ADDRESS) || \
		!defined(XMAIL_FEATURE_MAIL_HEADER) || \
		!defined(XMAIL_FEATURE_MAIL_MULTIPART)
		#error "XMAIL_MODULE_MAIL_BUILD dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_MESSAGE) || defined(XMAIL_FEATURE_MAIL_WIRE)
		#error "mail build unexpectedly enabled parser or transport modules"
	#endif

	if ( !xrtMailBuilderInit(&Builder, singleMailWrite, &Sink) ||
		 !xrtMailBuilderHeader(
			&Builder,
			XRT_STR_LITERAL("Subject"),
			XRT_STR_LITERAL("single"),
			0
		 ) || !xrtMailBuilderHeadersEnd(&Builder) ||
		 !xrtMailBuilderBody(&Builder, "body", 4u) ||
		 !xrtMailBuilderFinish(&Builder) ) {
		return 1;
	}
	return strcmp(Sink.Data, "Subject: single\r\n\r\nbody") == 0 ? 0 : 2;
}
