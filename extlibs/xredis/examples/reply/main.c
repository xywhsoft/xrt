#define XREDIS_MODULE_XREDIS_REPLY
#include <xredis.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	xredisreplyreader* Reader = NULL; xredisreply* Reply = NULL;
	if (!xredisReplyReaderCreate(NULL, &Reader)) return 1;
	unsigned char First[] = "|0\r\n*2\r\n$5\r\nhello\r\n,";
	unsigned char Second[] = "1.25\r\n";
	size_t Consumed;
	if (xredisReplyReaderFeed(Reader, (xbytesview){First, sizeof(First) - 1u}, &Consumed, &Reply) != XREDIS_REPLY_MORE ||
		Consumed != sizeof(First) - 1u || Reply != NULL) return 2;
	memset(First, 0, sizeof(First));
	if (xredisReplyReaderFeed(Reader, (xbytesview){Second, sizeof(Second) - 1u}, &Consumed, &Reply) != XREDIS_REPLY_READY ||
		Consumed != sizeof(Second) - 1u || !xredisReplyReaderFinish(Reader)) return 3;
	memset(Second, 0, sizeof(Second)); xredisReplyReaderDestroy(Reader);
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisreplyvalue Root, Text;
	if (!xredisReplyInfo(Reply, &Info) || !xredisReplyGet(Reply, Info.Root, &Root) ||
		Root.Attributes != 0 || !xredisReplyGet(Reply, Root.FirstChild, &Text)) return 4;
	double Number;
	if (!xredisReplyDouble(Reply, Text.Next, &Number) || Number != 1.25 || Text.Data.Size != 5 ||
		memcmp(Text.Data.Data, "hello", 5) != 0) return 5;
	printf("owned reply: %.*s, %.2f; %zu nodes, %zu payload bytes, %zu peak requested bytes\n",
		(int)Text.Data.Size, (cstr)Text.Data.Data, Number, Info.Nodes, Info.PayloadBytes, Info.PeakBytes);
	xredisReplyRelease(Reply); return 0;
}
