#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_WIRE
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 收集单头增量 dot writer 输出。 */
static bool testSingleMailWireWrite(xbytesview Data, ptr pUserData)
{
	size_t* pSize = (size_t*)pUserData;

	*pSize += Data.Size;
	return true;
}



/* 邮件线路单头只能依赖最小邮件核心。 */
int main(void)
{
	xmaildotwriter Writer;
	unsigned char arrOutput[16];
	size_t iSize;
	size_t iStreamSize = 0;

	#if !defined(XMAIL_FEATURE_MAIL_WIRE) || !defined(XMAIL_FEATURE_MAIL_CORE)
		#error "XMAIL_MODULE_MAIL_WIRE dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_STRING) || defined(XMAIL_FEATURE_MAIL_CODEC) || \
		defined(XMAIL_FEATURE_MAIL_HEADER)
		#error "mail wire unexpectedly enabled unrelated features"
	#endif

	return xrtMailDotWrite(
		XRT_STR_LITERAL(".x"),
		true,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == 8u) && (memcmp(arrOutput, "..x\r\n.\r\n", 8u) == 0) &&
		xrtMailDotWriterInit(&Writer) && xrtMailDotWriterWrite(
			&Writer,
			XRT_BYTES_LITERAL(".x"),
			testSingleMailWireWrite,
			&iStreamSize
		) && xrtMailDotWriterFinish(
			&Writer,
			testSingleMailWireWrite,
			&iStreamSize
		) && (iStreamSize == 8u) ?
		0 : 1;
}
