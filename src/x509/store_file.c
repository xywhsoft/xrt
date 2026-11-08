#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_STORE_FILE)

/* 文件装载层包装底层文件、PEM 和证书错误，同时保留完整原因链。 */
static void __xrtX509StoreFileError(
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
)
{
	__xrtX509Error(
		Kind, X509_ERROR_TRUST_STORE_FILE, "x509-store-add-file",
		sMessage, SIZE_MAX, pCause
	);
}



/* 读取并原子导入一个 DER 证书文件或包含多张证书的 PEM 文件。 */
XRT_API bool xrtX509StoreAddFile(
	xx509store* pStore,
	cstr sPath,
	size_t* pAdded
)
{
	bytes pData;
	size_t iSize;
	size_t iAdded = 0;
	bool bSuccess;

	if ( (pStore == NULL) || (sPath == NULL) || (sPath[0] == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pData = xrtFileReadAll(sPath, &iSize);
	if ( pData == NULL ) {
		const xerror* pCause = xrtGetError();
		xerrkind Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_IO;

		__xrtX509StoreFileError(
			Kind, "trust store file reading failed", pCause
		);
		return false;
	}
	if ( iSize == 0 ) {
		xrtFree(pData);
		__xrtX509StoreFileError(
			XERR_PROTOCOL, "trust store file is empty", NULL
		);
		return false;
	}
	bSuccess = xrtX509StoreAddData(pStore, (xbytesview){pData, iSize}, &iAdded);
	xrtFree(pData);
	if ( !bSuccess ) {
		const xerror* pCause = xrtGetError();
		xerrkind Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_PROTOCOL;

		__xrtX509StoreFileError(
			Kind, "trust store file import failed", pCause
		);
		return false;
	}
	if ( pAdded != NULL ) {
		*pAdded = iAdded;
	}
	return true;
}

#endif
