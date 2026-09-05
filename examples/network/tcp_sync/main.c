#include <stdio.h>
#include <string.h>
#include <xrt.h>



/*
 * 范例：network/tcp_sync —— 阻塞式 TCP：Accept 与收发的同步面
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtNetListen / xrtNetListenerLocal   监听与实际端点查询
 *   xrtNetAccept（同步面）               阻塞等待连接
 *   xnetstream 同步读                   一次完整接收
 * 模块宏：XRT_MODULE_NET_TCP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/tcp_sync/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   received: hello
 *
 * "同步面"的定位：同一套 xnetstream 对象既有事件回调面
 *   （见 tcp 范例）也有阻塞面——工具类程序（探活、
 *   一次性迁移）用同步面最少代码；端口 0 = 系统分配，
 *   ListenerLocal 取实际端口（并发测试不冲突的标准姿势）。
 */


/* 使用现有 Engine 完成一次本地阻塞 Accept 和接收。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetstream* pServer;
	xnetbytes* pBytes;
	xbytesview View;
	xnetaddr Address;
	bool bReceived;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	xrtNetListenConfigInit(&ListenConfig);
	(void)xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	);
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	if ( (pListener == NULL) || !xrtNetListenerLocal(pListener, &Address) ) {
		return 2;
	}
	pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		NULL,
		NULL,
		NULL
	);
	pServer = xrtNetListenerAcceptWait(
		pListener,
		xrtDeadlineAfter(3000000u),
		NULL
	);
	if ( (pClient == NULL) || (pServer == NULL) ||
		 (xrtNetStreamSend(pClient, "hello", 5) != XNET_RESULT_OK) ) {
		return 3;
	}
	pBytes = xrtNetStreamRecv(
		pServer,
		0,
		xrtDeadlineAfter(3000000u),
		NULL
	);
	View = xrtNetBytesView(pBytes);
	bReceived = pBytes != NULL;
	if ( pBytes != NULL ) {
		printf("received: %.*s\n", (int)View.Size, (cstr)View.Data);
	}
	xrtNetBytesDestroy(pBytes);
	(void)xrtNetStreamAbort(pClient);
	(void)xrtNetStreamAbort(pServer);
	(void)xrtNetListenerClose(pListener);
	while ( (xrtNetStreamState(pClient) != XNET_STREAM_CLOSED) ||
		 (xrtNetStreamState(pServer) != XNET_STREAM_CLOSED) ||
		 (xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED) ) {
		xrtThreadYield();
	}
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pServer);
	xrtNetListenerDestroy(pListener);
	return xrtNetEngineDestroy(pEngine) && bReceived ? 0 : 4;
}
