#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_CHACHA20)

/* 执行一次 ChaCha 四分之一轮。 */
static inline void __xrtChaCha20Quarter(
	uint32* pA,
	uint32* pB,
	uint32* pC,
	uint32* pD
)
{
	*pA += *pB;
	*pD = __xrtCryptoRotateLeft32(*pD ^ *pA, 16u);
	*pC += *pD;
	*pB = __xrtCryptoRotateLeft32(*pB ^ *pC, 12u);
	*pA += *pB;
	*pD = __xrtCryptoRotateLeft32(*pD ^ *pA, 8u);
	*pC += *pD;
	*pB = __xrtCryptoRotateLeft32(*pB ^ *pC, 7u);
}



/* 从固定密钥、IETF nonce 和块计数器建立 16 字状态。 */
static void __xrtChaCha20State(
	uint32 pState[16],
	const uint8 pKey[XRT_CHACHA20_KEY_SIZE],
	const uint8 pNonce[XRT_CHACHA20_NONCE_SIZE],
	uint32 iCounter
)
{
	pState[0] = UINT32_C(0x61707865);
	pState[1] = UINT32_C(0x3320646E);
	pState[2] = UINT32_C(0x79622D32);
	pState[3] = UINT32_C(0x6B206574);
	for ( size_t i = 0; i < 8; i++ ) {
		pState[4 + i] = __xrtCryptoLoadLe32(pKey + (i * 4));
	}
	pState[12] = iCounter;
	pState[13] = __xrtCryptoLoadLe32(pNonce);
	pState[14] = __xrtCryptoLoadLe32(pNonce + 4);
	pState[15] = __xrtCryptoLoadLe32(pNonce + 8);
}



/* 对一个已建立的状态执行 20 轮并序列化 64 字节密钥流。 */
static void __xrtChaCha20BlockState(
	const uint32 pState[16],
	uint8 pOutput[XRT_CHACHA20_BLOCK_SIZE]
)
{
	uint32 Working[16];

	memcpy(Working, pState, sizeof(Working));
	for ( uint32 i = 0; i < 10; i++ ) {
		__xrtChaCha20Quarter(&Working[0], &Working[4], &Working[8], &Working[12]);
		__xrtChaCha20Quarter(&Working[1], &Working[5], &Working[9], &Working[13]);
		__xrtChaCha20Quarter(&Working[2], &Working[6], &Working[10], &Working[14]);
		__xrtChaCha20Quarter(&Working[3], &Working[7], &Working[11], &Working[15]);
		__xrtChaCha20Quarter(&Working[0], &Working[5], &Working[10], &Working[15]);
		__xrtChaCha20Quarter(&Working[1], &Working[6], &Working[11], &Working[12]);
		__xrtChaCha20Quarter(&Working[2], &Working[7], &Working[8], &Working[13]);
		__xrtChaCha20Quarter(&Working[3], &Working[4], &Working[9], &Working[14]);
	}
	for ( size_t i = 0; i < 16; i++ ) {
		__xrtCryptoStoreLe32(
			pOutput + (i * 4),
			Working[i] + pState[i]
		);
	}
	xrtSecureZero(Working, sizeof(Working));
}



/* 为 AEAD 密钥生成过程提供一个不复制算法的内部块入口。 */
void __xrtChaCha20Block(
	const uint8 pKey[XRT_CHACHA20_KEY_SIZE],
	const uint8 pNonce[XRT_CHACHA20_NONCE_SIZE],
	uint32 iCounter,
	uint8 pOutput[XRT_CHACHA20_BLOCK_SIZE]
)
{
	uint32 State[16];

	__xrtChaCha20State(State, pKey, pNonce, iCounter);
	__xrtChaCha20BlockState(State, pOutput);
	xrtSecureZero(State, sizeof(State));
}



/* 校验固定输入、允许的原位形式以及 32 位计数器可覆盖的块数。 */
static bool __xrtChaCha20Validate(
	const void* pKey,
	const void* pNonce,
	uint32 iCounter,
	const void* pInput,
	void* pOutput,
	size_t iSize
)
{
	uint64 iBlocks;
	uint64 iAvailable;

	if ( (pKey == NULL) || (pNonce == NULL) ||
		 ((pInput == NULL) && (iSize != 0)) ||
		 ((pOutput == NULL) && (iSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (__xrtCryptoRangesOverlap(
			pOutput, iSize, pKey, XRT_CHACHA20_KEY_SIZE
		)) || (__xrtCryptoRangesOverlap(
			pOutput, iSize, pNonce, XRT_CHACHA20_NONCE_SIZE
		)) || ((pOutput != pInput) && (__xrtCryptoRangesOverlap(
			pOutput, iSize, pInput, iSize
		))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iBlocks = (uint64)(iSize / XRT_CHACHA20_BLOCK_SIZE);
	if ( (iSize % XRT_CHACHA20_BLOCK_SIZE) != 0 ) {
		iBlocks++;
	}
	iAvailable = ((uint64)UINT32_MAX - (uint64)iCounter) + 1u;
	if ( iBlocks > iAvailable ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	return true;
}



/* 复用一个基础状态逐块异或密钥流，并清理全部栈上密钥材料。 */
XRT_API bool xrtChaCha20(
	const void* pKey,
	const void* pNonce,
	uint32 iCounter,
	const void* pInput,
	void* pOutput,
	size_t iSize
)
{
	const uint8* pRead = (const uint8*)pInput;
	uint8* pWrite = (uint8*)pOutput;
	uint32 State[16];
	uint8 Stream[XRT_CHACHA20_BLOCK_SIZE];
	size_t iRemain = iSize;

	if ( !__xrtChaCha20Validate(
		pKey, pNonce, iCounter, pInput, pOutput, iSize
	) ) {
		return false;
	}
	if ( iSize == 0 ) {
		return true;
	}
	__xrtChaCha20State(
		State,
		(const uint8*)pKey,
		(const uint8*)pNonce,
		iCounter
	);
	while ( iRemain != 0 ) {
		size_t iChunk = iRemain < sizeof(Stream) ? iRemain : sizeof(Stream);

		__xrtChaCha20BlockState(State, Stream);
		if ( iChunk == sizeof(Stream) ) {
			for ( size_t i = 0; i < 16; i++ ) {
				__xrtCryptoStoreLe32(
					pWrite + (i * 4),
					__xrtCryptoLoadLe32(pRead + (i * 4)) ^
					__xrtCryptoLoadLe32(Stream + (i * 4))
				);
			}
		} else {
			for ( size_t i = 0; i < iChunk; i++ ) {
				pWrite[i] = pRead[i] ^ Stream[i];
			}
		}
		pRead += iChunk;
		pWrite += iChunk;
		iRemain -= iChunk;
		if ( iRemain != 0 ) {
			State[12]++;
		}
	}
	xrtSecureZero(Stream, sizeof(Stream));
	xrtSecureZero(State, sizeof(State));
	return true;
}

#endif
