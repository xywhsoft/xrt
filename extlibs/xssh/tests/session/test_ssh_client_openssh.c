#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xssh.h>



#define TEST_SSH_LIVE_OUTPUT_CAPACITY 16384u
#define TEST_SSH_LIVE_EXEC_TOKEN UINT64_C(0x6c6976650001)
#define TEST_SSH_LIVE_PTY_TOKEN UINT64_C(0x6c6976650002)
#define TEST_SSH_LIVE_SHELL_TOKEN UINT64_C(0x6c6976650003)
#define TEST_SSH_LIVE_STRESS_TOKEN_BASE UINT64_C(0x6c6976651000)
#define TEST_SSH_LIVE_STRESS_CHANNELS_DEFAULT 8u
#define TEST_SSH_LIVE_STRESS_CHANNELS_MAXIMUM 64u
#define TEST_SSH_LIVE_STRESS_COMMAND_CAPACITY 512u
#define TEST_SSH_LIVE_STRESS_MARKER_CAPACITY 64u
#define TEST_SSH_LIVE_STRESS_TAIL_CAPACITY 128u
#define TEST_SSH_LIVE_STRESS_MINIMUM_BYTES 8192u



/* 真实 OpenSSH 测试按 exec、可选 PTY 和可选 direct-tcpip 顺序复用一条连接。 */
typedef enum testsshlivephase {
	TEST_SSH_LIVE_EXEC_OPEN = 0,
	TEST_SSH_LIVE_EXEC_ACTIVE = 1,
	TEST_SSH_LIVE_PTY_OPEN = 2,
	TEST_SSH_LIVE_PTY_REQUEST = 3,
	TEST_SSH_LIVE_PTY_SHELL = 4,
	TEST_SSH_LIVE_FORWARD_OPEN = 5,
	TEST_SSH_LIVE_FORWARD_ACTIVE = 6,
	TEST_SSH_LIVE_STRESS_ACTIVE = 7,
	TEST_SSH_LIVE_DONE = 8
} testsshlivephase;



/* 私钥身份借用解码后的敏感二进制，直到 SSH 客户端完全关闭。 */
typedef struct testsshliveidentity {
	unsigned char* Binary;
	size_t BinarySize;
	xsshed25519identity Identity;
} testsshliveidentity;



/* 每条真实并发 channel 只保存稳定身份、计数和末尾证据，不缓存完整正文。 */
typedef struct testsshlivestress {
	xsshchannel* Channel;
	char Command[TEST_SSH_LIVE_STRESS_COMMAND_CAPACITY];
	char Marker[TEST_SSH_LIVE_STRESS_MARKER_CAPACITY];
	unsigned char Tail[TEST_SSH_LIVE_STRESS_TAIL_CAPACITY];
	size_t Bytes;
	size_t TailSize;
	uint32 ExitStatus;
	bool Reply;
	bool ExitReceived;
	bool Closed;
} testsshlivestress;



/* 所有 SSH 与网络对象只由 Engine Worker 驱动，主线程只观察原子终态。 */
typedef struct testsshlive {
	xsshclient Client;
	xnetengine* Engine;
	xnetresolver* Resolver;
	xnetdial* Dial;
	xnetstream* Stream;
	xnetpost PtyPost;
	xsshchannel* PtyChannel;
	testsshliveidentity Key;
	testsshlivestress* Stress;
	xstrview Password;
	const char* ExpectedFingerprint;
	const char* ExecCommand;
	const char* ExecExpected;
	const char* PtyCommand;
	const char* PtyExpected;
	const char* ForwardHost;
	const char* ForwardSend;
	const char* ForwardExpected;
	uint32 ForwardPort;
	unsigned char ExecOutput[TEST_SSH_LIVE_OUTPUT_CAPACITY];
	unsigned char ExecError[TEST_SSH_LIVE_OUTPUT_CAPACITY];
	unsigned char PtyOutput[TEST_SSH_LIVE_OUTPUT_CAPACITY];
	unsigned char ForwardOutput[TEST_SSH_LIVE_OUTPUT_CAPACITY];
	size_t ExecOutputSize;
	size_t ExecErrorSize;
	size_t PtyOutputSize;
	size_t ForwardOutputSize;
	size_t StressCount;
	size_t StressSubmitted;
	size_t StressClosed;
	uint32 ExecExitStatus;
	uint32 PtyExitStatus;
	testsshlivephase Phase;
	xatomic32 DialDone;
	xatomic32 Done;
	xatomic32 Errors;
	xatomic32 HostChecks;
	bool AcceptNew;
	bool ExecReply;
	bool ExecExitReceived;
	bool PtyReply;
	bool ShellReply;
	bool PtyCommandPending;
	bool PtyResizeSubmitting;
	bool PtyExitReceived;
	bool ForwardEof;
} testsshlive;



/* 判断环境变量是否显式启用布尔开关。 */
static bool testSshLiveEnabled(const char* sValue)
{
	return (sValue != NULL) &&
		((strcmp(sValue, "1") == 0) ||
		 (strcmp(sValue, "true") == 0) ||
		 (strcmp(sValue, "yes") == 0));
}



/* 严格读取无符号端口或超时值。 */
static bool testSshLiveUint(
	const char* sValue,
	uint32 iDefault,
	uint32 iMaximum,
	uint32* pValue
)
{
	char* pEnd = NULL;
	unsigned long iParsed;

	if ( pValue == NULL ) {
		return false;
	}
	if ( (sValue == NULL) || (sValue[0] == '\0') ) {
		*pValue = iDefault;
		return true;
	}
	iParsed = strtoul(sValue, &pEnd, 10);
	if ( (pEnd == sValue) || (*pEnd != '\0') ||
		(iParsed == 0ul) || (iParsed > (unsigned long)iMaximum) ) {
		return false;
	}
	*pValue = (uint32)iParsed;
	return true;
}



/* 在固定证据缓冲中追加数据，超限时保持已有前缀。 */
static bool testSshLiveAppend(
	unsigned char* pOutput,
	size_t* pOutputSize,
	const unsigned char* pData,
	size_t iSize
)
{
	if ( (pOutput == NULL) || (pOutputSize == NULL) ||
		((iSize != 0u) && (pData == NULL)) ||
		(*pOutputSize > TEST_SSH_LIVE_OUTPUT_CAPACITY) ||
		(iSize > (TEST_SSH_LIVE_OUTPUT_CAPACITY - *pOutputSize)) ) {
		return false;
	}
	if ( iSize != 0u ) {
		memcpy(pOutput + *pOutputSize, pData, iSize);
		*pOutputSize += iSize;
	}
	return true;
}



