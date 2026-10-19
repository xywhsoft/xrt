# Local ingress pressure beside authenticated streams

`tools/test_xrudp_ingress.py` builds a finite public-API C fixture with two
Workers, eight authenticated byte streams and real OS UDP. The fixture does
not interpose UDP admission or reach into private endpoint structures. The
Python producer targets only its returned IPv4/IPv6 loopback address.

The producer alternates a 512-byte malformed packet, a valid 144-byte HELLO,
and an 80-byte invalid encrypted packet naming an established connection ID
from a different source port. Pacing uses bounded 256-packet batches, avoiding
sub-tick Windows waits without changing the machine's timer resolution. The
default target is 6,000 packets/s for eight seconds per case; actual offered
rate and received rejection counters are recorded separately.

The fixture requires eight peers and no half-open peers throughout, an
unchanged reservation below the configured limit, send ledgers within 64
slots, exact stream contents based on every offset byte, and normal endpoint
and Engine shutdown. Every 500-ms sample must advance each stream; final
minimum/maximum stream progress must be at least 1/4. The owner Worker runs a
10-ms heartbeat; its maximum observed gap must remain under one second.

The producer must reach at least 60% of its target rate, the native rejection
counters must demonstrate receipt of the load, and the known-CID source-drop
counter must demonstrate that the third class reached that path. RETRY replies
must stay within the endpoint's per-IP budget. ICMP is tolerated only during
the final close interval. Native/observer phase disagreement invalidates the
run. A missing resource sample is allowed only for the last output/exit race,
and only a later normal terminal proof can complete that run.

Native process private memory must stay under 512 MiB and grow by less than
32 MiB; handles may grow by fewer than 16. Linux ASan/UBSan and leak detection
are enabled, with an explicit 8-MiB quarantine so allocator diagnostics do not
dominate the memory growth guard. CPU samples describe the aggregate fixture
(server and eight clients on two Workers), excluding the Python producer;
they are observations, not a server-only CPU benchmark.

Example from the XRT checkout:

```sh
python3 tools/test_xrudp_ingress.py --sdk /path/to/mdo-sdk --compiler clang --sanitize
```

Default builds require committed production, fixture, driver and imported
sampler bytes, and an SDK core equal to the committed XRT core. Development
mode is explicit and its records cannot substitute for committed acceptance.
Core objects and fixtures have source/compiler/flag fingerprints; executable
paths are immutable and reuse requires an explicit matching `--reuse`.
Windows/Linux records are `out/xrudp-ingress/acceptance-*.json`, with adjacent
build receipts and per-case transcripts. Failed fixture EOF reports its
captured native diagnostics rather than only a generic queue timeout.

The development matrix passed both address families and Select/IOCP on Windows
GCC and Select/Epoll on Linux Clang with sanitizers. Early fixture issues
(expected timer cancellation treated as an error, too-small Windows pacing
batches, and a final resource-sampling exit race) were corrected in the test
driver; they were not product-runtime defects. Committed-source verification
follows separately. This is evidence at a measured local traffic intensity,
not a DDoS immunity or Internet capacity guarantee, and not a 72-hour result.
