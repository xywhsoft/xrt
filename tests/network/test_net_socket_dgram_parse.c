#include "../test.h"

#include "../../src/internal/xrt_net_socket.h"



#if defined(_WIN32) || defined(_WIN64)

#ifndef UDP_COALESCED_INFO
	#define UDP_COALESCED_INFO 3
#endif



/* 人工构造 Winsock 合并控制消息，压实无需真实网卡卸载的解析边界。 */
static void testSocketDgramCoalescedParse(void)
{
	struct xnetsocket_impl Socket;
	xnetdgrammeta Meta;
	WSAMSG Message;
	WSACMSGHDR* pHeader;
	union {
		uint64 Align;
		unsigned char Data[WSA_CMSG_SPACE(sizeof(uint32))];
	} Control;
	uint32 iSegment = 1200;

	memset(&Socket, 0, sizeof(Socket));
	memset(&Message, 0, sizeof(Message));
	memset(&Control, 0, sizeof(Control));
	Message.Control.buf = (CHAR*)Control.Data;
	Message.Control.len = (ULONG)sizeof(Control.Data);
	pHeader = WSA_CMSG_FIRSTHDR(&Message);
	testRequire(pHeader != NULL,
		"constructing Windows UDP coalesced control failed");
	pHeader->cmsg_len = WSA_CMSG_LEN(sizeof(iSegment));
	pHeader->cmsg_level = IPPROTO_UDP;
	pHeader->cmsg_type = UDP_COALESCED_INFO;
	memcpy(WSA_CMSG_DATA(pHeader), &iSegment, sizeof(iSegment));

	Socket.DgramMeta = XNET_DGRAM_META_SEGMENT_SIZE;
	__xrtNetSocketDgramMetaParse(
		&Socket,
		&Meta,
		Control.Data,
		sizeof(Control.Data),
		0
	);
	testRequire(
		((Meta.Flags & XNET_DGRAM_META_SEGMENT_SIZE) != 0) &&
		(Meta.SegmentSize == iSegment),
		"Windows UDP coalesced control parse mismatch"
	);

	Socket.DgramMeta = 0;
	__xrtNetSocketDgramMetaParse(
		&Socket,
		&Meta,
		Control.Data,
		sizeof(Control.Data),
		0
	);
	testRequire((Meta.Flags == 0) && (Meta.SegmentSize == 0),
		"disabled Windows UDP coalesced metadata leaked");

	Socket.DgramMeta = XNET_DGRAM_META_SEGMENT_SIZE;
	iSegment = 0;
	memcpy(WSA_CMSG_DATA(pHeader), &iSegment, sizeof(iSegment));
	__xrtNetSocketDgramMetaParse(
		&Socket,
		&Meta,
		Control.Data,
		sizeof(Control.Data),
		0
	);
	testRequire((Meta.Flags == 0) && (Meta.SegmentSize == 0),
		"zero Windows UDP coalesced segment was published");
}

#endif



/* 执行无需网络流量的平台数据报控制消息解析回归。 */
int main(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		testSocketDgramCoalescedParse();
	#else
		(void)testRequire;
	#endif
	return 0;
}