/* 在二进制输出中查找期望文本，不要求远端补零。 */
static bool testSshLiveContains(
	const unsigned char* pData,
	size_t iSize,
	const char* sExpected
)
{
	size_t i;
	size_t iExpected;

	if ( (pData == NULL) || (sExpected == NULL) ) {
		return false;
	}
	iExpected = strlen(sExpected);
	if ( iExpected == 0u ) {
		return true;
	}
	if ( iExpected > iSize ) {
		return false;
	}
	for ( i = 0u; i <= (iSize - iExpected); ++i ) {
		if ( memcmp(pData + i, sExpected, iExpected) == 0 ) {
			return true;
		}
	}
	return false;
}



/* 按稳定 channel 地址查找真实并发场景槽位。 */
static testsshlivestress* testSshLiveStressByChannel(
	testsshlive* pLive,
	const xsshchannel* pChannel
)
{
	size_t i;

	if ( (pLive == NULL) || (pChannel == NULL) ) {
		return NULL;
	}
	for ( i = 0u; i < pLive->StressSubmitted; ++i ) {
		if ( pLive->Stress[i].Channel == pChannel ) {
			return &pLive->Stress[i];
		}
	}
	return NULL;
}



/* 按本端 channel id 关联 exit-status packet。 */
static testsshlivestress* testSshLiveStressByLocal(
	testsshlive* pLive,
	uint32 iLocal
)
{
	size_t i;

	if ( pLive == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < pLive->StressSubmitted; ++i ) {
		if ( (pLive->Stress[i].Channel != NULL) &&
			(pLive->Stress[i].Channel->Core.Local == iLocal) ) {
			return &pLive->Stress[i];
		}
	}
	return NULL;
}



/* 按唯一 reply token 关联并发 exec 请求。 */
static testsshlivestress* testSshLiveStressByToken(
	testsshlive* pLive,
	uint64 iToken
)
{
	uint64 iIndex;

	if ( (pLive == NULL) || (iToken < TEST_SSH_LIVE_STRESS_TOKEN_BASE) ) {
		return NULL;
	}
	iIndex = iToken - TEST_SSH_LIVE_STRESS_TOKEN_BASE;
	if ( iIndex >= pLive->StressCount ) {
		return NULL;
	}
	return &pLive->Stress[(size_t)iIndex];
}



/* 保留最后一小段正文，用于跨 packet 检查每条 channel 的结束标记。 */
static void testSshLiveStressTail(
	testsshlivestress* pStress,
	const unsigned char* pData,
	size_t iSize
)
{
	size_t iKeep;

	if ( (pStress == NULL) || ((iSize != 0u) && (pData == NULL)) ) {
		return;
	}
	if ( iSize >= TEST_SSH_LIVE_STRESS_TAIL_CAPACITY ) {
		memcpy(
			pStress->Tail,
			pData + iSize - TEST_SSH_LIVE_STRESS_TAIL_CAPACITY,
			TEST_SSH_LIVE_STRESS_TAIL_CAPACITY
		);
		pStress->TailSize = TEST_SSH_LIVE_STRESS_TAIL_CAPACITY;
		return;
	}
	iKeep = pStress->TailSize;
	if ( iKeep > (TEST_SSH_LIVE_STRESS_TAIL_CAPACITY - iSize) ) {
		iKeep = TEST_SSH_LIVE_STRESS_TAIL_CAPACITY - iSize;
		memmove(
			pStress->Tail,
			pStress->Tail + pStress->TailSize - iKeep,
			iKeep
		);
	}
	if ( iSize != 0u ) {
		memcpy(pStress->Tail + iKeep, pData, iSize);
	}
	pStress->TailSize = iKeep + iSize;
}



/* 记录唯一失败原因并把当前连接推进到异常关闭终态。 */
static void testSshLiveFail(testsshlive* pLive, const char* sMessage)
{
	if ( pLive == NULL ) {
		return;
	}
	if ( xrtAtomic32FetchAdd(&pLive->Errors, 1u, XMEMORY_ACQ_REL) == 0u ) {
		fprintf(stderr, "[FAIL] xssh OpenSSH interop: %s\n", sMessage);
	}
	if ( xrtSshClientIsCurrent(&pLive->Client) ) {
		(void)xrtSshClientAbort(&pLive->Client);
	}
}



/* 完成最后一个 channel 后主动关闭底层 TCP，等待统一 Close 回调。 */
static void testSshLiveFinish(testsshlive* pLive)
{
	xnetstream* pStream;

	if ( (pLive == NULL) || (pLive->Phase == TEST_SSH_LIVE_DONE) ) {
		return;
	}
	pLive->Phase = TEST_SSH_LIVE_DONE;
	pStream = xrtSshSessionStreamTcp(xrtSshClientStream(&pLive->Client));
	if ( (pStream == NULL) || !xrtNetStreamClose(pStream) ) {
		testSshLiveFail(pLive, "TCP close failed");
	}
}



/* 主机密钥必须匹配固定指纹，或由测试操作者显式接受新密钥。 */
static xsshclienthostdecision testSshLiveHostKey(
	xsshclientcore* pClient,
	const xsshclienthost* pHost,
	ptr pData
)
{
	testsshlive* pLive = (testsshlive*)pData;
	char sFingerprint[64];
	size_t iFingerprintSize = 0u;
	xsshcode Code;

	(void)pClient;
	(void)xrtAtomic32FetchAdd(&pLive->HostChecks, 1u, XMEMORY_RELEASE);
	Code = xrtSshHostKeyFingerprintSha256(
		pHost->Key,
		sFingerprint,
		sizeof(sFingerprint),
		&iFingerprintSize
	);
	if ( Code != XSSH_OK ) {
		return XSSH_CLIENT_HOST_REJECT;
	}
	if ( pLive->ExpectedFingerprint != NULL ) {
		return strcmp(sFingerprint, pLive->ExpectedFingerprint) == 0 ?
			XSSH_CLIENT_HOST_ACCEPT : XSSH_CLIENT_HOST_REJECT;
	}
	if ( pLive->AcceptNew ) {
		fprintf(stderr, "[NOTICE] xssh OpenSSH host fingerprint: %s\n",
			sFingerprint);
		return XSSH_CLIENT_HOST_ACCEPT;
	}
	return XSSH_CLIENT_HOST_REJECT;
}



