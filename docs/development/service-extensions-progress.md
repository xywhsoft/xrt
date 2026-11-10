# Service extensions progress

Target and rules: [development contract](service-extensions-plan.md).

## Current state

- All four final products are in development, not production accepted.
- P0 source/worktree isolation and execution contract are established.
- xredis binary commands, bounded RESP2/3 parsing, owned frames, one ordinary
  TCP/TLS async/sync request connection, bounded hostname/address dialing and
  explicit HELLO/AUTH/name/database setup are locally verified, including Redis
  8.0.5 interoperability. Initial client-certificate signing/authentication is
  locally verified. Atomic transactions, WATCH scopes and fixed/leased/automatic
  pools with bounded repair are locally verified. Identity-bound ticket
  management, remaining dedicated modes, HA/tracking and final performance
  acceptance remain pending.
- xcron expression/calendar, explicit zone/TZif occurrences, scheduling kernel,
  executor adapter and bounded live service are locally verified. The live owner
  now uses the public reusable precise event waiter. Named-zone database/reload,
  durable recovery, nominal DST compensation, bounded recurrence CPU and final
  native performance/long-run acceptance remain pending.
- P1 codec/expression/calendar foundations are locally verified. P2 now has a
  measured-workload TCP request/deadline/cancel/close slice and bounded pool
  repair; remaining modes and pinned overhead/performance acceptance are pending.
- The xrpc prototype exposed a JSON lexical-scan gap. The shared parser now has
  a locally verified raw Scan API. RPC envelope decoding/encoding, synchronous
  request dispatch, immutable method indexing, exact ID comparison keys and bounded
  pending-call admission/removal, immutable owned responses and complete-document
  response correlation, native call/Future lifetime and the first ordinary
  HTTP/TCP call client are locally verified. Bounded asynchronous server execution,
  ordinary/notification/batch HTTP client/server traffic, retained-result quotas,
  asynchronous DNS/prepared-address dialing and an explicit TLS client with
  verified server identity are also locally verified. The client has bounded concurrent submission,
  worker ownership and API-entry
  deadlines. TLS server/mutual/application authentication, richer HTTP
  metadata/recovery/pooling, other transports, bidirectional calls and deferred
  business handlers remain pending.
- xmqtt wire framing, variable integers, UTF-8 strings and binary fields are
  locally verified. All standard MQTT 5.0 property sections are also locally
  verified, along with all MQTT 5/3.1.1 packet body codecs and shared topic/filter
  grammar and negotiated connection constraints. The developing TCP
  client supports CONNECT/CONNACK, keep-alive, QoS-0/1/2 send/receive,
  bounded identifiers/credit, retained ACK/message ownership, diagnostic and
  application graceful DISCONNECT, subscriptions, send/receive aliases, enhanced
  authentication and physical retirement. Hostname/prepared-address Dial is
  locally verified, along with verified TLS and TLS1.3 credential handoff.
  In-memory Session/expiry and explicit numeric TCP recovery are locally verified.
  TLS/Dial/Auth recovery composition, automatic reconnect, durable storage,
  WebSocket and full performance/production qualification remain pending.
- Default development is directly in D:/GIT/xrt on master; preserve concurrent work.

## Baseline checks

Source: `1d4109aff9d52d0452742669e81caa8bef626922`.

| Check | Result |
| --- | --- |
| `python tools/check_release_maturity.py` | Existing failure: missing `net_file_future` single-test closure coverage |
| `python tools/generate_features.py --check` | Passed |
| Core single headers | Initially stale; synchronized from tracked inputs in `32abef18` |
| `python tools/generate_example_index.py --check` | Existing layout mismatch: the committed `examples/hash/crc32.c` is not a `main.c` entry; the primary checkout already has an unrelated staged rename, which this branch preserves |
| `tools.test_scope.ScopeTest.test_extension_integration_states_match_current_contract` | Existing mismatch: xhttp has no xlang integration entry, while the test expects deferred; both the manifest value and the test blob are unchanged from the initial revision |

## Feature ledger

| Product | Feature | State | Evidence |
| --- | --- | --- | --- |
| common | development/acceptance contract and isolated branch | complete | this document, development contract and baseline checks |
| common | independent API export inventory | complete | `36ea6bc8`, API/reference tooling tests |
| common | types-only public API header inventory | complete | shared public typedef/enum headers are included without an export function; comment-only exports/selectors stay excluded; 17 focused tooling tests pass |
| common | actual product-root example selection | locally verified | xredis/xcron/xrpc collect their leaf examples; root commands actually compile/run them on Windows and Ubuntu/WSL; build/API/workload tooling 67 tests pass |
| common | explicit workload fixture identity | locally verified | reproduced unchanged benchmark fingerprint after a peer-header mutation; optional inputs now pins exact fixture bytes, rejects missing/outside/directory/duplicate inputs and is emitted in reports; xredis runtime fixtures registered, 69 tooling tests pass |
| common | TLS server selector error isolation | locally verified | `3bfe34a0`; stale errors survive successful selection, new checked callback errors reject; Windows modular/single and WSL ASan/UBSan single, 64 OOM prefixes |
| common | TLS 1.2/1.3 CertificateRequest without local identity | locally verified | exact empty-certificate response, transcript/EMS/Finished, fragmentation, malformed/order/duplicate requests, backpressure/OOM; 17 Core tests per platform and 14 OpenSSL cases per platform; signing remains pending |
| common | raw JSON scan needed by xrpc | locally verified | exact borrowed scalar/key/container spans, lexical numbers, 10,000 zero-heap scans, limits/Unicode/control/OOM, Windows/WSL modular/single/shared-reader regression and external static/shared consumers; performance comparison remains unaccepted |
| xredis | binary command size/write | complete | Windows modular/single/O2/example/trim/static/DLL; Ubuntu/WSL O2 ASan+UBSan modular/single/static/shared; 10,000 randomized commands, overflow/aliasing, hot-path allocation counters, diagnostic OOM |
| xredis | incremental RESP2/3 event reader | complete | Windows modular/single/O2/example/trim/static/DLL; Ubuntu/WSL ASan+UBSan modular/single/example/trim/static/shared; all canonical split points, event pauses, 10,000 random arrays, 20,000 mutation pairs, limits, reentry, OOM and lifecycle quota |
| xredis | immutable owned response frames | locally verified | `e4bfab34`; all types, ordered metadata/children, every fixture split, 512 levels/1025 nodes, binary copy/growth, peak caps, quota/OOM, atomic sharing and independent packages |
| xredis | one numeric TCP connection, async/sync requests and bounded pipeline | locally verified | IOCP/epoll, split reads, typed outcome/certainty, queue-inclusive deadline, cancellation/tombstones/drain grace, push isolation, bounded close, 384 concurrent calls, 36/71 quota thresholds, worker OOM, reentry, ASan/UBSan/qualified TSan, actual examples/packages and workload smoke |
| xredis | managed HELLO/AUTH/ACL/name/database setup and readiness barrier | locally verified | exact scripts, explicit RESP3/AUTO/RESP2 modes, error redaction, queue-inclusive deadlines/cancel, OOM/close/reentry, Windows/WSL modular/single/sanitizers and real Redis 8.0.5 |
| xredis | fixed-arrival runtime workload and process counters | locally verified | absolute schedules, all-offer ledger, separate success/all-outcome tails, bounded observer slots, stalled/held-peer controls, allocation mode, Windows/WSL modular/single/sanitizers and Redis 8.0.5 diagnostics |
| xredis | initial TLS client certificate authentication | locally verified | explicit V1 factories, retained identity before DNS, real TLS 1.2/1.3 peers; identity-bound tickets remain pending |
| xredis | staged atomic MULTI/queue/EXEC | locally verified | exclusive FIFO, queue rejection/DISCARD, typed uncertainty/cancel/OOM, native static/shared consumers; final performance acceptance remains pending |
| xredis | WATCH, fixed/leased/automatic pools and repair | locally verified | dedicated P2 entries below record physical retirement, nested scopes, quotas/races/OOM, Windows/WSL tests and Redis 8.0.5 interop; performance remains unaccepted |
| xredis | remaining dedicated modes, HA, tracking | pending | no claim |
| xrpc | bounded JSON-RPC 2.0 envelope decode | locally verified | Windows modular/single/example/trim/static/DLL; Ubuntu/WSL ASan+UBSan modular/direct-single, example/trim/static/shared; mixed batch/role/lossless-ID/duplicate/error/limits/OOM and deterministic property/mutation tests |
| xrpc | bounded atomic JSON-RPC 2.0 envelope encode | locally verified | Windows modular/single/examples/trim/static/DLL; Ubuntu/WSL ASan+UBSan modular/direct-single, examples/trim/static/shared; 10,000 independent wire properties, 500 complete-document resource oracles, 10,000 mutated raw inputs, zero-heap/OOM/aliases/atomicity |
| xrpc | bounded synchronous request dispatch | locally verified | Windows/WSL modular/single/sanitizers/examples/trim/static/shared; full-validation-before-callback, notification suppression, exact IDs, standard errors, stack output, reentry/sticky failures, aggregate caps, 10000 mixed-batch properties, zero-heap ordinary calls and diagnostic OOM |
| xrpc | immutable semantic method index | locally verified | all Unicode scalars, explicit fingerprint collision, copied names, 4096 unsorted registrations, bounded lookup/constructor, local-abort integration, quota/OOM and retained concurrent readers; dedicated P3 entry below records package/performance evidence |
| xrpc | exact identifier comparison keys | locally verified | exact decimal/signed-zero/huge-exponent semantics, all Unicode scalars, independent rational and arbitrary-precision exponent oracles, bounded atomic Size/Write, standalone consumers and shared-string regression |
| xrpc | bounded serialized pending calls | locally verified | exact ID admission/removal, token ABA protection, deadline heap, shutdown drain, 100000-operation linear model, 50000 private invariants/exhaustion per platform, no steady table allocation and OOM/alias/publication coverage |
| xrpc | immutable responses and complete-document correlation | locally verified | full validation and ID preflight before any removal, one owned copy/allocation, rebased lossless views, retained concurrent readers, batch matched/repeated/unknown outcomes, OOM/alias/capacity/lifetime coverage and independent packages |
| xrpc | native typed call/Future lifetime | locally verified | independent result/context lifetime, delivery knowledge, cooperative cancellation/deadlines, nonblocking captured-cancel retirement, completion reentry, 4096 OOM prefixes, checked reference overflow and shuffled-batch Future integration |
| xrpc | first ordinary HTTP/TCP call runtime | locally verified | real persistent IOCP/epoll connection, bounded concurrent submission, copied input, fresh exact IDs, HTTP framing, deadlines/cancel retirement, quotas/OOM/reentry and qualified TSan; detailed P3 entry below |
| xrpc | remaining client/server and transports | pending | notifications/batches over network, DNS/TLS, pooling/pipelining, WebSocket/stdio, server/executors and negotiated extensions remain |
| xmqtt | bounded wire framing and string/binary primitives | locally verified | Windows O2 modular/single and Ubuntu/WSL O2 ASan+UBSan modular/direct-single; all Unicode scalars, exhaustive two-byte UTF-8/VBI prefixes, full flag matrix, fragmentation/coalescing, limits/atomicity/zero-heap/OOM; real examples/trim/static/shared consumers |
| xmqtt | bounded MQTT 5.0 property-section codec | locally verified | all 27 ids / 7 types, scope/direction/repetition/range/auth/UTF-8 rules, typed error classes independent of diagnostic OOM; Windows/WSL modular/single/sanitizer, examples/trim/static/shared; 20,000 mutations, prefix/limit/atomicity/zero-heap tests |
| xmqtt | shared packet descriptors/grammar and complete MQTT 5.0 packet codec | locally verified | all 15 types, 25 literal canonical fixtures, 4608 reason/sender cases, 256 CONNECT flags, 512 subscription options, packet boundary/borrowed-view checks, 30,000 mutations, limits/atomicity/zero-heap/OOM; Windows/WSL modular/single/sanitizer/examples/trim/static/shared |
| xmqtt | complete MQTT 3.1.1 packet codec | locally verified | all 14 types, 22 literal fixtures at 4 header widths, 256 CONNECT flags and subscription bytes, 65,536 CONNACK flag/code combinations, >2 MiB Errata 01 range, 30,000 mutations, limits/borrows/atomicity/zero-heap/OOM; Windows/WSL modular/single/sanitizer/examples/trim/static/shared and symbol-isolation size gate |
| xmqtt | negotiated property rules, client, sessions, persistence, transports | pending | no claim |
| xcron | bounded immutable expression compiler | complete | Windows modular/single/O2/example/trim/static/DLL; Ubuntu/WSL ASan+UBSan modular/direct single, example/trim/static/shared; 111,630 range/step combinations, 10,000 generated expressions, 20,000 raw inputs, alias/offset/atomicity/allocation/OOM tests |
| xcron | bounded Gregorian UTC/civil matching and earliest search | locally verified | full 400-year/146097-day predicate oracle, 1500 brute search oracles, full UTC int64 and civil int64 years, whole-second/bounds/BCE/century rules, 4801-month complete-cycle proof, aliases/atomicity/zero-heap/OOM; Windows/WSL modular/single/sanitizer/examples/trim/static/shared and isolated-size gate |
| xcron | named time zones, scheduler, recovery | pending | no claim |

Do not replace pending rows with success based only on metadata or compilation.

## Shared public type inventory

The RPC encoder introduced a public type header shared by separate encode and
decode leaves. This exposed an API-audit filter that required an export macro
and silently omitted typedef/enum-only headers. API generation and coverage now
share one filter accepting actual typedefs as well as export declarations.
Comment-only exports/typedefs and feature-selector-only headers stay excluded.
`python -m unittest tools.test_api_docs tools.test_api_reference
tools.test_extension_layout` passes all 17 tests, including two dedicated
manifest/header fixtures. This is tooling coverage, not encoder completion.

## Command encoder validation

- `python -m unittest tools.test_api_docs tools.test_api_reference tools.test_extension_layout`: 15 passed.
- `python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_tests`: Windows modular and single tests passed.
- `python tools/build.py --compiler gcc --arch x64 --manifest extlibs/xredis/config/modules.json --suite xredis_command --no-single --cflag=-O2`: Windows test and offline example passed.
- `python tools/build.py --compiler gcc --arch x64 --manifest extlibs/xredis/config/modules.json --suite xredis --trim-only`: Windows and Ubuntu/WSL product/leaf probes passed, including required-dependency rejection.
- Ubuntu/WSL `xredis_tests` with `-O2 -fsanitize=address,undefined -fno-omit-frame-pointer`: modular and single passed.
- `python tools/package.py --manifest extlibs/xredis/config/modules.json --suite xredis --kind static/shared --verify`: independent consumers passed on Windows and Ubuntu/WSL.
- Asset maturity, generated API coverage and reference consistency passed; strict release correctly rejects the developing product root.
- Performance workload smoke passed separately on Windows and Ubuntu/WSL, one sample each. This is not a pinned baseline or external-client comparison.

Raw ignored logs/reports: `out/service-extensions/evidence/`. See
[the feature matrix](../../extlibs/xredis/docs/coverage.md). The dedicated
workflow checks the current real module; actual GitHub Actions execution is
not claimed by local YAML validation.

## Incremental reader validation

- Independent RESP2/3 grammar and explicit depth stack: all scalar/aggregate
  types, binary strings/errors/verbatim, attributes, top-level pushes, streamed
  bulk/array/map/set, checked signed integers and locale-independent numeric text.
- Canonical expected event trees pass every two-part split and bytewise feeds;
  one/two/three-byte budgets, pause after every event and empty resumes preserve
  exact event delivery and pipeline boundaries.
- 10,000 independently encoded binary-array properties and 20,000 deterministic
  raw/mutated inputs pass; whole and bytewise/paused mutation results agree on
  terminal error, detection offset, consumed stream bytes and frame count.
- All configured byte/depth/item/value limits, malformed wire, every truncated
  prefix, orphan attributes, callback failure/reentry, invalid spans/aliasing,
  creation/diagnostic OOM and repeated create/destroy ownership quotas pass.
- 10,000 warm Feed/Finish/Info/Reset operations leave allocation counters and the
  ambient error unchanged. Create performs one bounded XRT allocation; callbacks
  and any later owned reply builder have their own costs.
- Windows GCC 16.2 O2 modular/single tests and both offline examples passed;
  product/leaf trim probes and independent static/DLL consumers passed.
- Ubuntu/WSL GCC O2 ASan+UBSan modular and reader single tests passed. O2 examples,
  trim probes and independent static/shared consumers passed. The single test
  also passed a direct compile of the already-current generated headers while
  the normal generator's cross-filesystem scan was slow; the normal build then
  completed successfully as well.
- API/docs/asset maturity and generated-input consistency pass. The 15 related
  Python tooling tests pass. CI now runs all implemented xredis examples.
- Reader workload smoke passed on both systems with a data-dependent checksum,
  one sample each. This is not a pinned performance baseline, an owned-tree
  benchmark, native Linux acceptance or external-client comparison.

Reproducible commands use `xredis_tests --cflag=-O2`, reader single selector
`--single-test test_single_xredis_reader`, Linux sanitizer flags
`--cflag=-fsanitize=address,undefined --cflag=-fno-omit-frame-pointer
--ldflag=-fsanitize=address,undefined`, product `--suite xredis` example/trim/
package consumers, and performance `--profiles xredis_reader --smoke --repeats 1`.
Raw `reader-*` logs and the unpinned smoke JSON reports remain under the ignored
`out/service-extensions/` evidence paths. Reader contracts and the full matrix
are in [reader.md](../../extlibs/xredis/docs/reader.md).

## Cron expression validation

- Explicit Unix five-field and seconds six-field dialects compile into a
  caller-owned immutable value. No input is retained. Natural field bitmaps,
  Sunday 0/7 normalization, names, range/star steps, lists and calendar aliases
  are covered. Day-star flags preserve syntax needed for the later calendar
  day rule; equivalent numeric full ranges do not erase that distinction.
- All 111,630 ascending minute range/step combinations pass an independent
  membership oracle. Another 10,000 generated five/six-field expressions cover
  unions, steps, stars and Sunday normalization across every field.
- Handwritten expected masks, all names/aliases, input lifetime, exact issue
  offsets, malformed/unsupported syntax, byte limit, numeric overflow, invalid
  spans/aliases/dialects, 20,000 raw byte inputs and atomic failure outputs pass.
- 10,000 successful compiles preserve allocation counters and ambient error;
  aliases succeed under a one-byte quota, and diagnostic OOM preserves both the
  caller's expression and independent local issue.
- Windows GCC 16.2 O2 modular/single, offline example, product/leaf trim and
  independent static/DLL consumers pass. Ubuntu/WSL GCC O2 ASan+UBSan modular
  and direct single-header compile/run pass, as do the O2 example, trimming and
  independent static/shared consumers. Direct single uses the registered single
  test source, current generated headers, strict C11 warnings, pthread, O2 and
  the same sanitizer flags as the modular run.
- Generated features/single/API consistency, API documentation and normal asset
  maturity pass; strict release correctly rejects the developing product root.
  Related 15 Python tooling tests pass. CI covers the two actual products on
  both systems; YAML and pinned action references are validated locally only.
- Windows and Ubuntu/WSL expression workload smoke pass, one sample each. This
  is compilation-only, unpinned evidence; no scheduler, timezone, latency or
  production-performance acceptance follows from it.

Commands mirror the preceding reader validation with manifest
`extlibs/xcron/config/modules.json`, suite `xcron_tests` or product `xcron`,
performance config `extlibs/xcron/config/performance_profiles.json`, and direct
single source `extlibs/xcron/tests/single/test_single_xcron_expression.c`.
Ignored `cron-*` logs/reports are under `out/service-extensions/`;
[the expression matrix](../../extlibs/xcron/docs/coverage.md) lists the contracts.

## Raw JSON scan prototype and validation

The xrpc prototype used existing public `xrtJsonVisit` with BigInteger=FLOAT.
A normal request and a 40-digit integer visited successfully without allocation;
an escaped parameter string needed one temporary heap block, and `1e9999`
failed native numeric conversion. These are existing Visit contracts, not new
bugs in its promised native-value behavior. xrpc needs bounded lexical number
preservation and opaque parameter/result spans before protocol classification.

The additive `xrtJsonScan` surface reuses the same strict parser with no payload
materialization. It returns borrowed raw scalar/key spans and complete container
spans on END, validates Unicode/escapes and all existing resource budgets, and
does not narrow numbers to native types. Parent names/indexes survive nested
containers. Existing public layouts and Visit/Read behavior are preserved;
the separate `xjsonscantype` enum adds no cases to existing decoded visitors. No protocol-specific
shared runtime or second JSON parser is introduced.

- `tests/json/test_json_scan.c` covers exact spans, escaped names/NUL, nested
  empty/full containers, parent identity, numeric overflow lexemes, Unicode,
  limits, callback stop/failure, malformed input, 10,000 warm scans with stable
  allocation counters/error identity and successful parsing under one-byte quota.
- Windows GCC 16.2 O2 and Ubuntu/WSL GCC O2 ASan+UBSan pass the shared
  `json_read,xlon_read,memory_stats` regression. Existing native numeric policies,
  DOM/OOM cases and XLON parsing are exercised. New Scan single tests pass on
  both systems; Windows also runs all selected existing single drivers.
- The registered public consumer is separately compiled against actual JSON-read
  static and shared packages on Windows and Ubuntu/WSL. It invokes Scan on
  `1e9999` and observes the same library's protocol diagnostic; all four consumers
  pass. The consumer's first malformed fixture invoked its intentionally strict
  numeric callback before reaching the bad terminator; a boolean fixture fixes
  that test setup and the complete consumer gates pass.
- JSON API inventory reports 31 functions / 56 constants / 20 types, missing=0.
  Generated core/extension headers and feature selection are current. The JSON
  leaf trim probes pass. Twenty focused API/reference/performance-tool tests
  pass. The broader 29-test run has only the unchanged xhttp integration-state
  baseline failure noted above. Core maturity still has only the previously
  recorded `net_file_future` single-closure failure.
- Service CI checks this shared prerequisite on both systems, including Linux
  sanitizers, after checking generated assets. Local YAML validation does not
  establish that remote Actions have run.
- The registered data workload smoke passes with separate raw Scan and decoded
  Visit metrics. These have different output semantics and are not an equivalent
  output comparison or RPC production acceptance.

Existing-Visit A/B probes against `2d72cf29:single/xrt.h` used identical inputs,
GCC O2, interleaved programs, warmups and a pinned logical CPU. Repeated samples
on the shared machine exceeded the planned 3% noise threshold, including the
longer-window and metadata-copy-elision prototypes. Logs retain these
inconclusive runs. **Neither throughput improvement nor a passed regression gate
is claimed.** A quiet-environment comparison remains required before acceptance.

Reproduction uses `tools/build.py --suite json_read,xlon_read,memory_stats
--cflag=-O2 --no-examples`, the preceding sanitizer flags, registered single source
`tests/single/test_single_json_scan.c`, `tools/package.py --suite json_read
--kind static/shared --verify`, and separately links
`tests/package/test_json_scan_consumer.c` against those artifacts. Smoke uses
`tools/measure_performance.py --profiles data --smoke --repeats 1`.
Ignored prototype, A/B, package, sanitizer and smoke evidence is under
`out/service-extensions/` with `json-scan-*` names. The API contract is documented
in [json.md](../api/json.md).

## RPC envelope decode validation

- Request, response and duplex modes classify calls, notifications, result/error
  responses and invalid envelopes. Complete-document validation precedes Count
  publication. Earlier scratch descriptors cannot be executed after a malformed
  tail. Empty batches produce one invalid descriptor with a single-object reply
  shape; invalid batch items do not erase valid siblings.
- All identifiers remain exact raw JSON: string/null/number, huge exponents and
  fractions included. Explicit null is a call id, not a notification. Escaped
  protocol names/version/reserved prefix are interpreted without allocation;
  semantic duplicate aliases are rejected in relevant envelope/error fields.
- Error-code integrality is checked as exact decimal arithmetic, without native
  float conversion. Params/result/error-data container spans remain borrowed.
  Opposite-role extras are ignored in one-way modes; duplex ambiguity is explicit.
- The deterministic suites pass 10,000 generated mixed batches, 10,000 independent
  rational integer-code oracles, and 20,000 raw/mutated documents, including bounded
  views and output publication invariants. Handwritten fixtures cover protocol
  failures, every configured resource class, aliases and local capacity failures.
- Ten thousand warm decodes preserve allocation counters and ambient error. A
  valid escaped-parameter batch succeeds under a one-byte quota; parse-diagnostic
  OOM becomes a local failure and preserves Document rather than publishing a
  spurious protocol error.
- Windows GCC 16.2 O2 modular/single tests, offline example, root/leaf trim and
  independent static/DLL consumers pass. Ubuntu/WSL O2 ASan+UBSan modular and
  direct single tests pass, as do its example, trim and static/shared consumers.
- Independent API inventory reports 2 functions / 41 constants / 10 types,
  missing=0. Generated/API/asset checks and 15 related tooling tests pass. Strict
  release correctly rejects the still-developing root. CI now includes all three
  actual products on both systems; remote Actions execution is not claimed.
- Windows codec workload smoke passes with an observed descriptor checksum. A
  single unpinned sample is not throughput/tail-latency or production acceptance.

The direct-header build also reproduced an existing missing xerror include in
the public JSON header. `12c32eea` fixes that dependency and registers a standalone
leaf-header consumer, verified with strict C11 compilation/link/run. A separate
module-selector trim exposed the need to process core features after extension
module selection; the xrpc header now does so explicitly. Neither change adds a
runtime layer.

Reproduce using `extlibs/xrpc/config/modules.json`, suite `xrpc_tests --cflag=-O2`,
product `xrpc` example/trim/static/shared package consumers, the sanitizer flags
above, and `extlibs/xrpc/tests/single/test_single_xrpc_decode.c`. Ignored
`rpc-decode-*` logs/reports are under `out/service-extensions/`.
[Decoder contracts](../../extlibs/xrpc/docs/coverage.md) describe scratch lifetime,
parse/local-error separation and deliberately pending runtime behavior.

## RPC envelope encode validation

- Separate `xrpc_encode` leaf provides Size/Write for calls, notifications,
  results, errors and explicit nonempty batches. Required/unused fields and
  batch direction are checked. Null is explicit; absent raw values use Size=0.
  Methods/error messages are decoded UTF-8 and are quoted with public XRT APIs.
  Raw identifiers, payloads, integer codes and their supplied whitespace are
  validated and retained without numeric narrowing. Standard error codes do not
  limit application codes to a native integer range.
- Config limits cover the complete encoded document: envelope keys/values,
  batch arrays, embedded depth, direct items, aggregate values, bytes, messages,
  id size and quoted method size. Exact-boundary fixtures and 500 randomized
  comparisons against a complete-output JSON scan verify the accounting.
- Both Size and Write preflight independently. Inputs stay borrowed/unchanged
  during the call. Output sizes and wire bytes are unchanged on every failure;
  vector/config/live-view/output aliases, malformed/trailing JSON, UTF-8,
  invalid field types, nonintegral codes and late batch failure are covered.
  Write emits no NUL. Zero-response batches are the runtime's responsibility:
  it must send nothing, and valid notifications must never receive replies.
- Ten thousand deterministic generated batches match an independent byte quote
  and wire oracle; modular tests also decode and classify the resulting messages.
  Ten thousand raw/mutated local values preserve output atomicity on rejection.
  Ten thousand Size+Write repetitions preserve allocation counters and ambient
  error identity. Valid output succeeds under a one-byte quota; diagnostic OOM
  leaves outputs unchanged and budget live bytes return to zero.
- Raw fields are grammar-scanned once per preflight and copied once per Write.
  Public quote currently validates UTF-8 then traverses for escaping, called in
  both preflight and emission: four traversals of each decoded text field per
  Write. Calling Size first adds its own preflight. Fixed syntax has small segment
  copies; public JSON calls still save/restore diagnostic references. No DOM,
  owned payload, heap scratch or new shared runtime is introduced. This is an
  explicit cost description, not a claim of zero bridge/transport overhead.
- Windows GCC O2 modular/single tests and Ubuntu/WSL O2 ASan+UBSan modular and
  direct encode single tests pass. Both systems pass root/leaf trim, actual
  examples and independent static/shared consumers that round-trip a huge id.
  The shared exact-decimal helper's decoder tests also pass after extraction.
- Generated files/API/maturity checks pass; inventory is 5 functions / 43
  constants / 12 types, missing=0. Strict release correctly rejects the developing
  product root. Windows and Ubuntu/WSL decode/encode workload smoke passes
  (one unpinned sample each); no baseline, tail latency or production parity is
  inferred. Build/API/workload tooling tests pass 67 cases.

The root example command originally selected an empty examples list and could
report pass without compiling/running examples. Roots now explicitly collect
same-product dependency examples, and a regression checks that the documented
commands select every current example on both systems. All xredis, xcron and
xrpc examples have now actually run on Windows and Ubuntu/WSL. CI runs that
selection/API regression on each OS; remote Actions execution is not claimed.

Reproduce with suite `xrpc_tests --cflag=-O2`; Ubuntu/WSL modular tests add
`--no-single --cflag=-fsanitize=address,undefined
--cflag=-fno-omit-frame-pointer --ldflag=-fsanitize=address,undefined`.
Direct single uses `extlibs/xrpc/tests/single/test_single_xrpc_encode.c`, strict
C11 warnings, the same sanitizer flags, `-pthread -lm`, and the current generated
headers. Product-root `xrpc --no-single` now compiles/runs both real examples.
Packaging uses `xrpc --kind static/shared --verify`; workload uses the extension
performance config with `--smoke --repeats 1`. Ignored `rpc-encode-*` evidence,
including the three products' actual example runs, is under
`out/service-extensions/`. See [codec contracts](../../extlibs/xrpc/docs/coverage.md).

## MQTT wire validation

- The independent `xmqtt_wire` leaf uses the OASIS MQTT 5.0/3.1.1 specifications
  for fixed headers, variable integers, uint16-length strings and binary data.
  No reference-client sources are embedded. It uses the public XRT Unicode,
  memory-range and diagnostic APIs; no shared runtime addition was needed.
- Header reading inspects at most five bytes, checking packet type/version,
  fixed flags, PUBLISH QoS/duplicate combinations, integer form and total packet
  cap before receiving a body. Frame reading borrows one complete Wire/Body span
  and leaves coalesced suffixes untouched. Default cap is 1 MiB, including actual
  header bytes. The protocol maximum and every integer-size transition are tested.
- MQTT 5.0 rejects nonminimal variable integers. The explicit 3.1.1 compatibility
  policy accepts 1..4-byte nonminimal forms; all writers are minimal. A fourth
  continuation bit fails immediately. Tests cover all 65,536 two-byte prefixes
  under both versions and round-trip every value 0..65535, plus upper boundaries.
- MQTT encoded strings reject NUL/invalid scalar UTF-8 and preserve BOM. An
  explicit optional policy rejects controls/noncharacters. All 1,112,064 Unicode
  scalars are generated by an independent byte encoder and checked under both
  policies; all 65,536 two-byte patterns also pass a separate grammar oracle.
  Overlong, surrogate, out-of-range and truncated sequences fail at the recorded
  byte offset. A scalar cannot cross a length-prefixed field into coalesced data.
- Binary fields retain arbitrary bytes. Read fields borrow the source; writes
  copy once after complete preflight. Maximum-length 65,535-byte strings pass
  every input prefix. Empty fields, arbitrary binary, coalescing and BOM round
  trips are verified. All 256 first-byte combinations are checked against a
  literal allowed-header table under both protocol versions.
- Every prefix of 0/1/2/127/128/16383/16384-byte opaque frame bodies passes its
  expected MORE/DONE boundary. Header/field outputs remain unchanged on failures
  or MORE. Twenty thousand deterministic arbitrary frames preserve header/frame
  result agreement, byte bounds and publication invariants. API misuse, aliases,
  every short writer capacity, protocol overflow and configured limits are tested.
- Ten thousand warm primitive cycles preserve allocation counters and ambient
  diagnostic identity. Valid UTF-8/framing operations succeed under a one-byte
  quota. Diagnostic OOM preserves write outputs; no budget live bytes remain.
  There is no successful heap allocation, retained prefix/input or callback.
- Windows GCC 16.2 O2 modular/single tests and Ubuntu/WSL GCC 15.2 O2 ASan+UBSan modular
  and direct single tests pass. Both systems actually run the offline example,
  root/leaf trim and separate static/shared package consumers. The consumer also
  observes the extension's error through the host XRT instance.
- API inventory is 11 functions / 35 constants / 7 types, missing=0. Generated
  files/API/maturity checks and 67 build/API/workload tooling tests pass. Strict
  release correctly rejects the developing product root. CI now includes all
  four real products on Windows/Linux; example selection regression includes
  xmqtt. Local YAML checks pass; remote Actions execution is not claimed.
- One-sample unpinned wire workload smoke passes on Windows and Ubuntu/WSL.
  It measures framing, one string field and fixed-header writing, **not** full
  packet/property validation, queue-inclusive latency, QoS or durability. It is
  neither a pinned regression baseline nor MQTT production acceptance.

Framing DONE deliberately says nothing about body-length rules, packet identifiers,
properties, topic/filter validity, directions or session ordering. The complete
packet/client scope is still pending. Reads are stateless: callers re-supply the
full prefix on MORE, and own bounded receive storage. This repeats up to five
header-byte checks per call, copies/validates no body, and allocates no buffer.
Complete string primitives walk their bytes once, calling the public XRT decoder
for non-ASCII scalars. Field writes copy payload once; integer/header writes
encode 4/5 stack bytes and copy that small header once. Config/descriptor copies,
span/alias checks and diagnostic calls remain explicit costs. No end-to-end
zero-copy/zero-overhead claim is made.

Reproduce with `extlibs/xmqtt/config/modules.json`, suite `xmqtt_tests
--cflag=-O2`; Ubuntu/WSL modular tests add `--no-single` and the same sanitizer
flags documented above. Direct single uses
`extlibs/xmqtt/tests/single/test_single_xmqtt_wire.c` with strict C11 warnings,
O2, ASan+UBSan, `-pthread -lm`, and current generated headers. Product `xmqtt
--no-single` runs the real example; root `xmqtt --trim-only` tests both selector
closure and missing dependencies. Package product `xmqtt --kind static/shared
--verify`; smoke uses its registered performance config. Ignored `mqtt-wire-*`
logs/reports are under `out/service-extensions/`.
[Wire contracts](../../extlibs/xmqtt/docs/coverage.md) preserve the exact scope
and remaining MQTT requirements.

## MQTT 5.0 property-section validation

- The separate `xmqtt_properties` leaf implements all 27 standard ids and seven
  representations from OASIS MQTT 5.0, including explicit packet/Will scopes and
  sender direction. Constant indexed lookup and a 64-bit seen set validate
  allowed ids/scopes, singleton/repetition rules and numeric domains. User
  Properties preserve order/repeated names and values; server PUBLISH preserves
  repeated equal subscription identifiers. No value defaults are inserted.
- Response topics require nonempty wildcard-free MQTT strings. CONNECT auth
  data requires a method independent of property order. Explicit AUTH sections
  require a method. Client PUBLISH subscription ids and server DISCONNECT session
  expiry are rejected. Negotiated connection state, omitted packet forms and
  packet-wide rules remain required at the packet/runtime layers.
- Decoder statuses distinguish malformed wire, protocol errors and local API/
  capacity/resource failures, even when cold diagnostics themselves hit OOM.
  The caller scratch array is provisional; Count/Consumed/Raw publish only on
  complete DONE. MORE changes no scratch/document and retains no prefix. Every
  field is restricted to the declared section, preventing coalesced payload
  bytes from completing a truncated string/scalar/property identifier.
- Size/Write independently preflight all input descriptors, active union views,
  semantics, aliases, bounds and output capacity. All failures preserve wire and
  size outputs. Writers preserve property order and data bytes, use shortest
  variable integers, and do not rescan UTF-8 during emission. Known byte-budget
  failures are rejected before UTF-8/response-topic scans; the complete section
  cap includes its length integer. Zero count limits admit empty sections only.
- Literal fixtures cover all seven wire representations and all 27 identifiers.
  The 756 scope/direction combinations verify accepted/rejected cases; singleton
  and repeat rules are checked in both producer preflight and raw decoding.
  Exhaustive byte values cover all eight boolean/maximum-QoS fields. Positive
  uint16/variable/uint32 domains, uint32 maxima, zero alias maximum, unknown ids,
  nonminimal/truncated integers, duplicates and UTF-8 failures are tested.
- Binary payload lengths exercise section bodies 127/128/16383/16384/65538,
  including every input prefix through the maximum 65535-byte binary field,
  exact caps and cap-minus-one before buffering a body. One-byte output handles
  an empty section. All short writer capacities, count/scratch/field limits,
  invalid config/spans/aliases and late failures preserve the stated outputs.
- Twenty thousand raw/mutated sections verify bounds/publication and exact
  accepted decode/encode round trips without order or byte loss. Ten thousand
  warm Read/Size/Write/MORE cycles preserve allocation counters and ambient error
  identity. One-byte allocation quotas allow valid processing; own/nested-wire
  diagnostic OOM retains malformed/protocol/local classification and no live
  budget bytes remain.
- Windows GCC 16.2 O2 modular/single tests and Ubuntu/WSL GCC 15.2 O2 ASan+UBSan
  modular/direct property single tests pass, along with the existing wire suite.
  Both systems run the real wire/property examples, root/leaf trim and independent
  static/shared consumers that exercise both leaves against one host XRT.
- Generated/API/maturity checks and 67 build/API/workload tooling tests pass.
  Inventory is 15 functions / 97 constants / 18 types, missing=0. Strict release
  correctly rejects the developing product root. Existing four-product CI
  automatically includes the new tests/examples/profiles; remote Actions
  execution is not claimed.
- Wire and property Read/Write workload smoke passes on Windows and Ubuntu/WSL,
  one unpinned sample each. This is neither a pinned baseline nor packet/client
  throughput, queue-inclusive tail latency, QoS, durability or production parity.

The codec borrows text/binary/pair data, copying only caller-owned descriptors on
read. Each complete string receives one UTF-8 validation per call, with an
additional wildcard walk for response topics; writers copy data once. Separate
Size+Write calls repeat preflight intentionally. Public wire span/alias checks,
configuration/descriptor copies and small variable-integer writes remain real
costs. No owned-value/DOM layer, new shared runtime or successful heap allocation
is introduced. No zero-overhead/end-to-end zero-copy claim is made.

Reproduce with `extlibs/xmqtt/config/modules.json`, suite `xmqtt_tests
--cflag=-O2`; Ubuntu/WSL modular runs add `--no-single` and the sanitizer flags
documented above. Direct single uses
`extlibs/xmqtt/tests/single/test_single_xmqtt_properties.c` and the same strict
C11/O2/ASan+UBSan/`-pthread -lm` command pattern. Product `xmqtt --no-single`
runs both actual examples; `xmqtt --trim-only` covers both selectors and missing
dependencies; static/shared packages use `xmqtt --kind static/shared --verify`.
Its performance config now has separate wire and property profiles. Ignored
`mqtt-properties-*` logs/reports are under `out/service-extensions/`.
See [property contracts](../../extlibs/xmqtt/docs/properties.md) for the exact
section lifetime, publication, costs and pending connection-level requirements.

## Complete MQTT 5 packet codec validation

`xmqtt_packet` adds feature-independent borrowed packet/config/issue types and
initializers, with private reason and topic/filter grammar helpers. Direction
now resides in the wire header so the later 3.1.1 codec does not depend on MQTT 5
properties. `xmqtt_codec5` implements Read/Size/Write for all fifteen MQTT 5 control
types. Existing wire/property preflight and emission have private reusable helpers
with hidden visibility; no private XRT facility or new shared runtime is needed.

- Twenty-five independent literal fixtures cover every type, all required fixed
  flags, QoS 0/1/2 identifiers, complete CONNECT/Will/username/password/auth fields,
  shared and multi-topic requests, ordered reason lists, alias-only PUBLISH,
  enhanced AUTH syntax and PFI character data containing NUL. Every valid prefix
  preserves Packet/Issue/scratch; coalesced suffixes stay unconsumed. Shorter
  declared packet bodies never use suffix bytes to finish an internal field.
  Every accepted fixture/mutation has bounded borrowed wire/scratch views.
- The 4,608 reason-byte/sender combinations, all 256 CONNECT flag bytes and 512
  subscription option/shared cases match literal OASIS tables/bit rules. Wrong
  sender roles, zero packet ids, invalid CONNACK session flags, topic/filter and
  shared group grammar, No Local on shared filters, unknown CONNECT protocols,
  missing/truncated/forbidden property sections and extra bytes are checked.
  MQTT 5 password without username is explicitly accepted. Encoded-string NUL
  remains malformed; PFI=1 payload NUL is legal, malformed scalar sequences are
  not. PFI error context retains a confirmed identifier and exact payload offset.
- Header packet caps reject before body buffering. Payload, aggregate property
  bytes/counts across CONNECT+Will, topic/reason counts and scratch limits are
  independent and real at zero. Boundary payloads through 65536 bytes exercise
  every prefix and VBI transitions; ClientId/Will fields exercise 65535-byte
  maxima and overlength rejection. Empty mandatory sections consume a byte.
- Size and Write preflight independently. All short capacities, invalid late
  fields, wrapped/null spans, count multiplication overflow and aliases preserve
  bytes and size outputs. Emission reuses prepared property sizes, validates no
  string twice and copies each active data field once. Known packet/property
  byte-budget failures precede property text scans. Abbreviated ACK/DISCONNECT/
  AUTH forms canonicalize; ordered data/properties/topics/reasons are retained.
- Thirty thousand arbitrary/mutated packets test publication/bounds and stable
  accepted canonical decode/encode forms. Ten thousand warm Read/MORE/Size/Write
  cycles preserve allocation counters and ambient error identity. Valid codec
  calls work under a one-byte quota. Own header/body/PFI and nested wire/property
  diagnostic OOM retain typed issue/status context with no live quota bytes.
- Windows GCC 16.2 O2 modular and all single tests pass. Ubuntu/WSL GCC 15.2 O2
  ASan+UBSan modular and direct new-leaf single tests pass. Prior wire/property
  suites remain green. Both platforms compile/run all four actual root examples,
  pass root/leaf trim including missing-dependency rejection, and run independent
  static/shared consumers exercising the five new public APIs against one XRT.
- PE/ELF export inspection confirms no `xmqttInternal` helper exports; ELF has the
  expected 20 public functions. Generated files, API inventory (20 functions /
  119 constants / 30 types, missing=0), asset maturity and 67 tooling tests pass.
  Strict release still correctly rejects the developing root. Four-product CI
  selects the real new tests/examples/profiles; remote Actions is not claimed.
- Four separate local workloads (wire, properties, private validated-ASCII filter
  grammar and full PFI/property QoS-1 PUBLISH decode+encode) pass Windows and
  Ubuntu/WSL smoke. One unpinned sample, with other validation potentially active,
  is build/workload evidence only. It proves neither a performance baseline nor
  network throughput, queue-inclusive tail latency, QoS/durability equivalence or
  production acceptance.

Packet/config and property/topic descriptor copies, span/alias checks, UTF-8
validation, topic grammar walks, scalar encoding and PFI payload scans remain
real costs. Reads borrow data; writers copy data once. Size+Write repeats a full
preflight. Cold diagnostics can allocate. No end-to-end zero-copy/zero-overhead
claim is made. The [packet contracts](../../extlibs/xmqtt/docs/codec.md) record
publication/lifetimes/limits, omitted forms, costs and pending connection rules.

Reproduce using `extlibs/xmqtt/config/modules.json`, suite `xmqtt_tests --cflag=-O2`.
Ubuntu/WSL modular uses `--arch x64 --no-single --cflag=-fsanitize=address,undefined
--cflag=-fno-omit-frame-pointer --ldflag=-fsanitize=address,undefined`. Direct
single tests use `tests/single/test_single_xmqtt_packet.c` and
`test_single_xmqtt_codec5.c` under the extension, with the strict C11/O2/ASan+UBSan
`-pthread -lm` pattern above. Product `xmqtt --no-single`, `xmqtt --trim-only` and
`package.py --suite xmqtt --kind static/shared --verify` cover examples/trimming/
external consumers. Ignored logs/reports use the `mqtt-codec5-*` prefix under
`out/service-extensions/`. MQTT 3.1.1 bodies, negotiated auth/alias/capability and
session rules, QoS runtime/recovery, durable storage and transports remain pending.

## Complete MQTT 3.1.1 packet codec validation

`xmqtt_codec311` implements independent Read/Size/Write for all fourteen 3.1.1
control types. It reuses public wire/Unicode and private packet grammar helpers,
with separate static protocol paths and no per-field version dispatch. It does
not depend on properties/codec5. Shared public layouts are unchanged; CleanStart
maps to Clean Session, and a CLIENT_ID issue code was appended for empty
persistent identifiers. No core/runtime gap or new shared runtime was required.

- Normative reference: MQTT 3.1.1 incorporating approved Errata 01. The corrected
  four-byte Remaining Length range is tested with complete PUBLISH packets at
  the 2,097,151/2,097,152 transition, plus the protocol-maximum header without a
  body. The existing nonminimal-header compatibility policy is explicit; writers
  always canonicalize to minimal integers.
- Twenty-two independent canonical fixtures cover all fourteen types, including
  complete optional CONNECT fields, QoS 0/1/2, raw binary/NUL/invalid-UTF8 data,
  multi-topic requests, ordered SUBACK codes and ordinary `$share/` text. All
  four header widths pass every prefix without changing Packet/Issue/scratch,
  coalesced-suffix and exact-cap tests. Returned views stay in the first packet
  and caller topic arrays. Short declared bodies cannot consume suffix bytes.
- Exhaustive CONNECT flags, SUBACK/CONNACK return bytes, all 65,536 CONNACK flag/
  code combinations and subscription option bytes cover version-specific rules.
  A 2,304-case fixed-body-length matrix rejects impossible lengths before body
  buffering. Both sender roles are checked for every type. Password without
  username, empty persistent ClientId, reserved AUTH, server DISCONNECT, nonzero
  ACK/DISCONNECT reasons, v5 properties/options and UNSUBACK reason lists are
  rejected. Nonempty ClientIds above 23 bytes and non-ASCII IDs are accepted by
  syntax; actual server identifier policy is separate.
- Payload and Will are opaque binary. V5-looking bytes inside PUBLISH data are
  preserved rather than guessed as a property section. Topic and payload caps,
  scratch/count limits, real zero budgets, uint16 ClientId/Will maxima and
  overlength fields, null/wrapped spans, aliases, vector multiplication overflow
  and late failures preserve the promised publication/atomicity boundaries.
  Ignored property scratch pointers/capacities are never dereferenced; a dedicated
  single test rejects accidental properties/codec5 feature selection at compile time.
- Thirty thousand raw/mutated packets preserve bounded views and stable accepted
  canonical encodings. Ten thousand warm MORE/Read/Size/Write cycles preserve all
  allocation counters and ambient error identity. Valid calls work under a one-
  byte quota; own/nested/header/identifier/protocol-version diagnostic OOM retains
  typed status/context, with zero live quota bytes after leaving the scope.
- Windows GCC 16.2 O2 modular and all single tests pass; Ubuntu/WSL GCC 15.2 O2
  ASan+UBSan modular and direct standalone 3.1.1 single pass, including existing
  wire/property/codec5 regressions. Both systems compile/run the five real root
  examples and pass root/leaf trim and missing-dependency probes. Independent
  static/shared consumers exercise both negotiated codec APIs against one XRT.
- PE/ELF checks find all three new public functions, 23 total ELF public functions
  and no exported internal helpers. Generated/API/asset checks pass (23 functions /
  121 constants / 30 types, missing=0), as do 67 build/API/workload tooling tests.
  Strict release still correctly rejects the developing root. Local Markdown
  links were checked, correcting an older codec-guide progress-link depth.
- A dedicated single-object size profile forbids all properties/codec5 function
  symbol prefixes. It passes on Windows and Ubuntu/WSL, with zero forbidden
  symbols. Windows GNU x64 O2 unstripped object: text 61,808, data 2,376, bss
  11,040, file 95,720 bytes. Ubuntu/WSL x64: text 61,483, data 2,416, bss 10,892,
  file 114,848 bytes. These include the public XRT dependency closure and object
  metadata, not just incremental codec text or a linked application footprint.
  Both native CI jobs now run this symbol-isolation gate; YAML/conditional steps
  were parsed locally. Remote Actions execution is not claimed.
- All five workload profiles pass Windows and Ubuntu/WSL one-sample unpinned
  smoke, potentially under other validation load. The new workload is local
  binary QoS-1 PUBLISH Read+Write, and is not directly comparable to the v5
  property/PFI workload. No pinned performance, queue-inclusive tail-latency,
  network delivery, QoS recovery, durability or production acceptance is claimed.

Config/metadata and fixed packet/topic descriptor copies, scalar writes, span/
alias checks, encoded-string UTF-8 and topic/filter grammar walks remain real
work. Data is borrowed on read and copied once on write; opaque payload has no
content scan. Size+Write repeats preflight. Cold error diagnostics can allocate.
The [3.1.1 contracts](../../extlibs/xmqtt/docs/codec311.md) document actual limits,
ownership, version differences and remaining client obligations.

Reproduce with manifest `extlibs/xmqtt/config/modules.json`, suite `xmqtt_tests
--cflag=-O2`; Ubuntu/WSL modular adds `--arch x64 --no-single` and the ASan+UBSan
flags above. Standalone source is `extlibs/xmqtt/tests/single/test_single_xmqtt_codec311.c`,
using strict C11/O2/ASan+UBSan `-pthread -lm`. Product `xmqtt --no-single`, trim,
static/shared verify and the performance config include the new assets. Run
`python tools/measure_size.py --compiler gcc --arch x64 --config
extlibs/xmqtt/config/size_profiles.json --profiles xmqtt_codec311 --kind single`
for the isolated closure gate. Ignored evidence/reports use `mqtt-codec311-*`
under `out/service-extensions/`. Client/session/negotiation/QoS state, persistence,
transports, peer interoperability and final performance/long-run acceptance remain
pending across the original four-product goal.

## Gregorian calendar validation

`xcron_calendar` adds exact UTC/civil matching and bounded earliest recurrence
search over the compiled Unix5/seconds6 value. It uses public XRT time APIs,
month/date/time bitmaps and a portable bit scan, with no new core gap/runtime.
UTC covers the full int64 millisecond domain. Floating civil labels cover all
nonzero int64 years; derived metadata is obtained in a safe equivalent year of
the 400-year cycle. Offsets/DST/weekday/year-day inputs are ignored, not treated
as a zone mapping. Output metadata is canonical with Offset=0 and IsDST=-1.

- UTC intervals are `[Begin,End]` or `(Begin,End]`, with inclusive End and safe
  upward whole-second rounding. Unix5 has second zero; nonzero milliseconds
  never match. Full timestamp endpoints and BCE/CE transitions work without
  constructing an unrepresentable midnight or a civil year zero. Civil year
  INT64_MIN/INT64_MAX and fractional/exclusive final-year cases are covered.
- Month/day masks exclude impossible dates, preserve star-vs-numeric-range
  DOM/DOW rules, and select earliest hour/minute/second lexicographically. No
  elapsed-second/minute scan is used. Gregorian 400-year repetition proves NONE
  after at most 4801 month visits, rather than imposing an arbitrary horizon.
  Tests exhaust the bound at both full UTC and full civil-year ranges.
- Ten independent predicate patterns walk every day of 2000..2399 (146097 days,
  1,460,970 comparisons), with an independently stepped calendar/weekday oracle.
  These include DOM-or-DOW, star steps vs numeric steps, explicit numeric full
  ranges, monthly restrictions, non-existent dates and leap centuries. Five
  hundred random earliest-time queries compare against a brute one-second
  oracle; one thousand sparse ten-year queries compare against a daily oracle.
- Known fixtures verify inclusive/exclusive millisecond interval
  edges, negative rounding, leap-century gaps, field carries, Fridays/weekends,
  year 12000, canonical civil metadata and ignored invalid offset/derived fields.
  Invalid expression masks/flags/dialect/Unix5 seconds, invalid dates/windows,
  wrapped/null pointers and output/input aliases preserve the stated outputs.
- Ten thousand warm cycles through all four APIs preserve allocation counters
  and ambient error identity. Whole-cycle NONE preserves both as a normal query
  result. One-byte quotas allow valid/none queries, while diagnostic OOM retains
  typed expression/date/window issues and leaves zero live budget bytes.
- Windows GCC 16.2 O2 modular/all single tests and Ubuntu/WSL GCC 15.2 O2
  ASan+UBSan modular/direct calendar single pass. Expression regressions remain
  green. A Linux modular test exposed a missing explicit `<limits.h>` test include;
  it was corrected. Trim-positive compilation exposed a missing public XRT feature
  expansion include in the new header; it was fixed and all positive/missing-
  dependency probes now pass. No source/runtime capability was weakened.
- Both systems compile/run the actual root expression/calendar examples. The
  calendar example returns Monday 2024-03-04 09:00 UTC after a fractional Friday
  firing, and 4801 visits for impossible February 30. Independent static/shared
  consumers exercise all four new APIs against one host XRT. PE/ELF checks find
  the expected five public xcron functions, with no exported private helpers.
- API/generated/asset checks pass: five functions / 35 constants / eight types,
  missing=0; 67 tooling tests pass. Strict release continues to reject the
  developing root. Local documentation links and both native CI conditional
  calendar-size steps were parsed/checked. Remote Actions execution is not claimed.
- The calendar single-object size gate forbids system-local time and time-text
  function prefixes; Windows and Ubuntu/WSL pass with zero forbidden symbols.
  Windows GNU x64 O2 unstripped object: text 46,720, data 2,376, bss 11,072,
  file 77,082 bytes. Ubuntu/WSL x64: text 49,706, data 2,648, bss 10,892,
  file 95,680 bytes. These include the public XRT closure/object metadata,
  not only incremental calendar text or a final application footprint.
- Expression and calendar workloads pass one-sample unpinned Windows and
  Ubuntu/WSL smoke, potentially under other validation load. Calendar workload
  mixes dense, sparse, leap and impossible searches over precompiled expressions.
  This establishes workload execution, not a pinned baseline, scheduler throughput,
  queue-inclusive tail latency, timezone/DST, durable recovery or production parity.

Input validation, span/alias checks, fixed date/metadata copies, XRT conversion
and month-length calls, bitmap scans/rotation, month iteration and optional work
counter writes remain real costs. Normal queries allocate/retain nothing; cold
diagnostics can allocate. Separate calls repeat validation. The
[calendar contracts](../../extlibs/xcron/docs/calendar.md) preserve these costs and
the remaining IANA transition-segment/gap/fold responsibilities. A forward search
from only the current local label cannot replace ordered UTC-segment search.

Reproduce with `extlibs/xcron/config/modules.json`, suite `xcron_tests --cflag=-O2`;
Ubuntu/WSL modular adds `--arch x64 --no-single` and the sanitizer flags above.
Direct single uses `extlibs/xcron/tests/single/test_single_xcron_calendar.c` with
strict C11/O2/ASan+UBSan/`-pthread -lm`. Product `xcron --no-single`, `--trim-only`,
and static/shared `--verify` cover examples/distribution. Its size config profile
`xcron_calendar --kind single` proves the isolated dependency boundary. Ignored
logs/reports use `cron-calendar-*` under `out/service-extensions/`. Named zones,
DST policies, clocks/executors, misfire/overlap and durable restart remain pending;
the full four-product goal and its acceptance gates are unchanged.

## Owned Redis response frames

`xredis_reply` incrementally builds immutable owned RESP2/3 semantic frames on
the existing event reader. It uses public XRT allocation, atomics and number
conversion; no new shared runtime/core capability was needed. READY transfers
one reference only after the complete frame is validated. Every other result
preserves the result output. Input may be recycled immediately, and transferred
replies survive reader Reset/Destroy.

- Index-based nodes/byte offsets preserve all scalar/aggregate types, binary
  data, numeric lexemes, verbatim format prefixes, original null kinds, streamed
  flags, duplicate map keys, wire order, pushes and ordered root/nested metadata.
  Attributes can annotate metadata keys/values without consuming child slots.
  Node views borrow the completed frame, with explicit child/metadata chains.
- Native integers/booleans come from the checked reader. Lazy DOUBLE conversion
  uses locale-independent correctly rounded public XRT parsing, preserving raw
  spelling. Tests cover negative zero, infinity, normalized/legacy signed NaN,
  finite rounding, subnormal/underflow, maximum finite double and overflow. A
  conversion overflow preserves the output and the valid lossless reply.
- Two bounded control/stack allocations are made at creation. A frame embeds
  one node and 64 payload bytes. Memory statistics verify exactly one allocation
  and free per small scalar across 1000 warm feed/access/convert/retain/release
  cycles; successful calls preserve ambient error identity. Larger frames grow
  node/data vectors without per-node allocations or a final flatten/second parse.
  Known bulk lengths reserve their bytes before fragmented payload delivery.
- MaxOwnedBytes limits peak requested frame bytes, including inline storage,
  capacity slack and the complete growth destination while the old buffer is
  still live. Its default is 64 MiB. It excludes allocator overhead, diagnostics,
  control stacks and transferred replies. Higher runtime layers must bound
  concurrent/retained replies. Node/data growth remains actual allocation/copy
  work; normal access/refcount/Finish/Reset does not allocate.
- All types and metadata fixtures pass every two-part split, bytewise input and
  feed budgets 1..3, recycling/overwriting source buffers immediately after each
  Feed. Ordered trees and binary payloads match independent explicit fixtures.
  A 32-bulk vector verifies payload relocation and fragmentation-independent
  capacity; an unknown 3000-byte scalar forces repeated byte-vector growth.
  A 512-level annotated tree verifies all 1025 node links and the deepest leaf.
- Malformed terminators after DATA, bad pushes/maps, orphan metadata and partial
  EOF never publish a frame. Invalid arguments, wrapped spans and input/output/
  control/immutable-frame aliases preserve the stated outputs/state. Coalesced
  normal/push frames stop at the correct boundaries; retained data outlives the
  builder. Protocol/local-limit/OOM/EOF failures free partial storage immediately
  and stay terminal until Reset. Typed builder causes remain observable even
  when the nested callback error or diagnostic OOM would obscure them.
- Real XRT quotas sweep 247 frame-allocation thresholds and 126 creation
  thresholds, crossing both failure and success boundaries, clearing diagnostics
  and proving zero live charged bytes. A one-byte diagnostic quota retains the
  protocol cause independently of the ambient memory fallback. Four native
  threads complete 80000 shared immutable reads/retain/release cycles with the
  original owner released before joining. This is lifetime stress, not a claim
  of complete race/production acceptance.
- Windows GCC 16.2 O2 modular and all single-header regressions pass. Ubuntu/WSL
  GCC 15.2 O2 ASan+UBSan modular and direct owned single tests pass, including
  the deep/alias/quota/thread additions. The first test compile caught a test
  indentation warning and an incorrect XRT wait constant; both were corrected.
  Both systems compile/run the actual command/reader/owned examples and pass
  positive/missing-direct-dependency trim probes. Independent static/shared
  consumers use one host XRT and exercise owned traversal/conversion/refcounts.
  PE/ELF exports contain all 21 public xredis functions with no private helpers.
- Generated features, amalgamation, API/asset audits and 67 tooling tests pass:
  21 functions / 60 constants / 20 types, missing=0. Strict release still rejects
  the developing product root. Both native CI size-boundary steps were parsed;
  remote CI execution is not claimed. The size gate finds zero forbidden network,
  thread, task or future function prefixes in the owned module's isolated closure.
- Final GNU O2 unstripped single-object closure: Windows x64 text 69408, data
  2376, bss 11040, file 102158 bytes; Ubuntu/WSL x64 text 71183, data 2424,
  bss 10892, file 124384 bytes. Both reports have source fingerprint
  `128be72e179b1fd6b4a3482ebef29622538ea406ef91c760b9a81d4c89aafc7f`.
  These include required XRT closure/object metadata, not incremental reply
  text or a final application footprint. The Linux checkout does not resolve
  the Windows worktree's Git metadata, so its report uses the content fingerprint.
- All three command/reader/owned workloads pass one-sample unpinned Windows and
  Ubuntu/WSL smoke, potentially under other validation load. Owned smoke mixes
  inline scalar, annotated array and streamed/binary frames, including allocation,
  traversal, lazy double conversion and release. It establishes workload execution,
  not a pinned baseline, network throughput or queue-inclusive tail latency.

The [owned-reply contract](../../extlibs/xredis/docs/reply.md) records actual
copies/allocations, atomic and validation costs, ownership and resource exclusions.
Reproduce with `extlibs/xredis/config/modules.json`, suite `xredis_tests --cflag=-O2`;
Ubuntu/WSL modular adds `--arch x64 --no-single` and the sanitizer flags used
above. Direct single source is `extlibs/xredis/tests/single/test_single_xredis_reply.c`,
using strict C11/O2/ASan+UBSan/`-pthread -lm`. Product `xredis --no-single`, trim
and static/shared `--verify` cover examples/distribution. Size profile
`xredis_reply --kind single` enforces the isolated closure. Ignored reports/logs
use `redis-reply-*` under `out/service-extensions/`.

Next is the xredis request/connection vertical slice: ordered reply correlation,
out-of-band pushes, bounded admission, queue-inclusive monotonic deadlines,
cancellation/wire obligations and deterministic close/failure ownership. Network
transports, pipelines/pools, TLS, transactions/blocking/PubSub, Sentinel/Cluster/
Tracking and all original four-product interoperability, performance and long-run
acceptance gates remain pending. The complete goal is unchanged.

## TCP request and connection vertical slice

`xredis_client` adds a real raw TCP connection on one existing XRT engine worker,
using public TCP/Future/cancellation/timer/allocation interfaces. Numeric endpoint
setup and calls are asynchronous; a sync facade rejects both the connection's
worker and another affinity on the same engine. No new shared runtime/core gap
was introduced. The product root still remains developing.

- Thread-safe admission copies/encodes arguments before returning acceptance.
  Request/count/encoded-byte caps include concurrent construction and actual
  wire/timer/cancel tails. Worker work is bounded for ingestion, sending,
  cancellation and retirement; receive slices have byte/frame budgets. TCP takes
  references to encoded buffers, avoiding another extension payload copy. Opaque
  wire bytes are not redundantly cleared before encoding.
- Accepted Futures resolve with an immutable typed request outcome, including
  timeout/cancel/failure. Generic RESOLVED alone is not success. Server errors,
  request deadlines, cancellation, local resource/OOM, protocol and connection
  failure remain distinct. NOT_SENT/MAY_HAVE_EXECUTED/REPLIED records knowledge
  at logical completion; TCP acceptance is not proof of remote execution. The
  preallocated outcome survives diagnostic OOM. Retained completed results do
  not retain the client/engine. No operation is silently replayed.
- One monotonic deadline starts at API entry, including encoding, submission
  wait and connection setup. Cancellation/deadline is rechecked before send and
  response publication. A controlled worker stall proves timeout before any
  command is sent; zero timeout also sends nothing. The sync guard is tested
  from both engine workers. Valid admission preserves caller error identity.
- Sent timeout/cancel leaves a counted FIFO tombstone until its late response
  drains, preventing response cross-talk. Positive drain grace bounds a stuck
  obligation and retires the connection. Surviving calls receive DISCONNECTED,
  while CloseStatus records the connection's TIMEOUT cause; another request's
  grace is not falsely reported as each surviving request's own expired deadline.
  Count/byte overload preserves outputs and still counts abandoned sent slots.
- RESP3 pushes/attributes use an independent bounded ownership ring. Normal
  replies continue to match the original sent FIFO. Push count/owned-byte
  overflow and unsolicited ordinary replies retire the connection explicitly.
  Known reply-cardinality switches are rejected before admission; dedicated
  PubSub/replication-style modes are not pretended to work as ordinary calls.
- Close stops admission, completes accepted requests with explicit certainty,
  and waits for physical transport/data ownership retirement. Actual request
  free precedes reservation release. Stream/parser owners and the engine pin
  drop before Closed publication; callback return tails still require normal
  engine stop/join before code unload. A one-item work budget retires 101 calls
  through bounded successive turns, including a sent blocked command. Result
  data survives subsequent client/engine destruction in actual examples and
  static/shared consumers.
- Functional tests cover binary echo, 64 ordered calls through a narrow pipeline,
  three-byte/two-byte receive budgets, server errors, root annotated pushes,
  timed/cancelled queued and sent requests, late replies, drain retirement,
  malformed payload terminators, partial EOF, peer reset, connect refusal,
  unsolicited frames, invalid modes/config/endpoints/aliases and reentrant
  Info/asynchronous admission/Close from a Future notification. Four native
  producers run 384 mixed successful/cancelled calls with unique correlation.
- Caller allocation quotas sweep 36 creation and 71 request thresholds, cross
  both OOM/success boundaries and prove zero live charged bytes after retirement.
  A one-byte worker quota proves typed MEMORY under worker/diagnostic failure.
  Initial quota runs exposed a retained Future-cancel token reference; the
  missing release was fixed. Inspection also moved counters after actual buffer
  free. These are extension fixes, not claimed XRT allocator defects.
- Windows GCC 16.2 O2 uses actual IOCP; Ubuntu/WSL GCC 15.2 uses actual epoll.
  Modular regressions and standalone client single tests pass, with Linux
  ASan+UBSan. TSan single tests also pass at O1 with `-Wno-error=tsan`: GCC warns
  that the generic public XRT `atomic_thread_fence` routine is not instrumented.
  The warning was retained, not hidden by a source/CI policy change. This
  qualified local run is not complete native-Linux race/production acceptance.
  A GCC single-TU diagnostic about the size output's address was fixed with
  explicit initialization; normal strict C11 warning policies remain enabled.
- Both systems compile/run the actual client example and independent static/
  shared consumers with one host XRT. Positive/missing-dependency trim probes,
  generated files, API/asset checks and 67 tooling tests pass. API inventory is
  31 functions / 84 constants / 29 types, missing=0. PE/ELF exports have the 31
  public functions and no private helpers. Strict release still rejects xredis.
- Both native CI jobs now check isolated reply/client dependency boundaries.
  Their YAML conditions and 40-hex action pins were parsed; remote Actions
  execution is not claimed. Both isolated profiles have zero forbidden symbols:
  owned reply remains independent of networking, and raw client excludes TLS,
  HTTP/WebSocket, Dial and task-executor functions.
- Final O2 unstripped single-object client closure: Windows x64 text 351604,
  data 2384, bss 11136, file 508176 bytes; Ubuntu/WSL x64 text 365411, data 4480,
  bss 10908, file 657400 bytes. Reports share content fingerprint
  `a07b0fdcad7102edbddf65025614540707c5f32867d4b4fe3130a17b3ebbd0b1`.
  Required XRT closure/object metadata is included, not only incremental client
  code or a final linked application's footprint. The owned-reply closure did
  not acquire network dependencies.
- All four workloads pass one-sample unpinned Windows and Ubuntu/WSL smoke.
  The new closed-loop local peer uses two workers and pipeline 16; latency is
  API-entry-to-observation and includes encoding/queueing, plus consumer batch
  observation wait. It reports throughput and P50/P95/P99/P99.9. It also measures
  peer parsing/response generation, ran alongside other validation work and is
  not fixed-arrival-rate, a stable baseline, Redis/Hiredis parity or a production
  latency claim. New metadata/Future/watch/timer allocations, copies, locks,
  atomics, syscalls and cold paths remain real, incompletely quantified costs.

The [client contract](../../extlibs/xredis/docs/client.md) records concrete
ownership, outcome, timing, quota and residency rules. Reproduce using manifest
`extlibs/xredis/config/modules.json`, suite `xredis_tests --cflag=-O2`; Linux
modular adds `--arch x64 --no-single` and the ASan+UBSan flags recorded above.
Standalone source is `extlibs/xredis/tests/single/test_single_xredis_client.c`:
Windows strict C11/O2 links `-lws2_32 -lmswsock`; Linux uses `-pthread -lm` and
ASan+UBSan, or O1/TSan with the qualified flag above. Root `xredis --no-single`,
trim and static/shared `--verify` exercise actual examples/consumers. Size
profiles `xredis_reply,xredis_client --kind single` enforce isolation. Ignored
reports/logs use `redis-client-*` under `out/service-extensions/`.

Protocol negotiation/AUTH/HELLO, DNS/TLS, reconnect policy, pools and dedicated
transaction/blocking/PubSub modes, Sentinel/Cluster/Tracking and all original
four-product interoperability, matched pinned performance/overhead and native
72-hour mixed-load/fault acceptance remain pending. The complete goal is active
and unchanged; this is the ordinary TCP connection foundation.

## Managed Redis connection initialization

Implemented the next P2 feature on the existing public-API TCP worker model.
The product root stays developing and the original four-product goal remains
active.

- New explicit ConnectInit configuration and a shared typed Ready Future add a
  barrier before ordinary requests enter the TCP pipeline. Default is strict
  HELLO 3; AUTH/ACL username, SETNAME and SELECT are optional. Explicit RESP2
  sends legacy AUTH/name/SELECT without requiring HELLO. Explicit AUTO falls
  back only on NOPROTO or canonical unknown HELLO, never auth/syntax/shape or
  other server errors. Old raw Connect permits manual control commands and
  reports transport readiness without asserting a negotiated protocol.
- HELLO validates unique server/version/proto fields, retains unknown fields
  and ordered attributes, and publishes the owned map only after every setup
  step succeeds. Legacy steps require exact simple-string OK. Typed failure
  retains the failed step and selection state. Server error text is discarded;
  retained diagnostics use fixed redacted text to avoid copying echoed auth
  arguments. Admission stops before failure notification reentry.
- One original monotonic setup deadline spans encoding, worker wait, TCP and
  every step. Business requests may queue, expire or cancel independently with
  their own original deadlines; no business bytes precede successful setup.
  Setup rejection completes surviving calls INITIALIZATION/NOT_SENT; native
  parser/transport/resource failures retain their distinct categories. Cancelling
  an observer does not cancel shared setup; Close explicitly does.
- Caller username/password/name spans are borrowed only during creation.
  Strict modes encode them once; AUTO preencodes HELLO and fallback forms, so
  credentials/name are copied twice. The aggregate wire cap includes unused
  fallback, and each command must fit TCP's write limit. One setup allocation
  is protected by independent owner/send references and wiped before its actual
  final free. InitBytes reaches zero only afterward. Closed waits for this
  physical ownership and the setup timer. Caller/peer/OS/transport buffers and
  arbitrary application payloads are outside that erasure contract. Raw AUTH/
  HELLO request storage receives the same final wire wipe.
- Deterministic tests exercise ten positive setup scripts, binary/empty/default
  inputs, caller-buffer overwrite, root metadata, one-byte reads, exact step
  ordering, held replies/pushes, queued timeout/cancel, observer cancellation,
  fifteen HELLO error/shape fixtures, each legacy failure step, generic AUTH
  server errors distinct from rejected credentials, before-TCP/
  worker-stall/HELLO/SELECT timeouts, peer reset, pending Close, aggregate cap,
  malformed options/aliases, managed control rejection and notification reentry.
  Creation quotas sweep 61 thresholds and cross failure/success with zero live
  charged bytes; two one-byte worker scopes cover timer/response and diagnostic
  OOM without losing preallocated typed results.
- Review corrected an overly broad legacy AUTH error classification: only
  explicit authentication rejection is AUTH_FAILED; generic command/server
  errors remain SERVER_ERROR. Dedicated fixtures cover this distinction. Peer
  counter checks also wait for the enclosing dispatcher, rather than assuming
  a delivered response means its final accounting instruction already ran.
- Windows GCC 16.2 strict C11/O2 modular and all five leaf single tests pass with
  IOCP. Ubuntu/WSL GCC 15.2 modular and isolated initialization single pass O2
  ASan+UBSan with epoll. The isolated initialization single also passes O1 TSan
  with the already-qualified `-Wno-error=tsan` for generic XRT atomic_thread_fence;
  that warning is retained, and native race/production acceptance remains open.
- A dedicated external consumer passed against three disposable Redis 8.0.5
  configurations under WSL: no auth, default-user password, ACL user. All run
  strict RESP3, explicit RESP2, AUTO, actual name/database, binary SET/GET,
  32-request pipelines, name/database/auth failures, raw HELLO and post-engine
  result lifetime. The supervisor uses only child-owned loopback instances,
  disables persistence, checks their own successful startup, terminates them
  and removes temporary data. It preserves redis-server's symlink invocation
  name after an initial argv[0] multiplexer error. Actual Redis binary SHA-256:
  `0f3f5d696b7dabd9af484c2d8844dbb0c9bc7722ddf794f63b911a9795509293`;
  server v8.0.5, jemalloc 5.3.0, build d47fc40702b1fad8. The corrected complete
  run pins source/client/server/runner hashes in its ignored JSON report.
  Older real peers and native Windows/Linux interoperability remain pending;
  deterministic legacy fallback is not advertised as an actual old Redis run.
- Both systems run the real root examples and independent static/shared
  consumers with one host XRT, including new configuration/readiness APIs and
  metadata after engine destruction. Trim probes, generated outputs, API/normal
  asset maturity, local documentation links and 67 tool regressions pass. API
  inventory is 34 functions / 110 constants / 34 types, missing=0. PE and ELF
  export the same 34 public functions, without private helpers. Strict release
  correctly rejects the developing product. No general core source changed.
  One parallel local example rerun collided in a shared native output directory
  (Windows COFF objects reached the Linux linker). Both example sets were then
  fully rebuilt in separate Windows-native and Linux-x64 directories and passed;
  the failed collision log is retained and is not accepted test evidence.
- Reply/client isolated O2 single objects still have zero forbidden symbols.
  Client closure Windows text/data/bss/file: 366732/2384/11136/526757 bytes;
  Ubuntu/WSL: 380981/4496/10908/680832. These include the required core and
  unstripped object metadata. Content fingerprint shared by both size reports:
  `b533c307630482c01f4203259df107f1b39274c9d0b325c2954462a91836c473`.
  Owned replies retain their previous footprint and network isolation.
- All five performance workloads pass one-sample unpinned smoke on both
  systems, alongside other validation. The new cold workload uses sixteen
  serial HELLO AUTH + SELECT setups; percentiles span API entry through ready
  observation. Lifecycle rate additionally includes fixture/engine create and
  retire costs. These sparse samples are workload plumbing, not reliable tails,
  production throughput, a stable baseline or Redis/Hiredis parity. Performance
  source fingerprint is
  `7e7197040f10345283de0724b93e916ccc5b990f036f638a4800911dda664923`;
  benchmark fingerprint is
  `e442e750435417d4dd3136c5370b0b77be8a9292ef9035058536839be41fc09f`.
  New cold Future/promise/result, setup wire/timer, copies, map traversal,
  reference operations and full wire wipe are explicitly real costs. Final
  per-operation allocation/copy/sync/syscall measurement is still required.

The [initialization contract](../../extlibs/xredis/docs/client-init.md) contains
public semantics, costs and runnable modular/single/example/real-Redis commands.
The external supervisor is opt-in, not a runtime dependency. Ignored evidence
uses `redis-init-*` under `out/service-extensions/` and its `evidence/` directory.
DNS/TLS, reconnect/pools, dedicated modes, HA/Tracking, remaining RPC/MQTT/Cron
runtime scopes, pinned matched performance and native 72-hour fault/long-run
acceptance all remain required. Completing setup does not complete any product.

## Performance workload input identity

A focused reproduction changed only a declared peer header while keeping the
benchmark source unchanged: the old fingerprint stayed identical. The failing
unit log is retained as `performance-inputs-red.log`. Benchmarks now declare
repository-relative `inputs` for helper/fixture bytes, which participate in the
benchmark fingerprint and remain visible in JSON reports. Missing files,
directories, outside-repository paths, malformed lists and duplicate declarations
are rejected. Production source identity remains separate so implementation
changes can still be measured under one workload.

The two Redis runtime profiles register their direct and transitive peer
headers. Windows-native and Ubuntu/WSL-x64 smoke runs of both profiles pass and
emit the expected input lists; these are execution checks, not new performance
baselines. All 69 relevant tooling tests pass, including the prior failing
mutation and path/contract checks. Existing historical baselines are preserved;
the changed runner/workload fingerprint requires fresh accepted measurements.
No library runtime or public ABI changes in this step. Logs/reports use
`performance-inputs-*` in the ignored service-extensions output directory.

## Fixed-arrival Redis runtime measurement

Added an actual fixed-arrival binary ECHO workload, separate from the existing
closed-loop and cold-setup workloads. No production library source, public ABI
or shared event runtime changes in this feature.

- Every offer retains its absolute planned time. Generator delay never shifts
  subsequent arrivals; expired-before-admission, harness observer-capacity,
  client overload/closed/error and every accepted typed terminal status remain
  counted. The harness verifies both admission and terminal-status partitions
  sum exactly. Fast rejection is separate from successful latency/throughput.
- Successful/all-outcome P50/P95/P99/P99.9 use planned-arrival-to-observation
  latency and nearest rank. Generator lag is reported separately. Remaining
  relative client deadlines are rounded down; independent intended-deadline
  misses still count delayed/equal-deadline observations. This is not an atomic
  absolute-deadline API claim. Normal runs with no successes are invalid.
- Preallocated offer records and bounded reusable observer slots protect
  retained Futures. Future notification only timestamps; payload verification
  runs on the generator. A slot is recycled after the Watch Release signal,
  including the already-ready registration case. Physical Close and engine
  join precede freeing shared callback storage. Binary IDs and payload bytes
  are verified for every successful reply.
- CPU, process-lifetime peak RSS/working set, sampled request/wire peaks, explicit
  harness bytes and before/after native handle/descriptor counts are reported.
  The generator's polling/yield and fixture work are included in local CPU.
  A distinct compiled/enabled memory-statistics profile records process-window
  XRT API/block/backing allocation calls. These include cleanup and local peer
  effects, not isolated incremental client counts. Unmeasured counters are
  explicit unavailable/null in external JSON; payload-copy bytes are lower
  bounds. Exact copy/lock/syscall tracing remains pending.
- Four registered profiles cover ordinary fixed arrivals, allocation counters,
  a 50 ms generator stall and held fixture replies. Controlled smoke observes
  all 32 stalled offers expired and all 64 held-case offers partitioned into
  eight timeouts plus 56 harness-capacity rejections on both systems. These
  control profiles verify methodology, not service throughput acceptance.
- Unit coverage includes 10,000 rational arrival oracles, overflow/atomic error
  outputs, small-sample/size_t-boundary nearest-rank cases, 10,000 single-finalize
  ledgers, deadline equality, invalid bounds and literal binary-corpus checks.
  Real local TCP tests cover 32 successes, a stall without sending business
  commands, held replies and enabled counters. Native Windows GCC 16.2 O2
  modular/single and Ubuntu/WSL GCC 15.2 O2 ASan+UBSan modular/single pass.
  O1 TSan also passes with the retained generic XRT atomic_thread_fence warning
  qualified by -Wno-error=tsan. An initial O1 compile required explicitly
  initializing checked schedule outputs; ordinary strict warnings stay enabled.
- A disposable real Redis 8.0.5 service runs the same model with one client
  worker plus the generator, outside the server process. Uninstrumented and
  instrumented diagnostics each complete all 512 offers at a requested 5000/s,
  with zero unsuccessful outcomes and descriptors 4 -> 4 after teardown. The
  instrumented process window observes 3072 malloc API calls and
  4096 block allocations; these are whole-process/window observations,
  not a claim of per-operation or steady-state isolated overhead. Actual
  executable/source/service-helper/build-report hashes and actual expanded
  arguments/facts are retained. The shared service supervisor's existing
  three authentication/protocol interoperability cases still pass.
- All 69 relevant tool regressions, generated/API checks and normal maturity
  pass. Public inventory remains 34 functions / 110 constants / 34 types with
  missing=0. Product root stays developing. Existing CI's registered test and
  workload selections include the new assets; remote CI execution is unclaimed.
- Additional direct local diagnostics retain complete raw counter output and
  verify every admission/terminal partition on Windows and WSL. They are
  separately labeled from the runner's one-sample smoke. Current four-profile
  workload fingerprint: `ffef590f9853ddbec28407afc78cd45df1741580cc9bac19e76531dc22f6f46d`;
  production/config source fingerprint: `955a1da6fa2f97501f7fe41edb9c2fee58d91e0ca3d66a6c53d9207314d91c0e`.

The [measurement contract](../../extlibs/xredis/docs/client-performance.md)
records limits, clock/quantile semantics, observer lifetime, instrumentation
scope and reproducible local/real-server commands. Ignored evidence uses
`openloop-*` under `out/service-extensions/` and `evidence/`. These are unpinned,
sparse diagnostics run alongside validation, not stable baselines or Hiredis
parity. Comparator installation is currently absent, and no reference source
has been embedded. Matched comparator/resource matrices, precise copy/sync/
syscall costs, native long runs, remaining reconnect/transport/product features
and every original four-product release gate remain required.

## Optional Redis DNS and multiple-endpoint setup

Implemented the optional `xredis_dial` leaf using the existing public XRT
Resolver/Dial APIs. The three functions configure network policy, submit a
bounded copied hostname with an application-owned Resolver, or submit a retained
immutable endpoint list. NULL initialization keeps strict RESP3 defaults;
existing AUTH/SELECT, request limits, FIFO ownership and readiness rules remain.
No core XRT source or public structure layout changed, and no shared runtime or
per-request transport vtable was introduced. Product root remains developing.

- Overall initialization deadline begins at API entry, including host scan/copy,
  worker queue, DNS, candidate attempts and Redis handshake. Separate native Dial
  timeout/fallback/attempt/family limits remain explicit. Pending calls preserve
  their original deadlines and cancel with NOT_SENT while DNS is blocked.
- Physical Close waits native callback retirement and any winning Stream. The
  interrupted-DNS test initially found the adapter omitted transport retirement
  when public Close won before a Stream existed; the terminal Dial callback now
  marks that case. The client and Engine can retire before a blocked underlying
  query returns. Shared Resolver/LookupData are separately joined and kept alive;
  no bounded OS-query interruption is promised. An earlier fixture failure was
  caused by returning nonzero ports against the Resolver's port-free contract,
  and was fixed in the fixture, without modifying core behavior.
- Tests cover host source recycling, IPv6/IPv4 candidates, explicit refused-port
  fallback, list immediate release, business FIFO, DNS wait/cancel/deadline,
  initialization and native timeouts, already-expired/closed queued startup,
  DNS miss, Resolver host/request caps, family/attempt caps, malformed spans,
  embedded NUL, aliases, 65 distinct endpoints/zero port, constructor quota
  thresholds, worker OOM both before setup timer and at native Dial allocation,
  RESP2, redacted auth failure and readiness Info/Close notification reentry.
  Retained results survive client/Engine/Resolver; blocked late lookup explicitly
  returns only after client/Engine destruction.
- Windows-native GCC 16.2 O2 passes all seven modular and seven single tests on
  IOCP. Ubuntu/WSL GCC 15.2 O2 ASan+UBSan passes all seven modular tests and the
  direct Dial single test on epoll. Dial O1 TSan passes with the existing generic
  XRT atomic_thread_fence warning retained via `-Wno-error=tsan`. These do not
  constitute native Linux long-run or exhaustive race acceptance.
- Both platforms pass root examples, dependency trim probes, and independent
  static/shared package consumers executing hostname resolution, HELLO and PING.
  Public inventory is 37 functions / 113 constants / 35 types, missing=0. PE and
  ELF exports exactly match all 37 public functions; private handoff helpers are
  not exported. Generated headers/reference and normal maturity checks pass.
- Real Redis 8.0.5 on WSL passes the existing no-auth/password/ACL matrix through
  `localhost` and one shared system Resolver: RESP3/RESP2/AUTO, actual name/db,
  binary SET/GET, 32-call pipelines, server/auth failures and retained metadata.
  Numeric manual HELLO remains a control. Server executable SHA-256 remains
  `0f3f5d696b7dabd9af484c2d8844dbb0c9bc7722ddf794f63b911a9795509293`.
  Native Windows real Redis and wider version/OS resolver matrices are pending.
- Numeric `xredis_client` selection compiles out private plan fields/functions
  and forbids Dial/Resolver/resolve-op/client-Dial symbols; selected Dial forbids
  TLS/HTTP/WebSocket/task executors. Both independent symbol/size checks pass.
  Text/data/bss/file bytes: Windows numeric 366668/2384/11136/526705, Dial
  420236/2384/11136/606678; WSL numeric 380917/4496/10908/680776, Dial
  437498/5200/10908/786496. These are current closure measurements, not matched
  historical growth or runtime overhead claims. Size source fingerprint is
  `1d45c2f3b896fccde28d32140756569950c2a3c8b1a173c5e7db7e918f13010a`.
- Four unpinned cold smoke profiles pass on both systems: numeric control in the
  same Dial-enabled binary, deterministic hostname, prepared good endpoint, and
  separately instrumented hostname. Each records 16 samples, exact mode/backend/
  query count, factory-to-ready-observation nearest-rank percentiles and complete
  fixture/Engine/Resolver lifecycle rate. Allocation counters are whole-process
  window including fixture and cleanup, not isolated client increments. The
  descriptor allocation, copied/scanned host, XRT's own host copies/normalization,
  native operations/timers, private fields/flags/posts/references and existing
  setup wipe remain real costs. No production tail/throughput or zero-overhead
  claim follows. Production/config fingerprint:
  `5305612185bf80adfa575060b245104c9b5d63f38e25fd5dcbb1b841187358c6`;
  four-profile workload fingerprint:
  `49bae21fb95fb9024a69fcaf9001e1af760f2eccdc131aa05c25a55e41a3ea53`.
- The 33 targeted asset/API/layout/performance tool regressions pass. An additional
  40-test broad check has 34 passes and six non-green cases: four strict product
  completion assertions correctly reject the developing roots; unchanged core
  maturity reports missing net_file_future single closure; unchanged xhttp ABI
  test references unavailable xhttp1callresult. These are recorded failures, not
  a claim that the repository-wide suite passes. The explicit xredis `--release`
  audit still exits 1 solely for its developing product root. No gate was relaxed.

The [Dial contract](../../extlibs/xredis/docs/dial.md) records limits, callback and
Resolver lifetime, runnable example/package/system-Resolver interoperability and
actual measurement scope. Ignored evidence uses `redis-dial-*` in
`out/service-extensions/evidence/`, with direct sanitizer binaries one directory
above. CI now checks the selected Dial and unselected numeric boundaries;
remote CI execution is unclaimed. TLS, reconnect/pools, dedicated modes,
HA/Tracking, remaining RPC/MQTT/Cron runtimes, matched pinned resource/performance
matrices and native 72-hour mixed/fault acceptance all remain required.

## Server-authenticated Redis TLS transport

Implemented the optional `xredis_tls` transport with four public functions:
config initialization, hostname Dial, prepared-address Dial and numeric Connect.
It uses the existing public NetDial and TLS Session/Stream APIs. One native TLS
session snapshots borrowed metadata/shared objects during the factory, then
attaches on the connection's worker after TCP wins. No core XRT implementation
was modified and no extra TLS-Dial/Future stack or shared runtime was added.
This is server-authenticated transport; mutual client-certificate authentication
and ticket harvesting/cache policy remain required follow-up work.

- TLS must succeed before Redis initialization or business writes. There is no
  insecure verifier default or plaintext fallback. Explicit Context/Verifier/
  Resume policy remains native XRT policy. Overall initialization deadline starts
  at API entry and includes native-session creation, DNS/TCP/TLS and setup.
  Bounded copied names/opaque ALPN, original queued request deadlines, optional
  DNS SNI/identity inference and explicit numeric/prepared identity are documented.
- TLS Send's accepted prefix is tracked exactly. A zero-byte AGAIN restores the
  original pending FIFO position; any accepted prefix is a sent obligation, even
  when ciphertext queue allocation subsequently fails. Cancellation/timeout of a
  partial command finishes that prefix's remainder and drains its tombstone under
  the existing grace; Close aborts it. Setup uses the same exact-prefix rule.
  Native plaintext is read/consumed through TLS APIs; raw buffer mutation and raw
  pause/resume are excluded for TLS. Native Close plus client callbacks/timers/
  requests/setup wire/internal posts retire before client Closed. Engine join
  remains necessary for native callback/session tails and code unloading.
- Tests force 3-byte setup/business budgets and 1-byte interrupted writes, binary
  names/ALPN snapshot recycling, binary AUTH/SELECT and close during partial AUTH,
  32-call binary pipelines, numeric/hostname/refused-endpoint fallback, verifier
  rejection, handshake/overall deadlines, close during blocked DNS, caller client
  release before physical Close, caller verifier release before DNS returns,
  constructor quota thresholds, malformed spans/limits and worker OOM. An initial
  send-OOM assertion assumed no accepted bytes; actual native prefix acceptance
  correctly returned MAY_HAVE_EXECUTED before queue metadata OOM. The test now
  checks typed memory failure with the native zero/partial delivery possibilities,
  rather than asserting a false NOT_SENT guarantee.
- Windows GCC 16.2 O2 passes all nine modular and nine single tests; the TLS pair
  was rerun with the final lifetime/auth cases. WSL GCC 15.2 O2 ASan+UBSan passes
  all nine modular tests and the direct TLS single test. O2 TSan passes both TLS
  and TLS open-loop single tests, retaining the generic atomic_thread_fence
  instrumentation warning through `-Wno-error=tsan`. The O1 TSan compile is not
  green: unchanged native xrtTlsClientPsks checked-output locals trigger
  -Wmaybe-uninitialized. O2 keeps -Werror and requires no such suppression. These
  are bounded local checks, not exhaustive races or native Linux long-run proof.
- Root examples and dependency trim probes pass on Windows and WSL. The
  client-only TLS example's no-argument CI invocation prints usage only; actual
  handshake/PONG runs are separately exercised by the real-service supervisor.
  Trim initially found seven undeclared header guards for the registered crypto
  backends; the TLS header now explicitly requires all declared backends and
  avoids importing native TLS headers when this leaf is unselected. The test RSA
  identity signer is in xredis_tls_tests/workload dependencies, not production.
- Independent static/shared package consumers pass on both systems, with one
  separately owned Core (133 sources) and xredis (six sources). Public inventory
  is 41 functions / 114 constants / 36 types with missing=0. PE/ELF exports match
  all 41 public functions exactly and expose no private handoff helpers. Normal
  maturity and 33 targeted asset/API/layout/performance tool regressions pass;
  strict release still exits 1 for the developing root. Existing unrelated broad
  repository failures recorded in the preceding stage were not relabeled green.
- Real Redis 8.0.5 and OpenSSL 3.5.5 on WSL pass TLS 1.2 and 1.3 separately with
  fresh CA/server certificates and a separate wrong CA. Both static and shared
  consumers run six no-auth/password/ACL cases with strict RESP3, RESP2/AUTO,
  actual name/database, binary SET/GET, pipeline and initialization failures.
  Wrong name/wrong trust are rejected before business data. The actual example
  also passes both versions. Explicit per-version policies restrict ciphers and
  signatures consistently; initial fixture context construction rejected a
  mismatched default multi-version preference list, which was corrected in the
  test policy. Service/key files are ephemeral and cleaned; reports retain only
  public certificate hashes, binary/source/helper identity and expanded arguments.
  These are server-authenticated AES-128-GCM/RSA-PSS interoperability cases, not
  a complete certificate/cipher/group/mTLS/platform matrix.
- Numeric/Dial leaves still forbid TLS symbols. The TLS leaf excludes HTTP,
  WebSocket, task executors, an extra native TlsDial stack and the fixture RSA
  identity constructor. Current text/data/bss/file bytes: Windows numeric
  366860/2384/11136/526913, Dial 420308/2384/11136/606758, TLS
  1066664/2384/11168/1408675; WSL numeric 380901/4496/10908/680768, Dial
  437482/5200/10908/786496, TLS 1093721/6436/10940/1792344. These are current
  closure/symbol checks, not proof of unchanged plain-path runtime costs.
  Size source fingerprint:
  `1606b7adc0d57e63158ff9233b7657bc1246f6113dbb07d6c3f88de0b674113d`.
- Four TLS profiles reuse the actual absolute-arrival ECHO model, including all
  offer/admission/terminal partitions, planned-time success/all latency, lag,
  binary IDs, deadline misses, watcher retirement and resource counters. TLS 1.3
  AES-128-GCM/X25519 fixture trust is explicit. Both platform ordinary and stats
  smokes finish 256/256 successfully at requested 5000/s; controlled stall retains
  all 32 expired offers, and held replies partition all 64 into eight timeouts
  plus 56 harness-capacity rejections. Separate direct runs verify the full raw
  partitions/counters. Allocation and CPU windows include local fixture/crypto
  teardown; handshake/warmup precede measurement. These are unpinned sparse
  diagnostics, not tail-latency or throughput acceptance. Native crypto copies,
  conditional branches/cursors, native/shared references, metadata scans/copies,
  descriptor/session allocations and automatic numeric-probe diagnostics remain
  real costs; exact matched per-operation and cold-path attribution is pending.
  Final production/config fingerprint:
  `f2291b4ed64cc61281148e2d0deb03063c21bc3fc3aaddb231aec694ec6b0d9a`;
  four-profile workload fingerprint:
  `acca60bd820399b1c5198bbeacd43023b3689649d8e8f993493b0ca783cc1f77`.
- Windows direct process snapshots retain a cold +19 handle change. A separate
  same-process probe observes 72 -> 91 on the first TLS lifecycle and 91 -> 91
  on seven repeats. A second probe observes 72 -> 82 from secure RNG alone, then
  82 -> 91 on the first TLS lifecycle and a stable 91 baseline; one immediate
  snapshot briefly sees 92 and returns to 91 before the next lifecycle. Thus
  first-use retained OS resources and transient effects are visible, not silently
  subtracted or declared zero. Ten handles correlate with RNG startup; the other
  increment is not fully attributed. WSL direct runs retain descriptors 4 -> 4
  (or inherited 5 -> 5). Short plateau evidence does not replace long-run resource
  acceptance or prove absence of leaks.

The [TLS contract](../../extlibs/xredis/docs/tls.md) records API semantics,
costs, runnable example, native default-verifier interoperability and measurement
scope. Ignored evidence uses `redis-tls-*` under `out/service-extensions/evidence/`
and direct binaries/probes one directory above. Old benchmark fingerprints are
historical: helper bytes changed and the new TLS workload declares its complete
fixture input list. Remote CI execution is unclaimed. Mutual TLS, ticket/cache
management, wider TLS validation, reconnect/pools/dedicated modes/HA/tracking,
RPC/MQTT/Cron runtimes, pinned matched performance and native 72-hour mixed/fault
acceptance all remain required; the four-product goal stays active.


## P2 core prerequisite: certificate requests without a client identity (2026-10-29)

The xredis mutual-TLS prototype reproduced a real native-client gap with an
OpenSSL server: TLS 1.2 rejected CertificateRequest as a nonempty
ServerHelloDone, and TLS 1.3 expected Certificate directly after
EncryptedExtensions. This also broke servers with optional client certificates.

The core now validates one initial request and sends an empty chain when no
identity is configured. TLS 1.2 orders Certificate before ClientKeyExchange;
TLS 1.3 coalesces it with Finished under the handshake epoch. Full-response
preflight, transcript/EMS/application-secret/resumption-master ordering,
malformed/duplicate/early/late requests, fragmentation and output-OOM behavior
are verified, including exact missing-extension/illegal-parameter/decode-error/
unexpected-message fatal alerts and unchanged primary diagnostics. Client signing, identity selection and post-handshake authentication
are not supplied by this prerequisite. Required-certificate servers correctly
refuse the connection, including a real fatal alert after TLS 1.3 local READY.

During regression the unchanged HEAD `test_tls_server12` also failed its final
255-byte ALPN connection: a previous rejected ticket operation left a thread
error that was mistaken for a selector failure. `3bfe34a0` independently fixes
that error scope without discarding newly raised callback errors or temporary
identity ownership. Its focused regression retains all 64 OOM prefixes.

Final empty-certificate validation:

- Windows GCC O2 and WSL GCC O2 ASan/UBSan each pass the combined client/server
  suite: 11 modular plus six single-header programs. The previously failing
  255-byte ALPN scenario now passes as part of the complete suite.
- Windows Python/OpenSSL 3.0.16 and WSL Python/OpenSSL 3.5.5 each pass 14 local
  independent-peer cases: TLS 1.2/1.3, AES-128-GCM, AES-256-GCM/SHA-384,
  ChaCha20-Poly1305, optional success with binary data/ALPN/close, required
  certificate refusal and received fatal alerts, and TLS 1.3 HRR.
- Windows and WSL ASan/UBSan each also pass both modular programs in the
  existing `xredis_tls_tests` slice against the final core. This is regression
  evidence for the existing transport, not completed mutual-TLS integration.
- Source/fixture/binary identities are recorded by the new opt-in runner.
  Both final reports have source fingerprint
  `9077604b844941a508d25cd55031dd934f07d8032f5ea17854bb1d4a5344c130`.
- Windows/WSL x64 layout probes compare the same old/new closures:
  `tls_client_resume` remains 728 bytes, production `xredis_tls` remains 1240,
  and public `xtlsclientconfig` remains 88. The initial eight-byte private
  growth was measured and removed by using existing alignment padding.
  Requested flights grow by seven/eight wire bytes; existing native capacity,
  allocation/size-class and transcript-scan/hash costs still apply. No pinned
  performance or zero-overhead acceptance is claimed.
- The focused TLS API detail checker still reports the exact same 77 problems
  as the committed baseline, chiefly missing sections for older owned/ownership
  APIs. There are no new public functions in this prerequisite. These unrelated
  documentation gaps are recorded, not reported as a green check.

The [feature contract](tls-client-certificate-request.md) records the standards,
limits, costs and runnable independent-peer command. Ignored evidence uses
`tls-empty-cert-*`, plus `tls-selector-error-*` for the separate fix, under
`out/service-extensions/evidence/`. Older performance fingerprints remain
historical. Native Linux long runs, full mutual TLS/signing and ticket handling,
all unfinished protocol runtimes/modes and final acceptance remain required;
the four-product goal stays active.

## P2 core prerequisite: bounded client certificate signing (2026-10-29)

`tls_client_auth` now supplies an explicit V1 full-handshake factory with a
retained immutable local identity, bounded chain/raw-transcript metadata and a
query distinguishing local proof queueing from remote acceptance. It preserves
the old public client configuration ABI. Initial TLS 1.2 and TLS 1.3 certificate
selection/signing covers the existing RSA/P-256/P-384/Ed25519 backends, CA and
signature hints, recognized OID conditions, record fragmentation, transcript/
EMS ordering and atomic output admission. No private-key operation is retried
after a signed-flight failure. Explicit resumption is refused and new tickets
are not retained until local-identity binding is implemented. Native server
client-authentication and post-handshake authentication are outside this slice.

Both Windows O2 and WSL O2 ASan/UBSan pass 13 modular and eight single-header
client/server/auth programs. The auth programs sweep 384 allocation prefixes
across construction and the two flights, checking every hit rollback plus
success past allocation tails. Real RSA proofs, both Finished transcripts,
backpressure/sign counts, callback diagnostics, retained identity cleanup,
CA/OID choices and invalid server Finished are exercised. Independently rebuilt
Python/OpenSSL libraries on each platform pass 34 authentication cases and 60
legacy-role/empty-certificate regressions. Their common source/fixture/helper
input fingerprint is
`893cf6c40f236b4875ef3c6071b613a396eee60a8a57dc7752699b090d1c9dd3`.

The selected private state grows by eight bytes; unselected closures are
unchanged (`tls_client_resume` 728, production `xredis_tls` 1240, public client
config 88). Windows and WSL logical-allocation probes measure one extra factory
allocation and a net +472 retained bytes for the single-leaf example in the
already-selected test closure: +504 auth metadata minus 32 disabled ticket-slot
bytes. Destruction restores those logical allocations. This is a bounded cold
allocation window, not complete handshake/OS-resource or performance acceptance.
Raw copies, scans, hashes, calls, crypto/backend temporaries and reference costs
remain explicit in the [feature contract](tls-client-auth.md).

The factory example and independent peer command are runnable. Three APIs,
one type and eight constants have complete symbol coverage; the detailed TLS
checker has the exact same 77 pre-existing failures. Generated extension
declaration restore guards are synchronized for the new core feature.
Windows and WSL static/shared packages pass stock and new-API consumers;
both factory examples and both existing Redis TLS modular programs pass
(WSL O2 ASan/UBSan). Trim and full generated consistency also pass.
Ignored evidence uses `tls-auth-*` and `layout-auth/` under
`out/service-extensions/`. Redis mutual-TLS integration, identity-bound tickets,
remaining Redis modes, RPC/MQTT/Cron runtimes and all final production/performance
acceptance remain required. The four-product goal stays active.

## P2 xredis: retained TLS client authentication (2026-10-29)

`xredis_tls_auth` adds three explicit V1 factories for hostname DNS, prepared
endpoints and a numeric address. Each passes native client-auth configuration
directly into the existing TLS connection plan and retains the immutable
identity synchronously before DNS/worker callbacks. Caller configurations,
views and identity references may be retired after admission. The default
requires a compatible certificate request; an explicit false permits no proof.
Managed Redis readiness still requires initialization replies from the peer.
No cleartext downgrade, private-sign retry, business replay or unbound TLS
resumption is added. The root product and auth module remain `developing`.

The existing command/TLS stream path and Redis client layout are shared. The
cold private factory adds a nullable auth argument and selects the native
factory once; native metadata, cryptography, references, selected-state growth
and TLS 1.2 raw-transcript costs remain counted by the core contract. This is
not a claim of zero cold-path overhead or completed performance acceptance.

Verified feature evidence:

- Windows GCC O2 and WSL GCC O2 ASan/UBSan each pass ten modular and ten
  single-header Redis programs. The new slice covers all three factories,
  refused-address fallback, caller-config recycling, blocked-DNS close/deadline/
  early caller release, strict no-request failure, server verifier failure,
  held TLS deadline, binary 32-command pipelines and per-turn work fragmentation.
  Forty allocation-budget windows include rejection and admitted survivors;
  every budget returns to zero live allocations/bytes and signer cleanup is
  exact. Invalid/overflow/overlapping descriptors leave output unchanged.
- Each platform passes 24 real Redis 8.0.5 initial-client-auth cases and two
  runnable RSA examples: TLS 1.2/1.3; RSA, P-256, P-384 and Ed25519; no password,
  password and ACL credentials; RESP2/RESP3/auto; binary SET/GET and pipelines.
  Wrong server name/trust invokes no private signer. Missing/untrusted/corrupt
  local proofs and signer-reported OOM cannot publish managed readiness or
  send queued business. Successful proofs and Redis initialization rejections
  sign exactly once. All retained forwarding identities retire after join.
- Real-peer reports bind the actual compiled platform closure, all public and
  private core/extension headers, helpers, compiler and client bytes. Inputs
  are checked before and after execution. Windows reports name the native
  Windows compiler; IOCP inputs are not compared to a WSL/epoll closure. An
  old success is invalidated before a new run starts. Exact input fingerprints:
  Windows `928045bb41285cbfa3ac88fb02ff5bcd678e28b848747ec803f9373bd71e7ebc`;
  WSL `50cb7cb59841b36dcce62988187d4fabee79f4c5ee659f6facc37292726520c7`.
- Windows uses a native executable and independently verified native Python TLS
  startup probe against a disposable WSL peer. A Linux-only PING cannot prove
  the Windows forwarding endpoint ready. This is local Windows-client/WSL
  interoperability, not native Linux acceptance.
- Minimal static/shared packages and all three new API consumers pass on both
  platforms. Separate Dial/TLS/Auth size profiles enforce forbidden-symbol
  boundaries; auth adds 1508 Windows / 1468 WSL product text bytes over the
  current TLS profile. Private-key constructors, process launchers and unrelated
  HTTP/WebSocket/task APIs stay outside the generic auth package.
- The C cold benchmark completes 100 new full TLS 1.3 authenticated connection/
  HELLO/PING/physical-close cycles on each platform. An independent OpenSSL
  fixture verifies all 100 client proofs. Import/fixture/engine startup is
  excluded; peer work and first-cycle effects remain inside client cycle time.
  These are single unpinned diagnostic repeats with no warmup, not matched
  throughput or tail-latency acceptance. The common implementation fingerprint
  is `e85aaaefbbcd35df8b51b9607ad62f89b9b0117aba20c99c703174ec8712b24c`;
  workload fingerprint is
  `c9234947d911f3552549c4c450a683615e3c19a51e3a4fcb4a8eb8d2b89cd3a7`.
- The generated headers, trim checks and 44-function/115-constant/36-type API
  symbol coverage pass. Normal maturity passes with 18 benchmarks and seven
  size profiles; strict release correctly rejects the two developing modules.

One earlier native-Windows/WSL-peer invocation failed the old generic readiness
assertion. Its typed status/cause was not recorded. Subsequent full diagnostic
matrices pass, and the runner now checks the native forwarding endpoint, but
that does not establish the cause of the earlier failure. It remains an open
acceptance observation rather than a claimed runtime fix.

The [feature guide](../../extlibs/xredis/docs/tls-auth.md) documents API ownership,
limits, callback execution, examples, exact receipts and measurement boundaries.
Ignored evidence uses `redis-mtls-*` and `auth-draft/package-*` under
`out/service-extensions/`. Identity-bound TLS tickets, other unfinished Redis
modes, RPC/MQTT/Cron runtimes, pinned matched performance and native 72-hour
mixed/fault acceptance remain required. The four-product goal stays active.

## P2 xredis: staged atomic transactions (2026-10-29)

`xredis_transaction` supplies bounded async/sync MULTI/queue/EXEC operations on
managed connections. It first waits for all preceding requests, then validates
MULTI before sending business commands. Queue errors suppress EXEC and drain
DISCARD; other calls cannot enter that server transaction. Unknown or malformed
control state retires the connection. Recognized EXECABORT/null EXEC establish
abort; ordered EXEC arrays retain runtime error children without inventing a
rollback. Pre-EXEC abandonment suppresses commit, while accepted EXEC bytes
produce explicit uncertain execution until a validated terminal response.
Logical cancellation does not release the physical FIFO/cleanup obligation;
the existing drain grace, byte caps, parser caps and observable Close still apply.

Three APIs, four types and bounded configuration have complete public symbol
coverage (47 functions, 122 constants, 40 types across xredis). Managed ordinary
calls reserve transaction/reset controls. Raw clients keep their manual
one-reply contract and cannot use the managed transaction API. Interactive
WATCH/read/decision sessions and lease integration remain pending; the whole
transaction family, auth family and product remain developing.

Both Windows GCC O2 and WSL GCC O2 ASan/UBSan pass 11 modular and 11 single-header
programs. The transaction program exercises binary input recycling, preceding/
following ordinary requests, empty transactions, fragmented TLS, RESP3 pushes,
queue/runtime/control/protocol errors, peer drops, typed execution certainty,
before/after-commit cancellation, queue-inclusive timeout, bounded stalled cleanup,
sync rejection from another engine worker, four concurrent producers/32 commits,
169 admission allocation-budget windows and worker OOM while queue/EXEC/DISCARD
replies are held. Physical byte/request quotas return to zero, and retained
outcomes survive engine destruction. Examples and declaration-only headers pass.

Each platform passes six independent real Redis 8.0.5 cases: TLS 1.2/1.3 with
RSA client authentication and no-password/password/ACL configurations, each
testing RESP2/RESP3 binary EXEC arrays, rejected queue/discard, runtime errors
without rollback, denied MULTI without escaped SET, and denied EXEC. An independent
socket oracle established Redis 8.0.5's denied EXEC response as EXECABORT. The
client recognizes that abort and permits reuse; a separate dirty-state fixture
checks conservative retirement for an unknown EXEC error. Windows uses a native
client and independently verified native loopback startup against a WSL peer.
Final input fingerprints: Windows `76a8cc20b2cd211231835efec4ad234dc8223153f3895b14995a1ea871b5aa0c`;
WSL `74824a5a603b06b7d704bbf4a4647555718ba39ad3f5971f50485c6e702d243d`. Receipts pin actual platform sources,
all public/private headers, helpers, compiler and binary before/after execution.

The optional request pointer's initial eight-byte growth was measured and
removed. A tag uses existing tail padding: ordinary request allocation size
remains 104 bytes with or without selection on both x64 platforms (wire offset
102/103). Client size remains Windows 848 / WSL 920 in the same TLS/auth closures;
public client config remains 232 and ordinary result 24. Transactions add one
72-byte aligned state plus at most seven padding bytes and a 64-byte typed
result, with no per-command Future/offset array. Wire/control/state storage is
charged to existing byte quotas. Copies, control round trips, frame allocations,
tag checks, state-address calculation, references, synchronization and timers
remain real costs; no complete zero-overhead/performance claim is made.

Minimal static/shared consumers now execute actual binary transactions and
all three APIs. This exposed a packaging verification gap: consumers did not
receive selected feature definitions and the old Redis consumer forced the
product root. `99b5bec1` separately forwards actual selection for GNU/MSVC
static/shared consumers (17 tool tests plus a native minimal command proof).
The Redis consumer now tests only shipped interfaces, rejects an empty selection
and exercises actual transactions. This does not certify a native MSVC build.
Separate client/transaction size profiles enforce forbidden TLS/DNS/HTTP/
WebSocket/task/process symbols and separate core/product ownership.

The full-load and smoke measurements retain raw/transaction-selected ordinary
pipelines and staged two-command transactions. Runs are unpinned; earlier
smoke/full measurements exceeded the unchanged quality thresholds. Keep all
quality failures, and do not infer an accepted speedup/regression from them.
Pinned matched throughput/tail-latency, complete copy/allocation/syscall accounting,
native Linux and 72-hour mixed/fault acceptance remain required. Normal maturity
has 20 workloads/eight size profiles; strict release must reject the three
developing modules. Generated/API/trim checks and minimal package/example
consumers are separate feature evidence, not full production acceptance.

See the [transaction contract](../../extlibs/xredis/docs/transaction.md).
Ignored evidence and source/fixture/binary identities are under
`out/service-extensions/transaction/`. The earlier unclassified Windows/WSL
readiness observation remains open. Remaining Redis modes/pools/HA/tracking,
RPC/MQTT/Cron runtimes and the original final acceptance remain required.
The four-product goal stays active.

## P2 xredis: exclusive WATCH/read/decision scopes (2026-10-29)

`xredis_watch` adds an exclusive, finite-lifetime session on an OPEN, ready,
physically idle managed client. It validates WATCH before exposing commands,
allows binary scoped reads/commands, then seals exactly one admitted staged
commit. Native admission checks ownership both before construction and at
publication, so an encoder racing a commit/abort cannot insert a late request.
Unowned calls/transactions and a second session cannot enter the server state.
All eleven APIs and four types are registered; the full public index now has
58 functions, 123 constants/macros and 44 types. The module, transaction family,
auth family and product remain developing; pooling/lease integration and
repeated WATCH key extension are still pending.

Every accepted start outcome, including failure/cancel/OOM, exposes a borrowed
session handle for its physical Closed barrier. Only OK permits commands.
One monotonic deadline covers start encoding, WATCH, application think time,
reads and commit; calls cannot renew it. Idle abandonment requires validated
UNWATCH, while a busy abandoned stream may close. EXEC/null/known EXECABORT and
completed queue-error DISCARD are clearing boundaries; rejected MULTI still
requires UNWATCH. Unknown execution/failed or malformed cleanup retires the
connection. Scoped request constructors, wire/timer/cancel tails and notification
registrations retire before claim release. An independent engine Pin protects
session work after external ClientClose. Completed retained metadata does not
pin the client/engine. No new shared runtime, executor or thread was introduced.

Both Windows GCC O2 and WSL GCC O2 ASan/UBSan pass 12 modular plus 12 single-header
programs (48 total). New checks include binary input reuse and scoped read/commit
ordering; conflict/null EXEC and MULTI/queue/runtime/opaque errors; malformed,
rejected and held UNWATCH; pending start cancel; idle/busy total expiry; cancelled
EXEC uncertainty; external close; reserved controls; raw/busy admission and
output/storage aliases; cross-worker sync refusal; dropped pending/completed
observers; 169 constructor budget windows (1..16384 step 97); worker quota=1 at
WATCH/UNWATCH/EXEC responses; and 16 rounds of eight-thread call/commit races.
The initial repeated-Abort implementation incorrectly treated the cleanup
request as abandoned business; its regression now validates idempotent cleanup.
The first strict zero-budget assertion ran before the final completion/post
return tail, reproducing on both Windows and WSL. The assertion now waits a
bounded ten seconds for real allocation counts/bytes to reach zero after
observers are dropped; it does not alter production ownership or counters.
The original failed logs remain, separate from final passing logs.

Each platform also passes six independent Redis 8.0.5 cases: TLS 1.2/1.3,
RSA client certificates and no-password/password/ACL authentication, with
RESP2 and RESP3 within every case. Two authenticated clients prove a real
conflicting write yields null EXEC/ABORTED without replay, successful binary
GET/EXEC, idle abort/expiry reuse, and ACL-denied UNWATCH followed by physical
retirement. The preceding atomic-transaction oracle also runs in each case.
Windows is a native client against a WSL peer, with independent native Windows
TLS startup health, not native Linux acceptance. These new passing matrices
do not diagnose the earlier generic peer-startup observation. Exact final
input fingerprints: Windows `57bb6d45aa9ef84f19c1c1ded59f4f0658883052d13507f43c98f5cf38e92952`;
WSL `4c4ea44369c31d3af7f59f1d8c3a923c0369d1398f44db094e833ae68468e9da`. Receipts pin and revalidate actual
platform source/header/helper/compiler/binary inputs before and after execution.

Matched layout probes keep ordinary requests at 104 bytes, wire offset 103
with transaction/WATCH selected, public client config 232 and ordinary result
24. WATCH selection adds one private client owner pointer: Windows 848 to 856,
WSL 920 to 928 in otherwise identical TLS/auth closures. A live scope occupies
Windows 608 / WSL 680 bytes, plus a 32-byte start result, 8-byte Closed result,
Future/cancellation/timer resources and native requests. Admission adds owner/
state checks; idle transitions retain/wake an owner when present. Scope calls
also use a short session gate and completion notifications. Descriptor sizing,
wire encoding/copies, reference operations, synchronization, allocation and
round trips remain real costs. There is no zero-overhead claim.

On each platform, minimal separate static/shared consumers execute ordinary
requests, binary atomic transactions and WATCH/read/commit/physical retirement.
Examples, declaration-only syntax, feature/API generation and trim checks pass.
The WATCH size profile covers single/static/shared artifacts and excludes
optional DNS/TLS/signer/process/HTTP/WebSocket/task symbols. Normal maturity
passes with 22 workloads and nine size profiles; strict release correctly
rejects the four developing modules. This does not establish native MSVC
build/ABI acceptance or native Linux deployment acceptance.

Performance runs use the original raw pipeline, the same raw pipeline with
transaction selected, the same pipeline with WATCH selected, and the complete
WATCH/read/two-command EXEC/physical-Closed/observer-release cycle. Both platforms
use five full repeats plus one warmup; all five scope runs finish 1000 cycles.
The unchanged 3% relative MAD / 6% central-range gates reject the unpinned runs:
Windows 16 quality failures, WSL eight. Preserve all measurements, do not select
single repetitions or claim an accepted overhead/speedup/tail-latency result.
Both source fingerprints are `a3c725798c595a3e6d0bdad11ddf949df3a05bb64fa46ed8dfcff5291e9b416c`;
workload fingerprint `4eb3b84cc850826953d05344e8c0edb740d2c4a80b85dcc832382e6eed3be096`.
Pinned matched native Windows/Linux performance, independent recovery/fault
coverage, the final >=72-hour mixed/fault run and all remaining product scope
are still required. The long-running four-library goal is not complete.

Local receipts, current binary hashes, layouts, raw passing/failing logs,
performance quality failures and `validation.json` are under ignored
`out/service-extensions/watch/`. See `extlibs/xredis/docs/watch.md` and its
self-contained example for the precise contract and reproducible commands.

## P2 xredis: bounded exclusive lease pool core (2026-10-29)

`xredis_pool` adds a fixed-membership lease core over existing ready managed
TCP/DNS/TLS clients, without a new shared runtime or thread. It accepts 1..65536
distinct, physically idle clients on one engine with one negotiated RESP version.
The caller supplies the intended common service/credentials/database/session
profile; erased credentials/arbitrary server-side state cannot be inferred.
Construction sorts a temporary pointer copy, claims members, and rolls back all
partial claims on failure. Sixteen APIs and seven types are registered (74
functions, 127 constants/macros, 51 types across the public product). Automatic
creation/repair/reconnect, pooled transaction/WATCH handoff, HA/routing and final
production/performance acceptance remain pending; the module stays developing.
This is an actual pool core, not a declaration that the final pool scope is done.

Acquisition is bounded FIFO by accepted enqueue order. Its original monotonic
deadline includes constructor and queue time. Grants have a separate finite
maximum hold, not renewed by calls. Queued cancel/expiry produces immutable typed
outcomes without Redis commands; all-dead membership reports DISCONNECTED.
Native leased calls reuse the existing Future, immutable wire and FIFO directly,
with no second per-call Future/encoding buffer/request pointer. Hard aggregate
request/command-byte quotas reserve before construction and release only after
physical storage free, including failed construction, sent abandonment and
wire/timer/cancel tails. Logical completion alone cannot release quota.

Return seals calls and drains within grace and the original hold deadline;
busy expiry/Abort retires a connection, while clean idle expiry can reuse it.
API constructors retire before a slot changes generation; old retained handles
cannot reach new leases. PoolClose atomically closes native quota-group admission
before bounded worker traversal. Client-close watches, coalesced idle/events,
waiter/idle FIFO lists and bounded work turns avoid full membership scans on each
request. Completed retained metadata pins no pool/client/engine. Dropped lease
observers recover through finite expiry. No sent operation is replayed.

Both Windows GCC O2 and WSL GCC O2 ASan/UBSan pass 13 modular and 13 single-header
programs (52 total). New tests cover binary native requests, two-/four-member
ownership, queue order/admission/cancel/expiry/zero deadline, aggregate count/byte
limits remaining charged after sent cancellation, graceful busy drain and
subsequent FIFO grant, return/hold expiry, idle reuse versus busy retirement,
forced/external/pool close, generation/retained/dropped-observer lifetimes,
invalid/control/output aliases, partial rollback, mixed-protocol/engine rejection,
cross-worker sync refusal and eight producers (128 leases, 512 pipelined calls).
Budget windows cover 65 pool constructors, 169 acquisitions and 67 native calls;
all counts/bytes return to zero after their actual callback tails. Two cold
worker quota=1 cases cover queue/hold timer allocation failure. An initial OOM
fixture unexpectedly obtained a recycled timer; the test now uses public
NodeCacheBytes=0 to force allocation. Production/example/benchmark/independent
peers retain normal caching. Original failed setup/OOM-fixture logs remain.

Each platform passes six independent Redis 8.0.5 cases (TLS 1.2/1.3, RSA client
certificates, no-password/password/ACL auth), each with RESP2 and RESP3. Two
pooled members and a separate admin prove binary calls, queued cancellation,
server-confirmed blocked BLPOP (CLIENT LIST ID flags), aggregate quota still
held after local cancellation, LPUSH-unblocked physical drain, busy Return/FIFO
handoff and rejection of old lease generations. Atomic-transaction and WATCH
oracles also run in every case. Windows is a native client against a WSL peer
with native Windows TLS startup health, not native Linux acceptance. These passes
do not diagnose the previously recorded generic startup observation. Final
input fingerprints: Windows `10b46ff954a0bf7527927fd407a72e42a040a10cd3a7602aa7cc42fa5a32f8bc`;
WSL `26f6fffd335082c1e5b05747fa75ceda496b456dfd03c570b664dfc0f8e770ca`. Exact platform sources, all relevant
headers/helpers, compiler and binary are revalidated before/after execution.

Both platforms pass separate minimal static/shared consumers that execute real
pool acquisition/native binary call/physical return, self-contained examples,
declaration-only syntax and trim. Single/static/shared size profiles enforce
excluded optional DNS/TLS/signer/process/HTTP/WebSocket/task symbols. Normal
maturity passes with 26 workloads/10 size profiles; strict release rejects the
five developing modules. Native MSVC and native Linux deployment acceptance
are not established by these GCC/WSL checks.

Matched x64 layouts keep request allocation 104 bytes, client config 232 and
request outcome 24. Pool or WATCH or both share the same eight-byte client owner
pointer: Windows 848 to 856, WSL 920 to 928 in identical TLS/auth closures.
Pool fixed prefix is 440/512 bytes (Windows/WSL), each member slot 184, lease
metadata 200/272, embedded quota group 40 (included in prefix). Both-feature
WATCH scope is 616/688, eight bytes above standalone WATCH due to its optional
quota pointer. Public layouts are feature-independent. These omit allocator,
Future/result/timer storage, temporary sorting and native connection resources.
Per-call lease gates, owner/state/profile checks, shared quota CAS/decrements,
notifications, allocation/copy/synchronization and cold construction remain real
costs. Active calls avoid an unnecessary pool post/reference on ordinary API
return; physical idle/seal/close notifications still do real bookkeeping.

Performance retains original raw, pool-selected raw, identical compiled managed
versus held-lease pipelines (100000 requests/depth16) and complete warm checkout/
PING/physical Return/observer-release cycles (1000 each). Pool/TCP construction
is outside those timed regions and remains a separate cold cost. Five full
repeats plus one warmup run on each platform, with all five cycle samples 1000.
Unchanged 3% relative MAD / 6% central-range gates report Windows
32 quality failures and WSL
18. Preserve every measurement; no
unpinned/local C-peer result constitutes an accepted overhead/speedup or
independent performance parity. Source fingerprint:
`370306060e4e92837018ce48f7f83795f6cb7fc350d740cf09f2d365f9d4aaf3`; workload fingerprint:
`d5b048cddbed367b0abce91ec1e747ae6df0a1d54d14eaba85034043a7089b87`. Pinned matched native Windows/Linux,
Hiredis, multiconnection/worker/fixed-arrival sweeps, complete cold/steady cost
attribution and the final >=72-hour mixed/fault runs remain required. The complete
four-library objective is still active and unachieved.

Ignored `out/service-extensions/pool/` holds exact receipts, 52 current binary
hashes, raw passing/failing logs, layouts, packages/size/performance reports and
`validation.json`. `extlibs/xredis/docs/pool.md` describes limits, ownership,
actual costs, remaining scope and runnable commands.

## P2 xredis: native pooled transactions and nested WATCH (2026-10-29)

`xredis_pool_transaction` supplies direct native staged transaction Futures on
leases. Accepted native enqueue seals the lease; preceding calls drain before
MULTI, queue errors suppress EXEC and complete DISCARD, runtime error children
remain ordered, known/null abort differs from uncertain execution and no replay
occurs. Native wire/control/aligned-state bytes retain the pool's aggregate
physical quota. Deadline is capped at the original lease hold. Explicit lease
Return/Closed preserves physical request obligations rather than treating a
logical Future as a slot boundary. No per-call adapter Future, wire clone or
per-request lease pointer is introduced.

`xredis_pool_watch` starts a native scope on a physically idle lease, with bounded
busy refusal and success-only input/output ownership. Validation/OOM/admission
failure restores an unused lease without reopening a concurrently returned or
closed parent. Acceptance consumes/seals the lease. Scope/read/transaction/
UNWATCH inherit quota and the original hold deadline. A live nested owner,
including a zero-request application think interval, cannot be returned. Return
requests bounded abandonment; grace/hold expiry, Abort and PoolClose can retire
a dirty/busy stream. Parent publication waits for child physical Closed.

The private handoff replaces/restores ownership under the native client gate,
retains child data before abandonment outside that gate, and uses a parent
retirement marker. Merely restoring the parent pointer is insufficient: marker
release/wake occurs only after the child's source notifications, native client
reference, independent engine Pin and Closed publication retire. Different
client/pool workers cannot publish lease/pool closure before that point. Retained
completed scope/lease metadata never dereferences a freed pool. This uses only
the existing engine/workers, posts, timers/Futures and extension-private helpers;
no new shared runtime/thread or XRT API implementation was required.

Four APIs are registered (78 functions, 129 constants/macros, 51 public types).
The integrations and their incomplete product dependencies remain developing;
automatic repair/reconnect, repeated WATCH key extension, HA/dedicated modes and
final production/performance acceptance are still pending. No narrower definition
of the four-library goal is adopted.

Windows GCC O2 and WSL GCC O2 ASan/UBSan each pass 14 modular plus 14 single-header
programs (56 total). New checks cover plain/TLS binary transactions and nested
reads/commit/abort, predecessor ordering, seal/rejected duplicate output, staged/
queue/runtime/malformed/opaque/null/drop failures, busy WATCH refusal, metadata
byte caps, nested physical quota retained after local cancellation, malformed/
rejected/held UNWATCH, explicit parent-before-child observation across workers,
parent grace/hold and scope deadlines, pending start cancellation, forced parent/
pool close, cancelled EXEC uncertainty, 146 admission/OOM rollback windows,
retained metadata after complete engine teardown, cross-worker sync refusal and
20 four-thread WATCH/transaction/call/Return races. Parent Closed must observe
child Closed already resolved. Existing full codec/client/TLS/pool suites also
run; positive integration tests do not substitute for them.

Each platform passes six independent Redis 8.0.5 cases: TLS 1.2/1.3, RSA client
certificates, no-password/password/ACL auth, RESP2/RESP3 within every case. Real
leased SET/GET EXEC, nested reads/conflicting writes, scope-before-lease closure,
reacquisition of the same physical member followed by an external modification
and plain EXEC (no inherited WATCH), and ACL-denied UNWATCH retirement pass.
Earlier transaction/WATCH/pool/blocking oracles also run. Windows is a native
client against a WSL peer with native Windows TLS startup health, not native
Linux acceptance. These passes do not diagnose the prior generic startup
observation. Exact input fingerprints: Windows
`0f3cb409a0614c0b13fab6b0a3fef95de47b9b250acb8417b8590ab3c56272d4`; WSL
`0f8d45719a91897acdc20bc0633f26d3d4505f33422c01eed2128a8c049d9250`. Receipts revalidate actual platform
sources, all relevant headers/helpers, compiler and binary before/after execution.

Separate minimal static/shared consumers exercise each optional integration,
not a forced product root; both examples, declaration-only syntax, trim and
single/static/shared size profiles pass. Transaction-only selection excludes
WATCH and optional DNS/TLS/HTTP/task/process symbols; nested selection excludes
those unrelated runtimes. Normal maturity has 28 workloads and 12 size profiles;
strict release correctly refuses seven developing modules. Native MSVC and
native Linux deployment acceptance remain unproven by these GCC/WSL checks.

Measured x64 private storage keeps ordinary requests at 104 bytes, client config
232 and result 24. Client remains 856 bytes Windows / 928 WSL (one shared owner
pointer). Optional parent/abandonment/atomic retirement/alignment add 24 bytes to
the owner: slots become 208, combined nested WATCH scopes 640/712. Pool prefix
440/512, lease 200/272 and quota group 40 remain unchanged. Standalone selections
omit nested fields. Public layouts are feature-independent. Gates, state/lineage
checks, native quota CAS/decrements, references, clocks, allocation/copy/encoding,
network/control round trips and cold parent handoff remain real costs. No
zero-overhead claim is made.

Performance runs retain native standalone two-command transactions/WATCH cycles
and full pooled transaction/WATCH cycles including acquire, child/parent physical
barriers and observer release; TCP/pool construction is outside timing. Both
platforms run five full repeats plus one warmup, all four workloads completing
1000 in every measured sample. Unchanged 3% relative MAD / 6% central-range
gates report Windows 5 and WSL
1 quality failures. Keep every sample;
unpinned local C-peer measurements do not establish accepted speedup/overhead,
tail latency or independent parity. Source fingerprint
`a1b9a2e94a0df30dd1e0b8d714799a40d1a4bdb685ffbc6ae3c75ab04b61feba`; workload fingerprint
`9939f9f13ec44123f0db33de404a22e5bc2359c3f12b708eae6d07dc510008f9`. Pinned matched native platform/
Hiredis/workload/cost sweeps and final >=72-hour mixed/fault acceptance remain
required; the complete four-product goal remains active and unachieved.

Ignored `out/service-extensions/pool-sessions/` contains exact receipts, 56 current
binary hashes, raw logs, layouts, packages/size/performance and `validation.json`.
See `extlibs/xredis/docs/pool-sessions.md` and its runnable examples for contracts,
limits and commands.

## P2 xredis: bounded automatic pool connections and repair (2026-10-29)

`xredis_pool_connect` adds asynchronous target-size construction and automatic
replacement to the existing lease pool. A serial nonblocking worker factory
starts existing managed numeric/DNS/TLS/auth clients, transfers one private
reference only on success and retains context until an exactly-once Drop before
PoolClosed. A private native claim protects candidates before initialization
exposes readiness. Expected RESP2/3, same-engine ownership and physical idle are
validated. New members retain the service/session profile supplied by the factory.
No command, transaction or WATCH operation is replayed across generations.

Candidate setup plus failure-close tails obey MaxConnecting. An indexed heap,
intrusive runnable queue, bounded worker turns and one timer cover setup and
exponential capped jittered retry. Due retries cannot hide candidate deadlines
when concurrency is full. Every old client/lease/nested scope/request/notification
physically retires before its slot starts a replacement. Acquisition deadlines
and cancellation include outage/backoff/setup wait without canceling shared
maintenance. Close stops new attempts, joins candidates/members/timers, invokes
context Drop outside gates, and releases engine ownership. Timer OOM fails closed
instead of retry-spinning. Factory reentrant close drains its transferred client.
No new shared runtime/thread or general XRT API change was needed.

Windows GCC O2 and WSL GCC O2 ASan/UBSan each pass 15 modular plus 15 single-header
programs (60 total). New checks cover transient factory failure/rate, hard
concurrency and delayed readiness, heap deadline fairness, timeout/cancel/close
during outage and long backoff, raw/RESP mismatch, server setup failure/drop,
RESP2 success, uncertain sent disconnection without replay, stale lease rejection,
normal/aborted nested WATCH, context physical/reentrant snapshot barriers,
13 invalid constructor bounds and 130 constructor/OOM windows with zero remaining
budget allocations, maintenance timer OOM, sync worker refusal, reentrant factory
close, and ten four-producer abort/refill/PoolClose races. Existing suites remain
in the full runs. A separate WSL O1 TSan modular run reports no race; GCC warns
that atomic_thread_fence is not modeled, retained as a limitation rather than a
complete race-freedom claim. Its executable is hashed separately.

Each platform also passes six Redis 8.0.5 cases (TLS 1.2/1.3, RSA client identity,
no-password/password/ACL, RESP2/3 inside each case). Automatic retry, server
CLIENT KILL/physical lease retirement, newly authenticated member IDs, binary
native transaction/state, nested WATCH abort and clean successor EXEC pass.
All previous transaction/WATCH/pool/blocking/session checks run too. Windows
uses the native client against a WSL peer with independent Windows TLS health;
WSL is not native Linux deployment acceptance. This does not diagnose the older
generic startup observation. Exact interop input fingerprints: Windows
`394b095a94bde4fb0d6a8eb0481b1293753d390a3292cf99a5d81b0d89db0e4c`; WSL
`82ffc33596dc839d49f4648f7deebb3c9d726328d7111d7a7cc1304ba7269ed9`.

Minimal static/shared consumers actually perform automatic replacement; examples,
trim, single/static/shared size profiles and declaration-only checks pass on
both platforms. Normal maturity is 23 modules, 14 implementation/test/single/docs
nodes, 31 performance workloads and 13 size profiles. Strict release correctly
rejects eight developing modules. Three public APIs bring the catalogue to
81 functions; actual counts of constants/types remain in the generated reference.
The product and optional connector remain developing.

Matched TLS/auth x64 layouts keep native requests 104, clients 856/928 Windows/WSL,
ordinary slots 184 (208 nested), and leases 200/272 bytes. Auto selection adds
8 bytes to the pool prefix (448/520), plus a 200-byte separately owned prefix,
128-byte entry and 8-byte heap cell per member. Config/info are 72/56. Fixed-only
selection omits auto code/storage. Native setup/Futures, allocator headers,
refs/pins, gate/atomic checks, clocks, heap/timer operations, user factory work
and native/TLS round trips remain real costs; no zero-overhead claim is made.

Three matched-source/feature workloads retain warm fixed and auto lease loops
with identical peer/input/workers/timestamps, plus physical Abort/retry/TCP/
HELLO/readiness/successor-return recovery. Pool startup is outside timing. Each
platform keeps five full 1000-cycle repeats plus one warmup for all three,
without dropping samples or changing 3% relative MAD / 6% central-range gates.
Windows reports 1, WSL
8 quality failures. Performance acceptance
remains false even for individual stable metrics: these are unpinned local peers,
not matched Hiredis/native multiworker/multiconnection/fixed-arrival/cost sweeps.
Source fingerprint `47ab5b05c97da821faadbd5af84c730c84b85c78c3715aa74d3ed3bb63078c2b`;
workload fingerprint `3da6b48276990e690ce4cb964649b9ef397b01b6dc7f8a59756b8434e6021579`.
With the configured 1 ms retry floor, repair-cycle P50 medians are
15565.000008792 us Windows and
1585.999999989 us WSL. The Windows ~15.6 ms behavior
requires native timer/transport profiling before attributing a cause or accepting
recovery latency. No general runtime change is inferred from this observation.

Ignored `out/service-extensions/pool-connect/` pins reports, 60 regression and
one TSan executable hashes, layouts, raw samples, package commands and validation.
See `extlibs/xredis/docs/pool-connect.md` for runnable examples, costs and limits.
Dedicated Redis modes, HA/tracking and remaining P3 runtimes for xrpc/xmqtt/xcron,
native Linux/MSVC where applicable and final >=72-hour mixed/fault and performance
acceptance remain pending. The original four-library goal remains active.

## P2 xredis: physical-idle prerequisite in the real-peer fixture (2026-10-30)

The IOCP timing investigation exposed a missing prerequisite in the automatic
pool interop fixture: after its synchronous GET Future completed, it immediately
started nested WATCH, although physical request tails can still be present.
The one-member fixture now awaits its existing aggregate physical counters at
zero before nesting, as the public contract requires. No production admission
rule is relaxed. Rejected WATCH admission also reports its numeric class.
The initial WSL admission failure remains in ignored
`out/service-extensions/windows-timer/wsl-interop-initial-failure.log`; both
native Windows-client/WSL-peer and WSL-client six-case TLS 1.2/1.3, credential
and RESP2/3 matrices pass after the correction. These runs also exercise the
pending IOCP candidate; they are not final production/performance acceptance.

## P2 base gap: precise IOCP wakeups and original deadlines (2026-10-30)

The previous ~15.6 ms Windows pool-repair result is reproduced outside Redis:
native GQCS, public port timeouts and AUTO engine timer chains show that scale,
while WSL public/engine waits are ~1.08 ms. A separately rebuilt reference uses
exact `d50664b2` IOCP source; reference/current pool executables share every
other source/header, workload, compiler option and worker budget. Source receipts
and binaries are pinned; a prototype label was overwritten during the first
diagnostic, so the separately rebuilt `reference-*` artifacts are authoritative.

The native IOCP backend now dynamically resolves the documented wait-completion
packet API and high-resolution waitable timer. It owns one fixed timer/packet
per port; no thread, APC, thread-pool callback, public API/config change or
timer-resolution-setting call is added. An already earlier scheduled wake is
retained across real I/O returns, avoiding per-event arm/cancel system calls.
Private timer packets only wake the original monotonic deadline loop. Missing
APIs/native optional failures preserve coarse IOCP behavior. STATUS_PENDING
forbids immediate packet reuse; unexpected cancellation failure disables the
optional precision path. Stop cancels associations, separates private packets
from tracked physical I/O, closes native handles and retires storage.

A native timeout fault injection first reproduced immediate premature public
TIMEOUT with the old fallback decision. Both the precision path and coarse
fallback now let the outer wait check its original deadline. The same injected
test passes after the fix; no logical timeout renews that deadline. New tests
also cover optional API absence, native timer/packet creation, set/association/
cancel failures, already-signaled targets, pending cancellation, 1000 earlier
wake reuses with one association/no cancels, 2000 native lifetimes, noninherited
handles, stale wakes, real posted I/O and observed zero handle delta.

Final exact candidates pass 19 Windows core/IOCP and 11 WSL core programs,
plus 30 Windows and 30 WSL xredis programs (90 total). WSL uses O2 ASan/UBSan.
Both six-case Redis 8.0.5 TLS/auth/RESP2/3 matrices pass, including old session/
blocking oracles and automatic CLIENT KILL/refill. Minimal static/shared native
consumers, examples, trim, three size kinds and generated-header checks pass.
The independent fixture correction is `c8f2df15`; its initial failure is retained.
Windows interop fingerprint `ad0a803a84933c01909ed54fe68a949d7530b0c7615b9add2154049ac823c9a9`;
WSL `dffe1000a8851c6492aff46a0487300ce97f992b82db443b0ddaa2bae2b3c870`. WSL is not native Linux deployment
acceptance; native MSVC and final sustained/fault acceptance remain pending.

Measured x64 IOCP context grows 184 -> 264 bytes: the helper is 80 bytes and two
native handles. Ordinary operation storage stays 112 bytes; public port config
40 and event 216 stay unchanged. Runtime checks, clock reads, native handle/
packet resources and necessary arm/association/cancel calls remain real costs.
No zero-overhead claim is made. `docs/api/net.md` links the official API contracts.

The new public port/AUTO/SELECT timer profile retains five full 1000-timer
repeats plus one warmup, with per-metric 3% MAD / 6% central-range gates.
Windows reports 0, WSL
0 quality failures. Windows AUTO P50 is
1989.900018089 us; SELECT control
15511.000005063 us. Zero readings
from the coarse Windows process-CPU counter are not proof of zero CPU cost.
Core source fingerprint `90c5a597251cb964519f4b601a4074fff29aeae4fa06367e3ab09858fb1e6f2f`;
timer workload fingerprint `3d894e4620116f88e37d18d874364de210ee75b7a4f01d4ef8e93df400c77eec`.

The alternating Windows paired pool comparison also keeps five 1000-cycle
repeats plus warmup. Repair P50 median changes
15537.199971732 ->
1937.800028827 us. All six workloads complete;
26 quality checks fail. Warm throughput medians
17034.70140062 ->
15432.479818926 cycles/s (-9.41%). This
apparent loss is not dismissed and is not an accepted regression result: noise
gates fail, the machine is unpinned, and pinned matched sampling/profiling must
resolve throughput and tail costs before performance acceptance. No samples are
removed and no threshold is relaxed. The full four-library goal stays active.

Core maturity still reports the previously known `net_file_future` missing
single-test closure; no unrelated release gate is bypassed. Ignored
`out/service-extensions/windows-timer/` contains exact 90 binary hashes, native
API/fallback failure records, reference/paired sources and binaries, raw samples,
interop/package/size/layout receipts and validation. Remaining Redis dedicated/
HA/tracking and P3 xrpc/xmqtt/xcron runtimes plus all final native deployment,
>=72-hour mixed/fault and production/performance gates remain unachieved.

## P2 measurement: sustained pool samples and explicit CPU placement (2026-10-30)

The pool benchmark now accepts an optional validated CPU mask before creating
workers, reports actual placement, and measures process CPU time per cycle.
Invalid/unavailable masks fail before fixture construction. Windows uses its
current processor group; Linux restricts the main thread before workers inherit
the set. No host priority, power policy or timer-resolution setting changes.
This is process placement, not per-worker pinning or host isolation. Windows
CPU counter granularity remains visible, including zero short-sample readings.

The new sustained profile keeps the same loop/peer and increases each fixed and
automatic warm sample to 100000 cycles; physical repair retains 1000. The old
short profile and its evidence remain. Native Windows and WSL reject four bad
masks and accept/report 0x110; all three WSL profile smoke workloads complete.
Windows comparator builds use O2/Wall/Wextra/Werror. Normal xredis maturity
passes with 34 workloads; no production source or public API changes here.

Host topology is six physical cores with SMT pairs 0x3/0xc/0x30/0xc0/0x300/0xc00.
Both native comparators use logical CPUs 4 and 8 (mask 0x110), on separate
physical cores. Each keeps five full samples plus one warmup, alternating order,
identical inputs/features/workers/compiler and every source/header except the
IOCP translation unit. The reference TU is exact d50664b2; current is 40ddf61c.
Reference executable SHA256 e1b152f79a6735b33795f253003646aec4719b8a7ee3acd39c167e891e1c14ee;
current accc9ab66e4e4386c7c0d72cec888f6488b3338d7f3ad608fba40d4a1755fc8b.

Reference/current warm throughput medians are 14827.222/14501.229 cycles/s
(-2.20%); P99 medians are 185.0/184.6 us, CPU 61.250/62.812 us/cycle.
Repair P50 medians are 15571.2/1933.5 us. All counts complete, but 39 checks fail
the unchanged 3% relative MAD / 6% central-range limits. No sample is dropped.
These results do not establish or dismiss the previous apparent -9.41% signal,
prove throughput parity, or pass performance acceptance. Process placement and
longer samples do not isolate worker migration, SMT siblings or competing host
work. Controlled native profiling remains needed before attributing a cost or
making another runtime optimization.

Ignored out/service-extensions/pinned-throughput/ retains raw samples, topology,
compiler/source/binary receipts, all failed gates, CLI/smoke results and a
post-commit identity check. The four-library goal stays active; Redis dedicated
modes/HA/tracking, xrpc/xmqtt/xcron runtimes, native Linux deployment and final
>=72-hour mixed/fault and production/performance acceptance remain pending.

## P2 measurement: preserve declared workload facts in reports (2026-10-30)

Report inspection found twelve xredis workloads across eight profiles using
`metadata`, which the performance tool ignores; its supported key is `facts`.
Only those keys are corrected. Actual workload strings now reach the JSON
reports, and all three sustained pool workloads also retain effective affinity.
The original raw paired samples already recorded placement independently and
remain untouched. An absent field in an older report is not backfilled.

Native Windows and WSL each rebuild/run all twelve affected smoke workloads,
one sample and no warmup, and verify the exact declared nonempty fact sets
and binary hashes (24 real workloads total). These are report-contract checks,
not new full performance samples or deployment acceptance. No runtime source,
sample-selection rule or threshold changes. Both earlier noisy comparisons
remain unaccepted. Ignored out/service-extensions/benchmark-facts/ keeps the
before-config snapshot, complete new reports, commands and per-binary receipts.

## P3 xrpc: bounded synchronous request dispatch (2026-10-30)

`xrpc_dispatch` connects the existing decoder/encoder to caller-thread business
resolution and a synchronous response sink. Every input byte is validated before
any user callback. Malformed JSON produces one Parse error/null id; invalid
items produce Invalid Request/null id. Unknown/reserved methods and missing
handler replies produce standard errors. Calls retain exact original ID tokens.
Every valid notification stays silent, including unknown/reserved/failed methods;
an all-notification batch emits nothing. Nonempty batches retain array shape.
Application parameter validation and decoded-name lookup remain explicit caller
duties; this leaf does not claim a method index or negotiated rpc. extensions.

The finite response count is admitted before callbacks. Per-object encoder
limits and complete-document byte/reply limits are distinct; aggregate bytes
include commas/brackets. Reply functions preflight and invoke Emit before
returning, so handler-local stack payloads are safe. Duplicate/reentrant reply
attempts, invalid output, a false handler return after a reply and sink rejection
are sticky local failures. The first diagnostic has an owning edge and survives
callback error clearing/replacement. Ordinary success restores ambient identity.
Later handlers do not run after failure; Info records accepted partial output
and entered/completed work. Earlier business/sink effects cannot be rolled back
or blindly replayed. The sink must discard a partial response document.

Final implementation tests pass three modular and three single-header programs
on each of Windows GCC O2/Wall/Wextra/Werror and WSL GCC O2 ASan/UBSan (12 verified
executables). Checks cover exact wire/error/id/batch fixtures, malformed tails
before execution, notification suppression, stack overwrite, independent nested
dispatch, duplicate/sink reply reentry, sticky errors, stopped later handlers,
actual/exact aggregate caps, API/alias/config rejection, 10000 independent mixed
batch properties plus 1000 malformed tails, 10000 ordinary allocation-counter
repetitions and diagnostic OOM under a one-byte quota. Earlier decode/encode
adversarial/property cases run too. Each platform passes three examples, trim,
real static/shared consumers, declaration-only compilation and all three size
kinds. Source/header receipts preserve the tested inputs; post-test changes are
exactly one facts-key correction and a diagnostic-allocation comment refinement
propagated into generated headers, with declarations/implementation unchanged.
The evidence validator checks those exact byte replacements rather than claiming
the older binaries compiled a later comment/config snapshot.

Measured Windows/WSL x64 sizeof values agree: private context 272, reply handle
32, outgoing descriptor 120, caller message scratch 224 each, config 176 and
info 64 bytes. These are not total stack measurements: compiler padding, nested
JSON/encoder stacks, input/sink storage, diagnostics and callback work are extra.
Complete input scanning, per-reply preflight followed by independent sink Write
validation/copying/quoting, config/counter checks, callback calls and error
ownership bookkeeping remain real costs. No new thread, queue, connection or
ordinary valid-input heap allocation is added; protocol/error diagnostics and
user code can allocate. No zero-overhead claim is made.

The new throughput workload keeps one three-item batch (one call, one valid
notification and one invalid item), including full decode/dispatch/reply
preflight/single-object encoding. It excludes name lookup, transport, queueing
and final batch-buffer assembly. Each platform retains five full 1000000-document
samples plus one warmup. Windows median is 324063.150 documents/s, WSL
156684.745; quality failures are respectively 0 and 4 at unchanged 3% MAD / 6%
central-range limits. Samples are not dropped. These unpinned local measurements
do not establish server throughput, cross-platform superiority, tail latency or
production/performance acceptance. WSL is not native Linux deployment evidence.
Source fingerprint a23a7808298067a7b2a94626b4ba34cabd08e21ec6d427b1a77dc1695296fe3b;
workload 34171eb815a2d41b773dd6937c7964c85ae511e6672e846ccaf9342f31e99159.

Normal xrpc maturity and API coverage pass (9 functions, 48 constants, 18 types;
three workloads/size profiles); strict release rejects the developing xrpc root.
Ignored out/service-extensions/xrpc-dispatch/ keeps exact tested inputs, twelve
binary hashes, distribution/layout/size commands and artifacts, all samples and
noise failures, refinement proofs and the post-commit identity check. Method
indexing, correlation/client/server async scheduling, transports, deadline/cancel
negotiation and all final native >=72-hour mixed/fault/performance gates remain
pending, as do the other products' remaining scope. The four-library goal stays
active. See extlibs/xrpc/docs/dispatch.md and its runnable example.

## P3 xrpc: explicit local abort from a handler/resolver (2026-10-30)

Method-index integration exposed a concrete callback gap: Call=false means an
unknown method, so a resolver's local resource/validation failure could otherwise
be mislabeled Method not found. `xrpcReplyAbort` now stops the dispatch locally
without emitting a wire error. An explicit borrowed XRT diagnostic survives
callback clearing through an owning edge; the current diagnostic transfers
without another Ref, while another error uses a checked Ref. NULL creates a
fresh XRPC_DISPATCH_ABORTED/XERR_STATE diagnostic and never adopts stale ambient
error. A prior sticky failure wins. Notifications and already accepted replies
can also abort; accepted output/effects remain in Info and later handlers stop.
This is a local control path, not negotiated protocol cancellation.

Windows O2/Wall/Wextra/Werror and WSL O2 ASan/UBSan each pass three modular and
three single-header programs (12), examples and trim. New tests cover abort
before/after output, notifications, sink reentry, repeated abort, explicit/current
diagnostic lifetime, stale ambient isolation and diagnostic OOM. Both native
static/shared consumers exercise real local abort, and declaration/layout checks
pass. Normal maturity/API coverage and generated checks pass (10 functions,
49 constants, 18 types); the product remains developing. This adds a cold API
path, with no new per-call state or hot-loop invocation. No new performance or
production acceptance is claimed. Ignored out/service-extensions/xrpc-abort/
pins exact inputs, commands, 12 test binaries, consumers and post-commit identity.
Method indexing, async runtime/transports and all other unfinished final gates
remain pending; the full four-library goal stays active.

## P3 xrpc: bounded immutable semantic method index (2026-10-30)

`xrpc_methods` copies decoded UTF-8 registrations into one immutable allocation
with atomic lifetime references. Names/callback/context values are copied; code
and context ownership stay with the application. Source vectors/names can be
freed after construction. Empty/NUL names, every Unicode scalar and equivalent
JSON escapes match exactly; case and normalization forms remain distinct.
Duplicate names, invalid UTF-8, NULL callbacks and the reserved rpc. prefix are
rejected. Lookup validates the complete raw JSON string before any match, even
for an empty table. Local validation/limit/diagnostic-OOM errors never become a
miss. The dispatch adapter uses the independently committed local-abort path
95268a4b, preventing a local failure from becoming Method not found.

The sorted key is a 32-bit noncryptographic FNV-1a fingerprint plus the COMPLETE
name. In-place heapsort uses no libc qsort/growth scratch. Binary search compares
complete names within collision groups; collisions do not create linear probe
chains. Query decoding uses only a four-byte iterator buffer; ordinary hit/miss
does not allocate a decoded name. Count, decoded name bytes, raw query bytes,
total copied names and requested owned storage have independent finite bounds.
Decoder admission remains separate and must be configured for long/heavily
escaped names. Find/Info need a live reference but add no index ref operation;
Retain/Release are actual atomic work. Keep one reference through an entire
dispatch and all borrowed bindings. Unprotected load-then-retain publication and
callback context thread safety are not supplied by this immutable container.

Windows GCC O2/Wall/Wextra/Werror and WSL GCC O2 ASan/UBSan each pass four modular
and four single-header programs (16). New tests cover all 1112064 Unicode scalars
in escaped and applicable raw UTF-8 spelling, all simple escapes/NUL, case/NFC
differences, an explicit FNV32 collision pair, copied/freed source vectors,
4096 permuted registrations, complete malformed tails/types/UTF-8/surrogates,
exact/rejected bounds, duplicate/reserved/API errors, dispatch/local-abort for
calls and notifications, 10000 allocation/ambient-identity loops, diagnostic OOM
and 511 constructor quota windows. Four retained readers complete 80000 lookups
after creator release. Earlier codec/dispatch regressions run too. A separate
WSL O1 TSan modular run reports no race; GCC's atomic_thread_fence modeling
warning is retained and no complete race-freedom claim is made.

Both platforms pass examples, trim, native static/shared consumers using actual
escaped-name lookup/dispatch/Retain/Release, declaration checks and all three
size kinds. The first manual WSL single-header link lacked the waiting leaf's
libm dependency; its failure log is preserved. Correcting the test command
passes without a production-code change. After tests the benchmark alone adds
an explicit memory.h include; its before bytes are retained and it is compiled
separately. Runtime/header/manifest inputs are unchanged and checked exactly.

Measured x64 layouts agree: index prefix 80, entry 40, binding 32, iterator 40,
config 48 and info 32 bytes. The 4096-entry workload requests 225360 owned bytes.
These exclude allocator headers/rounding, caller contexts and nested parser/
encoder stacks. Name copies, validation/hash scans, entry moves, query validation,
possible repeated escape decoding, callback calls, references and diagnostic
bookkeeping are real costs; there is no zero-overhead claim.

Four profiles separately retain plain/escaped/missing lookup and one-call
decode/index/handler/encode against 4096 registrations and 64 spread queries.
Each platform keeps five full 1000000-operation samples plus one warmup and
10000 separate latency observations per process. Throughput excludes per-op
clocks; latency includes two xrtTimer calls and shows clock quantization. No
sample is dropped and 3% relative MAD / 6% central-range gates are unchanged.
Windows has 10 quality failures, WSL 13. Indexed-dispatch median rates are
592728.314/589681.850 operations/s and P99 medians 3.199966/3.100000 us respectively;
these noisy local CPU workloads exclude construction, queues and transport and
do not establish server throughput/tail or cross-platform superiority.
Performance acceptance stays false, and WSL is not native Linux deployment
acceptance. Source fingerprint 18fdf94ce56eb136918fd870bd09d0e5efb23d1220bdb790056937738d9b5e29;
workload 46047d07f71942bb48b3ec3eafd54bf580ceca7bb2ef7d46abbb7445cac6f03d.

Normal maturity/API/generated checks pass (17 functions, 58 constants, 24 types;
seven workloads/four size profiles); strict release still rejects developing
xrpc. Ignored out/service-extensions/xrpc-methods/ pins exact inputs, 16 test
binary hashes plus TSan, failed/manual link evidence, consumers/layouts/sizes,
all samples/failures and post-commit identity. Correlation/client/server async
scheduling, transports, deadline/cancel negotiation, the other products' remaining
scope and final native >=72-hour mixed/fault/performance acceptance are pending.
The four-library goal remains active. See extlibs/xrpc/docs/methods.md and example.

## P3 xrpc: exact bounded identifier comparison keys (2026-10-30)

`xrpc_id` supplies caller-storage Size/Write for a complete string/number/null
identifier. The correlation policy compares strings after escape interpretation
and numbers as exact decimal values: 1/1.0/1e0 share a key, signed zeros share a
key, and string/number/null domains remain distinct. Original raw IDs remain
separate for wire use; binary keys are opaque, not JSON, numerically ordered or
a persistence format. Missing IDs and malformed/type/resource/diagnostic-OOM
failures are local. This is a correlation prerequisite, not a pending-call map.

Nonzero numbers normalize significant digits plus an exact signed decimal
exponent. Fraction/trailing-zero offsets use a bounded native byte count, while
the exponent remains decimal digits. Linear right-to-left add/subtract handles
arbitrary exponent carry/borrow without int/double conversion, saturation or
power-of-ten expansion. Zero needs no exponent materialization. Input/string/key
bounds are independent and finite. All failures preserve output/size variables;
each Write validates independently. No ordinary-path heap storage is owned.

The existing validated-string iterator is shared privately between Methods and
ID, with no public XRT change. Token comparison against db3809d4 proves identical
iterator logic after private identifier renaming. The full method Unicode and
concurrent-reader regression runs with the new header; a fresh WSL O1 TSan run
reports no race with GCC's retained atomic_thread_fence modeling limitation.
No complete race-freedom claim is made.

Windows GCC O2/Wall/Wextra/Werror and WSL GCC O2 ASan/UBSan each pass five modular
and five single-header programs (20). ID tests cover signed zeros, 2^53 neighbors,
fraction/scale changes, overflow/underflow distinctions, native-uint64 exponent
boundaries, long positive carry/negative borrow, type separation, all 1112064
Unicode scalars against an independent UTF-8 oracle, 20000 rational comparisons
plus equivalent coefficient/exponent shifts, exact/rejected limits/capacities,
canaries/aliases/config errors, 10000 allocation/ambient-identity loops and
one-byte diagnostic OOM. Earlier codec/dispatch/method tests remain in the runs.

The committed tools/test_xrpc_id_semantics.py additionally builds an ID-only C
line-pair probe and checks 20000 pairs per platform against Python arbitrary-
precision exponent/scale arithmetic (5000 unequal controls each). Positive and
negative exponents up to 200 digits and offsets through 1024 include cancellation
and nonzero mantissas. Both runs pass (40000 actual pairs); WSL uses ASan/UBSan.
The oracle expands no arbitrary power of ten and uses no floating-point value.
Source/compiler/binary/input/output hashes are retained independently.

Both platforms pass examples, trim, full static/shared consumers and separate
ID-only static/shared consumers. The minimal verifier manifest changes only the
consumer to the already registered ID example. Declaration/layout and all three
size kinds pass. Measured x64 plan is 160 bytes INCLUDING its 64-byte exponent
state; iterator is 40 and config 32. These are not complete stack measurements:
nested parser frames, compiler padding, caller output/storage and diagnostics
are additional. JSON validation, string traversals/encoding, numeric analysis,
exponent measurement/emission, output writes and error/reference bookkeeping are
real costs, not a zero-overhead claim. Existing parser/error bookkeeping can
synchronize even though the ID leaf adds no locks or retained state.

Three profiles retain five full samples plus one warmup on each platform:
1000000 escaped-string/ordinary-decimal IDs or 100000 thousand-digit-exponent
IDs per process, plus a separate 10000-observation latency phase. Throughput
excludes per-ID clocks; latency includes two xrtTimer calls and clock granularity.
All counts complete. Unchanged 3% relative MAD / 6% central-range checks report
11 Windows and 8 WSL failures; no sample/threshold changes or production acceptance.
Thousand-digit-exponent median throughput is 160790.058/214418.346 IDs/s and P99
11.799973/7.600000 us respectively. These noisy local CPU measurements exclude
queues, map correlation and transport, and establish neither cross-platform
superiority nor server/tail acceptance. WSL is not native Linux deployment.
Source fingerprint f70a763b451e342fe08536befceb46bab96922e25a72faac7fe63eb65ba2ddad;
workload aad093f9856d9e20124934a1c54943afbb48fc515a4d86461bacf05d95e1c2a4.

Normal maturity/API/generated checks pass (20 functions, 63 constants, 26 types;
ten workloads/five size profiles); strict release rejects developing xrpc.
Ignored out/service-extensions/xrpc-id/ pins 20 binaries, both oracle datasets,
consumer/layout/size/Tsan commands, complete samples/failures and post-commit
identity. Before rebuilding, 34 prior verified artifacts were copied with checked
hashes to xrpc-methods/before-id/. Pending-call admission/correlation, duplicate/
late/unknown response policy, deadlines/cancellation, async runtimes/transports,
the other products' scope and final native >=72-hour mixed/fault/performance
acceptance remain unfinished. The full goal stays active; see docs/id.md/example.


## P3 xrpc: bounded serialized pending calls (2026-10-30)

`xrpc_pending` supplies admission, response-ID removal, token cancellation,
finite-deadline expiry and complete shutdown draining. One serialized owner
controls a table; it adds no callbacks, locks, threads, executor or transport.
Only successful removal publishes a call record, after it has left both indices.
An owner can then complete the borrowed application state exactly once across
response/cancel/expiry events. The User pointer is borrowed; destruction never
frees it or invokes business code. Drain first when all calls need completion.

A preallocated AVL tree compares copied exact ID keys; a min-heap orders absolute
caller-supplied monotonic deadlines, with admission serial as the tie breaker.
UINT64_MAX is never, including for Expire(UINT64_MAX); TakeFirst can drain those
entries. Input parsing/key canonicalization happens independently on every
Insert/TakeId. Tokens skip parsing and check a slot before normal tree/heap
removal. Tree mutation transplants nodes rather than exchanging live payloads,
so token slots remain stable. A nonzero 64-bit serial is never reused; exhaustion
fails without wrapping. Duplicate wins over full; both are ordinary outcomes
with unchanged output and ambient diagnostic. Invalid ID input is still an error
when full. Failed/missing removals leave their output unchanged.

Local ABA protection is scoped to one table lifetime. It does not resolve old
remote responses after wire ID reuse: runtime-generated fresh IDs/connection
epochs and eligible-envelope admission remain necessary. This leaf takes an ID,
not a response document. A future response pump must fully validate a batch before
any removal. Null is a distinct allowed comparison key here, not a decision to
accept unsolicited null-ID protocol errors as eligible responses. No raw ID is
retained; the owner retains its original wire representation separately.

The public config is 56 bytes; token 16, returned call 32 and info 40 on both
measured x64 platforms. The private header is 88 bytes and each entry 56. Heap
indices add four bytes per slot; fixed key slots plus one shared key scratch area
are reserved in the same allocation. Requested storage is
`88 + MaxCalls * (56 + 4) + (MaxCalls + 1) * MaxKeyBytes` on these ABIs, excluding
allocator headers/caches and application state. Defaults (1024 calls, 512 key
bytes) request 586328 bytes under the 2 MiB ceiling. ID input/string limits are
4096/256 bytes. Constructor arithmetic and every limit are checked before
allocation. The private 32-bit slot links are a layout choice, not a measured
speed advantage over XRT's public pointer-node AVL. No shared runtime is added.

Windows GCC 16.2.0 O2/Wall/Wextra/Werror and WSL GCC 15.2.0 O2 ASan/UBSan each pass
six modular and six direct single-header programs (24). The pending public unit
performs 100000 operations against an independent linear reference model,
checking admission/removal records, counts and earliest deadline after every
operation. It covers equivalent decimal/huge-exponent/NUL IDs, type domains,
overwritten original input, response/expiry vs stale cancel, reused slots,
never/tied/exact deadlines, full/duplicate ordering, 4096 ascending/descending/
permuted mutations, exact/rejected owned/key/string limits, aliases and NULL APIs.
10000 steady cycles retain allocation counters and ambient diagnostic identity;
constructor and diagnostic failures under a one-byte quota preserve state/output.

The committed `tools/test_xrpc_pending_structure.py` separately builds the real
pending dependency closure with a private probe. On each platform, 50000 mutations
check independently recomputed AVL ordering/heights/balance, heap tuple order and
reverse indices, and complete disjoint live/free partitions. Forced serial state
exercises the final UINT64_MAX admission and subsequent rejection without running
2^64 operations. Both pass; the WSL probe uses ASan/UBSan. This deliberate private
state injection is not represented as a natural 2^64-operation endurance run.

Both platforms pass all six runnable examples, trim, complete xrpc static/shared
consumers and pending-only static/shared consumers. The minimal verifier manifest
changes only the consumer to the registered pending example. The declaration
consumer compiles the new pending API from declaration headers. Measured object
sizes are text/data/bss/file bytes:

| Platform | Complete-closure single object | Product-only static | Product-only shared |
| --- | --- | --- | --- |
| Windows x64 | 228348 / 2664 / 11040 / 345891 | 11652 / 0 / 0 / 17112 | 20438 / 144 / 272 / 54359 |
| WSL x64 | 237531 / 3280 / 10924 / 417472 | 11732 / 0 / 0 / 21468 | 14263 / 640 / 8 / 25256 |

The single object includes its core dependency closure; separate product objects
exclude host XRT. These columns are not interchangeable measures of total deployed
size. Raw commands, frozen source/compiler/binary hashes, layouts, consumer and
size receipts live under `out/service-extensions/xrpc-pending/`. Before overwriting
shared output locations, 42 prior ID-stage artifacts were copied and SHA-256
checked under `out/service-extensions/xrpc-id/before-pending/`, with an archive map.
All prior source/tests remain present. Generator/API checks pass; normal maturity
reports 9 modules, 7 covered implementation nodes, 15 workloads and 6 size profiles.
Strict release still fails on the developing product, without a bypass.


The dedicated benchmark runs five workloads: response/remove+reinsert at live
occupancy 1/1024/4096, plus token cancellation and expiry at 1024. Every workload
uses 1 warmup and 5 measured runs of 1000000 cycles, with a separate 10000-sample
latency phase. IDs are precomputed and locally reused solely for this leaf
benchmark, not a safe runtime wire-ID policy. Setup, formatting, queue wait,
future completion and transport are excluded. Latency includes two clock reads;
throughput has no per-cycle clocks. The 64-byte key slot configuration requests
127128 bytes at occupancy 1024. All completion counts match; all samples remain.
Medians below describe this workload only, not production capacity:

| Platform | Removal / live calls | Cycles/s | P50 us | P99 us |
| --- | --- | --- | --- | --- |
| Windows | response / 1 | 2732087 | 0.400 | 0.400 |
| Windows | response / 1024 | 1373046 | 1.000 | 1.300 |
| Windows | token cancel / 1024 | 1977111 | 0.500 | 0.600 |
| Windows | expiry / 1024 | 1389651 | 0.800 | 1.100 |
| Windows | response / 4096 | 1134335 | 0.700 | 1.300 |
| WSL | response / 1 | 1346328 | 0.600 | 0.700 |
| WSL | response / 1024 | 1028931 | 1.000 | 1.200 |
| WSL | token cancel / 1024 | 1333341 | 0.700 | 0.900 |
| WSL | expiry / 1024 | 1250714 | 0.700 | 1.000 |
| WSL | response / 4096 | 1125867 | 1.000 | 1.500 |

Windows has 17 failing metric/noise checks and WSL 14, under unchanged 3% relative
MAD and 6% central-range limits. Both performance runner exits are 1, retained in
separate exit receipts. No noise failure, outlier or result is suppressed. There
is no pinned comparator/absolute objective or native-Linux acceptance, and this
leaf excludes end-to-end queueing. Performance and production acceptance remain
**unaccepted**, regardless of individual throughput/latency medians.

Matching Windows/WSL source fingerprint: `86cdd82571fe6b615899f5179c30fc55b1bf6ad95b60d8425935862a50c8471a`.
Workload fingerprint: `c212859acc5c135d594d8fb4e56b7cfcfb52ee6701bb230b0ab4eed3f6e77c68`.
`validation.json` verifies 24 actual test binaries, the private probes and their
source/output hashes, consumers, declaration/layout/size objects, actual measured
counts, and both fingerprints against current inputs. The complete source-input
map remains unchanged through compilation/measurement; only documentation is
updated afterward. Raw command/metric/exit receipts stay in the phase directory.

Remaining P3 work is a whole-document response pump, immutable owned results,
client/server async scheduling and transports, connection epochs/fresh wire IDs,
negotiated cancellation/deadline behavior, fault/recovery tests and native pinned
performance/72-hour mixed-load acceptance. The other three product scopes remain
active; this table does not complete any final product.


## P3 xrpc: owned responses and complete batch correlation (2026-10-30)

`xrpc_response` reads a complete response document into an immutable owned
snapshot. `xrpc_receive` combines it with serialized pending-call correlation.
The [protocol reference](https://www.jsonrpc.org/specification#response_object)
is JSON-RPC 2.0 sections 5/6: response envelopes and ID-based matching, including
arbitrary response order. Whole-document publication and all-or-none local
admission failure are explicit xrpc policies, not new wire protocol requirements.
No upstream implementation code is used.

The response reader validates the whole JSON document and every envelope before
allocation/publication. Every item must be a result/error. Request/notification,
scalar, empty batch and invalid late item publish nothing. Status distinguishes
malformed JSON, valid-JSON invalid envelopes and local resource/API/OOM failures;
diagnostic OOM is local. No automatic response is sent to a received response.
The existing response-mode decoder's ignored-extra-field policy remains; a
bidirectional runtime still classifies its incoming document role separately.

One allocation owns the actual-count message descriptors, a contiguous raw-ID
view vector and one complete wire copy. All nine descriptor views are relocated
into that copy without another parse, unescaping or numeric narrowing. Whitespace,
unknown wire fields, raw huge/exponent IDs, results and full error/data spans stay
exact. Caller input/scratch can be reused after success. A retained snapshot and
item index can back asynchronous results without a per-item payload allocation.
Holding one item holds the entire batch; aggregate retained-result budgeting
still belongs in the runtime. User call-state pointers are outside the snapshot.

`xrpcPendingTakeBatch` validates all spans, independently canonicalizes every ID
under the table's own limits and finds every candidate before removing anything.
The resolution array is unpublished scratch on failure. The commit pass does no
parser, allocation, diagnostic or callback work: each first live-token occurrence
is MATCHED, later equivalent occurrences REPEATED, and preflight-missing IDs
UNKNOWN. Only MATCHED carries the borrowed Call/User. Previously completed IDs
are unknown, not distinguished through an unbounded history. All matched calls
leave both indices before receive publishes final output. The owner completes
calls afterward; future completion cannot reenter midway through this function.

A late malformed/invalid response, key/string limit, resolution capacity or
allocation/diagnostic failure leaves all pending calls and final object/info
outputs unchanged. Scratch/resolution contents must be discarded. Source input,
config, scratch, resolution storage, output pointers and pending storage are
mutually disjoint for Receive. Snapshot Info rejects owned-output overlap before
reading typed output fields. The table still requires one serialized owner;
wire ID reuse/epochs, response eligibility (especially null-ID errors), deadline
ordering, executor dispatch and physical I/O/cancellation remain runtime duties.

Measured on Windows/WSL x64: private snapshot header 40 bytes, public message
224, raw ID view 16, response config 96 and response info 64. Requested snapshot
storage is `40 + Count * (224 + 16) + InputBytes`; the one-item 38-byte fixture
requests 318 bytes. The default owned ceiling is 2 MiB, independently of the
response decoder's default 1 MiB input/256-message limits. Caller scratch,
resolution arrays, pending tables, allocator headers/caches and application state
are additional storage, not silently folded into this bound. The ID view vector
adds 16 bytes/item to avoid strided casts or a second descriptor scan for batch
correlation. Snapshot reference increments/decrements are atomic, overflow is
checked, and publication of the initial strong ref is the caller's duty.

Windows GCC 16.2.0 O2/Wall/Wextra/Werror and WSL GCC 15.2.0 O2 ASan/UBSan each pass
eight modular and eight direct single-header programs (32). The existing codec,
ID, method and pending coverage remains in those runs. Added tests cover:

- 10000 pending-batch transactions against a separate linear state/outcome oracle,
  including bad late IDs, canonical duplicates, arbitrary ID order and unchanged
  pending counts on failure; ordinary batch operations add no table allocation.
- 10000 generated response snapshots with independently formatted expected raw
  payloads, every view inside owned wire, contiguous ID alignment, overwritten/
  freed original input, huge IDs, error/data spans, NUL escapes and retained refs.
- 10000 mixed ten-response batches, observing eight matched call tokens/users,
  one repeated signed-zero ID, one unknown ID, all removals before return,
  result/error classification and source/scratch reuse.
- Exact/rejected owned limits, decoder bounds/role, malformed/invalid tails,
  late table-key limits, insufficient resolution storage, NULL/overflow/alias
  inputs, one-byte snapshot/diagnostic OOM and unchanged final outputs/state.
- 10000 ordinary snapshot create/retain/release cycles with exactly one requested
  Malloc call per document, no Calloc/Realloc calls, and unchanged ambient error;
  mixed successful receive outcomes likewise preserve ambient diagnostic identity.
- Four retained readers complete 80000 metadata/payload observations after the
  creator releases its reference. A separate private probe forces reference
  overflow and verifies owned-output rejection and real ABI/layout values. This
  is not represented as a natural 2^32-reference endurance run.

The private pending structural probe is rerun against the new batch implementation
on both platforms: 50000 AVL/heap/free-partition checks and forced serial exhaustion
pass. The WSL response unit also passes a fresh O1 TSan run; its command, binary,
full source-input map and logs are retained. GCC's atomic_thread_fence modeling
warning remains in the build log, so no complete race-freedom claim is made.

Both platforms pass all eight examples/trim, complete xrpc static/shared consumers
and pending/response/receive-only static/shared consumers. Minimal manifests only
change package_consumer to an already registered asset: the complete public pending
unit for the new batch ABI, and the two runnable examples for response/receive.
This includes actual cross-library allocation-budget/error identity checks.
Declaration consumers compile the new receive signature separately. Normal maturity
reports 12 modules, 9 covered implementation nodes, 18 workloads and 8 size profiles;
strict release still fails on developing xrpc without a bypass.

Size measurements are text/data/bss/file bytes, without stripping:

| Platform | Closure | Complete-closure single object | Product-only static | Product-only shared |
| --- | --- | --- | --- | --- |
| Windows x64 | xrpc_pending | 229572 / 2664 / 11040 / 347222 | 12828 / 0 / 0 / 18440 | 21661 / 144 / 272 / 55440 |
| Windows x64 | xrpc_response | 227716 / 2664 / 11040 / 346865 | 8868 / 0 / 0 / 13260 | 17907 / 144 / 272 / 51580 |
| Windows x64 | xrpc_receive | 243360 / 2664 / 11040 / 365787 | 22868 / 0 / 0 / 33896 | 32397 / 144 / 272 / 69627 |
| WSL x64 | xrpc_pending | 238777 / 3280 / 10924 / 418968 | 12897 / 0 / 0 / 22876 | 15494 / 640 / 8 / 25304 |
| WSL x64 | xrpc_response | 237030 / 3280 / 10924 / 419000 | 8934 / 0 / 0 / 17940 | 11989 / 696 / 8 / 24904 |
| WSL x64 | xrpc_receive | 252913 / 3280 / 10924 / 441624 | 23061 / 0 / 0 / 44112 | 27244 / 776 / 8 / 38648 |

Single objects include their core dependency closure; separate product artifacts
exclude host XRT. These are not interchangeable total-deployment sizes. The
response-only leaf does not depend on pending/receive or an async engine.

Raw commands, source/compiler/binary/input/output hashes, private probes, TSan,
consumer and size receipts are under `out/service-extensions/xrpc-response/`.
Before overwriting shared locations, 54 prior pending-stage artifacts were copied
and SHA-256 checked under `out/service-extensions/xrpc-pending/before-response/`
with an archive map. Current functional/package inputs stay frozen through their
checks; development iterations and the earlier smoke remain separately named.


Full performance measurement reruns pending's five workloads and adds three
response workloads, all at 1 warmup + 5 measured runs. Pending cycles use 1000000
iterations each. Small snapshots and mixed receive batches use 200000 documents;
64 KiB snapshots use 2000. Latency is a separate phase with min(Count,10000)
samples, including two clock reads. Throughput has no per-operation clocks.
Input preparation is outside timing. Snapshot timing includes validation,
allocation, wire/descriptor copy/rebase, Info and release. Receive includes two
admissions, a four-response document with two matches/one repeat/one unknown,
complete correlation/removal and release. Its wire IDs are locally reused for
this primitive test, not a runtime ID-reuse policy. Queues, Future completion,
transport, TLS and actual peer processing remain excluded.

All declared completion/sample counts match on both platforms; every measured
sample remains. New workload medians describe these paths only:

| Platform | Workload | Documents or batches/s | P50 us | P99 us |
| --- | --- | --- | --- | --- |
| Windows | xrpc_response_small | 1070956 | 1.200 | 1.400 |
| Windows | xrpc_response_64k | 9947 | 107.100 | 155.500 |
| Windows | xrpc_receive_batch | 244392 | 3.700 | 6.300 |
| WSL | xrpc_response_small | 1027448 | 1.300 | 1.600 |
| WSL | xrpc_response_64k | 11055 | 75.300 | 172.200 |
| WSL | xrpc_receive_batch | 280357 | 2.900 | 6.700 |

Windows has 31 failing metric/noise checks and WSL 21, under unchanged 3% relative
MAD and 6% central-range limits. Both performance runners exited 1, preserved in
exit receipts. No outliers are removed, no thresholds relaxed, no timing overhead
subtracted and no performance/production acceptance claimed. There is no pinned
comparator, frozen absolute target, native-Linux server run or end-to-end fixed-
arrival-rate/queue-inclusive latency proof. The complete pending rerun stays in
the raw reports; prior noisy medians are not a valid regression comparator.

Matching Windows/WSL source fingerprint: `cdf4d8a161996f436f9faf0da1ad9cd5a6c8d0095b8f8f6dc2d17c049fc904d5`.
Workload fingerprint: `292ed6669be328d66f1a8e9f6fe83c1b7fefbe42300c3b653502b9ae2333528b`.
`validation.json` binds the final frozen inputs, 32 actual test binaries, private
probe sources/binaries/output, qualified TSan, complete and six minimal consumers
per platform, declaration/layout/size objects and every actual count/fingerprint.
Generator/API/normal maturity checks pass; strict release remains intentionally
unaccepted on the developing product, without overriding its gate.

Remaining P3 work includes async client/server operation/future lifetime and
bounded cross-thread admission, owned request handling and executor dispatch,
connection epochs/fresh IDs, transport framing/HTTP/WebSocket/stdio, queue-inclusive
deadline/cancel/close behavior and negotiated extensions. Aggregate retained-
result/storage budgets, protocol peers, native pinned performance and 72-hour
mixed/fault acceptance remain required. Redis dedicated/HA/tracking modes, MQTT
sessions/persistence/client and Cron time zones/scheduler/recovery remain active.
This stage completes neither xrpc nor the full four-product goal.

## P3 xrpc native call/Future lifetime

`xrpc_call` adds the operation-lifetime component needed by the async runtime.
Its production dependencies are response snapshots, public XRT Future and time;
pending/receive, thread helpers and allocation statistics are test-only dependencies.
It creates neither a transport nor an engine, timer, executor or submission queue.
The existing xhttp layer supplies message/exchange machinery, not a complete
socket-managing HTTP client. No reproduced Core gap required a Core change here.

Create returns one unique serialized-owner producer and one independently owned
Future observer. A separate preallocated immutable result survives destruction
of the producer/context without retaining an engine. Config is copied; outputs
and context ownership transfer only on success. Ordinary typed outcomes resolve
the Future, including remote error, timeout, cancellation, close and local
transport/protocol/resource failure. Callers inspect the result Status rather
than interpreting XFUTURE_RESOLVED as success. Cause is explicitly supplied and
retained, never inferred from unrelated ambient diagnostics.

The monotonic deadline starts at Create entry, includes construction/queueing,
supports immediate/forever, and is exposed for a future runtime to schedule.
Finish checks response kind/index and retains needed references before changing
state. It samples deadline/cancellation immediately before publication; visible
deadline takes priority and a later concurrent cancellation can lose. MarkSent
records MAY_HAVE_EXECUTED after transport accepts any request bytes. A validated
reply records REPLIED even when a local stop wins and discards its payload.
No retry/replay policy or automatic remote cancellation is implied.

Public XRT native producer binding V2 captures a real producer reference before
routing explicit Future cancellation outside its lock. Wake only signals/posts
owner work. Raw cancellation-token requests require explicit owner polling and
do not route Wake. Publication/destroy do not wait for an executing Wake; its
captured reference keeps producer/context storage alive through that callback's
return. Thus Destroy can return before context Drop. A queued post needs its
own references beyond Wake return. Context release is mechanical and callback
code stays live through every tail. The node reports its actual native reference
count and refuses opaque ownership traversal rather than claiming collector
admission. This stage introduces no collector integration.

An internal activation reference protects synchronous completion notifications
and release tails, including terminal reentry and Destroy from a Future Watch,
also when Destroy itself triggered the completion. Finish/Destroy preserve the
caller's ambient error despite notification/context-drop diagnostics. Checked
reference/publication failure retains the producer's owner right for retry.
Producer access remains serialized; Future observers use XRT's concurrent API.
No claim is made that arbitrary business callbacks are safe on an I/O worker.

Windows GCC O2/Wall/Wextra/Werror and WSL GCC O2 ASan/UBSan each pass nine modular
and nine direct single-header programs (36 total), all nine examples and trim.
New coverage includes all terminal statuses/delivery knowledge, explicit causes,
observer drop, retained response lifetime, immediate/forever deadlines, repeated
Future cancellation and raw-token polling, invalid arguments/aliases/kind/index,
completion reentry and destruction-in-progress. Construction sweeps 4096 quota
prefixes with both failures and successes, success-only context transfer and
zero live budget bytes. Preallocated completion succeeds under a one-byte quota.

A controlled thread test suspends the cancellation callback to model descheduling
(production Wake must never wait). The owner finishes and destroys while that
callback is still held; Future completion is visible, context Drop is still zero,
and the captured-reference tail releases it exactly once after the test gate opens.
An integrated pending/Receive test routes shuffled replies into four Futures,
suppresses duplicate/unknown IDs and retains correctly indexed results after
all producer contexts, table and original snapshot reference are released.
The private probe forces snapshot and producer-activation reference overflow,
verifies pending state/retry and reports actual x64 layout: producer 88 bytes,
config 40, result 32. These are requested structure sizes, not total heap cost
and not a natural billions-of-references endurance claim.

The call unit passes a separate O1 WSL TSan build/run. GCC's existing
atomic_thread_fence modeling warning remains in the log; this is qualified
race evidence, not complete race freedom or native Linux acceptance.

Before the final refactor, the locally tested prototype used a cancellation
Watch and checked Unwatch. The suspended-callback test exposed completion waiting
for its return. Switching to existing native producer V2 removes that wait and
one Watch allocation. The actual old source/header/single header and linked
object files were SHA-256 archived under `xrpc-call/legacy-watch/`; separate
1000-cycle warmed allocation probes link those old objects and current objects.
On both Windows and WSL the old prototype reports Malloc=4000/Calloc=1000/Realloc=0,
and the final implementation Malloc=3000/Calloc=1000/Realloc=0. This is one fewer
public allocator call per complete create/close/destroy cycle against this
task's own prototype, not an external-client or throughput comparison. Future,
cancel source, producer/result allocation and synchronization costs remain.
Ordinary publication with NULL Cause adds no new heap allocation after Create;
diagnostic/reference failure paths remain checked.

The exact-input receipts, compiler commands, binary/output hashes, private probes,
allocation comparison and qualified TSan are under `out/service-extensions/xrpc-call/`.
Before shared artifacts were overwritten, 90 preceding response-stage artifacts
were copied and SHA-256 verified under `xrpc-response/before-call/` with an archive
map. Earlier prototype/smoke logs are historical, not proof of the final source.

Both platforms pass complete xrpc static/shared consumers and call-only
static/shared consumers using the already-registered runnable call example.
The minimal manifest differs only in package_consumer; its production closure
does not import Receive, pending, test statistics or thread helpers. Declaration
consumers compile the call constructor signature. Generated features/API/single
headers are current; the API inventory has 44 functions, 107 constants and 49
types with none missing. Normal maturity reports 14 modules, 10 covered
implementation nodes, 21 workloads and 9 size profiles. Strict release continues
to reject the developing product, without an override.

Call-closure size is text/data/bss/file bytes, without stripping:

| Platform | Complete-closure single object | Product-only static | Product-only shared |
| --- | --- | --- | --- |
| Windows x64 | 283708 / 2664 / 11104 / 438395 | 12900 / 0 / 0 / 21496 | 22957 / 144 / 272 / 60902 |
| WSL x64 | 295808 / 4144 / 10932 / 535824 | 13037 / 64 / 0 / 29216 | 18217 / 936 / 8 / 30624 |

The single object includes its public XRT dependency closure. Separate product
artifacts exclude host XRT; these are not comparable total deployment sizes.

Full performance profiles run reply, explicit Future cancellation and immediate
timeout with one warmup and five measured runs each. Every run completes 200000
create/publish/read/destroy cycles and a separate 10000-sample latency phase.
Reply uses a prebuilt retained snapshot outside timing. The cycle includes core
Future/cancel creation, native producer binding and both endpoint destructions;
encoding, admission queues, scheduled timers, executor, transport, TLS and peer
work remain excluded. Latency includes two clock reads; throughput does not
read the clock per operation. This is not queue-inclusive end-to-end latency.

| Platform | Workload | Calls/s | P50 us | P99 us |
| --- | --- | --- | --- | --- |
| Windows | reply | 1554203 | 0.700 | 0.700 |
| Windows | cancel | 1074249 | 0.900 | 1.600 |
| Windows | immediate timeout | 1089229 | 1.100 | 1.300 |
| WSL | reply | 1324315 | 0.700 | 1.200 |
| WSL | cancel | 1003817 | 0.900 | 1.600 |
| WSL | immediate timeout | 1306186 | 0.700 | 1.200 |

All completion/sample counts match. Windows has nine failing metric/noise checks
and WSL five under unchanged 3% relative MAD and 6% central-range limits. Both
performance runners exit 1; no samples are removed, thresholds relaxed or timing
overhead subtracted. These medians are unpinned local observations, not a stable
regression baseline, external comparator or performance/production acceptance.
Matching source fingerprint: `f29e8ffd503b4084d7e429f7dea51689a5561b87cab185805fa2c4e93063f2ea`.
Workload fingerprint: `28a72138031da967ddc3152fead11872fd8a83320e33319be675e8be5eb3d0b9`.

`xrpc-call/validation.json` checks final frozen inputs against all 36 actual test
binaries, private overflow/layout outputs, qualified TSan, complete/minimal
consumers, declaration/layout/size artifacts, both old/current allocation-probe
objects and exact counts/fingerprints. The final receipt binds the clean committed
HEAD/tree. WSL remains development evidence rather than native Linux acceptance.

Next P3 work is the bounded async client/server runtime: cross-thread admission,
owned requests/executor dispatch, connection epochs/fresh wire IDs, actual
HTTP/WebSocket/framed-stdio transport and physical deadline/cancel/close duties.
Negotiated extensions, aggregate retained-result/storage budgets, independent
peers, native pinned performance and at least 72-hour mixed/fault runs remain.
The original Redis dedicated/HA/tracking, MQTT client/session/persistence and
Cron time-zone/scheduler/recovery scope remains active. No product is complete.


## P3 xrpc first ordinary HTTP/TCP call runtime

`xrpc_http_client` adds an actual asynchronous network call path on a running
public XRT engine. One worker owns one persistent numeric plain-TCP connection;
concurrent callers submit copied ordinary method calls through bounded admission.
One HTTP exchange is active at a time. Public Core HTTP/1 writer/head/body parsers
provide framing; no shared runtime or new Core API is introduced. Eight public
APIs expose config/connect, retain/release, async call, info, close and closed
observer. The product remains developing. Notification/batch networking, DNS/TLS,
pooling/pipelining, per-call HTTP metadata, other transports and server/executors
remain under the original final scope rather than being declared complete here.

The endpoint, HTTP authority/target and config are copied at Connect. Call input
is validated/encoded on the submitting thread and copied into one owned HTTP/JSON
wire block. Caller views can be reused on return. Calls get never-reused decimal-
string IDs; refused construction can leave gaps and 64-bit serial exhaustion
refuses further calls. A complete single response must have an equivalent string
ID, including escaped spellings. HTTP response order is not used to waive this
check. A valid matching RPC error on HTTP 500 remains a remote RPC error. No
implicit status-to-RPC-error mapping, redirect, authentication or replay occurs.

The deadline begins at CallAsync entry, including gate/admission, encoding,
worker queue, connect and reply. A product-private constructor passes the exact
monotonic origin into existing call logic; the public call ABI is unchanged.
The private entry can add a compiler-dependent call/tail-call rather than a
quantized timeout workaround; current lifecycle workloads are remeasured.
Queued timeout/cancel removes unsent work. After TCP accepts any bytes, a local
stop resolves the call with MAY_HAVE_EXECUTED and retires this connection; other
queued work closes without replay. A valid reply is REPLIED. Explicit causes
remain separate from ambient diagnostics, and payload Futures outlive client and
engine cleanup without retaining them.

Each accepted queue entry, timer, cancellation post and captured call-context
reference has an actual lifetime owner. Fixed-work/read-byte turns bound queue
scans and wire reads; complete JSON validation/copy still runs on the owner and
is separately bounded by response/JSON byte limits. Count admission includes concurrent construction and physical request
retirement, while byte admission includes each owned encoded wire until its last
request reference drops. Core stream read/write budgets are separate. Limits
also cover HTTP heads/fields, body/chunk/trailer sizes, informational count,
JSON decode/owned snapshot and aggregate response wire bytes. The wire cap counts
headers and chunk framing/extensions independently of decoded body size, avoiding
unbounded wire amplification within an otherwise small body limit.

Close retires transport and all request construction/timer/post/captured-context
duties before its observer completes. Its publishing stack still has a mechanical
client-reference tail; engine join is required before checking the final allocation
baseline. Completion Watches can close/release the user client reference without
invalidating the publishing worker/request. No business code runs under the
submission gate. I/O-worker Watches must not block. Native opaque contexts are
not collector admission. Aggregate caller-retained-result quotas remain pending.

The deterministic C loopback fixture is separate from implementation sources. It
checks actual POST/target/framing and emits literal fixed, chunked, informational,
trailer, malformed, truncated and close-delimited responses. This is useful local
protocol/runtime coverage, not independent peer interoperability. Tests cover:

- 64 queued calls with overwritten caller input and exact per-call result routing;
  retained results remain readable after all client/peer/engine cleanup.
- Fragmented three-byte read turns; chunk data split into single-byte chunks;
  100 Continue, trailers, matching IDs, remote error at HTTP 500 and EOF framing.
- Wrong IDs, conflicting TE/CL, truncation, duplicate unsupported content encoding,
  response body/head/trailer/JSON/snapshot/total-wire limits and typed retirement.
- Immediate unsent timeout, queued cancellation, sent timeout/cancel and absence
  of replay or transmission of cancelled queued calls.
- Count/byte refusal with unchanged output and ambient diagnostic; invalid nested
  configuration, malformed local Params and output/client aliases.
- 4096 request-construction quota prefixes, both admission successes and allocation
  failures, success-only publication and zero live budget bytes after retirement.
  Separate factory quota thresholds cover failure and successful close/join.
- Four submitting threads produce 128 independently checked calls. Completion-
  Watch close/release reentry and exact accepted/completed/retired accounting pass.

A separate O1 WSL TSan run passes the HTTP client unit. The existing GCC
atomic_thread_fence modeling warning remains retained; this is qualified evidence,
not complete race freedom or native Linux acceptance. The independent call
payload/activation overflow and ABI probe is retained for the constructor refactor.

Development iterations are preserved separately. The first peer crash was a
fixture event-context installation error, diagnosed in Windows GDB and WSL
Sanitizer output; accepted streams now explicitly bind their event context.
A later wire-limit test correctly made the client abort while the peer was still
sending. The fixture now accepts its resulting terminal send status; healthy-path
result checks and negative-path typed limit checks remain required.

A WSL example compile encountered ENOSPC on /mnt/c, while Linux / and /tmp were
available. Native NTFS compression of this task's out/ preserved all content and
paths: 23432 files/675 directories, 2501568811 logical bytes stored in 1100403844
bytes (2.3:1). No evidence or sources were deleted. Final checks rerun after this
space recovery. Performance artifacts use logical file sizes; compression is an
additional environment qualification, not a pinned performance acceptance.

Raw frozen-source/compiler/binary receipts and development logs are under
`out/service-extensions/xrpc-http-client/`. Before shared outputs were overwritten,
270 preceding call-stage artifacts, including actual allocation-probe objects,
were copied and SHA-256 verified under `xrpc-call/before-http-client/` with an
archive map. The earlier before-wire-limit snapshot and fixture iteration logs and transcribed
environment-error excerpts remain distinct from final evidence. The original full
ENOSPC/native-compiler allocation-error logs were overwritten by retries; excerpts
are not presented as final validation evidence.


Final functional verification uses Windows GCC 16.2.0 O2/Wall/Wextra/Werror and
WSL GCC 15.2.0 O2 ASan/UBSan: ten modular and ten direct single programs per
platform (40), all ten examples and trim pass. Complete xrpc and HTTP-client-only
static/shared consumers pass on both platforms. The minimal manifest differs
only in package_consumer, selecting the already-registered offline TCP example;
its production closure does not pull in test statistics or other RPC runtime
leaves. Declaration consumers compile Connect and CallAsync independently.

A separate executable linked to each actual packaged host Core library creates,
starts, queries, stops and destroys an engine: Windows reports iocp, WSL epoll.
Commands, compiler/library/source/binary hashes and results are retained.
Generated API/features/singles and normal maturity pass: 52 functions, 121
constants, 55 types; 16 modules, 11 covered implementation nodes, 22 workloads,
10 size profiles and developing=1. Strict release remains rejected without
an override. WSL is development evidence, not native Linux acceptance.

Measured x64 layout: Windows client 816 bytes, WSL client 888; request 176,
public config 288, info 64, Core head 112 and body reader 120 on both platforms.
Private client size differs with native synchronization storage; public layouts
stay stable for the selected ABI. Extension request/call/result structures alone
request 176+88+32=296 bytes before wire and Core Future/cancel/timer/buffer costs.
Default receive scratch reserves 128 KiB per client, plus two 100-field arrays,
copied Host/Target, lazy payload growth, request metadata/wire and Core storage.
This is a reported cost, not high-connection-count memory acceptance. Complete
JSON validation/copy still runs on the I/O owner within body/JSON caps; larger
payload tail latency and worker-fault/OOM injection remain further acceptance work.

HTTP-client closure size is text/data/bss/logical-file bytes without stripping:

| Platform | Complete-closure single object | Product-only static | Product-only shared |
| --- | --- | --- | --- |
| Windows x64 | 590652 / 2672 / 11136 / 857270 | 39732 / 0 / 0 / 62718 | 80789 / 224 / 2848 / 138455 |
| WSL x64 | 611713 / 5664 / 10940 / 1086112 | 40210 / 128 / 0 / 82970 | 50959 / 1464 / 8 / 67680 |

The single object includes the Core closure; separate product artifacts exclude
host XRT. Those are not comparable deployment totals or physical NTFS block usage.
Requests explicitly advertise Accept-Encoding: identity. RFC 9110 section 12.5.3
allows any coding when that field is absent, so omission would not correctly
advertise this initial client's capability. The peer asserts this header, and
unsupported/duplicate response content codings remain rejected. Final tests,
packages and measurements use this policy; earlier pre-policy receipts are
historical development snapshots.


Performance is measured separately at one warmup plus five runs per workload.
The three call-only lifecycle modes each complete 200000 cycles and a separate
10000-sample latency phase. They remeasure the private-origin refactor without
queue, timer scheduling or network work. The HTTP profile offers 2000 requests
at fixed 2000/s absolute timestamps, retaining every offer/refusal/outcome and
measuring observed latency from planned arrival, including generator lateness
and Future polling. It is an offered-load observation, not a peak capacity test.
In every HTTP sample on both platforms: offered=accepted=completed=latency
samples=2000, rejected=failed=0. Final close/join is outside timed arrivals.

| Platform | Lifecycle | Calls/s | P50 us | P99 us |
| --- | --- | --- | --- | --- |
| windows | reply | 1582688 | 0.700 | 1.000 |
| windows | cancel | 1153707 | 0.900 | 1.200 |
| windows | immediate timeout | 1496662 | 0.700 | 1.000 |
| wsl | reply | 780385 | 1.200 | 1.700 |
| wsl | cancel | 827679 | 0.900 | 1.900 |
| wsl | immediate timeout | 1358821 | 0.700 | 1.200 |

| Platform | Completed/s at 2000 offers/s | All-offer observed P50 us | All-offer observed P99 us | Offer-lateness P99 us |
| --- | --- | --- | --- | --- |
| windows | 1958.295 | 22772.800 | 30763.200 | 15652.900 |
| wsl | 1997.566 | 1642.804 | 2185.117 | 1062.718 |

The Windows report includes 15.653 ms P99 offer lateness, while WSL includes
1.063 ms. These observations include the generator's Sleep/polling delays; they
cannot be presented as the RPC service's own tail latency. A more precise pinned
arrival/observation driver, larger payloads, saturation and high connection counts
remain necessary. None of these observations freeze an absolute performance target.

Windows has six failing metric/noise checks; WSL fourteen, under unchanged 3%
relative MAD and 6% central-range limits. Both runners exit 1. All samples remain,
no thresholds are relaxed and no generator/timing delay is subtracted. Counter
comparisons correctly treat rejected/failed as lower-is-better. There is no stable
regression baseline, equivalent external comparator, native Linux run or final
performance/production acceptance. Prior noisy lifecycle medians are not used as
a valid regression comparator.

Matching source fingerprint: `02f956a2ee1e6e9709af6e9bc8db1f98b0f9b26bd1243db116538870c8cc15ae`.
Workload fingerprint: `dfe5fc92d924b273f2b60449d3c359afa4ac0b75debbf499f0202dcf5efc63fa`.

`validation.json` checks current frozen inputs, all 40 actual test binaries,
private overflow/layout outputs, qualified TSan, complete/minimal consumers,
backend executables and packaged Core identities, declaration/layout/size and
four workload binaries/counts/fingerprints. The final receipt binds clean committed
HEAD/tree. Compiler/command/binary hashes and raw samples stay in the ignored
evidence directory; the feature assets and reproduction tools remain tracked.

Next work retains the full P3/P4 scope: notification/batch networking and richer
transport metadata, DNS/TLS/recovery/pooling, owned server requests and bounded
executor dispatch, WebSocket/framed stdio and negotiated extensions. Worker OOM/
fault injection, large payloads, high connection counts and retained-result quotas
still require coverage. Independent peers, native pinned Windows/Linux performance
and at least 72-hour mixed/fault runs remain unaccepted. Redis dedicated/HA/tracking,
MQTT client/session/persistence and Cron time-zone/scheduler/recovery remain active.
This commit completes neither xrpc nor the four-product objective.

## P3: atomic HTTP batches and notification completion

This phase extends the existing numeric plain-TCP HTTP client with notification
and array-shaped batch submissions. It retains one active exchange, bounded
cross-thread admission and one whole-document deadline/cancel unit. This is a
product-local addition; no Core runtime/API or shared service runtime is added.

Batch futures always hold xrpcbatchcallresult, including stop/failure, with one
owned snapshot and a request-order index map. Notifications map to SIZE_MAX;
non-success maps contain SIZE_MAX throughout. The complete document and every
call id are checked before any snapshot is published. Response order is arbitrary;
equivalent escaped decimal string ids are accepted. A serial range plus membership
flags gives direct bounded indexing without parsing an opaque id-key format.

An empty successful HTTP notification exchange yields NOTIFY_DONE and
MAY_HAVE_EXECUTED, not business confirmation. A nonempty 2xx body is rejected by
this local binding; non-2xx yields HTTP_ERROR with an optional explicit HTTP
status diagnostic. All-notification batches use the same policy. There is no
per-item cancellation, replay, new business callback or per-item Future.

Batch-only storage adds a preallocated result header/map plus one reserved
metadata/workspace block reused for outgoing encoding and response decoding.
MaxBatchItems and MaxBatchBytesTotal bound admission separately from wire limits;
Info.BatchBytes tracks actual reservation through construction/captured tails.
Retained maps/snapshots are still caller-owned outside pending admission; final
aggregate retained-result admission remains required. Ordinary result layout
remains unchanged. Actual layouts and performance are recorded below after
verification, without a zero-overhead or production claim.

Preceding commit 3507e4e2's available test/package/probe/performance receipts and
binaries are preserved in `out/service-extensions/xrpc-http-client/before-http-batch/`.
Its `archive.json` binds 1294 files by SHA-256, including the actual packaged Core
libraries and runtime package dependencies. The current phase evidence uses
`out/service-extensions/xrpc-http-batch/`.


Local source verification completed before package generation: Windows GCC 16.2.0
O2/Werror and WSL GCC 15.2.0 O2 ASan/UBSan each pass ten modular and ten direct
single-header binaries (40 total), eleven runnable examples and trim validation.
The HTTP suite adds mixed concurrent ordinary/batch/notification producers while
retaining callback-reentry and physical-close checks. New cases include reversed
and escaped ids, per-item RPC errors, single-item/all-notification arrays,
duplicate/missing/unknown/notification ids, nonarray/malformed tails, notification
HTTP outcomes, batch-byte/item refusal, whole-batch stops and no replay. It sweeps
4096 HTTP batch request-construction budget prefixes in addition to the existing
ordinary/factory checks. Native call tests add 2048 batch-constructor prefixes,
invalid/incomplete/duplicate maps and preallocated completion under one-byte quota.

Separate private probes pass on Windows and WSL ASan/UBSan. They force snapshot
and producer reference overflow, reject result-map aliasing, then retry completion
successfully. Actual x64 sizes: ordinary producer 88, config 40 and result 32;
batch result header 48, followed by Count * 8 bytes. No claim of no extra runtime
branches or zero overhead follows from unchanged ordinary storage sizes.

API documentation/features/amalgamation checks pass with 56 functions, 124
constants and 57 types. Normal maturity checks pass: 16 modules, 11 implementation
nodes with registered verification assets, 25 benchmark workloads and ten size
profiles. The product remains developing=1; strict release exits 1 as required.
This is not a final production candidate.


Complete and HTTP-only static/shared package consumers pass on both platforms;
the minimal consumer runs the mixed batch example. Declaration-only compilation
and actual private HTTP layout probes also pass. Packaged Core engine probes
report IOCP on Windows and epoll on WSL; their receipts bind the actual host
Core static library hash, compiler, source and executable, rather than inferring
the backend from link flags.

| Platform | HTTP client | HTTP request | Config | Info | HTTP head/body |
| --- | --- | --- | --- | --- | --- |
| windows | 840 | 176 | 304 | 72 | 112/120 |
| wsl | 912 | 176 | 304 | 72 | 112/120 |

The ordinary HTTP request remains 176 bytes. Batch-only workspace is separately
allocated/admitted; config and accounting structures expand deliberately under
their Size contracts. Whole-client buffers and common branching still carry
costs even for ordinary calls.

| Platform | Closure kind | text | data | bss | Logical file bytes |
| --- | --- | --- | --- | --- | --- |
| windows | single | 595960 | 2672 | 11136 | 863433 |
| windows | static | 45036 | 0 | 0 | 69632 |
| windows | shared | 86328 | 224 | 2848 | 144591 |
| wsl | single | 617767 | 5664 | 10940 | 1094152 |
| wsl | static | 45633 | 128 | 0 | 90688 |
| wsl | shared | 56707 | 1480 | 8 | 76272 |

Size rows retain the existing ownership boundary: the single object includes
its selected Core closure; static/shared product rows exclude the separate host
Core library. File bytes are logical artifact sizes, independent of NTFS storage
compression. These are footprint observations, not performance acceptance.


The qualified WSL O1 TSan HTTP suite passes, including concurrent ordinary,
batch and notification submitters, cancellation and close/reentry. GCC's retained
atomic_thread_fence modeling warning limits this evidence; it does not prove
all race freedom or native Linux qualification.

Seven workloads run at one warmup plus five measured samples per platform. Three
native lifecycle workloads complete 200000 cycles and use separate 10000-sample
latency loops. Four HTTP workloads offer 2000 whole HTTP operations at absolute
2000 operations/s arrivals: ordinary call, notification and batches of 16/64.
Every HTTP sample accepts/completes all 2000 offers, with zero rejected/failed;
this is a finite arrival workload, not a saturation/maximum-capacity result.
Notifications count completed HTTP exchanges, never confirmed business effects.

| Platform | Lifecycle | Calls/s | P50 us | P99 us |
| --- | --- | --- | --- | --- |
| windows | matched reply | 1306129 | 0.700 | 1.300 |
| windows | cancel | 957647 | 0.900 | 1.600 |
| windows | immediate timeout | 1415145 | 0.700 | 1.200 |
| wsl | matched reply | 1286726 | 0.700 | 1.300 |
| wsl | cancel | 888904 | 0.900 | 1.500 |
| wsl | immediate timeout | 1326848 | 0.700 | 1.200 |

| Platform | HTTP operation | Completed operations/s | All-offer observed P50 us | All-offer observed P99 us | Offer-lateness P99 us |
| --- | --- | --- | --- | --- | --- |
| windows | ordinary | 1958.676 | 22779.800 | 30820.900 | 15507.400 |
| windows | notification | 1964.361 | 22745.400 | 30821.700 | 15434.700 |
| windows | batch 16 | 1958.051 | 18860.500 | 27442.200 | 14828.100 |
| windows | batch 64 | 1956.960 | 13385.900 | 23035.000 | 13299.300 |
| wsl | ordinary | 1997.751 | 1659.311 | 2200.306 | 1067.314 |
| wsl | notification | 1998.723 | 1654.912 | 2198.103 | 1066.412 |
| wsl | batch 16 | 1997.366 | 1694.293 | 2260.895 | 1063.194 |
| wsl | batch 64 | 1997.226 | 1891.400 | 2556.538 | 1063.804 |

Batch rates/latencies count HTTP documents, not individual items. All-offer
latencies include generator Sleep/polling delays, queueing, construction,
encoding, transport, parsing, result reads and destruction. Offer lateness stays
in the observations; it is not subtracted. These medians do not establish an RPC
service-only P99 or comparative speedup between operation types. A precise pinned
arrival driver, saturation, large payloads, high connection counts and equivalent
external comparators remain necessary.

Windows has seven failing metric/noise checks and WSL eleven under unchanged 3%
relative MAD and 6% central-range thresholds; both measurement runners exit 1.
No noisy baseline is promoted for the later 5% throughput/10% P99 regression gates.
No source/performance threshold is weakened and no samples are discarded.

Matching source fingerprint: `e83b95b41f35e3d5098d2938ab86e33822452bbd1fcef2cb78fe843bb5af84e0`.
Workload fingerprint: `dc47c8bccb1bc9eedc9d72c13631982a7e8a4fc9e586d951f35b176d45a9e2ef`.

The current validation receipt checks frozen inputs, all 40 actual test binaries,
private overflow/layout outputs, qualified TSan, complete/minimal consumers,
actual packaged Core identities/backends, declarations/footprint and all seven
workload binaries/counts/fingerprints. The final receipt binds clean committed
HEAD/tree. Raw samples, commands, compiler/artifact hashes remain in the ignored
phase evidence folder; registered tests/examples/benchmarks remain tracked.

Next xrpc work retains richer HTTP metadata and aggregate retained-result
admission, DNS/TLS/recovery/pooling, owned server requests and bounded executor
dispatch, WebSocket/framed stdio, bidirectional calls and negotiated extensions.
Worker fault/OOM, independent interoperability, pinned native Windows/Linux
performance and at least 72-hour mixed/fault runs remain unaccepted. Redis
HA/tracking/dedicated modes, MQTT client/session/persistence and Cron IANA zones,
scheduler/durable recovery remain active under the original scope. This commit
completes neither xrpc nor the four-product objective.


## P3: owned requests and bounded server execution

The preceding 7e6b4378 phase is progress: it committed and verified atomic HTTP
batches and notification completion. Its available receipts, binaries, actual
Core/package dependencies and size/performance assets (1301 SHA-256-bound files)
are archived at `out/service-extensions/xrpc-http-batch/before-server/` before
shared test/package/size locations are rebuilt. This phase uses
`out/service-extensions/xrpc-server/` and preserves the full four-product goal.

The server execution component owns a bounded Core executor. Submit copies a
whole input document; a worker fully validates it through existing Dispatch
before invoking synchronous business handlers. Each document/batch is one job,
admission, deadline and cooperative cancellation unit. Jobs own their native
Future producer edge; callbacks/contexts remain opaque and are not collector
admission. No Core API or shared service runtime is added. Network server adapters
will reuse this path; this commit does not itself open HTTP/WebSocket/stdio
listeners or provide deferred business replies.

One result block owns its header and encoded response capacity, transferred into
the result Future on successful publication. Typed local failures discard partial
wire output but preserve actual handler-entry/dispatch partial-effects accounting.
DONE includes valid protocol/remote error responses; notification-only documents
have no wire response. Cancellation cannot roll back or forcibly interrupt user
handlers. Deadline starts at public submission entry, sampled around handlers
and output; queued stop completes on worker start or queued-job cancellation/drop.
A checked Core publication refusal is counted separately and requires consumers
to inspect Future state as well as a typed value.

Aggregate bounds cover live construction/work/drop duties, copied request/job
bytes, pending AND retained result count/header/capacity bytes, response limits
and preallocated per-worker decode scratch. Growth reserves the complete new
result block alongside the old one before realloc, so allocate/copy/free peaks
are included. Failure refunds new credit; success releases old credit. The
accounting boundary is requested private storage, excluding allocator metadata,
rounding, diagnostics and Core/executor/thread/user-code internal allocations;
finite operation/result/thread/queue limits independently bound their counts.
An O(1) protected free list lends exclusive preallocated scratch slots. Retained
results keep a separate quota ledger without pinning server/worker storage.

Close drains; Cancel requests the parent token and drops queued work outside
native locks. Wait closes admission and uses a single remaining timeout across
executor and constructor/drop-tail drain. Destroy additionally joins workers,
requires stopped concurrent native entries and refuses callback/self destruction.
Retained results survive it. Public tests verify eventual zero allocation-budget
bytes after the final retained result releases the quota ledger.

Initial Windows/WSL prototypes pass, including mixed documents, owned input,
raw huge ids, no handlers on malformed tails, result-ledger lifetime, old-plus-new
realloc quota, worker output OOM, constructor prefixes, cancellation/drop/reentry,
self-wait refusal and concurrent submission. A development test value limit was
corrected from four to three: the four-value request was valid at its exact bound.
That fixture correction is not a relaxed production limit or acceptance gate.


Windows GCC O2/Werror and WSL GCC O2 ASan/UBSan each pass eleven modular
and eleven direct single-header tests (44 actual binaries), twelve runnable
examples and trim validation. The new tests include 2048 request-construction
budget prefixes, separate factory thresholds, worker response realloc OOM,
aggregate old-plus-new growth rejection after the first accepted batch reply,
input/decode/output limits, mixed raw-id/error/notification documents, copied
input overwritten on return, four concurrent submitting threads and zero
allocation-budget bytes after a retained response outlives server destruction.
Controlled blocking handlers verify no forced interruption, queue cancellation,
deadline precedence, cancellation/completion reentry, self-wait/destroy refusal,
ambient-error preservation and physical duty accounting. These are deterministic
lifetime fixtures, not an independent transport peer or a production load run.

Generated feature/API/amalgamation checks and normal maturity pass with 64
functions, 139 constants and 66 types; 18 registered modules, 12 implementation
nodes, 27 benchmark workloads and eleven size profiles. The product remains
developing=1 (not a complete server/client); strict release still exits 1.


Complete and execution-only static/shared package consumers pass on Windows
and WSL. The complete consumer now executes the server path against the actual
packaged host Core and reads an exact raw-id response after physical server join;
the minimal consumer executes the offline mixed-batch example. Declaration-only
compilation, layout and all three footprint kinds pass. Actual packaged Core
backend probes still report IOCP and epoll; receipts bind library/compiler/source
and executable hashes rather than infer backends from link flags.

| Platform | Server | Scratch slot | Job header | Result block header | Quota ledger | Config | Info/result |
| --- | --- | --- | --- | --- | --- | --- | --- |
| windows | 312 | 16 | 152 | 112 | 144 | 272 | 96/96 |
| wsl | 312 | 16 | 152 | 112 | 280 | 272 | 96/96 |

Default private scratch is two workers * 256 descriptors * sizeof(xrpcmessage),
plus server/slot/ledger headers. Per request, the job header and input use one
allocation/copy; the result header and 256 initial output bytes use another
zero-initialized block. Promise/Future/cancel state, producer binding, executor
queueing, quota locks/refcounts and completion wakeups add costs. Dispatch/encoder
preflight plus EncodeWrite validate/scan output more than once; growth may copy
the old result header and capacity. Retained output is not a zero-copy path.
Creation also zero-initializes private scratch. No zero-overhead claim follows.

| Platform | Closure kind | text | data | bss | Logical file bytes |
| --- | --- | --- | --- | --- | --- |
| windows | single | 334080 | 2712 | 11136 | 511246 |
| windows | static | 27000 | 0 | 0 | 41520 |
| windows | shared | 38598 | 144 | 272 | 80917 |
| wsl | single | 348298 | 4400 | 10948 | 633504 |
| wsl | static | 27742 | 32 | 0 | 56042 |
| wsl | shared | 35138 | 1088 | 8 | 48680 |

The single object includes the selected Core closure; static/shared product rows
exclude the separate host Core library and OS thread stacks. File sizes are
logical bytes, independent of NTFS compression.

The qualified WSL O1 TSan execution suite passes, including concurrent producers,
queued cancellation/drop and self-wait/reentry. GCC atomic_thread_fence modeling
warnings remain in build evidence; this is not proof of all race freedom or a
native Linux qualification.

Two server workloads run at one warmup plus five measured samples per platform.
Each throughput sample completes 20000 documents with a 64-document submission
window and two executor workers. A separate one-in-flight phase measures 10000
latency samples. Both ordinary and batch-16 paths validate returned status/counts
and observed encoded result values, then destroy result Futures. The work includes
request copy, full decode/dispatch/encode, queue/Future/result and destruction;
server creation and final physical join are outside timed work. Windowed
throughput and one-in-flight latency use DIFFERENT concurrency. These are
in-process document measurements, not network RPC capacity, per-item batch
latency or an external comparator result.

| Platform | Document | Windowed documents/s | Single-in-flight P50 us | Single-in-flight P99 us |
| --- | --- | --- | --- | --- |
| windows | ordinary | 427758.992 | 10.900 | 23.400 |
| windows | batch 16 | 73005.517 | 27.600 | 51.800 |
| wsl | ordinary | 319890.981 | 27.300 | 63.300 |
| wsl | batch 16 | 76736.600 | 58.200 | 103.200 |

Windows has three failing metric/noise checks and WSL four under unchanged 3%
relative MAD / 6% central-range thresholds. Both runners exit 1; all samples are
retained. Neither report is a qualified performance baseline. The subsequent
5% throughput/10% P99 regression gates remain unchanged, and no acceptance is
inferred from these unpinned medians. Saturation/open-arrival behavior, payloads,
connection counts, equivalent peers and pinned native Windows/Linux performance
remain required.

Matching source fingerprint: `7a03fa57512f2b428e3d5fd4a8d9a689c9c61ca3995d9bab336ad3fc91da58b2`.
Workload fingerprint: `f9fe2124a3e49d9142089cb3bad9eb1e747d936426c9172d8dc67e923df2f0eb`.

The current validation receipt verifies current frozen inputs, 44 test binaries,
qualified TSan, complete/minimal package consumers, declaration/layout/footprint,
actual Core backend identities and both workload binaries/counts/fingerprints.
The final receipt binds clean committed HEAD/tree. Full commands/compiler hashes,
raw samples and artifact hashes stay in the ignored phase evidence folder.
Tracked production/tests/examples/profiles and standard root tools reproduce the
feature; the product still fails strict release as developing.

Next xrpc work is HTTP server listener/peer ownership, bounded worker completion
handoff and backpressure/close integration with this executor. Richer HTTP client
metadata, retained client-result quotas, DNS/TLS/recovery/pooling, WebSocket/framed
stdio, bidirectional calls, negotiated extensions and deferred business-handler
lifetime remain pending. Broad worker faults/OOM, independent interoperability,
native pinned Windows/Linux and at least 72-hour mixed/fault runs remain
unaccepted. Redis HA/tracking/dedicated modes, MQTT client/session/persistence and
Cron IANA zones/scheduler/durable recovery remain active under the original goal.
This commit completes neither xrpc nor the four-product production objective.


## 2026-10-30 — bounded HTTP/1 server transport (local feature complete)

Added `xrpc_http_server`: eight public operations for configuration, creation,
start/local address, info, close/wait and destruction. This is numeric plain TCP
HTTP/1 with synchronous business handlers on the existing bounded server executor.
Create copies configuration/path, creates no listener and invokes no handlers;
Start binds synchronously and accepts asynchronously on Core-distributed workers.
An existing host engine is pinned through Destroy. No Core runtime was changed.

Each peer is owned by its stream worker. Whole request documents go to the
separate executor; native Future Watch completion posts an embedded continuation.
Every request has a distinct operation/watch/header/result owner, so foreign
publication/drop tails cannot race with reuse of embedded watch storage. Owned
listener/stream event data, timers, posts and send references retain actual
owners; opaque ownership graphs are refused rather than admitted as leaves.

HTTP head/body parsing, targets/Host, parameters, expectations and trailers use
public Core APIs. Binding policy is POST at a configured raw path, optional query,
valid origin/absolute http targets, unique Host for HTTP/1.1, application/json
with validated parameters/unique UTF-8 charset, and no unsupported content coding.
Framing/CL/TE, chunk syntax and actual/declared trailer policy are checked before
business dispatch. Core body syntax alone does not reject forbidden trailer
names: this adapter explicitly validates the complete trailer section as well.
Valid pending bodies can receive 100 Continue; unsupported expectations are 417.
RPC responses use 200/json; notification-only output is empty 204 without
Content-Length. Local HTTP failures are empty closing responses, never fabricated
RPC errors. Malformed JSON and invalid RPC documents follow dispatch semantics.

Connection/operation/private-buffer/global-send quotas are independent. Input and
body growth reserves old-plus-new block capacity; large copied staging is released
after executor submission and empty large input scratch is trimmed on reuse.
Responses use reference sends backed by the actual Future/header owners. Global
send credit uses worker waiting-writer queues and embedded shard kicks, with no
backpressure polling timer. Each connection preserves pipelined request order,
processes one exchange at a time, and has per-turn read/write byte budgets.
The monotonic request origin includes header/body waiting and buffered pipeline
bytes; a private entered-time helper passes the exact origin to execution without
restarting or millisecond subtraction. Timers cover parsing, execution and send.

Close stops accepts, posts peer aborts and requests cooperative cancellation.
Wait observes listener/peer/post/timer/watch/send plus executor publication/drop
retirement under one deadline. Network/own execution callbacks cannot block in
Wait/Destroy. Noncooperative handlers may delay drain; timeout preserves ownership
for retry. Destroy joins execution workers and releases the engine pin after
concurrent API entries have stopped. Core mechanical callback return tails retire
at host engine join. Retained client results survive full server/engine cleanup.

Defaults: loopback ephemeral `/rpc`, 1024 connections, 256 transport operations,
16 MiB private buffers, 8 MiB referenced sends, 2 MiB request wire/1 MiB decoded
body, 64 KiB read/write turns, 16 KiB send chunks, 30 s idle/request timeouts, plus
the executor's independent quotas. Private quota includes server/shards/path,
peer/descriptors/input/body and operation blocks; it is not an RSS limit. Core
metadata, allocator rounding/diagnostics, socket buffers and thread stacks are
separate and bounded by their configured counts/capacities. Business allocations
remain application responsibilities. Copies include stream-to-input, fragmented/
chunked body staging, executor input ownership and JSON output assembly; reference
sending removes a second application payload send copy, not every end-to-end copy.

Validation used frozen source/header/manifest hashes. Windows native GCC O2/Werror
and WSL GCC O2 ASan/UBSan passed 12 modular plus 12 direct-single tests each (48
binaries), 13 runnable examples each, and trim validation. New coverage includes
real ordinary/remote-error/notification/mixed-batch traffic; byte fragmentation;
large exact ids; HTTP/1.0; absolute targets/queries/charset; chunks/trailers/100;
ordered pipelines; document errors/notification 204; pre-business head/body/wire/
trailer limits; connection/operation/private/executor refusal; eight peers with
only 25 global queued bytes; incomplete-header expiry; body time included in
execution; cooperative shutdown; noncooperative wait timeout; self-wait/destroy
refusal; eight constructor quota thresholds with zero live bytes; duplicate bind
failure then successful retry; and retained-response/full retirement accounting.
The bind test preserves platform defaults instead of forcing the Windows-only
exclusive-address option on Linux. Benchmark compilation required an explicit
memory header; this benchmark-only change did not alter product/test inputs.
The exact changed-file refreshes are preserved in the phase receipts.

Independent Python stdlib HTTP/socket peers passed 405 exchanges per platform,
including eight concurrent persistent connections, chunked requests, UTF-8/NUL
and large integer/string ids, notifications/mixed batches and incomplete-body EOF.
The tracked `tests/interop/http_server.py` accepts the runnable example binary;
this is independent HTTP/JSON framing interoperability, not JSON-RPC-library parity.

Both platforms passed complete and minimal static/shared packages, declaration
compilation, private/public layout and all three size kinds. Actual linked Core
libraries reported IOCP on Windows and epoll on WSL. The qualified WSL O1 TSan
suite passed with no runtime race report; the GCC atomic_thread_fence modeling
warning remains in the build log, so this is not proof of universal race freedom.
Twenty-one extension layout/single/header-coverage tool tests also passed.
Normal maturity audit reports 20 modules, 13 implementation nodes (12 C source
files), all 13 covered by tests/single/docs, 30 benchmark workloads, 12 size
profiles, and developing=1. Strict release fails only on unfinished `xrpc`.
Public API reference now records 72 functions, 149 constants/macros and 71 types.

Private/public x64 sizes (bytes): Windows HTTP server 912, WSL 1048; both peer 392,
operation 464, shard 112, config 568, info 216. The different server size includes
platform mutex/condition storage. Server allocation additionally includes one
shard per network worker and the copied route; peer allocation additionally
includes configured field/trailer descriptors and input/body capacities.

| Platform | Kind | Text | Data | BSS | Logical file bytes |
| --- | --- | ---: | ---: | ---: | ---: |
| windows | single | 669520 | 2712 | 11168 | 964623 |
| windows | static | 45764 | 0 | 0 | 72160 |
| windows | shared | 89223 | 224 | 2848 | 153619 |
| wsl | single | 690758 | 6368 | 10948 | 1220960 |
| wsl | static | 47121 | 208 | 0 | 95402 |
| wsl | shared | 62414 | 1832 | 8 | 82608 |

Single size includes the Core closure; static/shared rows are the product artifact,
with Core supplied separately. These are unstripped GCC O2 sizes, not a direct-C
comparator or a zero-overhead claim.

Network performance used one persistent production client/server connection,
round-robin network workers and a separate executor in the same host engine.
Each ordinary/notification/batch16 workload ran one warmup plus five measured
runs, 2000 fixed planned arrivals at 2000 HTTP documents/s per run. All measured
runs on both platforms accepted/completed every offer with zero rejected/failed
operations and retained all 2000 latency samples. Both noise checks passed with
zero failures under the unchanged 3% relative MAD/6% central-range policy.

| Platform | Workload | Completed documents/s | All-offer P99 us | Offer-lateness P99 us |
| --- | --- | ---: | ---: | ---: |
| windows | ordinary | 1954.708 | 30833.400 | 15477.600 |
| windows | notification | 1960.977 | 31029.800 | 15505.700 |
| windows | batch16 | 1961.658 | 28592.100 | 14975.300 |
| wsl | ordinary | 1997.933 | 2182.402 | 1060.907 |
| wsl | notification | 1997.488 | 2200.671 | 1065.687 |
| wsl | batch16 | 1998.160 | 2225.902 | 1058.903 |

These are observed end-to-end fixed-arrival composition measurements, not saturation
capacity or isolated server latency. Samples include construction/encoding,
queueing, actual RPC/network work, Future polling and coordinator scheduling
lateness; the Windows offer-lateness numbers show the substantial harness timer
contribution. Batch throughput counts HTTP documents, not individual RPC items.
No production baseline was pinned or performance gate relaxed. Representative
loads, native pinned Linux, direct comparators and tail-latency capacity/overload
runs remain required; passing this local noise check is not final P4 acceptance.
Source fingerprint: `38ed11017a085c034c5040048d49c75c74e6df0528846802a95d70994c039880`.
Workload fingerprint: `5b4609ac0e995e201021d6387cfbf1660da6df5a46fedcb2d07f86f44d7620b8`.

Before overwrites, 1161 SHA-bound predecessor artifacts were archived under
`out/service-extensions/xrpc-server/before-http-server/`. Current exact-input test,
package/backend/layout/size/TSan/interop/performance evidence and 29 additional
artifact hashes are bound by `out/service-extensions/xrpc-http-server/validation.json`.
The committed validation rerun binds the final clean HEAD/tree; raw commands,
compiler/binary/library hashes and samples remain in the ignored evidence folder.

Next xrpc work includes aggregate retained client-result admission, richer HTTP
metadata, DNS/TLS/authentication/recovery/pooling, WebSocket/framed stdio,
bidirectional calls, negotiated extensions and deferred handler lifetime. Broader
worker faults/OOM and at least 72-hour native Windows/Linux mixed/fault runs remain
unaccepted. Redis HA/tracking/dedicated modes, MQTT 5.0/3.1.1 client/session/store,
and Cron IANA/scheduler/durable recovery remain under the original active goal.
This feature commit completes neither xrpc nor the four-product production goal.


## 2026-10-30 — aggregate retained HTTP client result quotas (local feature complete)

Previously, HTTP request count/wire/batch limits ended with request retirement,
while caller-held typed Futures/maps and independently retained response snapshots
could continue growing. The client now owns a separate result-storage ledger;
`MaxResultBlocks` and `MaxResultBytes` cover pending and retained allocations.
Defaults are 4096 blocks/8 MiB. A typed result header with its entire batch map is
one block; a response snapshot is another. Ordinary/batch replies normally hold
two blocks; notifications and local terminal outcomes hold one. Additional strong
references do not charge the same allocation again. Public config/info structures
expanded (rebuild consumers with matching headers/libraries); layouts remain
independent of feature selection, not a promise of mixed-version ABI stability.

Header credit is required during construction/admission. Exhaustion returns
OVERLOADED without publishing a Future. Snapshot size becomes known after complete
document validation; later snapshot credit failure becomes LIMIT and retires the
connection without replay or partial batch publication. ResultRejected counts
refused header/snapshot reservations; Rejected retains admission-refusal meaning.
Info request/result snapshots are separate instants. Cancellation, observer drop
and Close cannot fabricate early credit while actual producers/readers still own
storage. A separately retained response keeps its own block after its Future/map
is gone, and remains valid after full client/engine destruction.

The product-private `xrpc_result_quota` module uses public Core memory/mutex/atomic
APIs. There is no new shared runtime or Core change. Each charged block's aligned
prefix holds its actual byte charge and one real ledger reference, replacing
its ordinary allocation rather than adding another allocation per result. A
mutex protects bounded counters; malloc/free happen outside it. Physical free
precedes refund. One ledger allocation is added per client; it keeps no client,
engine, worker or stream reference. Last block release can occur on any thread.
Future ownership still refuses opaque native graphs. Product code must stay
loaded until all destructor calls finish.

Raw standalone response/native-call selections do not pull the optional quota
locking module. Their allocation layouts and payload-copy rules remain intact;
HTTP charged storage adds prefix/bookkeeping. Quota bytes include headers/maps,
snapshot descriptors/raw-ID views/wire and private prefixes. Per-snapshot
ResponsesInfo.OwnedBytes excludes that transport prefix. Allocator metadata/
rounding/caches, diagnostic/Future/native request metadata, receive staging,
socket buffers and thread stacks are separate; this is not an RSS ceiling.
HTTP submission still allocates request bookkeeping before result credit, so
whole-API overload is not claimed zero-allocation. Exact-ledger refusal itself
performs no malloc. No bridge/end-to-end zero-overhead claim follows.

Validation passed 12 modular plus 12 direct single-header binaries on each of
Windows native GCC O2/Werror and WSL O2 ASan/UBSan (48 binaries), 14 runnable
examples each, and positive/negative trim checks. Existing call/response/server/
HTTP protocol/lifetime checks remain included. New coverage verifies pending
headers, retained Futures/maps/snapshots, unchanged output/ambient error on credit
refusal, count/byte bounds, returned-credit reuse, all-notification maps, later
snapshot LIMIT/connection retirement, independent snapshot after full engine
cleanup, constructor-budget lifetime returning to zero after last snapshot,
four parallel final-release threads, direct snapshot OOM/refund/reuse, and the
existing 4096 call/batch constructor allocation sweeps.

Trim initially identified missing dependency diagnostics for the optional helper;
public response-header guards now reject missing response/atomic/mutex support.
Both runtime/single/example suites and trim were rerun after that header change.
A required helper size profile was added after runtime tests; the exact metadata
refresh records that no compiler source/header/manifest/definitions changed.
Memory-peak metric directions were corrected before any performance execution;
counts, arguments, compiler flags and noise/regression gates were unchanged.
All metadata-only refreshes are preserved separately in the phase receipts.

Both platforms passed complete xrpc and minimal HTTP-client static/shared package
consumers, including actual quota refusal/release/reuse traffic in the minimal
consumer, declaration compilation, layouts, and all three size kinds for HTTP
client and quota helper. Actual Core backend probes report IOCP/epoll. Qualified
WSL O1 TSan passed the complete HTTP-client suite with no runtime race report;
GCC's atomic_thread_fence modeling warning stays in the log. Twenty-one extension
layout/single/header-coverage tool tests passed. Independent Python HTTP/JSON
peers completed 405 exchanges per platform, including eight persistent concurrent
connections and truncated-body EOF; this is not independent JSON-RPC-library parity.

Normal maturity reports 21 modules, 14 implementation nodes (13 C files), all 14
covered by tests/single/docs, 31 workloads and 13 size profiles. Public reference
has 72 functions, 150 constants/macros and 71 types. Strict release still fails
only because xrpc is developing, as required by the full outstanding scope.

x64 requested structure sizes: HTTP client Windows 888/WSL 960; ledger 72/144;
both request 176, aligned prefix 32, native call 88, snapshot header 40, public
config 320 and info 96 bytes. Platform mutex storage explains the ledger/client
difference. Typed ordinary result storage adds a 32-byte prefix to its existing
32-byte public header; batch headers additionally include all indices. An
independent response charges its own prefix until its last strong ref leaves.

| Platform | Profile | Kind | Text | Data | BSS | Logical file bytes |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| windows | xrpc_http_client | single | 598064 | 2672 | 11136 | 866429 |
| windows | xrpc_http_client | static | 46892 | 0 | 0 | 73924 |
| windows | xrpc_http_client | shared | 88316 | 224 | 2848 | 146838 |
| windows | xrpc_result_quota | single | 230052 | 2664 | 11040 | 350943 |
| windows | xrpc_result_quota | static | 10060 | 0 | 0 | 16534 |
| windows | xrpc_result_quota | shared | 19451 | 144 | 272 | 55243 |
| wsl | xrpc_http_client | single | 619653 | 5664 | 10940 | 1097520 |
| wsl | xrpc_http_client | static | 47765 | 128 | 0 | 96392 |
| wsl | xrpc_http_client | shared | 58627 | 1472 | 8 | 80768 |
| wsl | xrpc_result_quota | single | 240426 | 3280 | 10924 | 426264 |
| wsl | xrpc_result_quota | static | 10428 | 0 | 0 | 22492 |
| wsl | xrpc_result_quota | shared | 14065 | 744 | 8 | 25536 |

Single includes its Core closure. Static/shared rows are the extension artifact,
with Core supplied separately. The quota helper includes its actual response/
mutex/atomic dependency closure, not a fictional bare allocator footprint.
All rows are unstripped GCC O2, not a pinned equivalent-C performance comparison.

Performance ran one warmup plus five measured runs per workload. Real HTTP
ordinary/notification/batch16 used unchanged 2000 planned arrivals at 2000 documents/s,
a persistent production client/server connection in one host engine, and separate
execution workers. Each platform accepted/completed all 2000 offers per measured
run with no failed/rejected operations and all latency samples retained.

| Platform | Workload | Completed documents/s | All-offer P99 us | Offer-lateness P99 us |
| --- | --- | ---: | ---: | ---: |
| windows | ordinary | 1945.983 | 30896.800 | 15354.700 |
| windows | notification | 1962.732 | 30809.000 | 15335.100 |
| windows | batch16 | 1964.268 | 28406.400 | 14871.200 |
| wsl | ordinary | 1997.936 | 2210.460 | 1065.449 |
| wsl | notification | 1997.854 | 2204.461 | 1069.869 |
| wsl | batch16 | 1998.216 | 2263.593 | 1068.394 |

The dedicated private-ledger microbenchmark retains 64 256-byte payload blocks
and alternates exact-limit refusal with actual free/reallocation. Every measured
run offered 2000000 attempts, accepted 1000000, refused 1000000, peaked at 64 blocks/
18432 requested bytes and returned to zero blocks/bytes. Initial fill/last cleanup
and ledger creation/destruction are outside its timed loop; the loop includes
successful free/allocation and all refusals. It measures a warmed single-thread
ledger, not full HTTP admission, a contended workload, cold paths or RPC capacity.
Windows median successful allocations/s: 14218380.094; WSL: 13269111.365.

Windows passed unchanged 3% relative-MAD/6% central-range noise checks (runner 0).
WSL's ledger allocation rate failed both checks: 4.0% relative MAD and 6.8% central
range (runner 1); its three network workload checks passed. The rejected quality
result is retained without retrying to select favorable samples or relaxing any
gate. No baseline was pinned. HTTP observations include scheduled-offer lateness,
queueing and Future polling; Windows timer contributions remain substantial.
Fixed-arrival results are neither saturation capacity nor isolated server latency.
Native pinned Linux, representative loads/comparators, allocator/CPU/copy accounting,
contended/cold retained-pressure runs and final P4 remain pending.
Source fingerprint: `36a0272ec14cf219168d0be840364078b3b85fdae2005f2f957aa8f94402ae54`.
Workload fingerprint: `c860d2d4755608058a3021b4656d341ad286df3f9e4e5bf9a621bad4873d9083`.

Seventy SHA-bound shared predecessor artifacts were archived before overwrites at
`out/service-extensions/xrpc-http-server/before-client-quota/`. To free the nearly
full C drive, that immutable archive was moved to the task cache
`D:/CodexArtifacts/xrt-service-extensions/cee15759-before-client-quota` and a junction
preserves the original path; all seventy hashes were reverified through it. No
primary-checkout work or predecessor evidence was deleted. Current receipts at
`out/service-extensions/xrpc-http-client-quota/validation.json` bind 48 test binaries,
37 additional artifacts, actual backends, layouts/sizes/TSan/interop and raw 5+1
samples. The committed rerun binds clean HEAD/tree and exact current source inputs.

Next xrpc work remains DNS/TLS/authentication and richer HTTP metadata/recovery/
pooling, WebSocket/framed stdio, bidirectional calls, negotiated extensions and
deferred business handlers. Broad worker faults and at least 72-hour native
Windows/Linux mixed/fault runs remain unaccepted. Redis HA/tracking/dedicated
modes, MQTT 5.0/3.1.1 client/session/persistence and Cron IANA/scheduler/durable
recovery remain active under the original four-product goal. This storage feature
completes neither xrpc nor the whole production objective.

## P3: asynchronous HTTP DNS and prepared-address dialing (2026-10-30)

Base: `540a4a1e8283b9b2e9c5f93679434849571b2e1e`. Added three public
functions in `xrpc/http_dial.h`: config initialization, application-resolver DNS
dialing, and immutable prepared-endpoint dialing. Both paths reuse the production
HTTP client and native Core candidate controller. The HTTP authority/target stay
explicit and independent of the DNS name. Prepared endpoint ports are preserved,
with at most 64 entries. Defaults are dual stack, eight attempts, 250 ms fallback,
a 1024-byte DNS-name bound and a 30-second connection deadline. No IDNA conversion,
implicit TLS, cleartext fallback or application-request replay is introduced.

The monotonic connection deadline starts at factory entry and covers construction,
owner-worker admission, DNS, candidate racing and losing-resource retirement.
Zero is immediate asynchronous timeout; -1 disables this additional deadline.
Core Dial's separate relative timeout is disabled. Queued calls retain their own
API-entry deadlines and cancellation; an unsent stop leaves connection setup usable.
DNS/transport failure, admission limits, memory failure and timeout retain distinct
typed statuses and explicit immutable cause chains. The application retains the
resolver through Client Closed and retires/joins it separately; a noncooperative
lookup can delay physical close after logical cancellation.

This adds one pointer (eight requested bytes on these x64 builds) to the existing
HTTP client, plus one cold sidecar allocation containing source state and a copied,
NUL-terminated DNS name. Prepared dialing retains an immutable list until native
admission. Source references count actual client-owner and owned-callback credits;
opaque tracing is refused. The winning stream is published only after native
candidate retirement. Core's owned callback drop clears the source pointer and
releases cold source/Dial ownership immediately, instead of retaining it for an
open connection's lifetime. A budget-observation test verifies the requested-byte
decrease while the client stays open and zero after complete cleanup. The
additional absolute timer holds a real client reference and is canceled on the
owner worker. This is requested-storage accounting, not RSS or zero-overhead proof.

A review found a new completion-versus-Close race: OPEN publication could overwrite
concurrent CLOSING after an earlier unlocked state check. The tracked deterministic
`tests/interop/http_dial_close_race.py` probe interposes mutex calls only in its
test translation unit and links the other actual modular objects. Its preserved
pre-fix Windows run fails with `close request overwritten by dial completion`.
The publication now rechecks under the gate. Both final platform probes pass, and
both complete modular/single/example/trim runs were repeated after that source fix.
There is no production test hook. Pre-fix source/binary/receipt hashes remain in
`out/service-extensions/xrpc-http-dial/before-close-fix/`.

Local evidence: 13 modular and 13 single-header binaries per platform (52 total),
plus the two separate deterministic race-probe binaries. All 15 documented examples
per platform and trim checks pass. New tests cover copied-name reuse, owner-worker
queueing, ordinary/notification/mixed-batch traffic through setup, a reserved
non-listening endpoint followed by a working candidate, list-owner release,
request cancellation/deadlines, immediate/infinite/factory-entry setup deadlines,
close before admission, resolver admission exhaustion, lookup failure, invalid
policy/NUL names, seven constructor budget thresholds and early cold retirement.
Fast failure tests hold the owner worker until the caller has queued its request.
Retained response results remain valid after client/engine cleanup.

WSL runtime/single/example runs use ASan/UBSan with leak detection and halt-on-error.
Qualified WSL O1 TSan passes the HTTP-dial suite with no runtime race report; GCC's
atomic-thread-fence modeling warning is retained, not suppressed from the evidence.
Twenty-one extension layout/single/header-coverage tool tests pass. Complete xrpc
and minimal HTTP-dial static/shared consumers, declaration-only compilation and all
three dial size kinds pass on both platforms. Actual Core probes report IOCP/epoll.
Independent Python HTTP/JSON peers complete 405 server exchanges per platform,
including eight concurrent persistent connections; one additional C DNS client
talks to a stdlib HTTP peer per platform (810 + 2 exchanges). This is neither
independent JSON-RPC-library parity nor external DNS/TLS interoperability.

Normal maturity: 23 modules, 15 implementation nodes (14 C files), all 15 covered
by tests/single/docs, 32 workloads and 14 size profiles. Generated public reference:
75 functions, 151 constants/macros and 72 types. Read-only API/single checks pass.
Strict release still fails only on the intentionally developing xrpc aggregate.

Requested x64 structure sizes: client Windows 896/WSL 968; cold dial data 200
(including a 56-byte private source plan, excluding name tail); public dial config
48; unchanged HTTP config 320/info 96/request 176/native call 88/snapshot 40/prefix
32. Ledger remains Windows 72/WSL 144. Native Dial/resolver/candidate allocations,
engine timers, allocator headers, copying and synchronization are additional costs.

| Platform | Profile | Kind | Text | Data | BSS | Logical file bytes |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| windows | xrpc_http_dial | single | 655492 | 2672 | 11136 | 951230 |
| windows | xrpc_http_dial | static | 52648 | 0 | 0 | 83880 |
| windows | xrpc_http_dial | shared | 94732 | 224 | 2848 | 157442 |
| wsl | xrpc_http_dial | single | 677623 | 6432 | 10940 | 1206136 |
| wsl | xrpc_http_dial | static | 53722 | 184 | 0 | 109896 |
| wsl | xrpc_http_dial | shared | 65967 | 1616 | 8 | 86192 |

These are unstripped GCC O2 artifacts. Single includes its actual Core closure;
static/shared are the extension artifact, with Core supplied separately.

Each performance workload has one warmup plus five measured runs. The three
unchanged HTTP workloads schedule 2000 documents at 2000 planned arrivals/s,
accept/complete all 2000 with zero failed/rejected and retain all latency samples.
These observations include offer lateness/queueing/Future polling on this shared
host, not isolated server latency or saturation capacity.

| Platform | Workload | Completed documents/s | All-offer P99 us | Offer-lateness P99 us |
| --- | --- | ---: | ---: | ---: |
| windows | ordinary | 1961.514 | 30914.600 | 15371.400 |
| windows | notification | 1966.082 | 30929.000 | 15447.100 |
| windows | batch16 | 1960.286 | 28684.800 | 14877.200 |
| wsl | ordinary | 1998.142 | 2196.603 | 1063.406 |
| wsl | notification | 1997.405 | 2188.704 | 1063.407 |
| wsl | batch16 | 1997.405 | 2261.803 | 1064.303 |

The new cold benchmark performs 500 serial closed-loop client cycles per run,
using OS IPv4 localhost resolution, resolver cache disabled, then TCP/RPC/result
release and Client Closed observation. The existing engine/server/resolver are
outside its timed loop. Each run records all 500 cycles and exactly 500 submitted
lookups. Callback return tails/final engine join are outside per-cycle observation.
Windows median 1790.002 cycles/s, P50 534.900 us, P99 744.000 us; WSL 2662.138
cycles/s, P50 366.000 us, P99 558.900 us. This does not measure external DNS
capacity, fixed-arrival connection pressure, TLS or all final teardown costs.

Windows cold P50 fails both unchanged noise gates (3.496% relative MAD, 7.422%
central range versus 3%/6% limits); other Windows checks pass. All WSL noise checks
pass. Runner results 1/0 and all raw samples are retained without favorable-sample
retries or threshold changes. No baseline is pinned and performance is unaccepted.
Source fingerprint: `d5fa7458aaba941e381116d2733da1518cd751ba961b3d6490c89dedc1ee3a0a`.
Workload fingerprint: `b28f93b25d87e1d1169808a7951f179eb809de9c2fdaa4019dfa443f819c5bdf`.

Eighty-eight SHA-bound predecessor artifacts were archived before overwrites at
`out/service-extensions/xrpc-http-client-quota/before-http-dial/`, a junction to
`D:/CodexArtifacts/xrt-service-extensions/540a4a1e-before-http-dial`. No predecessor
evidence or primary-checkout work was deleted. Current receipts at
`out/service-extensions/xrpc-http-dial/validation.json` bind 52 runtime binaries,
35 additional artifacts, frozen exact source/tool inputs, race probes, layouts,
sizes, backends, TSan, interop and 5+1 raw samples. The committed validator rerun
binds clean HEAD/tree to the same tested inputs.

Next xrpc work remains TLS/authentication, richer HTTP metadata/recovery/pooling,
WebSocket/framed stdio, bidirectional calls, negotiated extensions and deferred
business handlers. Redis HA/tracking/dedicated modes, MQTT 5.0/3.1.1 full
client/session/persistence, Cron IANA/scheduler/durable recovery and original P4
remain active. Native Linux qualification, representative pinned comparators and
at least 72-hour native Windows/Linux mixed/fault runs remain unaccepted. This
connection feature completes neither xrpc nor the four-product production goal.

## P3: explicit HTTP client TLS and absolute readiness deadlines (2026-10-30)

Base: `e87c3c7d20e3cf6244ab008b86ed3285a20d8126`. Added an optional
`xrpc_http_tls` leaf with config initialization and numeric, DNS and prepared-list
TLS factories. Core owns certificate verification, cryptography, candidate racing
and the TLS stream. The adapter reuses the production HTTP client, response
correlation, deadlines, cancellation and retained-result quota. No new shared
runtime, TLS server completion, implicit cleartext fallback or request replay is
claimed. HTTP application authentication, mutual client authentication and managed
tickets remain separate work under the original scope.

DNS route, HTTP authority and TLS certificate identity are explicit inputs.
ServerName/VerifyName is mandatory and native verified-client construction rejects
missing trust (except explicit Core ResumeOnly policy). IP identities use VerifyName
without SNI. Names/ALPN are snapshotted and shared context/verifier/resume objects
are retained before return, allowing caller storage/verifier-reference reuse.
Verifier callback/context lifetime extends through final engine join. Default ALPN
is HTTP/1.1; empty ALPN is allowed, other offers are rejected before admission and
negotiated protocol is checked before readiness. Metadata has 64 KiB default/1 MiB
hard bounds; native verification/context/record/TCP limits remain additional bounds.

One API-entry connection deadline spans session construction, owner queueing, DNS,
TCP, candidate retirement and TLS readiness. Zero is immediate asynchronous timeout;
-1 disables it. The native handshake timer is an additional stage bound. The cold
connection plan remains until both physical TCP-Dial callback retirement and TLS
setup completion; timer cancellation owns a real client reference. Close observes
physical source and stream duties; the application owns/joins its resolver separately.
Public Close aborts, not a graceful close_notify delivery guarantee.

TLS sends accept short plaintext prefixes. A per-connection offset retains the
encoded request through complete acceptance or retirement, with no extra per-request
field. The first accepted byte marks MAY_HAVE_EXECUTED. Native send calls and accepted
bytes per worker turn are bounded by WorkRequests and WriteBytes (default 64 KiB,
hard 1 MiB). Backpressure waits for native writable events. Sent timeout/cancel
retires the connection and closes queued unsent work without replay. A valid early
response can complete its RPC, but an unsent suffix forces connection retirement.
TLS failure/truncated authentication is a transport failure, not HTTP EOF.

A targeted verifier gate reproduced a deadline bug before the fix: if verification
occupied the owner worker until after the absolute timer became due, TLS Open could
cancel that queued timer and publish a late ready connection. The Windows pre-fix
binary fails with `TLS readiness bypassed expired factory deadline`; source/object/
binary/receipt evidence is retained under `before-verification-deadline/`. TCP
delivery and TLS-ready publication now compare the absolute deadline directly.
The regression holds verification beyond the deadline and requires TIMEOUT/NOT_SENT
and zero peer RPCs. This is deliberate worker-fault injection, not a production
recommendation to block a verifier callback.

Final frozen-input runtime checks pass on both platforms: 14 modular and 14 single
binaries each (56 total), 16 examples each and trim. WSL uses ASan/UBSan with leak
detection and halt-on-error. New tests cover copied DNS/identity/ALPN storage,
shared-verifier release before admission, three endpoint forms and a reserved
non-listening candidate followed by a winner, small read/write work, ordinary/
notification/mixed-batch/remote-error traffic, trust rejection, total/handshake/
immediate deadlines, partial send cancel/timeout/close, unauthenticated truncation,
early replies, held DNS and owner queueing, API-entry expiry before DNS, creator
release during close, completion-watch reentry and independent retained results.
Nine constructor byte thresholds, after fixture/global warmup, cross success/failure
and return requested budget bytes to zero after full cleanup. They are not exhaustive
allocation-site or cold-start fault coverage. Invalid policies preserve outputs and
cannot bypass native certificate verification.

The new example accepts runtime CA data and explicit identity, checks echo `[7]`,
then closes/releases and joins its client/resolver/engine. It contains no fixture
trust callback or compiled private key. The separate bounded test/benchmark peer
uses the repository's historical public TLS identity and an explicit test verifier;
it is not the production HTTP server or arbitrary-peer trust policy.

Normal maturity: 25 modules, 16 implementation nodes (15 C files), all 16 covered
by tests/single/docs, 35 workloads and 15 size profiles. Generated reference has
79 functions, 152 constants/macros and 73 public types. Strict release continues to
reject the developing xrpc aggregate. The source and generated single/reference
checks stay tied to the exact same inputs.

The plain TCP client gains one optional secure-state pointer and no TLS allocation
or TLS IO call. TLS adds a private sidecar and native session/stream/candidate state,
snapshotted metadata, timer duties and native plaintext-to-record copies. Read bytes
still copy into the existing bounded HTTP staging buffer. These costs are reported
as requested structure/artifact sizes and measured workloads, not RSS, zero-copy or
zero-overhead proof. Cold TLS, CPU/allocator/copy counts, pinned comparators and
native production acceptance remain pending.

Before shared outputs changed, 244 SHA-bound predecessor artifacts/object files
were archived at `out/service-extensions/xrpc-http-dial/before-http-tls/`, a junction
to `D:/CodexArtifacts/xrt-service-extensions/e87c3c7d-before-http-tls`. New phase
single/TSan/probe binaries use a dedicated D cache via `xrpc-http-tls/bin/`. WSL
example outputs were relocated with 197 file hashes reverified and a junction at
the original output directory. After their readers retired, WSL modular-test
outputs were similarly relocated with 381 file hashes reverified. No
primary-checkout work or predecessor evidence was deleted.


Both platforms passed complete xrpc and minimal TLS static/shared consumers,
declaration-only compilation and size checks for the plain HTTP client and TLS
client. Actual backend probes remain IOCP/epoll; the two deterministic concurrent
Close probes pass against the final modular objects. Qualified WSL O2 TSan passes
the complete TLS suite without a runtime race report. Its original O1 build
triggered GCC maybe-uninitialized diagnostics in unchanged Core PSK helpers;
inspection shows both successful helper paths assign their output counts before
the comparison. O2 matches production optimization and retains -Werror without
that warning downgrade. Original O1 diagnostics/driver and the source hash remain
in the receipts, as does GCC's atomic-thread-fence modeling warning. This is not
full race freedom or native Linux acceptance.

Independent stdlib HTTP/JSON peers complete 405 server exchanges plus one DNS
client exchange per platform. Independent OpenSSL/Python TLS peers additionally
run five cases against the example, minimal static consumer and minimal shared
consumer on both platforms: TLS 1.3 HTTP/1.1 ALPN, TLS 1.2 HTTP/1.1 ALPN, TLS 1.3
without ALPN, wrong identity and unrelated CA. Eighteen authenticated TLS RPCs
complete with exact echoed values; twelve negative connections produce typed
TRANSPORT/NOT_SENT with a cause and zero peer RPCs. Runtime-generated private keys
are confined to checked temporary directories and removed after the peer threads
join; only public certificate hashes/tool identities are recorded. This proves
these native TLS policies, not every cipher/group/resumption mode or independent
JSON-RPC-library parity.

The first WSL peer failed while reading the next keepalive request after a complete
reply: ConnectionResetError 104 from the documented client Abort close. Its full
trace is retained. The peer now joins request threads and only records resets/EOF
following a completely flushed reply; handshake, write, count, value and negative
trust failures remain fatal. All six TLS peer matrices were rerun. The peer-only
input refresh verifies every other frozen C/header/JSON/definition input unchanged
and preserves the original checks receipts. No production source or gate changed.

Requested x64 layout: HTTP client Windows 904/WSL 976 (+8 from the predecessor),
request 176 unchanged, private TLS state 72 and public TLS config 192. HTTP config
320/info 96, native call 88, snapshot 40, result prefix 32, dial data 200/plan 56
remain unchanged. Ledger remains Windows 72/WSL 144.

| Platform | Profile | Kind | Text | Data | BSS | Logical file bytes |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| windows | xrpc_http_client | single | 600524 | 2672 | 11136 | 869581 |
| windows | xrpc_http_client | static | 48808 | 0 | 0 | 76660 |
| windows | xrpc_http_client | shared | 90168 | 224 | 2848 | 149741 |
| windows | xrpc_http_tls | single | 1278020 | 2704 | 11200 | 1718214 |
| windows | xrpc_http_tls | static | 57884 | 0 | 0 | 92846 |
| windows | xrpc_http_tls | shared | 100762 | 224 | 2848 | 166420 |
| wsl | xrpc_http_client | single | 622173 | 5664 | 10940 | 1101456 |
| wsl | xrpc_http_client | static | 50213 | 128 | 0 | 100418 |
| wsl | xrpc_http_client | shared | 61119 | 1472 | 8 | 81040 |
| wsl | xrpc_http_tls | single | 1312306 | 7760 | 10940 | 2175672 |
| wsl | xrpc_http_tls | static | 60366 | 328 | 0 | 123990 |
| wsl | xrpc_http_tls | shared | 74438 | 1872 | 8 | 91688 |

These are unstripped GCC O2 artifacts. Single includes its actual Core closure;
static/shared rows are the extension artifact with Core supplied separately.


## HTTP/TLS six-workload measurements

Each workload uses one warmup and five measured runs. The unchanged production
HTTP client/server/executor profiles offer 2000 documents at 2000 planned arrivals/s;
TLS profiles offer 1000 at 500 planned arrivals/s against a bounded same-engine,
single-connection TLS 1.3 AES-128-GCM fixture on the other worker. TLS fixture/client
construction, certificate handshake and a readiness RPC are outside its timed loop.
Client encode, record creation, send, parsing, result observation/destruction and
scheduled-arrival lateness remain inside observation. All offered/admitted/failed/
rejected/completed and latency-sample counts are retained. Different peers/rates
make these rows unsuitable for an equivalent TLS overhead or capacity comparison.

| Platform | Workload | Completed documents/s | All-offer P99 us | Offer-lateness P99 us |
| --- | --- | ---: | ---: | ---: |
| windows | HTTP ordinary | 1944.757 | 30554.300 | 15322.700 |
| windows | HTTP notification | 1962.750 | 30946.800 | 15448.300 |
| windows | HTTP batch16 | 1908.881 | 89575.200 | 50641.100 |
| windows | TLS ordinary | 495.857 | 31206.100 | 15620.200 |
| windows | TLS notification | 495.194 | 30963.700 | 15448.000 |
| windows | TLS batch16 | 496.256 | 30153.500 | 15308.500 |
| wsl | HTTP ordinary | 1997.321 | 2181.301 | 1060.983 |
| wsl | HTTP notification | 1997.911 | 2170.698 | 1059.990 |
| wsl | HTTP batch16 | 1997.742 | 2245.908 | 1059.905 |
| wsl | TLS ordinary | 500.022 | 2186.710 | 1061.803 |
| wsl | TLS notification | 500.046 | 2168.898 | 1058.303 |
| wsl | TLS batch16 | 500.123 | 2203.100 | 1054.502 |


Every measured run accepts/completes all its offers (2000 plain or 1000 TLS),
with zero failed/rejected operations and all latency samples. Windows HTTP batch16
P50 fails unchanged noise gates: 3.272% relative MAD and 28.902% central range,
versus 3%/6%. Other Windows checks, including all TLS workloads, pass; all WSL noise
checks pass. Runner exits 1/0 and raw samples remain without favorable-sample
retries or relaxed limits. No baseline is pinned and performance remains unaccepted.
Shared-host Windows offer lateness/queueing and timer granularity remain material;
these are neither saturation capacity nor isolated TLS/server latency.

Source fingerprint: `8dcbcf0749146fe4086a12b267cfd901699afc19e0ec98c167964a9a74129bf0`.
Workload fingerprint: `75862cdcf06fe81fd17445e1a56a84e65363f3d07d2d3b0dfb07d3ff92adde90`.

Current evidence at `out/service-extensions/xrpc-http-tls/validation.json` binds
56 runtime binaries and 52 additional artifacts to the frozen inputs, actual
backends, layouts/sizes, qualified TSan, race probes, interop and raw 5+1 samples.
The committed rerun requires clean HEAD/tree and exact input/artifact hashes.
Cross-platform receipt paths are normalized before canonical-path AND hash checks;
WSL's mounted artifact cache does not bypass artifact identity.

Next work remains TLS server transport, mutual and HTTP application authentication,
managed tickets, richer HTTP metadata/recovery/pooling, WebSocket/framed stdio,
bidirectional calls, negotiated extensions and deferred business handlers. Redis
HA/tracking/dedicated modes, MQTT 5.0/3.1.1 full client/session/persistence, Cron
IANA/scheduler/durable recovery and the full P4 matrix remain active. Cold TLS,
slow consumers and contended/fault workloads, CPU/allocator/copy accounting,
representative pinned comparators and at least 72-hour native Windows/Linux
mixed/fault runs are still unaccepted. WSL is not native Linux qualification.
This TLS-client slice completes neither xrpc nor the four-product production goal.


## P3: bounded HTTP TLS server transport (2026-10-30)

The explicit TLS server factory now returns the ordinary HTTP server owner;
Start/Local/Info/Close/Wait/Destroy remain the lifecycle surface. It uses public
Core TCP/TLS ownership APIs and the existing bounded business executor. No Core
source, new runtime, automatic cleartext fallback or operation replay is added.
The complete four-product scope and P4 acceptance gates remain unchanged.

Construction retains native context/static identity, snapshots ALPN metadata,
and leaves selector/resumption/error callback contexts borrowed through Destroy.
HTTP/1.1 ALPN is selected explicitly; absence is permitted unless required by
native policy. RSA/P-256/P-384/Ed25519 identity backends are in the declared
closure. Runtime DER/PKCS#8 example inputs are wiped/released after construction;
the final stats follow physical Close/Wait. Mutual client/HTTP application
authentication and managed ticket rotation remain future slices.

The ordinary TCP listener counts connection/private-buffer/handshake admission
before constructing a native TLS session. MaxHandshakes defaults to 128 and has
independent live/peak/admitted/completed/failed/rejected counters. The absolute
handshake deadline starts at TCP admission and is rechecked at TLS Open, including
selector/signing time; HTTP idle/request deadlines start at readiness/first
plaintext. Zero handshake timeout explicitly disables it. Native TCP WriteLimit
must still cover the selected context SendLimit; a smaller application-record
budget is independently valid if one minimum record fits.

TLS send work reserves conservative native record bounds under the HTTP server
gate before plaintext acceptance, tracks short writes, and refunds only at full
native drain or after session/transport destruction. Interim 100 responses also
support partial native sends. TLS consumes plaintext through native stream APIs,
preserving their receive flow control. Each TLS peer sidecar is in the existing
peer allocation, rather than a second per-peer allocation. Native callback data
owns a real peer reference; peer retirement explicitly releases the TLS creator
to break the creator/event cycle. Close/Wait and host engine join observe the
actual native/session/callback and executor duties.

A pre-existing plain HTTP interim-send race was reproduced: credit could return
between testing a full global budget and later registering its waiter. The new
atomic gate operation tests the budget and registers the waiter together. A
separate probe compiles the real pinned predecessor/current production source,
returns real credit at gate release, and observes the missed/received post.
Both Windows and WSL require predecessor failure and current success. No product
test hook or Core edit is introduced.

Local checks include 15 modular plus 15 single-header runtime binaries per
platform (60 total), 17 runnable examples per platform and dependency trimming.
Dedicated cases cover ordinary/notification/mixed batches; released caller
identity/context and overwritten policy; multiple large replies under a shared
128-byte wire budget; 32-byte interim-credit fragmentation; held TCP handshake
and connection caps; stalled/plaintext handshakes; trust/ALPN rejection; idle and
partial-header deadlines; HTTPS absolute targets, chunk/trailer/pipeline order;
noncooperative handler shutdown; retained results after complete teardown;
constructor and real accepting-worker allocation quotas. Both rejection and
success occur in quota sweeps; zero charged bytes/count remain after full join.
These thresholds do not claim exhaustive allocation-site coverage.

The Windows single build exposed ignored config-init success in the test helper.
The helper now zero-initializes and checks both initializers. All modular tests,
the affected TLS single test, examples and qualified O2 TSan are rerun. Fourteen
unchanged single binaries per platform are reused only with exact binary/log
hashes, a bound old/new input set differing in that helper and the separately
rebuilt example, and actual compiler dependency manifests proving they do not
include either. The minimal-package check additionally caught the example's
missing explicit memory header; that include and only its input receipt are
refreshed after rebuilding/running the affected example on both platforms. This
dependency proof is part of the final validator. No warning downgrade is added.

Independent Python/OpenSSL clients generate temporary real CA/leaf/key material
and exercise RSA, P-256, P-384 and Ed25519 identities. Each example/static/shared
server consumer on each platform performs 33 HTTP exchanges: ordinary calls,
notifications and mixed batches over TLS 1.3 with/without ALPN, plus TLS 1.2 for
RSA/ECDSA. Each matrix also rejects four plaintext connections and verifies zero
live connections/record credit/handshakes and admitted=completed+failed after
physical shutdown. The keys are removed after execution; receipts retain only
public certificate hashes. Existing plain-server 405-exchange/eight-connection
and DNS-client peer checks also run against the rebuilt artifacts.

Complete and minimal TLS-server static/shared packages, declaration-only headers,
actual private layouts, all three tool size forms plus separately measured modular
objects and split Core dependency sizes are validated. Real engine backend
queries require IOCP on Windows and epoll in WSL. ASan/UBSan and O2 TSan are local
WSL evidence; the known GCC atomic-fence modeling limitation remains explicit.
WSL is not native Linux production acceptance. API inventory is 82 functions,
153 constants/macros and 76 public types; normal maturity reports 27 modules,
17 implementations and only the aggregate xrpc product remains developing.
Strict release must continue to reject that aggregate.

The six workload profiles use the same compiled client/server closure, worker
counts, payload, 1000 planned arrivals/s and one warmed persistent connection:
TCP/TLS ordinary, notification and batch16. One readiness RPC, construction,
handshake and shutdown are outside timing. All offers and queue-inclusive
latencies/lateness are retained; one warmup plus five measured samples use the
unchanged 3% relative-MAD/6% central-range gates. This does not measure saturation,
isolated TLS cost, cold handshakes, allocator/copy cost or an independent pinned
comparator. No performance baseline or production acceptance is claimed.


Measured private sizes (bytes, Windows/WSL respectively): server 920/1056,
peer 400/400, HTTP operation 464/464, server TLS policy/counters 224/224,
TLS peer tail 32/32. Default ALPN snapshot adds 24 bytes to the server policy
allocation. The two always-present opaque TLS pointers add 8 bytes each to the
server and peer base layouts; native session/buffer/identity allocations are
separate. The tail uses the peer allocation, with no second per-peer allocation.
Each record chunk performs native bound checks, gate-protected reservation,
plaintext record encoding/copy and native drain observation; no zero-overhead,
zero-copy or process-RSS claim follows from these layouts.

| Platform | Owned modular text/data/file | Owned static text/file | Owned shared text/file | Single text/file (includes Core) | Core static text/file | Core shared text/file |
| --- | --- | --- | --- | --- | --- | --- |
| windows | 52496/0/81733 | 52496/83232 | 97007/166597 | 1133828/1549181 | 934632/1629948 | 1054849/1345456 |
| wsl | 53857/320/108000 | 53857/109496 | 71455/96584 | 1160844/1972504 | 974830/2099908 | 1118312/1291304 |

Code/data/file totals keep XRPC and Core separate for modular/static/shared forms;
the single object includes the actual Core closure. Import/archive metadata,
process stacks, pooled allocations and native TLS buffers are additional costs.

### Matched workload measurements

The report stores all five aggregate samples/counts per workload. Every offer
enters the in-memory latency/lateness population; a per-request trace is not
persisted by this harness. The reported observation includes Future polling.

| Platform | Workload | Completed/s median | P50 us | P99 us | Offer-lateness P99 us |
| --- | --- | --- | --- | --- | --- |
| windows | tcp_ordinary | 978.028 | 23442.500 | 31245.700 | 15646.400 |
| windows | tcp_notification | 973.570 | 23232.100 | 31067.500 | 15456.500 |
| windows | tcp_batch16 | 975.124 | 21471.100 | 29664.300 | 15343.100 |
| windows | tls_ordinary | 978.874 | 23440.700 | 31371.000 | 15450.600 |
| windows | tls_notification | 978.663 | 23327.400 | 31308.700 | 15422.900 |
| windows | tls_batch16 | 978.704 | 21901.100 | 29795.400 | 15311.300 |
| wsl | tcp_ordinary | 999.443 | 1639.360 | 2181.365 | 1062.265 |
| wsl | tcp_notification | 999.423 | 1640.568 | 2187.950 | 1062.989 |
| wsl | tcp_batch16 | 999.377 | 1662.875 | 2221.092 | 1058.599 |
| wsl | tls_ordinary | 999.452 | 1635.712 | 2179.916 | 1062.502 |
| wsl | tls_notification | 999.858 | 1640.895 | 2175.796 | 1062.200 |
| wsl | tls_batch16 | 999.229 | 1668.403 | 2226.601 | 1060.203 |

Each measured run retains 1000 offers/accepted/completed/latency observations,
with zero rejected/failed operations. Five raw aggregate samples plus one warmup
are retained without favorable-sample retries. Unchanged noise checks:

- windows: pass all 3% relative-MAD and 6% central-range checks.

- wsl: pass all 3% relative-MAD and 6% central-range checks.

Windows P99 is approximately 30-31 ms with about 15 ms offer lateness; both TCP
and TLS populations include planned-arrival delay and the Sleep(1) Future
observation loop. Passing the noise gates does not demonstrate low native RPC
latency or saturation capacity. Separating completion publication timestamps
from observer wakeup and improving arrival timing is the next measurement task;
process CPU and native fault/cold/contended workloads remain required. No
baseline is pinned and performance remains unaccepted.

windows source fingerprint: `f682d2dca508f375ef46556c8efe4135c3fb7e905b1d294521a8ede5fbd3a19d`; workload fingerprint: `4f1786c63459735b07a379dd2593fc7cbeb8da5fa33e744209711d6a05f137eb`.

wsl source fingerprint: `f682d2dca508f375ef46556c8efe4135c3fb7e905b1d294521a8ede5fbd3a19d`; workload fingerprint: `4f1786c63459735b07a379dd2593fc7cbeb8da5fa33e744209711d6a05f137eb`.

Evidence at `out/service-extensions/xrpc-http-server-tls/validation.json` binds
all 60 runtime binaries, rebuilt example-only include refresh, unchanged-single
dependency proofs, complete/minimal packages, split code/dependency sizes, actual
backends, O2 TSan, old/new wakeup probes, six independent TLS matrices and 5+1
performance aggregates to frozen source/artifact hashes. The committed validator
requires clean HEAD/tree and exact inputs/artifacts, normalizing mounted WSL
cache paths before canonical-path and hash comparisons.

The next production work remains mutual/HTTP application authentication, managed
tickets, richer metadata/recovery/pooling, WebSocket/framed stdio, bidirectional
calls, negotiated extensions and deferred handlers. Redis HA/tracking/dedicated
modes, MQTT 5.0/3.1.1 full client/session/store, Cron IANA/scheduler/durable recovery
and every P4 requirement remain active. Native Windows/Linux representative
comparators, CPU/allocator/copy costs, failures, cold/slow/contended workloads and
at least 72-hour native mixed/fault qualification are still unaccepted. This
slice completes neither xrpc nor the full four-product production goal.


## P4 instrumentation: terminal-watch timing and honest harness costs (2026-10-30)

The matched TCP/TLS benchmark now distinguishes planned arrival, actual
submission, terminal Watch observation and result collection. The old Sleep(1)
loop included collector wakeup delay in reported RPC latency. No production
implementation, public header, API, Core source or generated single header is
changed: the validator checks 434 prior production/header inputs byte-for-byte.
The new module is test-only, without a feature, production source or internal
production header. The full four-product contract and original gates remain.

A pending Watch captures a monotonic clock reading, stores one double, then
publishes a release flag; the collector uses acquire ordering. The caller owns
the Future and embedded Watch storage through WatchRemove, including an already
started callback's physical return. READY does not reveal historical completion:
it records a caller observation upper bound and is counted separately. Native
Core publication is not instrumented or claimed. The observer adds no private
heap allocation or extra Future reference; registration/clock/atomic/removal and
all Core duties still have costs. Collector validation/destruction is timed but
does not inflate the terminal-watch latency population.

All offers, including admission refusals, enter planned-to-terminal and
submit-to-terminal populations. Refusals use their admission return timestamp.
Collector lag has its own accepted-result population. P50/P95/P99/P99.9 use
nearest-rank values, and optional CSV traces retain every offer's index, four
timestamps, admission, status, delivery and READY provenance. Trace I/O occurs
after timing and an I/O failure makes the invocation fail after normal cleanup.
Elapsed terminal and collector endpoints are both printed. Throughput endpoints
and the arrival wait differ from the old harness; no reported change is a
transport speedup. A finite 1000-offer run at 1000 planned offers/s is a fixed-load
observation, not a saturation or maximum-capacity measurement.

The default yielding generator removes deliberate Sleep(1) waits, and the same
observer retains a diagnostic sleep mode. Yield consumes CPU and can contend
with other workers. Native Windows GetProcessTimes / Linux getrusage report
user+kernel CPU for the entire process, including the generator, watch handling,
Core/network/server and result validation/destruction. These calls are confined
to the benchmark; no Core API/runtime is introduced. The native Windows counter reported zero deltas for these short/bursty sleep
samples, even though work occurred; its cause/resolution is not independently
profiled. The
separate busy-work test verifies CPU-counter progress. Zero does not mean free
protocol work. These are not hardware cycle counts or isolated protocol CPU.
Arrays, initialization, TLS handshake/readiness, sorting, reporting, trace and
shutdown are excluded from the timed interval; extra observer storage and four
sample arrays are explicitly reported, with all other memory still separate.

Four new modular/single observer runtime binaries pass on Windows and WSL.
A delayed collector sleeps 40 ms while notification timestamps remain inside
the producer completion interval. Other cases cover READY upper bounds,
cancellation without fabricated terminal state, detach-before-free, 256 concurrent
registration/removal races per binary, one-byte allocation quota and native CPU
progress. WSL ASan/UBSan passes for both observer forms and the actual benchmark;
qualified O2 TSan passes the observer races. GCC atomic-fence modeling limitations
and the lack of native Linux acceptance remain explicit.

An independent Python CSV oracle checks 42 traces: TCP/TLS ordinary, notification
and batch16 with both waits, plus two high-arrival burst cases, on Windows, WSL
and the actual WSL ASan/UBSan benchmark. It independently recomputes counts,
provenance, nearest-rank percentiles, elapsed/throughput relationships, process
CPU normalization and harness storage. All 19,200 offers remain in the traces.
Each form observes real overloaded admissions, with zero failed accepted calls:

| Form | TCP ordinary accepted/refused | TLS batch16 accepted/refused |
| --- | --- | --- |
| windows | 151/1849 | 171/1829 |
| wsl | 143/1857 | 145/1855 |
| wsl-asan | 149/1851 | 152/1848 |

Trace-write failure and invalid arrival mode have their required nonzero exits.
Every diagnostic label has its own CSV/log hashes. Diagnostic loads ran before
formal serial measurements; those earlier parallel diagnostics are not noise or
performance-acceptance evidence. Source/tool/artifact hashes and unchanged
production dependency closure bind the ASan benchmark to the implementation.

### Serial harness calibration

The frozen predecessor binaries and new observer modes run once warm plus five
measured samples for each ordinary TCP/TLS case, serially on each platform. All
runs accept/complete 1000 offers with zero failures/refusals. The table contains
medians of the five aggregate samples; actual raw outputs are retained.

| Platform | Transport | Harness | Reported P99 us | Collector-lag P99 us | Process CPU us/offer |
| --- | --- | --- | --- | --- | --- |
| windows | tcp | legacy | 31322.700 | n/a | n/a |
| windows | tcp | watch_sleep | 15749.100 | 15889.100 | 0.000 |
| windows | tcp | watch_yield | 141.900 | 0.200 | 1125.000 |
| windows | tls | legacy | 31174.900 | n/a | n/a |
| windows | tls | watch_sleep | 15665.900 | 15888.200 | 0.000 |
| windows | tls | watch_yield | 152.600 | 0.200 | 1078.125 |
| wsl | tcp | legacy | 2173.622 | n/a | n/a |
| wsl | tcp | watch_sleep | 1261.349 | 970.200 | 234.576 |
| wsl | tcp | watch_yield | 285.600 | 8.200 | 1224.142 |
| wsl | tls | legacy | 2183.305 | n/a | n/a |
| wsl | tls | watch_sleep | 1293.650 | 946.001 | 265.861 |
| wsl | tls | watch_yield | 296.803 | 0.700 | 1237.678 |

The large old/new difference is a measurement/arrival-policy effect with the
same production source, not an RPC optimization. READY remains an observation
upper bound; diagnostic CPU zeros are native counter observations, not free protocol CPU. Yield's
roughly one busy generator core is included and cannot be removed from CPU
comparisons. No independent comparator or stable baseline is established.

### Matched six-workload observations

The final yield profiles use identical client/server closure, worker counts,
payload, 1000 planned arrivals/s and one warmed persistent connection. Each has
one warmup and five measured runs, retaining all 1000 accepted/completed offers
and latency observations, with zero failed/rejected operations. Raw aggregate
samples and source/workload fingerprints remain available.

| Platform | Workload | Completed/s | P50 us | P99 us | P99.9 us | Arrival-lateness P99 us | Collector-lag P99 us | Process CPU us/offer | Harness bytes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| windows | tcp_ordinary | 1000.907 | 88.500 | 140.200 | 176.800 | 0.200 | 0.200 | 1171.875 | 192000 |
| windows | tcp_notification | 1000.905 | 76.200 | 126.800 | 185.900 | 0.100 | 0.200 | 1109.375 | 192000 |
| windows | tcp_batch16 | 1000.875 | 123.200 | 188.600 | 225.500 | 0.200 | 0.200 | 1078.125 | 192000 |
| windows | tls_ordinary | 1000.889 | 96.500 | 154.700 | 211.700 | 0.100 | 0.200 | 1109.375 | 192000 |
| windows | tls_notification | 1000.902 | 84.600 | 145.600 | 194.300 | 0.100 | 0.200 | 1125.000 | 192000 |
| windows | tls_batch16 | 1000.847 | 139.500 | 218.700 | 268.500 | 0.200 | 0.300 | 1203.125 | 192000 |
| wsl | tcp_ordinary | 1000.829 | 173.100 | 255.513 | 307.714 | 2.754 | 0.700 | 1202.985 | 192000 |
| wsl | tcp_notification | 1000.831 | 165.827 | 249.514 | 327.232 | 5.710 | 2.400 | 1183.006 | 192000 |
| wsl | tcp_batch16 | 1000.773 | 218.602 | 317.603 | 359.902 | 2.770 | 0.700 | 1236.442 | 192000 |
| wsl | tls_ordinary | 1000.800 | 201.998 | 293.075 | 339.968 | 5.889 | 1.500 | 1231.472 | 192000 |
| wsl | tls_notification | 1000.825 | 180.792 | 254.998 | 313.880 | 2.494 | 0.800 | 1199.714 | 192000 |
| wsl | tls_batch16 | 1000.728 | 260.881 | 376.579 | 434.094 | 3.359 | 0.900 | 1279.184 | 192000 |

The unchanged 3% relative-MAD / 6% central-range checks fail in this shared-host
run. They are neither relaxed nor retried to select favorable samples. Failure
counts include separate MAD/range failures across primary and new diagnostic
metrics; they are not failed requests. All failing metric names and all five
samples are retained. Performance remains unaccepted and no baseline is pinned.

windows: 57 failed noise checks. Source fingerprint: `2923a94183f12793bad0fb70d150a293049d1db6f911d8ba9122ef8d0d58f36e`; workload fingerprint: `9dc494f9cde39efa4f2944d0cba92974ba209c57889447439516f85028321b23`.

wsl: 63 failed noise checks. Source fingerprint: `2923a94183f12793bad0fb70d150a293049d1db6f911d8ba9122ef8d0d58f36e`; workload fingerprint: `9dc494f9cde39efa4f2944d0cba92974ba209c57889447439516f85028321b23`.

Evidence: `out/service-extensions/xrpc-http-timing/validation.json` binds 434
unchanged production inputs, four observer test binaries, TSan, the actual ASan
benchmark, 42 per-offer traces, serial calibration binaries/raw outputs and six
5+1 workload reports to exact inputs/artifacts. The predecessor's 4636 copied
performance artifacts are SHA-checked before any overwrite in a separate cache.
The committed validator additionally requires clean HEAD/tree. Public API remains
82 functions / 153 constants or macros / 76 types; normal maturity retains 17
implementations and only the aggregate xrpc product is developing.

Next work remains protocol capability completion and qualified performance:
RPC mutual/HTTP authentication, tickets, metadata/recovery/pooling, WebSocket/
framed stdio, bidirectional calls, negotiated extensions and deferred handlers;
Redis HA/tracking/dedicated modes; full MQTT 5.0/3.1.1 client/session/store;
Cron IANA/scheduler/durable recovery. Representative pinned comparators, process
CPU/cycle/allocation/copy/synchronization/syscall accounting, cold/slow/contended
and fault/recovery matrices, stable native Windows/Linux objectives and 72-hour
native mixed/fault runs remain required. No smaller scope or production-complete
claim is substituted by this instrumentation slice.



## P3: bounded immutable HTTP request headers (2026-10-31)

The HTTP client now snapshots up to 64 case-insensitively unique request fields
before its numeric/DNS/prepared/TLS factory returns. The encoded block defaults
to 16 KiB with a hard 1 MiB cap. Invalid names/values, control injection,
duplicates, size/count overflow, output aliases and 15 transport-owned fields
are rejected before TCP construction. Authorization/Cookie values remain opaque;
this feature transports credentials without implementing authentication policy.
Server asynchronous authorization, per-request context, challenge/refresh,
mutual authentication and managed tickets remain pending.

Descriptors, names and values may be overwritten after factory return. The flat
client allocation holds the pre-encoded snapshot; the hot path copies that block
over the native writer's terminal CRLF. No additional header allocation per
request is introduced. Extra bytes count against existing wire budgets. Cold
validation includes bounded scans, duplicate comparisons and the native writer's
preflight/encoding. New presence checks and larger layouts affect the default
path too; no zero-overhead claim is made. Recompile all consumers for the changed
public config layout, including consumers selecting other module combinations.

All custom fields trigger potentially sensitive-data handling. The extension
wipes its complete encoded request wire at the real final request reference,
and its flat text snapshot at final client release. Completion is insufficient
while native TCP still borrows bytes. Whole-wire wiping adds work proportional
to the request, including its body. Caller/native/OS copies and dumps are outside
this contract. The TLS example accepts an explicit bounded authorization file,
wipes its input buffer, and prints no credential value; no ambient secret lookup
or credential negotiation is added.

Both platforms pass 16 modular and 16 single runtime binaries each (64 total),
17 runnable examples each, dependency trims, complete/minimal static and shared
consumers, declaration compilation and all three size forms for the plain/TLS
client profiles. Windows uses actual IOCP; the WSL composition uses actual epoll.
WSL is not native Linux production acceptance. The qualified O2 TSan run retains
the compiler's atomic-fence modeling warning; no complete race-freedom claim is
made. ASan/UBSan and real constructor/request OOM tests remain part of the matrix.

Two actual production-TU pre-free probes inspect live storage immediately before
the real frees, covering successful/cancelled/dropped wires, final snapshots and
partially constructed snapshots under OOM, followed by zero charged owner bytes.
They neither read freed storage nor replace the shipped allocator. TLS fixtures
count complete documents, with release/acquire publication of expectations.
The independent HTTPS peer runs Basic/Bearer fixture files across example,
static and shared consumers on both platforms: 12 matrices, 36 successful RPCs
and 24 identity/CA refusals before RPC, including TLS 1.3/1.2 and ALPN/no ALPN.
These are client transport checks, not an implemented authorization server.

The validation scripts initially exposed two harness defects: a duplicate thread
macro for the test-only observer and a modular-binary path assumption for that
observer's separate suite. The macro was corrected and all single builds rerun.
Successful build logs were preserved while fixing receipt mapping; single
runtime checks were repeated and the separate modular observer was built/run.
No production or C test input changed during those final runs. A prior fixture
counter had counted incomplete TLS body retries; it now checks complete documents.

### Layout and code size

Sizes below are measured bytes. Single objects include Core plus extension,
whereas static/shared profile primary artifacts contain extension-owned code;
the separate Core libraries are reported below. These are different denominators
and must not be interpreted as equivalent total footprints. The previous config
was 320 bytes and previous
client owners were 904 bytes on Windows / 976 bytes on WSL. Layout changes are
independent of a configured header count. Request/call/quota layouts are retained
in the receipt; the new text snapshot additionally stores actual encoded bytes.

| Platform | Client owner | HTTP config | Request owner | Default header budget |
| --- | --- | --- | --- | --- |
| windows | 952 | 344 | 176 | 16384 |
| wsl | 1024 | 344 | 176 | 16384 |

| Platform | Profile | Form | File bytes |
| --- | --- | --- | --- |
| windows | xrpc_http_client | single | 872434 |
| windows | xrpc_http_client | static | 79438 |
| windows | xrpc_http_client | shared | 152502 |
| windows | xrpc_http_tls | single | 1721058 |
| windows | xrpc_http_tls | static | 95626 |
| windows | xrpc_http_tls | shared | 169181 |
| wsl | xrpc_http_client | single | 1105032 |
| wsl | xrpc_http_client | static | 104186 |
| wsl | xrpc_http_client | shared | 85376 |
| wsl | xrpc_http_tls | single | 2179240 |
| wsl | xrpc_http_tls | static | 127758 |
| wsl | xrpc_http_tls | shared | 100120 |

| Platform | Suite | Form | Core dependency file bytes | Extension file bytes |
| --- | --- | --- | --- | --- |
| windows | xrpc | static | 1972716 | 183756 |
| windows | xrpc | shared | 1609346 | 248223 |
| windows | xrpc_http_client | static | 858894 | 79438 |
| windows | xrpc_http_client | shared | 786814 | 152502 |
| windows | xrpc_http_tls | static | 1809094 | 95626 |
| windows | xrpc_http_tls | shared | 1491120 | 169181 |
| wsl | xrpc | static | 2515252 | 242844 |
| wsl | xrpc | shared | 1562504 | 174200 |
| wsl | xrpc_http_client | static | 1089520 | 104186 |
| wsl | xrpc_http_client | shared | 734048 | 85376 |
| wsl | xrpc_http_tls | static | 2304734 | 127758 |
| wsl | xrpc_http_tls | shared | 1444584 | 100120 |

### Paired header observations

The four ordinary workloads pair no-header/two-header TCP and TLS using the same
production closure, workers, payload, warmed persistent connection and fixed
1000 offers/s. Each has one warmup plus five measured runs. Every measured run
accepts/completes all 1000 offers with zero refusals/failures; all offers enter
the latency population. Snapshot construction/erasure and handshake/shutdown are
outside timing. The process CPU window includes the busy yielding generator and
all concurrent work inside it. Native callback retirement and its wire wipe may
follow the last collector observation, so the window does not prove complete
per-request cleanup CPU. No isolated transport CPU, cycle, saturation-capacity,
stable-baseline or representative comparator claim follows from these samples.

| Platform | Workload | Completed/s | P99 us | P99.9 us | Process CPU us/offer | Encoded custom block bytes |
| --- | --- | --- | --- | --- | --- | --- |
| windows | tcp_baseline | 1000.896 | 239.200 | 683.400 | 1109.375 | 0 |
| windows | tcp_auth | 1000.912 | 139.100 | 194.400 | 1062.500 | 76 |
| windows | tls_baseline | 1000.913 | 154.100 | 217.600 | 1109.375 | 0 |
| windows | tls_auth | 1000.915 | 159.700 | 254.400 | 1093.750 | 76 |
| wsl | tcp_baseline | 1000.812 | 291.805 | 334.507 | 1212.596 | 0 |
| wsl | tcp_auth | 1000.813 | 258.210 | 308.011 | 1205.716 | 76 |
| wsl | tls_baseline | 1000.806 | 273.000 | 354.203 | 1228.477 | 0 |
| wsl | tls_auth | 1000.807 | 248.305 | 278.305 | 1220.109 | 76 |

The original 3% relative-MAD / 6% central-range gates remain unchanged. No retries
select favorable samples, no baseline is pinned and performance remains unaccepted.

windows: 60 failed noise checks. Source fingerprint: `128b4f844957603182c16a84b58493d66be910f0463d9c145bf73e0f040c19ea`; workload fingerprint: `19833c392823d003691d93ece4014ae60a514b470a442ca58c542b063c8f6371`.

wsl: 32 failed noise checks. Source fingerprint: `128b4f844957603182c16a84b58493d66be910f0463d9c145bf73e0f040c19ea`; workload fingerprint: `19833c392823d003691d93ece4014ae60a514b470a442ca58c542b063c8f6371`.

Evidence: `out/service-extensions/xrpc-http-headers/validation.json` binds all
64 runtime binaries, unchanged Core inputs, two pre-free probes, qualified TSan,
packages/layout/declaration/sizes, actual backends, 12 independent peer matrices
and four paired 5+1 workloads to current hashes. The predecessor's 201 artifacts
were SHA-bound before overwrite. Committed validation additionally requires clean
HEAD/tree. Public API: 82 functions / 155 constants or macros / 76 types; normal
maturity keeps 17 implementations, with only aggregate xrpc still developing.

The full four-product scope remains active. Next RPC capabilities include server
authorization and request context, mTLS/tickets, metadata/recovery/pooling,
bidirectional/WebSocket/framed stdio, negotiation and deferred handlers. Redis
HA/tracking/dedicated modes, complete MQTT 5.0/3.1.1 client/session/store, Cron
IANA/scheduler/durability and all performance/production acceptance remain.
Representative pinned comparators, complete cold/hot CPU/cycle/allocation/copy/
synchronization/syscall accounting, slow/contended/fault/recovery matrices and
native Windows/Linux mixed/fault runs of at least 72 hours are still required.



## P3: bounded copied server request context (2026-11-01)

`xrpcServerSubmitWithContext` supplies an independent immutable byte snapshot to
each execution document. Handlers borrow `Context->Data`; a whole batch shares
one snapshot. Inputs/context may overlap and may be overwritten/freed after
submission returns, but cannot overlap the Future output. Empty legacy/context
submissions present canonical NULL/zero data. Embedded pointers are not deep
copied and the view cannot be retained. Nonempty data is aligned for max_align_t.

The configured per-document cap defaults to 16 KiB, hard 1 MiB, with zero
disabling nonempty context. The private job remains one allocation containing
the existing header, input, alignment padding and context. All are charged to
MaxRequestBytesTotal. ContextBytes is included in RequestBytes and includes
construction reservations until actual final job retirement. Context bytes
are securely zeroed before the real job free, including constructor/stop paths;
caller/pointed-to/result/native/system copies are outside this erasure contract.
Results own their existing independent quota ledger and do not retain context.

This is the per-request execution data channel required by later authorization,
not implemented HTTP authorization or principal policy. Async head authorization
before 100 Continue/body/business, typed HTTP metadata, mutual authentication,
tickets and all remaining transport/runtime features stay pending. No Core
source or new shared runtime is added. The copied-context design infers its bounds from existing input/allocation
lengths, preserving the private job header size. The copied-context design installs no
context allocation, reference counter or user cleanup callback. There are still
new validation/presence/accounting operations, larger public/owner structures, one explicit
context copy and size-proportional final erasure; no zero-overhead claim is made.

The offline server example demonstrates copied context for a call/notification
batch, caller overwrite and reading the response after physical server destroy.
Tests cover binary/overlapping data, worker-gated lifetime, batch sharing,
canonical empties, configured/aggregate limits, all 1 MiB bytes, output aliases,
four producer threads with 128 independent contexts, queued Future/server cancel,
deadline precedence and independent retained results. Alternating empty/nonempty
contexts exercise 2048 request-construction quota prefixes and zero budget balance.

The two production-TU probes inspect live job storage immediately before real
free; each observes 10 context erasures and actual constructor-failure cleanup.
They gate successful jobs so completion cannot be mistaken for constructor
failure, do not read freed memory and do not modify the shipped allocator. With
the same sample input the requested job allocation is 192 bytes empty / 200 bytes
with eight context bytes on both platforms; other input lengths can add padding.
Allocator metadata/rounding and native storage are outside these requested-byte
figures and remain separate performance-accounting work.

Both platforms pass 15 modular runtime binaries and the three affected server,
HTTP-server and TLS-server single binaries: 36 total, plus 17 examples each.
WSL uses ASan/UBSan; qualified O2 TSan passes with its atomic-fence modeling
warning retained. This does not prove complete race freedom or native Linux
acceptance. Full/minimal static/shared consumers, declaration compilation and
three server size forms pass. Archived c4ab0d02 sources compile with their own
headers for actual previous/current private layout observations. Independent
peers drive 405 plain exchanges/eight concurrent connections and 33 TLS exchanges
with RSA/P256/P384/Ed25519, TLS 1.3/1.2, ALPN/absence and plaintext rejection on
each platform. These regress existing transports, not new authorization behavior.

### Measured layouts and file sizes

Consumers must rebuild for public config/context/info and nested HTTP layouts.
Values below are bytes; columns are compiled predecessor and current layouts.

| Platform | Object | Previous | Current |
| --- | --- | --- | --- |
| windows | server | 312 | 320 |
| windows | job | 152 | 152 |
| windows | quota | 144 | 152 |
| windows | config | 272 | 280 |
| windows | context | 16 | 32 |
| windows | info | 96 | 104 |
| windows | http_server | 920 | 936 |
| windows | http_config | 568 | 576 |
| windows | http_info | 216 | 224 |
| wsl | server | 312 | 320 |
| wsl | job | 152 | 152 |
| wsl | quota | 280 | 288 |
| wsl | config | 272 | 280 |
| wsl | context | 16 | 32 |
| wsl | info | 96 | 104 |
| wsl | http_server | 1056 | 1072 |
| wsl | http_config | 568 | 576 |
| wsl | http_info | 216 | 224 |

Single objects include Core plus extension. Static/shared primary artifacts
contain extension code and require separately reported Core dependencies; these
forms have different denominators and are not interchangeable total footprints.

| Platform | Form | Primary file bytes | Core dependency file bytes |
| --- | --- | --- | --- |
| windows | single | 512955 | included |
| windows | static | 43080 | 521590 |
| windows | shared | 82292 | 521724 |
| wsl | single | 635280 | included |
| wsl | static | 57824 | 659808 |
| wsl | shared | 52976 | 461128 |

### Paired execution-context observations

The 0/32/4096-byte workloads use identical workers, a 64-document window and one
item per document. Each run completes 2000 windowed documents plus 2000 separate
one-in-flight latency samples. One warmup and all five measured samples remain
available. These are windowed throughput/serial latency, not open-loop network
capacity or tail-latency acceptance. Handler size/edge verification and submit
selection are harness costs. Process CPU spans both phases after factory return
through physical server Destroy/join, including all context copy/erasure and
native retirement/shutdown plus possible startup-return tails. Source allocation,
later sort/report/free are outside this window. Wall metrics exclude shutdown
and can precede mechanical tails. CPU is normalized by all 4000 documents and
is neither isolated transport CPU nor steady-state CPU without shutdown.

| Platform | Context bytes | Windowed documents/s | Serial P50 us | Serial P99 us | Process CPU us/document through join |
| --- | --- | --- | --- | --- | --- |
| windows | 0 | 415256.524 | 11.700 | 22.400 | 11.719 |
| windows | 32 | 409651.388 | 12.000 | 25.900 | 11.719 |
| windows | 4096 | 357110.970 | 12.800 | 23.000 | 11.719 |
| wsl | 0 | 231085.640 | 49.100 | 100.500 | 27.193 |
| wsl | 32 | 280225.862 | 42.300 | 76.600 | 23.263 |
| wsl | 4096 | 204392.393 | 47.700 | 105.300 | 28.343 |

Original 3% relative-MAD / 6% central-range limits are unchanged. No favorable
retry, pinned baseline, stable comparator, saturation or production claim is made.
Performance remains unaccepted; full native Windows/Linux and 72-hour mixed/fault
qualification are still required.

windows: 21 failed noise checks. Source fingerprint: `d921e142bf5fb769b91ca8d4454876f3e35e65d50611c6c3c6bae3c0b535db61`; workload fingerprint: `1af0aae2709d7d39f5a6df0b7df9f5f677d0c0b6f019ad0627f50653533dfd41`.

wsl: 19 failed noise checks. Source fingerprint: `d921e142bf5fb769b91ca8d4454876f3e35e65d50611c6c3c6bae3c0b535db61`; workload fingerprint: `1af0aae2709d7d39f5a6df0b7df9f5f677d0c0b6f019ad0627f50653533dfd41`.

Evidence: `out/service-extensions/xrpc-server-context/validation.json` binds current
inputs, 36 runtimes/34 examples, two pre-free probes, TSan, packages/layout/sizes,
independent peers and paired 5+1 reports. 847 predecessor source/artifact files
were SHA-bound before overwrites; the committed validator requires clean HEAD/tree.
Public API: 83 functions / 157 constants or macros / 76 types. Normal maturity
retains 17 implementations and only aggregate xrpc is developing.

The complete four-product objective remains active. RPC HTTP authorization and
transport metadata, mTLS/tickets, recovery/pooling, bidirectional/WebSocket/framed
stdio, negotiation and deferred handlers remain. Redis HA/tracking/dedicated
modes, full MQTT 5.0/3.1.1 client/session/store and Cron IANA/scheduler/durability
remain, together with pinned representative comparisons, complete cold/hot
CPU/cycle/allocation/copy/synchronization/syscall accounting and native fault/
slow/contended/recovery matrices. This prerequisite completes neither xrpc nor
the four-product production goal. Integration with subsequent primary-checkout
changes also remains a separate verified step.



## P3: bounded asynchronous HTTP authorization (2026-11-01)

HTTP and HTTPS servers now optionally authorize a validated immutable request-head
snapshot on the existing bounded executor before 100 Continue, body processing or
business dispatch. The callback receives the accepted transport peer, TLS mode,
one child cancellation token and the original whole-request deadline. ALLOW
copies bounded opaque identity bytes into each handler's existing Context->Data;
a batch shares the document identity. Remote does not trust forwarded headers and
TLS mode supplies no client identity. Borrowed callback/code/User lifetimes,
concurrency and self-wait rejection are documented in http_authorization.md.

401 includes the copied configured WWW-Authenticate challenge; 403, callback
failure/cancellation 503, invalid callback results 500 and duplicate Authorization
400 have explicit paths. Refusals close without reinterpreting queued bodies or
subsequent authorized requests. The optional challenge is a valid field value up
to 128 bytes; its RFC scheme semantics remain the application's responsibility.
The HTTPS example reads runtime credential files, uses native constant-time
equal-length comparison, provides a sample identity, and wipes its local token
after physical destruction. This is an application policy callback, not a token
database, credential-refresh service or automatic negotiation/retry/replay.

Each authorization uses one flat allocation for the embedded executor node,
snapshot descriptors/raw head/alignment/output capacity. It creates no Future,
extra dispatch scratch or new shared runtime. MaxAuthorizations (default 128)
includes queued/running snapshots and allowed snapshots retained during body
wait; default identity capacity is 256 bytes. Snapshot bytes count against HTTP
MaxBufferBytes, while pending auxiliary work shares executor operation/byte/queue
limits. The logical ledgers overlap and are not additive RSS. Auxiliary credit
returns before completion publication so a one-operation executor can accept its
authorized document; Core drop/join and transport references cover actual tails.
Document Accepted/Completed and AuxiliaryAccepted/Completed remain separate.

Snapshots are wiped immediately before the actual free. Auth-enabled receive
growth allocates/copies then wipes the old storage, reserving old plus new peak
capacity; compaction and final retirement wipe removed/capacity bytes. Native
TCP/TLS/system/caller/result copies are outside this erasure contract. Disabled
authorization allocates no auth peer sidecar/snapshot/child/task, but owner layout,
presence checks and accounting have real costs. This is not zero overhead or a
claim of complete system-wide secret erasure. No Core source is changed.

Validation includes constructor OOM and child-token quota sweeps, real pre-free
snapshot observation, held/noncooperative callback drain, cancellation/deadline,
queue/operation/snapshot bounds, identity/batch and persistent reuse. A final
review found an omitted child-constructor rejection counter. Build controllers
were stopped for this correction, the current compiler was allowed to finish,
and its process tree was verified drained before edits. Initial Windows package
checks passed; its size phase was interrupted and the initial WSL distribution
had not begun. Superseded receipts are retained separately. All final matrices,
probes and distribution packages were rebuilt/retested against the corrected
inputs, rather than rebinding old hashes. A denied-pipeline peer test now appends
a valid authorized request and verifies closure plus no extra dispatch.

Both platforms pass 15 modular runtime binaries plus server/HTTP-server/TLS-server
single binaries (36 total), and 17 examples each (34 total). WSL uses ASan/UBSan;
qualified O2 TSan passes with its atomic-fence modeling warning retained. This is
neither complete race freedom nor native Linux qualification. Full and minimal
HTTPS static/shared consumers, declaration compilation, and plain/HTTPS single,
static and shared size forms pass. Twelve independent Python HTTPS matrices
(example/static/shared x Basic/Bearer x two platforms) pass 144 cases and 84
accepted documents, including verified TLS 1.2/1.3, identity/batch/notification,
large heads, Continue ordering, duplicate credentials and denied pipelines.

### Layout and requested-storage costs

Recompile consumers for the changed config/info and nested execution-info layout.
Values are compiled bytes from archived predecessor/current sources and headers.

| Platform | Object | Previous | Current |
| --- | --- | --- | --- |
| windows | server | 320 | 320 |
| windows | job | 152 | 152 |
| windows | quota | 152 | 176 |
| windows | config | 280 | 280 |
| windows | context | 32 | 32 |
| windows | info | 104 | 128 |
| windows | http_server | 936 | 1056 |
| windows | http_config | 576 | 624 |
| windows | http_info | 224 | 296 |
| wsl | server | 320 | 320 |
| wsl | job | 152 | 152 |
| wsl | quota | 288 | 312 |
| wsl | config | 280 | 280 |
| wsl | context | 32 | 32 |
| wsl | info | 104 | 128 |
| wsl | http_server | 1072 | 1192 |
| wsl | http_config | 576 | 624 |
| wsl | http_info | 224 | 296 |

Private auth allocation sizes and optional peer-sidecar bytes, where printed,
exclude allocator metadata/rounding, child-token/native storage and input growth.
The raw head/descriptor/output capacities and alignment padding are additional.

windows: auth snapshot header 288 bytes, peer sidecar 32 bytes, embedded work node 56 bytes (already included in snapshot); base peer 400 bytes.

wsl: auth snapshot header 288 bytes, peer sidecar 32 bytes, embedded work node 56 bytes (already included in snapshot); base peer 400 bytes.

Single objects include Core; static/shared primary artifacts require separately
reported Core dependencies and are not interchangeable total footprints.

| Platform | Profile | Form | Primary file bytes | Core dependency file bytes |
| --- | --- | --- | --- | --- |
| windows | xrpc_http_server | single | 973687 | included |
| windows | xrpc_http_server | static | 80672 | 963470 |
| windows | xrpc_http_server | shared | 162545 | 866689 |
| windows | xrpc_http_server_tls | single | 1557913 | included |
| windows | xrpc_http_server_tls | static | 91310 | 1629948 |
| windows | xrpc_http_server_tls | shared | 173929 | 1345456 |
| wsl | xrpc_http_server | single | 1231456 | included |
| wsl | xrpc_http_server | static | 105824 | 1219128 |
| wsl | xrpc_http_server | shared | 87360 | 817392 |
| wsl | xrpc_http_server_tls | single | 1982648 | included |
| wsl | xrpc_http_server_tls | static | 119470 | 2099908 |
| wsl | xrpc_http_server_tls | shared | 101296 | 1291304 |

### Matched authorization observations

The four profiles keep workers, payload, custom credential/trace headers, one
warmed persistent connection and 1000 planned offers/s identical, varying TCP/TLS
and authorization off/on. Each has one warmup and all five measured 1000-document
samples. Every offer is admitted/completed with no failed/rejected samples.
The authorizer returns no identity and sets its output capacity to zero; these
profiles exclude the default 256-byte capacity and nonempty identity copy/erasure.
Latency includes planned-arrival delay and uses terminal-watch observation;
collector lag is separate. Process CPU includes all threads/load generation in
the existing observation-to-collector window. Native retirement, auth erasure and
mechanical shutdown can extend beyond it; these are not full-cleanup CPU costs,
saturation capacity or an isolated authorization/native/TLS cost measurement.

| Platform | Profile | Documents/s | Planned P50 us | Planned P99 us | Collector-window CPU us/offer |
| --- | --- | --- | --- | --- | --- |
| windows | xrpc_http_auth_tcp_0 | 1000.917 | 80.200 | 154.900 | 1046.875 |
| windows | xrpc_http_auth_tcp_1 | 1000.906 | 109.000 | 173.200 | 1125.000 |
| windows | xrpc_http_auth_tls_0 | 1000.861 | 97.300 | 182.100 | 1125.000 |
| windows | xrpc_http_auth_tls_1 | 1000.880 | 109.100 | 182.100 | 1125.000 |
| wsl | xrpc_http_auth_tcp_0 | 1000.839 | 177.319 | 289.604 | 1208.133 |
| wsl | xrpc_http_auth_tcp_1 | 1000.805 | 226.094 | 333.391 | 1255.530 |
| wsl | xrpc_http_auth_tls_0 | 1000.795 | 197.101 | 283.202 | 1228.646 |
| wsl | xrpc_http_auth_tls_1 | 1000.791 | 230.600 | 329.601 | 1266.434 |

The original 3% relative-MAD / 6% central-range noise limits are unchanged.
All final samples and failed gates remain recorded; no favorable selection or
stable pinned comparator is claimed. Performance/production acceptance remains
unachieved.

The first Windows measurement completed its runs but failed JSON export because
a derived relative spread was infinite at zero median. Its log and executable
hashes are retained under failed-performance-export; the exited process did not
preserve raw sample values, so those values cannot be recovered and are not
represented as retained evidence. After a separate reporting-only fix, the
complete Windows 5+1 matrix was rerun and WSL ran for the first time. The
serializer encodes only positive infinite derived relative ratios as the JSON
string "inf". All gate math remains unchanged; raw nonfinite samples remain
rejected. Thirteen tool tests pass on each platform. This required rerun does
not select favorable timing samples or relax the original thresholds.

windows: 46 failed noise checks; source `5cd3e325db84446e81a03bcd5f4f9372c19ff67f7d745efa08147ca15d10469c`, workload `8947cf0ccbcf5d4c488e5861639f136665e51b3ed17e4bb4ccc0fa3255bc975c`.

wsl: 40 failed noise checks; source `5cd3e325db84446e81a03bcd5f4f9372c19ff67f7d745efa08147ca15d10469c`, workload `8947cf0ccbcf5d4c488e5861639f136665e51b3ed17e4bb4ccc0fa3255bc975c`.

Evidence: `out/service-extensions/xrpc-http-auth/validation.json` binds final
inputs, binaries, peer/script hashes, probes, packages, layouts/sizes and 5+1
profiles. 754 predecessor source/artifact files are SHA-bound.
The committed validator requires clean HEAD/tree. API coverage: 83 functions,
161 constants/macros, 79 types, zero missing documentation. Normal maturity
keeps 17 implemented modules and only aggregate xrpc developing; strict release
therefore fails as expected.

The full four-product goal remains active: xrpc typed transport metadata,
mTLS/tickets, recovery/pooling, bidirectional/WebSocket/framed stdio, negotiation
and deferred handlers; Redis HA/tracking/dedicated modes; complete MQTT 5.0/3.1.1
client/session/store; Cron IANA/scheduler/durability. Pinned comparators, complete
cold/hot CPU/cycle/allocation/copy/synchronization/syscall costs, slow/contended/
fault/recovery matrices and native Windows/Linux >=72-hour mixed/fault runs remain
required. WSL results do not satisfy native Linux acceptance. Integration with
subsequent primary-checkout changes remains a separate verified step.



## P3: MQTT negotiated connection constraints (2026-11-01)

The xmqtt negotiation leaf now independently preflights CONNECT/CONNACK and
checks their cross-packet relationship for MQTT 5.0 and 3.1.1. It returns a
caller-owned capability value with separate send/receive packet/receive/alias
limits, keep-alive override, requested/effective expiry and server capabilities.
Rejected connections are normal typed results with reason/optional redirect
metadata. No automatic downgrade, reconnect or credential verification occurs.
The leaf selects both complete codecs; the existing minimal codec311 closure
still excludes MQTT 5 code. Public existing packet/property/config layouts are
unchanged; three APIs and five value types are added. No Core source is changed.

Cross checks cover Clean Start/Session and Session Present, assigned identifiers,
authentication method consistency (including present empty methods), unsolicited
authentication data and unrequested response information. MQTT 5 missing local
state with Session Present requires closing as STATE_ERROR, not pretending to
resume or necessarily blaming the broker. 3.1.1 retains caller mismatch policy.
A continuing MQTT 5 client must discard old local state when Session Present is
zero; this pure function does not delete or claim to recover that state.

The client packet check enforces peer packet/QoS/retain/alias/subscription caps,
AUTH method and original CONNECT expiry constraints. Requested subscription QoS
may exceed the server's maximum PUBLISH QoS. Disabled shared/wildcard support
does not prohibit removing old subscriptions. A server expiry override does not
erase the original CONNECT restriction on a positive DISCONNECT expiry.
Receive credit, id/ACK correlation, alias maps, auth exchange order, full inbound
constraints, live transports, session persistence/recovery remain client work.

Result text borrows the original CONNECT/CONNACK bytes and does not retain
property scratch. There is no successful/refused allocation, payload/text copy,
DOM or shared runtime. Negotiation performs two complete codec preflights; the
packet check performs one, followed by property/alias walks and relevant text
comparisons/scans. A fixed result value is copied. Diagnostics can allocate;
typed semantic decisions survive diagnostic OOM. Invalid API/descriptor/alias
errors leave Issue unchanged, while all negative statuses preserve Result/size.
These costs are not a zero-overhead or end-to-end zero-copy claim.

Tests include literal peer bytes, all version/clean/present/local-state tuples,
numeric boundaries and all 32 capability combinations, explicit accepted/refused
auth rules, lifetime/alias/wrapped spans and atomic outputs. Ten thousand warm
cycles preserve allocation counters and ambient-error identity; one-byte quota
success and diagnostic OOM retain classifications and return charged bytes to
zero. The offline example decodes literal peer packets and checks allowed/
forbidden publishes without broker/files/cleanup. It prints keep-alive=30,
max-qos=1, receive-max=4, alias-max=8, packet-max=1024, publish-bytes=13.

Windows and WSL pass six modular plus six single tests each (24 total), six
examples each (12 total), trim, full/minimal static/shared package consumers,
declaration compilation and three negotiation size forms per platform. WSL uses
O2 ASan/UBSan. This stateless leaf adds no worker/cancellation owner and has no
TSan/runtime-race claim. API coverage is 26 functions / 143 constants / 35 types,
zero missing docs. Strict maturity still refuses developing aggregate xmqtt.

Initial verification exposed a misleading-indentation error, a fixture that
misclassified invalid AUTH without Method as a negotiated restriction, and a
Windows single-header collision with the platform IN annotation macro. They
were corrected; the last fix renamed only local size variables. Both final
matrices were rerun on final hashes, with previous source/receipts preserved.
Size measurement rebuilds package libraries. Final evidence therefore relinks
and runs the minimal consumers against the final measured libraries and refreshes
their artifact hashes, checking size file counts rather than keeping stale
pre-measurement package hashes. This repair changes no runtime source or samples.

### Actual value layouts and size forms

Both platforms compile these sizes in bytes:

| Type | Bytes |
| --- | --- |
| config | 72 |
| result | 112 |
| issue | 12 |
| packet | 168 |
| codec | 56 |
| property | 40 |

Single objects contain Core plus extension. Static/shared primary artifacts
require the separately reported Core files; denominators are not interchangeable.

| Platform | Kind | Primary bytes | Core dependency bytes |
| --- | --- | --- | --- |
| windows | single | 144205 | included |
| windows | static | 81144 | 70922 |
| windows | shared | 105969 | 132795 |
| wsl | single | 172520 | included |
| wsl | static | 107234 | 87462 |
| wsl | shared | 72384 | 73792 |

### Descriptor workload observations

One cycle checks two handshake descriptors and one QoS-0 outgoing descriptor,
with a changing keep-alive value and observable checksum. Each version/platform
retains one warmup plus all five measured million-cycle samples. These are
public descriptor-validation cycles, not wire parsing, connection throughput,
queueing/tail latency, QoS/durable delivery or comparator parity. Allocation,
copy and scan boundaries are documented; full CPU/cycle/synchronization/syscall
accounting and representative pinned comparisons remain required.

| Platform | Version byte | Cycles/s median | Failed noise checks |
| --- | --- | --- | --- |
| windows | 5 | 5003928.084 | 0 |
| windows | 4 | 7292201.431 | 0 |
| wsl | 5 | 4705800.439 | 1 |
| wsl | 4 | 6846323.085 | 1 |

The original 3% relative-MAD / 6% central-range gates are unchanged; no favorable
retry or stable pinned baseline is claimed. Performance remains unaccepted.

windows: source `bde1a7d294684f64a763120efc350106752d00be590564bb8a67ea69b151baa8`, workload `7475bf64210f5cab846ddbfc125eced22b4a3a9aea9fca4a357bfff9f1074d22`.

wsl: source `bde1a7d294684f64a763120efc350106752d00be590564bb8a67ea69b151baa8`, workload `7475bf64210f5cab846ddbfc125eced22b4a3a9aea9fca4a357bfff9f1074d22`.

Evidence: `out/service-extensions/xmqtt-negotiation/validation.json` binds current
inputs/artifacts, final runtime/example/package/size/declaration results and 5+1
reports. 327 predecessor source/artifact files are SHA-preserved; the
committed validator requires clean HEAD/tree.

The full four-product objective remains active. Next MQTT work is the actual
bounded client/session/keep-alive/auth/QoS/alias/reconnect/store/transport chain,
with MQTT 5.0 primary and 3.1.1 compatibility. Redis HA/tracking/dedicated modes,
RPC metadata/mTLS/tickets/recovery/pooling/bidirectional/WebSocket/framed stdio/
negotiation/deferred handlers, Cron IANA/scheduler/durability and native Windows/
Linux >=72-hour mixed/fault qualification remain. WSL does not satisfy native
Linux acceptance. Integration with subsequent primary checkout changes remains
a separate verified step; this prerequisite completes neither xmqtt nor the goal.


## 2026-11-02 — bounded MQTT TCP/QoS-0 client vertical slice

Added the developing `xmqtt_client` leaf on public XRT TCP, Future V2 producer,
post and timer APIs. No Core source/API changes and no shared runtime were
introduced. Root xmqtt remains developing. This is a real network vertical
slice, not completion of the complete MQTT/session or four-product objective.

### Implemented behavior and ownership

- Explicit MQTT 5.0 default / 3.1.1 numeric TCP connection, copied CONNECT with
  basic credentials/Will, independently validated CONNACK negotiation and owned
  original/assigned identity plus retained CONNACK. No fallback or replay.
- Bounded immutable QoS-0 publishing, receiving copied immutable messages,
  server keep-alive override, PINGREQ/PINGRESP (including one-byte write queues),
  typed server DISCONNECT reason, finite deadlines, cooperative Future
  cancellation and abortive close. No PUBLISH is sent before successful CONNACK.
- Independent retained-object quota; retained results/messages do not keep a
  Client or Engine alive. Request/wire reservations last through actual producer
  and native-send retirement, possibly after Future completion. Startup wire is
  wiped after negotiation/stopping AND final native send-reference retirement.
- One host-worker protocol owner; bounded admission/read/write/packet work and
  old+new staging capacity during growth. No thread or Engine per Client.
  Overfull receive/object budgets close; already queued messages remain readable.
  Cancellation/timeout after any accepted prefix retires the connection.
- Write OK means complete native-buffer retirement while open, never broker
  acknowledgement/receipt/routing/storage/delivery. Native acceptance of any
  prefix leaves delivery uncertain. Blocking APIs reject all same-Engine workers;
  notification reentrant submit/take/close and real physical teardown are tested.
- Root and client dependency guards, modular/implementation/declaration headers,
  separate Core/extension static/shared packages and a runnable offline TCP
  example. Independent stdlib wire peer and fault/OOM/lifetime tests are separate.
  Public API inventory: 39 functions / 168 constants / 46 types, missing=0.

Full contracts, allocation/copy/synchronization boundaries and costs are in
`extlibs/xmqtt/docs/client.md`; commands and peer limitations are in
`extlibs/xmqtt/tests/interop/README.md`. Foreign ownership collectors are refused
for opaque private child graphs; this does not claim a new GC integration.

### Actual verification

Evidence root: `out/service-extensions/xmqtt-client/`. Final runtime matrices:
`windows-checks.json`, `wsl-checks.json`; package consumers: `windows-packages.json`,
`wsl-packages.json`; packaged network checks: `windows-package-wire.json`,
`wsl-package-wire.json`; consolidated bindings: `validation.json`.

- Windows GCC -O2 and WSL GCC -O2 + ASan/UBSan/leak checks: seven modular and
  seven single-header test binaries per platform (28 executions total).
- Client test groups cover MQTT5/311 roundtrip, one-byte limits, negotiation
  refusals/session/order, queue/object bounds, queued deadline/cancel, keepalive
  response/timeout, worker restrictions, notification reentrancy, factory and
  request OOM accounting, retained results/messages and physical teardown.
- Independent Python peer: 31 scenarios in each of six configurations (Windows
  and WSL monolithic, static package and shared package), 186 successful child
  process cases. Includes split/coalesced traffic, copied CONNECT credentials,
  assigned ids, copied PUBLISH payload, retained binary/User Property views,
  malformed/early/duplicate control packets, server DISCONNECT, local/advertised/
  private/queue/object limits, truncated EOF and close after a publish prefix.
- Eight static/shared package consumers across full/minimal client selection and
  two platforms; two actual offline client example executions. Both platforms'
  dependency trimming, declaration-only compilation and layout checks passed.
- Earlier test-fixture failures were preserved: worker callback code now consumes
  its supplied read buffer; abortive EOF assertions also accept TCP reset.
  A physical-request-bound test waits for real reservation retirement before
  resubmitting at a limit of one. No performance threshold was relaxed.

Actual x64 sizes, identical on both platforms: client config 264, info 144,
readiness result 136, write result 24 bytes. Existing packet/codec/property
layouts remain 168/56/40 bytes. These are type sizes, not complete runtime memory
or delivered binary size measurements.

The broader Python tool run reports `Ran 76 tests; failures=3, errors=1`.
The same errors/subtest failures were reproduced by the three affected methods
in an independently extracted, unchanged c16a44c2 tree at
`D:/codex-service-extensions-cache/xmqtt-client-baseline-c16a44c2`:
xhttp full/trimmed ABI fixtures, xhttp external-integration registration and
xredis TLS-auth example selection on Windows/Linux. Logs are `tool-tests.log`
and `baseline-tool-failures.log`. These baseline defects are not an all-green
repository gate, and the Redis example selection remains a follow-up within the
four-product goal. No xhttp or Redis source was changed by this MQTT stage.

### Still required

Client/runtime throughput and P99 benchmarks, pinned real-broker comparisons,
size/cold-path accounting, race/stall/fault qualification and native Windows /
native Linux >=72-hour production tests are not accepted or complete. WSL is
not native Linux acceptance. Existing 3% relative-MAD / 6% central-range noise
and 5% throughput / 10% P99 regression gates remain unchanged.

MQTT QoS1/2, subscriptions/id/flow state, aliases, enhanced authentication,
graceful DISCONNECT, TLS/WebSocket/DNS, reconnect, expiry and durable recovery
remain. Redis HA/tracking/dedicated modes and its example gate, RPC remaining
metadata/security/recovery/transport/deferred scope, Cron IANA/scheduler/durable
scope and primary-checkout integration remain required. The development worktree
is still isolated; D:/GIT/xrt master was verified clean at 93347eb5. No merge,
push or production-complete claim is made. The full objective stays active.


## 2026-11-02 — complete Redis example selection without library expansion

Added the `xredis_examples` asset aggregate. It selects `xredis` plus the
existing certificate-file/RSA helper, so the documented all-example command
actually builds all 14 example entry points. Ordinary `xredis` selection keeps
its original library closure. Both service-extension workflow matrices now
carry an explicit example suite, selecting the new aggregate for Redis and
retaining the other products' example roots. No runtime C/H, public API, type
layout, algorithm default or library implementation changed.

The existing all-example regression still requires exact coverage of every
`examples/**/main.c` on Windows and Linux; it now uses the documented aggregate.
A separate regression verifies the aggregate is exactly the union of library
and example-helper source closures, and that the normal library does not
select TLS_IDENTITY_RSA. Prior/current comparison against b460ad0b confirms
identical source, test, example, feature, link and include/header-root lists
for the ordinary library on both platforms: 149 source units. The dedicated
example aggregate has 154 units and all 14 main programs. Generated headers
and API reference are unchanged/current; no package/runtime or performance
change is inferred from this metadata fix.

Actual evidence: `out/service-extensions/xredis-example-selection/`.

- Seven aggregate asset regression tests passed. The broader build/package/API
  reference subset reports 65 tests, OK. YAML parsing verifies both workflow
  matrices select the documented suites; no remote workflow run is claimed.
- All 14 actual examples compiled and ran on Windows GCC -O2 and WSL GCC -O2
  (28 program executions). Online TLS examples deliberately print prerequisites
  with no arguments; their usage exits are not live network acceptance.
- Fresh source-bound native Windows and WSL drivers then connected to disposable
  real Redis TLS peers. In each mode, RSA client-certificate authentication was
  verified for TLS 1.2 and TLS 1.3. The **new aggregate's actual TLS-auth example
  binary** was also invoked with real ephemeral CA/chain/key files for both
  versions, asserting trusted-server/client proof, Redis initialization, PONG
  and complete cleanup: four client-driver and four live-example executions.
  Windows binaries used a WSL Redis peer and native Windows health checks;
  WSL results do not satisfy native Linux production qualification.
- Exact client build receipts and peer/server/compiler hashes are retained in
  `windows-client-build.json`, `wsl-client-build.json`, `windows-live.json` and
  `wsl-live.json`; `selection.json` records prior/current closure equality.
  Aggregate build logs bind the actual 14 example names and execution exits.
- Full tool rerun now reports `Ran 77 tests; failures=1, errors=1`. The Redis
  Windows/Linux example-selection subtest failures are repaired. The remaining
  xhttp integration-registration and public-ABI fixture defects are the already
  reproduced b460ad0b predecessor/c16a44c2 baseline failures, not a green global
  repository gate. Their remediation/integration remains outstanding.

No public Core change, new shared runtime, performance threshold change,
production acceptance, push or primary-checkout merge occurred. Full Redis
HA/tracking/dedicated scope, RPC remaining client/server transports/security/
recovery, MQTT QoS1/2/subscriptions/session/alias/auth/recovery/store/transports,
Cron IANA/scheduler/durable scope, runtime throughput/P99 comparisons and native
Windows/Linux >=72h qualification remain. The four-product goal stays active;
the next implementation work is the real MQTT QoS state chain.


## 2026-11-02 — real MQTT QoS-1 sender, bounded identifiers and PUBACK ownership

Extended the developing TCP client from QoS0 to fresh QoS0/1 publishing for
MQTT 5 and 3.1.1. The original four-result-field prefix is preserved; appended
Identifier/Reason/Puback expose the actual broker decision and retained ACK
properties. No public Core change or new shared runtime was introduced.

### Implemented protocol/resource behavior

- QoS1 Identifier=0 assigns an unused nonzero id; explicit ids are reserved.
  Busy ids yield normal OVERLOADED with output/ambient-error preservation.
  A lazily allocated bitmap and bounded hash buckets are charged to private
  bytes. Only the worker mutates ACK lookup chains; reservations are gated.
  Failed constructions release their id; real native/producer references keep
  physical request/wire/id reservations charged through their actual retirement.
- Receive Maximum limits started unacknowledged QoS1 publications. The first
  accepted native prefix consumes credit; matching positive/negative PUBACKs
  return it. QoS0 and PING traffic proceed while QoS1 waits at zero credit, with
  no partial-frame interleaving. No same-connection retransmission or fallback.
- Native write completion alone cannot complete QoS1. A correlated PUBACK
  resolves a typed accepted/rejected decision, including no-subscriber success.
  Unknown, duplicate, unsent, wrong-kind, zero-id or malformed ACKs cannot finish
  another request. Awaiting-ACK cancellation/deadline retires the connection
  with uncertain delivery. Peer acceptance is not subscriber/durability proof.
- Immutable PUBACK metadata owns all reason/repeated User Property views and
  survives Client/Engine/Future destruction through message retention. Metadata
  quota failure closes with LIMIT/MEMORY and preserves the scalar ACK decision.
  A doubly linked active list avoids an extra request scan on ACK removal;
  deadline/send scans and hash collision chains remain bounded, not universally
  constant work. QoS0 allocates no table and adds no descriptor copy.
- The offline example now asserts both QoS0 echo and QoS1 PUBACK/lifetime rules.
  Its healthy local fixture is separate from adversarial tests and independent
  stdlib wire peers; it is not a production broker or a shared library runtime.

### Verified current-source behavior and delivered forms

Evidence root: `out/service-extensions/xmqtt-qos1/`.

Windows GCC -O2 and WSL GCC -O2 ASan/UBSan/leak-check matrices passed all seven
modular and seven single-header binaries per platform (28 executions). Sender
coverage includes automatic/explicit ids, full-write-but-no-ACK pending state,
positive/negative ACK metadata lifetime, receive-window/QoS0/PING progress,
collision chains/out-of-order ACKs, duplicate/unknown/unsent ACKs, explicit
65535, cancellation/deadline, peer QoS restriction, ACK object limits and cold
allocation failure/identifier rollback. Original QoS0/network tests also pass.

The independent peers cover 31 original plus 37 sender scenarios in six forms:
Windows/WSL monolithic, static and shared package clients, 408 successful
process cases. Full/minimal static/shared consumers passed on both platforms
(eight consumers); both updated offline examples ran. Both platforms passed
trim/declaration/layout checks. Config/info/ready/packet/codec/property sizes
remain 264/144/136/168/56/40 bytes. Writer result grows 24 to 40 bytes with
its original 24-byte prefix offsets checked at compilation.

One separate functional 70,000-publication run per platform verifies actual
identifier rollover and checksum 2157421225. Those runs are not additional
performance samples. Size reports cover single/static/shared forms; size
measurement's rebuilt minimal libraries were followed by final consumer
relink/run and artifact-hash refresh, with final byte counts revalidated.
Single objects include Core; primary static/shared extension files require
separate Core dependency libraries. Their byte denominators differ.

| Platform | Single object bytes | Extension static bytes | Extension shared bytes | Core static dependency bytes | Core shared dependency bytes |
| --- | --- | --- | --- | --- | --- |
| Windows | 565929 | 114818 | 142788 | 475580 | 445513 |
| WSL | 725200 | 151692 | 110336 | 606060 | 409264 |

A final independent protocol check reproduced acceptance of forbidden PUBACK
Reason String/User Properties after CONNECT Request Problem Information=0.
The client now preserves this setting after startup-wire retirement and rejects
such ACKs before acknowledging a request; bare PUBACKs remain valid. Two new
MQTT 5 peer scenarios exercise both decisions across all six forms. Functional,
package, size and performance reports were rebuilt against the corrected source.
`before-problem-info/` preserves 371 exact prior source/artifact files, their
hash map and prior reports; its earlier measurements are historical evidence,
not selected current-source samples.

A temporary C: disk exhaustion caused a failed performance **compile**, before
measurement. Only checked task-owned output directories were SHA-preserving
moved to D: junctions; native/compiler temporary files now use the task's D:
cache. No other task's files were deleted and no runtime source was changed
for that environmental repair. The failed compile log is preserved.

Build/package/API-reference/performance-tool regression reports 78 tests, OK.
Previously reproduced xhttp scope/ABI failures remain outside that passing
subset; no green global repository gate is claimed.

### Initial runtime measurements — unaccepted

Added a separate loopback workload: one client/local healthy peer, two host IO
workers plus the observer, 32 in-flight publishes, 32-byte payload, 20,000
publications/sample. Timing starts after CONNACK and ends at observed PUBACK
completion before teardown; first lazy id-table allocation is included. The
allocation variants include client AND peer XRT block traffic, not all system
allocations/RSS. The plain and instrumented variants are separate binaries.

Each variant retains all five measured samples, with one configured/logged
warmup, original 3% relative-MAD / 6% central-range quality gates and unchanged
5% throughput / 10% P99 regression thresholds. Warmup numeric metric values
are not retained by the existing collector. The 20k runs are short observations;
longer representative loads, pinned comparators and complete cold-path/CPU/
synchronization/syscall accounting remain required. No favorable retry, lower
threshold, pinned baseline, broker parity or production performance is claimed.

| Platform | Version | Instrumented | Publish/s median | P99 us median | Failed metric noise gates |
| --- | --- | --- | --- | --- | --- |
| windows | 5 | no | 328504.320 | 160.900 | 1 |
| windows | 4 | no | 342601.269 | 172.200 | 2 |
| windows | 5 | yes | 306106.675 | 170.800 | 4 |
| windows | 4 | yes | 285405.233 | 187.400 | 3 |
| wsl | 5 | no | 87594.899 | 753.200 | 3 |
| wsl | 4 | no | 109613.965 | 477.400 | 3 |
| wsl | 5 | yes | 102018.955 | 538.300 | 1 |
| wsl | 4 | yes | 103641.069 | 584.700 | 4 |

Both complete reports exited 1 on noise checks. Performance remains unaccepted.
The allocator variants observed approximately 6.002 XRT blocks / 757–758 block
bytes per publish on Windows and 6.127–6.146 / 973–974 on WSL, including the peer and
amortized first table allocation. These do not establish library-only costs or
zero overhead. Source/workload fingerprints and exact compiler/environment,
arguments and sample arrays remain in the performance reports.

Full target remains active: incoming QoS1, QoS2, subscriptions/id/session/alias/
auth/reconnect/store/TLS/WebSocket/DNS, Redis HA/tracking/dedicated modes, RPC
remaining client/server security/recovery/transports/deferred scope, Cron IANA/
scheduler/durability, pinned performance and native Windows/native Linux >=72h
qualification. WSL is not native Linux acceptance. No merge into D:/GIT/xrt,
push or production-complete claim was made. This is a verified sender stage,
not completion of MQTT or the four-product objective.


## 2026-11-02 — bounded MQTT QoS-1 receiver and ordered PUBACK output

Extended the real TCP client to receive QoS1 on MQTT 5 and 3.1.1. This is another
verified client state-chain stage; the full four-library scope remains active.
No public Core source, shared runtime or public structure layout was changed.

### Implemented behavior and actual costs

- A decoded QoS1 publication is accepted only after an owned immutable copy is
  enqueued within message/object/byte limits. The success PUBACK carries the
  corresponding id and is independent of application Take/Retain/Release.
  It establishes volatile local acceptance, not business completion, durable
  storage, subscriber delivery or exactly-once effects. An accepted message may
  remain queued after transport failure before its ACK reaches the server.
- A lazy private ring holds at most min(MaxMessages, CONNECT Receive Maximum)
  uint16 ids; 3.1.1 uses the default 65535 bound. Its header plus two bytes/entry
  remain charged to private bytes until Destroy. It allocates no per-ACK library
  object or receive-id bitmap; native copied-send nodes remain separately bounded
  by Stream limits. The QoS0 receive path allocates no ring.
- PUBACKs are emitted in received order and in bounded chunks, including a
  one-byte native write limit. Their credit remains occupied until all four ACK
  bytes enter the ordered native stream. A locally full ring pauses parsing at
  the retained next frame; actual ACK progress wakes parsing. There is no busy
  repost when the native write queue is blocked and no early success ACK/drop.
- One explicit partial-request pointer ensures a PUBLISH is finished before a
  PUBACK/PING/new publication can be inserted, including an earlier queued QoS1
  request that just regained credit. A partial PUBACK/PING also finishes first.
  Inbound ACK traffic proceeds independently of exhausted outgoing QoS1 credit.
- Post-ACK duplicate/id reuse is delivered as a new message even with DUP=1.
  Incoming and outgoing packet-id namespaces remain independent. Retained
  incoming topic/payload/User/binary-property views survive Client/Engine teardown.
  Queue/object/private/OOM failures never acknowledge an unaccepted message.
- The offline example enables a separate fixture echo mode, verifies incoming
  QoS1 and waits for its peer-observed ACK before Close. Sender-only benchmarks
  leave echo mode disabled. The fixture remains outside the library runtime and
  independent adversarial peers; it is not a production broker/comparator.

### Verification and packaging

Evidence root: `out/service-extensions/xmqtt-qos1-receive/`.

Windows GCC -O2 and WSL GCC -O2 ASan/UBSan/leak-check matrices passed all seven
modular and seven single-header programs (28 executions). The new receiver tests
cover one-byte PUBACKs, post-ACK DUP, retained views, object/queue/private limits
and allocation failures both before and after cold ring allocation. A QoS0 marker
enters a budget on the actual IO worker before the coalesced incoming QoS1;
the tests assert typed MEMORY, no false ACK and zero surviving budget allocations.
An initial test used an older Info snapshot after its peer had observed the ACK;
it now refreshes that snapshot. Original failing logs are retained separately.

Independent stdlib peers now run 31 original, 37 sender and 30 receiver scenarios
per delivered form: Windows/WSL monolithic, minimal static and minimal shared,
588 successful process cases. Receiver scenarios additionally verify fragmented
input, ordered ACKs, callback-driven ring backpressure, same-id duplex traffic,
ACK after a partial outgoing frame, peer EOF during ACK, callback Close, malformed
zero ids, unsupported QoS2, MQTT5 receive-window violations and peer packet caps.
The success peer sends a barrier only after observing the required ACKs; failure
peers check there is no success ACK for an unaccepted message.

Full/minimal static/shared package consumers passed on both platforms (eight
consumers). Both updated offline examples ran. Final size rebuilds were followed
by minimal consumer relink/run and static/shared peer tests; source and artifact
hashes match their final receipts. Both platforms passed trim, declarations,
layout and generated-header/reference checks. Public sizes remain config264,
info144, ready136, write40 (old prefix24), packet168, codec56, property40 bytes.
The build/package/API-reference/performance-tool subset reports 78 tests, OK;
previously reproduced xhttp global scope/ABI defects remain unresolved.

| Platform | Single object bytes | Extension static bytes | Extension shared bytes |
| --- | --- | --- | --- |
| Windows | 567306 | 116260 | 144383 |
| WSL | 727368 | 153668 | 114480 |

Single objects include Core. Static/shared extension files require separate Core
libraries; final reports retain their independent sizes and hashes. These are
not equal byte denominators, and no native Linux qualification is inferred.

### Performance workload and remaining qualification

Added `xmqtt_client_receive` as a distinct combined QoS1 publish/echo workload:
window32, payload32, one client/local peer, two host IO workers and one observer.
Latency measures application observation of the outgoing PUBACK result AND the
matching incoming message. Throughput additionally waits for peer observation
of every receiver PUBACK. Connect/close and latency-array allocation are outside
timing; first cold sender/receiver flow allocations are included. Allocator
variants count client AND peer XRT block traffic, not RSS/all system allocations.
It is not an isolated receive benchmark or an independent broker comparison.

Each variant retains all five measured samples and one configured/logged warmup,
with unchanged 3% relative-MAD / 6% central-range quality gates and 5% throughput /
10% P99 regression thresholds. Numeric warmup values are not retained by the
existing collector. The 20k samples are short observations, not a long-run or
pinned-comparator acceptance test. No favorable retry or threshold change occurred.

| Platform | Version | Instrumented | Exchanges/s median | P99 us median | Failed metric noise gates |
| --- | --- | --- | --- | --- | --- |
| windows | 5 | no | 289557.122 | 149.700 | 0 |
| windows | 4 | no | 294106.834 | 150.200 | 2 |
| windows | 5 | yes | 223949.370 | 192.300 | 2 |
| windows | 4 | yes | 237392.951 | 215.600 | 1 |
| wsl | 5 | no | 61986.304 | 811.800 | 1 |
| wsl | 4 | no | 61055.539 | 800.200 | 0 |
| wsl | 5 | yes | 59577.889 | 815.400 | 2 |
| wsl | 4 | yes | 60098.718 | 818.000 | 2 |

Both full reports exit 1 on noise checks; performance remains unaccepted even
though the Windows MQTT5 uninstrumented group clears its individual noise gates.
All raw sample arrays, environment/compiler/arguments and source/workload
fingerprints are preserved. A separate functional 70,000-exchange run per platform
checks all paired messages/identifiers through rollover with checksum 2449965000;
its output is not an additional performance sample. Current-source/artifact
validation binds 588 peer cases and the complete package/test/measurement receipts.


Full MQTT qualification still requires QoS2, subscribe/unsubscribe shared ids,
sessions/expiry/alias/auth/reconnect/store/TLS/WebSocket/DNS, graceful draining
and MQTT5 diagnostic error DISCONNECT (including Receive Maximum reason 0x93).
Current error exits remain explicitly abortive, so full MQTT protocol/production
acceptance is not claimed. Redis HA/tracking/dedicated scope, remaining RPC
client/server transports/security/recovery/deferred scope and Cron IANA/scheduler/
durability remain. Pinned comparators, complete cold/hot CPU/synchronization/
syscall accounting and native Windows/native Linux >=72h qualification are still
required. WSL is development evidence only. No primary-checkout merge or push
occurred; this goal remains active and incomplete.


## 2026-11-02 — MQTT5 diagnostic DISCONNECT and bounded native queue drain

Added automatic error-disconnect output after accepted MQTT5 negotiation, with a
finite native drain phase. Application Close remains abortive and normal reason-0
application DISCONNECT is still pending. The full four-library goal stays active.
No public Core source change or new shared runtime was introduced.

### Protocol, deadline and ownership behavior

- Protocol failures select client-permitted reasons: malformed framing/codec
  input 0x81, unexpected state 0x82, Receive Maximum violation 0x93, receive alias
  violation 0x94, advertised packet-size violation 0x95. The minimal three-byte
  packet omits property length as permitted when its value is zero. No reason
  text/User Property, retained message or session mutation is created for it.
  3.1.1, pre-readiness, server DISCONNECT and local resource/transport/unsupported
  unfinished-flow exits retain abortive behavior. No server-only code is emitted.
- The first typed error/cause wins. New admission stops. Fully accepted native
  output frames may drain before the final DISCONNECT. A partial PUBLISH/PING/
  PUBACK, peer packet cap below three, EOF/reset or explicit Close forces abort;
  no diagnostic is inserted inside another frame and no packet follows it.
  Chunk/turn limits include one-byte output. Zero writable capacity waits for
  real low-water/drain events instead of spinning.
- A single host timer bounds diagnostic output AND native queue drain from its
  initial attempt, without extending on progress. The new positive CloseTimeout
  defaults to 5000ms. Timer/send allocation failure and expiry abort without
  replacing PROTOCOL with MEMORY, TRANSPORT, TIMEOUT or successful closure.
  Timeout is observed on owner progress and cannot preempt a stalled callback.
- Initial CLOSING now notifies outside the gate; callback Close can interrupt
  the automatic drain. Request outcomes can complete before native references
  retire. Wire/id/private reservations and Wait continue to follow real timer,
  producer, post, event and native send ownership. Queued receive messages survive.
- New Info fields expose intended LocalReason, installed-timer DrainStarted,
  accepted native DisconnectBytes (0..3) and independent DrainTimedOut. They do
  not prove peer receipt. A prior QoS0 OK still means local write-buffer retirement;
  it can precede later processing of an unread peer protocol error.

The developing API appends CloseTimeout and Info diagnostic fields: x64 config
264 -> 272 and Info 144 -> 160 bytes. Original prefix offsets are compile-checked,
but callers must rebuild; binary compatibility with old structure sizes is not
claimed. Ready136/write40/old-write-prefix24/packet168/codec56/property40 remain.
All module selections use the same new public layouts. Private bytes use the
actual larger Client/scratch allocation. Core timer/send nodes are separate
bounds; no extra library result object, worker or shared runtime is created.

### Current-source verification and artifacts

Evidence root: `out/service-extensions/xmqtt-error-disconnect/`.

Windows GCC -O2 and WSL GCC -O2 ASan/UBSan/leak checks passed seven modular and
seven single-header binaries per platform (28 executions). Diagnostic OOM tests
cover cold timer and copied-send allocation failures plus successful controls,
using the public Engine NodeCacheBytes=0 option so cached nodes cannot hide the
cold timer failure. They assert original PROTOCOL and zero live budget objects
through Client/Engine destruction; existing QoS1/receive OOM tests also pass.

Independent wire peers cover 31 original + 37 sender + 30 receiver + 23 diagnostic
scenarios in six forms (Windows/WSL monolithic, minimal static/shared): 726
successful process cases. Byte-exact reason/final-packet checks extend the prior
matrices. New cases check caps two/three, byte-sized writes, pre-readiness/311
abort, partial PUBLISH/PUBACK suppression, retained receive/outcome views, peer
reset, reentrant CLOSING abort and deadline expiry after an intentionally stalled
adversarial callback. That stall is fault injection, not production guidance to
block callbacks.

The large native output case preserves the native/kernel/peer distinction.
In this monolithic observation Windows retired its queue before the 50ms bound;
WSL required timeout fallback at about 53ms. Both preserve the first protocol
failure and valid outgoing prefixes. A native write may have completed before
the peer error is processed. The original overstrict Windows assertion and the
earlier WSL blocked-queue result are saved with 307 exact source inputs under
`before-close-callbacks/`. Current tests do not require a kernel-buffering outcome
that the public native API does not promise.

Full/minimal static/shared consumers passed on both platforms (eight consumers).
The healthy echo and new malformed-peer error examples ran on both (four program
executions). Size rebuilds were followed by consumer relink/run and final static/
shared wire suites; all final source/artifact hashes match. Both platforms passed
trim/declaration/layout/prefix/generated checks. The size-profile caption now
correctly includes incoming QoS1. The build/package/API-reference/performance-tool
subset reports 78 tests, OK; earlier xhttp global scope/ABI defects remain pending.

| Platform | Single object bytes | Extension static bytes | Extension shared bytes |
| --- | --- | --- | --- |
| Windows | 569666 | 119292 | 146713 |
| WSL | 730408 | 156916 | 114624 |

Single objects include Core; extension static/shared files need separate Core
libraries whose independent sizes/hashes remain in final receipts. These are
different denominators. WSL is not native Linux qualification.

### Close-cycle measurements — qualification remains pending

Added `xmqtt_client_disconnect`: whole connection/error/physical-retirement cycles
on one persistent local Engine, both protocol versions and separate allocator
variants. Each measured sample uses 100 cycles. Timing includes Client creation,
CONNECT/CONNACK, malformed peer header, MQTT5 diagnostic or 311 abort, observer
waits and Client/peer-stream retirement. Per-cycle latency ends at physical
Client Wait; aggregate cycle rate also includes destruction/reuse. Engine/listener
setup, latency-array allocation/sorting and final Engine teardown are outside
measurement. First cache use and client AND peer XRT allocations are included;
RSS/all system allocations are not. Public millisecond observer waits affect the
numbers. This is not isolated DISCONNECT speed, steady messaging throughput or
an independent/pinned broker comparison.

Each variant retains all five measured samples and one configured/logged warmup,
with unchanged 3% relative-MAD / 6% central-range gates and 5% throughput / 10% P99
regression limits. Warmup numeric values are not retained by the existing
collector. No threshold change, selected favorable retry, pinned baseline or
production acceptance is claimed.

| Platform | Version | Instrumented | Cycles/s median | P99 us median | Failed metric noise gates |
| --- | --- | --- | --- | --- | --- |
| windows | 5 | no | 32.055 | 16087.000 | 0 |
| windows | 4 | no | 32.176 | 16045.100 | 0 |
| windows | 5 | yes | 32.203 | 16096.100 | 0 |
| windows | 4 | yes | 32.281 | 16069.400 | 0 |
| wsl | 5 | no | 424.625 | 1340.100 | 0 |
| wsl | 4 | no | 424.472 | 1328.300 | 0 |
| wsl | 5 | yes | 424.295 | 1322.400 | 0 |
| wsl | 4 | yes | 424.355 | 1317.200 | 0 |

Both platform reports exit 0 on noise quality; this does not establish acceptable
latency or comparator parity. Windows observed ~16ms whole-cycle P99 through
physical Client Wait and ~32 cycles/s expose
a material pending cost in this whole-cycle workload. Investigate the fixed
millisecond polling in Client Wait/Destroy using a proper ownership completion
signal; do not substitute busy-spin or system timer-resolution changes to obtain
a better number. Both platforms
remain unaccepted for performance/production, and WSL is not native Linux.
Separate functional 1000-cycle runs passed on both platforms, asserting every
peer-observed diagnostic and checksum 129000. They are not additional performance
samples.
Raw arrays, compiler/environment/arguments and source/workload fingerprints are
bound by the final validation receipts.


Full MQTT scope still requires normal application DISCONNECT/graceful close,
QoS2, subscriptions/shared outgoing ids, sessions, expiry/alias/auth/reconnect/
store/TLS/WebSocket/DNS. Redis HA/tracking/dedicated, remaining RPC client/server
security/recovery/transports/deferred and Cron IANA/scheduler/durability scope
remain. Pinned comparators, complete hot/cold CPU/synchronization/syscall costs,
performance acceptance and native Windows/native Linux >=72h qualification are
still required. No primary-checkout merge, push or production-complete claim was
made. The complete four-product objective remains active and incomplete.


## 2026-11-02 — prototype-proven physical TCP retirement wait

The fc35b100 close-cycle observation (~16ms Windows P99 through physical Client
Wait) is a real pending performance cost, not an accepted production baseline.
Before changing Core, a real owned TCP connection deliberately held its actual
Drop callback. Both native Windows and WSL observed CLOSED while the owned Drop
was still pending with two real context references. Ten ordinary Sleep(1) calls
averaged about 14,432us on Windows and 1,073us on WSL in this prototype. Existing
logical close waits cannot replace actual retirement. Source, commands, object
hashes and outputs are retained in `out/service-extensions/xmqtt-physical-wait/`;
exact fc35b100 inputs, measurement reports and executable copies are preserved.

Implemented the opt-in Core `net_tcp_retire` module with one public operation,
`xrtNetStreamWaitRetired`. It waits for CLOSED and exactly the caller's one real
Stream reference. Aliases remain real; native/control/send/owned-callback tails
must physically drop their credits. It neither closes nor consumes/retains the
caller owner. Waiters share a caller-pinned owner and any same-Engine Worker is
rejected. It uses existing condition variables with one monotonic deadline, no
wait-node allocation, polling, graph freeze or new shared runtime.

The module adds embedded synchronization only when selected. Release CAS above
two keeps the ordinary >2 -> >=2 path out of the wait lock. The 2->1 publication
and 1->0 destruction share the predicate lock, preventing lost wakeups and a freed
notification tail. Embedded bytes and terminal synchronization are real costs;
no zero-overhead or accepted throughput claim is made. This Core stage does not
yet change MQTT Wait/Destroy or claim that its measured latency has improved.

### Validation and known pre-existing failures

Windows O2 and WSL O2 ASan/UBSan passed 18 modular programs each across
`net_tcp,net_tcp_future,net_tcp_retire,net_tcp_ownership_tests`, excluding exactly
two separately reproduced baseline failures below. Three selected single-header
programs ran on each platform: retirement, graph ownership and owned Stream
events. Minimal static/shared package verification and the actual retirement
test linked against each delivered library passed on both platforms (four
ordinary package consumers plus four packaged retirement executions). The new
retirement test runs one deliberately held Drop and 32 wake races per executable;
its 33 cases ran in modular, single, static and shared forms on both platforms.
It verifies real aliases, two concurrent borrowers of a stable caller owner,
zero/finite deadlines, same-Engine Worker rejection, allocation-free ready waits
and an inert Stream retained after Engine destruction.

An early ASan run caught a real use-after-free in the initial implementation:
`xrtNetEngineCurrent` scans the supplied Engine, which can already be destroyed
while a caller keeps the inert Stream. The final implementation obtains the
current live native XRT thread's data only after matching its immutable entry
procedure to the existing Engine Worker procedure. It compares that live context
without dereferencing the old Engine. This uses existing thread context, no new
TLS slot or public thread API. The regression includes a real alias retained
AFTER Engine destruction; it times out until that alias physically drops.

Dependency trimming now rejects both missing direct dependencies (`net_tcp` and
`cond`) in the PUBLIC header. Declaration-only single headers compile with strict
C11 warnings on both platforms. Generated features, amalgamation and network API
reference checks pass. The Python build/package/API-reference/performance-tool
subset reports 78 tests, OK. The generated network reference was stale at the
baseline: refreshing it reconciles 125 previously missing function entries plus
the one new operation (390 -> 516), and existing constants/types (284 -> 288,
128 -> 141). These regenerated entries are not newly implemented runtime scope.

The expanded regression run exposed two pre-existing failures. Both reproduce
with `net_tcp_retire` NOT selected on Windows and WSL, and their exact commands,
nonzero exits and logs are retained separately:

- `test_net_tcp.c` still expects a heap-backed off-Worker SendBuffer to fail. The
  existing public contract and unchanged send path explicitly permit that case.
- `test_net_tcp_invalid.c` retains a 576-byte base Stream size gate; the actual
  baseline Stream is already 704 bytes on both tested x64 platforms.

Those two programs were explicitly excluded from the 18-program run above;
neither a fully green Core suite nor their repair is claimed. The Stream storage
probe records base/retirement/future/future+retirement sizes of 704/744/760/800 on
Windows and 704/880/760/936 on WSL. Embedded synchronization thus adds 40 and 176
bytes respectively; placing it last avoids extra padding. The public structure
layouts are unchanged. The opt-in release path adds terminal locking/broadcasts,
constructor initialization and final primitive cleanup; no performance parity
or zero overhead is established. TinyCC and native Linux remain unverified.

Immutable input/artifact hashes, compiler commands, individual logs and the
separate baseline failures are indexed by
`out/service-extensions/xmqtt-physical-wait/validation.json`. All comparisons use
the same source inputs; the earlier fc35b100 MQTT performance data are preserved
as a separate baseline. MQTT runtime adoption and new message/close measurements
are deliberately absent from this Core-only stage.

The full xredis/xrpc/xmqtt/xcron scope, MQTT caller integration and performance
comparison, pinned comparators and native Windows/Linux >=72h qualification
remain required. WSL is not native Linux qualification. No primary-checkout merge
or push occurred, and the goal remains active and incomplete.


## 2026-11-02 — MQTT physical retirement notifications

Adopted the separately committed, prototype-proven Core `net_tcp_retire` module
in `xmqtt_client`. Client Wait/Destroy no longer use fixed Sleep(1) observations
or an ownership graph Count operation as a concurrent operational wait. The
Client owns an embedded mutex/condition dedicated to actual reference retirement.
A load/CAS drop above two returns without subsequent Client access; terminal
2->1 publication and notification share the wait lock. The admission Gate is
never acquired under that lock, since post rollback can drop a reference while
Gate is held. All primitives initialize/unwind/free with the Client, including
factory failures. Existing producer/post/timer/request/native/event owners remain
real obligations. After the caller-only Client state, the same remaining timeout
budget feeds `xrtNetStreamWaitRetired` for the native callback tail. The native
phase rounds that remaining budget to public milliseconds, rather than starting
a fresh full timeout. Timeout preserves the caller owner.

Multiple waiters borrow one stable caller owner; all borrowers and API entries
must finish before Destroy. Engine workers remain rejected, including a CLOSED
notification. Repeated Close on CLOSED returns without posting a new task.
Application Close stays abortive; this change adds no normal MQTT DISCONNECT,
protocol feature, worker, shared runtime, busy-spin or timer-resolution change.

### Verification and real costs

Windows O2 and WSL O2 ASan/UBSan passed the seven modular and seven selected
single-header programs on each platform. New tests hold the actual CLOSED
notification while two threads borrow the same Client: zero and finite waits
must time out without consuming its owner, then every borrower must wake after
release. Both protocol versions also run 16 wake races each, ready-wait and
repeated-Close checks under a one-byte XRT allocation budget, and ambient error
preservation. Existing fault/OOM, admission, cancellation, quota, first-error and
retained result/message lifetime cases pass. The initial build integration
attempt failed because the dependency substitution had not changed the manifest;
that was corrected and a source/feature closure preflight was added before the
successful complete runs.

Independent stdlib wire peers pass 726 cases: (31 basic + 37 sender QoS1 +
30 receiver QoS1 + 23 diagnostic DISCONNECT) across modular/static/shared on both
platforms. Minimal/full static/shared consumers pass on both (eight consumers).
Both documented offline examples run on each platform. Trim/declaration/layout/
feature/amalgamation/reference checks pass. The build/package/API-reference/
performance-tool subset reports 78 tests, OK. Two separate O2 1000-cycle runs
assert every peer diagnostic and checksum 129000, one on each platform; these
functional runs are not additional performance samples. Previously recorded Core
TCP assertion/size-gate failures and global xhttp gates remain unresolved.

The private Client sizeof probe compares the actual fc35b100 definition with the
current definition under unchanged public layouts: Windows 1088 -> 1128 (+40),
WSL 1160 -> 1336 (+176). Selecting Core retirement adds another 40/176 bytes per
native Stream, including the fixture peer. Client bytes are charged to the
private quota; Core storage is separate. Public config/info/ready/write/packet/
codec/property stay 272/160/136/40/168/56/40 bytes with previous prefixes. Primitive
initialization/cleanup, terminal locking/broadcasts and ordinary reference
load/CAS work are real costs, not zero overhead. Native OS internals are outside
the XRT allocation counters. Steady-message throughput and CPU parity are not
established by this close workload.

| Platform | Single object bytes | Extension static bytes | Extension shared bytes |
| --- | --- | --- | --- |
| Windows | 573732 | 119820 | 147927 |
| WSL | 737400 | 158900 | 114864 |

Single objects include Core; extension static/shared artifacts require the
separately measured Core dependencies. Post-size relink/run and final packaged
wire tests use the final artifacts; their source and binary hashes match.

### Close-cycle observation — noise gates fail, no performance acceptance

The benchmark and its observer source are byte-identical to the preserved
fc35b100 baseline. All five measured samples and one configured/logged warmup
remain, with unchanged 3% relative MAD / 6% central range and 5% throughput / 10%
P99 regression limits. The collector does not retain numeric warmup values.
Only the erroneous `publish/s` label for cycles_per_sec was corrected to
`cycles/s`; configuration equivalence excluding those four labels was checked.
The workload is still whole connection/error/physical-retirement cycles,
including fixture scheduling/polls and destruction, not message throughput or
an isolated close primitive. The baseline is a prior local revision, not a
pinned independent broker comparator. Windows and WSL collectors ran serially
after all functional builds; all raw samples are retained without favorable
retries or relaxed thresholds.

| Platform | Version | Instrumented | Old cycles/s | New cycles/s | Old P99 us | New P99 us | Noisy metric dimensions |
| --- | --- | --- | --- | --- | --- | --- | --- |
| windows | 5 | no | 32.055 | 139.619 | 16087.000 | 570.200 | 2 |
| windows | 4 | no | 32.176 | 745.448 | 16045.100 | 363.100 | 4 |
| windows | 5 | yes | 32.203 | 162.060 | 16096.100 | 463.300 | 3 |
| windows | 4 | yes | 32.281 | 1079.508 | 16069.400 | 343.500 | 3 |
| wsl | 5 | no | 424.625 | 1961.904 | 1340.100 | 318.000 | 1 |
| wsl | 4 | no | 424.472 | 3484.017 | 1328.300 | 315.700 | 2 |
| wsl | 5 | yes | 424.295 | 1687.775 | 1322.400 | 346.500 | 0 |
| wsl | 4 | yes | 424.355 | 4332.756 | 1317.200 | 281.000 | 3 |

Both platform collectors exit 1 on noise quality. The observed median P99 values
are lower than the preserved polling baseline, but these runs establish neither
performance qualification nor a stable comparator/regression verdict. Every
failing metric, raw array, compiler/environment argument and benchmark/source
fingerprint remains in `out/service-extensions/xmqtt-retirement/validation.json`
and the indexed reports/logs. The 100-cycle samples and remaining fixture polling
need a separate sustained measurement investigation; no passing retry was
selected. WSL remains development evidence, not native Linux qualification.

The full objective remains active and incomplete: normal application graceful
DISCONNECT, MQTT QoS2/subscriptions/shared identifiers/session/expiry/alias/auth/
recovery/store/TLS/WebSocket/DNS, Redis HA/tracking/dedicated, remaining RPC
security/recovery/transports/deferred, Cron IANA/scheduler/durability, pinned
comparators and full hot/cold CPU/synchronization/syscall accounting, accepted
throughput/tail latency and native Windows/native Linux >=72h runs remain. No
primary-checkout merge, push or production-complete claim is made.


## 2026-11-02 — explicit zero-timeout MQTT retirement polls

Final review after `1bb84eb` found that the zero-timeout path relied on a later
monotonic clock read advancing beyond nextafter(now). Make zero explicit in both
Client-reference and native phases, so an unchanged timer tick cannot turn a
poll into a one-millisecond condition/native wait. Positive and infinite waits
keep the previous remaining-budget behavior. The caller owner, public layouts,
protocol handling and module closure are unchanged.

Windows O2 and WSL O2 ASan/UBSan passed the existing full Client lifetime/fault
suite in modular and single forms (four program executions), including zero
polls on open/retired Clients, finite waits on held CLOSED callbacks, multiple
borrowers, 32 wake races, OOM and original-error cases. The generated single
implementation matches. This follow-up is verified locally; the complete package
and performance evidence above belongs specifically to `1bb84eb`, rather than a
new measurement claim for this two-guard correction. Its source snapshot and
original artifacts are retained, and all new commands/hashes are under
`out/service-extensions/xmqtt-retirement-zero/`. No performance rerun was selected
and neither noise failure nor pending full/native qualification is waived.
The complete four-library objective remains active and incomplete.


## 2026-11-02 — bounded application MQTT DISCONNECT

Implemented `xmqttClientDisconnectAsync` and its off-worker blocking counterpart
for MQTT 5.0 and 3.1.1. After accepted CONNACK, admission atomically stops new
requests only after its encoded snapshot, quota result, Future/producer token
and worker wake are committed. A NULL packet means normal reason zero. One
operation reserves one existing request slot and its wire/object budgets; quota,
validation, peer-cap/expiry and OOM failure leave the caller output unchanged and
do not turn an open connection into a closing one. Duplicate/closed admission
and ordinary overload preserve ambient error. The request metadata and typed
write result are reused; there is no new runtime, thread, operation dispatch or
public structure layout change.

The sole protocol owner keeps draining prior accepted PUBLISH outcomes, partial
PUBLISH/PING/receiver-PUBACK frames and QoS1 acknowledgements before sending the
final DISCONNECT. Each earlier request retains its own typed outcome/deadline;
unsent expiry/restriction need not make a healthy connection fail. The shutdown
has one finite API-entry deadline through encoding, all drain work and native
queue close. Zero is explicitly expired, independently of timer precision.
Explicit Future cancellation wakes the owner; raw token requests are sampled on
progress/deadline. Cancellation, abortive Close or the first connection/resource
failure interrupts the operation without reopening admission or appending an
additional packet after a partial frame. Once final output starts, keep-alive
processing stops. Existing timer/send nodes and actual producer/native leases
remain real obligations through physical retirement.

A successful result means normal native drain/close, not a broker acknowledgement,
Will guarantee or proof that every preceding PUBLISH succeeded. The Future keeps
AcceptedBytes, requested Reason, first Status/Cause, Identifier=0 and Puback=NULL;
any accepted prefix remains MAY_HAVE_REACHED_PEER even on OK. Retained results
outlive Client and Engine. Wait additionally observes all actual reference tails.
The final Status snapshot and CLOSED publication share the admission gate, so a
racing explicit Close cannot produce mismatched Future and Info outcomes.
Application callbacks can request asynchronous shutdown; blocking helpers reject
all workers of the same Engine.

The independent protocol basis is OASIS MQTT 5.0 sections 3.14/3.14.2.2.2 and
MQTT 3.1.1 section 3.14, linked in the client documentation. The wire peer checks
normal/0x04 final bytes, reason/user-property snapshot lifetime, original CONNECT
expiry constraints, exact peer packet caps, ordering and client-initiated EOF.
The 0x04 fixture verifies the requested wire reason, not actual broker Will
publication. Pinned broker Will/session interoperability remains required.

### Final verification

Windows O2 and WSL O2 ASan/UBSan pass seven modular plus seven selected single
programs each. Added normal/byte-at-a-time shutdown, held QoS1 ACK drain,
per-request drain timeout, cancellation, explicit abort, first protocol error,
pre-CONNACK and quota rejection, allocation-budget rollback/success/lifetimes,
retained outcome and 32 explicit-Close completion races. The race requires the
Future Status and final Info CloseStatus to agree, allowing either legal winner.

Independent stdlib socket peers pass 1070 cases: the existing (31 basic + 37
sender QoS1 + 30 receiver QoS1 + 23 diagnostics) in modular/static/shared on both
platforms, plus 43 application-shutdown cases in modular/single/static/shared on
both. New cases include zero expiry, fragmented receiver ACK, partial prior
PUBLISH, immutable properties, peer size/expiry constraints, held-ACK drain,
quota/admission, duplicate shutdown, partial final-frame abort/timeout, immediate
and posted reentrant Close, slow reader and final EOF with no further packets.
A before-CONNACK local abort may legitimately cancel TCP before peer accept;
the fixture permits only that absent-connection case, not an accepted socket leak.

The initial receive-ACK fixture incorrectly classified every `_ack` name as an
outgoing QoS1 test and added an unintended PUBLISH. Both platform failures and
the previous driver are retained under `before-case-classification-fix/`;
explicit case classification fixes the fixture without dropping the ACK case.
A successful earlier candidate is preserved under `before-explicit-zero/`.
Zero-deadline and final-status publication were then tightened, followed by the
final complete matrix; no stale earlier artifacts support this final stage.

Minimal/full static/shared package consumers pass on both platforms (eight).
The original healthy echo/error examples and the new offline graceful example
run on both (six executions). The new example checks QoS1 PUBACK, final normal
DISCONNECT, peer EOF, physical Stream/Client/Listener/Engine cleanup and a retained
result. Both new entry points compile through declaration-only single headers.
Trim/feature/amalgamation/reference/layout/prefix checks pass, and the existing
build/package/API-reference/performance-tool subset reports 78 tests, OK. The
prior Core TCP assertion/size-gate and global xhttp failures remain unresolved;
no fully green global suite is claimed. WSL is development evidence, not native
Linux production qualification.

Client private sizeof grows 1128 -> 1144 on Windows and 1336 -> 1352 on WSL
(+16 bytes), charged to MaxPrivateBytes. The request metadata is unchanged; one
accepted shutdown allocates its owned request/wire, result/Future/token and uses
existing native nodes. Snapshot validation/encoding, quota/producer bookkeeping,
additional state/deadline branches and timer/send work are real costs. Public
config/info/ready/write/packet/codec/property remain 272/160/136/40/168/56/40 bytes.
No zero-overhead or steady-message throughput parity is inferred.

| Platform | Single object bytes | Extension static bytes | Extension shared bytes |
| --- | --- | --- | --- |
| Windows | 578976 | 124314 | 151120 |
| WSL | 741344 | 161966 | 119080 |

Single objects contain Core; extension static/shared outputs require separate
Core files whose sizes/hashes remain in receipts. Post-size consumer relink and
all final packaged wire runs use the final artifacts. Exact commands, inputs,
binaries and per-case outputs are indexed by
`out/service-extensions/xmqtt-graceful/validation.json`. Reproducible single wire
and offline example commands are in `extlibs/xmqtt/docs/client.md`.

There is no fresh performance acceptance for this new operation. Existing noise
failures are not relaxed or replaced by functional counts. Dedicated graceful
shutdown load/CPU measurements, complete hot/cold allocation/copy/synchronization/
syscall accounting, pinned throughput/P99 comparisons and native Windows/native
Linux >=72h runs remain. MQTT QoS2/subscriptions/shared outgoing ids/session/
expiry/alias/auth/reconnect/store/TLS/WebSocket/DNS, Redis HA/tracking/dedicated,
remaining RPC client/server security/recovery/transports/deferred and Cron
IANA/scheduler/durability remain required. No primary-checkout merge or push
occurred. The complete four-library objective remains active and incomplete.


## MQTT current-connection QoS-2 sender (2026-11-02)

Baseline: `6d3ae7aca68a0279e1abed60197805f66b0ae3c5`. The developing client
now admits fresh QoS-2 PUBLISH on MQTT 5.0 and 3.1.1, using the existing bounded
QoS-1/2 identifier table and native-owned request lifetime. Positive PUBREC
schedules immutable inline PUBREL; successful final completion waits for
PUBCOMP. Send credit remains held through PUBCOMP or negative PUBREC. Negative
PUBCOMP (including id-not-found) describes the protocol result, not proof of
non-delivery. There is no PUBLISH replay, timer retransmission, reconnect or
session store in this slice. Unknown/early PUBCOMP cannot complete a different
flow or mint credit; unknown PUBREC/wrong-kind/unsent ACKs close with PROTOCOL.

Repeated successful PUBREC coalesces while release is unsent/partial and repeats
the immutable frame after a complete release. A terminal ACK arriving during a
partial repeat defers logical completion until that frame boundary. Native
release tails retain the actual request and its identifier; no borrowed stack
buffer is passed to TCP. Timeout/cancellation/Close after an accepted prefix
retires the connection with uncertain delivery. Graceful shutdown drains the
final ACK and partial release before its final packet. No Core or new shared
runtime changes were needed.

The typed result appends independent owned Pubrec/Pubcomp messages and scalar
receipt/completion flags. Scalar reasons/decisions are recorded before metadata
allocation; quota failure may therefore return LIMIT with the protocol decision
still available. ACK properties survive Client/Engine destruction and can be
retained independently. Duplicate positive receipts make no additional ACK
copy. Original PUBLISH wire stays charged through actual request retirement,
even after PUBREC. AcceptedBytes excludes release bytes.

Measured layout on both tested x64 ABIs: write result 40 -> 64 bytes, preserving
the complete old prefix (Puback offset 32; Pubrec starts at 40); request 96 ->
104 bytes. Those requested-size costs also affect QoS-0/1 and shutdown. Private
Client remains 1144 Windows / 1352 WSL bytes; public config/info/ready/packet/
codec/property remain 272/160/136/168/56/40. Struct sizes do not measure allocator
buckets, RSS, all copies or throughput/P99. PUBREC and PUBCOMP each consume an
owned message in the existing object budget. No zero-cost claim is made.

Verification: native Windows GCC O2 and WSL GCC O2 with ASan/UBSan/leak halt for
modular/single programs. All 7 modular + 7 single programs per OS pass, together
with minimal/full static/shared package consumers, four offline client examples
per OS, declaration-only/prefix layout, trimming, generated features/single/API
checks and 78 build/package/API-reference/performance-tool regression tests.
The new QoS-2 stdlib socket peer has 37 cases on each of modular/single/static/
shared, each on both OSes (296 new cases). Existing basic/QoS1/receive/error
cases remain 121 x 6; graceful cases remain 43 x 8: 1366 total independent wire
cases, 28 modular/single programs, 8 package consumers and 8 example executions.

Cases include both versions, byte writes, repeated/late/premature/wrong-kind
ACKs, unchanged original PUBLISH, credit held through completion, QoS0 bypass,
negative receipt/completion, phase timeout/cancel/close, graceful ordering,
metadata quota decisions and retained owned properties. Initial fixture failures
assumed peer receipt from a local QoS0 Future or required PUBREL ahead of legal
QoS0 traffic; both observation/order assumptions were corrected, with prior
receipts retained. No runtime workaround was added for those assumptions.

Final rebuilt artifact file bytes (single includes Core; static/shared extension
files require separately recorded Core artifacts):

| Platform | single object | extension static | extension shared |
| --- | --- | --- | --- |
| windows | 580234 | 125094 | 152202 |
| wsl | 743608 | 164414 | 119128 |

Exact commands, source/compiler identities, final artifact hashes and per-case
results are indexed by `out/service-extensions/xmqtt-qos2-send/validation.json`.
The runnable `examples/qos2_send/main.c` and independent single-wire reproduction
commands are documented in `extlibs/xmqtt/docs/client.md`. Post-size consumers
and wire fixtures use the final packages.

There is no fresh performance acceptance for this slice. Previous noise-quality
failures remain unaccepted; frozen thresholds are unchanged. Full hot/cold
allocation/copy/synchronization/syscall/CPU accounting, QoS-2 load and pinned
throughput/P99 comparisons, native Linux and native >=72h fault/mixed-load runs
remain required. WSL is development evidence, not native Linux qualification.
MQTT QoS-2 receive, subscriptions/shared ids/session/expiry/alias/auth/reconnect/
store/TLS/WebSocket/DNS, Redis HA/tracking/dedicated, remaining RPC security/
recovery/transports/deferred handlers and Cron IANA/scheduler/durability remain
required. There was no primary-checkout merge or push. The complete four-library
objective remains active and incomplete.


## MQTT volatile QoS-2 receiver (2026-11-02)

Baseline: `3e749c88c83aba39043b6f76ca660ec0fe0f9544`. The developing client
now accepts incoming MQTT 5.0/3.1.1 QoS-2 exchanges: one owned message enters
the bounded application queue before PUBREC; the incoming id remains active
through complete PUBCOMP acceptance into ordered native output. Application
Take/Release does not return protocol credit. QoS-1 pending PUBACKs and active
QoS-2 ids share CONNECT Receive Maximum; incoming/outgoing ids are independent.

Active duplicate PUBLISH repeats/coalesces PUBREC without another local enqueue,
message copy or Received increment, regardless of DUP. PUBREL completes the
sequence without another delivery, including an early release queued behind its
PUBREC. Unknown/repeated PUBREL receives minimal PUBCOMP (unknown reason 0x92 in
v5); it creates no application message or credit. After PUBCOMP, PUBLISH may
reuse the id even with DUP=1. Forbidden properties, cross-QoS active-id reuse,
peer packet caps and receive-window violations retain typed failure semantics.
This is volatile local enqueue on the current connection, not crash-safe or
business exactly-once delivery. Session/persistence/reconnect remain required.

A private direct-index state table is lazy on the first QoS-2 PUBLISH or unknown
PUBREL. Its fixed x64 header/state size is 65576 bytes, plus four bytes for each
of 2*min(MaxMessages,ReceiveMaximum) ACK slots: 66088 requested bytes at default
settings, 65584 with Receive Maximum=1. It is charged to MaxPrivateBytes and
cached through Destroy. It stores no message reference and has at most 65535
active ids. Application queue/retained-object count and byte limits remain
independent. Private Client adds one pointer: Windows 1144 -> 1152; WSL 1352 ->
1360. Request remains 104; public config/info/ready/write/packet/codec/property
remain 272/160/136/64/168/56/40. Struct sizes do not establish allocator/RSS or
throughput/tail cost.

The finite control queue coalesces queued duplicates and uses native copied
output; native buffers may still allocate. Complete ACK frames alternate
between ready QoS-1 and QoS-2 classes, preventing a replenished QoS-1 ring from
monopolizing small write budgets. Partial frames keep priority, with byte/chunk/
packet work bounds. New QoS-2 ids also scan the bounded pending QoS-1 ring to
reject cross-QoS collisions. There is no claim of zero overhead for inactive
paths, duplicate traffic, cold allocation or control scheduling. Graceful
shutdown drains accepted incoming flows and ACKs within its original finite
deadline; timeout/cancel/Close aborts without fabricating completion. Queued
messages remain available independently of transport failure.

Two negative controls are retained. The pre-fix receiver fails the deterministic
ACK-fairness socket case (a buffered QoS-1 backlog delays PUBREC). Separately, a
test-only readiness barrier proves that accepted submissions arriving after a
worker's initial collection could be stranded if graceful Close failed in that
same turn. The failure path now recollects accepted submissions before settling
the disconnect request. The gated test verifies both typed Futures and balanced
physical accounting. Logs are `windows-before-ack-fairness.json/.log` and
`windows-before-queued-close-fix.log`; the previously observed WSL single graceful
abort failure is retained in `before-final-boundaries-wsl-single-graceful-wire.log`.
An existing diagnostic fixture now waits for the peer callback before asserting
its final reason; local physical close alone cannot prove that observation. A
local verification-helper output-path typo was also corrected, without changing
library behavior. All final runs below use the corrected inputs.

Verification: native Windows GCC O2 and WSL GCC O2, with ASan/UBSan/leak halt for
modular and single programs. All 7 modular + 7 single programs per OS, minimal/
full static/shared consumers, five offline client examples per OS, declaration-
only/prefix layouts, trim and generated feature/single/reference checks pass.
The separate stdlib receiver peer now has 66 cases on modular/single/static/
shared for each OS (528 new cases). Existing basic/QoS1/receive/error cases
remain 121 x 6; graceful cases 43 x 8; sender cases 37 x 8: 1894 total independent
wire cases, 28 modular/single programs, 8 package consumers, 10 example executions.
The old receive peer's two unsupported-QoS2 expectations now verify the real
handshake and peer-observed barrier. The 78 build/package/API-reference/
performance-tool regression tests also pass.

Cases cover duplicate/late/reused/early/unknown/negative release, id boundaries,
shared windows, queue/object/byte/private limits, ACK fairness and backpressure,
fragmented and partial mixed ACKs, duplex ids, interrupted graceful drain and
retained topic/payload/properties after Client/Engine destruction. Unit allocation
budgets cover failure before state allocation, between state/message allocation,
a success control and zero live budget owners after teardown. No Core or new
shared runtime changes were required. `examples/qos2_receive/main.c` and the
single-wire reproduction command are documented in `extlibs/xmqtt/docs/client.md`.

Final rebuilt file bytes (single includes Core; extension static/shared require
separately recorded Core artifacts):

| Platform | single object | extension static | extension shared |
| --- | --- | --- | --- |
| windows | 584019 | 128206 | 155000 |
| wsl | 747096 | 167814 | 123352 |

Exact compiler commands, source identities, final artifact hashes and per-case
outputs are indexed by `out/service-extensions/xmqtt-qos2-receive/validation.json`.
Post-size consumers and wire runs use the final libraries.

Dedicated QoS-2 throughput/P99/load/CPU work and complete hot/cold allocation/
copy/synchronization/syscall accounting remain required. Existing performance
noise failures remain unaccepted, with frozen thresholds unchanged. Pinned
broker/comparators, native Linux and native >=72h fault/mixed-load qualification
remain; WSL is development evidence. MQTT subscriptions/shared ids/session/
expiry/alias/auth/reconnect/store/TLS/WebSocket/DNS, Redis HA/tracking/dedicated,
remaining RPC security/recovery/transports/deferred handlers and Cron IANA/
scheduler/durability remain required. No primary-checkout merge or push occurred.
The complete four-library objective remains active and incomplete.


## QoS-2 complete-exchange CPU/heap/timing workload (2026-11-02)

Runtime baseline remains `77036057cdc6ca6577cf2056b54b5e119cbd3f89`. No product
C/header or generated-runtime changes were made in this stage. The new pure-C
`bench/bench_client_qos2.c` and two performance profiles cover complete sender and
duplex echo exchanges on one healthy loopback connection. They support MQTT
5.0/3.1.1, QoS 1/2, windows 1..64, separately instrumented heap traffic and cold/
internal-warm state. This supplies a workload and local observations, not a
pinned comparator or accepted release baseline.

Sender latency ends at observer-visible final ACK; duplex additionally requires
the retained echo and peer-observed final receiver ACK. Taking a message does
not finish the measurement. Phase wall/CPU additionally includes observation
release and physical client-request retirement; copied native ACK tails can
remain until recorded connection teardown. Sequence stamps, separate request/
echo id spaces, phase checksums, owned data/ACK results and terminal counters
prevent a fast result from skipping either handshake. The peer decodes requests,
re-encodes/copies echoes and ACKs; its costs and observer polling are included.

Frozen profiles: 32-slot/32-byte closed-loop traffic; hot uninstrumented counts
200000 after 4096 internal warmup operations; cold/hot instrumented counts 20000
with 0/4096 internal warmup. Instrumented and uninstrumented binaries select
memory stats independently and publish that fact. The existing five measured
repeats / one process warmup, relative MAD 3%, central range 6%, and 5% throughput/
10% P99 regression limits were preserved. No regression comparison was run
against a pinned baseline. Process CPU uses GetProcessTimes on Windows and
CLOCK_PROCESS_CPUTIME_ID in WSL, for observer plus IO/fixture threads. Logical
MQTT wire is 4 frames/60 bytes for v5 sender and 8/120 for duplex (v4: 59/118),
excluding handshake and TCP/IP/kernel framing. Full scope/cost/cleanup commands
are in `extlibs/xmqtt/bench/README.md`.

Validation: 168 executions per OS across modular/single/static/shared forms
(336 total), including eight >65535-operation identifier-wrap cases per OS,
QoS1/2, version4/5, windows1/64, cold/warm and instrumentation modes. Windows uses
GCC O2; WSL modular/single use O2 ASan/UBSan/leak halt; reused package libraries
are the previously verified uninstrumented O2 artifacts. Runtime sources and
package hashes were checked against the baseline; the manifest runtime graph is
unchanged, with only a benchmark asset added. All instrumented connection
lifetimes balance XRT block counts/bytes after teardown. Selected malloc/calloc/
backing counters and separate setup/warmup/measurement/teardown/lifecycle wall/
CPU, fixture/observer storage and overload attempts are retained. The 24 profile
smoke executions, generated feature/single/reference checks on both OSes and 78
build/package/API-reference/performance-tool regression tests pass.

Windows and WSL formal collectors ran serially. Every platform completes 12
variants, one process warmup plus five measured runs each: 144 full outputs,
including all 120 measured samples. Raw stdout also retains diagnostics not used
as gate metrics. This is 9.6 million measured exchanges (plus explicit warmups),
not 120 independent production qualifications. Each platform's collector exits
1 on the unchanged noise gate: Windows has 15 failing metric series (3 throughput,
5 P99, 7 process-CPU); WSL has 12 (2 throughput, 8 P99, 1 P50, 1 process-CPU).
All such samples remain in the record; there was no favorable retry or threshold
relaxation. Windows process-CPU deltas in this experiment are multiples of
15625000 ns, making the 20000-operation CPU values step in 781.25 ns/exchange.
That observed quantization partly explains CPU quality failures; throughput/P99
also fail and are not excused by it. Short smoke CPU zeros are accounting
resolution, not a zero-cost result. Longer CPU measurement remains necessary.

Uninstrumented hot medians, **unaccepted local observations** (P99 in us, process
CPU ns/exchange; these include fixture and observer and must not be interpreted
as native Windows-vs-Linux platform parity):

| Environment | Version | Workload | exchanges/s | P99 us | process CPU ns/exchange |
| --- | --- | --- | --- | --- | --- |
| windows | 5 | sender | 236083 | 196.8 | 11484 |
| windows | 4 | sender | 239329 | 203.6 | 11172 |
| windows | 5 | duplex echo | 166332 | 326.6 | 16562 |
| windows | 4 | duplex echo | 164649 | 343.0 | 16719 |
| wsl | 5 | sender | 63006 | 772.6 | 45162 |
| wsl | 4 | sender | 64177 | 763.5 | 44318 |
| wsl | 5 | duplex echo | 34044 | 1556.5 | 85733 |
| wsl | 4 | duplex echo | 34524 | 1502.1 | 84336 |

Windows hot instrumented requested XRT block traffic observes 8 blocks and
1072/1071 bytes per v5/v4 sender exchange, and 9 blocks and 1320/1318 bytes per
duplex exchange. These are logical XRT block requests for client plus fixture,
not eight/nine OS allocator calls, RSS, all copies or syscall counts. Cold
allocation and full lifecycle values are retained separately. Timing from those
instrumented modes is not uninstrumented throughput. There is no zero-overhead
or broker/reference parity claim.

WSL's initial preflight could not resolve the Windows-form gitdir before any
measurement launched. The baseline manifest was exported with Windows Git,
checked against the prior proof (including line endings) and then read as data
on both platforms. WSL's unavailable Git revision/dirty fields remain null in
the raw report; runtime/code file hashes still verify against the known baseline
and frozen workload. Only preflight access changed; Windows samples were not
rerun. The deterministic environment error is recorded in `wsl-git-preflight.log`.

Portable complete measured arrays/diagnostics, environment and source/tool
fingerprints are committed in `extlibs/xmqtt/bench/results/qos2-local-20261102.json`.
All warmup/measured stdout, exact commands, binary hashes, four-form execution
receipts and the source proof are indexed by
`out/service-extensions/xmqtt-qos2-performance/validation.json`. The collectors'
actual exit codes are preserved, not replaced by the orchestration exit code.

These numbers remain unaccepted: shared host/power/affinity state is unpinned;
closed-loop single-connection load does not establish externally paced load or
production overload/tail distributions. System-wide interrupt/DPC/softirq CPU,
detailed synchronization/copy/syscall tracing, pinned minimal-C/reference-client
and broker comparisons, native Linux and >=72h native mixed/fault runs remain.
The next measurement work must address CPU timing resolution and stable load
before claiming regressions/parity. Protocol scope still requires MQTT
subscriptions/session/expiry/alias/auth/reconnect/store/TLS/WebSocket/DNS, Redis
HA/tracking/dedicated, remaining RPC security/recovery/transports/deferred and
Cron IANA/scheduler/durability. No primary merge or push occurred. The complete
four-library goal remains active and incomplete.


## Predeclared longer CPU-statistics measurement (2026-11-02)

Baseline `d418f4ce11f0407e76bce52102a23955eb85e523`. Runtime remains the verified
`77036057` implementation. The workload C source, runtime C/headers, selected
module graph and existing 336 four-form validations are unchanged by byte hash.
The prior profiles and `qos2-local-20261102.json` observation are unchanged. A
new `xmqtt_qos2_resolution` profile copies only the eight instrumented variants,
renames them with `_long` and increases operation count from 20000 to 200000.
Every other argument, fact, metric and default/gate is identical, verified
against the saved original configuration before collection.

The predeclared `bench/results/qos2-long-plan-20261102.json` records the hashes,
change, hypothesis and no-retry policy. Longer samples reduce the previously
observed Windows 15625000 ns process-CPU accounting step per exchange by 10x
(781.25 -> 78.125 ns). Five measured repeats and one process warmup remain;
relative MAD 3%, central range 6%, throughput regression 5% and P99 regression
10% are unchanged. Each OS runs exactly one collection, Windows then WSL. Raw
outputs, failures and original short observations are retained. The new method
is a precision experiment, not a product speedup or replacement accepted baseline.

Both collections finish all eight variants: 80 measured samples and 16 process
warmups total, 16 million measured exchanges plus warmups. Every output passes
phase/sequence checksums, final ACK/echo completion, request retirement and XRT
lifecycle block/byte balance. All five measured arrays per variant are checked
against raw stdout and binary hashes. The workload executable and library hashes
are tied to the prior source proof; no runtime rebuild or ABI change was needed.
The 13 performance-tool regression tests pass. WSL Git metadata limitations stay
explicit, with baseline/runtime content hashes verified as in the previous stage.

| Environment | CPU series failing at 20k (of matching 8) | CPU series failing at 200k | All metric series failing at 200k | Collector exit |
| --- | --- | --- | --- | --- |
| windows | 6 | 2 | 12 | 1 |
| wsl | 1 | 1 | 6 | 1 |

Windows phase CPU deltas still have observed GCD 15625000 ns, confirming the
per-exchange quantization step is now 78.125 ns. The matching Windows CPU series have four fewer failures overall: five
formerly failing series now pass, one still fails and one formerly passing
series now fails. Two CPU series and other throughput/latency series still fail. WSL retains one failing CPU series and five
failing P99 series. No global performance acceptance is inferred. Longer duration
also changes id-wrap count, allocation amortization and observer latency-array
size, and host/power/affinity load is unpinned. Differences in throughput/P99 or
cold bytes per exchange are not evidence of product optimization.

All measured arrays, diagnostics, matching short/long CPU quality and source/
environment fingerprints are committed in `bench/results/qos2-long-20261102.json`.
Exact commands, 96 complete warmup/measured stdout records, binary hashes and
frozen-input proof are indexed by
`out/service-extensions/xmqtt-qos2-resolution/validation.json`. No favorable
recollection or threshold relaxation occurred; both quality exits remain 1.

This completes the precision correction being tested, not stable-load or
production qualification. Residual observer/host/scheduling and P99 effects need
separate profiling/isolation; repeatedly increasing counts is not evidence of a
runtime fix. Product implementation proceeds next with MQTT subscribe/unsubscribe
and shared id/ACK/lifetime state. Stable load, complete CPU/copy/synchronization/
syscall accounting, pinned references/broker, native Linux and >=72h native fault/
mixed-load runs remain required. Other MQTT session/expiry/alias/auth/reconnect/
store/transports, Redis HA/tracking/dedicated, remaining RPC security/recovery/
transports/deferred and Cron IANA/scheduler/durability remain. No primary merge
or push occurred. The complete four-library goal remains active and incomplete.


## Bounded MQTT subscription operations (2026-11-02)

Baseline `183ad3f223ea2d1495728818f7555d30c1710893`. The developing TCP client
now implements asynchronous and blocking SUBSCRIBE/UNSUBSCRIBE for MQTT5 and
3.1.1. They return `xmqttsubscriptionresult` Futures with ordered owned ACKs,
requested/success/rejection counts, id, accepted prefix and typed local status.
Any negative per-filter result returns REJECTED without claiming rollback of
successful filters. MQTT311 UNSUBACK acknowledges removals without reason bytes.
ACK kind, shared outgoing id, complete request, count, granted QoS versus the
original requested QoS and Request Problem Information restrictions are checked.
Scalar decisions precede metadata allocation; local LIMIT/MEMORY can preserve
acknowledgement/counts with a null owned Ack. The message clone now rebases ACK
reason views as well as existing publish/property views.

Admission encodes a complete immutable snapshot and shares the existing finite
request/wire/object quotas, monotonic deadline and outgoing id bitmap/hash table
with publications. Physical reservations survive native request tails, without
retaining Client/Engine in finished results. Queued controls wait for CONNACK;
precomputed wildcard/shared/subscription-id requirements are checked before the
first send, independently of publish credit. UNSUBSCRIBE remains possible with
disabled subscription capabilities. Requested subscription QoS can exceed the
peer's maximum publication QoS. Partial-frame ordering, timeout/cancel after
accepted prefix, Close, graceful drain and same-Engine worker restrictions use
the existing protocol owner and retirement duties. No shared runtime/Core API
change, implicit retry, active subscription registry or recovery was added.

The private request uses a tagged result pointer and a union for QoS2 release
state versus subscription count. Existing publication allocations/result ABI
and public config/info layouts are preserved; added request-type/capability
branches are actual runtime costs. Subscribe admission scans filter bytes for
capability requirements; ACK validation skips its immutable encoded filters and
compares options without descriptor allocation. One result box and one owned
ACK still allocate, plus existing request/wire/Promise/Future/cancel costs. No
zero-overhead, optimization, broker parity or production claim follows.

The new unit/fault header covers both versions, copied caller descriptors,
one-byte output/read work, explicit-id reuse, result/ACK ownership beyond all
connection owners, shared publish-id exclusion, typed timeout/cancel/Close,
construction allocation budgets and reservation rollback. A protocol-worker
barrier deterministically covers unsent cancel, zero timeout, Close and queued
control plus graceful abort. The independent socket peer validates byte-exact
requests, v5 options and multi-byte property lengths; all/partial rejections,
wrong/unknown/duplicate ACKs, bad domains/counts/excess grants, reversed and auto
ids, exhausted publish credit, duplex messages, metadata quotas, admission caps,
negotiated capability restrictions, graceful drain and blocking-worker rejection.
It has 107 distinct version/case pairs. The new offline subscriptions example
includes prerequisites, commands, assertions and explicit owner cleanup.

Local validation passes native Windows GCC O2 and Linux WSL x64 GCC O2 with
ASan/UBSan/leak halt for modular/single programs and peers; static/shared
packages and consumers use plain O2. The full current stage runs 2750 independent
wire-case executions, including 856 new subscription executions (107 distinct
version/case pairs in modular/single/static/shared on both systems), plus 28
modular/single unit programs, eight package consumers and 12 offline examples.
The 26 API-reference/extension-layout/header-coverage/single tool tests pass.
Selected declaration/trim generation, feature/amalgamation/API-reference checks
and package symbol/consumer checks pass. Compiler inputs were frozen throughout
each final build. Both final orchestration handles exited 0.

Measured x64 layouts retain config272/info160/ready136/write64/packet168/codec56/
property40. Private Client stays 1152 bytes on Windows, 1360 on WSL; Request
stays 104. The new subscription result is 64 bytes on both. Portable complete
subscription observations, per-form executable hashes and LF-normalized changed
source fingerprints are committed in
`extlibs/xmqtt/tests/results/subscriptions-20261102.json`. Exact commands, raw
reports, binary hashes and frozen-input evidence are indexed in
`out/service-extensions/xmqtt-subscriptions/validation.json`.
WSL is not native Linux qualification. These are local functional/ownership
checks, with no subscription load/performance or final production acceptance.

This completes current-connection subscription packet operations, not the
planned MQTT product. Subscription-specific performance/CPU/copy/synchronization/
syscall/allocation measurements, pinned broker/client comparisons, stable-load
acceptance, native Linux and >=72h native mixed/fault qualification remain.
Earlier failed QoS2 measurement quality gates are unchanged and unaccepted.
MQTT sessions/store/reconnect/expiry/aliases/enhanced auth/DNS/TLS/WebSocket,
Redis HA/tracking/dedicated paths, remaining RPC security/recovery/transports/
deferred work and Cron IANA/scheduler/durability remain. Product manifests stay
developing. No primary merge or push occurred; the complete four-library goal
remains active and incomplete.


## Bounded MQTT5 incoming Topic Alias (2026-11-02)

Baseline `665e191476b3e98731505c8aa5a982610cc7f3a4`. Positive MQTT5 CONNECT
Topic Alias Maximum now opts into bounded incoming aliases. Absent/zero retains
the disabled default. A lazy direct table accepts all advertised ids through
65535; full-topic packets bind/remap, compact packets resolve, and unaliased
publications do not alter mappings. CONNACK's opposite-direction alias limit is
independent. The protocol worker alone accesses the table. Active duplicate
QoS2 PUBLISH still updates its alias before duplicate delivery is suppressed.
The existing receive ACK/credit, duplex, graceful-drain and retirement rules
continue to apply. No shared runtime/Core API or implicit replay was introduced.

Mapping table/entry capacity is charged to MaxPrivateBytes through Destroy.
Shorter/same-length remaps reuse capacity; a longer one temporarily charges both
entries before installing the replacement and refunding the old one. Messages
never borrow an alias entry. Compact messages append a resolved-topic copy to
their existing owned allocation, charged to object and queued-message byte
budgets. Their logical descriptor has the resolved topic; Wire retains the
actual empty-topic packet. Old messages survive remapping and all connection/
Engine destruction. A fresh connection, even with the same ClientId, has no
mapping. Dense-table footprint is explicit (maximum 65535 pointer slots), not
an unbounded cache or a claim of minimal memory for sparse aliases.

The existing private property/codec boundary now preserves alias zero/repetition
context independently of diagnostic allocation. Two appended codec error enum
values select DISCONNECT 0x94 for zero or above-maximum alias and 0x82 for repeated
properties; an unbound compact reference also uses 0x82. Public property statuses
and existing enum values remain unchanged. Diagnostic OOM tests assert the
specific codec context with unchanged packet output. Other malformed packets
retain their prior reason mapping and bounded diagnostic/abort policy.

The new unit header exercises max id, raw versus logical ownership, repeated
same/shorter/larger rebinds, duplicate QoS2 mapping updates, and cold table/topic/
message, compact copy and larger-replacement OOM budgets. Initial WSL fault
injection used a second network arrival under the worker allocation scope; an
isolated diagnostic reproduced TRANSPORT before any alias allocation at budget
1. The fixture now buffers the marker/bind and tested packet in one input batch,
matching the existing receive OOM methodology and isolating alias allocation
from native socket read allocation. It retains strict MEMORY/no-partial-delivery
assertions and verifies zero live budget owners after destruction; the native
failure was not reclassified or counted as an alias failure.

The independent byte peer has 39 distinct cases: bindings and unbound references,
all remap sizes, independent ids, 65535 id/topic boundaries, fragmented/coalesced
input, QoS1/2, duplicate updates, exact zero/duplicate/over-limit reasons (including
a preceding User Property), default/explicit zero capacity, direction separation,
plain-topic independence, retained User/subscription-id metadata, local object/
message/private/expanded-topic quotas, duplex flow, callback Close, graceful
incoming QoS2 drain and fresh-connection isolation. A separate offline alias
example documents commands, expected resolved messages and explicit cleanup.

Final validation passes native Windows GCC O2 and Linux WSL x64 GCC O2 with
ASan/UBSan/leak halt for modular/single programs/peers; static/shared packages
and consumers use plain O2. Both final orchestration handles exited 0 with frozen
C/header inputs. There are 3062 independent wire-case executions: 312 new alias
cases (39 in each of four forms on two systems), 856 subscription executions and
1894 prior functional cases. The stage also runs 28 modular/single unit programs,
eight package consumers and 14 offline examples. The 26 API-reference/extension-
layout/header-coverage/single tool tests pass; declaration/trim/features/
amalgamation/reference and package verification pass.

Public x64 config272/info160/ready136/write64/packet168/codec56/property40 and
subscription result64 remain unchanged. Private Client changes 1152->1160 bytes
on Windows and 1360->1368 on WSL; Request stays104. Alias entries have two
size fields and owned topic bytes. Destroy scans the advertised pointer slots.
Portable complete alias observations, form-specific binary hashes and normalized
changed-source fingerprints are committed in
`extlibs/xmqtt/tests/results/receive-alias-20261102.json`. Exact commands, raw
reports, the isolated initial native-failure diagnostic and immutable source/
artifact evidence are indexed in
`out/service-extensions/xmqtt-alias-receive/validation.json`.
These are local functional/ownership results; WSL is not native Linux and no
alias/subscription load or production acceptance is claimed.

Costs remain explicit: one private Client pointer, cold direct table/entry
allocations, binding copies, compact lookup and an appended topic copy in an
existing message allocation. Default alias scans existed previously as rejection
checks; optional clone/context branches add instructions. No mutex is added to
lookup, but allocation accounting uses the existing Client Gate. Compact wire
savings are not a claim of eliminating retained-topic storage/copies. No
subscription/alias benchmark, reference/broker parity or production acceptance
is inferred. Earlier QoS2 performance-quality failures stay unaccepted.

Outgoing aliases, session/store/reconnect/expiry/enhanced auth/DNS/TLS/WebSocket,
Redis HA/tracking/dedicated paths, remaining RPC security/recovery/transports/
deferred work, Cron IANA/scheduler/durability, native Linux and >=72h native mixed/
fault qualification remain required. All product manifests stay developing.
No primary merge or push occurred. The complete four-library goal remains active
and incomplete.


## Bounded MQTT5 outgoing alias snapshots (2026-11-02)

Baseline `165e202e57175ddff1f5d17ab76371b318277f71`. MQTT5 publishing now supports
full-topic alias bindings and explicit empty references. Successful admission
fixes each operation's logical topic, with a separately referenced latest-admitted
intent. The worker compares it with actual TCP packet order before first output,
compacting only a matching mapping. If an earlier unsent bind was cancelled,
timed out, rejected locally or blocked by Receive Maximum, a later reference
can safely carry its captured full topic. Delayed older publications also
recheck the actual mapping; they cannot retarget newer snapshots or overwrite
latest-admitted intent. No earlier publication is replayed. Peer alias limits
and actual selected local/peer packet caps apply before sending. Unknown empty
references and invalid aliases fail admission without changing the Future or
cache intent. Failed/overloaded/closed admission cannot install a new intent.

The existing 104-byte Request is unchanged. Aliased operations add one aligned
48-byte tail plus 0..7 bytes of padding in their existing allocation, charged
to private bytes through physical retirement. A lazy x64 2048-byte root and
4096-byte pages provide direct slots up to id65535; immutable topics have a
16-byte header plus their bytes. Planned, actual and request references use the
existing Gate. Common same-topic/empty references reuse cached storage. Ordinary
requests keep their original raw send loop without the gather stack/iteration.
Actual changed mappings are installed after the complete PUBLISH is accepted
into TCP; this is ordered stream state, not broker receipt or durable delivery.

Raw matching full binds and explicit compact references use one original buffer.
Compacting a full input gathers an inline prefix plus the copied original tail;
repairing an empty reference gathers prefix, captured topic and tail through
existing atomic XRT SendRefs. No Core API/shared runtime or second payload copy
is added. Each nonempty native span owns a real Request reference; NativeRefs,
physical id reservation and logical-topic lifetime include all release tails.
Partial frame ordering and QoS1/2 ACK decisions use actual selected wire length.
AcceptedBytes can therefore exceed or fall below retained descriptor encoding;
OutgoingBytes still counts the bounded retained snapshot, while optional metadata
and caches use MaxPrivateBytes and native bytes use Stream.WriteLimit.

Admission rollback now also handles late private-cache overload after constructing
the Promise/Future/result, and rechecks metadata room after cold identifier-table
allocation. It refunds every physical reservation and producer reference while
retaining only documented cold cache storage. OOM matrix and independent normal
private-cap overload cases cover rollback, unchanged output and destruction.
Queued cancel/zero timeout tests gate the protocol owner deterministically, and
check later snapshot topics and compact/full choices with one-byte write work.

The independent byte peer has 30 cases: normal and explicit compaction, mutable
intent snapshots, remaps, cancelled/timed-out binds, blocked QoS1 reorder plus
post-flow latest intent, actual peer rejection/local restriction, QoS1/2, ids and
topics through65535, Remaining Length width transitions, local/peer compact frame
caps, partial cancel/deadline/EOF, invalid/unknown aliases, private-cap overload,
directional duplex and graceful drain. Every observed publication's byte length
is compared independently with its Future's AcceptedBytes. Maximum topic data
is saved as size/hash. The new offline send example checks 23/14-byte QoS1 results
and cleanup after Client/Engine destruction.

Final validation passes native Windows GCC O2 and Linux WSL x64 GCC O2
ASan/UBSan/leak halt for modular/single programs/peers; static/shared packages
and consumers use plain O2. Both final orchestration handles exit0 and all source
inputs stay frozen while compilers run. The stage executes 3302 independent wire
cases, including 240 new outgoing-alias executions (30 distinct cases in four
forms on two systems), 312 incoming-alias, 856 subscription and 1894 prior cases.
There are 28 modular/single unit programs, eight package consumers and 16 offline
examples. The 26 API-reference/extension-layout/header-coverage/single tool tests,
selected declaration/trim, generated features/amalgamation/reference and package
checks pass. The normal private-overload peer case also reaches complete physical
retirement with unchanged admission output.

Public x64 config272/info160/ready136/write64/packet168/codec56/property40 and
subscription result64 remain fixed. Request stays104; optional sender metadata
is48 and topic header16. Private Client changes1160->1168 on Windows and
1368->1376 on WSL. Portable complete sender observations, per-operation actual
wire lengths, form-specific binary hashes and normalized changed-source hashes
are committed in `extlibs/xmqtt/tests/results/send-alias-20261102.json`. Exact
commands, raw reports and immutable input/artifact evidence are indexed by
`out/service-extensions/xmqtt-alias-send/validation.json`. WSL is not native Linux
qualification; these are local functional/ownership results.

Costs include optional metadata/cache allocations, snapshot and topic copies,
same-topic admission comparison, pointer/byte mapping comparison, short prefix
encoding and extra native nodes/ref operations/release callbacks in two/three-
span gather cases. Default admission and selected-wire tagging add instructions.
No throughput, tail latency, zero-overhead or broker-reference parity claim is
made; alias/subscription load/CPU/copy/synchronization/syscall/allocation profiles
still need measurements. Earlier QoS2 failed quality gates remain unaccepted.

Task-owned completed artifact files were moved from C to D with verified scoped
paths and directory junctions retaining the original paths/content. File symbolic
links were unavailable without OS privilege, so that attempted file move was
rolled back before the directory-junction approach. New stage outputs live on D.
This addresses actual disk exhaustion without changing sources or deleting other
work. Artifact hashes remain content-based.

Sessions/store/reconnect/expiry/enhanced auth/DNS/TLS/WebSocket, Redis HA/tracking/
dedicated, remaining RPC security/recovery/transports/deferred, Cron IANA/
scheduler/durability, native Linux and >=72h native mixed/fault qualification
remain. Product manifests stay developing. No primary merge or push occurred;
the complete four-library goal remains active and incomplete.

## Explicit Cron timezone rule core (2026-11-02)

Added a developing `xcron_timezone` leaf, selected by the developing root, using
only public XRT time/memory/error/atomic APIs. It independently compiles explicit
POSIX TZ strings and RFC9636 TZif3 extended footer rules. There is no embedded
reference implementation, installed database, host TZ lookup or libc global
timezone dependency. Binary TZif loading, historical IANA transitions and zoned
calendar search remain pending; this is their required recurrence-rule engine.

The retained immutable owner makes one allocation for metadata, copied names
and (for DST) 800 cached int32 transition offsets over the 400 Gregorian years
2000..2399. Names are bounded at255 bytes and input at4096. It supports Jn/n/M
rules, default/explicit wall times, signed extended times, southern/negative/
zero-delta DST and perpetual/empty seasons. Missing DST rules return a typed
error instead of consulting ambient defaults. Queries expose exact state and
next effective change, full signed-int64 XRT civil decomposition, and all sorted
UTC candidates for a label. Gap=zero, fold=two; capacity discovery leaves the
candidate buffer intact. ERROR preserves candidate/Count outputs. Normal
queries/NONE/CAPACITY preserve ambient errors and allocate nothing.

XRT timestamps retain their 0001-01-01 epoch. The Unix oracle bridge explicitly
uses `XRT_TIME_UNIX_EPOCH`; exact epoch fixtures and core fixed-offset agreement
prevent a self-consistent roundtrip from masking a scale/calendar mistake.
Offset conversion avoids whole-timestamp overflow at either int64 endpoint.
Next search checks coalesced actual state changes and a complete403-rule-year
bound; a leap-only fixture crosses2096->2104 over the non-leap2100 century.

Native Windows GCC O2 and Linux WSL GCC O2 ASan/UBSan/leak-halt pass all three
modular and three selected single-header programs (12 programs total), six root
offline examples, root/isolated static and shared consumers, and four additional
post-size consumers (12 consumer executions total). Size measurement rearchives
the isolated packages, so final artifacts were independently linked/executed
again; original operation receipts and final hashes are both retained. Selected
declaration-only compilation, trim, generated features/amalgamation/reference
checks and26 API-reference/extension-layout/header-coverage/single tool tests pass.

Per timezone unit program, tests include15000 full-range roundtrips (BCE, both
endpoints and offsets beyond24h),3000 raw parser inputs, strict grammar, capacity/
gap/fold behavior, mutable-input ownership, output aliases, factory/diagnostic
OOM with zero live quota bytes,10000 allocation/error-preserving warm cycles,
and four concurrent retained workers each doing12000 query/ref cycles after
the creator releases its owner. These are concurrency functional tests, not
TSan or long-duration native qualification.

The independent public probe compares state/civil/resolve/next against Python
zoneinfo reading synthetic TZif3 (14 rules,18396 cases on each system), plus
direct Linux libc (13 rules,14724 cases), totaling51516 comparisons. CPython3.12
zoneinfo's observed one-based unprefixed-n behavior is excluded from that oracle
only; libc and fixed C cases still verify the exact zero-based rule. Direct libc
is tested from1971 because its rule era is not proleptic; it also differs for
two RFC perpetual-footer fixtures, covered by zoneinfo and fixed C expectations.
No implementation was changed to match either oracle's unsupported behavior.

The dedicated unpinned lookup smoke and isolated single/static/shared size
profiles run on both systems and exclude ambient local-time/calendar symbols.
Their results are workload/harness evidence only: cold creation, other query
paths, contention, latency, pinned repeatability and end-to-end scheduler costs
still require profiling. No performance, native Linux or production acceptance
is claimed. Portable observations/source/binary hashes are committed in
`extlibs/xcron/tests/results/timezone-rules-20261102.json`; exact commands, raw
logs and the frozen252-input/126-artifact/45-receipt proof are indexed by
`out/service-extensions/xcron-zone-rules/validation.json`. All C/H inputs stayed
frozen during compilers. Stage/build artifacts use scoped D-drive junctions to
preserve disk space without deleting concurrent work.

All four product roots remain developing. MQTT sessions/store/reconnect/expiry/
auth/transports, Redis HA/tracking/dedicated modes, remaining RPC security/
recovery/transports/deferred work, Cron TZif/history/zoned scheduling/durability,
native Linux and>=72h native mixed/fault qualification still remain. No primary
merge or push occurred; the complete four-library goal remains active.

## Owned Cron TZif histories (2026-11-02)

Added independent `xcronZoneCreateTzif` for RFC9636 application/tzif v1..4 bytes.
It performs no file/name lookup, host TZ access or upstream implementation
embedding. Newer files skip the legacy block except for locating the authoritative
64-bit block. Main counts/bounds, strictly ordered transition seconds, references,
flags, offsets, name termination and footer consistency are validated. Native
history accepts all legal int32 offsets and full int64 transition seconds,
including values outside the XRT millisecond timeline. Out-of-timeline footer
consistency uses a corresponding Gregorian cycle. Empty/suffix-shared names work;
nonportable bytes produce an offset-derived numeric designation.

Creation prefilters redundant state changes, builds native binary-search tables,
descending unique offsets and unknown intervals, and owns copied names. It uses
one history allocation plus an optional retained POSIX footer allocation. Native
history lookup avoids civil decomposition; next-change lookup has no scan over
redundant raw records. Resolution checks at most258 offsets and supports arbitrary
multi-fold histories (a three-candidate independent fixture is included), with
sorted complete candidates and unchanged buffers on capacity/error outcomes.
Both parent and separately owned footer/unknown name aliases are protected.

RFC coverage is explicit: type0 before the first record, footer on/after the last,
unknown thereafter if it is absent, and type0 everywhere when there are no records
or footer. The -00 designation also means unknown. At/Split fail atomically with
typed UNKNOWN; NextTransition exposes coverage boundaries with the reserved
impossible offset sentinel. Resolution rejects incomplete candidates when an
unknown interval could map to the label under any legal int32 offset, instead of
publishing a guessed gap. This intentionally includes some known local labels
near truncated intervals. Leap-aware data receives structural/table checks and
typed SCALE rejection; leap conversion/truncation/expiration handling remains
pending. This stage does not claim application/tzif-leap completeness.

The direct selected public header now maps its required XRT features, verified
without including another extension header first. Public x64 state24/transition56/
issue24 stay unchanged on Windows and WSL. Private owner32->40 adds one pointer;
direct POSIX queries add a history-pointer branch. Further costs include cold
parsing/decode/name copies and table allocation, unique-offset work and binary
searches, and footer cycle reduction/public-call checks. The dedicated unpinned
4096-effective-change lookup smoke and updated isolated size reports run on both
systems. Other query paths, cold cost/CPU/allocations, contention, latency and
scheduler/reference performance still need profiling. No acceptance claim follows
from these short smoke samples; all measurement quality gates remain unchanged.

Final validation passes native Windows GCC O2 and Linux WSL GCC O2
ASan/UBSan/leak halt for modular/single/probes/examples, with plain O2 packages.
There are16 modular/single programs, eight root offline examples, and12 package
consumer executions. Four final consumers run the complete TZif adversarial/OOM
test against freshly linked isolated static/shared libraries after size tooling
rearchives them. Per new TZif unit program, tests include every fixture truncation
prefix,7000 mutated/raw inputs, extreme offsets/seconds, name-only transitions,
suffix/empty/numeric names, unknown and triple-fold results, malformed fields,
source retirement, partial factory OOM cleanup,10000 zero-allocation/error-identity
query cycles and four retained workers with12000 cycles each after creator release.
Selected declarations/direct-header compilation, trim, generated files and26
API-reference/extension-layout/header-coverage/single tool tests pass. Initial
example/benchmark missing memory declarations were fixed before the final run;
the known WSL orchestration/compiler tree was stopped and reaped before edits.

An immutable compiled-data snapshot labelled IANA2026e from WSL zone.tab plus
four fixed/unknown zones contains422 files. Its version and every file hash are
pinned before testing; no source implementation or data is embedded in the
extension. Python zoneinfo independently decodes the exact same bytes, with raw
RFC coverage checked separately from its last-type extrapolation. All stored
transitions in1800..2500 have exact before/at/after probes plus historical/future
queries. The two systems execute266792 IANA queries:259130 complete state/civil/
resolve/next comparisons,582 unknown mappings and7080 explicitly incomplete
resolutions. Existing explicit-rule oracles add51516 complete comparisons, for
318308 total outcomes. Unsupported behavior is never counted as a successful
complete resolution. WSL remains a local functional environment, not native Linux
or>=72h mixed/fault qualification.

Portable corpus/per-file counts, binary/source hashes, layouts and smoke/size
observations are committed in `extlibs/xcron/tests/results/tzif-history-20261102.json`.
The exact47 receipts,682 frozen source/data inputs and150 artifacts are indexed
by `out/service-extensions/xcron-tzif/validation.json`. Sixty-two final normal
build outputs are archived into the stage to survive future output-path reuse.
All C/H inputs stayed frozen during final compilers. Task-owned completed build
directories were relocated with scoped path checks/junctions to preserve disk
space; no concurrent work was deleted.

Database/name lookup and reload, leap-aware conversion, zoned calendar ordering,
live scheduling/misfires/overlap/executors/durable recovery remain. The earlier
MQTT/Redis/RPC and native/performance/long-duration gaps remain unchanged. All four
roots stay developing. No primary merge or push occurred; the full goal remains
active and incomplete.

## Ordered actual Cron occurrences in zones (2026-11-02)

Added the developing `xcron_zoned` leaf with `xcronZonedMatch` and `xcronZonedNext`.
It depends on calendar/timezone and performs exact actual-instant calculation:
missing labels have no occurrence, all repeated instants are eligible, and the
earliest result is in the explicit inclusive/exclusive UTC window. Nominal gap
compensation, fold selection, misfires, waiting/execution and durable delivery
remain scheduler work. No incomplete policy API or new shared runtime was added.
The original UTC calendar leaf keeps its dependency set.

Search walks ordered effective UTC transition segments, projects each with its
fixed offset, searches calendar masks and maps a result back with overflow-safe
arithmetic. It does not assume local labels increase across rollback, or assume
only two fold instances. A match at the rounded start returns before calculating
the next transition/end date. Whole-second granularity applies locally too:
Unix5 with offset+17 has occurrences at UTC second43. Fractions/negative rounding,
second-level offsets and the full int64 XRT domain are supported.

Only a proven periodic suffix permits a complete400-year search bound. POSIX/
fixed zones repeat throughout; a TZif footer starts periodicity after its raw
historical boundary, even when that boundary is a redundant state change. The
history is searched first. Tests prove an always-missing annual label has no
actual occurrence over a full cycle, and a500-year history of annual gaps finds
the first occurrence in2500 after a fixed footer. An arbitrary Begin+400 cutoff
would miss that result. A complete constant-offset civil cycle can independently
prove the calendar predicate impossible. Unknown coverage before proving a
result is a typed ZONE/UNKNOWN error; it is not silently skipped. A bounded known
window does not require globally resolving its labels across unknown intervals.

All queries retain/mutate nothing and allocate nothing on normal FOUND/NONE or
Match, including complete-cycle NONE under a one-byte quota. Output/Issue aliases
with expressions and parent/footer/name allocations are rejected atomically.
Typed value issues survive diagnostic OOM. Product-private pure expression/span
validation, calendar search and zone value operations are shared between the
leaves; no private XRT implementation is used. Nested duplicate invalid-zone
diagnostics are removed while preserving public codes/messages/ownership. The
new issue is24 bytes on both x64 systems; existing expression56/calendar-issue12/
state24/transition56/zone-issue24/private-owner40 remain unchanged.

Final Windows GCC O2 and WSL GCC O2 ASan/UBSan/leak-halt validation passes20
modular/single programs, ten root offline examples and12 package consumer runs.
Four final consumers execute the complete zoned adversarial/OOM unit program
against freshly linked isolated static/shared packages after size rearchives.
The new unit covers NY later folds, half-hour/negative DST, triple folds, gaps,
periodic/historical completeness,160 independent second-stepping searches,1000
full-range UTC/calendar specializations, endpoints, second offsets, unknown
windows, aliases/flags/value errors, OOM publication and10000 warm cycles plus
complete-cycle NONE with allocation/error checks. Four retained concurrent owners
each run12000 Match/Next cycles after the creator releases its zone reference.
Selected direct header/declarations/layout, trim, generated files and26 tool tests
also pass. These are functional concurrency checks, not TSan/native soak evidence.

The independent oracle enumerates local labels from nine manually specified
profiles, maps zoneinfo folds, validates roundtrips and sorts UTC candidates. It
does not use the library matcher or its segment algorithm. It checks the same
frozen422-zone compiled-data snapshot labelled IANA2026e, including DOM/DOW OR,
star-step AND, weekday hours, sparse seconds, leap/impossible dates and known/
unknown coverage. Both systems together execute437400 Match and126360 Next
queries:57266 searches find a result,68400 are NONE and1360 total Match/Next
outcomes report the expected zone error. There are563760 new zoned outcomes;
the prior rule/history oracles add318308, yielding882068 independent outcomes.
Unknown/incomplete results remain separately classified in portable reports.

Dedicated dense/fold/full-period smoke and isolated-size profiles run with the
original quality/regression gates. They exercise precompiled calculation only;
one-sample unpinned smoke has no performance/P99 acceptance significance. Cold
costs, broader CPU/copy/allocation/synchronization profiles, adversarial latency,
native Linux and>=72h mixed/fault qualification remain pending. Portable complete
counts/data hashes, form-specific binaries, layouts/source hashes and smoke/size
observations are in `extlibs/xcron/tests/results/zoned-occurrences-20261102.json`.
Exact51 receipts,692 frozen source/data inputs and158 artifacts are indexed by
`out/service-extensions/xcron-zoned/validation.json`. Sixty-eight final default
outputs are archived under the stage. All final C/H inputs stayed frozen during
compilers; the typed output/assertions and all final hashes were checked before
commit.

Database lookup/reload, leap-aware conversion, configurable nominal gap/fold
delivery, live scheduling/clock jumps/misfire/overlap/executors/durable recovery
remain. Earlier MQTT/Redis/RPC and native/performance/long-duration gaps remain.
All four roots stay developing. No primary merge or push occurred; the complete
four-library goal remains active and incomplete.

## Bounded explicit-clock Cron scheduling kernel (2026-11-02)

Added the developing `xcron_scheduler` leaf with eleven public operations for
creation, admission, pull dispatch, completion, cancellation, job inspection,
retirement receipts, close, wake projection, statistics and destruction. One
preallocation bounds job/run slots, the due heap and intrusive queues within an
explicit byte cap. Expressions are copied; zones are success-only retained; Data
is borrowed through the retirement receipt. IDs use only the necessary low index
bits and all remaining generation bits, with nonwrapping terminal retirement.
A one-slot table uses all64 generation bits;128 slots retain57 bits. IDs are
scoped to one live scheduler, not global capabilities.

The owner supplies paired wall/monotonic millisecond samples. Monotonic rollback
fails atomically; wall rollback delays future work without rewinding consumed
cursors. Forward gaps use explicit catch-up/skip/coalesced misfires with a
saturating grace cutoff. Wait/allow/skip overlap and global/per-job running caps
include published leases and executor queue time. Alternating due promotion and
FIFO ready dispatch prevent an old catch-up job monopolizing the heap. Each Poll
has explicit work/output budgets. WorkLimit bounds actions, not CPU time inside
one recurrence search, which can still traverse full history/a Gregorian cycle.
The live adapter therefore still needs bounded/precomputed calculation work.

Completion is exactly once and acknowledges the physical body and data-using
tails. Success/failure/cancellation outcomes are counted without implicitly
stopping future triggers. Cancel/Close stop future dispatch while retaining live
leases. Only a terminal job with zero leases can return its Data receipt and
release its zone. Busy destruction preserves the owner. There are no callbacks,
threads, timers, executors or storage inside the kernel. Public XRT APIs suffice;
no Core/runtime change was introduced. A product-private pure zoned value query
records post-admission typed job faults without allocating/replacing ambient
errors. Public zoned validation, results and typed failure semantics remain.

Final native Windows GCC O2 and WSL GCC O2 ASan/UBSan/leak-halt validation passes24
modular/single unit programs,12 offline root examples and12 static/shared package
consumer executions. Four post-size leaf consumers run the complete scheduler
adversarial/OOM unit against the actual rebuilt libraries. Each scheduler unit
checks92160 independent finite-occurrence-list samples across576 policy traces,
including all three execution outcomes; eight executions yield737280 samples.
Additional cases cover arbitrary heap deletion/reuse and linear-minimum order,
work=1 fairness, caps/grace/clock endpoints, actual folds, impossible calendars,
initial/post-publication unknown TZif coverage, alias/overflow/value errors,
close with an active scheduled lease, repeated generations and10000 zero-heap
lifecycle cycles under a one-byte quota with allocation/error-identity checks.

Both systems rerun563760 independent zoned Match/Next outcomes over the frozen
422-zone IANA2026e-labelled binary-data snapshot after the pure-query refactor.
Direct modular header, selected declarations, fixed public layout, trim,
generated files and26 Python tool tests pass. The new issue32/config24/job-config112/
job-info96/run64/poll24/wake24/receipt112/stats120 layouts agree on both x64 ABIs.
New1/128-job pull dispatch/completion batch-P99 smoke, changed public zoned
calculation smoke and isolated kernel size profiles pass with the original
quality/regression gates. Single-sample unpinned smoke is workload evidence;
there is no throughput, P99, native Linux or production acceptance claim.

Portable complete source/data fingerprints, layouts, binary hashes and smoke/size
observations are committed in `extlibs/xcron/tests/results/scheduler-kernel-20261102.json`.
The final proof `out/service-extensions/xcron-scheduler/validation.json` indexes709
frozen source/data inputs,160 artifacts and53 receipts;74 final default outputs
are archived under the stage to survive path reuse. All final C/H inputs stayed
frozen while compilers ran. The documented offline example demonstrates range
coalescing and cancellation followed by physical retirement with full cleanup.

Named database/reload, leap-aware conversion, configurable nominal DST delivery,
NetEngine/Executor integration and rejection/cancellation hand-off, durable
recovery, adversarial CPU/P99, native Linux and>=72h qualification remain. Earlier
MQTT/Redis/RPC gaps remain. All four roots and the scheduler leaf stay developing;
the complete goal remains active. The primary checkout remains separate: its77
pre-existing-file checkpoint is `93347eb5`; no primary merge or push occurred.

## Actual XRT executor adapter for Cron leases (2026-11-02)

Added the developing `xcron_executor` leaf with nine public operations/types for
actual worker execution, cancellation, per-job cancellation, owned completion
receipts, close/inspection/destruction and a scoped cancellation query. It borrows
an existing public XRT Executor and creates no runtime, worker, timer or executor.
The creating native thread owns control/drain operations. A preallocated slot
array, <=50%-loaded lease-ID hash with backward-shift deletion and MPSC completion
publication bound all local storage within BytesLimit. Active IDs are scoped to
one originating scheduler. Successful admission transfers only the completion
return duty; rejection invokes no body and leaves the lease with its caller.

Callbacks borrow the copied64-byte Run/token and must finish data-using tails.
FAILED transfers that worker's ambient error into an owned receipt; invalid return
enums normalize to FAILED/RESULT without stealing another task's error. Queued
cancellation skips the body; running cancellation is cooperative and never
rewrites an already produced outcome. Close does not close a borrowed shared
executor. Drain copies an80-byte receipt and returns adapter capacity; the owner
then completes the kernel lease and eventually reaps Data. This is not an
exactly-once business-effect guarantee or a global lease capability.

The completion producer never accesses its slot/Data/Error after publication,
so a drained slot can be reused before its previous notification ends. A separate
producer reference protects adapter/notification context through that tail;
Destroy remains BUSY until receipts and producers are zero. Owner-notification
reentry and cross-thread control calls fail explicitly. Selecting a user body on
its adapter's owner thread returns typed OWNER_CONFLICT instead of entering owner
state. Optional Notify can signal a host event/post a wake; bounded resampling
must recover from failed wake delivery. No blocking adapter wait or new shared
runtime/Core API is introduced. Native thread-ID checks, hashing, copies, CAS
contention and public Executor bookkeeping remain explicit measured costs.

Final native Windows GCC O2 and WSL GCC O2 ASan/UBSan/leak-halt pass28 modular/
single programs,14 offline examples,12 static/shared package consumers and26
Python tool tests. Root static/shared consumers now execute real worker callbacks
for both actual-fold leases, drain them on the owner and return them to the
scheduler before retirement. Four post-size leaf consumers run the complete
executor adversarial/OOM unit against actual rebuilt libraries. Direct header,
selected declarations, trim, generated files, public layout and isolated size
pass; new config56/issue8/completion80/stats64 layouts agree on both x64 ABIs.

The complete WSL GCC O1 TSan-instrumented executor unit also runs without a
reported race. GCC's existing atomic_thread_fence instrumentation warning first
fails strict -Werror compilation, then is retained with -Wno-error=tsan only for
this isolated diagnostic build. The original failed compile log/receipt and the
successful build/run are both preserved. This does not prove fence-only
synchronization, the whole XRT runtime or native Linux qualification.

Nine complete executor unit executions (four modular/single, four post-size
consumers and one TSan) cover290304 randomized many-worker receipts and90000
warmed zero-heap cycles. Tests include real shared-queue rejection, original
AGAIN cause preservation despite diagnostic OOM, native queued cancellation,
owned worker errors/invalid results, owner-conflict and Notify reentry, close,
explicitly blocked notification tails, one-slot reuse before an old tail exits,
real scheduler/Data retirement, hashed-ID churn, caps/aliases/overflow/OOM and
ambient-error identity. Four transitive scheduler units additionally repeat
368640 independent finite-list policy samples. These remain finite functional/
sanitizer checks, not mixed-load production qualification.

New1/4-worker64-lease batch throughput/P99 smoke and isolated size profiles pass
with the original quality/regression gates. A separately labelled minimal-XRT
submit/counter reference omits lease copies, cancellation, hash/receipt metadata
and error transfer; it is not a feature-equivalent competitor. Single-sample
unpinned smoke has no accepted throughput/P99 significance. CPU, fixed-arrival
queue wait, slow bodies, contention, adversarial tails and cold costs remain to
qualify with the actual live scheduling path.

Portable source/layout/binary hashes, sanitizer scope and smoke/size observations
are in `extlibs/xcron/tests/results/executor-adapter-20261102.json`. The final proof
`out/service-extensions/xcron-executor/validation.json` indexes299 frozen inputs,
210 artifacts and74 receipts (including the expected initial TSan compiler-warning
failure);80 default outputs are archived under the stage. All final C/H inputs
stayed frozen while compilers ran. The dedicated documented example exercises
real execution, owner completion, retirement and complete pool/adapter cleanup.

Live timer/engine driving, bounded/precomputed recurrence calculation, named zone
database/reload, leap-aware conversion, nominal DST delivery and durable recovery
remain. Earlier MQTT/Redis/RPC gaps and pinned/native Linux/>=72h acceptance remain.
All roots and the new leaf stay developing; the complete goal remains active.
There is no primary checkout merge or push.

## Proven minimum Core gap for live Cron waits (2026-11-02)

While designing the actual live driver, reproduced a required public-API gap:
a single borrowed Executor worker cannot block waiting for its queued child.
The same condition-variable prototype with two workers completes normally.
Existing public ExecutorWaitFor reports the same STATE for an external caller
of an open executor and a self-worker call, so it cannot be used as an identity
query. Before-change Core libraries, source, binaries and one/two-worker outputs
are archived under `out/service-extensions/xcron-current/baseline` for both systems.

Added exactly the public `xrtExecutorIsCurrent(const xexecutor*)` observation,
delegating to the existing native worker/cancellation callback predicate. No
runtime, per-job state, ownership model or public structure field changes.
It matches worker bodies, drops, worker-exit callbacks and queued cancellation
cleanup on any caller, including nested cross-executor cancellations. An unrelated
executor/NULL is false; successful queries preserve ambient error. Normal Core
context setup is separate from allocation-free repeated queries. Existing TLS/
native-thread fallback and synchronized callback registry remain; this is not a
wait-free ownership-freeze query and does not retain or authorize destruction.
Post-join exit-context Drop is outside the documented worker/queued-cleanup scope.

The new registered public example rejects a single-worker dependent wait before
blocking, then lets the queued child run and closes/drains/destroys its pool. A
registered selected-single/current-context diagnostic suite tests positive/negative
identity, body/drop/exit/post-join scope, native external cancellation, nested
A-worker/B-cleanup/C-cleanup scopes and exact restoration after each return.
All foreign worker contexts are warmed before global allocation snapshots.
The existing abbreviated documentation reference to OwnershipAdapterV2 is expanded
to its full symbol; the complete Executor symbol-inventory check now finds19
functions,2 constants and8 types. This is an inventory check, not production proof.

Final native Windows GCC O2 and WSL GCC O2 ASan/UBSan/leak-halt pass16 modular/
single programs across the current-context, ordinary Executor and ownership suites,
two forced native-thread-fallback programs, six actual examples and eight package
consumer runs. Four static/shared consumers execute the complete new unit against
real rebuilt Core libraries. Direct feature/header and selected declaration probes,
trim, generated features/amalgamations and20 focused API/declaration tool tests
pass. Config24/item32/stats88/data-policy32/exit-policy32 layouts agree on both x64
ABIs. The fallback wrapper selects the existing no-compiler-TLS branch only in
executor.c; it does not certify an entire TinyCC/non-TLS toolchain.

Separate complete WSL GCC O1 TSan TLS and forced-fallback units both pass without
a reported race. GCC's existing atomic_thread_fence instrumentation warning is
retained; only these diagnostic builds use -Wno-error=tsan. Fence-only/whole-Core
synchronization and native Linux qualification are not implied. Sixteen complete
new-unit executions in all forms repeat1280000 warmed groups (5120000 predicate
calls) under one-byte quotas with error identity and allocation checks. Owned exit,
external and nested cancellation lifetimes remain real native tests, not mock IDs.

The broader25-test release-tool invocation still reports five expected failing
subtests: four service products remain developing, and Core still lacks selected
single closure for net_file_future. A separate current-state maturity observation
records exactly these gaps; no gate is reduced or declared green. Portable source,
ABI, binary and before-change fingerprints and explicit maturity/sanitizer scope
are in `docs/development/results/executor-current-context-20261102.json`.
The exact proof `out/service-extensions/xcron-current/validation.json` indexes253
frozen inputs,150 artifacts and66 receipts;31 final default outputs are archived.
All final C/H inputs stayed frozen during compilers. No pinned throughput/P99,
native Linux, production or>=72h acceptance is claimed.

The live driver's bounded-command/completion-reservation, safe post-stop reaping,
retry/deadline, wake and physical shutdown contract is preserved in
`docs/development/xcron-live-driver-design.md`. It is explicitly an implementation
contract, not a delivered service or manifest stub. Actual live driving, bounded
recurrence CPU work, persistence, nominal DST/named database and all earlier
MQTT/Redis/RPC gaps remain. The full four-library goal stays active; no primary
checkout merge or push occurred.

## 2026-11-02 — bounded live xcron service

Implemented the actual `xcron_service` developing leaf, selected by the xcron
root. Fourteen public APIs cover creation, bounded Add/Cancel/Info/Reap commands,
finite collection, result metadata drain, close/join, post-stop retirement,
inspection/fatal diagnostic/wake and caller-exclusive destruction. A dedicated
public-XRT thread owns the existing kernel and borrowed-executor adapter; no new
shared runtime, private Core API or borrowed-pool ownership was introduced.

The preallocated arena holds command/reply generation tickets, completion
reservations, active run mapping, retry heap and batches. Every kernel lease
reserves its final output first. Full output stops dispatch, while all final
physical completions and close still have space. Completion records carry no
Data pointer: original Data returns only via failed Add replies or terminal,
zero-running retirement receipts. Join can precede collection/drain/reaping;
Destroy never silently discards those duties. Queued unprocessed Adds close with
their original Data, while successful racing admission retains its Job receipt.
Native submission retries only no-effect AGAIN, with original monotonic deadline,
bounded attempts and explicit rejection/expiry/cancellation results. Native owner
and unrelated borrowed-pool callbacks cannot block waiting on the service.

Clock=NULL drives real UTC/monotonic deadlines; custom paired clocks support
deterministic jumps. Clock read failure/regression records typed first failure and
controlled close. Event delivery preserves ambient errors and counts failures;
bounded resampling backs up notifications. Adapter physical/Notify tails end
before owner stop/handoff. Component stats are owner-turn snapshots; the final
snapshot retains adapter layout bytes after release, as documented, and is not a
live heap/RSS counter. Explicit caps reserve arena plus both component byte caps;
optional InitializationBudget covers direct creator and owner component startup.

Native Windows GCC O2 and WSL GCC O2 ASan/UBSan/leak-halt pass 36 modular/single
programs,16 runnable examples and12 root/leaf static/shared consumer runs. Packages
are final plain O2 artifacts and are exercised after size measurement. A separate
complete WSL GCC O1 TSan service unit also passes. The existing GCC unsupported
atomic_thread_fence warning is retained with diagnostic-only -Wno-error=tsan;
this does not qualify fence-only synchronization, whole Core or native Linux.
All26 API-reference/layout/header-coverage/single tools pass. Service x64 ABI sizes
match on both platforms: config136,reply296,completion80,issue8,stats320 bytes.

Thirteen complete service-unit executions include real UTC firing, actual DST
fold instances, forward/backward wall jumps and catch-up/skip/coalesce; physical
success/failure/error transfer/invalid results/cooperative cancellation; bounded
commands and output credits; timed-out requests retaining Data; generation reuse
and racing collectors; held-owner close; no-effect retry success, elapsed and
attempt expiry; clock faults; owner/unrelated-pool self waits; alias/overflow
rejection; observed cold creator-thread/kernel/adapter allocation-prefix failures.
They total6656 concurrent Info commands,104 four-producer Add/Close races and26624
warmed dispatch cycles. Zero-requested-heap assertions run in seven instrumented
units,14336 cycles; uninstrumented package units are not counted as heap proofs.
Root scheduler regressions add368640 independent model samples.

Added an actual fixed-UTC-arrival start-lateness benchmark, not a closed-loop
completion-only throughput proxy. Its one32-arrival smoke tick reports Windows
start P99/max13ms and owner-sample-to-worker P990.587ms; WSL start P99/max0ms at
millisecond resolution and owner-to-worker P990.808ms. Zero means below reporting
resolution. Owner-to-worker includes owner work and is not isolated native-pool
queue delay. Arrival rate is an input, not maximum throughput. These unpinned
single samples qualify neither performance nor P99; no favorable timing retries
were made and all existing gates remain unchanged. Windows deadline/wake tail
latency, fixed environmental baselines and bounded recurrence CPU need further work.

The initial final Windows unit consumer lacked explicit Core selection because
the reused test harness includes Core before extension features. Correcting only
the validation build to supply the actual leaf closure fixed it. Already-passing
measurement/tool steps were reused; production header/example/package consumers
use public module selection directly. Final C/H remained frozen during compilers.

Portable source/layout/artifact fingerprints, measurement details and scope limits
are in `extlibs/xcron/tests/results/live-service-20261102.json`. The exact local
proof `out/service-extensions/xcron-service/validation.json` indexes305 frozen
inputs,229 artifacts,77 receipts and84 archived outputs. Named-zone database/reload,
leap-aware conversion, nominal DST policies, durable recovery, recurrence CPU
bounds, native Linux, pinned throughput/P99 and >=72h acceptance remain open,
alongside the previously recorded MQTT/Redis/RPC gaps. The four-library goal stays
active; this stage is not production completion, and no master merge/push occurred.

## 2026-11-02 — reusable public event deadline waiter prerequisite

Investigated the previous Windows live-service smoke's13ms start tail before
changing generic XRT capabilities. A native child pinned to one permitted CPU
compared ordinary XRT event waits, native event waits, standard waitable timers
and high-resolution timers. Each predetermined mode/request group contains256
waits at1ms or5ms, with no favorable rerun. At1ms, median additional wait was
14.6729ms for XRT events,14.7175ms for native events,14.5962ms for standard timers,
and0.9576ms for precise timers. This isolates a real ordinary Windows timeout
granularity gap; it does not establish application P99 or a CPU/power guarantee.
All observed Windows process-CPU deltas were below accounting resolution, not
proof of zero CPU work. Raw source/binary/baseline-library/settings fingerprints
are retained in the local proof and portable result.

Added the actual Core `event_waiter` leaf with four public APIs:
Init/Unit/WaitFor/Get. Caller-owned opaque context borrows an existing initialized
event. Windows owns one reusable nonperiodic waitable timer and prefers the
high-resolution backend without calling timer-resolution APIs; POSIX reuses the
event's existing condition. No requested heap allocation, thread, callback or
shared runtime is added. This is additive: existing EventWaitFor, event storage,
other synchronization APIs and source behavior are unchanged. Production source
closure has no thread creation, executor or network dependency.

WaitFor is bound to the native Init thread; Get can inspect immutable metadata
from another thread; caller-exclusive Unit can follow a physical handoff/join.
Auto/manual reset and event-ready priority are preserved. One original signed-ms
monotonic budget survives timer/spurious wakes, including huge finite/infinite
waits; outward checked100ns conversion avoids overflow. Event-won waits cancel
their timer; rearming resets old timer signals. Unsupported precise mechanisms
have observable strict rejection or default fallback. Native resource errors
never trigger silent degradation, and cancellation/close failures remain visible.
Metadata states storage bytes/backend/native-timer count. Native clocks/kernel
scheduling/interrupt energy costs are not made latency or power guarantees.

Native Windows GCC O2 and WSL GCC O2 ASan/UBSan/leak-halt pass16 final modular/single
programs,2 runnable examples and12 final static/shared consumer runs. Both selected
modules and the all-module source closure contain the implementation. Existing
event/sync-OOM regressions, direct/declaration headers, trim/size, both public
constant namespaces and all26 API/layout/header-coverage/single tools pass. Public
ABI: waiter40 bytes Windows/32 POSIX, info16 bytes, unchanged event24/176 bytes.
Separate WSL GCC O1 TSan public-thread and actual realtime-condition fallback
programs both pass. This closure emitted no atomic_thread_fence warning; only the
diagnostic build retains the known -Wno-error=tsan allowance. No whole-Core/native
Linux qualification is inferred.

Final13 basic-unit executions cover auto/manual readiness, no-early deadlines,
event interruption, large/infinite budgets, owner guard and physical cleanup
handoff, span/flag rejection and full-byte context/event alias preservation. They
run260000 ready cycles,416 real armed cycles and13000 Init/Unit cycles. Seven
instrumented units assert zero requested-heap calls, including224 armed cycles
and7000 context lifetimes. All6 Windows final units assert that1000 native timer
lifetimes return process handle count to baseline. Native fault coverage includes
unsupported/fallback/resource/diagnostic OOM/arm/wait/cancel/unit/reentry; POSIX
fallback uses a real default-clock condition with strict/default selection.

During final review, an alias assertion was found comparing an unrelated untouched
buffer. It was strengthened to compare every context/event byte; all affected
modular/single/package/TSan programs were rebuilt. The exact initial test source
is archived by its recorded hash, and all other production/measurement inputs
match. Existing measurements were retained. Early benchmark path/module-selection
setup errors were corrected before any timing sample ran. Final C/H stayed frozen
during compilers; no performance gate was relaxed.

One16-sample public primitive smoke per mode/platform reports additional-wait
P99: Windows event14.9949ms/waiter1.0086ms, WSL event0.0761ms/waiter0.0765ms.
Windows CPU deltas are below accounting resolution; WSL process CPU averages are
7.89375/7.70625us per wait. Samples are unpinned/nonqualifying; primitive waiting
is distinct from fixed-arrival service lateness and maximum throughput. New
profile metrics use MAD3%,central-range6%,lower regression10%; older Core defaults
were preserved. Ready polling, clocks, timer arm/wait/cancel and a native handle
are explicit costs, with no zero-overhead or system power claim.

Portable evidence is `docs/development/results/event-waiter-20261102.json`;
`out/service-extensions/xcron-precise/validation.json` indexes250 current frozen
inputs,162 artifacts,76 receipts and27 archived outputs. This independently
committed prerequisite is not yet selected by xcron service: its owner/startup/
shutdown/backend accounting and fixed-arrival service remeasurement remain next.
All original four-product protocol, persistence, zone/recovery, native performance
and >=72h requirements remain open. The full goal stays active; no master merge
or push occurred.

## 2026-11-03 - service extensions integrated into master

User explicitly requested the merge. The two-parent merge joins master
93347eb5 and service branch 6960e778, preserving the 10 and 83 independent
commits respectively. Four manifest/reference/test/generated-header conflicts
were resolved while retaining both independent API-reference regressions. A
three-way audit covers all 596 modules and all independently changed files;
among files changed by only one parent, differences are limited to generated
declarations, the corrective WebSocket change and release-gate test metadata. All 25 extension declaration headers now preserve and
restore the EVENT_WAITER and NET_TCP_RETIRE feature switches.

Integration found a Windows access violation already present on pre-merge
master. GDB identifies the reference-send ownership-policy probe preceding
connection range validation. All affected stream entry wrappers now validate
the range before probing metadata. The shared runtime test adds explicit
wrapping-range regressions for pause/resume/retain, data/control/close/abort
and reference/immutable-byte sends. No public API or ownership contract changes.
The guard is an explicit extra range predicate, with no new allocation, copy or
synchronization primitive; no performance acceptance is inferred.

Frozen C/H sources pass 126 Windows GCC O2 modular/single-header programs
and 72 WSL GCC O2 programs under ASan/UBSan with leak detection; WSL uses
modular baseline suites plus a modular/single-header TLS-auth follow-up. The
selected regressions cover all four products, event waiter, current executor,
thread-pool exit, logger, WebSocket ownership, file futures and TCP. All 104
Python API/layout/header/ABI/scope/release/build/package tool tests pass, as do
Core feature/amalgamation/network-reference and four-product feature/amalgamation/
API-reference consistency checks.
Both baseline runtime phases use identical hashes for 2149 validation inputs. The original
master crash and pre-fix merge run are retained as local diagnostic evidence;
final runtime results were rebuilt against the corrected frozen C/H inputs.

The release-state tools also exposed a prior TLS-auth metadata omission. A
complete developing-module inventory now lists tls_client_auth without promoting
its state. io_uring retains its restricted-environment compile gate; non-io_uring
developing nodes must be covered by a real Linux modular/single runtime gate.
The native workflow adds TLS-auth tests and independent peer checks with an
explicit report path. These final documentation/CI/Python-test changes preserve
every runtime C/H hash; all tool tests were rerun against the revised metadata.
The new TLS gate passes 4 modular/single programs and an independently rebuilt
OpenSSL peer library passes 34 authentication cases and 60 legacy-role/empty-
identity regressions. This local WSL execution does not claim a remote CI run.

Portable record: `docs/development/results/service-extensions-merge-20261103.json`.
Local commands, source hashes and log receipts: `out/service-extensions-merge/`.
All four libraries remain developing. Protocol completeness, persistence and
recovery, native performance, native Linux qualification and >=72h acceptance
remain open; this is an integration checkpoint. No remote push was performed.

## 2026-11-05 - precise waits integrated into the live Cron owner

Previous goal turn made authoritative progress: both histories were merged with
integration regressions and portable records. This turn resumes from a clean
worktree and fast-forwards the four newer master commits to 7cecb354 before edits.
No existing work is overwritten. Only xcron service selects the already proven
public event_waiter primitive; no additional Core runtime or API is introduced.

The actual owner initializes its reusable waiter after the kernel/adapter. Native
init failure now rolls both components back. Work/wake ordering retains the
auto-reset event with no reset after checking pending work. Stop releases the
waiter after physical adapter producers finish, before publishing STOPPED. A
release failure publishes FAILED with a typed SYSTEM cause and retains live
resource accounting. After native join, exclusive Destroy retries cleanup before
kernel destruction; another failure preserves an inspectable object for retry.

Stats adds the immutable selected backend/capacity and live native timer count.
Config remains136 bytes and function signatures are unchanged; stats grows320 to
344 bytes on both x64 targets, requiring consumers to rebuild with matching
headers. Inline context is40 Windows/32 POSIX bytes included in ArenaBytes; one
Windows native timer is separately counted and no process/global resolution is
changed. POSIX reuses the existing condition. Per-wait validation/clock reads and
Windows arm/wait/cancel calls are explicit costs, with no zero-overhead claim.

Both platforms pass19 full modular/single programs plus the service regression
and runnable example. Static/shared consumers pass on each. WSL GCC O2 runs with
ASan/UBSan/leak-halt; two separate O1 TSan modular/native-wrapper units also pass.
Windows66 tool tests, generator/reference/docs checks and direct/declaration
layout probes pass. Failure injection covers real waiter init/wait/unit, charged
startup rollback, stopped cleanup retry/handoff and actual Windows standard-timer
fallback. A warmed64-service loop returns Windows handle count to baseline.
Existing OOM, command races, reserved outputs and2048 zero-heap cycles remain.

Identical fixed UTC arrivals (8 periods x256 jobs, all6144 samples per phase) give
Windows P99 samples15/14/14ms before and2/2/1ms after. WSL gives3/4/1ms before and
3/1/2ms after. All phases retain quality failures at the original MAD3%/central-
range6% thresholds. No noise gate is relaxed. Separate whole-process CPU samples
include lifecycle overhead; Windows accounting zeros are below useful resolution.
These unpinned millisecond measurements do not establish production performance,
maximum throughput, power, native Linux qualification or a maximum latency bound.

Portable evidence: extlibs/xcron/tests/results/precise-service-20261105.json.
Local source maps/commands/logs, unchanged original measurements and archived old
benchmark binaries: out/service-extensions/xcron-precise-service/. The final876
protected inputs match across platforms; unmodified Core fixture blobs are pinned
separately. Old dangling cache junctions are preserved, with new output aliases;
WSL Git metadata uses explicit Linux paths. Next audit stopped component-byte
snapshots against physical adapter destruction, then improve measurement precision
and bounded recurrence work. All four products and the complete goal remain in
development; durable scheduling and protocol/performance/long-run gates remain.

## 2026-11-05 - retained service component-byte snapshots

Previous goal turn was progress: precise waiters were implemented, validated and
committed in7dcc8d36. Both worktrees are clean at that parent before this stage.
An audit confirms that the prior guide intentionally described sampled/provisioned
bytes. The developing semantic contract now reports retained component allocations
consistently, with no layout/signature changes (config136, stats344). This is an
observability contract change; the previous adapter was already physically freed.

The new regression holds the first clock callback. The old implementation reports
5704 bytes (arena only, kernel0/adapter0) despite8688 budget-charged bytes and fails
the new invariant. Complete metadata is now published before Create exposes the
service. On Windows it reports8584 component bytes, then7960 after stopping;
actual charged bytes fall8688 to8064, exactly the624-byte adapter. Remaining
thread/bootstrap charges are explicitly outside component totals. Both
AdapterBytes and Executor.Bytes become0 under the mutex before stop publication;
other executor counters/limit remain historical. Waiter-release failure also
retains correct component totals. Final Destroy returns all charged allocations.

Windows and WSL modular/single service/primitive/memory regressions, examples and
static/shared consumers pass. WSL uses O2 ASan/UBSan/leak-halt and two separate O1
TSan units. The66 tools and feature/single/reference/docs checks pass. No new
allocation, thread or storage is introduced; cost is one constant startup snapshot
and one constant stop update. The ordinary owner snapshot still takes one mutex
critical section. No fresh performance or native Linux qualification is inferred.

Evidence: extlibs/xcron/tests/results/service-resources-20261105.json and
out/service-extensions/xcron-accounting/. Next implement resumable calendar work
as a prerequisite for truly bounded zone/scheduler/command processing. Existing
recurrence CPU, persistence/zone/DST, protocol and performance/long-run requirements
remain active.

## 2026-11-05 - resumable UTC/civil calendar queries

After resource accounting commit d3cb9318, this stage adds four public calendar
cursor APIs: UTC/civil Init and Step. A256-byte,8-byte-aligned stable caller
context snapshots expression/bounds; serialized same-address handoff is supported.
Positive month budgets bound additional visits per call, PENDING preserves Next,
only FOUND publishes a result, and terminal reads are repeatable. Failed init,
argument/state failures and structural aliases preserve the documented outputs.
Typed issues survive diagnostic OOM. Existing declarations/layouts/enum values
remain; PENDING and STATE are appended. Synchronous Next never returns PENDING.

An analytic month/day impossibility proof now avoids all4801 visits for impossible
February30-style conjunctions while preserving restricted-day OR semantics.
MonthsExamined becomes0 for this proof; the guide documents that observability
change. Independent146097-day tables cover5208 conjunction/disjunction existence
cases. All1500 brute earliest-match oracles also exercise varied cursor budgets.
The raw typed engine separately exhausts the4801 fallback cap at budget1. Tests
also cover94 actual yields, unchanged pending results/error identity, copied
inputs, UTC/civil int64 extremes, aliases, relocation/mode errors, handoff and OOM.

The first full Windows native service run exposed a natural executor physical-
join deadlock. Captures show Shutdown=1 with a parked idle worker; that failed
run and the earlier passing WSL run are archived, not reused as final receipts.
Cold lifecycle broadcasts now rendezvous through SleepLock in separate Core
commit8cd1155f. Both platforms pass9 executor/ownership/memory programs, plus two
WSL TSan units and16 repeated Windows native service runs. The1024-round bounded
retirement stress is not a deterministic reproducer: eight old-version runs
passed. See docs/development/executor-stop-wake-20261105.md and its portable JSON.

Concurrent master commits3b6dfc20/88bfa0de are preserved through merge5675a16c;
all uncommitted cursor-file hashes remain identical through that merge. Final
Windows/WSL source maps match on2469 protected inputs after the Core fix. Each
platform passes19 full modular/single programs, runnable calendar example and
static/shared consumers executing all four new symbols. WSL uses O2 ASan/UBSan/
leak-halt and two separate O1 TSan units. Windows66 tools, generated headers,
reference/docs and both direct/declaration ABI probes pass.

Public Step copies private state in/out (at most256 bytes each way), validates
identity/mode/spans and does bounded date/bitmap work. Init also copies expression/
dates and clears/encodes the context. No heap/thread/lock/dependency is added.
Shared synchronous control-flow prototypes regressed common queries by5-13%, so
the final synchronous loop remains specialized, sharing matching/pruning helpers;
control-flow duplication is an explicit maintenance cost. Five alternating Windows
before/after per-case medians give ratios1.0218 dense,0.9639 quarter,0.9583 leap and
221.5 impossible. The latter dominates the mixed workload improvement. All raw
samples, including prototype regressions, are retained. Original before and final
after reports fail unchanged MAD3%/central-range6% quality gates on both systems;
there is no qualified no-regression claim. A separate public API probe retains
equal checksums at budgets1/16/64 and demonstrates extra call/copy cost at budget1.

Portable evidence: extlibs/xcron/tests/results/calendar-cursor-20261105.json.
Local commands/source maps/binaries/logs:
`out/service-extensions/xcron-calendar-cursor/`. The runnable calendar example
and consumer use the real new APIs.
This primitive does not yet bound ordered zone traversal, synchronous admission
or scheduler advancement. Incremental zone search and scheduler/service fairness
integration are next. All four libraries remain in development; native Linux,
protocol/recovery, production performance and >=72h acceptance remain open.

## 2026-11-05 - zoned queries yield inside every traversal layer

Previous goal turn was progress: calendar cursor commit f49b186d was validated
and synchronized to master. Both worktrees start this stage clean at that parent.
The live scheduler still calls synchronous recurrence search. This stage builds
its next prerequisite: real ordered zoned Init/Step queries with three independent
budgets, including the403-rule-year next-transition fallback hidden inside a
nominal single segment. No Core change or new shared runtime is introduced.

Init copies the expression/window and records zone identity, rounds the start
and calculates the periodic suffix bound without traversing. Each Step bounds
new UTC segment starts, calendar month visits and transition rule years; the
actual month/year loops resume internally. Capped logarithmic history lookup
and a fixed five-year state lookup are bounded segment overhead. All folds,
gaps, unknown coverage and history-before-periodicity semantics remain. Only
FOUND writes Next; PENDING preserves it, and terminal results/counters repeat
without traversal. Typed value errors survive diagnostic OOM; failed reset,
argument/state and structural aliases preserve their documented outputs.

The512-byte address-bound context borrows the same live immutable zone explicitly
passed at every call. It copies expression/bounds, retains no reference, requests
no heap and has no Unit duty. Serialized handoff requires the caller's zone owner
to remain alive. New issue32 and budget12 leave the legacy issue24 and existing
declarations/layouts/enum values intact. Both x64 direct/declaration probes pass.
The typed value is416 bytes and private transition walk120. Public Step copies
private state in/out (at most512 bytes each way), validates spans/identity/budgets
and optionally publishes32 issue bytes. Init avoids duplicate whole-context
clearing. The typed engine is available for later kernel integration without
opaque wrapper copies. No zero-overhead or elapsed CPU guarantee is claimed.

Golden folds/gaps,160 brute windows and1000 full-int64 UTC-specialization cases
also execute varied cursor budgets. Complete400-year absence and a500-year
history prefix before the first2500 footer result survive resumptions.600 private
transition comparisons cover rare leap/zero-delta/extreme rules; an exclusive
general-engine fixture retains403 exact fallback yields normally pruned at
creation. Tests exercise century rule-year yields,1024 zero-heap resets,
snapshots/identity/aliases, wrapped spans, each zero budget, repeated terminals,
unknown boundaries under OOM and explicit-owner thread handoff.

Frozen422-zone2026e input bytes match every prior corpus digest and version.
The unchanged independent zoneinfo label-enumeration/UTC-sorting oracle validates
63180 actual cursor Next queries on each platform, including347 unknown errors;
218700 additional Match queries still test legacy Match. All three budget deltas
are checked by the actual compiled cursor probe. Windows/WSL each pass19 full
modular/single programs, the updated offline example and static/shared consumers
executing both new symbols. WSL uses O2 ASan/UBSan/leak-halt plus two separate O1
TSan units. Windows79 tool tests and feature/single/reference/docs checks pass.
The final2472 protected inputs match on both platforms and remain unchanged.

Six new public-cursor profiles include Init, every Step and result inspection,
at budgets1/16 over dense/fold/full-cycle queries. Adding these profiles changes
the global benchmark fingerprint; original reports are preserved, never forcibly
paired. Separately archived baseline binaries and unchanged legacy workloads use
five alternating before/after repetitions. Ratios are Windows1.0528/1.0887/1.0070
and WSL0.9915/0.9944/0.9787 for dense/fold/cycle. Every median stays within the
original5% regression limit, but several MAD3%/central-range6% quality gates fail.
There is no qualified no-regression or speedup claim. Full-cycle cursor budgets
1/16 require6801/350 host turns for the same801 segments,5600 months and2002 rule
years, with equal checksums. Dense setup and frequent wrapper calls have material
cost; all measured samples and controlled-pair warmups are retained.

Evidence: extlibs/xcron/tests/results/zoned-cursor-20261105.json and local
`out/service-extensions/xcron-zoned-cursor/`. Existing dangling cache junctions
are preserved; new aliases and explicit WSL Git paths avoid them. Next integrate
typed work into actual scheduler admission/advancement and owner-turn global
budgets with fairness/cancellation/reserved-output tests. Named database/reload,
nominal delivery, durable recovery, original Redis/RPC/MQTT scope and native/
performance/>=72h production acceptance remain active across all four libraries.

## 2026-11-05 - budgeted live admission and recurrence advancement

Previous goal turn was progress: ee3a125a was validated and synchronized to
master. Both worktrees started this stage clean at that parent. The actual
scheduler and service now consume the typed resumable zoned engine. No Core
change, new dependency or shared runtime is introduced.

New PollBudgeted applies three positive GLOBAL per-call search budgets across
all jobs, alongside the existing WorkLimit action cap. Heap/ready/search classes
rotate; pending searches use FIFO one-unit quanta. Capacity0 advances queries
without publishing a lease, even when completion output and run slots are full.
SEARCHING is nonterminal; Complete preserves its query and Data, and Cancel
removes it without evaluating the remaining history. Later faults preserve live
leases. Existing Add/Poll stay synchronous, with the same public layouts; mixing
old Poll into pending work gives up the recurrence-work bound.

The live owner reserves an unpublished job slot for ADD, then computes its first
occurrence through the same budgeted queue. Only successful completion commits
the public ID. Failed/closed admission releases its reservation and retained zone,
returns original Data through the reply and creates no job/retirement duty.
Public Job/Cancel cannot use a guessed provisional ID. FIFO command dequeue does
not imply ordered replies: a quick command or later light ADD can finish first.
Per-turn admission inspections are capped before and after calculation. Every
turn budgets each traversal resource to Scheduler.WorkLimit, even with zero
output reservations. Existing close/job scans, adapter cancellation/run scans,
foreign callbacks, error allocation and OS scheduling still preclude a hard
whole-owner CPU/P99 bound.

One416-byte typed query per configured job is added to the single kernel
allocation, plus a16-byte private header increase and up to4 alignment bytes on
verified x64 targets. The hot job-slot stride remains240 using existing padding.
BytesLimit accounts for the entire reserve; cold allocation/zeroing are real
costs. Dense successors use a shared charged constant probe, avoiding full query
initialization. A miss seeds the initialized arena without repeating its charged
lookup; no heap, public opaque copies, lock or hidden zone retain occurs during
normal successor calculation. Service LastPoll adds one mutex-protected40-byte
publication. Service stats grows344 to384 bytes (LastPoll at344); configuration
remains136. Direct and generated-declaration probes agree on both platforms;
consumers must rebuild matching headers.

The finite independent policy model now checks184320 samples (92160 through each
entry point). Adversarial tests cover mixed long/light queries, global budgets,
full leases/zero output, cancellation, provisional commit/rollback, guessed IDs,
typed deferred OOM and later faults with two live leases.128 sparse lifecycles
produce more than10000 real paused/resumed turns under a one-byte allocation
quota, preserving ambient diagnostics and requested-allocation counters.
An actual clock-credit barrier holds a full completion output and400-year absent
initial query across an owner boundary while later Reap/light ADD progress.
Close returns original Data without an ID/body; a separate unknown mapping also
fails transactionally. The archived parent libraries fail that same boundary
witness on each OS, while the new owner passes; no latency-threshold inference
or fabricated reply is used.

Two rejected prototypes remain in the evidence. Clearing a full query for every
dense successor initially cost about18% at128 jobs; the charged ready probe and
separate hot/query arrays removed that specific gap. A second standalone
Capacity1/WorkLimit3 probe then found real cross-call starvation despite correct
per-call counters: eager dense work repeatedly consumed all segment/month credit
before an older query. The failing binary is retained. Reserving the last credit
for queued queries fixes it, and the regression is in the final full suites.
Neither prototype's earlier passing checks substitute for final validation.

Final Windows/WSL source maps match all2477 protected inputs and remain unchanged
through every accepted run (the new evidence file excludes its own hash).
Each platform passes19 modular/single programs, the updated scheduler example,
and static/shared consumers executing the actual new API. Windows79 tool tests
and feature/single/reference/docs checks pass. WSL uses O2 ASan/UBSan/leak-halt
and four separate O1 TSan scheduler/service modular/single units. Frozen422-zone
2026e independent zoneinfo oracles validate63180 actual cursor Next queries and
218700 legacy Match queries per OS after sharing the ready-probe helper. Final
rebuilt oracle executables have identical hashes to the checked executables.

Separate dense budgeted1/128 profiles include all recurrence work and physical
Complete, with whole-batch P99. Adding profiles changes the global fingerprint;
old reports are not rewritten or forcibly paired. Archived parent binaries and
unchanged legacy workloads also run five alternating before/sync/budgeted trials,
with all samples/warmups retained and the original5% throughput/10% P99 and
MAD3%/central-range6% gates. Windows unpinned budgeted/parent throughput ratios
are0.7570/1.0171 and P99 ratios1.5000/1.0385 for1/128 jobs. The separate process-
affinity probe (CPU4,1M instead of200K one-job batches) gives throughput
0.9582/0.9626 and P99 1.0000/1.1148. The128-job pinned P99 exceeds the original10%
limit; the unpinned one-job throughput and P99 exceed their limits too. These
remain failures, not passes excused by noise. WSL unpinned ratios are
1.0204/0.9992 throughput and1.0000/1.0284 P99; pinned ratios1.0006/1.0238 and
1.0000/0.9761. Both formal platform reports still fail noise gates. No qualified
no-regression, maximum throughput or production P99 claim follows. Other local
work was observed compiling in WSL; its work/process settings were preserved.

Portable evidence: extlibs/xcron/tests/results/bounded-owner-20261105.json.
Raw logs, source maps, archived parent libraries/binaries, rejected prototypes,
barrier/starvation witnesses and all performance trials remain under
`out/service-extensions/xcron-bounded-scheduler/`, including pre-credit and
pre-empty-shortcut snapshots. Next resolve the Windows performance differences
with qualified native measurements; named database/reload, nominal DST delivery,
durable recovery and the original Redis/RPC/MQTT scopes remain active. None of
the four libraries has completed the native/performance/>=72h production gates.

## 2026-11-05 - budgeted scheduler control-path measurement and refinement

Previous goal turn was progress: f64a1bad delivered transactional live admission
and global recurrence budgets and was synchronized to master. Both worktrees
start clean there. This stage addresses the recorded control-path overhead;
it does not close the original performance or product acceptance gates.

Each action formerly built a three-entry availability array and performed a
modulo/index loop. Direct ordered branches now implement the exact same cyclic
orders0/1/2,1/2/0,2/0/1 with the same turn update. Eager query results replace the
complete LastIssue/HasNext value directly, avoiding an immediately overwritten
clear; every empty/error/deferred path still initializes its complete result.
No public/private layout, ABI, budget, action cap, query credit, ownership,
allocation, lock or error contract changes. Generated single code is updated.

GCC static disassembly confirms removal of the modulo3 selection and16 fewer
prologue stack-reservation bytes on both x64 platforms (Windows408 to392,
WSL360 to344, excluding pushes/callees). Full-function static instruction counts
are700 to693 and673 to679 respectively: this is not a claim that every path has
fewer executed instructions or a guarantee of speedup. Existing query416/job,
hot slot240, validation/issue/output/LastPoll copies, calendar/zone/queue work,
startup reserve and cold scans retain their real costs. No Core/runtime change.

Final2478 protected inputs agree on Windows/WSL and remain unchanged through
every accepted check. Both pass19 modular/single programs, updated scheduler
example and static/shared real consumers. The184320 independent policy-model
samples per scheduler program, actual clock-boundary owner tests, global budget/
output-credit/admission/physical-lease/OOM/zero-heap cases and cross-call starvation
regression remain intact. WSL O2 ASan/UBSan/leak-halt and four O1 TSan units pass;
Windows79 tool tests and feature/single/reference/docs checks pass. No extra
implementation-mirroring test substitutes for those behavioral checks.

The four existing profiles, source workload, flags and5% throughput/10% P99 plus
MAD3%/central-range6% gates are unchanged. Official current/baseline fingerprints
match naturally; neither report is rewritten or force paired. Both formal
reports still fail noise gates. Their raw median comparisons also breach original
limits, including budgeted1-job throughput (Windows4.632M versus5.497M/s and
WSL5.026M versus6.981M/s), WSL budgeted128-job P99 (23.8 versus20.1us) and
several unchanged synchronous controls. These remain unresolved failed samples,
not a qualified no-regression result. Further five alternating samples retain archived
ee3a125a pre-budget, f64a1bad synchronous/budgeted and current synchronous/budgeted
executables, every warmup, stdout/checksum and binary hash. Default probes retain
200K/10K1/128-job batches. Separate own-process CPU4 affinity probes use1M/100K
batches; they never replace the original qualification path.

Windows pinned budgeted/before ratios are1.06178/1.04498 throughput and
1.00000/0.96059 batch P99 for1/128 jobs. Versus the same-window pre-budget control,
current ratios are1.11920/0.99535 throughput and1.00000/1.00000 P99. Unpinned
budgeted/before ratios are0.99895/1.01595 throughput and0.66667/1.03382 P99.
These observations do not close noise/native/production gates or erase the
previous stage's failures. Actual Windows QPC frequency is10MHz (nominal0.1us
tick);0.2/0.3us single-job P99 still obeys the original threshold without a
quantization waiver.

WSL exposes conflicting original/bulk results. Unpinned budgeted/before
throughput0.94125/1.05060 and P99 1.00000/1.10965 exceed the original1-job
throughput and128-job P99 limits. Pinned throughput0.92797/1.03603 also breaches
the1-job5% limit; P99 is1.00000/0.96569. A targeted three-binary1M single-job
isolation then gives class-only1.00098 and both-edits1.02222 versus before,
with equal P99. It does not reproduce the suspected second-edit slowdown, but
does not identify the root cause or excuse/remove any failed sample.

The separate bulk diagnostic removes per-batch timer reads AND sample-array
allocation/writes, retaining actual kernel Poll/advance/Complete and reporting
no latency or worker/body execution. Windows records thread cycles; Linux records
thread CPU nanoseconds, incomparable across platforms. Pinned budgeted CPU-unit
ratios are Windows0.90984/0.96133 and WSL0.95317/0.96300 for1/128 jobs. Unchanged
synchronous controls and all raw trials are retained to expose environment/code-
placement variation; the differences cannot be attributed to timers alone.
Local unrelated work was observed compiling on both systems and was preserved.
No qualified general speedup/no-regression or production P99 claim follows.

Portable evidence: extlibs/xcron/tests/results/budget-hotpath-20261105.json.
Local raw recipes, assembly, before/class-only/current sources and binaries,
setup-failure logs, full validation and original/CPU/isolation samples remain in
`out/service-extensions/xcron-budget-hotpath/`. Native deployment isolation and
the unresolved original performance gates remain required. Continue named-zone
database/reload, nominal DST delivery, durability and the full Redis/RPC/MQTT
scopes; all four libraries remain developing with native/performance/>=72h gates
open. No recurring benchmark rerun is treated as new product completion.

### 2026-11-08: xcron named TZif snapshots and explicit root reload

Implemented `xcron_tzdb` and optional `xcron_tzdb_file` as developing leaves,
with11 public operations and runnable memory/file examples. Exact case-sensitive
names select independently decoded immutable TZif zones; no process TZ, implicit
directory, worker, download or shared runtime is introduced. Cold builders copy
and heapsort the manifest outside the publication mutex, then atomically publish
a complete version with expected-revision checks. Readers retain snapshots and
independent zones across reload/close/manager destruction. Existing jobs retain
their original zone until explicit replacement; no cursor or lease is rebased.

One Core allocation quota charges manager, all candidate/current/retired catalog
arenas, zones/footer rules and temporary load allocations until physically freed.
Builder/name/byte limits reject excess work. The fixed quota-control allocation,
native mutex, allocator backing/headers, caller root/inputs and final diagnostic
remain outside the requested-byte cap; inner lower-level diagnostics are charged.
Normal Find performs bounded validation/binary search and a retained zone result,
without heap/IO/publication lock. Acquire adds a mutex/refcount. Aliases are
decoded separately; no zero-overhead/deduplication claim is made.

Root loading accepts an explicit trusted immutable release root/name manifest,
regular files <=16MiB, full read/EOF and second metadata check. Observable change,
missing/escaping/invalid input rejects the whole candidate and rolls it back.
Publication is atomic, but reading a mutable directory is not a filesystem-wide
transaction or authenticated release check. Destroy requires Close, zero builders
and caller-enforced physical exclusion/join of every database call; Loading0
alone is insufficient. Old external owners can be released afterward.

Functional verification ran November5 before the requested pause; resume-time
hash checks on November8 confirm identical product/tests/generated source bytes.
Both Windows and WSL pass23 modular/single programs, both examples, six actual
IO-wrapper failure paths and static/shared consumers invoking all11 operations.
WSL additionally passes O2 ASan/UBSan/leak-halt and two O1 TSan units. Windows85
tool tests and feature/single/reference/docs checks pass. Public reference now
lists67 functions,164 constants and71 types. Direct/generated declarations agree
on new public layouts16/32/48/40/64; existing service136/384 layouts are unchanged.

Behavioral tests include286 quota boundaries (all8 footer/history allocation
positions), malformed-input diagnostic OOM, exact old/candidate quota accounting,
snapshot/zone lifetime,10000 positive plus10000 negative no-allocation lookups,
reference/revision saturation, held builders/CAS/close rollback and four readers
through250 real publications. Each OS imports one422-zone2026e snapshot and
checks54173 independent offset/state/name queries after input/root/manager
destruction. Separate3798 named loads/lookups then serve63180 real public cursor
Next calls and218700 legacy Match calls, with680 expected coverage errors.
Windows symlink creation is skipped with native error1314; WSL exercises actual
contained aliases and escape to an existing valid outside file. Wrapped close
faults consume the file before reporting failure; this does not prove every
native failed-close scenario. Setup/probe compile failures and their logs remain.

Only benchmark post-sort output/profile metadata changed after the functional
window, adding batch P50/P95/P99.9 and explicit batch/name/acquisition facts.
The timed loop and product bytes are unchanged. Final Windows/WSL measurement
maps agree on2495 captured inputs and match current source. A targeted13-tool-
test rerun and docs/single checks pass on resume. The original functional map is
retained, with exactly those two benchmark-only differences recorded separately.

Four O2 profiles retain5 repeats/1 warmup, MAD3%/central-range6% noise and original
5% throughput/10% P99 defaults. Both initial and expanded reports still exit1 on
noise; neither is accepted or force paired with another baseline. Final raw
medians for16/1024 names are Windows11.270M/7.201M lookups/s on a held snapshot,
8.679M/5.398M with Acquire per lookup; WSL18.854M/7.458M and11.472M/5.943M.
Whole128-lookup batch P99 medians are17.8/31.3us and26.1/41.0us on Windows,
13.4/34.8us and23.5/38.3us on WSL. They exclude cold load/IO/concurrent reload/
workers/body and are not per-lookup or production latency claims. Single/static/
shared size checks pass for memory and file leaves; single includes the XRT
closure, whereas static/shared figures own xcron only with separate companion
XRT. Their sizes cannot be compared as equivalent artifacts.

Portable evidence: `extlibs/xcron/tests/results/tzdb-snapshots-20261108.json`.
Raw frozen maps, accepted/failed receipts, commands, binaries, logs, reference
corpus identities and both performance windows remain under
`out/service-extensions/xcron-tzdb/`. This stage is based on0a35e1d2; the eight
new independent master commits will be integrated with separate closure evidence.
Native Linux, original performance acceptance, >=72h runs, nominal DST delivery,
durability and the remaining Redis/RPC/MQTT scope stay open. All four products
remain developing and the overall goal remains active.

### 2026-11-08: preserve and integrate concurrent master work

After feature commit `dc6914b2`, normally merged the eight independent commits
through master `a01b4a13` in the service worktree with no conflict. All value,
xwork and xllm-session changes/history are preserved. Closure analysis shows
only xrpc selects the incoming `src/value/value_container.c`; selected runtime
sources for xredis/xmqtt/xcron are unchanged. Core generated single/decl headers
have changed, so integrated consumers are tested separately from the old window.

Windows and WSL pass the actual memory-debug object-post-commit/scoped-read
reproducer,32 xrpc modular/single programs and23 xcron modular/single programs,
including real HTTP/TLS client/server and scheduler ownership paths. WSL uses
O2 ASan/UBSan with leak/halt settings. Both platforms pass xcron static/shared
real consumers. Core and all four extension generated singles check clean.
Captured2905-input maps match between platforms and are verified before/after
each command. The initial ignored runner chose `value` instead of the owning
`value_container` test suite; that selector failure and receipts are retained,
and exact successful generated-check commands/log hashes are verified for reuse.

No xcron product bytes are changed by this merge, and no benchmark is rerun or
qualified. The feature-stage oracle/layout/TSan and failed noise results retain
their original source identities; no new execution of those checks is claimed.
Windows native symlink creation, native Linux, >=72h and final production gates
remain open. Integration evidence is
`extlibs/xcron/tests/results/tzdb-master-integration-20261108.json`; raw receipts,
maps, closure recipes and logs are under the feature's ignored `integration/`
directory. Final synchronization is a clean-primary-checkout fast-forward only;
no remote push is authorized or performed. Continue nominal DST delivery and
durable recovery, plus the remaining Redis/RPC/MQTT scopes; goal stays active.

### 2026-11-08: explicit complete nominal-label selection

Added the developing `xcron_delivery` calculation leaf and public
`xcronDeliveryResolve`. It resolves one floating label completely, then applies
ALL/FIRST/LAST/REJECT fold selection and SKIP/REJECT missing-label handling.
The complete candidate count remains available independently of selected output
capacity. Global first/last selection does not change when a later query window
excludes earlier instants. Unknown coverage cannot be waived by selection.
Valid civil labels outside the representable timeline are missing under this API.
Fractions, more than two candidates, negative/zero/half-hour DST and the full
signed-int64 timestamp endpoints are covered. Structural failures preserve every
output; typed policy/zone errors survive diagnostic OOM. No new owned resource,
thread, timer, filesystem, network or shared runtime is introduced.

This is the single-label foundation for nominal delivery. It does not implement
expression matching, recurring policy search, gap shift/compensation, physical
delivery metadata or persistence. Existing actual-instant zoned/kernel/service
semantics and implementations are unchanged. These remaining stages are still
required by the full goal, not removed from its scope.

Both Windows and WSL pass 25 full modular/single programs, the offline example,
static/shared real consumers and direct/generated declaration layouts (policy8,
issue16 on both x64 platforms). WSL uses O2 ASan/UBSan/leak-halt and additionally
passes two O1 TSan selection units. Windows 85 tooling tests and generated/docs
checks pass; the public reference now contains 68 functions, 181 constants and
77 types. The first prototype test compile's misleading-indentation error is
fixed before the accepted window; original failure logs are retained.

A raw independently authored TZif constructs 256 simultaneous valid candidates
with a shared designation and known future footer. Independent UTC stepping
checks 768 labels through 64 arbitrary histories under all eight policies,
including required capacity and unchanged output tails. Tests cover global
unknown completeness, aliases/wrapped spans, typed strict errors under OOM,
20000 warmed calls with unchanged allocation/error identity and four concurrent
retained owners. The IANA oracle independently uses zoneinfo states, stored/footer
offsets and explicit RFC unknown coverage, checking all eight policies after
source-byte retirement. Each OS checks the same 422 frozen 2026e zones, 188532
labels and 1508256 calls: 946736 FOUND, 207712 NONE, 102040 AMBIGUOUS, 207712
MISSING and 44056 UNKNOWN. Their complete portable oracle JSONs agree exactly.

One 258-timestamp selection scratch remains in addition to the underlying
resolver's temporary; normal operations request no heap. ALL+SKIP delegates the
original complete resolver after its own checks. Selected paths copy only their
accepted result after capacity checks. O2 assembly retains separate helpers:
Windows main/selected static stack subtractions are 184/2176 bytes; WSL main has
120 and an additional8-byte subtraction, selected2136. These are static local
frame instructions excluding pushes/callees, not a whole-call stack maximum.
Repeated validation/calls/copies and complete unknown checks are real costs.

Both platforms pass single/static/shared size profiles and forbidden symbols.
Single objects include their Core closure; static/shared own xcron only and use
a separate companion XRT. The original performance configuration incorrectly
used a facts dictionary: the tool rejected it before compiling any benchmark.
The failed log/config/runner are retained. Only that JSON metadata changes after
the functional window, to a list of fact names. Product/tests/generated source
bytes are identical; original functional/size maps and receipts are preserved.
The final 2505-input measurement maps agree between platforms and are verified
before/after measurement; 13 targeted performance-tool tests also pass.

Four sequential-platform O2 profiles time original resolution, ALL, FIRST and
LAST in 32-call batches, five repeats plus one warmup, retaining P50/P95/P99/P99.9
and original MAD3%/central6% plus 5% throughput/10% P99 defaults. Both reports
exit1 on noise and remain unaccepted; no baseline is rewritten or force paired.
Windows raw throughput medians are 7.152M/6.164M/6.026M/5.679M labels/s and batch
P99 6.7/6.4/8.9/9.3us respectively. WSL records
6.072M/5.541M/5.348M/5.605M and 8.2/9.6/10.3/9.4us. These are unqualified
observations for a precompiled two-fold POSIX label, excluding cold work,
binary-history throughput, unknown/errors, recurring search, queueing and bodies.
The identical-result ALL path's observed raw throughput ratios are about 0.862
and 0.913; this is not a qualified regression bound or a causal estimate. Source
review identifies duplicate output validation even with no requested Issue.
Optimize that path next while preserving all alias/typed-error/unknown contracts;
the raw current samples are the archived before state, not an accepted baseline.

Portable evidence: `extlibs/xcron/tests/results/delivery-selection-20261108.json`.
Raw commands, source maps, probes, oracle reports, prototype/config failures,
assembly and samples remain under `out/service-extensions/xcron-delivery/`.
Native Linux, production performance, >=72h, recurring selection/compensation,
durability and the remaining Redis/RPC/MQTT scopes stay open. All four products
remain developing and the overall goal stays active.

### 2026-11-08: remove duplicate validation for optional-Issue ALL selection

Optimized ALL+SKIP with IssueNULL to delegate complete output/input/owner/footer
validation and resolution to the original zone resolver exactly once. An
ARGUMENT-initialized typed issue distinguishes structural rejection (the zone
contract preserves that output) from typed date/unknown failures. The outward
delivery domain/code/kind mapping uses that typed result, never ambient errors.
Requested-Issue and selected-policy paths retain their complete validation and
selection contracts. Public signatures/structures/limits and ownership stay
unchanged. No Core capability, worker, normal-path allocation/syscall or shared
runtime is added. The retained
underlying typed issue, wrapper call/argument moves and selected scratch remain;
this is not a zero-overhead conversion. Delegated structural failures can now
attempt both lower-level and outward diagnostics; that extra cold error cost is
explicit, while normal FOUND/NONE/CAPACITY still request no heap.

Before edits, verified all 2505 previous measurement inputs against the actual
98be7d1e source, then archived all eight original Windows/WSL benchmark executables,
source witnesses and untouched reports. The original benchmark program, four
profiles, arguments, timer/sample/checksum loop, flags, facts and gates are
unchanged. Current/before benchmark fingerprints and environments match
naturally. No baseline is rewritten or force paired; original noisy reports
remain rejected rather than becoming a qualified reference.

Both platforms pass 25 modular/single programs, the runnable example and real
static/shared consumers. WSL O2 ASan/UBSan/leak-halt and two O1 TSan units pass;
Windows 85 tooling and generated/docs checks pass. Direct/generated declarations
still agree on policy8/issue16. New semantic tests pair whole arrays/counts with
and without Issue for every golden and 768 brute label case, including 256 real
folds. Direct checks cover NULL/overflow/input/array/count/zone/footer aliases,
argument versus date/unknown error identity and diagnostic OOM. The warmed loop
now makes 30000 calls, alternating Issue and including short-capacity discovery,
with unchanged error identity/allocation counters and no one-byte-quota rejection.

The independent 422-zone 2026e oracle requires a new probe protocol and checks
all eight policies twice per label, with/without Issue. Each OS checks 188532
labels, 1508256 independent policy cases and 3016512 physical calls: 1893472
FOUND, 415424 NONE, 204080 AMBIGUOUS, 415424 MISSING and 88112 UNKNOWN. The probe
compares complete arrays, counts and error domain/code; both oracle JSONs agree.
The first paired-probe compile failed a signed enum/int comparison warning.
Only that application-probe cast changed afterward; product/tests/headers stayed
identical. Initial maps, failed logs and receipts are retained. Already passed
suite/example/static/shared checks are reused only after exact command/log hashes
and all unchanged inputs are verified; their logs do not compile the probe.
Remaining checks run on the resumed source identity. Final Windows/WSL maps
agree on 2506 inputs, with that one noncompiled-probe delta recorded separately.

Single/static/shared size and forbidden-symbol checks pass. Static owned xcron
text grows by 176 bytes on Windows and 157 on WSL; data/BSS stay unchanged.
Single objects include Core and grow by 276/341 text bytes. Static/shared figures
own xcron only with separate companion Core, not equivalent deployment sizes.
O2 main/selected stack reservations are unchanged: Windows184/2176, WSL main120
plus an additional8-byte subtraction and selected2136. These exclude register
pushes/callees and are not a maximum whole-call stack measurement.

Both official performance comparisons exit1 on original noise gates, before
their baseline-threshold phase. Separate read-only raw-median comparisons retain
no Windows threshold failures and a WSL unchanged-RAW P99.9 violation (20.9us
versus18.9us, above10%). Current formal throughput medians for raw/ALL/FIRST/LAST
are Windows7.211M/6.660M/6.138M/6.102M and WSL6.822M/6.229M/5.733M/5.704M labels/s;
whole32-call P99 medians are5.5/5.8/6.4/6.5us and7.7/7.6/10.4/9.7us. Original
5% throughput/10% P99 and MAD3%/central6% contracts are unchanged. No qualified
no-regression, speedup or production-tail claim follows from these noisy samples.

Additional same-window diagnostics alternate saved before/current binaries in
five forward/reverse rounds, retaining all eight warmups and forty measurements
per OS. Original10000 batches of32 actual calls and every path remain; independent
Gregorian expected checksums are required on every execution. Windows ALL raw
current/before ratios are1.06812 throughput,0.94416 full-process cycles and0.96386
P99. WSL records1.06369 throughput,0.93886 full-process CPU time and0.86022 P99.
Unchanged RAW controls give0.98165/0.98927 throughput and1.01653/1.01064 CPU ratios.
Windows ALL P95 rises1.13462; unchanged RAW P95/P99.9 also rise1.22727/1.32468.
All observations, including threshold breaches and unchanged-mode variation, are
retained rather than used to waive the failed original gates. Full-process CPU
includes startup, zone construction, allocation/sort/output and cleanup outside
the timed loop. Windows cycles and Linux CPU nanoseconds are incomparable units.
Cold/history/error/with-Issue/recurring/worker contention are not performance
qualified by this single precompiled POSIX fold workload.

Portable evidence: `extlibs/xcron/tests/results/delivery-fastpath-20261108.json`.
All raw maps, source/binary identities, failed-probe setup, accepted receipts,
assembly, official/alternating samples and CPU counters remain under
`out/service-extensions/xcron-delivery-fastpath/`. Continue budgeted recurring
selection and gap compensation, then physical delivery/durable recovery and the
remaining Redis/RPC/MQTT scope. Native Linux, >=72h and full production/performance
acceptance stay open; all four libraries remain developing and goal stays active.

### 2026-11-08: resumable complete nominal-label resolution

Added DeliveryCursorInit/Step to the optional delivery leaf. Independent Checks,
Offsets and RuleYears budgets bound actual traversal increments; zero in a needed
dimension yields PENDING. Unknown coverage is established before candidate probes,
including incremental footer transition searches. Complete all/first/last/reject
and missing skip/reject semantics match the original synchronous API. PENDING
preserves public Count/array and hides partial multiplicity; terminal results and
retryable capacity discovery repeat without traversal. No scheduler job semantics
change yet. Cursor borrows the exact immutable zone at a stable address and permits
serialized thread handoff; normal operations request no heap/IO/locks or retention.

The private typed resolver is208 bytes on both x64 platforms; public control is240
bytes inside2560 caller-owned bytes with space for258 ALL timestamps. Init includes
fixed-storage zeroing; Step copies only control plus newly found ALL values, then
copies selected terminal results. FIRST/LAST/REJECT omit full-array staging in the
private engine, allowing future scheduler state to use the compact representation.
These are counted design costs, not a zero-overhead or elapsed owner-turn bound.
Existing synchronous resolution code is unchanged; guarded TZif helpers are new.

Both platforms pass25 modular/single programs, updated runnable example, real
static/shared consumers and direct/generated public layouts. WSL O2 ASan/UBSan/
leak-halt and two O1 TSan units pass; Windows85 tooling/generated/docs checks pass.
Every golden and768 independently UTC-stepped label runs all8 cursor policies,
including256 simultaneous actual folds, full-int64 fractions, zero budgets,
unchanged pending arrays, terminal capacity retries and copied inputs. Dedicated
cases cover invalid/copied/wrong-zone/aliased cursors, diagnostic OOM,1024 warmed
zero-requested-heap resets/error identity and native serialized handoff. A real
leap-only unknown footer yields over9 actual rule years across2100 before UNKNOWN;
a separately labelled private general-engine model exercises403 one-year resumptions
with the constructor shortcut disabled. This model is not normal public latency.

Both frozen422-zone2026e oracle reports agree. In cursor mode each OS checks188532
labels,1508256 complete cursor resolutions,4025606 actual Steps,1508256 Checks and
5746024 offset probes. The same run pairs3016512 original synchronous resolutions
with/without Issue against independent zoneinfo/RFC expectations. Init+Step+sync
therefore totals8550374 API calls, not a count of distinct policy cases. This corpus
needs no footer search years; the explicit9/403-year cases cover that dimension.

Prototype failures are retained: an indentation warning, incorrect6-versus8-year
fixture expectation, and an empty-season fixture removed by the constructor shortcut.
Final full validation initially failed only a package-consumer local variable name
collision. After all readers terminated, only that consumer variable was renamed;
successful suite/example commands/log hashes and absence of consumer compilation
were verified before reuse. Initial/failed/resumed receipts and source maps remain.
Windows/WSL final functional maps agree on2510 inputs. Subsequent measurement maps
differ only by appending three new performance profiles; original profiles/defaults,
old timed source, arguments, gates and benchmark fingerprints are unchanged for
the original four-profile comparison, which completed before that metadata change.

Size/forbidden-symbol checks pass. Static owned xcron text grows4600 Windows/4969
WSL bytes; data/BSS are unchanged. Single objects include Core and grow4960/5202
text bytes; static/shared own xcron with a separate companion Core. No equivalence
between these deployment sizes is implied. Original formal comparisons both exit1
on noise and retain8 Windows/11 WSL raw-median threshold violations diagnostically.
One additional forward/reverse alternating collection keeps8 warmups+40 samples
per OS. ALL current/before throughput ratios are1.06629/1.01821, while unchanged RAW
is0.94296/0.95006; tail violations also remain. These observations do not certify a
speedup or no-regression and do not waive original failed gates or noisy baselines.

New full/one-offset/synchronous-control profiles time10000 whole32-label batches,
including every Init and Step. Independent Gregorian checksums and exact aggregate
step/check/offset facts agree. Throughput medians are Windows4.891M/4.288M/6.429M
and WSL4.756M/4.359M/6.300M labels/s; whole-batch P99 is11.0/12.6/8.6us and
11.3/11.3/7.5us. Both new reports exit1 on original noise gates. This precompiled
two-fold POSIX workload includes cursor cold zeroing/validation/control/result
copies, excludes zone construction/history/unknown/errors/recurrence/worker load,
and establishes no per-operation percentile or production acceptance.

Portable evidence: `extlibs/xcron/tests/results/delivery-cursor-20261108.json`.
Raw maps, original failures, exact commands/logs, saved binaries, oracle reports,
samples/checksums/CPU counters remain in `out/service-extensions/xcron-delivery-cursor/`.
Continue recurring policy search, gap compensation and physical/durable delivery;
remaining Redis/RPC/MQTT scope, native Linux, >=72h and production performance/
reliability gates stay required. All four products developing; overall goal active.


### 2026-11-08: complete selected recurring queries with budgeted gap rejection

Added the optional developing xcron_occurrence calculation leaf, composing the
existing delivery and zoned leaves. Next/Init/Step select the earliest actual
match with globally complete ALL/FIRST/LAST/REJECT semantics; a candidate outside
the query still determines FIRST/LAST. Missing REJECT checks matching labels in
each positive-offset transition gap anchored at its UTC boundary, including the
rounded start and preceding an actual result/error on a tie. Every apparent gap
label is globally resolved: another segment can fill it. Unknown coverage is
never replaced by a guessed result. Canonical nominal metadata accompanies the
actual result or typed error when known. Signed-int64/fractional bounds remain.
The proof limit completes history, then includes the maximum legal offset-window
margin before a full400-year periodic suffix; no arbitrary400-year history cutoff.

Five independent nonnegative budgets count actual segments, calendar months,
transition-search years, complete-label checks and distinct offset probes across
all engines. PENDING hides provisional metadata and preserves the output; all-zero
pending calls preserve cursor bytes. Terminal results repeat without traversal.
Init copies expression/bounds/policy and borrows the exact immutable zone at a
stable address, without search/retention/Unit duty. Serialized native handoff is
supported. Normal paths request no heap/IO/lock and preserve ambient error; typed
issues survive diagnostic OOM. Private typed state is880 bytes on both x64 targets,
control904, public cursor1024, occurrence64, budget20 and issue128. Cold zeroing,
904-byte wrapper copies each way, validation/state lookups, canonical metadata
and final result/Issue copies are explicit costs. No zero-overhead claim is made.
The private engine reuses dormant actual calendar/walk storage for gap validation.

No Core source/dependency/shared runtime changes. A guarded product-private helper
continues canonical civil calendar search strictly after a label without requiring
its civil value to fit xtime. Original synchronous/calendar/zoned control loops
are unchanged. Root/leaf builds, umbrella/features, generated single/declarations,
public API reference and runnable offline example are registered. The reference
now has73 functions,199 constants/macros and86 public types. Existing kernel/
service jobs keep their original actual-occurrence semantics pending integration.

Both OSes pass27 modular/single programs, the standalone leaf example and actual
root AND isolated leaf static/shared consumers. WSL O2 ASan/UBSan/leak-halt and two
O1 TSan units pass; Windows85 tooling and generated/docs checks pass. New tests
independently enumerate UTC seconds and all offsets for32 arbitrary8-transition
histories under8 windows/all8 policies (2048 queries), with both sync Issue forms
and cursors. Separate256-fold,500-year historical prefix, whole-cycle always-gap,
rare unknown footer, full-int64, gap-fill/tie/window, independent zero-budget,
snapshot/alias/state/OOM,1024 warmed zero-requested-heap and joined handoff cases
pass. The existing Windows symlink fixture remains skipped1314; this is not native
Linux or a completed production/long-run qualification.

The frozen422-zone2026e oracle independently enumerates Gregorian labels, all
stored/footer offsets and global candidates, RFC coverage and footer M-rule UTC
transitions. Its new required protocol checks all nominal metadata, including
Weekday/YearDay, and physically executes both sync forms plus a budgeted cursor.
Both full reports agree:121664 queries (114452 FOUND,152 NONE,1896 AMBIGUOUS,
2860 MISSING,2304 UNKNOWN),243328 synchronous calls,121664 cursor initializations,
401818 Steps and766810 total API calls per OS. Actual cumulative work is317099
segments,230634 months,31143 rule years,125602 checks,470552 offsets and125602
completed label attempts. Every Step delta and pending output is verified.

Final functional source maps agree on2521 inputs; all receipts pass on that
captured identity. Prototype logs also pass. After functional, size and old-profile
measurements terminate, only the performance JSON changes to append5 new profiles;
original defaults/profiles/benchmarks remain intact. Actual loader validation and
new performance runs use the separately captured measurement identity. No samples,
failed gates, old baselines or benchmark fingerprints are rewritten/force paired.

Size/forbidden-symbol checks pass. Existing delivery single/static/shared text,
data, BSS and file sizes have ZERO growth on both OSes. Occurrence static owned
closure text is59852 Windows/64412 WSL bytes, including its8 xcron sources/dependencies;
single text103732/111477 includes selected Core. Shared owned text69765/69882 uses
separate companion Core. These are not equivalent total deployment sizes.
The original3 cursor benchmark fingerprints/environments match naturally. Both
comparisons exit1 on original noise gates (8 Windows/17 WSL violations). Separate
raw-median threshold diagnostics have no Windows failures and retain3 WSL tail
breaches: offset1 P99.9 and sync P99/P99.9. No qualified no-regression follows.

Five new whole32-query batch profiles retain independent Gregorian/nominal-metadata
checksums. Typical LAST sync/cursor1 work counts agree:3 segments,3 months,5 rule
years,2 checks,4 offset probes and2 complete labels/query; cursor1 uses7 Steps.
The public-API LAST+SKIP composition is a same-fixture result comparator, with its
internal traversal counters unavailable (zero facts mean uninstrumented). Error
profiles include diagnostic allocation/clear. Throughput medians for last_sync,
last_cursor1,gap_reject,cycle_reject_cursor16,public_composition are respectively
Windows937969/660353/660556/3877/716859 and WSL783868/575890/631109/3559/710060
queries/s. Whole-batch P99 is57.3/81.1/81.3/12089.2/72.3us and
71.6/92.8/84.1/13266.099/78.1us. Both new reports exit1 on unchanged noise gates
(17/10 violations). No speedup/production-tail or divided per-query percentile
claim is made. The100-batch cycle profile has coarse tail resolution; worker/
arrival/CPU/resource/durable paths and native mixed-load acceptance remain open.

The measured always-gap REJECT path is a concrete performance limitation: current
sequential composition first proves actual absence, then discovers an early missing
label. Each query consumes803 segments,5601 months,2004 rule years and351 Steps at
budget16 before returning MISSING. Prioritize fusing ordered segment search with
gap validation so the early boundary can decide before a full cycle, and retain
this exact workload/evidence. Dense rejected folds also remain an optimization
concern. These are open performance requirements, not accepted final behavior.

Portable evidence: extlibs/xcron/tests/results/occurrence-selection-20261108.json.
Raw source maps, commands, all logs/receipts, oracle/probe/layout/consumer identities
and original/new performance samples remain in out/service-extensions/xcron-occurrence/.
Continue performance optimization, explicit nominal policy integration into jobs,
gap compensation, physical delivery/durable recovery and the remaining Redis/RPC/
MQTT scope. Native Linux,>=72h and production stability/performance gates stay
required. All four products developing; overall goal active.


### Mainline migration and fresh four-product verification

At the user's explicit request, moved all26 pending service-extension source/
documentation/generated files from the c2a8b434 worktree into D:/GIT/xrt master,
retaining the already-integrated916f3197 history. Every overwritten destination
was first verified to have the same base blob; all transferred source hashes
matched. No development branch or merge commit was created for this migration.
Updated the versioned workflow and local AGENTS guidance to default to direct
master development, preserving concurrent work and requiring a concrete reason
for branch/worktree exceptions. Remote publishing remains explicitly authorized
separately; no remote push was performed.

Copied and hash-verified8425 existing regular evidence files (1619572098 bytes),
including raw failures, probes, frozen data and performance reports. Preserved37
cache junctions whose targets were already absent before migration; no new target
loss was introduced. Additionally retained297 surviving performance executables/
size artifacts (93763688 bytes), with current hashes and provenance. Rebuildable
object/test caches are not needed to retain the source/evidence history. All
migration manifests and raw receipts are in out/service-extensions/mainline-migration-20261108/.

Fresh tests execute on actual master Core/tools at D:/GIT/xrt, not the old worktree.
Windows and WSL each pass101 root modular/single programs: xredis30, xrpc30,
xmqtt14, xcron27. WSL suites use O2 ASan/UBSan/leak-halt. All4 root static/shared
package consumers and the isolated occurrence static/shared consumers pass on
both platforms. The runnable occurrence example, direct/generated layouts and
new422-zone oracle also pass; both oracle reports match the pre-migration
expected results and every Step budget/metadata field. Two O1 TSan occurrence
units pass. Windows86 tooling tests and all4 products' feature/single/API/
docs checks pass. Existing Windows symbolic-link fixture skips1314 remain recorded.

Both final mainline execution maps agree on2891 source/config/tool inputs.
The feature and migration evidence JSONs record the initial full-suite identity.
Final review adds only a package-consumer short-circuit guard, avoiding an
uninitialized Selected read after a preceding failure. Both platforms root
static/shared xcron consumers pass again; all other command/log hashes and absence
of that consumer compilation are verified before reuse. Exact initial/final maps,
the sole consumer delta and all four rerun receipts remain. Production C/H and
other test drivers are unchanged. Historical
performance reports remain labelled c2-based pre-migration measurements, with
all failed noise gates preserved. They are not relabelled as performance results
for the integrated master Core. The sequential always-gap rejection performance
problem, job integration/compensation/durability, remaining Redis/RPC/MQTT scope,
native Linux and>=72h production acceptance remain open. The original long-term
goal remains paused; this user request performs migration/verification only.

Portable feature evidence: extlibs/xcron/tests/results/occurrence-selection-20261108.json.
Portable migration evidence: docs/development/service-extensions-mainline-20261108.json.


### 2026-11-09: retain implementation work as per-sample observations

Added optional performance observations, separate from strict workload facts and
gated metrics. Declared fields must exist on every execution and cannot duplicate
or overlap facts/metrics. Every measured sample retains its raw string dictionary,
including exact integers beyond double precision; differing observation values do
not bypass unchanged workload, environment/fingerprint, metric/noise contracts.
Existing profiles without observations retain their report structure/schema2.

The five occurrence profiles move only Step/segment/month/rule-year/check/offset/
label work counts into observations. Batch size, kind, budget, query count and
complete result-metadata checksum stay strict facts. Benchmark source/timed loops,
arguments/defaults/5% throughput/10% latency and MAD3%/central6% gates are unchanged.
The cycle description now names its workload and historical problem so it remains
valid after optimization. New configuration/runner fingerprints require fresh
sampling; older failed reports are retained and never force paired or rewritten.

Windows/WSL each pass23 performance/size tool tests, including real child-process
sampling, varying observations/large integers, missing fields, invalid/overlapping
declarations, strict changing facts and regression rejection. Actual Cron CLI
smoke preserves all counters separately from facts. Its initial checker incorrectly
expected5 samples; smoke correctly reports1 sample/0 warmups. Initial reports,
checker and failures remain; the checker now uses the reported repeat count and
both corrected CLI runs pass. Smoke is not a qualified performance baseline.

Evidence: docs/development/service-extensions-work-observations-20261109.json.
Raw artifacts: out/service-extensions/xcron-ordered-occurrence-20261109/.
Continue fresh mainline before/after measurement and ordered actual/gap traversal;
full four-product scope/performance/native/>=72h acceptance stays open. Goal active.

### 2026-11-09: ordered recurring actual/gap traversal on master

Replaced the provisional-actual-then-gap composition with one ordered UTC walk.
Every gap is resolved before actual labels in its following segment; FIRST/LAST
filtering reuses the calendar and transition state within that segment. Complete
global candidate/UNKNOWN resolution is retained. Actual absence may use its safe
earlier periodic proof only before any actual label matches; selected absence
keeps the maximum legal offset margin. Missing REJECT still walks gaps/mapping
after actual absence. Public ABI remains occurrence64/budget20/issue128/cursor1024;
x64 private value/control shrink880/904 to792/816. Private cursor magic is revised.

New regressions require the earliest yearly missing gap to do the same work with
a short or INT64_MAX horizon, dense120-label fold rejection to share exactly two
segments while resolving121 labels, and the synthetic exclusively owned general
POSIX fallback to yield each of403 actual rule years. The synthetic fixture is
never used in performance measurements. Existing ownership, independent budgets,
pending atomicity, optional Issue/OOM and native handoff tests remain.

Windows and WSL each pass27 root modular/single programs, the runnable example,
root and isolated occurrence static/shared consumers, and direct/generated ABI
checks. WSL suites run O2 ASan/UBSan/leak-halt; both O1 TSan occurrence programs
pass. Windows90 tool tests and feature/single/API/docs checks pass. Existing
Windows symbolic-link fixture skips (system1314) are retained. Both independent
422-zone oracle reports match exactly:121664 queries,243328 synchronous calls,
349582 cursor Steps and714574 API calls. Expected found/none/ambiguous/missing/
unknown outcomes match the earlier implementation; ordered work reduces segment
visits317099 to173554 and Steps401818 to349582. Both backends use the same2526
frozen execution input hashes; production C/H were never edited or regenerated
while any of this stage's compiler/measurement pipelines could read them.

Fresh formal before/after measurements use the same five profiles, timed driver,
arguments, strict query/result-metadata facts, runner/config fingerprints and
original5% throughput/10% latency/MAD3%/central6% gates. All five measured samples
and executable hashes are retained per backend/phase. No baseline is rewritten
or force paired. Per query, the yearly always-gap REJECT workload now needs2
segments/4 months/2 rule years/1 Step at budget16, versus803/5601/2004/351. Ready
LAST keeps both global resolutions, sharing2 rather than3 segments. Ordinary gap
REJECT resolves1 label rather than computing a later unused actual result too.

Descriptive throughput medians before->after: Windows LAST sync948220->1100482,
cursor688030->754225, gap702712->1108057, yearly3949->943090 queries/s; WSL
831517->966950,581044->636061,642367->1145312,3503->827322 respectively. Public
composition controls change781419->752025 and733297->743446. These are NOT qualified
performance results: all four reports fail original noise gates. CLI noise
rejection precedes median comparison, so the unchanged strict comparator is also
applied to original reports without relaxing environment/fingerprint/facts or
thresholds. It exposes Windows LAST cursor batch p95 regression56.2->70.6us
(10% limit); WSL medians have no threshold failure. The Windows tail regression
remains unresolved; it is not dismissed as noise. The now-fast yearly sample is
short at unchanged query count and needs a separate longer controlled workload
for future qualification. No production tail/performance acceptance is claimed.

Both platforms' single/static/shared size reports and dependency-symbol checks
are retained. Source inputs, raw failures, scripts, before/after reports/binaries,
full oracle and validation receipts remain under
out/service-extensions/xcron-ordered-occurrence-20261109/.
Portable evidence: extlibs/xcron/tests/results/occurrence-ordered-20261109.json.
Evidence publication follows execution capture and changes no compiled input.
Remaining work includes investigating the Windows tail, controlled native
performance, job integration/compensation/durability, remaining Redis/RPC/MQTT
features/interop and>=72h acceptance. Goal active; all development stays on
D:/GIT/xrt master unless a concrete exception is needed.

### 2026-11-09: remove duplicate occurrence cursor initialization work

Investigated the earlier Windows LAST cursor batch p95 regression using preserved
pre-ordered and e507 executables, unchanged100000x32-query workload and complete
result-metadata facts. Each default/single-logical-CPU condition retains A/B warmup
and three ABBA cycles (six measured executions per version). Child affinity is
verified; only the owned helper/children are affected and the helper restores its
mask. System power policy is read only. Process user/kernel CPU and outer wall
time supplement the unchanged benchmark metrics. Original failed reports remain.

Same-window pre-ordered->e507 medians do not reproduce a threshold failure:
default throughput699365->781476 queries/s and p9560.45->55.7us; logical CPU2
707208->794859 and47.85->40.35us. Both conditions still fail the original noise
checks. This does NOT establish the cause or close the original56.2->70.6us
regression, and no causal link to the following cleanup is claimed.

GCC O2 Windows disassembly confirms CursorInit cleared its entire816-byte control,
then ValueInit cleared the same792-byte value again, then cleared1024 caller
bytes before overwriting816 of them. Init now clears only the control prefix,
lets ValueInit initialize its value once, copies that control and clears only
the unused public tail. Source-requested x64 zeroing drops2632->1024 bytes per
Init (1608 removed); the816-byte control copy and normal Step copies remain.
These extents do not measure cache/bus traffic and are not zero-overhead claims.
Windows and Linux before/after disassembly and exact executable hashes remain.
Public64/20/128/1024 and private792/816 layouts/magic are unchanged.

Added a regression comparing every cursor/Issue byte after Init over A5/5A caller
storage at the same address, for both pending and empty rounded windows. Both
execute correctly; exclusive reinitialization after terminal execution restores
all bytes. Windows/WSL modular and single occurrence tests, root+leaf static/shared
consumers, example and direct/generated layout checks pass. WSL uses O2 ASan/
UBSan/leak-halt; both O1 TSan programs pass. Both422-zone reports exactly match
e507 results, metadata and all traversal counters (121664 queries,349582 Steps).
Feature/single/API/docs checks pass; no unrelated tool or full-product suites
are rerun for this isolated Init change. Both backends share2527 frozen inputs;
all production/generated C/H stay unchanged during compiler and timing pipelines.

Initial scoped driver incorrectly combined mutually exclusive modular/single
selectors; both rejected before C compilation. Corrected to separate commands;
initial driver, logs, receipts and input maps remain. A supplementary environment
helper initially named a nonexistent private version function; original helper
and error note remain. It now uses the runner's release_identity.tool_version;
CPU/compiler/default affinity/power metadata match the original environments.

Fresh same-window e507->cleanup diagnostics retain the same ABBA scheme and all
warmup/raw samples. Descriptive medians: Windows default767429->787786 queries/s,
p9564.45->53us; logical CPU2 776502->790018 and55.65->48.65us; WSL680962->706540
and56.7->53.55us. No median crosses the original5% throughput/10% latency limits;
all conditions still fail original3% MAD/6% central-noise checks on some tails.
These supplementary diagnostics do not replace formal baselines, resolve the
earlier tail cause, or qualify native production performance. WSL is not native
Linux acceptance. No scheduler policy, callback/owner behavior or protocol scope
is changed. Next functional work remains selected occurrence job integration,
compensation/durability and remaining Redis/RPC/MQTT features; controlled native
performance and>=72h gates remain open. Goal active; work stays on master.

Portable evidence: extlibs/xcron/tests/results/occurrence-cursor-init-20261109.json.
Raw evidence: out/service-extensions/xcron-cursor-tail-20261109/.

### 2026-11-09: integrate selected recurrence policies into bounded job leases

Added the optional developing xcron_scheduler_selected mode and five exported
entry points for Create/Add/Poll/Job/Reap. Its implementation is guarded in the
existing scheduler source and shares slot/lease/heap ownership; the module does
not duplicate that source or introduce another runtime. Actual-only builds keep
their original calculation closure. Public existing layouts remain config24,
job config112/info96/run64/retired112 on verified x64. Baseline and current lean
private layouts agree (owner224, hot job240, query416); enabling the optional
capability adds16 owner bytes while preserving the hot slot/actual query. The
selected mode preallocates792 query bytes/job. At two jobs, extended actual mode
requests1584 bytes and selected mode2336; exact BytesLimit covers the complete
allocation. New run/poll/info/retired types are120/48/208/224 in modular and
generated declarations on both platforms. No zero-overhead claim is made.
Extended actual-mode admission/poll/report calls also validate the mode flag;
actual-only builds remove that check. Selected leases copy56 nominal bytes in
addition to the plain run. These costs are not hidden by the unchanged public ABI.

Add validates/snapshots/retains without recurrence traversal, admitting SEARCHING
or an empty EXHAUSTED job. Initial and future mapping/fold/missing faults become
typed job faults with full occurrence issues; admitted Data remains borrowed
through physical retirement. This explicit admission contract differs from the
unchanged synchronous actual Add. The actual Add/Poll/Job/Reap methods reject
selected instances, avoiding hidden unbudgeted label checks or truncated faults.
Complete/Cancel/Wake/Inspect/Close/Destroy share the existing owner/lease contract.

Poll bounds the sum of five traversal dimensions across all jobs, plus existing
WorkLimit/output limits. FIFO one-unit queries alternate with ready/heap work;
eager successors receive a quantum only without older queries. Capacity zero
advances calculations even at full lease/output capacity; zero search budgets
can publish a previously known lease and defer its successor. Each lease copies
canonical nominal metadata and complete multiplicity before successor mutation.
Faults preserve live leases through Cancel/Close until physical Complete/Reap.
Misfire/overlap discards intentionally do not enumerate interior labels; catch-up
retains their gap validation. Tests and documentation make that choice explicit.
Gap compensation remains separate. Existing executor submission accepts the
embedded run; owner-side nominal metadata is not silently passed to its callback.

Independent second-by-second modeling checks2048 complete job traces across32
arbitrary8-transition histories,8 query windows and all8 fold/missing policies.
It enumerates global candidate sets and independently derives Gregorian labels,
earliest gap/ambiguity anchors, every lease and final physical counters. Separate
tests cover each exhausted dimension, zero budgets/output, full-int64 endpoints,
future global UNKNOWN beyond a label's actual mapping, active-fault lifetime,
eight400-year searches versus a light job, sparse-query credit beside dense
catch-up, exact byte limits, invalid modes/policies/aliases, diagnostic OOM,
1024 warmed zero-requested-heap cycles/error identity, misfire/overlap/rollback,
retained versus coalesced gaps and serialized pending-query native handoff.

Windows/WSL each pass29 root modular/single programs, the isolated actual-mode
leaf, selected example, root and isolated selected static/shared consumers,
direct/generated and baseline/lean/extended layouts. Root consumers dispatch a
selected lease through an actual XRT worker/executor and physically collect it.
WSL root/leaf suites run O2 ASan/UBSan/leak-halt; both selected O1 TSan programs
pass. The unchanged422-zone occurrence oracle matches prior complete metadata
and counters (121664 queries,349582 Steps). Windows90 tool tests and features/
single/API/docs checks pass. Existing Windows symlink fixture skips1314 remain.

Initial prototypes retain two test-format warnings and one incorrect test
expectation: misfire SKIP and overlap SKIP each discarded a range, while that
test expected one total. Correcting the independent sum did not change production
logic. Subsequent Windows/WSL prototypes and full validation pass. All failed
logs remain. After both full pipelines ended, only the previously unexecuted
benchmark validation/cleanup and one example-document registration were finalized.
Every compiled implementation/header/test input stays identical; the exact two
input deltas and absence of that driver from validation compiler commands are
verified. Generated outputs remain current. Final measurement snapshots include
the completed driver and documentation registration. No production/generated
C/H was edited while a compiler or measurement pipeline could read it.

First unpinned five-repeat profiles retain full requested metadata validation,
admission, query advancement, physical completion and timer costs. Independent
Gregorian arithmetic verifies their complete run/nominal checksums. Source work
observations are deterministic: selected1 needs200001 segment/month/check/offset
visits and400000 owner actions for200000 leases; selected16 needs3200016 visits
and6400016 actions for3200000 leases, with zero actual rule years in fixed UTC.
Original5%/10% regression settings and MAD3%/central6% noise gates are unchanged.
Windows medians are4438428/6302071 selected dispatches/s for1/16 jobs, batch
p990.3/4.2us; WSL4032160/5293084 and0.4/4.9us. Actual-only controls are recorded
separately with their different semantics, not used as a selected speedup claim.
Both reports retain failed noise gates and remain unqualified component baselines;
no historical fingerprint/report is rewritten or force paired. All eight actual
performance executables are retained with hashes. Both platforms single/static/
shared size and no-file/network/executor/service symbol checks pass.

Evidence: extlibs/xcron/tests/results/selected-scheduler-20261109.json.
Raw inputs, prototypes, complete validation, finalization proof, measured samples,
binaries and sizes: out/service-extensions/xcron-selected-scheduler-20261109/.
Live-service selected commands/responses and worker nominal metadata, compensation,
durable recovery, remaining Redis/RPC/MQTT5.0 scope/interop, controlled native
Windows/Linux performance (including the earlier Windows p95 cause) and>=72h
mixed-load acceptance remain open. Goal active; development stays on master.

### 2026-11-09: preserve selected metadata through real worker lifetimes

Added the optional xcron_executor_selected leaf and four actual exported APIs
in the existing executor implementation: CreateSelected, SubmitSelected,
DrainSelected and ExecutionSelected. It retains the existing config/callback
signature and borrowed XRT pool. Submission copies the complete ordinary lease
and nominal Local/global Candidates before dispatch; the callback can request
an immutable borrowed view only during Proc. Drain returns a complete copied
selected receipt and transfers its owned worker Error, including when native
queued cancellation skipped Proc. Caller submission storage may be reused at
return; copied receipts survive slot reuse. The original physical Complete/Reap
and borrowed Data duties are unchanged. Plain/selected Submit/Drain reject mode
mismatches atomically; shared owner/cancel/close/inspect/destroy behavior remains.

The existing worker dispatch and publication/Notify-tail bodies match their
committed source byte-for-byte after newline normalization. No new runtime,
worker, registry or per-execution allocation is introduced. One bounded arena
preallocates selected labels. Verified x64 old public config/completion/stats/run
stay56/80/64/64 bytes; actual-only private owner stays176 and hot slot104/token
offset88 stay unchanged. Enabling the optional capability adds an8-byte owner
pointer, including plain instances in that build; selected mode adds56 bytes per
slot. Two-slot extended plain/selected allocations request408/520 bytes. Submit
and Drain each copy56 additional nominal bytes, mode checks and an optional
getter pointer lookup remain real costs. Direct/generated new label/completion
sizes agree at56/136. No zero-overhead claim is made.

Windows/WSL each pass31 root modular/single programs, the isolated actual-only
leaf, the new offline example, root and isolated selected static/shared consumers
and baseline/lean/extended/declaration layout checks. Root consumers use actual
selected worker callbacks and full receipts. WSL root/actual-leaf suites run O2
ASan/UBSan/leak-halt; both selected O1 TSan programs pass. Existing2048 independent
selected job traces and the complete422-zone oracle remain exact (121664 queries,
349582 Steps,714574 API calls and every prior counter/metadata field). Windows90
tool tests and features/single/API/docs checks pass; existing Windows native
symbolic-link permission skips1314 remain recorded, not disabled.

Selected tests cover16384 concurrent exact-once metadata receipts per program,
poisoned submission storage and blocked immutable views, worker failure/error
ownership and invalid results, late cancellation, close with active Data, queued
native cancellation without Proc, duplicate/capacity/rejection rollback, typed
diagnostic OOM, owner conflict/foreign controller/Notify reentry, sole-slot reuse
while an old Notify blocks and producer-tail BUSY destruction. Real repeated-label
scheduler jobs retain owned Data through both actual workers and physical
Complete/Reap. Exact byte caps, aliases/overflow/factory OOM, ambient error identity
and4096 warmed no-requested-heap cycles cover bounded resources. The initial new
test-format warning is retained in prototype-windows-2.log; fixing its newline
disabled no warning and later prototypes/full validation pass.

All2542 code/config/tool input hashes agree across both full validation and both
final measurement maps. No production/generated C/H was edited or regenerated
while a compiler or measurement pipeline could read it. Only this stage's final
evidence and documentation are published after all those child processes ended.
All failed logs, raw samples, source witnesses, complete validation receipts and
four actual measured executables with hashes remain in the raw stage directory.

First unpinned five-repeat component profiles include64 synthetic leases/batch,
real XRT workers, immutable view validation, two shared callback atomics, complete
owner payload/exact-once validation and receipt collection/timer/polling costs.
Independent Gregorian arithmetic checks every measured run's full checksum and
64000 callbacks. Original MAD3%/central6%,5% throughput/10% latency settings and
one warmup remain unchanged; no historical baseline or fingerprint is rewritten.
Windows1/4-worker medians are1971529.877/808674.040 executions/s; WSL medians are
2023408.369/170236.269. Both reports fail noise gates. Four workers are materially
slower than one in both environments; the cause is not isolated and is an explicit
performance investigation item, not explained away as noise or attributed to a
specific layer. These are unqualified component baselines, not fixed-arrival or
live-service latency, plain-adapter speedups, linear scaling or native Linux
qualification. Both platforms' actual/selected single/static/shared size and
forbidden-symbol checks pass.

Evidence: extlibs/xcron/tests/results/selected-executor-20261109.json.
Raw data: out/service-extensions/xcron-selected-executor-20261109/.
Selected live-service commands/responses, compensation, durable recovery,
remaining Redis/RPC/MQTT5.0 scope/interop, the selected four-worker throughput
cause, earlier Windows occurrence p95 investigation, qualified native performance
and>=72h mixed-load acceptance remain open. Goal active; development stays on master.

### 2026-11-09: bounded atomic executor batches without another runtime

Investigated the fine-grained selected four-worker throughput drop on frozen
de0255ba sources before choosing a change. Per-thread linker-wrapped mutex probes
show4 ownership mutex acquisitions per scalar job, while SleepLock acquisitions
and aggregate wait rise substantially at four workers. Probe timers/bookkeeping
extend acquired lock holds, so trace throughput and wait percentages are not
release measurements. Frozen ABBA queued-zero wake guards do not give a stable
four-worker gain. Bounded32/128-pause worker probes worsen Windows four/eight-worker
throughput and CPU; GetProcessTimes has coarse quantization. They are rejected;
WSL spin probes are not run after that rejection. Neither experiment changes
canonical XRT. All sources, failed compiler logs, samples and actual executables
remain in out/service-extensions/xcron-executor-contention-20261109/.

Added real xcronExecutorSubmitBatch and SubmitBatchSelected APIs, bounded to1..64
already-ready leases. The caller does not wait to fill a batch. All fields,
spans/modes and owned-input aliases are checked before reserving slots/hash IDs;
one existing public XRT SubmitBatch call accepts the complete descriptor array
or rejects it. A duplicate at any prefix or native failure rolls every new slot
and hash entry back while retaining older leases. Failure transfers no physical
duties; the caller may retry, scalar-submit or Complete all rejected leases.
Native atomic capacity is one worker queue, even when separate scalar calls
could use aggregate free space. This distinction is documented and tested.

Each accepted lease retains its individual token, result, owned Error and full
physical receipt, including native queued cancellation without Proc. Complete/
Reap/Data lifetimes and owner/reentry/Notify-tail rules remain. Counters count
leases rather than calls; native AGAIN remains typed even when diagnostic OOM
sets the ambient error to MEMORY. The entire ordinary/selected payload is copied
before any native dispatch. Caller arrays may be reused at return. Input arrays
are additionally required to be disjoint from the adapter arena.

No extra retained storage or public/private layout is added. Successful batch
preparation requests no heap; callback/diagnostic/native cold costs remain. A maximum
of64 native descriptors uses2048 x64 stack bytes, plus ordinary locals/callee
frames. Pre-publication reservation links reuse slot Next fields, saving an
extra index scratch array. They are traversed only on rejection, where native
acceptance guarantees no callback. After success no submit traversal touches
them, so Drop may publish/overwrite them immediately. Removing the two added
batch blocks leaves the old executor source byte-for-byte after newline
normalization: all scalar/create/drain/worker/Notify/lifetime code is preserved.
Canonical Core, Core single headers/tools and the original scalar benchmark
remain unchanged. Direct/generated old layouts and byte limits still agree.

Windows/WSL each pass31 root modular/single programs, actual-only leaf tests,
old/new runnable examples, root and isolated batch static/shared consumers,
baseline/lean/extended/declaration layouts and the unchanged422-zone oracle
(121664 queries,349582 Steps,714574 API calls and all prior metadata/counters).
WSL root/lean suites run O2 ASan/UBSan/leak-halt; four O1 TSan programs cover
plain/selected modular and single-header forms. Windows90 tooling tests and
features/single/API/docs checks pass. Existing Windows symbolic-link permission
skips1314 remain recorded. All2546 frozen code/config/tool input hashes match
both validation and both final measurement maps. No production/generated C/H or
config is edited while either pipeline can read it; publication is docs/evidence.

New tests collect40910 plain and36936 selected batch receipts per program in
addition to existing suites. They cover every internal duplicate position1..63,
older scalar duplicate positions0..62, all IDs/capacity reused after rollback,
count/mode/last-field/alias/foreign-owner/Notify-reentry errors, full native
queues after live-job credit reservation, native one-queue limits, typed OOM,
independent errors/results, poisoned inputs and cancellation of64 leases across
running and queued tasks. Both slots are reused by a new batch while both old
Notify tails still block. Each mode collects32768 warmed maximum-batch receipts
with unchanged requested-allocation counters and ambient error identity. The
new offline example resolves both New York01:30 occurrences, poisons submitted
input and physically completes/reaps both leases before releasing resources.

Retained initial failures are diagnostic consecutive-if formatting, a GCC
maybe-uninitialized warning for the first index-array rollback design, and a
new test incorrectly expecting typed MEMORY instead of retained AGAIN during
diagnostic OOM. The scratch array is replaced by private Next links; the test
expectation is corrected without changing runtime error behavior. No warning is
disabled. Repeated --test options initially select only the final test, so that
run is not claimed as both tests; final full root/lean suites cover every case.

On both environments, deterministic ownership mutex counts for64000 validated
jobs are256019/256025 (one/four-worker scalar) and67019/67025 (64-item batch).
This is one native reference release per job plus three admission locks per
batch, with unchanged cold lifecycle counts; it is not a zero-overhead claim.
Sleep-lock counts also fall in the instrumented traces, but depend on scheduling.
All12 formal scalar/ready-scalar/ready-batch executables are retained with hashes,
plus all diagnostic executables. Original scalar workloads/reports are intact.

First ready-array profiles construct64 available leases before either API path,
retain all nominal/run-field/exact-once checks and both shared callback atomics,
and include physical receipt collection, owner polling/timers and array work.
Submission width1/64 is an explicit fact, not a forced same-workload regression
pair. Independent Gregorian arithmetic verifies64000 callbacks and checksum
12262312746650400000 for every measured process. Original five repeats/one
warmup/MAD3%/central6% and5% throughput/10% latency settings remain unchanged.
Windows ready scalar1/4 medians are1912777.353/846179.171 executions/s, batch1/4
3425244.048/2440047.276. WSL scalar1/4 medians are1842903.850/164977.992, batch1/4
2438615.290/1457184.847. Both complete reports retain noise-gate failures. These
are unqualified component observations, not native Linux, fixed-arrival service,
linear scaling or production acceptance. Arbitrary scalar fine-grained callback
scaling and validation-counter contention are not declared resolved. Both
platforms' actual/selected single/static/shared size/forbidden-symbol checks pass.

Evidence: extlibs/xcron/tests/results/executor-batches-20261109.json.
Selected live-service commands/responses and appropriate caller batching,
compensation, durable recovery, remaining Redis/RPC/MQTT5.0 scope/interop, the
earlier occurrence p95 investigation, qualified native performance and>=72h gates
remain open. The full goal remains active; development stays on master.


## 2026-11-09 - selected policies through the live service owner

Developed directly on D:/GIT/xrt master from5f22bed0; no branch/worktree created
and no push. The optional xcron_service_selected leaf adds six typed APIs for
creation, async selected admission, complete collection, nominal completion,
full stopped retirement and selected stats. It reuses the existing service
owner/event waiter/tickets/reserved outputs/individual retry heap and the existing
selected kernel/borrowed executor; no Core/runtime edits. Scalar width1 or bounded
width1..64 groups only already-ready initial leases, without a fill wait. Each
native-rejected lease counts its own attempt and retries individually under its
original deadline/cap. A group must fit one native worker queue.

Queue acceptance, cheap selected job admission, physical execution and retirement
remain distinct. Initial/future recurrence faults belong to the admitted job's
full Info/retired LastIssue; they retain original Data through physical retirement.
Five global query budgets apply per owner turn even with output credit full.
Nominal Local/full Candidates survive retries, unsent expiry/cancel, native queued
cancel without Proc, running bodies, error results and metadata drain after Data
has been reaped/freed. Ordinary output APIs reject selected instances; shared
control/lifecycle requests work in both modes. New example and root/isolated
static/shared consumers execute real selected jobs and consume all duties.

Windows and WSL each pass all33 root xcron/event-waiter programs, ordinary service
leaf tests, examples, root/isolated static/shared consumers and exact prior422-zone
2026e oracle totals. WSL root/ordinary tests use ASAN/UBSAN with leak-halt; four
plain/selected modular/single TSan invocations pass. Windows90 tool tests,
feature/single/reference/docs checks and both platforms' actual/selected
single/static/shared size/forbidden-symbol checks pass. The first Windows root
attempt exposed an old competing-collector test's potentially uninitialized
Issue read; initialized that test variable, retained failed/interrupted logs and
reran both final suites. No warning was disabled to suppress that finding.

New tests cover all8 fold/missing policy combinations, global first/last window
semantics, initial fold/missing faults and future UNKNOWN beside an active lease, copied queued
policies/config/zone, typed errors and exactly-once Data duties, retries/deadlines,
queued/running cancellation, output credits and owner/pool self-waits, clock faults,
512 parallel Info replies and competing retirement collectors, startup allocation
prefixes/exact byte boundaries, full-input/output aliases and2048 warmed
successful cycles with flat global malloc/calloc/realloc/block request counts.
An actual clock-turn barrier proves five-budget fair calculation while output is
full. Existing privileged Windows symlink subtests still report1314 skips.

Old public config/reply/completion/stats remain136/296/80/384 bytes; selected
144/520/136/448, matching direct/declaration headers. The old440-byte command,
184-byte reservation and checked offsets remain; a visible C11 union snapshots
nominal metadata before activating Done. Extended service owner adds8 bytes.
Selected arena adds120 state+232*commands+56*completion credits+112*BatchLimit
plus alignment; typed scratch replaces original buffers. Kernel selected queries
add376/job and adapter labels56/live lease. Extended dependency owners carry the
existing16/8-byte kernel/adapter additions. Measured2/2/3/2/2 instance values and
mode/copy/error-reference costs are documented; no zero-overhead claim.

Final functional and formal measurement maps match on all2556 inputs;51 frozen
relevant source/config/header copies and all8 formal binaries/hashes are retained.
Each measured process validates64000 callbacks/checksum2048032000. The first
selected live-component profiles include64-due controlled-clock bursts, complete
recurrence/owner/worker/nominal/exactly-once checks, two shared callback atomics
and physical receipts. Width1/64 is an explicit workload fact. Windows scalar1/4
medians897564.095/581979.549 executions/s; batch1/4 1004802.643/993801.165. WSL
scalar1/4 793267.558/182935.117; batch1/4 799676.121/593623.188. Original5 repeats,
1 warmup, MAD3%/central6% and5%/10% limits remain unchanged. Reports retain12/16
failed noise checks. Separate preserved-binary two-round checks confirm native
batch mode has2 groups/128 leases and scalar0. These are unqualified synthetic
component observations, not fixed-arrival/native/scaling/production acceptance.

Evidence: extlibs/xcron/tests/results/service-selected-20261109.json.
Compensation, durable recovery, complete Redis/RPC/MQTT5.0 scope/interop, the
four-worker scalar scaling and earlier occurrence p95 investigations, qualified
native Windows/Linux real-arrival performance and>=72h release gates remain open.
The overall goal stays active; subsequent work continues on master by default.


## 2026-11-09 - real selected-service fixed arrivals and complete event accounting

Worked directly on master fromdf107b8c, preserving every prior source/driver and
profile. Added a separate C fixed-arrival workload for actual default service
clocks:4 immutable UTC second boundaries x256 pre-admitted jobs,1/2/4/8 workers
and scalar/batch64 initial admission, plus an explicit2200ms full-output consumer
pause. A missed admission deadline invalidates the process; it is never moved
forward. CATCH_UP+WAIT preserve all backlog. Each native process requires1024
unique callback/physical-receipt pairs, full nominal/global Candidates and every
Run/payload/observation field, checksum524800, four successes per original Data
retirement and complete physical owner/pool joins. No skip/coalesce/clock
regression, rejected admission/retry or sticky service failure is accepted.

New profiles retain P50/P95/P99/P99.9/max for real scheduled-to-worker start,
monotonic owner-to-worker delay and scheduled-to-Drain-return latency, including
consumer polling/queue waits and fixture costs. The controller'sxrtSleep(1),
per-event callback claim, shared callback counter, two nominal checks, opaque ID
lookup and full record validation remain counted. CPU/requested heap/component
and fixture bytes, native batch/backend and actual consumer pause observations
are separate from stable workload facts. Native CPU instrumentation is restricted
to the benchmark; no production runtime/Core/public API/generated header changed.
CPU/heap windows exclude creation/admission/shutdown, CRT/OS/borrowed native
storage and can miss initial work if the first deadline crosses between separate
pre-window checks/snapshots. Complete cold-path accounting therefore remains
open; no intentional in-process first-callback warmup is performed.

Windows modular, memory-stat-disabled and single forms each pass both64-event
scalar and96-event three-period stalled-consumer cases. WSL modular/single
ASAN+UBSAN/leak-halt and modular/single TSan pass both cases; invalid argument
sets return the expected failure. Windows90 tool tests, feature/single/API/docs
checks pass. This stage changes benchmark/config/documentation assets, so existing
unchanged production suites/packages were not relabelled as rerun.

Final functional/formal input maps are identical on2558 source/config/tool files.
Seven diagnostic and eighteen actual formal binaries plus source copies/hashes
are retained. Per platform nine profiles have one warmup/five formal runs; every
one of54 native process stdout records is preserved and independently matched
against every stored sample/fact/observation. Original MAD3%/central6% noise and
5% throughput/10% latency regression settings remain. Reports keep79 Windows and
62 WSL failed noise checks. No favorable retry/filter or forced comparison.

Windows normal start P99 medians are around1ms, receipt P99 mostly16ms; WSL normal
receipt P99 is1..4ms. Slow-profile start/receipt P99 medians are1235/2219ms and
1203/2201ms respectively; all1024 expected events survive. The observed consumer
polling gap needs isolated readiness-wait measurement. Windows process CPU
increments include many0 and15.625ms values, which are coarse observations, not
zero-cost claims. Both environments observe zero XRT heap request increments
inside their counter windows; this is not a whole-lifecycle/cold-path claim.

Evidence: extlibs/xcron/tests/results/service-arrivals-20261109.json.
The first prototype omitted memory_stats.h and failed compilation; explicit
include corrected, no warning suppressed. An out-only orchestration bracket error
failed before any native execution; source/log retained before the first complete
formal collection. Functional source was frozen throughout both formal pipelines.
Next: bounded efficient completion-readiness wait/isolation of polling delay and
strict initial counter-window alignment, then remaining compensation/recovery,
Redis/RPC/MQTT5.0 scope/interop and qualified native performance/>=72h gates.
The full goal remains active; default development stays on master.


## 2026-11-09 - bounded completion readiness and matched consumer latency

Developed directly on master from62d41646. Added xcronServiceWaitReadable for
ordinary/selected modes using the existing mutex/condition. It is nonconsuming:
no record, reserved credit, owned Error or Data duty transfers. One finite total
monotonic timeout survives unrelated/spurious wakeups; ready wins over timeout
or terminal failure. Stopped-empty reports CLOSED or exact sticky Code/Cause and
an owned original diagnostic reference (possibly NULL), never caller ambient as
another task's cause. Pending positive owner/ANY borrowed-pool waits reject
THREAD; poll/already-ready observations are allowed. Closing alone is not EOF.
All callers finish before caller-exclusive Destroy.

The existing owner only broadcasts on empty->nonempty publication when actual
readers register. Native notification failure preserves the physical record and
starts controlled sticky SYSTEM shutdown. Wait errors unregister, counters reject
saturation, bounded condition chunks preserve total timeout. No runtime/queue/
per-wait allocation or new retained bytes. The4-byte counter uses existing private
padding at Windows820/Linux1100, immediately before Now824/1104. Old public sizes,
owner880/1160 or extended888/1168, command440, reservation184 and checked offsets
remain. Normalized source checks prove only the counter, new API and Finish
publication changed in the old service owner. Core/other runtime sources and all
prior benchmark drivers/profiles remain unchanged.

Both full xcron/event-waiter suites pass34 programs; plain/selected modular/single
ASAN/UBSAN/leak-halt and TSan checks pass. New deterministic single diagnostics
observe actual blocked registrations, prove one broadcast wakes both and no
nonempty/no-reader broadcast, inject notification/wait failure, saturate the
counter, exercise diagnostic OOM and a missing fatal diagnostic. Shared tests
cover physical/unsent cancellation, Error/Data/credit ownership, EOF/fatal,
ready-before-self-wait,512 competing receipts, total timeout under unrelated and
spurious wakes and512 warmed cycles with flat requested heap counts. Ordinary/
selected examples and root plus isolated static/shared consumers exercise the
new export. Both oracle results exactly match the prior422-zone2026e evidence;
Windows90 tool and generated/docs checks pass. Both leaf single/static/shared
size/forbidden-symbol checks pass.

An error-identity regression first reproduced stale caller error wrapping. The
fix installs the service's owned original diagnostic reference; the red/fixed
preserved-source replay is retained on both OSes. Earlier interrupted validation
is preserved, not accepted. A final test-only refinement retains caller Error
references through comparison; original/new test snapshots and source maps are
retained and all five affected programs rerun (native, ASAN/UBSAN/leak, TSan).
All production/config/generated inputs remain identical across full validation,
refined checks and final measurements; final2562-input maps match. Original and
controlled replay artifacts are distinguished. A replay helper initially checked
stdout rather than testRequire's stderr; helper log/source/binary retained and
only that helper was rerun after correction, without masking the expected red.

Six new matched-source fixed-arrival profiles preserve1024 expected callback/
nominal/physical-receipt pairs and checksum524800 per process. They add an explicit
poll/readiness policy and final positive first-arrival margin after CPU/heap
snapshots, preserving the earlier polling driver unchanged. Both platforms retain
all36 actual warmup/formal outputs and six executable hashes. Windows four-worker
poll/readiness receipt P99 medians15/1ms; WSL2/1ms. Readiness1/2/4/8-worker receipt
P99 medians are1ms in both reports. Slow-consumer start/receipt P99 medians are
1221/2221ms and1201/2201ms; no delayed arrivals are omitted. Original5 repeats,
1 warmup, MAD3%/central6% and5%/10% regression settings remain; reports retain39/31
failed latency noise checks. Zero latency samples are sub-resolution observations,
not zero time. CPU/native/borrowed/bootstrap/copy/synchronization costs retain
their explicit windows and limits; no zero-overhead claim.

Existing no-readiness throughput driver/profiles were also rerun unchanged:
Windows scalar1/4 952210.915/588446.226 and batch1/4 1036442.282/988661.291 exec/s;
WSL787753.360/164600.964 and830591.710/591651.423. Each platform retains24 native
outputs/four binaries, including6/14 failed noise checks. WSL scalar4 remains
unstable, so separately compiled immutable before/current service objects use the
same unchanged driver in one warmup plus five ABBA cycles (24 native outputs).
Medians193144.472/188165.5225 exec/s differ-2.6%; relative MAD7.08%/3.61%, central
range13.75%/7.22%, both above original thresholds. This is insufficient for a
qualified regression or no-overhead conclusion; all samples/both binaries remain.

Evidence: extlibs/xcron/tests/results/service-readable-20261109.json.
No performance/native Linux/endurance gate is declared complete. Residual scalar
scaling/occurrence p95, durable recovery/compensation, complete Redis/RPC/MQTT5.0
scope/interop and>=72h native mixed-load release gates remain in the full active
goal. Default development continues directly on master.


## 2026-11-09 - bounded MQTT5 enhanced authentication on master

Developed directly in D:/GIT/xrt on master from4618fde3; no new branch/worktree.
Added optional xmqtt_client_auth on the existing TCP client: initial AUTH rounds,
application verification of final CONNACK before Ready/application sends/TakeMessage,
bounded early-publication quarantine, and client-initiated reauthentication with
an independently owned typed result. Ordinary traffic continues during reauth.
Unique immutable quota-charged tickets pin Client until release; retained messages
and results independently outlive Client/Engine. One mailbox/output/exchange,
finite whole/local deadlines, round/byte/private/object bounds, rollback/OOM,
partial-frame arbitration and physical wire erasure are explicit contracts.

Initial Keep Alive continues across delayed offloaded challenges; pre-CONNACK
PINGRESP is rejected, its response budget starts at successful CONNACK and the
server override uses last control-output time. Final-proof work does not suspend
heartbeats. Deterministic red/green cases preserve client-legal diagnostic80, stale
reauth cancellation, late local decisions with a delayed timer owner, and the
completion/admission race. Old result/producer detachment now reserves the old
exchange so another admission cannot overwrite it. All failed prototypes remain.

Windows and WSL root MQTT suites pass18 programs each, plus base-client/auth
example executions and23 independent literal-byte peer cases each. Six root/auth/
base static/shared consumers per OS pass; the four affected leaf consumers pass
again after measured artifacts are archived. WSL ASan/UBSan/leak-halt and four
TSan targets pass; Windows90 tooling and generated/features/API/docs checks pass.
The earlier xredis30/xrpc30/xcron34 results are reused with6147 non-MQTT input
hashes unchanged:112 combined root programs per OS, not112 newly rerun programs.
The current6258-input source map is frozen across validation and measurement;
pre-keepalive results and sources remain separately archived.

Old public layouts and base Client sizes remain unchanged (Windows1168/WSL1376).
Compiling auth adds one8-byte private pointer. Auth state144/ticket48/wire32;
method storage and wire snapshots allocate only for auth instances. Whole-process
XRT heap requests per fixture reauth are15/1766 requested bytes on Windows and
20.994/2309.664 on WSL, including the peer; pool hits count, not libc calls/RSS/native
allocation counts. Single/static/shared sizes and basic-leaf forbidden-symbol
checks pass, with measured binaries, archives and build logs retained by hash.

All short and long samples remain. Seven alternating300000-QoS1/window32 samples
per variant qualify throughput on both platforms: before/base/auth-compiled
medians396390.626/395258.427/397177.918 publishes/s on Windows and
121127.777/118451.183/121201.648 on WSL, within the original5% regression limit.
Ordinary P99 comparisons remain unqualified because baseline noise exceeds the
original3% MAD/6% central-range limits. Formal2000-reauth medians are11228.518/s
and132.4us P99 on Windows,4511.876/s and392.9us on WSL; the WSL P99 report retains
two noise failures. Five additional20000-exchange samples qualify throughput/P99
on both platforms:11121.238/s and147.2us,4588.724/s and351us. These unpinned
closed-loop fixture-token workloads exclude cryptography, external enhanced-auth
brokers and fixed-arrival overload; no zero-overhead or production-parity claim.

Evidence: extlibs/xmqtt/tests/results/client-auth-20261109.json. Strict release
completeness still fails for the four developing products; the retained audit
also records the xcron tzdb_file performance-profile gap. TLS/WebSocket, reconnect/
durable sessions, external auth algorithms/broker interop, remaining four-library
scope and qualified native Linux/>=72h mixed-load acceptance remain in the active
long-term goal. Future development defaults directly to master.


## 2026-11-09 - bounded MQTT hostname and prepared-address Dial

Developed directly on master from5cbf5aa8 with unchanged Core and shared tools.
Added optional xmqtt_client_dial: bounded counted hostname snapshots, explicit
Resolver, immutable prepared endpoints with preserved ports, family filtering,
at most64 endpoints/attempts and dual-stack Core candidate racing. The same
finite API-entry SetupTimeout covers DNS, candidates, CONNECT/CONNACK and enhanced
auth final proof; fallback never restarts it. Auth-composition factories retain
the existing ticket/proof and reauth semantics. Queued ordinary traffic keeps its
original deadlines and remains unsent before readiness. No new runtime or hot
transport vtable; old public config/info/results remain unchanged.

Cancellation/Close detaches this subscriber through Core while the Resolver may
still own a blocked shared query. Tests retire Client and Engine before releasing
that query, then join Resolver work. A separate blocked client notification proves
logical Ready cannot bypass actual callback retirement. First terminal reason,
late winner ownership, binding OOM, exact output preservation, endpoint/host/alias
bounds, constructor allocation budgets and retained results/causes are covered.

A deterministic EndRead-before-event-installation replay exposed a real gap: Ready
remained pending until setup timeout. The winner handoff now snapshots public
Stream stats and fails promptly with TRANSPORT; a final same-wrapper replay goes
from250ms pending to resolved TRANSPORT, and the adversarial single test covers it
on both OSes. An already-closed winner prefers its owned native cause over a
setter-state error. Earlier compiler-format failures and two incorrect test
assumptions (waiting for independent DNS work and65 deduplicated identical
endpoints) remain preserved with the corrected unique-port/ownership cases.

Final6267-input maps are frozen across validation and measurement. Each platform
passes21 root MQTT programs, plus the Dial unit/example,46 independent literal-byte
Auth scenarios across hostname/prepared factories, eight root/client/auth/Dial
static/shared consumers and four leaf consumers repeated after size artifacts are
archived. WSL ASan/UBSan/leak and three TSan targets pass; Windows90 shared-tool and
generated/API/docs checks pass. Executed test binaries are copied per target
before later sanitizer output can overwrite them. Pre-EOF validation/source maps
and as-found binaries remain explicitly separate.6147 non-MQTT inputs match the
original xredis30/xrpc30/xcron34 run, yielding115 combined root programs per OS;
94 are reused identical-source executions, not newly rerun programs.

Base Client sizes remain1168/1376 bytes on Windows/WSL; compiling Dial adds one
8-byte private pointer. Numeric connections allocate no Dial/host snapshot.
Hostname uses one bounded temporary NUL copy, then Core copy/canonicalization;
MaxPrivateBytes accounts for that local peak. Native Dial/Resolver/Stream storage
uses their own limits. Completed Dial control state remains retained to Destroy;
zero extra steady retained bytes are not claimed. Process-wide XRT loop-window
requested blocks/bytes for numeric/prepared/hostname are
22.165/26237.905,24.17/26802.2,26.175/26926.33 on Windows and
29.04/27564.16,31.065/28129.6,33.13/28259.37 on WSL. These include peer and pool
hits, exclude service setup/final teardown, and are not libc calls/RSS/kernel
allocations. Single/static/shared sizes and base forbidden-symbol checks pass.

Seven alternating300000-QoS1/window32 samples qualify throughput: before/base/
Dial-compiled medians394420.32/396412.101/402570.331 publishes/s on Windows and
121525.628/121827.754/122060.825 on WSL, within the original5% regression limit.
Ordinary P99 comparisons remain unqualified because baseline noise fails. Five
200-connection lifecycle samples per mode retain all formal data and6/9 failed
noise checks: numeric/prepared/hostname medians3711.525/3703.676/3671.874 cycles/s
and425.6/429.4/405.8us cycle P99 on Windows;3047.893/3279.296/3177.12 and
529.7/493.6/480.4us on WSL. Each cycle includes CONNECT, one QoS1, Client/peer
physical retirement and Future disposal. These unpinned closed-loop fixture
measurements do not establish external DNS/broker parity or fixed-arrival/native
production performance. No baseline/noise thresholds were forced or relaxed.

Evidence: extlibs/xmqtt/tests/results/client-dial-20261109.json. All four products
remain developing. Native Linux/>=72h, strict release completeness (including
the retained xcron tzdb_file profile gap), remaining Redis/RPC/Cron work and MQTT
TLS/WebSocket/session/expiry/reconnect/durable state remain in the active goal.


## 2026-11-09 - release completed Dial control references before readiness

Developed directly on master from1c62259c. The prior Dial stage measured and
documented one bounded completed controller retained until Client Destroy. A
worker-owned observation now reproduces that cost while the connection is live:
prepared/hostname constructors retain540/541 requested controller+host bytes in
the constructor allocation scope and24 selected-address bytes. Manual reference
release leaves QoS1 traffic valid, proving the controller is no longer needed.

Terminal Done now clears Client.Dial under its existing publication gate and
releases exactly the client's caller reference as its final action. Core keeps
actual callback/runtime/Resolver references through their tails. Pending dials
remain owned and cancellable; failed/cancelled outcomes keep their original cause
and shared DNS jobs remain independently Resolver-owned. Source normalization
proves every other client function is byte-identical; public/private layouts,
message hot paths and Core remain unchanged; no allocation, queue or lock is added.

The new private single-header regression fails on the saved old header, observes
no completed handle on a live current connection, checks pending/terminal cancel
ownership, exercises fallback and continues QoS1. It holds its raw observation
context through actual fixture Engine join. Windows/WSL each pass the three Dial
programs, root Auth composition and Dial unit/example (five distinct programs,
six executions),46 independent byte-peer cases and four root/Dial static/shared
consumers. WSL ASan/UBSan/leak and four TSan program executions pass; targeted
shared-tool and generated/API/docs checks pass. Executed binaries/package
closures, old/current source headers and all failures remain retained. This is
matching targeted validation, not a newly rerun complete four-library suite.

Current requested constructor-scope live bytes are12934 on Windows and13422 on
WSL in both one-endpoint scenarios, versus13474/13475 and13962/13963 before.
Controls are no longer held by Client after Done; saved metadata accounts for
at least564/565 requested controller/address bytes per connection here. Old
candidate-error storage also retires when its last actual owner releases it.
Heap caches/independent owners mean this is not a process-RSS release guarantee.

Same current Core and unchanged lifecycle driver use seven alternating2000-cycle
samples per numeric/prepared/hostname variant, all84 formal outputs across both
platforms retained. Windows before/current throughput medians are
3857.153/3862.179,3809.958/3814.47,3699.256/3698.316 cycles/s; WSL
3295.475/3310.28,3373.093/3381.8,3225.368/3206.083. All throughput pairs qualify
under original3% MAD/6% central range and stay within the5% regression limit.
Qualified cycle-P99 pairs (Windows numeric/hostname and WSL numeric) stay within
10%; other setup/cycle tail comparisons remain unqualified and are not accepted.
No source, baseline or noise threshold was forced. Unpinned loopback lifecycle
measurements still do not establish broker parity/fixed-arrival/native acceptance.

Evidence: extlibs/xmqtt/tests/results/dial-retirement-20261109.json. The original
four-library goal stays active; MQTT TLS/WebSocket/session/expiry/reconnect/durable
state, remaining Redis/RPC/Cron scope and full native/performance/>=72h production
release requirements remain.


## 2026-11-09 - serialize TCP prefix consumption across reentrant release callbacks

Developed directly on master from cd34984c; no branch, checkout or worktree
operation. The MQTT TLS prerequisite probe exposed a real Core TCP defect: the
first release callback of a vector write could append and drive the still-being-
consumed original prefix again. The Windows byte-peer diagnostic received64 bytes
instead of48, with no Drain and incorrect byte credit. The final identical
regression fails on the saved old header and passes on current Core on both OSes.

A worker-only WriteConsuming flag now spans buffer consumption and exact credit
return. Nested sends remain admissible and append FIFO; the enclosing readiness
or completion driver resumes after the completed prefix is fully consumed. No
allocation, lock, queue, runtime or public API is added. Before/current private
Stream sizes are identical in all three measured profiles: base/retire/Future+
retire704/744/800 bytes on Windows and704/880/936 on WSL. The flag occupies prior
padding; private bool offsets change, following aligned fields do not.

Seven real-loopback scenarios exercise copy/ref-batch/single-ref appends,
close/abort from the releaser, and high/low-water appends. They assert exact peer
bytes and FIFO order, release pointer/size and exactly-once counts, zero remaining
credit and actual physical retirement. The test is registered for modular/single
selection plus explicit IOCP and io_uring. The single header is regenerated.

Frozen6274 C/header/config/tool inputs remain identical throughout final
validation and measurement. This Core change therefore reruns all four explicit
*_tests aggregators plus event_waiter: xredis30, xrpc30, xmqtt22, xcron/waiter34,
116 C programs per OS. Windows additionally passes30 Core TCP/retire/ownership/
IOCP programs and four related TLS programs. WSL passes22 select/epoll/io_uring
Core programs, four TLS programs under ASan/UBSan/leak checking, plus two TSan
reentrant-write executions. Both OSes pass eight root static/shared consumers;
executed test binaries and actual package closures are archived with hashes.
Generated/features/API/docs checks and61 shared-tool unit tests pass.

Initial combined testing exposed an existing size assertion that omitted the
optional physical-retirement mutex/cond. Old/current layout probes prove no
size growth; that test now separately budgets those actual platform primitives,
retaining the original base/Future/dial limits. Earlier test-format, diagnostic
feature-selection and docs-invocation failures remain preserved. Initial public
root targets launched0/1 programs because they collect examples; those runs are
not counted as complete tests. Explicit test aggregators provide the116 counts.

Seven alternating300000-QoS1/window32 samples use the same current MQTT client,
fixture and compiler flags, changing only immutable before/current Core headers.
WSL throughput medians120051.485/120170.825 publishes/s qualify under the original
3% MAD/6% central-range noise limits and remain within the5% regression limit.
Windows medians388797.449/391014.946 are descriptive only: current-sample noise
fails. Both platforms' P99 comparisons remain unqualified. All28 formal outputs
are retained; no baseline/noise/regression threshold is forced or relaxed. These
unpinned loopback measurements do not establish general performance acceptance,
broker parity, fixed-arrival behavior or native Linux production performance.

Evidence: docs/development/tcp-write-reentry-20261109.json, with raw artifacts in
out/service-extensions/tcp-write-observer-20261109. All four products remain
developing and the original goal remains active. MQTT TLS/WebSocket/session/
expiry/reconnect/durable state, remaining Redis/RPC/Cron scope, native Linux,
>=72h soak and full release/performance acceptance (including the retained xcron
tzdb_file formal-profile gap) remain outstanding.


## 2026-11-09 - separate Cron batch rollback from older publication tails

Fresh master Windows testing exposed a stale batch-test assertion: receiving
all physical results returns their slots, while the previous native Drop/Notify
may still hold a real producer duty. Controlled before/current Core probes hold
Notify and deterministically observe Active0/Producers1 after duplicate-prefix
rollback, then reuse all64 slots with Active64/Producers65. Production follows
the existing documented separate-duty contract.

The test now boundedly retires prior successful publishers before attributing
zero producers to a newly rejected batch; the original strict rollback assertion
remains. A controlled blocked-notification regression verifies retained old duties,
complete capacity reuse and actual destruction/join. Production Cron is unchanged.
Both actual-master full Cron/waiter suites pass34 programs, WSL with ASan/UBSan/
leak-halt; two corrected WSL executor programs additionally pass TSan. The original
Windows failure, prior inputs/binaries, controlled probes and corrected receipts
are retained. Evidence: extlibs/xcron/tests/results/executor-publication-tail-20261109.json.
The four-product production/performance goal remains active.


## 2026-11-09 - optional TLS ciphertext prefix retirement on master

All current development remains in D:/GIT/xrt master. The old service-extension
branch is already an ancestor; the26 old dirty source paths were migrated in
05dc625d and subsequently improved on master. The old worktree is a preserved
historical scene, not a second current implementation. No branch/worktree switch,
creation or remote push is performed in this stage.

A real TLS slow-peer probe demonstrates the prerequisite gap: the saved first
ciphertext cut can retire with1279668 later bytes pending and no Drain/Writable
edge. New optional net_tcp_write_progress/tls_stream_write_progress leaves expose
one local permanent Produced/Completed epoch, independent of diagnostic stats.
The exact native consumed prefix and byte credit are consistent before counting.
Public Worker-only SetWriteProgress/WriteProgress reuse the existing TLS driver,
zero-copy ciphertext transfer and deferred DriveCommand; no shared runtime or
per-write watch node is added. Notifications coalesce outside Send/SendVec and
native completion, use existing event Data/owner lifetimes, and clear on terminal
close or TLS event replacement. Native rebind invalidates the epoch. Active
independent observers refuse V1 graph admission until removed and quiescent.
Failure/abort/close/overflow never imply successful prefix completion; pending
unencoded async plaintext is outside a captured cut. This is local native queue
retirement, not peer acknowledgement.

Before/current unselected private TCP/TLS sizes remain704/360 bytes on both OSes;
selected sizes720/384 add16/24 bytes. Public Config32/Events56 stay unchanged.
Activation/snapshot pass a one-byte budget with zero allocation counters. No RSS,
pool-size or zero-overhead claim is made. Twenty-two affected extension declaration
headers receive only20 generated restore lines each for the two Core feature
macros; every catalog declaration matches the generator and five existing shared
tool tests compile/check the catalog. Canonical unrelated implementations stay intact.

Core validation freezes2592 inputs: Windows44 and WSL45 progress/ownership/TCP/
TLS program executions pass, including stats-off single and explicit io_uring;
WSL correctness uses ASan/UBSan/leak-halt and five additional TSan executions pass.
Generic and full-crypto static/shared packages and public-only actual-TLS consumers
pass on each OS. Sixty-six focused shared-tool tests and generated/API/docs/
maturity checks pass. Fresh four-product aggregators plus event_waiter pass
Redis30/RPC30/MQTT22/Cron34 programs,116 per OS, with eight root static/shared
consumers each. The final3025-input product map differs from the preceding map
only in the corrected Cron test fixture documented by15af1b9d; complete Cron
suites rerun on both OSes plus two WSL executor TSan programs. Other retained
successful commands do not compile that fixture. Generation separately freezes885
header/config/tool inputs. Actual binaries and package closures are archived.

The original legacy verifier graph fixture fails on saved old Core too. Its
corrected OwnedV1 verifier has a real trust leaf and no unregistered creator alias;
old/current Core accept the corrected graph without weakening admission checks.
The original resumed-chain test also fails before this change: the authoritative
header/design shares the immutable original chain. Tests now require one
certificate and exact original DER; the benchmark aligns its visibility count.
Ticket/resumption/KeyUpdate/byte checks remain; production getters are unchanged. GCC15 WSL TSan O1 ownership compilation
warns about psk.c output counters and executes0 programs; strict O2 runs both
programs successfully without disabling that warning or changing the parser.
These and earlier fixture/probe/packaging/source-freeze failures remain preserved.

Seven alternating50000-message64-byte samples for each before/no-leaf/compiled-
inactive/active-observer variant retain all56 formal outputs. All throughput
pairs fail original noise gates. Only Windows before/no-leaf P99 qualifies and
stays within10%; other tails remain unqualified. No baseline, noise or regression
threshold is forced or relaxed. These unpinned loopback results do not establish
general feature overhead, broker parity, fixed-arrival or native production acceptance.

Evidence: docs/development/tls-write-progress-20261109.json and
out/service-extensions/xmqtt-tls-20261109. This is the necessary Core prerequisite;
MQTT TLS integration is still outstanding. All four products and the original
goal remain developing, including remaining protocol/durability scope, qualified
throughput/P99 profiles, native Linux and at least72h production release gates.

## 2026-11-09: MQTT verified TLS transport on master

Developed directly at D:/GIT/xrt master, without creating/switching branches.
The prior service branch remains an ancestor; the old worktree is a preserved
historical backup. No Core/Redis/RPC/Cron canonical inputs changed in this stage.

Optional xmqtt_client_tls now provides numeric/hostname/prepared-address setup,
TLS1.2/1.3 peer verification, optional native mutual authentication and all three
MQTT enhanced-auth compositions. Names/ALPN/shared contexts and signing identity
are captured before DNS; original deadlines cover DNS/TCP/TLS/MQTT gates.
The existing bounded protocol owner handles QoS/aliases/subscriptions/auth.
One actual request lease and aligned24-byte fence plus padding observe native
ciphertext-prefix retirement without extending an earlier QoS0 cut for later
output. ACK completion remains separate from physical quota/id retirement.
TLS output shares WriteBytes/WorkPackets budgets. Graceful shutdown includes
authenticated native close; Wait observes actual final Session/verifier tails.

3050 frozen inputs identify actual-mainline verification: Windows and WSL each
pass27 full MQTT and9 TLS-disabled client/auth/Dial programs. WSL adds explicit
io_uring, TSan and ten ASan repetitions of the slow-reader/lifecycle suite;
normal WSL correctness uses ASan/UBSan/leak-halt. Each OS verifies four root/leaf
static/shared consumers, plus72 independent SSL literal-MQTT cases and4 real
examples per isolated package:304 executions across both OSes. Strict CA/name,
mutual-auth refusal/success, TLS versions, ALPN/SNI and authenticated shutdown
are checked. Runtime keys remain temporary. Generated/API/docs/maturity checks
and89 focused tool tests pass. Example, performance and size profiles are runnable.

The original1KiB slow-peer window intermittently left Linux's kernel window at
3072 bytes with1.8MiB queued and100% receive-window limitation, despite empty,
unpaused native TLS input. Accepted fixture uses32KiB only while paused and
restores4MiB on resume; original prefix/quota/byte/deadline assertions stay.
Macro/policy/ALPN/overlap-fixture/link-command failures remain archived. D-disk
exhaustion interrupted one archive and one compile; own artifacts/build caches
were moved to C with per-file SHA256 checks and junctions preserving old paths.
Affected steps pass after rerun; no unrelated artifacts were removed.

Seven alternating300000-message/window32 samples per before/plain/TLS-compiled
variant retain all42 raw outputs. WSL throughput pairs qualify and change
-0.69%/-1.19%, within the original5% gate. Windows pairs and all P99 comparisons
fail original noise gates and remain unqualified. Twelve20000-message TLS QoS1
baseline runs include both native peers, with handshake outside the timer.
These unpinned closed-loop results do not establish broker parity, fixed-arrival
behavior or production acceptance. Public ClientConfig272/Info160 and request
header104 stay fixed; measured TLS config200/state88/fence24 plus padding and
selected Client/Dial pointer costs are recorded explicitly.

Evidence: extlibs/xmqtt/tests/results/client-tls-20261109.json and
out/service-extensions/xmqtt-tls-client-20261109. All four products remain
developing. MQTT ticket cache/reconnect/durable session recovery, WebSocket,
pinned brokers, qualified deployment P99/native Linux and at least72h native
production gates remain required by the original goal.

## 2026-11-09: bounded TLS1.3 credential handoff on master

Optional xmqtt_client_tls_resume closes the gap between accepting a supplied
native Resume and exporting a newly issued credential. A latest-only native
reference slot provides thread-safe Info/Take, authenticated TLS establishment/
resumption observation and saturating capture/eviction/rejection counts.
Cache data bytes (including saved chain DER) use MaxPrivateBytes. Optional
retention refusal preserves connection health and an older credential; native
opaque metadata/parsed-chain/queue storage keep separate Core bounds. Take
transfers ownership/budget to the caller and allocates/copies/posts nothing.
Native callbacks account/move immutable references and coalesce existing owner
work; Notify uses that work and its existing entry gate. No Core/runtime change.

Old public TLS config200, ClientConfig272/Info160 and request header104 remain
fixed. New snapshot48 and selected TLS-private state160 versus88 (+72) are
measured on Windows/WSL x64; the Client object itself and capture-disabled state
do not grow. Normal data sends add no adapter allocation. This is not an RSS,
exact native metadata-size or zero-overhead claim.

3061 frozen actual-mainline inputs: each OS passes31 full MQTT,5 TLS capture-off
and9 TLS-off programs. WSL uses ASan/UBSan/leak-halt, plus explicit io_uring and
TSan. Real resumed rounds cover all factories and both MQTT versions, physical
close/new CONNECT, byte charge/refund, replacement/refusal, disabled capture,
concurrent takers, Notify take/Close reentry, bad/overlapping outputs, no-allocation
Info/Take and credentials retained after actual Engine join. Failed example
indentation and an abort-test assumption (FAILED versus authenticated CLOSED)
remain recorded; normal authenticated reset assertions remain strict.

Both root and isolated optional-leaf static/shared consumers pass on each OS.
Each isolated package runs the prior72-case strict TLS/mTLS matrix and4 examples,
plus36 independent SSL1.3 resume/fallback/name/ALPN/expiry cases and1 two-connection
example:452 scenario/example executions. The independent server confirms real
session_reused, literal fresh MQTT traffic and authenticated shutdown. Native
ResumeOnly refusal exposes no second MQTT session. Credentials stay runtime-only.
Generated/API/docs/maturity and89 focused tool tests, runnable setup benchmark
smoke and static size profiles pass. Non-MQTT canonical inputs remain unchanged.

All42 alternating300000-message/window32 TLS data outputs are retained. Original
noise/regression limits remain3% MAD/6% central range and5% throughput/10% P99.
All four throughput pairs qualify (Windows before/off -2.15%, off/compiled +2.28%;
WSL +0.64%/+0.86%) and stay within5%. All P99 pairs remain noise-unqualified.
Seven100-pair setup runs per OS measure actual full/resumed factory-to-CONNACK
including caller wake and fresh MQTT readiness. Median run P50 is8.907/1.453ms
on Windows and8.935/1.304ms on WSL. These callback-trust local peers, unpinned
closed-loop data and setup samples do not certify broker/deployment performance.

Evidence: extlibs/xmqtt/tests/results/client-tls-resume-20261109.json and
out/service-extensions/xmqtt-tls-resume-20261109 (new test/evidence storage on C,
canonical work remains D:/GIT/xrt master). Native mTLS resumption/TLS1.2 ticket
resumption, cross-connection cache/reconnect, MQTT durable session recovery and
WebSocket remain pending, alongside the original four-product qualified P99,
pinned-broker, native Linux and at least72h native production gates.

## 2026-11-09: pinned real MQTT broker interoperability on master

Adds an opt-in public-API probe and process harness against Eclipse Mosquitto
2.1.2. The official source archive, licenses, extracted fixture packages, tools,
runtime libraries and built broker/password tool have recorded SHA256 identities.
All1949 original upstream files match the archive. Upstream build include paths
are supplied through CMake flags; upstream code is unchanged and is not used as
a protocol implementation reference or product dependency.

The68-case matrix covers MQTT5/311, TCP/TLS, numeric/hostname/prepared factories,
65537-byte binary QoS0/1/2, both QoS2 ACKs, subscriptions, outgoing aliases,
No Local ordering, retained delivery/deletion, abnormal-close Will, basic auth
success/refusal,37/41-byte read/write and13-byte chunks, physical retirement,
and retained messages after all Client/Resolver/Engine owners join. Independent
literal Python CONNECTs seed and observe SessionPresent=1. Current clients must
return STATUS_SESSION; these24 cases per run expose unsupported MQTT session
recovery rather than claiming recovery or durability.

Windows native and WSL clients each pass modular/single/static/shared matrices;
WSL adds fully instrumented ASan/UBSan/leak-halt and applicable TSan-halt matrices:
680 scenario executions across10 variants. GCC's existing atomic_thread_fence
TSan instrumentation warning remains recorded. All20 matrix broker processes are
reaped. An additional intentional failing client exits1 while its own broker is
reaped and temporary runtime keys/passwords disappear. Single/features/API/docs/
maturity checks and Python compilation pass. Previously validated static/shared
leaf packages remain byte-identical; their new consumers run the actual matrix.
All previous3061 frozen canonical inputs remain unchanged, with five new test/
pin inputs. No Core or product runtime/generated code change.

The broker runs in WSL Linux, with native Windows clients using localhost
forwarding. Official native Windows installer downloads failed (timeout, empty
reply or TLS transport error); incomplete files remain preserved and were never
executed. Earlier missing-header/dependency and listener-option-placement errors
remain archived alongside successful builds. Old prototype runs are excluded
from the680 formal count. These are correctness checks, not a performance result.

Evidence: extlibs/xmqtt/tests/results/broker-20261109.json and
out/service-extensions/xmqtt-broker-20261109 (C-backed stage storage, canonical
sources D:/GIT/xrt master). No branch operation or remote push. Native Windows
broker/native Linux deployment, additional brokers/enhanced-auth algorithms,
MQTT durable session/expiry/reconnect/replay and WebSocket remain pending.
All four products remain developing under the unchanged scope and comparator/
qualified P99/native >=72h release gates.

## 2026-11-10: bounded in-memory MQTT session protocol state on master

Adds optional xmqtt_session, a caller-driven state component for the next Client
recovery/store integration. It owns no Engine, Future, credential, retry thread
or shared runtime. Immutable canonical QoS1/2 snapshots, original/auto ids and
separate ready queues implement PUBLISH/PUBREC/PUBREL/PUBCOMP state. Connection
aliases are removed, an exposed partial PUBLISH retries with DUP, and successful
PUBREC means only PUBREL replays. New connections reset send quota; duplicate/
unmatched PUBCOMP cannot mint extra credit or complete an unsent new generation.

Incoming QoS2 preparation is rollbackable before delivery; allocation-free commit
precedes the owner's prepared queue publication and PUBREC. Committed identities
survive reconnect through full PUBCOMP emission. Explicit Open/End/Expire/reset
use monotonic elapsed time, including rounded very large clocks, and never erase
live state. MQTT5 missing-server state resets old obligations;311 mismatch fails
closed until an explicit clean reset. Data/id/object/byte duties survive logical
completion through final physical references, including after unique Session
destruction. Fixed nonempty client identity and caller-bound broker/principal
namespace are explicit. Client attachment/assigned-id adoption remain pending.

3075 frozen actual-master inputs identify validation. Windows and WSL each pass
the33-program full MQTT suite,2 minimal session programs and1 runnable state
example. WSL uses ASan/UBSan/leak halt, ten extra full lifecycle/id-space runs and
2 applicable TSan programs with eight retained readers. All65535 outgoing and
incoming ids, six reconnect cuts/version, stale/partial frames, negative ACKs,
hash collisions, peer restrictions, exact byte/object limits, OOM rollback and
incoming commit/duplicate rules execute. Root/leaf static/shared consumers run
on both OSes. Thirteen literal TCP matrices pass156 cases (312 actual connections)
through modular/single/root/leaf/static/shared/ASan forms; no broker recovery or
Client integration is inferred from the test-only public Core socket driver.

An initial OOM snapshot memcmp failed only at byte68 of unspecified C padding;
diagnostics confirmed unchanged semantic fields. Tests now compare fields and
still verify id-cursor rollback. Original failed/diagnostic sources, binaries and
logs are preserved. Unsupported single/example CLI selector attempts are also
recorded and the documented example command is corrected. Generated/API/docs,
Core/extension maturity and89 focused tool tests pass. Non-MQTT canonical inputs
are unchanged. Both OSes' prior8 MQTT and139 Core archive objects are byte-identical;
only the session object is added. No old Client execution code/layout changes.

The12-case primitive benchmark covers both versions, QoS1/2 and64/1024/65536-byte
payloads with seven measured samples and one warmup. It includes one immutable
packet allocation/copy, independent codec Size/Write/Read validation, gates and
refs. Original100000-cycle noise checks qualify Windows10/12 and WSL12/12.
All12 Windows cases rerun at1000000 cycles qualify11/12. A separate own-process
logical-CPU10 experiment verifies inherited child affinity0x400 and also qualifies
11/12. No system power-policy change, discarded rows or relaxed3% MAD/6% central
range. All raw original/long/pinned outputs remain archived. Windows remains
noise-unqualified as a full matrix; original5% throughput/10% P99 release gates,
independent comparators and native deployment measurements remain pending.

Measured public config112/info88/frame64/packet-info32 stay feature-independent.
Default fixed state with13-byte benchmark identity is27077 bytes on Windows and
27149 on WSL. Per-payload physical live peaks and static leaf size/symbol checks
are recorded; logical quotas exclude allocator pools/native/caller queues.
Evidence: extlibs/xmqtt/tests/results/session-20261110.json and
out/service-extensions/xmqtt-session-20261110 (own stage on C; canonical source
D:/GIT/xrt master). No branch operation or push. Existing Client factories still
refuse unowned retained sessions. Future/deadline/cancel and pending receive-payload
ownership, reconnect/offline queues, durable store/crash recovery, MQTT WebSocket
and remaining Redis/RPC/Cron plus native>=72h acceptance remain under the full
unchanged goal. All four products remain developing.

## 2026-11-10: restore anchored-file performance registration during mainline checks

Fresh four-root Windows regression passes127 module/single programs but the
xcron maturity check reports missing tzdb_file performance coverage. Adds a real
anchored-root reload driver and1/16-name profiles, without changing product C/H
or runtime APIs. It creates pinned UTC fixtures outside timing, checks each CAS
revision and final names/offsets, and removes its own files. Timed work includes
resolve/open/stat/read/close, TZif decode/copy, publication and old snapshot free.

Windows and WSL each pass both file-loader tests and generated/API/docs/maturity
checks. WSL additionally runs the benchmark with ASan/UBSan/leak halt for both
workloads and three invalid-argument cases. Seven measured samples and one warmup
per profile execute on both platforms. Both matrices remain noise-unqualified
under unchanged3% MAD/6% central-range bounds; no accepted performance baseline.
Windows initial local compile overlap and WSL DrvFs storage are explicit limits.
Original failed maturity/quality logs and executable/source hashes remain in
out/service-extensions/mainline-recheck-20261110. Portable evidence:
extlibs/xcron/tests/results/tzdb-file-profile-20261110.json. Full mainline and
packaging revalidation follows this focused fix; four products remain developing.

## 2026-11-10: reconfirm mainline migration and rerun all four products

The preserved service branch is an ancestor of master. All26 pending source,
documentation and generated files in the retired worktree still match their
original migration archive; no new pending paths exist. They were integrated at
05dc625d and improved on master, so the backup is not reapplied over current work.
The existing direct-master workflow and retired-worktree guidance remain in force.

Fresh validation executes against ad6e19774f0bd61561023687ed3f3246cefb8325 at D:/GIT/xrt master.
Windows and WSL each pass127 root modular/single programs: Redis30, RPC30,
MQTT33 and Cron34. WSL uses ASan/UBSan with leak detection and halt on errors.
All four roots also pass static/shared package consumer verification on both
platforms (eight package commands each). All four feature/single/API/docs/maturity
checks and89 focused shared-tool tests pass. Exact counts, commands, binaries and
logs are preserved with source identity in the portable report and C-backed stage.
The initial capture script mistook mocked tool stdout for a real test path after
89 tools already passed. The raw error/log/recipe remain; the corrected capture
reruns tools and resumes packaging while verifying prior successful suite hashes.

No product source changes, new branch, merge, checkout or push. The old worktree
remains a backup; subsequent development continues directly on master. These are
regression checks, not performance/native-deployment/72h release qualification.
Evidence: docs/development/service-extensions-mainline-20261110.json and
out/service-extensions/mainline-verified-20261110. The four-product goal remains active.

## 2026-11-10: exact MQTT frame selections during repeated PUBREL

A retained earlier PUBREL frame could finish a later selection in the same
connection: Packet, Generation, type/id and immutable spans were identical.
A new regression demonstrates old Sent returning DONE where STATE_ERROR is
required. Next now issues a distinct Generation/Selection pair; stale Sent
leaves the actual selection and physical reference intact. uint64 sequence
exhaustion is transactional; accepted Open resets it under a new generation.

Rebuild consumers for the developing frame ABI64->72 on x64; earlier field
offsets and config112/info88/packet-info32 remain. Fixed managed state grows8
bytes. No extra successful-path allocation/payload copy/gate. Tests cover256
repeated selections/version, changed/old-size tokens, diagnostic OOM, PUBCOMP
while a repeated frame is selected, ready-flow fencing, retained owner destruction
and controlled UINT64_MAX/reconnect boundaries in the single implementation.

Both OSes cover33 MQTT root programs,2 minimal units and the example. WSL adds
ASan/UBSan/leak,2 TSan units and10 extra lifecycle runs. A new literal peer case
checks a rejected old PUBREL token, a one-byte repeated write cut and reconnect.
Thirteen modular/single/root/leaf/static/shared/ASan forms pass182 cases over364
actual TCP connections. Eight package consumers and89 focused tools pass.
An OOM-test scope typedef typo initially fails compilation after13 modular
passes/OS; correcting only that test and running the last modular plus19 singles
completes coverage, with exact source delta, old failures and all binaries retained.

Both root archives retain8 other MQTT and139 Core objects byte-identically.
Matched7-sample/1-warmup primitive comparisons use preserved pre-phase binaries
and unchanged benchmark source/arguments. Qualified pairs: Win9/12, WSL4/12;
all qualified pairs and raw medians stay inside5% throughput loss, but full
matrices remain unqualified under unchanged3% MAD/6% central-range bounds.
Pure leaf text: Win74132, WSL75680 bytes; no Client/Engine/Future/Promise symbols.
This is not broker/P99/native deployment/72h performance acceptance.

A separate controlled reference-count audit also confirms Next can wrap its
unchecked two-reference increment (artificial boundary, restored before free).
Fix bounded atomic reservation next, before Client attachment. The full goal
remains active and all products developing. Portable evidence:
extlibs/xmqtt/tests/results/selection-20261110.json; raw sources, programs and
failures: out/service-extensions/xmqtt-selection-20261110. No branch or push.

## 2026-11-10: bounded MQTT frame reference reservation

The earlier controlled audit is resolved: unchecked Next increments could exceed
the public cap and wrap. New boundaries first reproduce DONE instead of RESTRICTED.
The bulk-CAS proposal passes correctness but qualified small-payload throughput
exceeds5% loss; raw contended tail also worsens. Its complete evidence is retained
and it is rejected. The final owner holds Gate and an active reference, adds2
internal counts, checks and refunds before ready/quota/sequence/output mutation.
One speculation/packet bounds supported internal counts to cap+2. The active
reference prevents a physical free during refund; public Retain keeps original
CAS. No over-cap frame is published. One successful RMW replaces two; refusal
adds a raw refund RMW. No extra allocation/copy/gate/storage or ABI change.

Both OSes rerun2 final root Session and2 minimal programs plus the example.
WSL adds ASan/UBSan/leak,2 TSan programs and10 lifecycle repeats. Controlled
single tests cover20 count/phase/version boundaries, exact success, Retain cap,
4 OOM cases and12 forced pressure/refund schedules, including aggregate release
down to protected active/caller references. Real8-reader contention runs for
both versions with5000 owner frames/version and strict physical cleanup.
Test interception hooks are absent from generated/packaged SDK code.

All8 root/leaf static/shared consumers,182 literal two-connection cases across13
forms (364 TCP connections), generated/API/docs/maturity and89 tools pass.
The earlier33-program suites belong to rejected CAS code, not the final source.
Other8 MQTT/139 Core archive members remain byte-identical/OS. Windows container
metadata differs; comparison uses member bodies and timings link one common Core.

Same-source7-sample/1-warmup regular pairs qualify Win8/12 and WSL7/12 under
unchanged3% MAD/6% central range; all qualified and raw median ratios meet5%
throughput loss. Closed-loop0/8-retainer P99 remains noise-unqualified and tiny
timings quantized; Linux8-reader raw P99 exceeds1.10. No complete performance,
fixed-arrival/broker/native-deployment/72h acceptance. Rejected CAS, two isolated
constant-operation proposals and all failed benchmark/selector/publication
preparation attempts remain preserved. Final x64 layouts and managed sizes agree.

Continue with Client attachment and pending payload/Future ownership. Full goal
stays active, all products developing. Evidence: extlibs/xmqtt/tests/results/
frame-refs-20261110.json and out/service-extensions/xmqtt-frame-refs-20261110.
No branch operation or remote push.

## 2026-11-10: passive MQTT Inbox attached to real Client

Six optional APIs preserve accepted PUBLISH FIFO and one physical result/message
box quota across sequential Clients and Engines. Attach remains exclusive through
actual Client Destroy; failed constructors refund it. Old queued/taken messages,
CONNACK/ACK metadata and result boxes continue consuming shared bounds. Destroying
Inbox releases untaken messages; retained outputs outlive Inbox and all Engines.
No new worker/Future/timer/runtime or per-message allocation/copy. Enabled enqueue
adds an Inbox gate beside the existing Client gate; Take uses one Inbox gate.
The opt-in Client field is one pointer; ordinary Client-only builds omit Inbox
code/field. Public x64 Client config272/Info160/SessionFrame72 are unchanged;
Inbox config48/Info48. Fixed Inbox+quota metadata: two managed allocations,
Win184 / WSL328 requested bytes, fully refunded. Native/allocator/Future/error
storage remains outside box accounting; these gauges are not process memory.

Final Windows and WSL each pass35 MQTT root programs,2 minimal Inbox programs,
the two-Engine example,4 root/leaf static/shared consumers and4 actual packaged
Client suites. WSL includes ASan/UBSan/leak,2 TSan forms and5 repeated sanitized
lifecycle runs. Both MQTT versions cover incoming QoS0/1/2, FIFO across3 Engines,
held old results, count/byte limits, exact byte refunds, exclusive lease, failed
constructor reuse, OOM cleanup, concurrent consumers and notification reentry.
Generated/features/API/docs/maturity and89 focused tools pass. Only Client changes
among9 MQTT archive members; all139 Core members/OS and8 other MQTT members agree.

Initial suites pass33 root programs/OS before explicit Inbox root registration.
Windows leaf single compilation warns about an uninitialized test local; its
zero initialization and explicit root test/owner assets are the only final
source-map delta. Both final35-program suites are rerun. An example selector
initially targets an unregistered inherited test; removing it runs the example.
A performance helper name and metadata probe include are corrected before useful
runs. Earlier logs, binary snapshots and corrected recipes remain preserved.

Seven matched samples/one warmup compare parent/current ordinary Client and
current Inbox/ordinary using the same current driver source and one common
parent Core archive. For each comparison, qualified throughput pairs are Win0/4;
WSL2/4. Qualified P99 pairs are Win0/4; WSL ordinary0/4, Inbox1/4. Qualified pairs
meet unchanged5% throughput/10% P99 bounds. All raw throughput/P99 medians also
meet these bounds; raw maximum latency sometimes grows substantially. Matrices
remain noise-unqualified under3% MAD/6% central-range bounds. Eight registered
profile smoke cases/OS are execution checks, not performance qualification.
This closed-loop local QoS1 echo includes peer/observer/final PUBACKs, not fixed-
arrival network tails, independent broker parity or native/72h release acceptance.

Inbox currently attaches to numeric raw TCP. It preserves application payloads,
not MQTT protocol session state: full Session attachment/reconnect, TLS/Dial/Auth
composition and durable recovery remain required. The four-product goal stays
active and all products developing. Development continues on D:/GIT/xrt master.
Old26-file worktree backup is unchanged, archived and already integrated; no
new pending backup paths. No branch operation or push. Evidence:
extlibs/xmqtt/tests/results/client-inbox-20261110.json and
out/service-extensions/xmqtt-inbox-20261110 (C-backed artifacts).

## 2026-11-10: explicit MQTT Client Session recovery on master

The optional numeric TCP factory now attaches the in-memory Session and shared
Inbox to actual Client IO. QoS1 and pre-PUBREC QoS2 replay DUP with a full logical
topic and no old aliases; post-PUBREC QoS2 replays PUBREL only. Incoming QoS2
commits before queue visibility/PUBREC, suppresses resumed duplicates and retires
known old PUBREL without corrupting current receive-window accounting. Futures
stay scoped to the original connection and retained outputs outlive both Engines.
Claims last through actual Client Destroy; failed setup/replay allocation refunds
temporary storage and preserves old obligations. Expiry starts at actual native
termination and respects a fully accepted DISCONNECT override. Early old ACKs
during partial PUBLISH replay commit progress then retire the transport.

Old obligations reserve request slots/outgoing bytes and identifiers during setup;
current negotiation and conservative in-flight bounds still apply. The ready-list
transition for early old PUBREC is fixed: the same focused regression fails on
the parent archives and passes current archives on both OSes. Packet/request
capacity is now refunded after actual allocator retirement, verified with held
free barriers. Controlled zero/one-byte native output and replay-header allocation
failures are test-only hooks, absent from generated and packaged SDKs.

Windows/WSL each pass37 MQTT root programs,2 minimal Client Session programs,
2 pure Session programs, the new runnable example,4 generic root/leaf static/shared
consumers and4 actual packaged recovery suites. WSL includes ASan/UBSan/leak,
2 TSan forms and5 repeated sanitized lifecycle runs. Generated/features/API/docs/
maturity checks and89 focused tools pass on each OS. Buffered parent/child output
initially hides the final Windows binary from the evidence parser; corrected
basename extraction snapshots all37 from the same successful run. No test retry
is represented as a new successful root run. Preparation/compiler/fixture/probe
failures remain in the evidence directory.

All139 Core archive members per OS are byte-identical to the parent;7/9 MQTT
members agree, with only Client and Session changed. Public x64 Client config272,
Info160, Session config112/Info88/frame72 and request header104 are unchanged.
The optional driver is120 requested bytes, optional request packet pointer8 bytes;
enabled private Client adds one pointer and Session two pointers. New QoS1/2
currently incurs an extra canonical allocation/payload copy. Replay borrows
immutable packet data with native reference leases and allocates a request header
per old flow, without a new Future/result allocation or payload copy. These costs
are explicit performance debt, not zero-overhead or production acceptance.

Seven samples/one warmup compare parent/current ordinary Client and current
Session/ordinary using one common parent Core archive. Exact distributions,
ratios and unchanged3% MAD/6% central-range/5% throughput/10% P99 gates are retained.
External host builds are observed and recorded; closed-loop local echo results
are diagnostic, not an isolated native/broker/fixed-arrival/72h qualification.
Eight registered workload smoke cases and two size profiles per OS are execution
checks. Complete performance acceptance remains false.

Development remains on D:/GIT/xrt master. The unchanged26-file old worktree is a
preserved, already integrated backup; no new pending paths, branch operations or
push. TLS/Dial/Auth Session factories, assigned-id adoption, automatic reconnect,
durable storage, broker recovery interoperability, removal of the extra snapshot
copy and final production/performance gates remain required. The four-product
goal stays active. Evidence: extlibs/xmqtt/tests/results/client-session-20261110.json
and out/service-extensions/xmqtt-client-session-20261110 (C-backed artifacts).

| OS | Comparison | Noise-qualified throughput pairs | Noise-qualified P99 pairs | Raw median bound failures (throughput/P99) |
| --- | --- | --- | --- | --- |
| windows | after | 0/4 | 0/4 | 1/0 |
| windows | session | 0/4 | 0/4 | 2/2 |
| wsl | after | 2/4 | 2/4 | 0/0 |
| wsl | session | 2/4 | 2/4 | 0/0 |

All qualified throughput/P99 pairs meet their bounds. Windows window32 raw
Session throughput is12-14% lower and P99 is13-21% higher. Those series are not
noise-qualified; the extra snapshot and unresolved regression risk remain open.
The after comparison means current ordinary/parent ordinary; session means
current Session/current ordinary. This table does not grant performance acceptance.

## 2026-11-10: share unaliased Client Session publication snapshots

Unaliased QoS1/2 now uses one immutable Session snapshot from public admission
through output and recovery. The request has no private wire buffer; first output
uses one native reference and does not reparse/re-encode or allocate another
packet. Independent request and protocol packet headers remain two allocations.
Initial packet reference1 gains an active model reference on first selection;
the existing frame/selection/native ownership continues through physical free.
Outgoing aliases retain their original binding/queue linearization and two-copy
path; they remain required optimization work rather than a removed capability.

Object/id/byte capacity is physically reserved under the Session gate before
allocation/encoding, then encoded outside both gates. It is not in the active
protocol lists and does not set HasState before first selection. Early Session
pressure returns OVERLOADED with an unchanged Future output. Unsent cancellation,
failed setup/allocation and close during encoding refund Client/Session resources
without creating a recoverable obligation. Caller allocation scopes retain charges
for genuinely retained protocol packets across connection destruction.

The exact257-byte MQTT memcpy probe counts two payload copies on parent d6b79373
and one on current code, in four MQTT-version/QoS combinations per OS. The alias
control remains two on both. Hooks are absent from generated/packaged SDKs and
exclude Core/native/kernel copies; no end-to-end zero-copy claim is made.
Constructor plus eight operation-budget prefixes, physical-capacity pre-CONNACK
pressure, four concurrent producers, cancellation and controlled close-during-copy
tests pass. The copy barrier proves both gates remain accessible while encoding.

Windows/WSL each pass37 MQTT programs plus3 Core Time programs, two Client Session
programs plus3 Time programs, two pure Session programs plus3 Time programs, the
runnable recovery example,4 generic root/leaf static/shared consumers and4 actual
packaged recovery suites. The explicit Time suite suffix isolates ignored build
output from archived evidence without changing MQTT's dependency closure. WSL
uses ASan/UBSan/leak, two TSan forms and five sanitized lifecycle repetitions.
Generated/features/API/docs/maturity and89 tooling tests pass per OS. Only Client
and Session change among9 MQTT members; all139 Core members agree with the parent.
Public/private measured layouts and fixed driver120/request-tail8 bytes agree.

Seven matched samples/one warmup compare current/parent ordinary Client, current/
parent Session and current Session/ordinary using identical benchmark sources and
one common parent Core. Original3% MAD/6% central-range,5% throughput and10% P99
bounds are retained, with every raw/noise-qualified failure recorded. Eight smoke
cases and two size profiles per OS are execution checks. External compiler load
is captured; these are local closed-loop observations, not broker/fixed-arrival/
native/72h production acceptance. Full performance acceptance remains false.

| OS | Comparison | Qualified throughput | Qualified P99 | Qualified bound failures T/P99 | Raw median failures T/P99 |
| --- | --- | --- | --- | --- | --- |
| windows | after | 2/4 | 1/4 | 0/0 | 0/0 |
| windows | optimized | 2/4 | 2/4 | 0/0 | 0/0 |
| windows | session | 2/4 | 1/4 | 0/0 | 0/1 |
| wsl | after | 4/4 | 1/4 | 0/0 | 0/0 |
| wsl | optimized | 4/4 | 2/4 | 0/0 | 0/0 |
| wsl | session | 4/4 | 2/4 | 0/0 | 0/0 |

The four-product goal remains active and all products developing. Continue with
alias sharing, TLS/Dial/Auth Session composition, durable/automatic recovery and
full performance/production gates. Development remains on D:/GIT/xrt master; no
branch operation or push. Evidence: extlibs/xmqtt/tests/results/shared-publish-20261110.json
and out/service-extensions/xmqtt-shared-publish-20261110 (C-backed artifacts).

## 2026-11-10: share aliased Client Session publication snapshots

Aliased QoS1/2 now uses the same one immutable canonical snapshot as unaliased
publication. A separate per-Client alias transaction gate serializes all alias
admissions, including QoS0 binds, from capture to successful queue publication.
Failed admission does not change planned bindings. Cancelling an admitted bind
keeps its plan; actual output restores the full topic when needed. Encoding
holds neither Client nor Session main gate, allowing close/ordinary producers
to progress. Entry deadlines include alias-gate wait. No new worker/runtime/timer.

Full-topic alias writes gather four immutable spans; compact output combines
only a small header and gathers two. User Property order is preserved. Shared
gathers refused by host pressure make one scalar attempt, then yield. Native
leases retain request/packet backing through actual retirement; one-byte writes
and alias-free PUBLISH/PUBREL recovery still pass. Independent request/packet
allocations, alias serialization, small header copies and native leases are costs.

Both OSes reproduce parent6e8be2b0 alias two-copy/current one-copy for QoS1/2,
with four MQTT5/311 unaliased controls unchanged. Tests add twelve gather/property/
chunk/lease-pressure variants, 125-to128 property-length expansion, ordered user
pairs, eight alias budget prefixes, admitted cancellation versus failed bind,
four concurrent mixed-QoS producers and a controlled close with alias holder/
waiter. A non-alias producer progresses while alias copying is held. Initial
test-local misleading-indentation compilation failure is retained and corrected.

Windows/WSL each pass37 MQTT plus3 Core Time root programs, two Client Session
plus3 Time programs, two pure Session plus3 Time programs, the recovery example,
four root/leaf static/shared generic consumers and four actual packaged suites.
WSL includes ASan/UBSan/leak, two TSan forms and five lifecycle repetitions.
Generated/features/API/docs/maturity and89 tools pass per OS. All139 Core members
and8/9 MQTT members match the parent; only Client changes. Public layouts and
request header104/base session tail8 remain unchanged. Alias state is32 bytes,
alias session tail40; driver size is120 plus the measured native mutex (one new
mutex per optional Client Session). Metadata allocation/initialization is bounded.

The new MQTT5 alias echo workload binds once then uses empty topics in both
directions. Six matched binaries share one parent Core archive and identical
current fixtures/drivers. Seven samples/one warmup preserve3% MAD/6% central range,
5% throughput/10% P99 bounds. All raw/qualified failures remain recorded. Ten
registered smoke cases and two size profiles per OS are execution checks. Local
closed-loop observations and external host load do not establish broker,
fixed-arrival, native Linux or >=72h production/performance acceptance.

| OS | Comparison | Qualified throughput | Qualified P99 | Qualified bound failures T/P99 | Raw median failures T/P99 |
| --- | --- | --- | --- | --- | --- |
| windows | after | 2/4 | 0/4 | 0/0 | 0/0 |
| windows | unaliased | 2/4 | 1/4 | 0/0 | 0/0 |
| windows | session | 2/4 | 1/4 | 0/0 | 0/0 |
| windows | alias | 1/2 | 0/2 | 0/0 | 0/0 |
| windows | alias_overhead | 1/2 | 0/2 | 0/0 | 0/0 |
| wsl | after | 4/4 | 2/4 | 0/0 | 0/0 |
| wsl | unaliased | 3/4 | 1/4 | 0/0 | 0/1 |
| wsl | session | 4/4 | 1/4 | 0/0 | 0/1 |
| wsl | alias | 2/2 | 1/2 | 0/0 | 0/0 |
| wsl | alias_overhead | 2/2 | 1/2 | 0/0 | 0/0 |

Legacy non-shared/QoS0 alias gather pressure and contended producer profiling
remain audit items. Continue with those costs, TLS/Dial/Auth composition, assigned
ids, automatic/durable recovery, broker interoperability and full acceptance.
The complete four-product goal remains active and all products developing.
Development remains on D:/GIT/xrt master; no branch operation or push. Evidence:
extlibs/xmqtt/tests/results/shared-alias-20261110.json and
out/service-extensions/xmqtt-shared-alias-20261110 (C-backed artifacts).
