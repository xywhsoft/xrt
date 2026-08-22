#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_PACKET_CODEC
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 单头测试使用确定性 padding，确保核心 codec 不依赖系统随机源。 */
static bool testSingleSshPacketCodecPadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	(void)pUserData;
	memset(pOutput, 0xa5, iSize);
	return true;
}



/* 统一 codec 闭包包含 AES-GCM，但不携带随机与网络运行时。 */
int main(void)
{
	unsigned char arrKey[16] = { 0u };
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = { 0u };
	unsigned char arrWire[64];
	unsigned char arrPlain[32];
	xsshpacketcodec Codec;
	xsshpacketview Packet;
	xsshwriter Writer;
	xsshreader Reader;

	#if !defined(XSSH_FEATURE_PACKET_CODEC) || \
		!defined(XSSH_FEATURE_PACKET_AES_GCM) || \
		!defined(XRT_FEATURE_CRYPTO_AES_GCM)
		#error "SSH packet codec dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_RANDOM_SECURE) || defined(XRT_FEATURE_NETWORK)
		#error "SSH packet codec unexpectedly enabled random or network"
	#endif

	if ( (xrtSshPacketCodecInit(&Codec, 0u) != XSSH_OK) ||
		(xrtSshPacketCodecSetWriteAesGcm(
			&Codec,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) }
		) != XSSH_OK) || (xrtSshPacketCodecSetReadAesGcm(
			&Codec,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) }
		) != XSSH_OK) || !xrtSshWriterInit(
			&Writer,
			arrWire,
			sizeof(arrWire)
		) || (xrtSshPacketCodecWriteWithPadding(
			&Codec,
			&Writer,
			XRT_BYTES_LITERAL("single"),
			testSingleSshPacketCodecPadding,
			NULL
		) != XSSH_OK) || !xrtSshReaderInit(
			&Reader,
			(xbytesview){ arrWire, Writer.Size }
		) || (xrtSshPacketCodecRead(
			&Codec,
			&Reader,
			&Packet,
			arrPlain,
			sizeof(arrPlain)
		) != XSSH_OK) ) {
		xrtSshPacketCodecClear(&Codec);
		return 1;
	}
	xrtSshPacketCodecClear(&Codec);
	return 0;
}
