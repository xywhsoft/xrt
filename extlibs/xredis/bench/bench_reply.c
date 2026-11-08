#include <xredis.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
	uint64 Count = argc > 1 ? strtoull(argv[1], NULL, 10) : 1000000u;
	uint64 Checksum = 0, WireBytes = 0; xredisreplyreader* Reader = NULL;
	const xbytesview Wires[] = {XRT_BYTES_LITERAL("+OK\r\n"),
		XRT_BYTES_LITERAL("|0\r\n*3\r\n:123\r\n$64\r\n0123456789012345678901234567890123456789012345678901234567890123\r\n#t\r\n"),
		XRT_BYTES_LITERAL("*?\r\n$?\r\n;3\r\na\0b\r\n;3\r\ncde\r\n;0\r\n,1.25\r\n.\r\n")};
	if (Count == 0 || Count > UINT64_MAX / 1024u || !xredisReplyReaderCreate(NULL, &Reader)) return 1;
	double Start = xrtTimer();
	for (uint64 Index = 0; Index < Count; ++Index) {
		xredisreply* Reply = NULL; size_t Consumed;
		xbytesview Wire = Wires[Index % 3u];
		if (xredisReplyReaderFeed(Reader, Wire, &Consumed, &Reply) != XREDIS_REPLY_READY || Consumed != Wire.Size) return 2;
		xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisreplyvalue Value;
		if (!xredisReplyInfo(Reply, &Info) || !xredisReplyGet(Reply, Info.Root, &Value)) return 3;
		Checksum += Info.Nodes + Info.PayloadBytes + Value.Length;
		if (Index % 3u == 2) {
			xredisreplyvalue Text; double Number;
			if (!xredisReplyGet(Reply, Value.FirstChild, &Text) || !xredisReplyDouble(Reply, Text.Next, &Number)) return 4;
			Checksum += (uint64)(Number * 100.0);
		}
		xredisReplyRelease(Reply); WireBytes += Wire.Size;
	}
	double Seconds = xrtTimer() - Start;
	bool Complete = xredisReplyReaderFinish(Reader); xredisReplyReaderDestroy(Reader);
	if (!Complete || !(Seconds > 0.0)) return 5;
	printf("frames_per_sec: %.3f\n", (double)Count / Seconds);
	printf("wire_bytes_per_sec: %.3f\n", (double)WireBytes / Seconds);
	printf("checksum: %" PRIu64 "\n", Checksum); return 0;
}
