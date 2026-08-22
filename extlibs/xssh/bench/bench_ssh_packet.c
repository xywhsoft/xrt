#include "../../../dev/bench/bench_common.h"

#define XSSH_MODULE_SSH_PACKET
#include <xssh.h>



/* 使用轻量会话状态生成 benchmark padding，避免计入系统随机调用。 */
static bool benchSshPacketPadding(
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



/* 测量固定 payload 的 plain packet 构建与解析往返。 */
int main(int argc, char** argv)
{
	static const unsigned char arrPayload[] = {
		94u, 'x', 'l', 'a', 'n', 'g', '-', 's', 's', 'h', '-', 'p', 'a', 'c', 'k', 'e', 't'
	};
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 1000000u);
	unsigned char arrWire[64];
	uint32 iPaddingState = UINT32_C(0x9e3779b9);
	uint32 iWriteSequence = 0u;
	uint32 iReadSequence = 0u;
	xbenchtimer Timer;
	uint64 iElapsed;
	uint64 iChecksum = 0u;
	uint32 i;

	if ( iIterations == 0u ) {
		fprintf(stderr, "iteration count must be non-zero.\n");
		return 1;
	}
	xbenchTimerStart(&Timer);
	for ( i = 0u; i < iIterations; ++i ) {
		xsshwriter Writer;
		xsshreader Reader;
		xsshpacketview Packet;

		if ( !xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) ||
			(xrtSshPacketWrite(
				&Writer,
				(xbytesview){ arrPayload, sizeof(arrPayload) },
				16u,
				&iWriteSequence,
				benchSshPacketPadding,
				&iPaddingState
			) != XSSH_OK) || !xrtSshReaderInit(
				&Reader,
				(xbytesview){ arrWire, Writer.Size }
			) || (xrtSshPacketRead(
				&Reader,
				16u,
				0u,
				&iReadSequence,
				&Packet
			) != XSSH_OK) ) {
			return 2;
		}
		iChecksum += (uint64)Packet.Payload.Size +
			(uint64)Packet.Padding.Data[0] + (uint64)Packet.Sequence;
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xssh packet benchmark\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	xbenchPrintMetricDouble(
		"packet_roundtrips_per_sec",
		xbenchSafeRate(iIterations, iElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
