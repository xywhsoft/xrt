#include "../internal/xrt_xlon.h"



#if defined(XRT_FEATURE_XLON_CORE)

/* 设置带稳定域、代码和可选文本位置的 XLON 错误。 */
void __xrtXlonError(
	xerrkind Kind,
	xxlonerror Code,
	cstr sOperation,
	cstr sMessage,
	const xxlonlocation* pLocation
)
{
	__xrtTextValueError(
		Kind,
		(int32)Code,
		"xrt.xlon",
		sOperation,
		sMessage,
		pLocation != NULL,
		pLocation != NULL ? pLocation->Offset : 0,
		pLocation != NULL ? pLocation->Line : 0,
		pLocation != NULL ? pLocation->Column : 0
	);
}



/* 从 XLON 错误机器数据中读取完整文本位置。 */
XRT_API bool xrtXlonErrorLocation(
	const xerror* pError,
	xxlonlocation* pLocation
)
{
	if ( pLocation == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueErrorLocation(
		pError,
		"xrt.xlon",
		&pLocation->Offset,
		&pLocation->Line,
		&pLocation->Column
	);
}

#endif
