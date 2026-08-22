#include <stdlib.h>
#include <string.h>

#include <xrt/http_route.h>



#define XRT_HTTP_ROUTE_FUZZ_INPUT_MAX ((size_t)1048576u)
#define XRT_HTTP_ROUTE_FUZZ_PARAMS_MAX ((size_t)16u)



/* 判断借用视图是否完整落在指定输入中。 */
static bool __xrtHttpRouteFuzzViewInside(
	xstrview Input,
	xstrview View
)
{
	uintptr_t iInput;
	uintptr_t iView;

	if ( View.Data == NULL ) {
		return View.Size == 0;
	}
	if ( Input.Data == NULL ) {
		return false;
	}
	iInput = (uintptr_t)Input.Data;
	iView = (uintptr_t)View.Data;
	return (iView >= iInput) &&
		((iView - iInput) <= Input.Size) &&
		(View.Size <= (Input.Size - (iView - iInput)));
}



/* 验证成功捕获全部借用正确输入，且名称查找返回同一个条目。 */
static void __xrtHttpRouteFuzzParams(
	xstrview Pattern,
	xstrview Path,
	const xhttprouteparam* pParams,
	size_t iCount
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpRouteFuzzViewInside(
			Pattern, pParams[i].Name
		) || (pParams[i].Name.Size == 0) ||
			!__xrtHttpRouteFuzzViewInside(
				Path, pParams[i].Value
			) || (xrtHttpRouteParam(
				pParams, iCount, pParams[i].Name
			 ) != &pParams[i]) ) {
			abort();
		}
	}
}



/* 检查一个任意模板与路径对的容量查询、实际写入和错误终态。 */
static void __xrtHttpRouteFuzzPair(
	xstrview Pattern,
	xstrview Path
)
{
	xhttprouteparam Params[XRT_HTTP_ROUTE_FUZZ_PARAMS_MAX];
	xhttprouteparam Before[XRT_HTTP_ROUTE_FUZZ_PARAMS_MAX];
	xhttproutestatus Query;
	xhttproutestatus Repeat;
	size_t iValidated = 0;
	size_t iRequired = 0;
	size_t iRepeat = 0;
	bool bValid;

	bValid = xrtHttpRouteValidate(Pattern, &iValidated);
	xrtClearError();
	Query = xrtHttpRouteMatch(
		Pattern, Path, NULL, 0, &iRequired
	);
	Repeat = xrtHttpRouteMatch(
		Pattern, Path, NULL, 0, &iRepeat
	);
	if ( (Query != Repeat) || (iRequired != iRepeat) ) {
		abort();
	}
	if ( !bValid ) {
		if ( Query != XHTTP_ROUTE_ERROR ) {
			abort();
		}
		xrtClearError();
		return;
	}
	if ( Query == XHTTP_ROUTE_ERROR ) {
		xrtClearError();
		return;
	}
	if ( Query == XHTTP_ROUTE_MISS ) {
		if ( iRequired != 0 ) {
			abort();
		}
		return;
	}
	if ( ((Query == XHTTP_ROUTE_MATCH) &&
		(iRequired != 0)) ||
		((Query == XHTTP_ROUTE_MORE) &&
		 ((iRequired == 0) || (iRequired != iValidated))) ) {
		abort();
	}
	if ( iRequired > XRT_HTTP_ROUTE_FUZZ_PARAMS_MAX ) {
		return;
	}
	memset(Params, 0xA5, sizeof(Params));
	if ( xrtHttpRouteMatch(
		Pattern, Path, Params,
		XRT_HTTP_ROUTE_FUZZ_PARAMS_MAX, &iRepeat
	) != XHTTP_ROUTE_MATCH || (iRepeat != iRequired) ) {
		abort();
	}
	__xrtHttpRouteFuzzParams(
		Pattern, Path, Params, iRequired
	);
	if ( iRequired != 0 ) {
		memset(Params, 0x5A, sizeof(Params));
		memcpy(Before, Params, sizeof(Before));
		if ( xrtHttpRouteMatch(
			Pattern, Path, Params, iRequired - 1u, &iRepeat
		) != XHTTP_ROUTE_MORE || (iRepeat != iRequired) ||
			(memcmp(Params, Before, sizeof(Params)) != 0) ) {
			abort();
		}
	}
	xrtClearError();
}



/* 统一公开确定性回归和 libFuzzer 使用的本地路由协议入口。 */
int xrtHttpRouteFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	size_t iPayload;
	size_t iSplit;
	xstrview Pattern;
	xstrview Path;

	if ( ((pData == NULL) && (iSize != 0)) ||
		(iSize > XRT_HTTP_ROUTE_FUZZ_INPUT_MAX) ||
		(iSize < 2u) ) {
		return 0;
	}
	iPayload = iSize - 2u;
	iSplit = ((((size_t)pData[0] << 8u) |
		(size_t)pData[1]) % (iPayload + 1u));
	Pattern.Data = (cstr)(pData + 2u);
	Pattern.Size = iSplit;
	Path.Data = (cstr)(pData + 2u + iSplit);
	Path.Size = iPayload - iSplit;
	__xrtHttpRouteFuzzPair(Pattern, Path);
	return 0;
}



#if defined(XRT_HTTP_ROUTE_FUZZ_LIBFUZZER)

/* 把独立路由入口适配为 Clang/libFuzzer 约定符号。 */
int LLVMFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	return xrtHttpRouteFuzzerTestOneInput(pData, iSize);
}

#endif
