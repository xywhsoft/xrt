# Service extensions progress

Target and rules: [development contract](service-extensions-plan.md).

## Current state

- All four final products are in development, not production accepted.
- P0 source/worktree isolation and execution contract are established.
- xredis binary commands, bounded RESP2/3 parsing, owned frames, one ordinary
  TCP/TLS async/sync request connection, bounded hostname/address dialing and
  explicit HELLO/AUTH/name/database setup are locally verified, including Redis
  8.0.5 interoperability. Initial client-certificate signing/authentication is
  locally verified. Atomic transaction execution is locally verified; identity-
  bound ticket management, other dedicated modes, reconnect/pools, HA and final
  performance acceptance remain pending.
- xcron five-field/seconds-six-field expression compilation and Gregorian
  UTC/civil matching/earliest search are locally verified. Named time zones,
  scheduling and durable recovery remain pending.
- P1 codec/expression/calendar foundations are locally verified. P2 now has a
  measured-workload TCP request/deadline/cancel/close slice; connection recovery,
  remaining modes and pinned overhead/performance acceptance remain pending.
- The xrpc prototype exposed a JSON lexical-scan gap. The shared parser now has
  a locally verified raw Scan API. RPC envelope decoding and encoding are locally
  verified; correlation, dispatch and transports remain pending.
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
| xredis | staged atomic MULTI/queue/EXEC | locally verified | exclusive FIFO, queue rejection/DISCARD, typed uncertainty/cancel/OOM, native static/shared consumers; WATCH sessions and performance acceptance remain pending |
| xredis | remaining modes, reconnect/pools, HA, tracking | pending | no claim |
| xrpc | bounded JSON-RPC 2.0 envelope decode | locally verified | Windows modular/single/example/trim/static/DLL; Ubuntu/WSL ASan+UBSan modular/direct-single, example/trim/static/shared; mixed batch/role/lossless-ID/duplicate/error/limits/OOM and deterministic property/mutation tests |
| xrpc | bounded atomic JSON-RPC 2.0 envelope encode | locally verified | Windows modular/single/examples/trim/static/DLL; Ubuntu/WSL ASan+UBSan modular/direct-single, examples/trim/static/shared; 10,000 independent wire properties, 500 complete-document resource oracles, 10,000 mutated raw inputs, zero-heap/OOM/aliases/atomicity |
| xrpc | correlation, dispatch, runtime, transports | pending | no claim |
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
