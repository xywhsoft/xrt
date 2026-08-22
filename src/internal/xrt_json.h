#ifndef XRT_INTERNAL_JSON_H
#define XRT_INTERNAL_JSON_H

#include "xrt_text_value.h"



#if defined(XRT_FEATURE_JSON_CORE)

/* 设置 JSON 模块错误；位置为空时不写入文本定位数据。 */
void __xrtJsonError(
	xerrkind Kind,
	xjsonerror Code,
	cstr sOperation,
	cstr sMessage,
	const xjsonlocation* pLocation
);

#endif



#if defined(XRT_FEATURE_JSON_READ)

/* 验证读取配置已初始化且字段值完整有效。 */
bool __xrtJsonReadConfigValid(const xjsonreadconfig* pConfig);

#endif

#endif
