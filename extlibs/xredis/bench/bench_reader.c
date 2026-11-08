#include <xredis.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static xredisreadaction countReply(const xredisreadevent* Event, ptr User)
{
	uint64* Checksum = (uint64*)User;
	if (Event->Kind == XREDIS_READ_DATA) *Checksum += Event->Data.Data[0] + Event->Data.Size;
	else if (Event->Kind == XREDIS_READ_END) *Checksum += Event->Length;
	return XREDIS_READ_NEXT;
}

int main(int argc, char** argv)
{
	uint64 Count = argc > 1 ? strtoull(argv[1], NULL, 10) : 1000000u;
	uint64 Checksum = 0; xredisreader* Reader = NULL;
	unsigned char Wire[] = "*3\r\n:123\r\n$64\r\n"
		"0123456789012345678901234567890123456789012345678901234567890123\r\n#t\r\n";
	if (Count == 0 || Count > UINT64_MAX / sizeof(Wire) || !xredisReaderCreate(NULL, countReply, &Checksum, &Reader)) return 1;
	double Start = xrtTimer();
	for (uint64 Index = 0; Index < Count; ++Index) {
		Wire[15] = (unsigned char)('0' + Index % 10u); size_t Consumed;
		if (xredisReaderFeed(Reader, (xbytesview){Wire, sizeof(Wire) - 1u}, &Consumed) != XREDIS_READER_READY ||
			Consumed != sizeof(Wire) - 1u) { xredisReaderDestroy(Reader); return 2; }
	}
	double Seconds = xrtTimer() - Start;
	bool Complete = xredisReaderFinish(Reader); xredisReaderDestroy(Reader);
	if (!Complete || !(Seconds > 0.0)) return 3;
	printf("frames_per_sec: %.3f\n", (double)Count / Seconds);
	printf("wire_bytes_per_sec: %.3f\n", (double)Count * (double)(sizeof(Wire) - 1u) / Seconds);
	printf("checksum: %" PRIu64 "\n", Checksum);
	return 0;
}
