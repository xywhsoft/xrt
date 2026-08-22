#include "../internal/xrt_json.h"
#include "../internal/xrt_text_value.h"



#if defined(XRT_FEATURE_JSON_CORE)

/* 设置带稳定域、代码和可选文本位置的 JSON 错误。 */
void __xrtJsonError(
	xerrkind Kind,
	xjsonerror Code,
	cstr sOperation,
	cstr sMessage,
	const xjsonlocation* pLocation
)
{
	__xrtTextValueError(
		Kind,
		(int32)Code,
		"xrt.json",
		sOperation,
		sMessage,
		pLocation != NULL,
		pLocation != NULL ? pLocation->Offset : 0,
		pLocation != NULL ? pLocation->Line : 0,
		pLocation != NULL ? pLocation->Column : 0
	);
}



/* 从 JSON 错误机器数据中读取完整文本位置。 */
XRT_API bool xrtJsonErrorLocation(
	const xerror* pError,
	xjsonlocation* pLocation
)
{
	if ( pLocation == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueErrorLocation(
		pError,
		"xrt.json",
		&pLocation->Offset,
		&pLocation->Line,
		&pLocation->Column
	);
}

#endif
