# xwork v3.11

xwork is the C runtime above xllm-session: runtime/agent/run lifetimes, immutable
catalogs, tool permissions, bounded tool results, asynchronous tasks, subagents,
MCP transports, durable recovery, scheduling, events and host-owned artifacts.

The public API is in `include/xwork/api.h`. ABI version is 9; event schema is 3.
This replaces the v2 public record layouts. Recompile hosts and SDK consumers
when adopting it; an existing v2 binary must not load a v3 library.

## Build and test

Run from the XRT repository root:

```text
python tools/build.py --compiler gcc --manifest extlibs/xwork/config/modules.json --suite xwork --no-examples
python tools/amalgamate.py --manifest extlibs/xwork/config/modules.json --check
```

Core XRT, xllm, xllm-session and the HTTP products remain separate libraries.
Single-header consumers select dependency features before XRT declarations and
provide implementations in dependency order: XRT, xhttp, xhttp-policy,
xhttp-client, xllm, xllm-session, xwork. See
`tests/single/test_single_xwork.c` for a complete example. Never embed another
old XRT implementation through a transport bridge.

`test_single_xwork_mdo` adds the migrated v3 runtime/recovery fault fixtures.
Its selectors include `--request-tokens`, `--result-files`,
`--utf8-tool-output`, `--artifact-publication`, `--user-image` and
`--resume-effects`. Run these from the repository root. The TLS/provider and
session migration/OOM fixtures are registered with their respective manifests.

## Runtime and ownership

An explicit runtime owns catalogs, tasks, MCP definitions, schedules, artifacts
and audit events. Agents retain that runtime and an immutable definition.
A run retains its agent until completion. Dynamic tools can retain callback
owners; publication either retains the complete candidate or leaves the old
catalog intact. Session/client bindings retain the host's aggregate owner when
the host supplies the owner callbacks. See [DESIGN.md](DESIGN.md).

Only one run mutates an agent/session at a time. Immutable snapshots remain
readable across subsequent catalog, task and schedule changes. Follow each
snapshot's retain/release contract instead of keeping borrowed internal entries.

## Tools and permission effects

The default catalog contains `read`, `ls`, `glob`, `grep`, `write`, `edit`,
`exec`, `spawn`, `status`, `wait`, `stdin` and `stop`. Registered subagent
and MCP catalogs add their own tools. Legacy names are canonicalized at the
runtime boundary; new hosts should use canonical names.

A v2 tool callback declares an effects mask, discovers its resources before
execution and returns bounded text/images through a callback-bound result
writer. Read-only trust does not follow from an arbitrary tool name or MCP
annotation. Independent tools may run concurrently only under their declared
resource and serialization rules. Results enter the session in original call
order even when execution completes out of order.

Process tools return structured JSON. `status` observes a task; `wait` waits for
completion. Host APIs independently wait for tasks, read their output using
byte cursors and release terminal entries. Polling status does not consume the
host's output cursor or release its task. The optional process-owner callbacks
pair native process lifetimes with a product host's ownership registry.

## Time and persistence

Product-facing absolute deadlines use monotonic microseconds. Persisted civil
timestamps use Unix microseconds. Conversion to native XRT signed relative
milliseconds occurs only at waits. `UINT64_MAX` means no operation deadline;
use the explicit initializers for public records. Do not pass native XRT civil
timestamps or millisecond waits directly into these product fields.

Sessions preserve the user's durable sequence and complete multimodal messages.
Effectful interrupted tools require an explicit recovery decision. Hosts may
opt recoverable reads into completed-result receipts. A receipt is published
before the completion event and ledger append; reopening verifies its exact
identity, file size and SHA-256 before reusing the completed result.

## Artifacts, result files and external request identities

An `xwork_artifact_store` retains an independently anchored directory selected
by the host. It does not expand normal workspace permissions. Immutable files
are flushed to exclusive temporary files and atomically published with a root-
relative hard link; unsupported filesystems fail instead of overwriting history.
Only the caller-owned temporary path is removed during failure cleanup.

`xworkToolResultWriterSaveArtifact` publishes full tool text while the model
receives a bounded preview/index. The existing `read` tool can page the current
session's read-only `@results/...` namespace. Byte offsets align with complete
UTF-8 scalars. See [result-files.md](docs/result-files.md).

`xworkToolResultWriterPrepareRequest` persists an opaque request identity for a
host-declared recoverable read. Service/account scope and exact call identity
bind the record. Damaged records or changed scope fail closed. It supports an
external service's idempotency protocol; it does not make arbitrary POSTs or
mutating tools safe to replay, nor guarantee exactly-once execution by itself.

## MCP and schedules

MCP supports stdio and Streamable HTTP with bounded discovery, retained dynamic
catalogs and explicit authorization callbacks. HTTP errors preserve their real
status and cannot accept an error-body `result` as success. Only a valid matched
protocol header rejection permits the designated routing refresh/retry.

Schedules support atomic replacement, rollback of unpersisted replacement,
explicit run-now triggers, misfire/overlap policy and durable task restoration.
Hosts own persistence and execution admission; cancellation remains cooperative.

## SDK export

`tools/export_agent_libraries.py --xserver <checkout>` generates the flat XS/TCC
layout from committed canonical sources. `--check` verifies the exact mapping.
The export changes guards, DLL annotations and include spellings, preserving
implementation bodies and leaving the host's XRT/HTTP bridges in place.
