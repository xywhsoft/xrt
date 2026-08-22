#include "../../../dev/bench/bench_common.h"

#define XSSH_MODULE_SSH_TRANSPORT_STATE
#include <xssh.h>



#define BENCH_SSH_KEX_INIT 30u
#define BENCH_SSH_KEX_REPLY 31u
#define BENCH_SSH_APPLICATION_MESSAGE 94u



/* 填充一次 client/server 初始 KEXINIT 协商所需的借用视图。 */
static bool benchSshTransportKexViews(
	xsshkexinit* pLocal,
	xsshkexinit* pPeer,
	xsshkexnegotiation* pNegotiation
)
{
	memset(pLocal, 0, sizeof(*pLocal));
	memset(pPeer, 0, sizeof(*pPeer));
	memset(pNegotiation, 0, sizeof(*pNegotiation));
	pLocal->KexAlgorithms =
		XRT_STR_LITERAL(XSSH_KEX_CLIENT_INITIAL_DEFAULT);
	pPeer->KexAlgorithms =
		XRT_STR_LITERAL(XSSH_KEX_SERVER_INITIAL_DEFAULT);
	pLocal->ServerHostKeyAlgorithms = XRT_STR_LITERAL("ssh-ed25519");
	pPeer->ServerHostKeyAlgorithms = XRT_STR_LITERAL("ssh-ed25519");
	pLocal->EncryptionClientToServer =
		XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	pLocal->EncryptionServerToClient =
		XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	pPeer->EncryptionClientToServer =
		XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	pPeer->EncryptionServerToClient =
		XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	pLocal->MacClientToServer = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	pLocal->MacServerToClient = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	pPeer->MacClientToServer = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	pPeer->MacServerToClient = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	pLocal->CompressionClientToServer =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	pLocal->CompressionServerToClient =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	pPeer->CompressionClientToServer =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	pPeer->CompressionServerToClient =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	return xrtSshKexNegotiate(
		pLocal,
		pPeer,
		pNegotiation
	) == XSSH_OK;
}



/* 把 client transport 推进到稳定的 OPEN 数据面。 */
static bool benchSshTransportOpen(xsshtransportstate* pState)
{
	xsshtransportkexrules Rules;
	xsshkexinit Local;
	xsshkexinit Peer;
	xsshkexnegotiation Negotiation;
	uint32 iActions;

	if ( !xrtSshTransportStateInit(pState, XSSH_ROLE_CLIENT) ||
		!xrtSshTransportKexRulesInit(&Rules) ||
		!xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_LOCAL,
			BENCH_SSH_KEX_INIT,
			1u
		) || !xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_PEER,
			BENCH_SSH_KEX_REPLY,
			1u
		) || !benchSshTransportKexViews(
			&Local,
			&Peer,
			&Negotiation
		) || (xrtSshTransportIdentificationCommit(
			pState,
			XSSH_TRANSPORT_LOCAL
		) != XSSH_OK) || (xrtSshTransportIdentificationCommit(
			pState,
			XSSH_TRANSPORT_PEER
		) != XSSH_OK) || (xrtSshTransportKexInitCommit(
			pState,
			XSSH_TRANSPORT_LOCAL,
			false
		) != XSSH_OK) || (xrtSshTransportKexInitCommit(
			pState,
			XSSH_TRANSPORT_PEER,
			false
		) != XSSH_OK) || (xrtSshTransportKexConfigure(
			pState,
			&Local,
			&Peer,
			&Negotiation,
			&Rules
		) != XSSH_OK) || (xrtSshTransportMessageCommit(
			pState,
			XSSH_TRANSPORT_LOCAL,
			BENCH_SSH_KEX_INIT
		) != XSSH_OK) || (xrtSshTransportMessageCommit(
			pState,
			XSSH_TRANSPORT_PEER,
			BENCH_SSH_KEX_REPLY
		) != XSSH_OK) || (xrtSshTransportNewKeysCommit(
			pState,
			XSSH_TRANSPORT_LOCAL,
			&iActions
		) != XSSH_OK) || (xrtSshTransportNewKeysCommit(
			pState,
			XSSH_TRANSPORT_PEER,
			&iActions
		) != XSSH_OK) ) {
		return false;
	}

	/* 关闭首包 EXT_INFO 机会窗口，使计时段全部走稳定分支。 */
	return (xrtSshTransportMessageCommit(
		pState,
		XSSH_TRANSPORT_LOCAL,
		BENCH_SSH_APPLICATION_MESSAGE
	) == XSSH_OK) && (xrtSshTransportMessageCommit(
		pState,
		XSSH_TRANSPORT_PEER,
		BENCH_SSH_APPLICATION_MESSAGE
	) == XSSH_OK);
}



/* 测量发送方在可靠入队前检查、入队后提交的完整状态转换成本。 */
static bool benchSshTransportRun(
	xsshtransportstate* pState,
	uint32 iIterations,
	uint64* pElapsed
)
{
	xbenchtimer Timer;
	uint32 i;

	xbenchTimerStart(&Timer);
	for ( i = 0u; i < iIterations; ++i ) {
		if ( (xrtSshTransportMessageCheck(
			pState,
			XSSH_TRANSPORT_LOCAL,
			BENCH_SSH_APPLICATION_MESSAGE
		) != XSSH_OK) || (xrtSshTransportMessageCommit(
			pState,
			XSSH_TRANSPORT_LOCAL,
			BENCH_SSH_APPLICATION_MESSAGE
		) != XSSH_OK) ) {
			return false;
		}
	}
	xbenchTimerStop(&Timer);
	*pElapsed = xbenchTimerElapsedNs(&Timer);
	return true;
}



/* 分别测量 OPEN 与对端已发起 rekey 时仍可发送在途数据的热路径。 */
int main(int argc, char** argv)
{
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 10000000u);
	xsshtransportstate Open;
	xsshtransportstate Rekey;
	uint64 iOpenElapsed;
	uint64 iRekeyElapsed;
	uint64 iChecksum;

	if ( (iIterations == 0u) || !benchSshTransportOpen(&Open) ) {
		fprintf(stderr, "transport state setup failed.\n");
		return 1;
	}
	Rekey = Open;
	if ( xrtSshTransportKexInitCommit(
		&Rekey,
		XSSH_TRANSPORT_PEER,
		false
	) != XSSH_OK ) {
		fprintf(stderr, "transport rekey setup failed.\n");
		return 2;
	}
	if ( !benchSshTransportRun(
		&Open,
		iIterations,
		&iOpenElapsed
	) || !benchSshTransportRun(
		&Rekey,
		iIterations,
		&iRekeyElapsed
	) ) {
		fprintf(stderr, "transport state transition failed.\n");
		return 3;
	}

	iChecksum = Open.LocalPackets + Rekey.LocalPackets +
		Open.PeerPackets + Rekey.PeerPackets;
	printf("xssh transport state benchmark\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	xbenchPrintMetricDouble(
		"open_packet_transitions_per_sec",
		xbenchSafeRate(iIterations, iOpenElapsed)
	);
	xbenchPrintMetricDouble(
		"rekey_inflight_packet_transitions_per_sec",
		xbenchSafeRate(iIterations, iRekeyElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
