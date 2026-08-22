#include "../../../dev/bench/bench_common.h"

#define XSSH_MODULE_SSH_TRANSPORT_CORE
#include <xssh.h>



#define BENCH_SSH_KEX_INIT 30u
#define BENCH_SSH_KEX_REPLY 31u



/* 基准使用轻量确定性 padding，不混入系统 CSPRNG 调用。 */
static bool benchSshTransportCorePadding(
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



/* 填充一次 client/server 初始 KEXINIT 协商视图。 */
static bool benchSshTransportCoreKexViews(
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



/* 在计时前通过公开底层状态 API 建立稳定 AES-GCM OPEN 数据面。 */
static bool benchSshTransportCoreOpen(xsshtransportcore* pCore)
{
	unsigned char arrKey[16] = { 0u };
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = { 0u };
	xsshtransportkexrules Rules;
	xsshkexinit Local;
	xsshkexinit Peer;
	xsshkexnegotiation Negotiation;
	uint32 iActions;

	return xrtSshTransportCoreInit(
		pCore,
		XSSH_ROLE_CLIENT,
		0u,
		NULL,
		0u
	) && benchSshTransportCoreKexViews(
		&Local,
		&Peer,
		&Negotiation
	) && xrtSshTransportKexRulesInit(&Rules) &&
		xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_LOCAL,
			BENCH_SSH_KEX_INIT,
			1u
		) && xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_PEER,
			BENCH_SSH_KEX_REPLY,
			1u
		) && (xrtSshTransportIdentificationCommit(
			&pCore->State,
			XSSH_TRANSPORT_LOCAL
		) == XSSH_OK) && (xrtSshTransportIdentificationCommit(
			&pCore->State,
			XSSH_TRANSPORT_PEER
		) == XSSH_OK) && (xrtSshTransportKexInitCommit(
			&pCore->State,
			XSSH_TRANSPORT_LOCAL,
			false
		) == XSSH_OK) && (xrtSshTransportKexInitCommit(
			&pCore->State,
			XSSH_TRANSPORT_PEER,
			false
		) == XSSH_OK) && (xrtSshTransportKexConfigure(
			&pCore->State,
			&Local,
			&Peer,
			&Negotiation,
			&Rules
		) == XSSH_OK) && (xrtSshTransportMessageCommit(
			&pCore->State,
			XSSH_TRANSPORT_LOCAL,
			BENCH_SSH_KEX_INIT
		) == XSSH_OK) && (xrtSshTransportMessageCommit(
			&pCore->State,
			XSSH_TRANSPORT_PEER,
			BENCH_SSH_KEX_REPLY
		) == XSSH_OK) && (xrtSshTransportNewKeysCommit(
			&pCore->State,
			XSSH_TRANSPORT_LOCAL,
			&iActions
		) == XSSH_OK) && (xrtSshTransportNewKeysCommit(
			&pCore->State,
			XSSH_TRANSPORT_PEER,
			&iActions
		) == XSSH_OK) && (xrtSshPacketCodecSetWriteAesGcm(
			&pCore->Codec,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) }
		) == XSSH_OK) && (xrtSshPacketCodecSetReadAesGcm(
			&pCore->Codec,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) }
		) == XSSH_OK) &&
		(xrtSshPacketCodecResetWriteSequence(&pCore->Codec) == XSSH_OK) &&
		(xrtSshPacketCodecResetReadSequence(&pCore->Codec) == XSSH_OK) &&
		xrtSshRekeyReset(&pCore->Rekey, 0u) &&
		xrtSshTransportCoreKexComplete(pCore);
}



/* 测量最终线路包发送事务与认证接收事务的完整无分配往返。 */
int main(int argc, char** argv)
{
	static const unsigned char arrPayload[] = {
		94u, 'x', 's', 's', 'h', '-', 'c', 'o', 'r', 'e'
	};
	unsigned char arrWire[96];
	unsigned char arrPlain[64];
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 1000000u);
	uint32 iPaddingState = UINT32_C(0x9e3779b9);
	xsshtransportcore Core;
	xbenchtimer Timer;
	uint64 iElapsed;
	uint64 iChecksum = 0u;
	uint32 i;

	if ( (iIterations == 0u) || !benchSshTransportCoreOpen(&Core) ) {
		return 1;
	}
	xbenchTimerStart(&Timer);
	for ( i = 0u; i < iIterations; ++i ) {
		xsshrekeydecision Decision;
		xsshpacketview Packet;
		xsshwriter Writer;
		xsshreader Reader;

		if ( !xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) ||
			(xrtSshTransportCoreWritePrepareWithPadding(
				&Core,
				&Writer,
				(xbytesview){ arrPayload, sizeof(arrPayload) },
				benchSshTransportCorePadding,
				&iPaddingState,
				(uint64)i
			) != XSSH_OK) || (xrtSshTransportCoreWriteCommit(
				&Core,
				(uint64)i,
				&Decision
			) != XSSH_OK) || !xrtSshReaderInit(
				&Reader,
				(xbytesview){ arrWire, Writer.Size }
			) || (xrtSshTransportCoreReadPrepare(
				&Core,
				&Reader,
				&Packet,
				arrPlain,
				sizeof(arrPlain),
				(uint64)i
			) != XSSH_OK) || (xrtSshTransportCoreReadCommit(
				&Core,
				(uint64)i,
				&Decision
			) != XSSH_OK) ) {
			xrtSshTransportCoreClear(&Core);
			return 2;
		}
		iChecksum += (uint64)Packet.Payload.Size +
			(uint64)Packet.Payload.Data[0] +
			(uint64)Core.Write.Message + (uint64)Core.Read.Message;
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);
	iChecksum += Core.Rekey.Sent.Packets + Core.Rekey.Received.Packets;
	xrtSshTransportCoreClear(&Core);

	printf("xssh transport core benchmark\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	xbenchPrintMetricDouble(
		"transport_core_roundtrips_per_sec",
		xbenchSafeRate(iIterations, iElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
