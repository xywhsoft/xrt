#define XREDIS_MODULE_XREDIS
#include <xredis.h>
#include <string.h>
#include "../support/client_peer.h"

static xredisreadaction readReply(const xredisreadevent* Event, ptr User)
{
	size_t* Values = (size_t*)User;
	if (Event->Kind == XREDIS_READ_END) ++*Values;
	return XREDIS_READ_NEXT;
}

int main(void)
{
	xbytesview Argument = XRT_BYTES_LITERAL("PING");
	unsigned char Output[14]; size_t Written;
	if (!xredisCommandWrite(Output, sizeof(Output), &Argument, 1, &Written)) return 1;
	if (Written != sizeof(Output) || memcmp(Output, "*1\r\n$4\r\nPING\r\n", Written)) return 2;
	if (xredisCommandSize(NULL, 1, &Written)) return 3;
	if (xrtErrorKind(xrtGetError()) != XERR_ARGUMENT ||
		strcmp(xrtErrorDomain(xrtGetError()), "xrt.redis") != 0) return 4;
	xrtClearError();
	xredisreader* Reader = NULL; size_t Values = 0, Consumed = 0;
	if (!xredisReaderCreate(NULL, readReply, &Values, &Reader)) return 5;
	const xbytesview Reply = XRT_BYTES_LITERAL("|0\r\n*2\r\n:1\r\n$?\r\n;2\r\nOK\r\n;0\r\n");
	if (xredisReaderFeed(Reader, Reply, &Consumed) != XREDIS_READER_READY || Consumed != Reply.Size || Values != 4) {
		xredisReaderDestroy(Reader); return 6;
	}
	if (!xredisReaderFinish(Reader) || !xredisReaderDestroy(Reader)) return 7;
	xredisreplyreader* Owned = NULL; xredisreply* Frame = NULL;
	if (!xredisReplyReaderCreate(NULL, &Owned)) return 8;
	if (xredisReplyReaderFeed(Owned, XRT_BYTES_LITERAL("|0\r\n*2\r\n$2\r\nOK\r\n,1.25\r\n"), &Consumed, &Frame) !=
		XREDIS_REPLY_READY || !xredisReplyReaderFinish(Owned)) return 9;
	xredisReplyReaderDestroy(Owned);
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisreplyvalue Root, Text;
	if (!xredisReplyInfo(Frame, &Info) || !xredisReplyGet(Frame, Info.Root, &Root) || Root.Attributes != 0 ||
		!xredisReplyGet(Frame, Root.FirstChild, &Text) || Text.Data.Size != 2 || memcmp(Text.Data.Data, "OK", 2)) return 10;
	double Number;
	if (!xredisReplyDouble(Frame, Text.Next, &Number) || Number != 1.25 || !xredisReplyRetain(Frame)) return 11;
	xredisReplyRelease(Frame); xredisReplyRelease(Frame);
	redispeer Peer; redisPeerStart(&Peer); xredisclient* Client = NULL;
	if (!xredisClientConnect(Peer.Engine, &Peer.Address, 0, NULL, &Client)) return 12;
	xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Future = NULL;
	if (xredisClientCall(Client, &Ping, 1, 5000, &Future) != XREDIS_ADMISSION_ACCEPTED) return 13;
	const xredisrequestresult* Result = (const xredisrequestresult*)xrtFutureValue(Future);
	if (Result == NULL || Result->Status != XREDIS_REQUEST_OK || Result->Delivery != XREDIS_REPLIED) return 14;
	xfuture* Closed = xredisClientClosed(Client);
	if (!xredisClientClose(Client) || xrtFutureWaitFor(Closed, 5000) != XWAIT_OK) return 15;
	xredisClientRelease(Client); xrtFutureDestroy(Closed); redisPeerStop(&Peer);
	if (!xredisReplyGet(Result->Reply, 0, &Text) || Text.Data.Size != 4 || memcmp(Text.Data.Data, "PONG", 4)) return 16;
	xrtFutureDestroy(Future);
	return 0;
}
