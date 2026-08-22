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



/* 有效 DER 证书优先；其余内容按行首 BEGIN 边界识别 PEM 文本。 */
static bool __xrtX509StoreFilePem(const unsigned char* pData, size_t iSize)
{
	static const char Begin[] = "-----BEGIN ";

	if ( (iSize != 0) && (pData[0] == 0x30u) ) {
		return false;
	}
	if ( iSize < (sizeof(Begin) - 1u) ) {
		return false;
	}
	for ( size_t i = 0; i <= iSize - (sizeof(Begin) - 1u); i++ ) {
		if ( ((i == 0) || (pData[i - 1u] == '\n') ||
			(pData[i - 1u] == '\r')) &&
			(memcmp(pData + i, Begin, sizeof(Begin) - 1u) == 0) ) {
			return true;
		}
	}
	return false;
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
	size_t iBefore;
	size_t iAdded = 0;
	bool bSuccess;

	if ( (pStore == NULL) || (sPath == NULL) || (sPath[0] == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iBefore = xrtX509StoreCount(pStore);
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
	if ( __xrtX509StoreFilePem(pData, iSize) ) {
		bSuccess = xrtX509StoreAddPem(
			pStore, (cstr)pData, iSize, &iAdded
		);
	} else {
		xx509result Result = xrtX509StoreAdd(pStore, pData, iSize);

		bSuccess = Result != X509_ERROR;
		iAdded = Result == X509_VALUE ? 1u : 0u;
	}
	xrtFree(pData);
	if ( !bSuccess ) {
		const xerror* pCause = xrtGetError();
		xerrkind Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_PROTOCOL;

		__xrtX509StoreTruncate(pStore, iBefore);
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
