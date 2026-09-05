#include "../../fuzz/tls_state.c"

#ifndef XRT_TLS_STATE_FUZZ_ROUNDS
#define XRT_TLS_STATE_FUZZ_ROUNDS 200u
#endif

int main(void)
{
	static const uint8 Trace[] = {
		0, 11, 0, 2, 12, 12, 0, 3, 32, 4, 0, 12, 0,
		5, 32, 12, 0, 8, 5, 12, 0, 6, 0, 12, 0, 9, 0
	};
	uint8 Bytes[512];
	test_tls_server_rng Rng = { UINT32_C(0x74514D38) };

	(void)xrtTlsStateFuzzerTestOneInput(NULL, 0);
	(void)xrtTlsStateFuzzerTestOneInput(Trace, sizeof(Trace));
	for ( size_t i = 0; i < XRT_TLS_STATE_FUZZ_ROUNDS; i++ ) {
		for ( size_t j = 0; j < sizeof(Bytes); j++ ) {
			Bytes[j] = (uint8)testTlsServerRandom(&Rng);
		}
		(void)xrtTlsStateFuzzerTestOneInput(Bytes, sizeof(Bytes));
	}
	xrtTlsStateFuzzerCleanup();
	testMemoryDebugDrain("TLS state fuzz retained live allocations");
	printf("[PASS] TLS state fuzz: %u random traces\n", XRT_TLS_STATE_FUZZ_ROUNDS);
	return 0;
}
