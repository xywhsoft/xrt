#include "../../../dev/bench/bench_common.h"

#define XSSH_MODULE_SSH_PACKET_AES_GCM
#include <xssh.h>



/* benchmark 使用确定性会话流，单独测量 packet 与 AES-GCM 成本。 */
static bool benchSshAesGcmPadding(
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



/* 测量 AES-128-GCM packet 的无分配加密、认证与解密往返。 */
int main(int argc, char** argv)
{
	static const unsigned char arrPayload[] = {
		94u, 'x', 'l', 'a', 'n', 'g', '-', 's', 's', 'h', '-',
		'e', 'n', 'c', 'r', 'y', 'p', 't', 'e', 'd'
	};
	unsigned char arrKey[16] = { 0u };
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = { 0u };
	unsigned char arrWire[96];
	unsigned char arrPlain[64];
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 1000000u);
	uint32 iPaddingState = UINT32_C(0x9e3779b9);
	xsshaesgcm WriteState;
	xsshaesgcm ReadState;
	xbenchtimer Timer;
	uint64 iElapsed;
	uint64 iChecksum = 0u;
	uint32 i;

	if ( (iIterations == 0u) || (xrtSshAesGcmInit(
		&WriteState,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) != XSSH_OK) || (xrtSshAesGcmInit(
		&ReadState,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) != XSSH_OK) ) {
		return 1;
	}
	xbenchTimerStart(&Timer);
	for ( i = 0u; i < iIterations; ++i ) {
		xsshwriter Writer;
		xsshreader Reader;
		xsshpacketview Packet;

		if ( !xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) ||
			(xrtSshAesGcmWrite(
				&Writer,
				(xbytesview){ arrPayload, sizeof(arrPayload) },
				&WriteState,
				NULL,
				benchSshAesGcmPadding,
				&iPaddingState
			) != XSSH_OK) || !xrtSshReaderInit(
				&Reader,
				(xbytesview){ arrWire, Writer.Size }
			) || (xrtSshAesGcmRead(
				&Reader,
				&ReadState,
				0u,
				NULL,
				&Packet,
				arrPlain,
				sizeof(arrPlain)
			) != XSSH_OK) ) {
			xrtSshAesGcmClear(&WriteState);
			xrtSshAesGcmClear(&ReadState);
			return 2;
		}
		iChecksum += (uint64)Packet.Payload.Size +
			(uint64)Packet.Payload.Data[0] + (uint64)Packet.PaddingSize;
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);
	xrtSshAesGcmClear(&WriteState);
	xrtSshAesGcmClear(&ReadState);

	printf("xssh aes-gcm packet benchmark\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	xbenchPrintMetricDouble(
		"aes_gcm_packet_roundtrips_per_sec",
		xbenchSafeRate(iIterations, iElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
