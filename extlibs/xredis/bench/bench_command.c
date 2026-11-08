#include <xredis.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
	uint64 Count = argc > 1 ? strtoull(argv[1], NULL, 10) : 1000000u;
	unsigned char Payload[64] = {0}, Output[128];
	xbytesview Arguments[] = {XRT_BYTES_LITERAL("SET"), XRT_BYTES_LITERAL("key"),
		{Payload, sizeof(Payload)}};
	uint64 Checksum = 0; size_t Written = 0;
	if (Count == 0 || Count > UINT64_MAX / sizeof(Output)) return 1;
	double Start = xrtTimer();
	for (uint64 Index = 0; Index < Count; ++Index) {
		Payload[0] = (unsigned char)Index;
		if (!xredisCommandWrite(Output, sizeof(Output), Arguments, 3, &Written)) return 2;
		Checksum += Output[Written - sizeof(Payload) - 2u];
	}
	double Seconds = xrtTimer() - Start;
	if (!(Seconds > 0.0)) return 3;
	printf("commands_per_sec: %.3f\n", (double)Count / Seconds);
	printf("wire_bytes_per_sec: %.3f\n", (double)Count * (double)Written / Seconds);
	printf("checksum: %" PRIu64 "\n", Checksum);
	return 0;
}
