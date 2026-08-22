#include "../test.h"



#define TEST_SSH_KEX_INIT 30u
#define TEST_SSH_KEX_REPLY 31u
#define TEST_SSH_APPLICATION 94u



/* 测试 packet 使用确定性 padding，便于比较取消前后的线路字节。 */
static bool testSshTransportCorePadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	uint8 iValue = *(const uint8*)pUserData;
	bytes pBytes = (bytes)pOutput;
	size_t i;

	for ( i = 0u; i < iSize; ++i ) {
		pBytes[i] = (uint8)(iValue + (uint8)i);
	}
	return true;
}



/* 构建并解析一个完整初始 KEXINIT payload。 */
static bool testSshTransportCoreKexInit(
	xsshrole Role,
	void* pOutput,
	size_t iCapacity,
	xbytesview* pPayload,
	xsshkexinit* pKexInit
)
{
	unsigned char arrCookie[XSSH_KEX_COOKIE_SIZE] = { 0u };
	xsshkexinitconfig Config;
	xsshwriter Writer;

	arrCookie[0] = (uint8)Role;
	if ( !xrtSshKexInitConfigInit(&Config, Role, true) ||
		!xrtSshWriterInit(&Writer, pOutput, iCapacity) ||
		(xrtSshKexInitWrite(
			&Writer,
			(xbytesview){ arrCookie, sizeof(arrCookie) },
			&Config
		) != XSSH_OK) ) {
		return false;
	}
	pPayload->Data = (const unsigned char*)pOutput;
	pPayload->Size = Writer.Size;
	return xrtSshKexInitRead(*pPayload, pKexInit) == XSSH_OK;
}



/* 使用 peer codec 构建一包，模拟网络已经聚合出的完整线路输入。 */
static bool testSshTransportCorePeerWrite(
	xsshpacketcodec* pPeer,
	xbytesview Payload,
	void* pWire,
	size_t iCapacity,
	size_t* pWireSize,
	uint8* pPadding
)
{
	xsshwriter Writer;

	if ( !xrtSshWriterInit(&Writer, pWire, iCapacity) ||
		(xrtSshPacketCodecWriteWithPadding(
			pPeer,
			&Writer,
			Payload,
			testSshTransportCorePadding,
			pPadding
		) != XSSH_OK) ) {
		return false;
	}
	*pWireSize = Writer.Size;
	return true;
}



/* 准备并提交一包本端输出，模拟网络队列立即可靠接收。 */
static bool testSshTransportCoreLocalWrite(
	xsshtransportcore* pCore,
	xbytesview Payload,
	void* pWire,
	size_t iCapacity,
	uint64 iNowMs,
	uint8* pPadding
)
{
	xsshrekeydecision Decision;
	xsshwriter Writer;

	return xrtSshWriterInit(&Writer, pWire, iCapacity) &&
		(xrtSshTransportCoreWritePrepareWithPadding(
			pCore,
			&Writer,
			Payload,
			testSshTransportCorePadding,
			pPadding,
			iNowMs
		) == XSSH_OK) && (xrtSshTransportCoreWriteCommit(
			pCore,
			iNowMs,
			&Decision
		) == XSSH_OK) && (Decision != XSSH_REKEY_REQUIRED);
}



/* 解码并提交一包 peer 输入，模拟上层解析已经接受 payload。 */
static bool testSshTransportCorePeerRead(
	xsshtransportcore* pCore,
	const void* pWire,
	size_t iWireSize,
	void* pPlain,
	size_t iPlainCapacity,
	uint64 iNowMs,
	xsshpacketview* pPacket
)
{
	xsshrekeydecision Decision;
	xsshreader Reader;

	return xrtSshReaderInit(
		&Reader,
		(xbytesview){ (const unsigned char*)pWire, iWireSize }
	) && (xrtSshTransportCoreReadPrepare(
		pCore,
		&Reader,
		pPacket,
		pPlain,
		iPlainCapacity,
		iNowMs
	) == XSSH_OK) && (Reader.Position == iWireSize) &&
		(xrtSshTransportCoreReadCommit(
			pCore,
			iNowMs,
			&Decision
		) == XSSH_OK) && (Decision != XSSH_REKEY_REQUIRED);
}



