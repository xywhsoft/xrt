#include "../internal/xrt_crypto_rsa.h"



#if defined(XRT_FEATURE_CRYPTO_RSA)

/* 设置 RSA 操作的结构化错误。 */
void __xrtRsaError(cstr sOperation, cstr sMessage, int iCode)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = (iCode == XCRYPTO_ERROR_KEY) ? XERR_VALUE : XERR_PROTOCOL;
	Desc.Domain = "xrt.crypto";
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}

#endif
