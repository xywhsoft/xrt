#include "../internal/xrt_text_value.h"



#if defined(XRT_FEATURE_JSON_FILE) || defined(XRT_FEATURE_XSON_FILE)

/* 把底层文件错误包装到具体文本协议，并保留完整原因链。 */
static void __xrtTextValueFileError(
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_IO;
	Desc.Code = iCode;
	Desc.Domain = sDomain;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
	xrtErrorFree(pCause);
}



/* 限额读取完整协议文件。 */
bytes __xrtTextValueFileReadAll(
	cstr sPath,
	size_t iLimit,
	size_t* pSize,
	cstr sDomain,
	int32 iCode,
	cstr sMessage
)
{
	bytes pData = xrtFileReadAllLimit(sPath, iLimit, pSize);

	if ( pData == NULL ) {
		__xrtTextValueFileError(
			sDomain,
			iCode,
			"read_file",
			sMessage
		);
	}
	return pData;
}



/* 原子替换完整协议文件。 */
bool __xrtTextValueFileWriteAll(
	cstr sPath,
	xbytesview Data,
	cstr sDomain,
	int32 iCode,
	cstr sMessage
)
{
	if ( xrtFileWriteAtomic(sPath, Data) ) {
		return true;
	}
	__xrtTextValueFileError(
		sDomain,
		iCode,
		"write_file",
		sMessage
	);
	return false;
}

#endif
