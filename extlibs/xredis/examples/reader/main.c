#include <xredis.h>
#include <stdio.h>

static xredisreadaction onReply(const xredisreadevent* Event, ptr User)
{
	(void)User;
	if (Event->Kind == XREDIS_READ_BEGIN)
		printf("begin type=%d depth=%zu\n", (int)Event->Type, Event->Depth);
	else if (Event->Kind == XREDIS_READ_DATA) {
		printf("data (%zu bytes): ", Event->Data.Size);
		if (fwrite(Event->Data.Data, 1, Event->Data.Size, stdout) != Event->Data.Size) return XREDIS_READ_FAIL;
		putchar('\n');
	} else printf("end type=%d depth=%zu length=%llu\n", (int)Event->Type,
		Event->Depth, (unsigned long long)Event->Length);
	return XREDIS_READ_NEXT;
}

int main(void)
{
	xredisreader* Reader = NULL;
	/* One semantic frame: root attribute followed by a streamed array. */
	const xbytesview Wire = XRT_BYTES_LITERAL("|1\r\n+ttl\r\n:60\r\n*?\r\n$5\r\nhello\r\n#t\r\n.\r\n");
	if (!xredisReaderCreate(NULL, onReply, NULL, &Reader)) return 1;
	size_t Offset = 0, Frames = 0;
	while (Offset < Wire.Size) {
		size_t Part = Wire.Size - Offset; if (Part > 3) Part = 3; /* deliberately fragmented */
		size_t Consumed = 0;
		xredisreadstatus Status = xredisReaderFeed(Reader, (xbytesview){Wire.Data + Offset, Part}, &Consumed);
		if (Status == XREDIS_READER_ERROR) { xredisReaderDestroy(Reader); return 2; }
		Offset += Consumed;
		if (Status == XREDIS_READER_READY) ++Frames;
	}
	bool Complete = Frames == 1 && xredisReaderFinish(Reader);
	xredisReaderDestroy(Reader);
	return Complete ? 0 : 3;
}
