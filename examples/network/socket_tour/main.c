/*
 * 范例：network/socket_tour —— 同步 Socket 层自省/多播/向量与批量 IO
 * ----------------------------------------------------------------
 * 演示 API：
 *   【自省】      xrtNetSocketNative（借用句柄）
 *                 xrtNetSocketFamily / Type（创建时确定）
 *                 xrtNetSocketAvailable（可读字节数）
 *                 xrtNetSocketSet / Get（通用选项）
 *                 xrtNetSocketDgramMetaEnabled（已启用元数据位）
 *   【多播】      xrtNetSocketMulticastJoin / Leave /
 *                 Loop / HopLimit / Interface（环回自收配方）
 *   【向量 IO】   xrtNetSocketSendToVec / RecvFromVec（数据报）
 *                 xrtNetSocketSendVec / RecvVec（连接式）
 *                 xrtNetSocketSendMsgVec / RecvMsg / RecvMsgVec
 *                 xrtNetSocketDgramRecvError（空队列 AGAIN）
 *   【批量】      xrtNetSocketSendBatch / RecvBatch（一次 64 个）
 *   【连接】      xrtNetSocketFinishConnect（非阻塞收口）
 *                 xrtNetSocketRemote（已连接对端）
 * 模块宏：XRT_MODULE_NET_SOCKET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/socket_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   socket: open family/type/native + set/get + meta=0 ok
 *   socket: sendtovec/recvfromvec 4 bytes ok
 *   socket: recvmsg + sendmsgvec/recvmsgvec ok
 *   socket: connect sendvec/recvvec + remote ok
 *   socket: batch 2+2 datagrams ok
 *   socket: multicast loopback self-receive ok
 *   socket: tcp finishconnect + remote ok
 *   socket: dgram error queue gated by platform ok
 *
 * 两个 UDP 套接字在 127.0.0.1 上互发；多播段沿用
 *   "Loop+HopLimit+Interface+Join 全指环回"的单成员自收配方；
 *   TCP 段演示非阻塞 Connect 后用 FinishConnect 收口。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

/* 非阻塞连接轮询：FinishConnect 直到 OK/ERROR 或超时。 */
static xnetresult exampleFinishConnect(xnetsocket Socket, int iSpinMs)
{
	xnetresult Result = XNET_RESULT_AGAIN;

	for ( int i = 0; i < iSpinMs; i++ ) {
		Result = xrtNetSocketFinishConnect(Socket);
		if ( Result != XNET_RESULT_AGAIN ) {
			return Result;
		}
		xrtSleep(1u);
	}
	return Result;
}

