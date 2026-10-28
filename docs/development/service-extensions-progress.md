# Service extensions progress

Target and rules: [development contract](service-extensions-plan.md).

## Current state

- All four final products are in development, not production accepted.
- P0 source/worktree isolation and execution contract are established.
- xredis binary-safe command encoding and the bounded RESP2/3 event reader are
  locally verified. Runtime requests and owned reply objects remain pending.
- xcron five-field/seconds-six-field expression compilation is locally verified.
  Calendar search, named time zones and scheduling remain pending.
- P1 continues with independently testable RPC/MQTT protocol leaves.
- The xrpc prototype exposed a JSON lexical-scan gap. The shared parser now has
  a locally verified raw Scan API. RPC envelope decoding and encoding are locally
  verified; correlation, dispatch and transports remain pending.
- xmqtt wire framing, variable integers, UTF-8 strings and binary fields are
  locally verified. All standard MQTT 5.0 property sections are also locally
  verified; complete packets, negotiated property rules and the client are pending.
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
| common | raw JSON scan needed by xrpc | locally verified | exact borrowed scalar/key/container spans, lexical numbers, 10,000 zero-heap scans, limits/Unicode/control/OOM, Windows/WSL modular/single/shared-reader regression and external static/shared consumers; performance comparison remains unaccepted |
| xredis | binary command size/write | complete | Windows modular/single/O2/example/trim/static/DLL; Ubuntu/WSL O2 ASan+UBSan modular/single/static/shared; 10,000 randomized commands, overflow/aliasing, hot-path allocation counters, diagnostic OOM |
| xredis | incremental RESP2/3 event reader | complete | Windows modular/single/O2/example/trim/static/DLL; Ubuntu/WSL ASan+UBSan modular/single/example/trim/static/shared; all canonical split points, event pauses, 10,000 random arrays, 20,000 mutation pairs, limits, reentry, OOM and lifecycle quota |
| xredis | runtime, pools, HA, tracking | pending | no claim |
| xrpc | bounded JSON-RPC 2.0 envelope decode | locally verified | Windows modular/single/example/trim/static/DLL; Ubuntu/WSL ASan+UBSan modular/direct-single, example/trim/static/shared; mixed batch/role/lossless-ID/duplicate/error/limits/OOM and deterministic property/mutation tests |
| xrpc | bounded atomic JSON-RPC 2.0 envelope encode | locally verified | Windows modular/single/examples/trim/static/DLL; Ubuntu/WSL ASan+UBSan modular/direct-single, examples/trim/static/shared; 10,000 independent wire properties, 500 complete-document resource oracles, 10,000 mutated raw inputs, zero-heap/OOM/aliases/atomicity |
| xrpc | correlation, dispatch, runtime, transports | pending | no claim |
| xmqtt | bounded wire framing and string/binary primitives | locally verified | Windows O2 modular/single and Ubuntu/WSL O2 ASan+UBSan modular/direct-single; all Unicode scalars, exhaustive two-byte UTF-8/VBI prefixes, full flag matrix, fragmentation/coalescing, limits/atomicity/zero-heap/OOM; real examples/trim/static/shared consumers |
| xmqtt | bounded MQTT 5.0 property-section codec | locally verified | all 27 ids / 7 types, scope/direction/repetition/range/auth/UTF-8 rules, typed error classes independent of diagnostic OOM; Windows/WSL modular/single/sanitizer, examples/trim/static/shared; 20,000 mutations, prefix/limit/atomicity/zero-heap tests |
| xmqtt | complete MQTT 5.0/3.1.1 packets, negotiated property rules, client, sessions, persistence, transports | pending | no claim |
| xcron | bounded immutable expression compiler | complete | Windows modular/single/O2/example/trim/static/DLL; Ubuntu/WSL ASan+UBSan modular/direct single, example/trim/static/shared; 111,630 range/step combinations, 10,000 generated expressions, 20,000 raw inputs, alias/offset/atomicity/allocation/OOM tests |
| xcron | calendar, time zones, scheduler, recovery | pending | no claim |

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
