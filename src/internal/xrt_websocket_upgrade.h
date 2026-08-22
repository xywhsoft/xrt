#ifndef XRT_INTERNAL_WEBSOCKET_UPGRADE_H
#define XRT_INTERNAL_WEBSOCKET_UPGRADE_H

#include "xrt_websocket.h"

#include <xrt/http_upgrade.h>
#include <xrt/websocket_upgrade.h>



#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE)

/* 从允许未对齐的字段数组复制一个描述符。 */
static inline void __xrtWsUpgradeFieldRead(
	const xhttpfield* pFields,
	size_t iIndex,
	xhttpfield* pField
)
{
	memcpy(
		pField,
		(const uint8*)pFields + (iIndex * sizeof(*pField)),
		sizeof(*pField)
	);
}



/* 按大小写敏感规则比较协议文本。 */
static inline bool __xrtWsUpgradeTextEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE)

/* 服务端从重复扩展字段中协商唯一的 permessage-deflate。 */
bool __xrtWsUpgradeServerDeflate(
	const xhttpfield* pFields,
	size_t iCount,
	const xwsupgradeserverconfig* pConfig,
	xwsupgrade* pUpgrade
);



/* 客户端严格验证服务器选择的扩展。 */
bool __xrtWsUpgradeClientDeflate(
	const xhttpfield* pFields,
	size_t iCount,
	const xwsupgradeclientconfig* pConfig,
	xwsupgrade* pUpgrade
);



/* 验证一个完整的扩展字段值。 */
bool __xrtWsUpgradeExtensionsValid(xstrview Extensions);

#endif

#endif

#endif
