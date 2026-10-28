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
  a locally verified raw Scan API. RPC envelope decoding is locally verified;
  encoding, correlation, dispatch and transports remain pending.
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
| common | raw JSON scan needed by xrpc | locally verified | exact borrowed scalar/key/container spans, lexical numbers, 10,000 zero-heap scans, limits/Unicode/control/OOM, Windows/WSL modular/single/shared-reader regression and external static/shared consumers; performance comparison remains unaccepted |
| xredis | binary command size/write | complete | Windows modular/single/O2/example/trim/static/DLL; Ubuntu/WSL O2 ASan+UBSan modular/single/static/shared; 10,000 randomized commands, overflow/aliasing, hot-path allocation counters, diagnostic OOM |
| xredis | incremental RESP2/3 event reader | complete | Windows modular/single/O2/example/trim/static/DLL; Ubuntu/WSL ASan+UBSan modular/single/example/trim/static/shared; all canonical split points, event pauses, 10,000 random arrays, 20,000 mutation pairs, limits, reentry, OOM and lifecycle quota |
| xredis | runtime, pools, HA, tracking | pending | no claim |
| xrpc | bounded JSON-RPC 2.0 envelope decode | locally verified | Windows modular/single/example/trim/static/DLL; Ubuntu/WSL ASan+UBSan modular/direct-single, example/trim/static/shared; mixed batch/role/lossless-ID/duplicate/error/limits/OOM and deterministic property/mutation tests |
| xrpc | encode, correlation, dispatch, runtime, transports | pending | no claim |
| xmqtt | MQTT 5.0/3.1.1, sessions, persistence, transports | pending | no claim |
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
parse/local-error separation and deliberately pending encoder/runtime behavior.
