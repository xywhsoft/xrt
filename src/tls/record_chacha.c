#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_RECORD_CHACHA)

/* 初始化 ChaCha20-Poly1305 记录密钥。 */
bool __xrtTlsRecordChaChaInit(
	xtlsrecordkey* pKey,
	const void* pData,
	size_t iSize
)
{
	if ( iSize != sizeof(pKey->ChaChaKey) ) {
		return false;
	}
	memcpy(pKey->ChaChaKey, pData, iSize);
	return true;
}



/* 清除 ChaCha20-Poly1305 原始密钥。 */
void __xrtTlsRecordChaChaClear(xtlsrecordkey* pKey)
{
	xrtSecureZero(pKey->ChaChaKey, sizeof(pKey->ChaChaKey));
}



/* 使用绑定的 ChaCha20-Poly1305 密钥保护记录负载。 */
bool __xrtTlsRecordChaChaSeal(
	const xtlsrecordkey* pKey,
	const uint8* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pPlain,
	size_t iPlainSize,
	void* pOutput,
	size_t iOutputSize
)
{
	return xrtChaCha20Poly1305Seal(
		pKey->ChaChaKey, pNonce, pAad, iAadSize,
		pPlain, iPlainSize, pOutput, iOutputSize
	);
}



/* 使用绑定的 ChaCha20-Poly1305 密钥认证并打开记录负载。 */
bool __xrtTlsRecordChaChaOpen(
	const xtlsrecordkey* pKey,
	const uint8* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pInput,
	size_t iInputSize,
	void* pPlain,
	size_t iPlainSize
)
{
	return xrtChaCha20Poly1305Open(
		pKey->ChaChaKey, pNonce, pAad, iAadSize,
		pInput, iInputSize, pPlain, iPlainSize
	);
}

#endif
