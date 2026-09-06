/*
 * 范例：network/port_tour —— 事件端口：readiness 与 completion 双形态
 * ----------------------------------------------------------------
 * 演示 API：
 *   【端口自省】  xrtNetPortBackend / Capabilities / GetConfig
 *   【readiness】 xrtNetPortWatch / Unwatch（SELECT 后端）
 *   【流完成】    xrtNetPortConnect / Accept / ReadProbe /
 *                 Recv / RecvVec / Send / SendVec（IOCP 后端）
 *   【数据报完成】xrtNetPortRecvFromVec / RecvMsg / RecvMsgVec /
 *                 SendToVec / SendMsgVec
 *   【控制】      xrtNetPortCancel（在途操作以 CANCELLED 终结）
 *                 xrtNetPortPost（USER 事件）/ Wake（WAKE 事件）
 *                 xrtNetPortRecvError（平台能力门控）
 * 模块宏：XRT_MODULE_NET_PORT（Windows 演示 IOCP + SELECT）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/port_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   port: backends iocp=completion select=readiness ok
 *   port: watch -> READY -> unwatch ok
 *   port: tcp connect + accept + readprobe + recv/send ok
 *   port: stream vec recv/send ok
 *   port: dgram recvfromvec/recvmsg/recvmsgvec ok
 *   port: dgram sendtovec/sendmsgvec ok
 *   port: cancel in-flight recv -> CANCELLED ok
 *   port: post USER + wake WAKE ok
 *   port: recv-error gated by platform ok
 *
 * Windows 上两类能力分属两个后端：IOCP 只有 completion
 *   （Watch 报 "no readiness capability"），SELECT 只有
 *   readiness（提交操作报 "no completion capability"）——
 *   本例用两个端口分别演示；Linux 上 EPOLL/URING 二者兼备。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

/* 端口事件暂存项与等待辅助共用的事件类型一致。 */
typedef xnetportevent xnetportexample;

/* 跨调用暂存不匹配事件：一次 Wait 可能同批返回多个操作的
 * 终态（如 CONNECT 与 ACCEPT），逐个等取时其余事件必须留存。 */
static xnetportexample g_Pending[16];
static size_t g_iPending = 0;

/* 等待一个匹配 (Type, Id) 的端口事件；成功返回 true 并写出事件。 */
static bool exampleWaitFor(xnetport* pPort, xnetporteventtype Type,
	uint64 Id, xnetportevent* pEvent, uint64 iTimeoutUs)
{
	xnetportevent Events[8];
	size_t iCount = 0;

	for ( int i = 0; i < 100; i++ ) {
		/* 先消费先前批次中暂存的事件。 */
		for ( size_t j = 0; j < g_iPending; j++ ) {
			if ( (g_Pending[j].Type == Type) &&
				(g_Pending[j].Id == Id) ) {
				*pEvent = g_Pending[j];
				memmove(g_Pending + j, g_Pending + j + 1u,
					(g_iPending - j - 1u) *
						sizeof(g_Pending[0]));
				--g_iPending;
				return true;
			}
		}
		if ( xrtNetPortWait(pPort, Events, 8u,
				xrtDeadlineAfter(iTimeoutUs / 100u),
				&iCount) != XNET_RESULT_OK ) {
			continue;
		}
		{
			/* 整批处理：匹配项交付，其余全部暂存——
			 * 匹配项可能不是批次最后一个，提前返回会
			 * 丢掉同批后续事件。 */
			const xnetportexample* pMatch = NULL;

			for ( size_t j = 0; j < iCount; j++ ) {
				if ( (pMatch == NULL) &&
					(Events[j].Type == Type) &&
					(Events[j].Id == Id) ) {
					pMatch = &Events[j];
				} else if ( g_iPending < 16u ) {
					g_Pending[g_iPending++] = Events[j];
				}
			}
			if ( pMatch != NULL ) {
				*pEvent = *pMatch;
				return true;
			}
		}
	}
	return false;
}

