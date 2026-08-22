#include "../internal/xrt_crypto_ecdsa.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_CORE)

/* 设置签名编码、签名生成或签名验证错误。 */
void __xrtEcdsaError(cstr sOperation, cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_PROTOCOL;
	Desc.Domain = "xrt.crypto";
	Desc.Code = XCRYPTO_ERROR_SIGNATURE;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}

#endif
