# Wire contracts and pending scope

The full [service-extension target](../../../docs/development/service-extensions-plan.md)
remains required. The wire leaf implements data representations and fixed-header
framing, not the complete MQTT codecs/client. MQTT 5.0/3.1.1 body/property rules,
topic/filter validation, QoS/session/auth/alias/expiry/subscriptions, reconnect,
persistence and transports remain pending. There is no reference-peer or final
performance/production acceptance yet.

| Contract | Verification |
| --- | --- |
| Variable integer | Canonical boundary fixtures, every 2-byte prefix for both versions, exhaustive 0..65535 round trips, nonminimal compatibility, fourth continuation error |
| Fixed header | Independent literal valid-first-byte matrix across all 256 bytes/both versions, AUTH compatibility and PUBLISH flag combinations |
| Total packet bounds | All integer-length transitions through the protocol maximum, exact cap and cap-minus-one, malformed header rejection before body buffering |
| Complete frame | Every prefix of 0/1/2/127/128/16383/16384-byte opaque bodies, borrowed views, coalesced suffix and 3.1.1 nonminimal header byte accounting |
| UTF-8 strings | Every Unicode scalar generated with an independent encoder; all 65536 two-byte patterns; controls/noncharacters, NUL, overlong/surrogate/out-of-range/truncated sequences and error offsets |
| Field representations | Maximum 65535-byte string and every input prefix; arbitrary binary, empty fields and coalesced suffix; borrowed pointers and retained BOM |
| Atomicity/API | Capacity below required size, protocol overflow, unknown config/flags, wrapped/null spans, input/config/output/size aliases and unchanged outputs |
| Adversarial | 20,000 deterministic arbitrary frames, header/frame status agreement, bounded borrowed spans and publication invariants |
| Allocation/fault | 10,000 warm primitive cycles preserve allocation counters/ambient error; successful operation under one-byte quota; diagnostic OOM leaves output unchanged and no live budget bytes |
| Distribution | Modular/single tests, real offline example, independent package consumer and dedicated performance/size profiles |

Reads publish only DONE; MORE/ERROR leave all output values unchanged. A frame's
opaque body may still be an invalid MQTT packet. Read views require the caller's
original bytes to remain alive/unchanged. No partial prefix is retained, no body
buffer is allocated, and no business callback or network action occurs here.

Success allocation counters do not claim zero CPU/scan/copy costs. The README
lists actual header re-parsing, UTF-8 decoder calls, span/alias checks and small
header/field copies. Workload smoke does not prove performance parity, queueing
latency, TLS/QoS/durability equivalence or production stability. Current platform
evidence is in the [progress ledger](../../../docs/development/service-extensions-progress.md).