/* 完成一轮 client strict KEX，并分别提交两个方向的新 cipher。 */
static void testSshTransportCoreOpen(
	xsshtransportcore* pCore,
	xsshpacketcodec* pPeer
)
{
	unsigned char arrLocalKex[512];
	unsigned char arrPeerKex[512];
	unsigned char arrWire[1024];
	unsigned char arrPlain[512];
	unsigned char arrKey[16] = { 0u };
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = { 0u };
	unsigned char arrMethodInit[] = { TEST_SSH_KEX_INIT };
	unsigned char arrMethodReply[] = { TEST_SSH_KEX_REPLY };
	unsigned char arrNewKeys[1];
	uint8 iPadding = 0x20u;
	xbytesview LocalPayload;
	xbytesview PeerPayload;
	xsshkexinit LocalKex;
	xsshkexinit PeerKex;
	xsshkexnegotiation Negotiation;
	xsshtransportkexrules Rules;
	xsshpacketview Packet;
	xsshwriter Writer;
	size_t iWireSize;

	testRequire(xrtSshTransportCoreInit(
		pCore,
		XSSH_ROLE_CLIENT,
		0u,
		NULL,
		0u
	), "ssh transport core init failed");
	testRequire(xrtSshPacketCodecInit(pPeer, 0u) == XSSH_OK,
		"ssh transport peer codec init failed");
	testRequire(testSshTransportCoreKexInit(
		XSSH_ROLE_CLIENT,
		arrLocalKex,
		sizeof(arrLocalKex),
		&LocalPayload,
		&LocalKex
	), "ssh transport local KEXINIT build failed");
	testRequire(testSshTransportCoreKexInit(
		XSSH_ROLE_SERVER,
		arrPeerKex,
		sizeof(arrPeerKex),
		&PeerPayload,
		&PeerKex
	), "ssh transport peer KEXINIT build failed");
	testRequire((xrtSshTransportCoreIdentificationCommit(
			pCore,
			XSSH_TRANSPORT_LOCAL
		) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
			pCore,
			XSSH_TRANSPORT_PEER
		) == XSSH_OK), "ssh transport identification commit failed");
	testRequire(testSshTransportCoreLocalWrite(
		pCore,
		LocalPayload,
		arrWire,
		sizeof(arrWire),
		1u,
		&iPadding
	) && testSshTransportCorePeerWrite(
		pPeer,
		PeerPayload,
		arrWire,
		sizeof(arrWire),
		&iWireSize,
		&iPadding
	) && testSshTransportCorePeerRead(
		pCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		2u,
		&Packet
	), "ssh transport core KEXINIT exchange failed");
	testRequire((xrtSshKexNegotiate(
		&LocalKex,
		&PeerKex,
		&Negotiation
	) == XSSH_OK) && xrtSshTransportKexRulesInit(&Rules) &&
		xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_LOCAL,
			TEST_SSH_KEX_INIT,
			1u
		) && xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_PEER,
			TEST_SSH_KEX_REPLY,
			1u
		) && (xrtSshTransportCoreKexConfigure(
			pCore,
			&LocalKex,
			&PeerKex,
			&Negotiation,
			&Rules
		) == XSSH_OK), "ssh transport core KEX configure failed");
	testRequire(testSshTransportCoreLocalWrite(
		pCore,
		(xbytesview){ arrMethodInit, sizeof(arrMethodInit) },
		arrWire,
		sizeof(arrWire),
		3u,
		&iPadding
	) && testSshTransportCorePeerWrite(
		pPeer,
		(xbytesview){ arrMethodReply, sizeof(arrMethodReply) },
		arrWire,
		sizeof(arrWire),
		&iWireSize,
		&iPadding
	) && testSshTransportCorePeerRead(
		pCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		4u,
		&Packet
	), "ssh transport core KEX methods failed");
	testRequire(xrtSshWriterInit(&Writer, arrNewKeys, sizeof(arrNewKeys)) &&
		(xrtSshNewKeysWrite(&Writer) == XSSH_OK) &&
		testSshTransportCoreLocalWrite(
			pCore,
			(xbytesview){ arrNewKeys, Writer.Size },
			arrWire,
			sizeof(arrWire),
			5u,
			&iPadding
		) && xrtSshTransportCoreWriteKeysPending(pCore) &&
		!xrtSshTransportCoreCanApplication(
			pCore,
			XSSH_TRANSPORT_LOCAL
		) && (xrtSshTransportCoreSetWriteAesGcm(
			pCore,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) },
			6u
		) == XSSH_OK) && (pCore->Codec.WriteSequence == 0u),
		"ssh transport core write NEWKEYS failed");
	testRequire(testSshTransportCorePeerWrite(
		pPeer,
		(xbytesview){ arrNewKeys, Writer.Size },
		arrWire,
		sizeof(arrWire),
		&iWireSize,
		&iPadding
	) && testSshTransportCorePeerRead(
		pCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		7u,
		&Packet
	) && xrtSshTransportCoreReadKeysPending(pCore) &&
		(xrtSshTransportCoreSetReadAesGcm(
			pCore,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) },
			8u
		) == XSSH_OK) && (pCore->Codec.ReadSequence == 0u) &&
		xrtSshTransportCoreKexComplete(pCore) &&
		(xrtSshPacketCodecSetWriteAesGcm(
			pPeer,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) }
		) == XSSH_OK) &&
		(xrtSshPacketCodecResetWriteSequence(pPeer) == XSSH_OK),
		"ssh transport core read NEWKEYS failed");
}