/* 认证完成后打开第一条 exec session channel。 */
static void testSshLiveReady(xsshclient* pClient, ptr pData)
{
	testsshlive* pLive = (testsshlive*)pData;
	xsshchannel* pChannel = NULL;

	pLive->Phase = TEST_SSH_LIVE_EXEC_OPEN;
	if ( (xrtSshClientSessionOpen(pClient, &pChannel) != XSSH_OK) ||
		(pChannel == NULL) ) {
		testSshLiveFail(pLive, "exec channel open submission failed");
	}
}



/* 在 packet 提交前保存 exit-status，并把 exit-signal 视为失败。 */
static xsshsessionstreamdecision testSshLivePacket(
	xsshclient* pClient,
	const xsshsessiontcppacket* pPacket,
	ptr pData
)
{
	testsshlive* pLive = (testsshlive*)pData;
	testsshlivestress* pStress;
	const xsshconnectionpacket* pConnection;
	uint32 iStatus;
	xsshchannelexitsignal Signal;

	(void)pClient;
	if ( pPacket->Session.Kind != XSSH_SESSION_PACKET_CONNECTION ) {
		return XSSH_SESSION_STREAM_ACCEPT;
	}
	pConnection = &pPacket->Session.Message.Connection;
	if ( pConnection->Kind != XSSH_CONNECTION_PACKET_CHANNEL_REQUEST ) {
		return XSSH_SESSION_STREAM_ACCEPT;
	}
	if ( xrtSshChannelExitStatusRead(
		&pConnection->Message.ChannelRequest,
		&iStatus
	) == XSSH_OK ) {
		if ( pLive->Phase == TEST_SSH_LIVE_EXEC_ACTIVE ) {
			pLive->ExecExitStatus = iStatus;
			pLive->ExecExitReceived = true;
		} else if ( pLive->Phase == TEST_SSH_LIVE_PTY_SHELL ) {
			pLive->PtyExitStatus = iStatus;
			pLive->PtyExitReceived = true;
		} else if ( pLive->Phase == TEST_SSH_LIVE_STRESS_ACTIVE ) {
			pStress = testSshLiveStressByLocal(
				pLive,
				pConnection->Message.ChannelRequest.Recipient
			);
			if ( pStress == NULL ) {
				testSshLiveFail(pLive, "stress exit-status channel is unknown");
			} else {
				pStress->ExitStatus = iStatus;
				pStress->ExitReceived = true;
			}
		}
	} else if ( xrtSshChannelExitSignalRead(
		&pConnection->Message.ChannelRequest,
		&Signal
	) == XSSH_OK ) {
		(void)Signal;
		testSshLiveFail(pLive, "remote command exited by signal");
	}
	return XSSH_SESSION_STREAM_ACCEPT;
}



/* 小块延迟消费真实并发正文，并显式恢复接收窗口。 */
static void testSshLiveStressData(
	testsshlive* pLive,
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshchanneliostream Stream
)
{
	testsshlivestress* pStress;
	unsigned char arrData[256];
	size_t iRead;
	xsshcode Code;

	pStress = testSshLiveStressByChannel(pLive, pChannel);
	if ( (pStress == NULL) || (Stream != XSSH_CHANNEL_IO_DATA) ) {
		testSshLiveFail(pLive, "stress DATA channel or stream is invalid");
		return;
	}
	for ( ;; ) {
		iRead = 0u;
		Code = xrtSshChannelIoRead(
			&pChannel->Io,
			Stream,
			arrData,
			sizeof(arrData),
			&iRead
		);
		if ( Code != XSSH_OK ) {
			testSshLiveFail(pLive, "stress channel DATA read failed");
			return;
		}
		if ( iRead == 0u ) {
			break;
		}
		pStress->Bytes += iRead;
		testSshLiveStressTail(pStress, arrData, iRead);
		xrtSleep(1u);
	}
	Code = xrtSshClientChannelAdjust(pClient, pChannel);
	if ( (Code != XSSH_OK) && (Code != XSSH_NEED_MORE) ) {
		testSshLiveFail(pLive, "stress channel window adjust failed");
	}
}



/* 把已提交 channel 数据完整取出，并按当前场景保存独立证据。 */
static void testSshLiveData(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshchanneliostream Stream,
	ptr pData
)
{
	testsshlive* pLive = (testsshlive*)pData;
	unsigned char arrData[1024];
	unsigned char* pOutput;
	size_t* pOutputSize;
	size_t iRead;
	xsshcode Code;

	if ( pLive->Phase == TEST_SSH_LIVE_STRESS_ACTIVE ) {
		testSshLiveStressData(pLive, pClient, pChannel, Stream);
		return;
	}

	if ( pLive->Phase == TEST_SSH_LIVE_EXEC_ACTIVE ) {
		pOutput = Stream == XSSH_CHANNEL_IO_STDERR ?
			pLive->ExecError : pLive->ExecOutput;
		pOutputSize = Stream == XSSH_CHANNEL_IO_STDERR ?
			&pLive->ExecErrorSize : &pLive->ExecOutputSize;
	} else if ( pLive->Phase == TEST_SSH_LIVE_PTY_SHELL ) {
		pOutput = pLive->PtyOutput;
		pOutputSize = &pLive->PtyOutputSize;
	} else if ( pLive->Phase == TEST_SSH_LIVE_FORWARD_ACTIVE ) {
		pOutput = pLive->ForwardOutput;
		pOutputSize = &pLive->ForwardOutputSize;
	} else {
		testSshLiveFail(pLive, "DATA arrived in an invalid phase");
		return;
	}
	for ( ;; ) {
		iRead = 0u;
		Code = xrtSshChannelIoRead(
			&pChannel->Io,
			Stream,
			arrData,
			sizeof(arrData),
			&iRead
		);
		if ( Code != XSSH_OK ) {
			testSshLiveFail(pLive, "channel DATA read failed");
			return;
		}
		if ( iRead == 0u ) {
			break;
		}
		if ( !testSshLiveAppend(pOutput, pOutputSize, arrData, iRead) ) {
			testSshLiveFail(pLive, "channel evidence output exceeded 16 KiB");
			return;
		}
	}
	if ( (pLive->Phase == TEST_SSH_LIVE_FORWARD_ACTIVE) &&
		!pLive->ForwardEof && testSshLiveContains(
			pLive->ForwardOutput,
			pLive->ForwardOutputSize,
			pLive->ForwardExpected
		) ) {
		pLive->ForwardEof = true;
		if ( xrtSshClientChannelEof(pClient, pChannel) != XSSH_OK ) {
			testSshLiveFail(pLive, "direct-tcpip EOF failed");
		}
	}
}



