#include "../../../dev/bench/bench_common.h"

#define XSSH_MODULE_SSH_WIRE
#include <xssh.h>



/* 分别测量基础类型、名称协商和 mpint 规范化的无分配往返。 */
int main(int argc, char** argv)
{
	static const unsigned char arrMagnitude[] = {
		0u, 0u, 0x80u, 0x11u, 0x22u, 0x33u, 0x44u
	};
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 1000000u);
	unsigned char arrWire[64];
	xbenchtimer Timer;
	uint64 iWireElapsed;
	uint64 iNameElapsed;
	uint64 iMpintElapsed;
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
		uint32 iU32;
		uint64 iU64;
		xbytesview Text;

		if ( !xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) ||
			(xrtSshWriteU32(&Writer, i) != XSSH_OK) ||
			(xrtSshWriteU64(&Writer, UINT64_C(0x0102030405060708)) != XSSH_OK) ||
			(xrtSshWriteString(&Writer, XRT_BYTES_LITERAL("xlang")) != XSSH_OK) ||
			!xrtSshReaderInit(
				&Reader,
				(xbytesview){ arrWire, Writer.Size }
			) || (xrtSshReadU32(&Reader, &iU32) != XSSH_OK) ||
			(xrtSshReadU64(&Reader, &iU64) != XSSH_OK) ||
			(xrtSshReadString(&Reader, &Text) != XSSH_OK) ) {
			return 2;
		}
		iChecksum += (uint64)iU32 + iU64 + (uint64)Text.Size;
	}
	xbenchTimerStop(&Timer);
	iWireElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( i = 0u; i < iIterations; ++i ) {
		xstrview Match;

		if ( xrtSshNameListFirstMatch(
			XRT_STR_LITERAL(
				"sntrup761x25519-sha512,curve25519-sha256,"
				"diffie-hellman-group14-sha256"
			),
			XRT_STR_LITERAL(
				"diffie-hellman-group14-sha256,curve25519-sha256"
			),
			&Match
		) != XSSH_OK ) {
			return 3;
		}
		iChecksum += (uint64)Match.Size;
	}
	xbenchTimerStop(&Timer);
	iNameElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( i = 0u; i < iIterations; ++i ) {
		xsshwriter Writer;
		xsshreader Reader;
		xbytesview Value;

		if ( !xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) ||
			(xrtSshWriteMpint(
				&Writer,
				(xbytesview){ arrMagnitude, sizeof(arrMagnitude) }
			) != XSSH_OK) || !xrtSshReaderInit(
				&Reader,
				(xbytesview){ arrWire, Writer.Size }
			) || (xrtSshReadMpint(&Reader, &Value) != XSSH_OK) ) {
			return 4;
		}
		iChecksum += (uint64)Value.Size + (uint64)Value.Data[Value.Size - 1u];
	}
	xbenchTimerStop(&Timer);
	iMpintElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xssh wire benchmark\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	xbenchPrintMetricDouble(
		"wire_roundtrips_per_sec",
		xbenchSafeRate(iIterations, iWireElapsed)
	);
	xbenchPrintMetricDouble(
		"namelist_matches_per_sec",
		xbenchSafeRate(iIterations, iNameElapsed)
	);
	xbenchPrintMetricDouble(
		"mpint_roundtrips_per_sec",
		xbenchSafeRate(iIterations, iMpintElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
