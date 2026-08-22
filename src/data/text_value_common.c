#include "../internal/xrt_text_value.h"

#include <stdio.h>



#if defined(XRT_FEATURE_JSON_CORE) || defined(XRT_FEATURE_XSON_CORE)

/* 建立稳定错误域、代码和可选文本位置。 */
void __xrtTextValueError(
	xerrkind Kind,
	int32 iCode,
	cstr sDomain,
	cstr sOperation,
	cstr sMessage,
	bool bLocation,
	size_t iOffset,
	size_t iLine,
	size_t iColumn
)
{
	char Data[128];
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = iCode;
	Desc.Domain = sDomain;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	if ( bLocation ) {
		(void)snprintf(
			Data,
			sizeof(Data),
			"offset=%llu;line=%llu;column=%llu",
			(unsigned long long)iOffset,
			(unsigned long long)iLine,
			(unsigned long long)iColumn
		);
		Desc.Data = Data;
	}
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 严格读取指定格式错误域中的文本位置机器数据。 */
bool __xrtTextValueErrorLocation(
	const xerror* pError,
	cstr sDomain,
	size_t* pOffset,
	size_t* pLine,
	size_t* pColumn
)
{
	cstr sData;
	unsigned long long iOffset;
	unsigned long long iLine;
	unsigned long long iColumn;

	if (
		(pError == NULL) || (sDomain == NULL) ||
		(pOffset == NULL) || (pLine == NULL) || (pColumn == NULL)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		(xrtErrorDomain(pError) == NULL) ||
		(strcmp(xrtErrorDomain(pError), sDomain) != 0)
	) {
		return false;
	}
	sData = xrtErrorData(pError);
	if (
		(sData == NULL) ||
		(sscanf(
			sData,
			"offset=%llu;line=%llu;column=%llu",
			&iOffset,
			&iLine,
			&iColumn
		) != 3)
	) {
		return false;
	}
	if (
		(iOffset > (unsigned long long)SIZE_MAX) ||
		(iLine > (unsigned long long)SIZE_MAX) ||
		(iColumn > (unsigned long long)SIZE_MAX)
	) {
		return false;
	}
	*pOffset = (size_t)iOffset;
	*pLine = (size_t)iLine;
	*pColumn = (size_t)iColumn;
	return true;
}

#endif
