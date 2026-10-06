/*
 * 范例：network/tcp_server_tour —— 多端点 Server 自省族
 * ----------------------------------------------------------------
 * 演示 API：
 *   【生命周期】  xrtNetServerRef（线程安全增引用）
 *   【拉取接受】  xrtNetServerAccept（非阻塞取流）
 *   【端点族】    xrtNetServerEndpointCount / xrtNetServerLocal
 *                  xrtNetServerListenerCount / xrtNetServerListener
 *                  （借用底层 Listener，多端点共享模式逐端点一个）
 *   【自省】      xrtNetServerData / xrtNetServerStats（聚合统计）
 * 模块宏：XRT_MODULE_NET_TCP_SERVER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/tcp_server_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   server-tour: endpoints=2 listeners=2 shared-port=1
 *   accept: stream=1 stats(accepted=2 queued>=0) data=ok
 *   listener[0]/[1]: ok
 *
 * 双端点共享动态端口（SharedPort）：第零端点端口零由系统分配，
 *   附加端点继承同一端口——两个 Listener 分别监听 IPv4 和 IPv6 环回，
 *   EndpointCount=2、ListenerCount=2、两个 Local 地址端口一致。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#include <math.h>
static inline double exampleTimerLimit(int64 Timeout)
{
    return Timeout == XRT_WAIT_FOREVER ? INFINITY : xrtTimer() + (double)Timeout / 1000.0;
}
static inline bool exampleTimerExpired(double Limit)
{
    return xrtTimer() >= Limit;
}
static inline int64 exampleTimerRemaining(double Limit)
{
    double Ms;
    if (Limit == INFINITY) return XRT_WAIT_FOREVER;
    Ms = ceil((Limit - xrtTimer()) * 1000.0);
    return Ms <= 0 ? 0 : Ms >= 0x1p63 ? INT64_MAX : (int64)Ms;
}

#define EXAMPLE_DEADLINE_MS	INT64_C(3000)

static uint64 g_Tag = 0;

static bool exampleWaitStreamState(xnetstream* pStream,
	xnetstreamstate State)
{
	double iDeadline = exampleTimerLimit(EXAMPLE_DEADLINE_MS);

	while ( xrtNetStreamState(pStream) != State ) {
		if ( exampleTimerExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}

int main(void)
{
	xnetengineconfig EngineConfig;
	xnetserverconfig ServerConfig;
	xnetlistenconfig Extra;
	xnetengine* pEngine = NULL;
	xnetserver* pServer = NULL;
	xnetserver* pServerRef = NULL;
	xnetlistener* pListener0 = NULL;
	xnetlistener* pListener1 = NULL;
	xnetstream* pStream = NULL;
	xnetaddr Addr0;
	xnetaddr Addr1;
	xnetserverstats Stats;
	int iResult = 1;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	xrtNetServerConfigInit(&ServerConfig);
	(void)xrtNetAddrLoopback(&ServerConfig.Listen.Address,
		XNET_FAMILY_IPV4, 0);
	ServerConfig.SharedPort = true;
	/* 两个地址族共享端口（清除默认 ExclusiveAddress），
	 * SharedPort 让端口零的附加端点继承首端点的系统分配端口。 */
	ServerConfig.Listen.ReuseAddress = true;
	ServerConfig.Listen.ExclusiveAddress = false;
	Extra = ServerConfig.Listen;
	(void)xrtNetAddrLoopback(&Extra.Address, XNET_FAMILY_IPV6, 0);
	Extra.IPv6Only = true;
	Extra.ReuseAddress = true;
	Extra.ExclusiveAddress = false;
	ServerConfig.Additional = &Extra;
	ServerConfig.AdditionalCount = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	pServer = xrtNetServerStart(pEngine, &ServerConfig, NULL, NULL,
		&g_Tag);
	if ( pServer == NULL ) {
		goto Cleanup;
	}
	if ( (xrtNetServerEndpointCount(pServer) != 2u) ||
		 (xrtNetServerListenerCount(pServer) != 2u) ||
		 !xrtNetServerLocal(pServer, 0, &Addr0) ||
		 !xrtNetServerLocal(pServer, 1, &Addr1) ||
		 (Addr0.Port == 0u) || (Addr0.Port != Addr1.Port) ) {
		iResult = 2;
		goto Cleanup;
	}
	printf("server-tour: endpoints=%zu listeners=%zu shared-port=1",
		xrtNetServerEndpointCount(pServer),
		xrtNetServerListenerCount(pServer));
	/* Ref：线程安全增引用，收尾多一次 Destroy。 */
	pServerRef = xrtNetServerRef(pServer);
	printf("\n");

	/* 拉取接受：向两个端点各拨一条（同端口的 IPv4 和 IPv6 Listener 各一条）。 */
	{
		xnetstream* pA = xrtNetStreamConnect(pEngine, &Addr0, 0,
			NULL, NULL, NULL);
		xnetstream* pB = xrtNetStreamConnect(pEngine, &Addr1, 0,
			NULL, NULL, NULL);
		int iGot = 0;
		double iDeadline = exampleTimerLimit(EXAMPLE_DEADLINE_MS);

		if ( (pA == NULL) || (pB == NULL) ) {
			xrtNetStreamDestroy(pA);
			xrtNetStreamDestroy(pB);
			iResult = 3;
			goto Cleanup;
		}
		while ( iGot < 2 ) {
			xnetstream* pOne = xrtNetServerAccept(pServer);

			if ( pOne != NULL ) {
				if ( pStream == NULL ) {
					pStream = pOne;  /* 留一条做状态核对 */
				}
				else {
					xrtNetStreamClose(pOne);
					xrtNetStreamDestroy(pOne);
				}
				++iGot;
				iDeadline = exampleTimerLimit(
					EXAMPLE_DEADLINE_MS);
				continue;
			}
			if ( exampleTimerExpired(iDeadline) ) {
				break;
			}
			xrtThreadYield();
		}
		xrtNetStreamAbort(pA);
		xrtNetStreamAbort(pB);
		(void)exampleWaitStreamState(pA, XNET_STREAM_CLOSED);
		(void)exampleWaitStreamState(pB, XNET_STREAM_CLOSED);
		xrtNetStreamDestroy(pA);
		xrtNetStreamDestroy(pB);
		if ( iGot != 2 ) {
			iResult = 4;
			goto Cleanup;
		}
	}
	printf("accept: stream=1");

	/* 统计聚合 + 用户数据。 */
	if ( !xrtNetServerStats(pServer, &Stats) ||
		 (Stats.Accepted < 2u) ||
		 (xrtNetServerData(pServer) != &g_Tag) ) {
		iResult = 5;
		goto Cleanup;
	}
	printf(" stats(accepted=%llu queued>=%u)",
		(unsigned long long)Stats.Accepted,
		(unsigned)Stats.QueuedAccepts);
	printf(" data=%s\n",
		xrtNetServerData(pServer) == &g_Tag ? "ok" : "fail");

	/* 借用底层 Listener：两个独立引用都要配对 Destroy。 */
	pListener0 = xrtNetServerListener(pServer, 0);
	pListener1 = xrtNetServerListener(pServer, 1);
	if ( (pListener0 == NULL) || (pListener1 == NULL) ||
		 (pListener0 == pListener1) ) {
		iResult = 6;
		goto Cleanup;
	}
	printf("listener[0]/[1]: ok\n");
	iResult = 0;

Cleanup:
	xrtNetStreamClose(pStream);
	if ( pStream != NULL ) {
		(void)exampleWaitStreamState(pStream, XNET_STREAM_CLOSED);
	}
	xrtNetStreamDestroy(pStream);
	xrtNetListenerDestroy(pListener0);
	xrtNetListenerDestroy(pListener1);
	if ( pServer != NULL ) {
		double iEnd = exampleTimerLimit(EXAMPLE_DEADLINE_MS);

		(void)xrtNetServerClose(pServer);
		while ( xrtNetServerState(pServer) != XNET_SERVER_CLOSED ) {
			if ( exampleTimerExpired(iEnd) ) {
				break;
			}
			xrtThreadYield();
		}
	}
	xrtNetServerDestroy(pServerRef);
	xrtNetServerDestroy(pServer);
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) &&
		 (iResult == 0) ) {
		iResult = 7;
	}
	return iResult;
}