int main(void)
{
	xnetsocket A = 0;
	xnetsocket B = 0;
	xnetsocket L = 0;
	xnetsocket C = 0;
	xnetaddr AddrA;
	xnetaddr AddrB;
	xnetaddr DestB;
	xnetaddr Group;
	xnetaddr Iface;
	xnetaddr From;
	xnetaddr Remote;
	xnetdgramrecv Recv[2];
	xnetdgramsend Send[2];
	xnetdgrammeta Meta;
	xnetdgramcontrol Control;
	xnetdgramerror DgramError;
	xnetspan Out[2];
	xnetwspan In[2];
	uint8 arrBuf[64];
	int64 iValue = 0;
	size_t iGot = 0;
	size_t iSent = 0;
	int iResult = 1;

	/* ---- 打开与自省：族/类型/句柄 + 选项往返 + 元数据默认零。 ---- */
	A = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_DGRAM, 0u);
	B = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_DGRAM, 0u);
	if ( (A == 0) || (B == 0) ||
		(xrtNetSocketFamily(A) != XNET_FAMILY_IPV4) ||
		(xrtNetSocketType(A) != XNET_SOCKET_DGRAM) ||
		(xrtNetSocketNative(A) == 0) ||
		!xrtNetSocketSet(A, XNET_OPTION_RECEIVE_BUFFER, 65536) ||
		!xrtNetSocketGet(A, XNET_OPTION_RECEIVE_BUFFER, &iValue) ||
		(iValue <= 0) ||
		(xrtNetSocketDgramMetaEnabled(A) != 0u) ||
		/* 空接收队列上 Available 成功返回 0。 */
		!xrtNetSocketAvailable(A, &iGot) ||
		(iGot != 0u) ) {
		goto Cleanup;
	}
	/* 通配绑定让系统分配端口，Local 读回实际端口。
	 * 注意：A 绑定后 AddrA 已携带具体端口，B 须用新的通配地址。 */
	if ( !xrtNetAddrAny(&AddrA, XNET_FAMILY_IPV4, 0u) ||
		!xrtNetSocketBind(A, &AddrA) ||
		!xrtNetSocketLocal(A, &AddrA) ||
		(AddrA.Port == 0u) ||
		!xrtNetAddrAny(&AddrB, XNET_FAMILY_IPV4, 0u) ||
		!xrtNetSocketBind(B, &AddrB) ||
		!xrtNetSocketLocal(B, &AddrB) ||
		(AddrB.Port == 0u) ||
		/* 通配绑定的 Local 是 0.0.0.0，不能当发送目标；
		 * B 的实际可达地址 = 环回地址 + B 的端口。 */
		!xrtNetAddrLoopback(&DestB, XNET_FAMILY_IPV4, AddrB.Port) ) {
		goto Cleanup;
	}
	printf("socket: open family/type/native + set/get + meta=0 ok\n");

	/* ---- 数据报向量：SendToVec 两段聚集 → RecvFromVec 两段分散。 ---- */
	Out[0].Data = (cbytes)"ab";
	Out[0].Size = 2u;
	Out[1].Data = (cbytes)"cd";
	Out[1].Size = 2u;
	In[0].Data = arrBuf;
	In[0].Size = 2u;
	In[1].Data = arrBuf + 2;
	In[1].Size = 2u;
	if ( (xrtNetSocketSendToVec(A, Out, 2u, &iSent, &DestB) !=
			XNET_RESULT_OK) ||
		(iSent != 4u) ||
		(xrtNetSocketRecvFromVec(B, In, 2u, &iGot, &From) !=
			XNET_RESULT_OK) ||
		(iGot != 4u) ||
		(memcmp(arrBuf, "abcd", 4u) != 0) ||
		(From.Port != AddrA.Port) ) {
		goto Cleanup;
	}
	printf("socket: sendtovec/recvfromvec 4 bytes ok\n");

	/* ---- 消息形态：SendTo → RecvMsg；带控制 SendMsgVec → RecvMsgVec。
	 * RecvMsg 族要求非空 Meta 出参（传 NULL 是参数错误）。 ---- */
	if ( (xrtNetSocketSendTo(A, "msg", 3u, &iSent, &DestB) !=
			XNET_RESULT_OK) ||
		(iSent != 3u) ||
		(xrtNetSocketRecvMsg(B, arrBuf, 64u, &iGot, &From, &Meta) !=
			XNET_RESULT_OK) ||
		(iGot != 3u) ||
		(memcmp(arrBuf, "msg", 3u) != 0) ) {
		goto Cleanup;
	}
	memset(&Control, 0, sizeof(Control));
	Out[0].Data = (cbytes)"m1";
	Out[0].Size = 2u;
	Out[1].Data = (cbytes)"m2";
	Out[1].Size = 2u;
	if ( (xrtNetSocketSendMsgVec(A, Out, 2u, &iSent, &DestB,
			&Control) != XNET_RESULT_OK) ||
		(iSent != 4u) ||
		(xrtNetSocketRecvMsgVec(B, In, 2u, &iGot, &From, &Meta) !=
			XNET_RESULT_OK) ||
		(iGot != 4u) ||
		(memcmp(arrBuf, "m1m2", 4u) != 0) ) {
		goto Cleanup;
	}
	/* 启用 Hop Limit 元数据：Enabled 反映实际位，收包带 HopLimit。 */
	if ( !xrtNetSocketDgramMetaSet(B, XNET_DGRAM_META_HOP_LIMIT) ||
		(xrtNetSocketDgramMetaEnabled(B) !=
			XNET_DGRAM_META_HOP_LIMIT) ||
		(xrtNetSocketSendTo(A, "m3", 2u, &iSent, &DestB) !=
			XNET_RESULT_OK) ||
		(xrtNetSocketRecvMsg(B, arrBuf, 64u, &iGot, &From, &Meta) !=
			XNET_RESULT_OK) ||
		(iGot != 2u) ||
		(Meta.Flags != XNET_DGRAM_META_HOP_LIMIT) ||
		(Meta.HopLimit <= 0) ) {
		goto Cleanup;
	}
	printf("socket: recvmsg + sendmsgvec/recvmsgvec ok\n");

	/* ---- 连接式 UDP：Connect 后 SendVec/RecvVec + Remote。 ---- */
	if ( (xrtNetSocketConnect(A, &DestB) != XNET_RESULT_OK) ||
		!xrtNetSocketRemote(A, &Remote) ||
		!xrtNetAddrEqual(&Remote, &DestB) ) {
		goto Cleanup;
	}
	Out[0].Data = (cbytes)"xy";
	Out[0].Size = 2u;
	Out[1].Data = (cbytes)"z";
	Out[1].Size = 1u;
	In[0].Data = arrBuf;
	In[0].Size = 3u;
	In[1].Data = arrBuf + 3;
	In[1].Size = 1u;
	if ( (xrtNetSocketSendVec(A, Out, 2u, &iSent) != XNET_RESULT_OK) ||
		(iSent != 3u) ||
		(xrtNetSocketRecvVec(B, In, 2u, &iGot) != XNET_RESULT_OK) ||
		(iGot != 3u) ||
		(memcmp(arrBuf, "xyz", 3u) != 0) ) {
		goto Cleanup;
	}
	printf("socket: connect sendvec/recvvec + remote ok\n");

	/* ---- 批量：一次提交两个数据报，一次收回两个。
	 * Windows 环回上第二个数据报可能晚于第一个可见——
	 * RecvBatch 返回的是"已到达前缀"，部分返回后继续补收。 ---- */
	Send[0].Remote = NULL; /* 空远端 → 连接式固定 Peer。 */
	Send[0].Data = "P1";
	Send[0].Size = 2u;
	Send[1].Remote = NULL;
	Send[1].Data = "P2";
	Send[1].Size = 2u;
	memset(Recv, 0, sizeof(Recv));
	Recv[0].Data = arrBuf;
	Recv[0].Capacity = 8u;
	Recv[1].Data = arrBuf + 8;
	Recv[1].Capacity = 8u;
	if ( (xrtNetSocketSendBatch(A, Send, 2u, &iSent) != XNET_RESULT_OK) ||
		(iSent != 2u) ||
		(xrtNetSocketRecvBatch(B, Recv, 2u, &iGot) != XNET_RESULT_OK) ) {
		goto Cleanup;
	}
	if ( iGot < 2u ) {
		size_t iMore = 0;

		if ( xrtNetSocketRecvBatch(B, Recv + iGot, 2u - iGot,
				&iMore) != XNET_RESULT_OK ) {
			goto Cleanup;
		}
		iGot += iMore;
	}
	if ( (iGot != 2u) ||
		(Recv[0].Result != XNET_RESULT_OK) ||
		(Recv[0].Size != 2u) ||
		(Recv[1].Result != XNET_RESULT_OK) ||
		(Recv[1].Size != 2u) ||
		(memcmp(arrBuf, "P1", 2u) != 0) ||
		(memcmp(arrBuf + 8, "P2", 2u) != 0) ) {
		goto Cleanup;
	}
	printf("socket: batch 2+2 datagrams ok\n");

	/* ---- 多播自收：Loop+Hop+Iface+Join 全指环回（Windows 配方）。 ---- */
	if ( !xrtNetAddrParse(&Group, "239.255.0.1", AddrB.Port) ||
		!xrtNetAddrLoopback(&Iface, XNET_FAMILY_IPV4, 0u) ||
		!xrtNetSocketMulticastLoop(A, true) ||
		!xrtNetSocketMulticastHopLimit(A, 1) ||
		!xrtNetSocketMulticastInterface(A, &Iface) ||
		!xrtNetSocketMulticastJoin(B, &Group, &Iface) ) {
		goto Cleanup;
	}
	{
		xnetresult McResult;

		McResult = xrtNetSocketSendTo(A, "m", 1u, &iSent, &Group);
		if ( (McResult == XNET_RESULT_OK) && (iSent == 1u) ) {
			McResult = xrtNetSocketRecvFrom(B, arrBuf, 8u, &iGot,
				&From);
		}
		if ( (McResult != XNET_RESULT_OK) || (iGot != 1u) ||
			(arrBuf[0] != 'm') ) {
			goto Cleanup;
		}
	}
	if ( !xrtNetSocketMulticastLeave(B, &Group, &Iface) ||
		!xrtNetSocketMulticastInterface(A, NULL) ) {
		goto Cleanup;
	}
	printf("socket: multicast loopback self-receive ok\n");

	/* ---- TCP 非阻塞连接：FinishConnect 轮询收口 + Remote。 ---- */
	L = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0u);
	C = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0u);
	if ( (L == 0) || (C == 0) ||
		!xrtNetAddrLoopback(&AddrA, XNET_FAMILY_IPV4, 0u) ||
		!xrtNetSocketBind(L, &AddrA) ||
		!xrtNetSocketListen(L, 1) ||
		!xrtNetSocketLocal(L, &AddrA) ||
		!xrtNetSocketSet(C, XNET_OPTION_NONBLOCK, 1) ) {
		goto Cleanup;
	}
	/* 环回连接可能立即完成（OK）或仍在途（AGAIN）——
	 * 只允许调用一次 Connect，AGAIN 交给 FinishConnect 收口。 */
	{
		xnetresult ConnResult = xrtNetSocketConnect(C, &AddrA);

		if ( (ConnResult != XNET_RESULT_OK) &&
			(ConnResult != XNET_RESULT_AGAIN) ) {
			goto Cleanup;
		}
		if ( exampleFinishConnect(C, 2000) != XNET_RESULT_OK ) {
			goto Cleanup;
		}
	}
	if ( !xrtNetSocketRemote(C, &Remote) ||
		!xrtNetAddrEqual(&Remote, &AddrA) ) {
		goto Cleanup;
	}
	printf("socket: tcp finishconnect + remote ok\n");

	/* ---- 数据报错误队列：平台能力门控。
	 * Windows 无 IP_RECVERR 等价物——DgramRecvError 恒返回
	 * ERROR("not supported on this platform")且 DGRAM_ERRORS
	 * 选项无法启用；Linux 上空队列返回 AGAIN。 ---- */
	{
		xnetresult ErrResult = xrtNetSocketDgramRecvError(A,
			arrBuf, sizeof(arrBuf), &iGot, &DgramError);

		if ( ErrResult == XNET_RESULT_OK ) {
			goto Cleanup; /* 没有注入错误却成功——异常。 */
		}
	}
	printf("socket: dgram error queue gated by platform ok\n");
	iResult = 0;

Cleanup:
	xrtNetSocketClose(C);
	xrtNetSocketClose(L);
	xrtNetSocketClose(B);
	xrtNetSocketClose(A);
	return iResult;
}
