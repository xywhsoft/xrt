#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_STORE_SYSTEM)

/* 构造系统信任库错误，并保留平台码与底层原因链。 */
void __xrtX509StoreSystemError(
	xerrkind Kind,
	int32 iSystemCode,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.x509";
	Desc.Code = X509_ERROR_TRUST_STORE_SYSTEM;
	Desc.SystemCode = iSystemCode;
	Desc.Operation = "x509-store-add-system";
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 使用当前错误的类别和原因链构造系统信任库错误。 */
void __xrtX509StoreSystemFailure(cstr sMessage)
{
	const xerror* pCause = xrtGetError();
	xerrkind Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_IO;

	__xrtX509StoreSystemError(Kind, 0, sMessage, pCause);
}



/* 区分可跳过的存量证书兼容问题与必须终止导入的基础设施失败。 */
bool __xrtX509StoreSystemCanSkip(const xerror* pError)
{
	while ( pError != NULL ) {
		cstr sDomain = xrtErrorDomain(pError);
		int32 iCode = xrtErrorCode(pError);
		xerrkind Kind = xrtErrorKind(pError);

		if ( (sDomain != NULL) && (strcmp(sDomain, "xrt.x509") == 0) &&
			(iCode != X509_ERROR_TRUST_STORE) &&
			(iCode != X509_ERROR_TRUST_STORE_SYSTEM) ) {
			return (Kind == XERR_PROTOCOL) || (Kind == XERR_UNSUPPORTED);
		}
		pError = xrtErrorCause(pError);
	}
	return false;
}



/* 原子导入当前平台可见的系统信任锚。 */
XRT_API bool xrtX509StoreAddSystem(xx509store* pStore, size_t* pAdded)
{
	size_t iBefore;
	size_t iAfter;

	if ( pStore == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iBefore = xrtX509StoreCount(pStore);
	if ( !__xrtX509StoreSystemLoad(pStore) ) {
		__xrtX509StoreTruncate(pStore, iBefore);
		return false;
	}
	iAfter = xrtX509StoreCount(pStore);
	if ( pAdded != NULL ) {
		*pAdded = iAfter - iBefore;
	}
	return true;
}



/* 创建一份由调用方独占并可继续追加证书的系统信任库快照。 */
XRT_API xx509store* xrtX509StoreSystem(void)
{
	xx509store* pStore = xrtX509StoreCreate();

	if ( pStore == NULL ) {
		return NULL;
	}
	if ( !xrtX509StoreAddSystem(pStore, NULL) ) {
		xrtX509StoreFree(pStore);
		return NULL;
	}
	return pStore;
}

#endif
