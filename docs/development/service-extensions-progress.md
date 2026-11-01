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
- xcron five-field/seconds-six-field expression compilation and Gregorian
  UTC/civil matching/earliest search are locally verified. Named time zones,
  scheduling and durable recovery remain pending.
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
  grammar. Negotiated rules, sessions and the client remain pending.
- Primary checkout changes remain outside this branch.

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