/* 离开网络回调栈后，再提交 PTY 命令和 shell 退出输入。 */
static void testSshLivePtySubmit(xnetworker* pWorker, ptr pData)
{
	testsshlive* pLive = (testsshlive*)pData;
	xsshchannel* pChannel;
	xsshcode Code;

	(void)pWorker;
	if ( pLive == NULL ) {
		return;
	}
	pChannel = pLive->PtyChannel;
	pLive->PtyChannel = NULL;
	if ( (pLive->Phase != TEST_SSH_LIVE_PTY_SHELL) ||
		(pChannel == NULL) || !xrtSshClientOwnsChannel(
			&pLive->Client,
			pChannel
		) ) {
		testSshLiveFail(pLive, "PTY command drain state is invalid");
		return;
	}
	Code = xrtSshChannelIoWrite(
		&pChannel->Io,
		XSSH_CHANNEL_IO_DATA,
		pLive->PtyCommand,
		strlen(pLive->PtyCommand)
	);
	if ( Code != XSSH_OK ) {
		testSshLiveFail(pLive, "PTY command buffer failed");
		return;
	}
	Code = xrtSshChannelIoWrite(
		&pChannel->Io,
		XSSH_CHANNEL_IO_DATA,
		"\nexit\n",
		6u
	);
	if ( Code != XSSH_OK ) {
		testSshLiveFail(pLive, "PTY exit buffer failed");
		return;
	}
	if ( xrtSshClientChannelFlush(
		&pLive->Client,
		pChannel,
		XSSH_CHANNEL_IO_DATA
	) != XSSH_OK ) {
		testSshLiveFail(pLive, "PTY command flush failed");
	}
}



/* resize 排空后把下一条写事务投递回同一 Worker，避免同步回调重入。 */
static void testSshLiveDrain(xsshclient* pClient, ptr pData)
{
	testsshlive* pLive = (testsshlive*)pData;
	xnetworker* pWorker;

	if ( (pLive == NULL) || !pLive->PtyCommandPending ||
		pLive->PtyResizeSubmitting ) {
		return;
	}
	pWorker = xrtNetStreamWorker(pLive->Stream);
	if ( (pWorker == NULL) || !xrtNetPost(
		pWorker,
		&pLive->PtyPost,
		testSshLivePtySubmit,
		pLive
	) ) {
		testSshLiveFail(pLive, "PTY command post failed");
		return;
	}
	pLive->PtyCommandPending = false;
	(void)pClient;
}



/* 顺序提交 channel open，已开始的远端命令继续并发运行。 */
static void testSshLiveStressOpenNext(testsshlive* pLive)
{
	testsshlivestress* pStress;
	xsshchannel* pChannel = NULL;
	xsshcode Code;

	if ( (pLive == NULL) || (pLive->StressSubmitted >= pLive->StressCount) ) {
		return;
	}
	pStress = &pLive->Stress[pLive->StressSubmitted];
	Code = xrtSshClientSessionOpen(&pLive->Client, &pChannel);
	if ( (Code != XSSH_OK) || (pChannel == NULL) ) {
		testSshLiveFail(pLive, "stress channel open submission failed");
		return;
	}
	pStress->Channel = pChannel;
	pLive->StressSubmitted += 1u;
}



/* 打开可选 PTY 或 direct-tcpip 场景；没有后续场景时结束连接。 */
static void testSshLiveNext(testsshlive* pLive, testsshlivephase Completed)
{
	xsshchannel* pChannel = NULL;
	xsshcode Code;

	if ( (Completed == TEST_SSH_LIVE_EXEC_ACTIVE) &&
		(pLive->PtyCommand != NULL) ) {
		pLive->Phase = TEST_SSH_LIVE_PTY_OPEN;
		Code = xrtSshClientSessionOpen(&pLive->Client, &pChannel);
	} else if ( (Completed != TEST_SSH_LIVE_FORWARD_ACTIVE) &&
		(pLive->ForwardHost != NULL) ) {
		pLive->Phase = TEST_SSH_LIVE_FORWARD_OPEN;
		Code = xrtSshClientDirectTcpipOpen(
			&pLive->Client,
			(xbytesview){
				(const unsigned char*)pLive->ForwardHost,
				strlen(pLive->ForwardHost)
			},
			pLive->ForwardPort,
			XRT_BYTES_LITERAL("127.0.0.1"),
			0u,
			&pChannel
		);
	} else if ( pLive->StressSubmitted == 0u ) {
		pLive->Phase = TEST_SSH_LIVE_STRESS_ACTIVE;
		testSshLiveStressOpenNext(pLive);
		return;
	} else {
		testSshLiveFinish(pLive);
		return;
	}
	if ( (Code != XSSH_OK) || (pChannel == NULL) ) {
		testSshLiveFail(pLive, "next channel open submission failed");
	}
}



