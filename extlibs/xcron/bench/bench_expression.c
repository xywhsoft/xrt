#include <xcron.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
	uint64 Count = argc > 1 ? strtoull(argv[1], NULL, 10) : 1000000u, Checksum = 0;
	char Text[] = "*/5 9-17/2 1,15 JAN-MAR MON-FRI";
	if (Count == 0 || Count > 1000000000u) return 1;
	double Start = xrtTimer();
	for (uint64 Index = 0; Index < Count; ++Index) {
		xcronexpression Expression; Text[2] = (char)('1' + Index % 9u);
		if (!xcronExpressionCompile((xstrview){Text, sizeof(Text) - 1u}, XCRON_UNIX5, &Expression, NULL)) return 2;
		Checksum ^= Expression.Fields[XCRON_FIELD_MINUTE] + Index;
	}
	double Seconds = xrtTimer() - Start;
	if (!(Seconds > 0.0)) return 3;
	printf("expressions_per_sec: %.3f\n", (double)Count / Seconds);
	printf("checksum: %" PRIu64 "\n", Checksum);
	return 0;
}