int main(void)
{
	xnetportconfig Config;
	xnetport* pIocp = NULL;
	xnetport* pSelect = NULL;
	xnetsocket UdpA = 0;
	xnetsocket UdpB = 0;
	xnetsocket Listener = 0;
	xnetsocket Client = 0;
	xnetsocket Server = 0;
	xnetaddr AddrUdpA;
	xnetaddr AddrUdpB;
	xnetaddr DestUdpA;
	xnetaddr AddrListen;
	xnetportevent Event;
	xnetdgramcontrol Control;
	xnetspan Out[2];
	xnetwspan In[2];
	uint8 arrBuf[64];
	size_t iSent = 0;
	uint32 iCaps = 0;
	int iResult = 1;

	/* ---- 双后端创建与能力自省。 ---- */
	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_IOCP;
	pIocp = xrtNetPortCreate(&Config);
	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_SELECT;
	pSelect = xrtNetPortCreate(&Config);
	if ( (pIocp == NULL) || (pSelect == NULL) ||
		(xrtNetPortBackend(pIocp) != XNET_PORT_IOCP) ||
		(xrtNetPortBackend(pSelect) != XNET_PORT_SELECT) ) {
		goto Cleanup;
	}
	iCaps = xrtNetPortCapabilities(pIocp);
	if ( ((iCaps & XNET_PORT_CAP_COMPLETION) == 0u) ||
		((iCaps & XNET_PORT_CAP_READINESS) != 0u) ) {
		goto Cleanup;
	}
	iCaps = xrtNetPortCapabilities(pSelect);
	if ( ((iCaps & XNET_PORT_CAP_READINESS) == 0u) ||
		((iCaps & XNET_PORT_CAP_COMPLETION) != 0u) ) {
		goto Cleanup;
	}
	{
		xnetportconfig Resolved;

		if ( !xrtNetPortGetConfig(pIocp, &Resolved) ||
			(Resolved.Backend != XNET_PORT_IOCP) ) {
			goto Cleanup;
		}
	}
	/* 两个 UDP 套接字：A 收、B 发。 */
	UdpA = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK);
	UdpB = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK);
	if ( (UdpA == 0) || (UdpB == 0) ||
		!xrtNetAddrAny(&AddrUdpA, XNET_FAMILY_IPV4, 0u) ||
		!xrtNetSocketBind(UdpA, &AddrUdpA) ||
		!xrtNetSocketLocal(UdpA, &AddrUdpA) ||
		!xrtNetAddrAny(&AddrUdpB, XNET_FAMILY_IPV4, 0u) ||
		!xrtNetSocketBind(UdpB, &AddrUdpB) ||
		!xrtNetSocketLocal(UdpB, &AddrUdpB) ||
		!xrtNetAddrLoopback(&DestUdpA, XNET_FAMILY_IPV4,
			AddrUdpA.Port) ) {
		goto Cleanup;
	}
	printf("port: backends iocp=completion select=readiness ok\n");

	/* ---- readiness：SELECT 端口上 Watch → READY → Unwatch。 ---- */
	if ( !xrtNetPortWatch(pSelect, UdpA, 100u, XNET_PORT_EVENT_READ,
			NULL) ||
		(xrtNetSocketSendTo(UdpB, "r", 1u, &iSent, &DestUdpA) !=
			XNET_RESULT_OK) ||
		!exampleWaitFor(pSelect, XNET_PORT_EVENT_READY, 100u,
			&Event, 2000000ull) ||
		!xrtNetPortUnwatch(pSelect, UdpA) ) {
		goto Cleanup;
	}
	/* 消费掉触发的数据报，保持后续段干净。 */
	(void)xrtNetSocketRecv(UdpA, arrBuf, sizeof(arrBuf), &iSent);
	printf("port: watch -> READY -> unwatch ok\n");

	/* ---- TCP：Connect + Accept + ReadProbe + Recv/Send。 ---- */
	Listener = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK);
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK);
	if ( (Listener == 0) || (Client == 0) ||
		!xrtNetAddrLoopback(&AddrListen, XNET_FAMILY_IPV4, 0u) ||
		!xrtNetSocketBind(Listener, &AddrListen) ||
		!xrtNetSocketListen(Listener, 1) ||
		!xrtNetSocketLocal(Listener, &AddrListen) ||
		!xrtNetPortConnect(pIocp, Client, &AddrListen, 201u, NULL) ||
		!xrtNetPortAccept(pIocp, Listener, 202u, NULL) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_CONNECT, 201u,
			&Event, 2000000ull) ||
		(Event.Result != XNET_RESULT_OK) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_ACCEPT, 202u,
			&Event, 2000000ull) ||
		(Event.Accepted == 0) ) {
		goto Cleanup;
	}
	Server = Event.Accepted;
	/* ReadProbe 等待可读，不借数据缓冲；终态后再提交 Recv。 */
	if ( !xrtNetPortReadProbe(pIocp, Server, 203u, NULL) ||
		(xrtNetSocketSend(Client, "hi", 2u, &iSent) !=
			XNET_RESULT_OK) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_READ_PROBE, 203u,
			&Event, 2000000ull) ) {
		goto Cleanup;
	}
	if ( !xrtNetPortRecv(pIocp, Server, arrBuf, 8u, 204u, NULL) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV, 204u,
			&Event, 2000000ull) ||
		(Event.Bytes != 2u) ||
		(memcmp(arrBuf, "hi", 2u) != 0) ||
		!xrtNetPortSend(pIocp, Server, "ok", 2u, 205u, NULL) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_SEND, 205u,
			&Event, 2000000ull) ) {
		goto Cleanup;
	}
	(void)xrtNetSocketRecv(Client, arrBuf, sizeof(arrBuf), &iSent);
	printf("port: tcp connect + accept + readprobe + recv/send ok\n");

	/* ---- 流向量：RecvVec 两段分散 + SendVec 两段聚集。 ---- */
	Out[0].Data = (cbytes)"v1";
	Out[0].Size = 2u;
	Out[1].Data = (cbytes)"v2";
	Out[1].Size = 2u;
	In[0].Data = arrBuf;
	In[0].Size = 2u;
	In[1].Data = arrBuf + 2;
	In[1].Size = 2u;
	if ( !xrtNetPortRecvVec(pIocp, Server, In, 2u, 210u, NULL) ||
		(xrtNetSocketSendVec(Client, Out, 2u, &iSent) !=
			XNET_RESULT_OK) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV, 210u,
			&Event, 2000000ull) ||
		(Event.Bytes != 4u) ||
		(memcmp(arrBuf, "v1v2", 4u) != 0) ||
		!xrtNetPortSendVec(pIocp, Server, Out, 2u, 211u, NULL) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_SEND, 211u,
			&Event, 2000000ull) ) {
		goto Cleanup;
	}
	(void)xrtNetSocketRecv(Client, arrBuf, sizeof(arrBuf), &iSent);
	printf("port: stream vec recv/send ok\n");

	/* ---- 数据报接收三形态：FromVec / Msg / MsgVec。
	 * RecvMsg 族要求 Socket 先启用至少一位元数据——
	 * 未启用时提交被拒（"metadata is not enabled"）。 ---- */
	if ( !xrtNetSocketDgramMetaSet(UdpA, XNET_DGRAM_META_HOP_LIMIT) ) {
		goto Cleanup;
	}
	if ( !xrtNetPortRecvFromVec(pIocp, UdpA, In, 2u, 220u, NULL) ||
		(xrtNetSocketSendTo(UdpB, "d1", 2u, &iSent, &DestUdpA) !=
			XNET_RESULT_OK) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV_FROM, 220u,
			&Event, 2000000ull) ||
		(Event.Bytes != 2u) ||
		(memcmp(arrBuf, "d1", 2u) != 0) ||
		(Event.Address.Port != AddrUdpB.Port) ) {
		goto Cleanup;
	}
	if ( !xrtNetPortRecvMsg(pIocp, UdpA, arrBuf, 16u, 221u, NULL) ||
		(xrtNetSocketSendTo(UdpB, "d2", 2u, &iSent, &DestUdpA) !=
			XNET_RESULT_OK) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV_MSG, 221u,
			&Event, 2000000ull) ||
		(Event.Bytes != 2u) ||
		(memcmp(arrBuf, "d2", 2u) != 0) ||
		/* 终态事件携带已启用的元数据位。 */
		((Event.Meta.Flags & XNET_DGRAM_META_HOP_LIMIT) == 0u) ) {
		goto Cleanup;
	}
	if ( !xrtNetPortRecvMsgVec(pIocp, UdpA, In, 2u, 222u, NULL) ||
		(xrtNetSocketSendTo(UdpB, "d3", 2u, &iSent, &DestUdpA) !=
			XNET_RESULT_OK) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV_MSG, 222u,
			&Event, 2000000ull) ||
		(Event.Bytes != 2u) ||
		(memcmp(arrBuf, "d3", 2u) != 0) ) {
		goto Cleanup;
	}
	printf("port: dgram recvfromvec/recvmsg/recvmsgvec ok\n");

	/* ---- 数据报发送两形态：ToVec / MsgVec（含逐包控制）。
	 * 空控制或零 Flags 走普通发送路径：有远端地址为 SEND_TO，
	 * 无远端地址为 SEND；仅非零 Flags 的控制发送为 SEND_MSG。 ---- */
	memset(&Control, 0, sizeof(Control));
	Out[0].Data = (cbytes)"t1";
	Out[0].Size = 2u;
	Out[1].Data = (cbytes)"t2";
	Out[1].Size = 2u;
	if ( !xrtNetPortSendToVec(pIocp, UdpB, Out, 2u, &DestUdpA,
			230u, NULL) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_SEND_TO, 230u,
			&Event, 2000000ull) ||
		(Event.Result != XNET_RESULT_OK) ||
		!xrtNetPortSendMsgVec(pIocp, UdpB, Out, 2u, &DestUdpA,
			&Control, 231u, NULL) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_SEND_TO, 231u,
			&Event, 2000000ull) ||
		(Event.Result != XNET_RESULT_OK) ) {
		goto Cleanup;
	}
	/* A 侧清掉两份发出的数据报。 */
	while ( xrtNetSocketRecv(UdpA, arrBuf, sizeof(arrBuf), &iSent) ==
		XNET_RESULT_OK ) {
	}
	printf("port: dgram sendtovec/sendmsgvec ok\n");

	/* ---- Cancel：提交一个等不到的 Recv 再取消 → CANCELLED。 ---- */
	if ( !xrtNetPortRecv(pIocp, UdpA, arrBuf, 8u, 600u, NULL) ||
		!xrtNetPortCancel(pIocp, 600u) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV, 600u,
			&Event, 2000000ull) ||
		(Event.Result != XNET_RESULT_CANCELLED) ) {
		goto Cleanup;
	}
	printf("port: cancel in-flight recv -> CANCELLED ok\n");

	/* ---- Post 与 Wake：USER 事件携带 Id，WAKE 事件可合并。 ---- */
	if ( !xrtNetPortPost(pIocp, 777u, NULL) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_USER, 777u,
			&Event, 2000000ull) ||
		!xrtNetPortWake(pIocp) ||
		!exampleWaitFor(pIocp, XNET_PORT_EVENT_WAKE, 0u,
			&Event, 2000000ull) ) {
		goto Cleanup;
	}
	printf("port: post USER + wake WAKE ok\n");

	/* ---- RecvError：Windows IOCP 不支持数据报错误等待——
	 * 提交被拒并给出明确错误；Linux URING 上可受理。 ---- */
	if ( xrtNetPortRecvError(pIocp, UdpA, arrBuf, 16u, 300u, NULL) ) {
		goto Cleanup; /* 本平台不应受理。 */
	}
	printf("port: recv-error gated by platform ok\n");
	iResult = 0;

Cleanup:
	xrtNetSocketClose(Server);
	xrtNetSocketClose(Client);
	xrtNetSocketClose(Listener);
	xrtNetSocketClose(UdpB);
	xrtNetSocketClose(UdpA);
	if ( pSelect != NULL ) {
		xrtNetPortDestroy(pSelect);
	}
	if ( pIocp != NULL ) {
		xrtNetPortDestroy(pIocp);
	}
	return iResult;
}
