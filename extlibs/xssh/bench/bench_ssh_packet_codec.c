#include "../../../dev/bench/bench_common.h"

#define XSSH_MODULE_SSH_PACKET_CODEC
#include <xssh.h>



/* benchmark 使用确定性 padding，避免把系统随机源吞吐混入 codec 开销。 */
static bool benchSshPacketCodecPadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	uint32* pState = (uint32*)pUserData;
	bytes pBytes = (bytes)pOutput;
	size_t i;

	for ( i = 0u; i < iSize; ++i ) {
		*pState ^= *pState << 13u;
		*pState ^= *pState >> 17u;
		*pState ^= *pState << 5u;
		pBytes[i] = (uint8)*pState;
	}
	return true;
}



/* 测量 transport 实际使用的统一 AES-GCM codec 往返热路径。 */
int main(int argc, char** argv)
{
	static const unsigned char arrPayload[] = {
		94u, 'x', 's', 's', 'h', '-', 'p', 'a', 'c', 'k', 'e', 't'
	};
	unsigned char arrKey[16] = { 0u };
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = { 0u };
	unsigned char arrWire[96];
	unsigned char arrPlain[64];
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 1000000u);
	uint32 iPaddingState = UINT32_C(0x9e3779b9);
	xsshpacketcodec Codec;
	xbenchtimer Timer;
	uint64 iElapsed;
	uint64 iChecksum = 0u;
	uint32 i;

	if ( (iIterations == 0u) ||
		(xrtSshPacketCodecInit(&Codec, 0u) != XSSH_OK) ||
		(xrtSshPacketCodecSetWriteAesGcm(
			&Codec,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) }
		) != XSSH_OK) || (xrtSshPacketCodecSetReadAesGcm(
			&Codec,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) }
		) != XSSH_OK) ) {
		return 1;
	}
	xbenchTimerStart(&Timer);
	for ( i = 0u; i < iIterations; ++i ) {
		xsshpacketview Packet;
		xsshwriter Writer;
		xsshreader Reader;

		if ( !xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) ||
			(xrtSshPacketCodecWriteWithPadding(
				&Codec,
				&Writer,
				(xbytesview){ arrPayload, sizeof(arrPayload) },
				benchSshPacketCodecPadding,
				&iPaddingState
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
			return 2;
		}
		iChecksum += (uint64)Packet.Payload.Size +
			(uint64)Packet.Payload.Data[0] + (uint64)Packet.PaddingSize;
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);
	xrtSshPacketCodecClear(&Codec);

	printf("xssh packet codec benchmark\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	xbenchPrintMetricDouble(
		"packet_codec_roundtrips_per_sec",
		xbenchSafeRate(iIterations, iElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
