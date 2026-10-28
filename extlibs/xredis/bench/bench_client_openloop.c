#include "openloop_run.h"
#include <errno.h>

static bool redisOpenArgument(cstr Text, uint64* Value)
{
	if (Text[0] < '0' || Text[0] > '9') return false;
	char* End = NULL; errno = 0; unsigned long long Parsed = strtoull(Text, &End, 10);
	if (errno != 0 || *End != 0 || Parsed > UINT64_MAX) return false;
	*Value = (uint64)Parsed; return true;
}

int main(int argc, char** argv)
{
	redisopenconfig Config = redisOpenDefaults(); uint64 Args[9] = {Config.Count, Config.Rate, Config.Slots, Config.Payload, (uint64)Config.Timeout, 0, 0, 0, 0};
	if (argc > 10) return 2;
	for (int Index = 1; Index < argc; ++Index) if (!redisOpenArgument(argv[Index], &Args[Index - 1])) return 2;
	if (Args[0] > SIZE_MAX || Args[2] > SIZE_MAX || Args[3] > SIZE_MAX || Args[4] > INT64_MAX ||
		Args[5] > 2 || Args[6] > 5000 || Args[7] > UINT16_MAX || Args[8] > 1) return 2;
	Config = (redisopenconfig){(size_t)Args[0], (size_t)Args[2], (size_t)Args[3], Args[1], (int64)Args[4], (unsigned)Args[5], (uint32)Args[6], (uint32)Args[7], Args[8] != 0};
	redisopenresult Result; if (!redisOpenConfigValid(&Config)) return 2;
#if !defined(XRT_FEATURE_MEMORY_STATS)
	if (Config.Stats) return 2;
#endif
	bool Valid = redisOpenRun(&Config, &Result); redisopencounts* C = &Result.Counts;
	printf("arrival_model=fixed_absolute\ntarget=%s\nnetwork_backend=%s\n", Config.Port == 0 ? "local_fixture" : "external_loopback", Result.Backend);
	printf("memory_stats=%s\n", Config.Stats ? "enabled" : Result.StatsCompiled ? "compiled_disabled" : "not_compiled");
	printf("generator_mode=%u\nworkers=%u\nslot_capacity=%zu\npayload_bytes=%zu\nmax_inflight=%zu\n", Config.Mode, Config.Port == 0 ? 2u : 1u, Config.Slots, Config.Payload, Config.Slots < 128u ? Config.Slots : 128u);
	printf("target_rate_per_sec=%" PRIu64 "\nintended_timeout_ms=%" PRId64 "\n", Config.Rate, Config.Timeout);
	printf("offered: %zu\naccepted: %zu\nsuccessful: %zu\nunsuccessful: %zu\n", C->Total, C->Accepted, C->Successful, C->Total - C->Successful);
	printf("harness_capacity: %zu\nclient_overloaded: %zu\nclient_closed: %zu\nadmission_error: %zu\nexpired_before_admission: %zu\n",
		C->Outcomes[REDIS_OPEN_CAPACITY], C->Outcomes[REDIS_OPEN_OVERLOADED], C->Outcomes[REDIS_OPEN_CLOSED], C->Outcomes[REDIS_OPEN_ERROR], C->Outcomes[REDIS_OPEN_EXPIRED]);
	static const cstr Status[] = {"ok", "server_error", "timeout", "cancelled", "closed", "disconnected", "protocol", "limit", "memory", "initialization"};
	for (size_t Index = 0; Index <= XREDIS_REQUEST_INITIALIZATION; ++Index) printf("request_%s: %zu\n", Status[Index], C->Status[Index]);
	printf("corrupt: %zu\ndeadline_missed: %zu\n", C->Corrupt, C->Missed);
	printf("success_requests_per_sec: %.3f\noffered_requests_per_sec: %.3f\n", (double)C->Successful / Result.Elapsed, (double)C->Total / Result.Elapsed);
	static const unsigned Quantiles[] = {50, 95, 99, 999};
	for (size_t Index = 0; Index < 4; ++Index) printf("success_p%u_us: %.3f\nall_p%u_us: %.3f\ngenerator_p%u_us: %.3f\n", Quantiles[Index], Result.Success[Index], Quantiles[Index], Result.All[Index], Quantiles[Index], Result.Lag[Index]);
	printf("logical_seconds: %.9f\nretire_seconds: %.9f\nprocess_window_cpu_seconds: %.6f\n", Result.Elapsed, Result.Retire, Result.Cpu);
	printf("harness_bytes: %zu\nrequests_peak_sampled: %zu\ncommand_bytes_peak_sampled: %zu\n", Result.HarnessBytes, Result.RequestsPeak, Result.CommandBytesPeak);
	printf("process_peak_rss_bytes: %" PRIu64 "\nhandles_before: %" PRIu64 "\nhandles_after: %" PRIu64 "\n", Result.RssPeak, Result.HandlesBefore, Result.HandlesAfter);
	printf("window_malloc_calls: %" PRIu64 "\nwindow_calloc_calls: %" PRIu64 "\nwindow_realloc_calls: %" PRIu64 "\nwindow_memdup_calls: %" PRIu64 "\n", Result.Malloc, Result.Calloc, Result.Realloc, Result.MemDup);
	printf("window_block_alloc_calls: %" PRIu64 "\nwindow_backing_alloc_calls: %" PRIu64 "\nwindow_backing_alloc_bytes: %" PRIu64 "\n", Result.BlockAlloc, Result.BackingAlloc, Result.BackingBytes);
	printf("accepted_payload_copy_bytes_lower_bound: %" PRIu64 "\nreply_payload_copy_bytes_lower_bound: %" PRIu64 "\n", (uint64)C->Accepted * Config.Payload, (uint64)C->Successful * Config.Payload);
	return Valid ? 0 : 1;
}