/* 验证背压取消、加密数据面和方向性 rekey 计数。 */
static void testSshTransportCoreDataPath(void)
{
	unsigned char arrLocalPayload[] = { TEST_SSH_APPLICATION, 'l' };
	unsigned char arrPeerPayload[] = { TEST_SSH_APPLICATION, 'p' };
	unsigned char arrFirst[128];
	unsigned char arrSecond[128];
	unsigned char arrPlain[64];
	uint8 iPadding = 0x70u;
	xsshtransportcore Core;
	xsshpacketcodec Peer;
	xsshwriter Writer;
	xsshreader Reader;
	xsshpacketview Packet;
	xsshrekeydecision Decision;
	size_t iWireSize;

	testSshTransportCoreOpen(&Core, &Peer);
	testRequire(xrtSshWriterInit(&Writer, arrFirst, sizeof(arrFirst)) &&
		(xrtSshTransportCoreWritePrepareWithPadding(
			&Core,
			&Writer,
			(xbytesview){ arrLocalPayload, sizeof(arrLocalPayload) },
			testSshTransportCorePadding,
			&iPadding,
			9u
		) == XSSH_OK) && Core.Write.Active && Core.Codec.WritePending &&
		(Core.Codec.WriteSequence == 0u) &&
		(Core.Rekey.Sent.Packets == 0u) &&
		(xrtSshTransportCoreWriteAbort(&Core) == XSSH_OK) &&
		!Core.Write.Active && !Core.Codec.WritePending &&
		(Core.Codec.WriteSequence == 0u) &&
		(Core.Rekey.Sent.Packets == 0u),
		"ssh transport core write abort consumed state");
	testRequire(xrtSshWriterInit(&Writer, arrSecond, sizeof(arrSecond)) &&
		(xrtSshTransportCoreWritePrepareWithPadding(
			&Core,
			&Writer,
			(xbytesview){ arrLocalPayload, sizeof(arrLocalPayload) },
			testSshTransportCorePadding,
			&iPadding,
			10u
		) == XSSH_OK) && (xrtSshTransportCoreWriteCommit(
			&Core,
			10u,
			&Decision
		) == XSSH_OK) && (Core.Codec.WriteSequence == 1u) &&
		(Core.Rekey.Sent.Packets == 1u),
		"ssh transport core encrypted write commit failed");
	testRequire(testSshTransportCorePeerWrite(
		&Peer,
		(xbytesview){ arrPeerPayload, sizeof(arrPeerPayload) },
		arrFirst,
		sizeof(arrFirst),
		&iWireSize,
		&iPadding
	) && xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrFirst, iWireSize - 1u }
	) && (xrtSshTransportCoreReadPrepare(
		&Core,
		&Reader,
		&Packet,
		arrPlain,
		sizeof(arrPlain),
		11u
	) == XSSH_NEED_MORE) && !Core.Read.Active &&
		xrtSshReaderInit(
			&Reader,
			(xbytesview){ arrFirst, iWireSize }
		) && (xrtSshTransportCoreReadPrepare(
			&Core,
			&Reader,
			&Packet,
			arrPlain,
			sizeof(arrPlain),
			11u
		) == XSSH_OK) && testSshBytesEqual(
			Packet.Payload,
			(xbytesview){ arrPeerPayload, sizeof(arrPeerPayload) }
		) && (xrtSshTransportCoreReadCommit(
			&Core,
			11u,
			&Decision
		) == XSSH_OK) && (Core.Codec.ReadSequence == 1u) &&
		(Core.Rekey.Received.Packets == 1u),
		"ssh transport core encrypted read failed");
	xrtSshPacketCodecClear(&Peer);
	xrtSshTransportCoreClear(&Core);
}



