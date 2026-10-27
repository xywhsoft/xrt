#define XREDIS_MODULE_XREDIS
#include <xredis.h>
#include <string.h>

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
	return 0;
}