/* Channel 通知驱动 exec、PTY shell、direct-tcpip 与确定性关闭。 */
static void testSshLiveChannel(
	xsshclient* pClient,
	const xsshclientchannelnotice* pNotice,
	ptr pData
)
{
	testsshlive* pLive = (testsshlive*)pData;
	testsshlivestress* pStress;
	testsshlivephase Completed;
	unsigned char arrModes[8];
	xsshwriter ModeWriter;
	uint32 iLocal;
	xsshcode Code;

	if ( pNotice->Incoming ) {
		testSshLiveFail(pLive, "unexpected incoming channel");
		return;
	}
	if ( pNotice->Event == XSSH_CLIENT_CHANNEL_EVENT_OPENED ) {
		if ( pLive->Phase == TEST_SSH_LIVE_STRESS_ACTIVE ) {
			pStress = testSshLiveStressByChannel(pLive, pNotice->Channel);
			if ( pStress == NULL ) {
				testSshLiveFail(pLive, "opened stress channel is unknown");
				return;
			}
			Code = xrtSshClientSessionExec(
				pClient,
				pNotice->Channel,
				(xbytesview){
					(const unsigned char*)pStress->Command,
					strlen(pStress->Command)
				},
				true,
				TEST_SSH_LIVE_STRESS_TOKEN_BASE +
					(uint64)(pStress - pLive->Stress)
			);
		} else if ( pLive->Phase == TEST_SSH_LIVE_EXEC_OPEN ) {
			pLive->Phase = TEST_SSH_LIVE_EXEC_ACTIVE;
			Code = xrtSshClientSessionExec(
				pClient,
				pNotice->Channel,
				(xbytesview){
					(const unsigned char*)pLive->ExecCommand,
					strlen(pLive->ExecCommand)
				},
				true,
				TEST_SSH_LIVE_EXEC_TOKEN
			);
		} else if ( pLive->Phase == TEST_SSH_LIVE_PTY_OPEN ) {
			pLive->Phase = TEST_SSH_LIVE_PTY_REQUEST;
			Code = xrtSshWriterInit(
				&ModeWriter,
				arrModes,
				sizeof(arrModes)
			) && (xrtSshTerminalModeWrite(
				&ModeWriter,
				XSSH_TTY_OP_ECHO,
				0u
			) == XSSH_OK) && (xrtSshTerminalModeEnd(&ModeWriter) == XSSH_OK) ?
				xrtSshClientSessionPty(
					pClient,
					pNotice->Channel,
					XRT_BYTES_LITERAL("xterm-256color"),
					80u,
					24u,
					0u,
					0u,
					(xbytesview){ arrModes, ModeWriter.Size },
					true,
					TEST_SSH_LIVE_PTY_TOKEN
				) : XSSH_ERROR_STATE;
		} else if ( pLive->Phase == TEST_SSH_LIVE_FORWARD_OPEN ) {
			pLive->Phase = TEST_SSH_LIVE_FORWARD_ACTIVE;
			Code = xrtSshChannelIoWrite(
				&pNotice->Channel->Io,
				XSSH_CHANNEL_IO_DATA,
				pLive->ForwardSend,
				strlen(pLive->ForwardSend)
			);
			if ( Code == XSSH_OK ) {
				Code = xrtSshClientChannelFlush(
					pClient,
					pNotice->Channel,
					XSSH_CHANNEL_IO_DATA
				);
			}
		} else {
			testSshLiveFail(pLive, "channel opened in an invalid phase");
			return;
		}
		if ( Code != XSSH_OK ) {
			testSshLiveFail(pLive, "channel start operation failed");
		}
		return;
	}
	if ( pNotice->Event == XSSH_CLIENT_CHANNEL_EVENT_REQUEST_SUCCESS ) {
		if ( !pNotice->HasReplyToken ) {
			testSshLiveFail(pLive, "request success missed its reply token");
			return;
		}
		pStress = testSshLiveStressByToken(pLive, pNotice->ReplyToken);
		if ( (pLive->Phase == TEST_SSH_LIVE_STRESS_ACTIVE) &&
			(pStress != NULL) ) {
			pStress->Reply = true;
			testSshLiveStressOpenNext(pLive);
			return;
		}
		if ( (pLive->Phase == TEST_SSH_LIVE_EXEC_ACTIVE) &&
			(pNotice->ReplyToken == TEST_SSH_LIVE_EXEC_TOKEN) ) {
			pLive->ExecReply = true;
			return;
		}
		if ( (pLive->Phase == TEST_SSH_LIVE_PTY_REQUEST) &&
			(pNotice->ReplyToken == TEST_SSH_LIVE_PTY_TOKEN) ) {
			pLive->PtyReply = true;
			pLive->Phase = TEST_SSH_LIVE_PTY_SHELL;
			if ( xrtSshClientSessionShell(
				pClient,
				pNotice->Channel,
				true,
				TEST_SSH_LIVE_SHELL_TOKEN
			) != XSSH_OK ) {
				testSshLiveFail(pLive, "shell request failed");
			}
			return;
		}
		if ( (pLive->Phase == TEST_SSH_LIVE_PTY_SHELL) &&
			(pNotice->ReplyToken == TEST_SSH_LIVE_SHELL_TOKEN) ) {
			pLive->ShellReply = true;
			pLive->PtyChannel = pNotice->Channel;
			pLive->PtyCommandPending = true;
			pLive->PtyResizeSubmitting = true;
			Code = xrtSshClientSessionResize(
				pClient,
				pNotice->Channel,
				100u,
				30u,
				0u,
				0u
			);
			pLive->PtyResizeSubmitting = false;
			if ( Code != XSSH_OK ) {
				pLive->PtyChannel = NULL;
				pLive->PtyCommandPending = false;
				testSshLiveFail(pLive, "PTY resize submission failed");
			}
			return;
		}
		testSshLiveFail(pLive, "unexpected channel reply token");
		return;
	}
	if ( (pNotice->Event == XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED) ||
		(pNotice->Event == XSSH_CLIENT_CHANNEL_EVENT_REQUEST_FAILURE) ) {
		testSshLiveFail(pLive, "OpenSSH rejected a channel operation");
		return;
	}
	if ( pNotice->Event == XSSH_CLIENT_CHANNEL_EVENT_EOF ) {
		if ( xrtSshClientChannelClose(pClient, pNotice->Channel) != XSSH_OK ) {
			testSshLiveFail(pLive, "channel close response failed");
		}
		return;
	}
	if ( pNotice->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED ) {
		if ( pLive->Phase == TEST_SSH_LIVE_STRESS_ACTIVE ) {
			pStress = testSshLiveStressByChannel(pLive, pNotice->Channel);
			if ( (pStress == NULL) || pStress->Closed ) {
				testSshLiveFail(pLive, "closed stress channel is invalid");
				return;
			}
			iLocal = pNotice->Channel->Core.Local;
			pStress->Closed = true;
			pStress->Channel = NULL;
			pLive->StressClosed += 1u;
			if ( !xrtSshChannelsDiscard(
				xrtSshClientChannels(pClient),
				iLocal
			) ) {
				testSshLiveFail(pLive, "closed stress channel discard failed");
				return;
			}
			if ( pLive->StressClosed == pLive->StressCount ) {
				testSshLiveFinish(pLive);
			}
			return;
		}
		Completed = pLive->Phase;
		iLocal = pNotice->Channel->Core.Local;
		if ( !xrtSshChannelsDiscard(
			xrtSshClientChannels(pClient),
			iLocal
		) ) {
			testSshLiveFail(pLive, "closed channel discard failed");
			return;
		}
		testSshLiveNext(pLive, Completed);
	}
}



/* 协议错误保留结构化诊断，Close 回调仍负责发布主线程终态。 */
static void testSshLiveError(
	xsshclient* pClient,
	xsshcode Code,
	const xerror* pError,
	ptr pData
)
{
	testsshlive* pLive = (testsshlive*)pData;

	(void)pClient;
	fprintf(stderr,
		"[SSH ERROR] code=%d domain=%s operation=%s message=%s\n",
		(int)Code,
		pError != NULL ? xrtErrorDomain(pError) : "",
		pError != NULL ? xrtErrorOperation(pError) : "",
		pError != NULL ? xrtErrorMessage(pError) : "");
	(void)xrtAtomic32FetchAdd(&pLive->Errors, 1u, XMEMORY_RELEASE);
}



