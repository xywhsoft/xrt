#ifndef XRT_INTERNAL_PEM_H
#define XRT_INTERNAL_PEM_H

#include "xrt_internal.h"

#include <xrt/pem.h>



#if defined(XRT_FEATURE_PEM)

/* 设置带文本偏移和可选原因链的 PEM 结构化错误。 */
void __xrtPemError(
	xerrkind Kind,
	xpemerror Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset,
	const xerror* pCause
);

#endif

#endif
