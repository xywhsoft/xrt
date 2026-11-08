#define XRT_MODULE_JSON_READ
#define XRT_MODULE_TIME
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static xjsonvisitaction benchJsonDecoded(const xjsonevent* Event, ptr User)
{
	uint64* Checksum = (uint64*)User;
	*Checksum += Event->Type + Event->Depth + Event->Name.Size + 1u;
	return XJSON_VISIT_NEXT;
}

/* The local A/B probe can compile the identical existing-Visit workload
 * against an earlier generated header, without needing the new Scan API. */
#if !defined(XRT_BENCH_JSON_VISIT_ONLY)
static xjsonvisitaction benchJsonRaw(const xjsonscanevent* Event, ptr User)
{
	uint64* Checksum = (uint64*)User;
	*Checksum += Event->Type + Event->Depth + Event->Name.Size + 1u;
	return XJSON_VISIT_NEXT;
}
#endif

int main(int argc, char** argv)
{
	uint64 Count = argc > 1 ? strtoull(argv[1], NULL, 10) : 100000u, Checksum = 0;
	static const char Plain[] = "{\"method\":\"echo\",\"id\":12345,\"params\":[\"abcdef\",true,{\"x\":[1,2,3]}]}";
	static const char Escaped[] = "{\"me\\u0074hod\":\"ec\\u0068o\",\"id\":12345,\"params\":[\"\\u0061\\u0062\\u0063\\u0064\\u0065\\u0066\",true,{\"x\":[1,2,3]}]}";
	bool UsePlain = argc > 2 && argv[2][0] == 'p';
	xstrview Input = {UsePlain ? Plain : Escaped, UsePlain ? sizeof(Plain)-1u : sizeof(Escaped)-1u};
	xjsonreadconfig Config; xrtJsonReadConfigInit(&Config);
	if (Count == 0 || Count > 1000000000u) return 1;
	double Start = xrtTimer();
	for (uint64 Index = 0; Index < Count; ++Index)
		if (xrtJsonVisit(Input, &Config, benchJsonDecoded, &Checksum) != XJSON_VISIT_DONE) return 2;
	double Seconds = xrtTimer() - Start;
	if (!(Seconds > 0.0)) return 3;
	printf("visits_per_sec: %.3f\n", (double)Count / Seconds);
	#if !defined(XRT_BENCH_JSON_VISIT_ONLY)
	Start = xrtTimer();
	for (uint64 Index = 0; Index < Count; ++Index)
		if (xrtJsonScan(Input, &Config, benchJsonRaw, &Checksum) != XJSON_VISIT_DONE) return 4;
	Seconds = xrtTimer() - Start;
	if (!(Seconds > 0.0)) return 5;
	printf("scans_per_sec: %.3f\n", (double)Count / Seconds);
	#endif
	printf("checksum: %" PRIu64 "\n", Checksum);
	return 0;
}
