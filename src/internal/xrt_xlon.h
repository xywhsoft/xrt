#ifndef XRT_INTERNAL_XLON_H
#define XRT_INTERNAL_XLON_H

#include "xrt_text_value.h"



#if defined(XRT_FEATURE_XLON_CORE)

/* 设置 XLON 模块错误；位置为空时不写文本定位数据。 */
void __xrtXlonError(
	xerrkind Kind,
	xxlonerror Code,
	cstr sOperation,
	cstr sMessage,
	const xxlonlocation* pLocation
);

#endif



#if defined(XRT_FEATURE_XLON_READ)

/* 验证读取配置已初始化且所有保留字段为零。 */
bool __xrtXlonReadConfigValid(const xxlonreadconfig* pConfig);

/* 逐行适配器共享 DOM/验证路径；验证成功返回可释放的 null 单例。 */
xvalue* __xrtXlonReadBudget(
	xstrview Text,
	const xxlonreadconfig* pConfig,
	xtextvaluebudget* pBudget,
	bool bValidate
);

#endif



#if defined(XRT_FEATURE_XLON_WRITE)

/* 验证写出配置已初始化且所有保留字段为零。 */
bool __xrtXlonWriteConfigValid(const xxlonwriteconfig* pConfig);

#endif

#endif
