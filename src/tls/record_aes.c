#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_RECORD_AES)

/* 初始化 AES-GCM 记录密钥。 */
bool __xrtTlsRecordAesInit(
	xtlsrecordkey* pKey,
	const void* pData,
	size_t iSize
)
{
	return xrtAesGcmInit(
		&pKey->Aes, pData, iSize, XRT_AES_GCM_TAG_DEFAULT_SIZE
	);
}



/* 清除 AES-GCM 展开密钥。 */
void __xrtTlsRecordAesClear(xtlsrecordkey* pKey)
{
	xrtAesGcmClear(&pKey->Aes);
}



/* 使用绑定的 AES-GCM 密钥保护记录负载。 */
bool __xrtTlsRecordAesSeal(
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
	return xrtAesGcmSeal(
		&pKey->Aes, pNonce, 12u, pAad, iAadSize,
		pPlain, iPlainSize, pOutput, iOutputSize
	);
}



/* 使用绑定的 AES-GCM 密钥认证并打开记录负载。 */
bool __xrtTlsRecordAesOpen(
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
	return xrtAesGcmOpen(
		&pKey->Aes, pNonce, 12u, pAad, iAadSize,
		pInput, iInputSize, pPlain, iPlainSize
	);
}

#endif
