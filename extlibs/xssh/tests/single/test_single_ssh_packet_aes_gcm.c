#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_PACKET_AES_GCM
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 为单头测试生成固定 padding。 */
static bool testSingleSshAesGcmPadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	(void)pUserData;
	memset(pOutput, 0xa5, iSize);
	return true;
}



/* AES-GCM packet 单头闭包到 crypto，但不隐式携带随机与网络。 */
int main(void)
{
	unsigned char arrKey[16] = { 0u };
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = { 0u };
	unsigned char arrWire[64];
	xsshaesgcm State;
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_PACKET_AES_GCM) || \
		!defined(XRT_FEATURE_CRYPTO_AES_GCM)
		#error "SSH packet AES-GCM dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_RANDOM_SECURE) || defined(XRT_FEATURE_NETWORK)
		#error "SSH packet AES-GCM unexpectedly enabled random or network"
	#endif

	if ( (xrtSshAesGcmInit(
		&State,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) != XSSH_OK) || !xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) ) {
		return 1;
	}
	if ( xrtSshAesGcmWrite(
		&Writer,
		XRT_BYTES_LITERAL("gcm"),
		&State,
		NULL,
		testSingleSshAesGcmPadding,
		NULL
	) != XSSH_OK ) {
		xrtSshAesGcmClear(&State);
		return 1;
	}
	xrtSshAesGcmClear(&State);
	return 0;
}