/* SSH/TCP 关闭后发布唯一完成信号。 */
static void testSshLiveClose(
	xsshclient* pClient,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testsshlive* pLive = (testsshlive*)pData;

	(void)pClient;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(&pLive->Done, 1u, XMEMORY_RELEASE);
}



/* Dial 完成只保存 Stream 所有权；SSH Ready 由协议回调独立证明。 */
static void testSshLiveDialDone(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	testsshlive* pLive = (testsshlive*)pData;

	(void)pDial;
	pLive->Stream = pStream;
	if ( (Result != XNET_RESULT_OK) || (pStream == NULL) ) {
		fprintf(stderr,
			"[DIAL ERROR] result=%d domain=%s operation=%s message=%s\n",
			(int)Result,
			pError != NULL ? xrtErrorDomain(pError) : "",
			pError != NULL ? xrtErrorOperation(pError) : "",
			pError != NULL ? xrtErrorMessage(pError) : "");
		(void)xrtAtomic32FetchAdd(&pLive->Errors, 1u, XMEMORY_RELEASE);
		(void)xrtAtomic32FetchAdd(&pLive->Done, 1u, XMEMORY_RELEASE);
	}
	(void)xrtAtomic32FetchAdd(&pLive->DialDone, 1u, XMEMORY_RELEASE);
}



/* 读取并严格解析一把未加密 OpenSSH Ed25519 私钥。 */
static bool testSshLiveIdentityRead(
	const char* sPath,
	testsshliveidentity* pKey
)
{
	FILE* pFile;
	char* pText = NULL;
	long iFileSize;
	size_t iRead;
	size_t iBinarySize = 0u;
	xsshopensshprivatekey PrivateKey;
	bool bSuccess = false;

	if ( (sPath == NULL) || (pKey == NULL) ) {
		return false;
	}
	memset(pKey, 0, sizeof(*pKey));
	pFile = fopen(sPath, "rb");
	if ( pFile == NULL ) {
		return false;
	}
	if ( (fseek(pFile, 0, SEEK_END) != 0) ||
		((iFileSize = ftell(pFile)) <= 0) ||
		(fseek(pFile, 0, SEEK_SET) != 0) ) {
		fclose(pFile);
		return false;
	}
	pText = (char*)malloc((size_t)iFileSize + 1u);
	if ( pText != NULL ) {
		iRead = fread(pText, 1u, (size_t)iFileSize, pFile);
		pText[iRead] = '\0';
		if ( (iRead == (size_t)iFileSize) &&
			(xrtSshPrivateKeyPemRead(
				(xstrview){ pText, iRead },
				NULL,
				0u,
				&iBinarySize,
				NULL
			) == XSSH_OK) && (iBinarySize != 0u) ) {
			pKey->Binary = (unsigned char*)malloc(iBinarySize);
		}
		if ( (pKey->Binary != NULL) &&
			(xrtSshPrivateKeyPemRead(
				(xstrview){ pText, iRead },
				pKey->Binary,
				iBinarySize,
				&pKey->BinarySize,
				&PrivateKey
			) == XSSH_OK) &&
			(xrtSshPrivateKeyEd25519Read(
				&PrivateKey,
				&pKey->Identity
			) == XSSH_OK) ) {
			bSuccess = true;
		}
	}
	fclose(pFile);
	if ( pText != NULL ) {
		xrtSecureZero(pText, (size_t)iFileSize + 1u);
		free(pText);
	}
	if ( !bSuccess && (pKey->Binary != NULL) ) {
		xrtSecureZero(pKey->Binary, iBinarySize);
		free(pKey->Binary);
		memset(pKey, 0, sizeof(*pKey));
	}
	return bSuccess;
}



/* 清除私钥和所有已经进入终态的网络对象。 */
static bool testSshLiveClear(testsshlive* pLive)
{
	bool bSuccess = true;

	if ( pLive->Stream != NULL ) {
		xrtNetStreamDestroy(pLive->Stream);
	}
	if ( pLive->Dial != NULL ) {
		xrtNetDialDestroy(pLive->Dial);
	}
	if ( pLive->Resolver != NULL ) {
		bSuccess = xrtNetResolverDestroy(pLive->Resolver) && bSuccess;
	}
	if ( pLive->Engine != NULL ) {
		bSuccess = xrtNetEngineDestroy(pLive->Engine) && bSuccess;
	}
	bSuccess = xrtSshClientClear(&pLive->Client) && bSuccess;
	if ( pLive->Key.Binary != NULL ) {
		xrtSecureZero(pLive->Key.Binary, pLive->Key.BinarySize);
		free(pLive->Key.Binary);
		memset(&pLive->Key, 0, sizeof(pLive->Key));
	}
	free(pLive->Stress);
	pLive->Stress = NULL;
	return bSuccess;
}



/* 等待原子完成值，避免测试依赖隐藏同步对象。 */
static bool testSshLiveWait(const xatomic32* pValue, uint32 iTimeoutMs)
{
	uint32 i;

	for ( i = 0u; i < iTimeoutMs; ++i ) {
		if ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) != 0u ) {
			return true;
		}
		xrtSleep(1u);
	}
	return false;
}



