#include "../test.h"
#include "../../src/internal/xrt_tcp.h"



/* 验证 TCP 配置、空指针和 Engine 状态的错误口径。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig StreamConfig;
	xnetstreamstats StreamStats;
	xnetlistenerstats ListenerStats;
	xnetengine* pEngine;
	xnetaddr Address;
	xnetspan Span;
	xnetref Ref;
	size_t iStreamSizeLimit = 576u;

	/* 核心对象及可裁剪扩展都必须守住各自的固定尺寸预算。 */
	#if defined(XRT_FEATURE_NET_TCP_DIAL)
		iStreamSizeLimit += 40u;
	#endif
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		iStreamSizeLimit += 64u;
	#endif
	testRequire(sizeof(xnetstream) <= iStreamSizeLimit,
		"TCP stream fixed object is too large");

	xrtNetStreamConfigInit(&StreamConfig);
	testRequire((StreamConfig.ReadSize != 0) &&
		 (StreamConfig.ReadLimit >= StreamConfig.ReadSize) &&
		 (StreamConfig.ReadMode == XNET_STREAM_READ_ADAPTIVE) &&
		 (StreamConfig.WriteLowWater <= StreamConfig.WriteHighWater) &&
		 (StreamConfig.WriteHighWater <= StreamConfig.WriteLimit) &&
		 (StreamConfig.ConnectTimeout != 0) && StreamConfig.NoDelay,
		"TCP stream defaults are invalid");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire((ListenConfig.Address.Family == XNET_FAMILY_IPV4) &&
		 (ListenConfig.Address.Port == 0) &&
		 (ListenConfig.AcceptConcurrency != 0) &&
		 (ListenConfig.AcceptQueueLimit != 0) &&
		 (ListenConfig.Backlog > 0) &&
		 (ListenConfig.ReuseAddress != ListenConfig.ExclusiveAddress),
		"TCP listener defaults are invalid");

	testRequire(xrtNetStreamRef(NULL) == NULL,
		"TCP stream ref accepted NULL");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"TCP NULL stream ref error mismatch");
	xrtClearError();
	testRequire(xrtNetListenerRef(NULL) == NULL,
		"TCP listener ref accepted NULL");
	testRequire(xrtNetListenerAccept(NULL) == NULL,
		"TCP listener accept accepted NULL");
	Ref = (xnetref){ (cbytes)"x", 1, NULL, NULL };
	testRequire(xrtNetStreamSend(NULL, "x", 1) == XNET_RESULT_ERROR &&
		 (xrtNetStreamSendVec(NULL, NULL, 0) == XNET_RESULT_ERROR) &&
		 (xrtNetStreamSendBuffer(NULL, NULL) == XNET_RESULT_ERROR) &&
		 (xrtNetStreamSendRef(
			NULL,
			"x",
			1,
			NULL,
			NULL
		 ) == XNET_RESULT_ERROR) &&
		 (xrtNetStreamSendRefs(NULL, &Ref, 1) == XNET_RESULT_ERROR) &&
		 (xrtNetStreamSendTake(NULL, NULL, 0) == XNET_RESULT_ERROR),
		"TCP NULL send arguments were accepted");
	xrtClearError();
	testRequire(!xrtNetStreamResume(NULL) &&
		 !xrtNetStreamShutdownWrite(NULL) &&
		 !xrtNetStreamSetEvents(NULL, NULL, NULL) &&
		 !xrtNetStreamClose(NULL) && !xrtNetStreamAbort(NULL) &&
		 !xrtNetListenerClose(NULL),
		"TCP NULL control arguments were accepted");
	xrtClearError();
	testRequire((xrtNetStreamState(NULL) == XNET_STREAM_CLOSED) &&
		 (xrtNetListenerState(NULL) == XNET_LISTENER_CLOSED) &&
		 (xrtNetStreamPending(NULL) == 0) &&
		 (xrtNetStreamWriteLimit(NULL) == 0) &&
		 (xrtNetStreamWritable(NULL) == 0) &&
		 (xrtNetStreamWorker(NULL) == NULL) &&
		 (xrtNetListenerWorker(NULL) == NULL) &&
		 (xrtNetStreamSocket(NULL) == NULL) &&
		 (xrtNetStreamData(NULL) == NULL) &&
		 (xrtNetListenerData(NULL) == NULL) &&
		 (xrtNetStreamError(NULL) == NULL),
		"TCP NULL query defaults mismatch");
	testRequire(!xrtNetStreamStats(NULL, &StreamStats) &&
		 !xrtNetListenerStats(NULL, &ListenerStats),
		"TCP NULL stats were accepted");
	xrtClearError();

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine != NULL, "TCP invalid engine create failed");
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		1
	), "TCP invalid loopback address failed");
	testRequire(xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		NULL,
		NULL,
		NULL
	) == NULL, "stopped engine accepted TCP connect");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_CLOSED,
		"stopped TCP connect error mismatch");
	xrtClearError();
	testRequire(xrtNetListen(
		pEngine,
		NULL,
		NULL,
		NULL,
		NULL
	) == NULL, "stopped engine accepted TCP listener");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_CLOSED,
		"stopped TCP listener error mismatch");
	xrtClearError();
	testRequire(xrtNetEngineStart(pEngine),
		"TCP invalid engine start failed");

	Address.Family = XNET_FAMILY_UNSPEC;
	testRequire(xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		NULL,
		NULL,
		NULL
	) == NULL, "TCP connect accepted an invalid family");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"invalid TCP family error mismatch");
	xrtClearError();
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		1
	), "TCP invalid address reset failed");
	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadSize = 0;
	testRequire(xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		&StreamConfig,
		NULL,
		NULL
	) == NULL, "TCP connect accepted zero read size");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_STREAM_CONFIG),
		"invalid TCP stream config error mismatch");
	xrtClearError();

	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadMode = (xnetstreamreadmode)99;
	testRequire(xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		&StreamConfig,
		NULL,
		NULL
	) == NULL, "TCP connect accepted an unknown read mode");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_STREAM_CONFIG),
		"invalid TCP read mode error mismatch");
	xrtClearError();

	xrtNetListenConfigInit(&ListenConfig);
	ListenConfig.ExclusiveAddress = true;
	ListenConfig.ReuseAddress = true;
	testRequire(xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	) == NULL, "TCP listener accepted exclusive reuse conflict");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_LISTENER_CREATE),
		"invalid TCP listener config error mismatch");
	xrtClearError();
	xrtNetListenConfigInit(&ListenConfig);
	ListenConfig.AcceptConcurrency = 0;
	testRequire(xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	) == NULL, "TCP listener accepted zero accept concurrency");
	xrtClearError();
	xrtNetListenConfigInit(&ListenConfig);
	ListenConfig.AcceptQueueLimit = 0;
	testRequire(xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	) == NULL, "TCP listener accepted zero queue limit");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_LISTENER_CREATE),
		"invalid TCP queue limit error mismatch");
	xrtClearError();

	Span.Data = NULL;
	Span.Size = 1;
	testRequire(xrtNetStreamSendVec(NULL, &Span, 1) ==
		 XNET_RESULT_ERROR, "TCP vector send accepted NULL stream");
	xrtClearError();
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP invalid engine destroy failed");
	printf("[PASS] network TCP invalid inputs\n");
	return 0;
}
