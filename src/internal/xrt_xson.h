#ifndef XRT_INTERNAL_XSON_H
#define XRT_INTERNAL_XSON_H

#include "xrt_text_value.h"



#if defined(XRT_FEATURE_XSON_CORE)

/* 设置 XSON 模块错误；位置为空时不写文本定位数据。 */
void __xrtXsonError(
	xerrkind Kind,
	xxsonerror Code,
	cstr sOperation,
	cstr sMessage,
	const xxsonlocation* pLocation
);

#endif



#if defined(XRT_FEATURE_XSON_READ)

/* 验证读取配置已初始化且所有保留字段为零。 */
bool __xrtXsonReadConfigValid(const xxsonreadconfig* pConfig);

#endif



#if defined(XRT_FEATURE_XSON_WRITE)

/* 验证写出配置已初始化且所有保留字段为零。 */
bool __xrtXsonWriteConfigValid(const xxsonwriteconfig* pConfig);

#endif

#endif
