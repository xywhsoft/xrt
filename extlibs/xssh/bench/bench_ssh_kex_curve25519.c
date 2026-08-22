#include "../../../dev/bench/bench_common.h"

#define XSSH_MODULE_SSH_KEX_CURVE25519
#include <xssh.h>



/* 测量固定私钥与 peer 公钥的 X25519 共享秘密热路径。 */
int main(int argc, char** argv)
{
	static const unsigned char arrPrivate[32] = {
		0x77u, 0x07u, 0x6du, 0x0au, 0x73u, 0x18u, 0xa5u, 0x7du,
		0x3cu, 0x16u, 0xc1u, 0x72u, 0x51u, 0xb2u, 0x66u, 0x45u,
		0xdfu, 0x4cu, 0x2fu, 0x87u, 0xebu, 0xc0u, 0x99u, 0x2au,
		0xb1u, 0x77u, 0xfbu, 0xa5u, 0x1du, 0xb9u, 0x2cu, 0x2au
	};
	static const unsigned char arrPeer[32] = {
		0xdeu, 0x9eu, 0xdbu, 0x7du, 0x7bu, 0x7du, 0xc1u, 0xb4u,
		0xd3u, 0x5bu, 0x61u, 0xc2u, 0xecu, 0xe4u, 0x35u, 0x37u,
		0x3fu, 0x83u, 0x43u, 0xc8u, 0x5bu, 0x78u, 0x67u, 0x4du,
		0xadu, 0xfcu, 0x7eu, 0x14u, 0x6fu, 0x88u, 0x2bu, 0x4fu
	};
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 10000u);
	unsigned char arrShared[32];
	xbenchtimer Timer;
	uint64 iElapsed;
	uint64 iChecksum = 0u;
	uint32 i;

	if ( iIterations == 0u ) {
		return 1;
	}
	xbenchTimerStart(&Timer);
	for ( i = 0u; i < iIterations; ++i ) {
		if ( xrtSshCurve25519Shared(
			arrPrivate,
			arrPeer,
			arrShared
		) != XSSH_OK ) {
			return 2;
		}
		iChecksum += arrShared[i & 31u];
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xssh curve25519 benchmark\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	xbenchPrintMetricDouble(
		"curve25519_agreements_per_sec",
		xbenchSafeRate(iIterations, iElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