/* 验证每个显式启用场景都取得请求、输出和正常退出证据。 */
static bool testSshLiveEvidence(const testsshlive* pLive)
{
	size_t i;

	if ( (xrtAtomic32Load(&pLive->Errors, XMEMORY_ACQUIRE) != 0u) ||
		(xrtAtomic32Load(&pLive->HostChecks, XMEMORY_ACQUIRE) == 0u) ||
		(pLive->Phase != TEST_SSH_LIVE_DONE) || !pLive->ExecReply ||
		!pLive->ExecExitReceived || (pLive->ExecExitStatus != 0u) ||
		!testSshLiveContains(
			pLive->ExecOutput,
			pLive->ExecOutputSize,
			pLive->ExecExpected
		) ) {
		return false;
	}
	if ( (pLive->PtyCommand != NULL) &&
		(!pLive->PtyReply || !pLive->ShellReply ||
		 !pLive->PtyExitReceived || (pLive->PtyExitStatus != 0u) ||
		 !testSshLiveContains(
			pLive->PtyOutput,
			pLive->PtyOutputSize,
			pLive->PtyExpected
		 )) ) {
		return false;
	}
	if ( (pLive->ForwardHost != NULL) && !testSshLiveContains(
			pLive->ForwardOutput,
			pLive->ForwardOutputSize,
			pLive->ForwardExpected
		) ) {
		return false;
	}
	if ( (pLive->StressSubmitted != pLive->StressCount) ||
		(pLive->StressClosed != pLive->StressCount) ) {
		fprintf(stderr,
			"[STRESS EVIDENCE] submitted=%zu closed=%zu expected=%zu\n",
			pLive->StressSubmitted,
			pLive->StressClosed,
			pLive->StressCount);
		return false;
	}
	for ( i = 0u; i < pLive->StressCount; ++i ) {
		if ( !pLive->Stress[i].Reply ||
			!pLive->Stress[i].ExitReceived ||
			(pLive->Stress[i].ExitStatus != 0u) ||
			!pLive->Stress[i].Closed ||
			(pLive->Stress[i].Bytes < TEST_SSH_LIVE_STRESS_MINIMUM_BYTES) ||
			!testSshLiveContains(
				pLive->Stress[i].Tail,
				pLive->Stress[i].TailSize,
				pLive->Stress[i].Marker
			) ) {
			fprintf(stderr,
				"[STRESS EVIDENCE] channel=%zu reply=%d exit=%d status=%u "
				"closed=%d bytes=%zu marker=%d tail=%zu\n",
				i,
				(int)pLive->Stress[i].Reply,
				(int)pLive->Stress[i].ExitReceived,
				pLive->Stress[i].ExitStatus,
				(int)pLive->Stress[i].Closed,
				pLive->Stress[i].Bytes,
				(int)testSshLiveContains(
					pLive->Stress[i].Tail,
					pLive->Stress[i].TailSize,
					pLive->Stress[i].Marker
				),
				pLive->Stress[i].TailSize);
			return false;
		}
	}
	return true;
}



