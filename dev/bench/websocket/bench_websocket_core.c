#include "../bench_common.h"

#define XRT_MODULE_WEBSOCKET_FRAME
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 测量无连接对象参与的帧头解析、写出和原地掩码核心。 */
int main(int argc, char** argv)
{
	static const uint8 Head[] = {
		0x82, 0xFE, 0x04, 0x00, 0x12, 0x34, 0x56, 0x78
	};
	static const uint8 Mask[XWS_MASK_SIZE] = {
		0x12, 0x34, 0x56, 0x78
	};
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 1000000u);
	xwsframeconfig Config;
	xbenchtimer Timer;
	uint64 iParseElapsed;
	uint64 iWriteElapsed;
	uint64 iMaskElapsed;
	uint64 iChecksum = 0;
	uint8 Output[XWS_FRAME_HEAD_MAX];
	uint8 Payload[256];
	size_t iOutputSize = 0;

	if ( iIterations == 0 ) {
		fprintf(stderr, "iteration count must be non-zero.\n");
		return 1;
	}
	xrtWsFrameConfigInit(&Config);
	Config.Mask = XWS_MASK_REQUIRED;
	memset(Payload, 0xA5, sizeof(Payload));

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xwsframe Frame;

		if (
			xrtWsFrameParse(
				(xbytesview){ Head, sizeof(Head) },
				&Frame,
				&Config,
				NULL
			) != XWS_FRAME_READY
		) {
			return 2;
		}
		iChecksum += Frame.PayloadSize + (uint64)Frame.HeadSize;
	}
	xbenchTimerStop(&Timer);
	iParseElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xwsframe Frame;

		xrtWsFrameInit(&Frame);
		Frame.Flags = (uint32)XWS_FRAME_FIN | (uint32)XWS_FRAME_MASKED;
		Frame.Opcode = (uint8)XWS_OPCODE_BINARY;
		Frame.PayloadSize = 1024;
		memcpy(Frame.Mask, Mask, sizeof(Mask));
		if (
			!xrtWsFrameWrite(
				&Frame,
				&Config,
				Output,
				sizeof(Output),
				&iOutputSize
			)
		) {
			return 3;
		}
		iChecksum += (uint64)iOutputSize + (uint64)Output[0];
	}
	xbenchTimerStop(&Timer);
	iWriteElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		if ( !xrtWsMask(Payload, sizeof(Payload), Mask, 0) ) {
			return 4;
		}
	}
	xbenchTimerStop(&Timer);
	iMaskElapsed = xbenchTimerElapsedNs(&Timer);
	iChecksum += (uint64)Payload[0];

	printf("xrt WebSocket core benchmark\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	xbenchPrintMetricDouble(
		"ws_frame_parse_ops_per_sec",
		xbenchSafeRate(iIterations, iParseElapsed)
	);
	xbenchPrintMetricDouble(
		"ws_frame_write_ops_per_sec",
		xbenchSafeRate(iIterations, iWriteElapsed)
	);
	xbenchPrintMetricDouble(
		"ws_mask_mib_per_sec",
		xbenchSafeRate(
			(uint64)iIterations * (uint64)sizeof(Payload),
			iMaskElapsed
		) / (1024.0 * 1024.0)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
