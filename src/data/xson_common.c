#include "../internal/xrt_xson.h"



#if defined(XRT_FEATURE_XSON_CORE)

/* 设置带稳定域、代码和可选文本位置的 XSON 错误。 */
void __xrtXsonError(
	xerrkind Kind,
	xxsonerror Code,
	cstr sOperation,
	cstr sMessage,
	const xxsonlocation* pLocation
)
{
	__xrtTextValueError(
		Kind,
		(int32)Code,
		"xrt.xson",
		sOperation,
		sMessage,
		pLocation != NULL,
		pLocation != NULL ? pLocation->Offset : 0,
		pLocation != NULL ? pLocation->Line : 0,
		pLocation != NULL ? pLocation->Column : 0
	);
}



/* 从 XSON 错误机器数据中读取完整文本位置。 */
XRT_API bool xrtXsonErrorLocation(
	const xerror* pError,
	xxsonlocation* pLocation
)
{
	if ( pLocation == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueErrorLocation(
		pError,
		"xrt.xson",
		&pLocation->Offset,
		&pLocation->Line,
		&pLocation->Column
	);
}

#endif