/* 运行由环境变量显式配置的真实 OpenSSH 互操作门禁。 */
int main(void)
{
	static const xsshclientevents Events = {
		.Open = NULL,
		.Ready = testSshLiveReady,
		.HostKey = NULL,
		.Authenticate = NULL,
		.Packet = testSshLivePacket,
		.Data = testSshLiveData,
		.Rekey = NULL,
		.Error = testSshLiveError,
		.End = NULL,
		.HighWater = NULL,
		.LowWater = NULL,
		.Drain = testSshLiveDrain,
		.Close = testSshLiveClose,
		.Channel = testSshLiveChannel,
		.Global = NULL
	};
	testsshlive Live;
	xsshclientconfig ClientConfig;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetdialconfig DialConfig;
	const char* sHost = getenv("XSSH_LIVE_HOST");
	const char* sUser;
	const char* sPassword;
	const char* sIdentity;
	const char* sForwardPort;
	uint32 iPort;
	uint32 iTimeoutMs;
	uint32 iStressCount;
	size_t i;
	int iWritten;
	bool bSuccess;

	if ( (sHost == NULL) || (sHost[0] == '\0') ) {
		printf("[SKIP] xssh OpenSSH interop: set XSSH_LIVE_HOST and credentials\n");
		return 0;
	}
	memset(&Live, 0, sizeof(Live));
	if ( !xrtNetPostInit(&Live.PtyPost) ) {
		return 2;
	}
	sUser = getenv("XSSH_LIVE_USER");
	sPassword = getenv("XSSH_LIVE_PASSWORD");
	sIdentity = getenv("XSSH_LIVE_IDENTITY");
	Live.ExpectedFingerprint = getenv("XSSH_LIVE_HOST_FINGERPRINT");
	if ( (Live.ExpectedFingerprint != NULL) &&
		(Live.ExpectedFingerprint[0] == '\0') ) {
		Live.ExpectedFingerprint = NULL;
	}
	Live.AcceptNew = testSshLiveEnabled(getenv("XSSH_LIVE_ACCEPT_NEW"));
	if ( (sUser == NULL) || (sUser[0] == '\0') ||
		(((sPassword == NULL) || (sPassword[0] == '\0')) ==
		 ((sIdentity == NULL) || (sIdentity[0] == '\0'))) ||
		(((Live.ExpectedFingerprint == NULL) ||
		  (Live.ExpectedFingerprint[0] == '\0')) && !Live.AcceptNew) ||
		!testSshLiveUint(getenv("XSSH_LIVE_PORT"), 22u, 65535u, &iPort) ||
		!testSshLiveUint(
			getenv("XSSH_LIVE_TIMEOUT_MS"),
			30000u,
			600000u,
			&iTimeoutMs
		) || !testSshLiveUint(
			getenv("XSSH_LIVE_STRESS_CHANNELS"),
			TEST_SSH_LIVE_STRESS_CHANNELS_DEFAULT,
			TEST_SSH_LIVE_STRESS_CHANNELS_MAXIMUM,
			&iStressCount
		) ) {
		fprintf(stderr, "[FAIL] xssh OpenSSH interop configuration is invalid\n");
		return 2;
	}
	Live.ExecCommand = getenv("XSSH_LIVE_COMMAND");
	Live.ExecExpected = getenv("XSSH_LIVE_EXPECT");
	Live.ExecCommand = Live.ExecCommand != NULL ?
		Live.ExecCommand : "echo xssh-live-exec";
	Live.ExecExpected = Live.ExecExpected != NULL ?
		Live.ExecExpected : "xssh-live-exec";
	if ( (Live.ExecCommand[0] == '\0') || (Live.ExecExpected[0] == '\0') ) {
		fprintf(stderr, "[FAIL] xssh OpenSSH exec evidence is empty\n");
		return 2;
	}
	Live.PtyCommand = getenv("XSSH_LIVE_PTY_COMMAND");
	Live.PtyExpected = getenv("XSSH_LIVE_PTY_EXPECT");
	if ( Live.PtyCommand != NULL ) {
		if ( (Live.PtyCommand[0] == '\0') ||
			((Live.PtyExpected != NULL) && (Live.PtyExpected[0] == '\0')) ) {
			fprintf(stderr, "[FAIL] xssh OpenSSH PTY command is empty\n");
			return 2;
		}
		Live.PtyExpected = Live.PtyExpected != NULL ?
			Live.PtyExpected : "xssh-live-pty";
	}
	Live.ForwardHost = getenv("XSSH_LIVE_FORWARD_HOST");
	Live.ForwardSend = getenv("XSSH_LIVE_FORWARD_SEND");
	Live.ForwardExpected = getenv("XSSH_LIVE_FORWARD_EXPECT");
	sForwardPort = getenv("XSSH_LIVE_FORWARD_PORT");
	if ( Live.ForwardHost != NULL ) {
		if ( (Live.ForwardHost[0] == '\0') ||
			(sForwardPort == NULL) || !testSshLiveUint(
				sForwardPort,
				0u,
				65535u,
				&Live.ForwardPort
			) ) {
			fprintf(stderr, "[FAIL] xssh OpenSSH forward configuration is invalid\n");
			return 2;
		}
		Live.ForwardSend = Live.ForwardSend != NULL ?
			Live.ForwardSend : "xssh-live-forward";
		Live.ForwardExpected = Live.ForwardExpected != NULL ?
			Live.ForwardExpected : Live.ForwardSend;
		if ( (Live.ForwardSend[0] == '\0') ||
			(Live.ForwardExpected[0] == '\0') ) {
			fprintf(stderr, "[FAIL] xssh OpenSSH forward evidence is empty\n");
			return 2;
		}
	}
	Live.StressCount = (size_t)iStressCount;
	Live.Stress = (testsshlivestress*)calloc(
		Live.StressCount,
		sizeof(*Live.Stress)
	);
	if ( Live.Stress == NULL ) {
		fprintf(stderr, "[FAIL] xssh OpenSSH stress allocation failed\n");
		return 2;
	}
	for ( i = 0u; i < Live.StressCount; ++i ) {
		iWritten = snprintf(
			Live.Stress[i].Marker,
			sizeof(Live.Stress[i].Marker),
			"xssh-live-stress-%zu-done",
			i
		);
		if ( (iWritten <= 0) ||
			((size_t)iWritten >= sizeof(Live.Stress[i].Marker)) ) {
			(void)testSshLiveClear(&Live);
			return 2;
		}
		iWritten = snprintf(
			Live.Stress[i].Command,
			sizeof(Live.Stress[i].Command),
			"i=0; while [ \"$i\" -lt 256 ]; do "
			"printf '%%0128d\\n' \"$i\"; "
			"i=$((i + 1)); sleep 0.002; done; printf '%%s\\n' '%s'",
			Live.Stress[i].Marker
		);
		if ( (iWritten <= 0) ||
			((size_t)iWritten >= sizeof(Live.Stress[i].Command)) ) {
			(void)testSshLiveClear(&Live);
			return 2;
		}
	}
	if ( !xrtSshClientConfigInit(&ClientConfig) ) {
		(void)testSshLiveClear(&Live);
		return 2;
	}
	ClientConfig.Channels.MaxChannels = Live.StressCount + 4u;
	ClientConfig.Channels.ReceiveWindow = 32768u;
	ClientConfig.Channels.ReceiveMaxPacket = 8192u;
	ClientConfig.Channels.AdjustThreshold = 16384u;
	ClientConfig.Channels.Io.ReceiveLimit = 65536u;
	if ( (sIdentity != NULL) &&
		!testSshLiveIdentityRead(sIdentity, &Live.Key) ) {
		fprintf(stderr, "[FAIL] xssh OpenSSH Ed25519 identity could not be read\n");
		(void)testSshLiveClear(&Live);
		return 2;
	}
	ClientConfig.Core.User = (xstrview){ sUser, strlen(sUser) };
	ClientConfig.Core.HostKey = testSshLiveHostKey;
	ClientConfig.Core.HostKeyData = &Live;
	if ( Live.Key.Binary != NULL ) {
		ClientConfig.Core.Authenticate = xrtSshClientEd25519Auth;
		ClientConfig.Core.AuthenticateData = &Live.Key.Identity;
	} else {
		Live.Password = (xstrview){ sPassword, strlen(sPassword) };
		ClientConfig.Core.Authenticate = xrtSshClientPasswordAuth;
		ClientConfig.Core.AuthenticateData = &Live.Password;
	}
	if ( !xrtSshClientInit(&Live.Client, &ClientConfig, &Events, &Live) ) {
		(void)testSshLiveClear(&Live);
		return 2;
	}
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1u;
	Live.Engine = xrtNetEngineCreate(&EngineConfig);
	xrtNetResolverConfigInit(&ResolverConfig);
	Live.Resolver = xrtNetResolverCreate(&ResolverConfig);
	if ( (Live.Engine == NULL) || (Live.Resolver == NULL) ||
		!xrtNetEngineStart(Live.Engine) ) {
		(void)testSshLiveClear(&Live);
		return 2;
	}
	xrtNetDialConfigInit(&DialConfig);
	DialConfig.Timeout = (uint64)iTimeoutMs * 1000u;
	DialConfig.Stream.ReadLimit = 1048576u;
	DialConfig.Stream.WriteLimit = 1048576u;
	Live.Dial = xrtSshClientDial(
		&Live.Client,
		Live.Engine,
		Live.Resolver,
		sHost,
		(uint16)iPort,
		&DialConfig,
		testSshLiveDialDone,
		&Live
	);
	if ( Live.Dial == NULL ) {
		(void)testSshLiveClear(&Live);
		return 2;
	}
	if ( !testSshLiveWait(&Live.Done, iTimeoutMs) ) {
		testSshLiveFail(&Live, "live operation timed out");
		(void)testSshLiveWait(&Live.Done, 5000u);
	}
	(void)testSshLiveWait(&Live.DialDone, 5000u);
	bSuccess = testSshLiveEvidence(&Live);
	if ( !testSshLiveClear(&Live) ) {
		fprintf(stderr, "[FAIL] xssh OpenSSH interop resource cleanup failed\n");
		bSuccess = false;
	}
	if ( !bSuccess ) {
		fprintf(stderr,
			"[EVIDENCE] exec=%zu stderr=%zu pty=%zu forward=%zu phase=%d errors=%u\n",
			Live.ExecOutputSize,
			Live.ExecErrorSize,
			Live.PtyOutputSize,
			Live.ForwardOutputSize,
			(int)Live.Phase,
			xrtAtomic32Load(&Live.Errors, XMEMORY_ACQUIRE));
		return 1;
	}
	printf("[PASS] xssh OpenSSH interop: exec%s%s, stress=%zu channels\n",
		Live.PtyCommand != NULL ? ", pty" : "",
		Live.ForwardHost != NULL ? ", direct-tcpip" : "",
		Live.StressCount);
	return 0;
}