/* 验证已认证输入被上层拒绝后 transport 不会伪装回滚继续使用。 */
static void testSshTransportCoreReadAbortClose(void)
{
	unsigned char arrPayload[] = { TEST_SSH_APPLICATION, 'x' };
	unsigned char arrWire[128];
	unsigned char arrPlain[64];
	uint8 iPadding = 0x50u;
	xsshtransportcore Core;
	xsshpacketcodec Peer;
	xsshpacketview Packet;
	xsshreader Reader;
	size_t iWireSize;

	testSshTransportCoreOpen(&Core, &Peer);
	testRequire(testSshTransportCorePeerWrite(
		&Peer,
		(xbytesview){ arrPayload, sizeof(arrPayload) },
		arrWire,
		sizeof(arrWire),
		&iWireSize,
		&iPadding
	) && xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize }
	) && (xrtSshTransportCoreReadPrepare(
		&Core,
		&Reader,
		&Packet,
		arrPlain,
		sizeof(arrPlain),
		9u
	) == XSSH_OK) && (xrtSshTransportCoreReadAbort(&Core) == XSSH_OK) &&
		(Core.State.Phase == XSSH_TRANSPORT_CLOSED) &&
		!xrtSshTransportCoreCanApplication(
			&Core,
			XSSH_TRANSPORT_PEER
		), "ssh transport core read abort did not close state");
	xrtSshPacketCodecClear(&Peer);
	xrtSshTransportCoreClear(&Core);
}



/* 验证认证失败和畸形特殊消息都会终止不可恢复的线路状态。 */
static void testSshTransportCoreFatalInput(void)
{
	unsigned char arrApplication[] = { TEST_SSH_APPLICATION, 'x' };
	unsigned char arrBadSuccess[] = { XSSH_MSG_USERAUTH_SUCCESS, 0u };
	unsigned char arrWire[128];
	unsigned char arrPlain[64];
	uint8 iPadding = 0x30u;
	xsshtransportcore Core;
	xsshpacketcodec Peer;
	xsshpacketview Packet;
	xsshreader Reader;
	size_t iWireSize;

	testSshTransportCoreOpen(&Core, &Peer);
	testRequire(testSshTransportCorePeerWrite(
		&Peer,
		(xbytesview){ arrApplication, sizeof(arrApplication) },
		arrWire,
		sizeof(arrWire),
		&iWireSize,
		&iPadding
	), "ssh transport core authentication setup failed");
	arrWire[iWireSize - 1u] ^= 1u;
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize }
	) && (xrtSshTransportCoreReadPrepare(
		&Core,
		&Reader,
		&Packet,
		arrPlain,
		sizeof(arrPlain),
		9u
	) == XSSH_ERROR_AUTHENTICATION) &&
		(Core.State.Phase == XSSH_TRANSPORT_CLOSED),
		"ssh transport core authentication failure remained open");
	xrtSshPacketCodecClear(&Peer);
	xrtSshTransportCoreClear(&Core);

	testSshTransportCoreOpen(&Core, &Peer);
	testRequire(testSshTransportCorePeerWrite(
		&Peer,
		(xbytesview){ arrBadSuccess, sizeof(arrBadSuccess) },
		arrWire,
		sizeof(arrWire),
		&iWireSize,
		&iPadding
	) && xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize }
	) && (xrtSshTransportCoreReadPrepare(
		&Core,
		&Reader,
		&Packet,
		arrPlain,
		sizeof(arrPlain),
		9u
	) == XSSH_ERROR_PROTOCOL) && (Reader.Position == 0u) &&
		(Core.State.Phase == XSSH_TRANSPORT_CLOSED),
		"ssh transport core malformed auth success remained open");
	xrtSshPacketCodecClear(&Peer);
	xrtSshTransportCoreClear(&Core);
}



/* 运行无网络 transport core 的背压、密钥和接收事务测试。 */
int main(void)
{
	testSshTransportCoreDataPath();
	testSshTransportCoreReadAbortClose();
	testSshTransportCoreFatalInput();
	return 0;
}
