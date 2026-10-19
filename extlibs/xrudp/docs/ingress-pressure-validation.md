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

## Committed-source result

Test revision `82e39f64` was rebuilt and passed all eight Windows/Linux,
IPv4/IPv6, Select/IOCP/Epoll processes; the four Linux processes enabled
ASan/UBSan/leak checks. The eight-second cases offered 388,352 actual datagrams
in total, approximately 6,000/s per process. All 64 authenticated stream
instances advanced in every 500-ms sample, with minimum/maximum final progress
ratio at least 0.9581. The maximum heartbeat gap was 21.776 ms. The largest
observed private-memory growth was 15,998,976 bytes including sanitizer
overhead; endpoint reservations stayed constant. All native processes joined.

Counters additionally recorded 129,190 known-CID wrong-source drops and 512
bounded RETRY replies. CPU values remain aggregate two-Worker fixture
observations; they do not compare production server performance between OSes.

`out/xrudp-ingress/verify_acceptance.py` rechecked production sources against
runtime `3d55ec41`, committed fixture/driver input, SDK core and object hashes,
executables and all per-case transcripts. The aggregate is
`out/xrudp-ingress/acceptance.json`; 153 actual source/metadata files are frozen
in `frozen-82e39f64-ingress-source.tar.gz`, SHA256
`9980efad652e76027141207abac404a84191c7265bfe9929a68a3b64ac55fc7c`.
Logs are `dev/xrudp-ingress-{windows,linux}-fixed.log`. No product runtime,
dependency lock, installed application or production service changed.
