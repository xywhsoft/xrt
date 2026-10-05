#ifdef XIMAP_MODULE_XIMAP
	#undef XIMAP_MODULE_XIMAP
#endif
#define XIMAP_MODULE_IMAP_DATA
#define XIMAP_IMPLEMENTATION
#include "../../include/ximap/features.h"
#include "../../../xmail/include/xmail/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#define XMAIL_IMPLEMENTATION
#include "../../../../single/extlibs/xmail.h"
#include "../../../../single/extlibs/ximap.h"



/* IMAP 数据层单头只保留协议原语，不引入客户端或网络。 */
int main(void)
{
	ximapdatacursor Cursor;
	ximapdataview Value;

	#if !defined(XIMAP_FEATURE_IMAP_DATA) || \
		!defined(XIMAP_FEATURE_IMAP)
		#error "XIMAP_MODULE_IMAP_DATA dependency closure is incomplete"
	#endif
	#if defined(XIMAP_FEATURE_IMAP_CLIENT) || \
		defined(XMAIL_FEATURE_MAIL_NET) || \
		defined(XRT_FEATURE_NET)
		#error "XIMAP_MODULE_IMAP_DATA unexpectedly retained networking"
	#endif

	return xrtImapDataCursorInit(
		&Cursor,
		XRT_STR_LITERAL("42")
	) && (xrtImapDataNext(&Cursor, &Value) == XMAIL_NEXT_ITEM) &&
		(Value.Kind == XIMAP_DATA_NUMBER) && (Value.Number == 42u) ? 0 : 1;
}
