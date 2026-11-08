# xwork v3.11

xwork is the C runtime above xllm-session: runtime/agent/run lifetimes, immutable
catalogs, tool permissions, bounded tool results, asynchronous tasks, subagents,
MCP transports, durable recovery, scheduling, events and host-owned artifacts.

The public API is in `include/xwork/api.h`. ABI version is 10; event schema is 3.
This replaces the v2 public record layouts. Recompile hosts and SDK consumers
when adopting it; an existing v2 binary must not load a v3 library.

## Boundary

```text
xcode CLI / IDE host
        |
      xwork        agent loop, tools, policy, artifacts, compaction scheduling
        |
  xllm-session     context ledger, budgets, pruning, persistence, compaction transaction
        |
      xllm         one provider/model call, streaming normalization
        |
       xrt         HTTP/1.1, filesystem, subprocess, platform runtime
```

`xwork` is the only layer that executes model-requested tools. It borrows an `xllm_client` and an `xllm_session`; the host owns both and must keep them alive until the agent is destroyed.

## Current capabilities

- no hard agent-turn limit by default (`uMaxAgentTurns = 0`);
- repeated identical tool-batch and consecutive tool-failure loop guards;
- multiple tool calls per assistant response, executed in response order;
- assistant tool calls and tool results preserved as valid session pairs;
- automatic context-pressure checks and transactional summary compaction;
- explicit `xworkAgentCompact()` for a host-controlled safe compaction checkpoint;
- journal-backed atomic session checkpoints after prompts, assistant responses, tool batches, and compaction;
- explicit `xworkAgentResume()` recovery for interrupted model calls and partially completed tool batches, without appending a duplicate user prompt;
- streaming text/reasoning, model, tool, compaction, completion, and error events;
- cooperative cancellation;
- optional reference-counted operation context with inherited cancellation and monotonic deadlines;
- managed long-running processes with stable IDs, incremental output, stdin, stop, and cleanup;
- approval modes for automatic, callback-controlled, and read-only execution;
- structured per-tool permission decisions with risk and path/command/process resource metadata;
- before/after tool hooks over full, untruncated arguments and results;
- a completion verification gate that requires a successful command after the latest workspace mutation;
- workspace-contained filesystem path resolution;
- oversized tool output spill to `.xcode/artifacts`, with bounded head/tail context returned to the model;
- injectable model callback for deterministic offline tests.
- source-owned dynamic tool registration, enumeration, generation tracking,
  exact removal, and bulk removal without mutating a running agent;
- MCP 2025-06-18 stdio client support for initialization, paginated tool
  discovery, namespaced proxy registration, synchronous tool calls, structured
  results, cancellation notifications, deadlines, and protocol diagnostics.
- isolated read-only subagents with independent sessions, inherited cancellation,
  hard timeout/turn/output budgets, three inspection-only tools, depth-tagged
  events, protected `.git`/`.xcode` paths, and no session or artifact writes.

Built-in tools:

| Tool | Purpose |
| --- | --- |
| `read` | Read workspace files; text with line numbers and pagination, images (jpg/png/gif/webp/bmp) attached for viewing. |
| `write` | Create, overwrite, or append files; parent directories are created automatically and reported. |
| `edit` | Batch exact text edits in one atomic pass; 0 or ambiguous matches return candidate lines for self-correction. |
| `exec` | Run one command to completion; argv direct (no shell), env overrides, expected_exit_codes contract. |
| `spawn` | Start a background task (argv direct); bounded tail capture; completion announced at the next turn boundary. |
| `poll` | Incrementally read new output from a task and report its state. |
| `wait` | Block until any or all of the given tasks exit; timeout returns with still-running states. |
| `stdin` | Write to a process task's stdin, optionally close it. |
| `stop` | Stop a task: interrupt/terminate/kill/kill_tree for processes, cooperative cancel for subagents. |

Search and file listing are not builtin tools: the host deploys `rg`/`fd` (Git for Windows does NOT ship them) and the model reaches them through `exec`/`spawn` argv.

Filesystem tools reject paths outside the configured workspace. `exec_command` starts inside the workspace, but it is a real shell and is not an OS sandbox. Hosts that do not fully trust commands should use `XWORK_APPROVAL_CALLBACK` or `XWORK_APPROVAL_READ_ONLY`.

Workspace containment uses physical paths. Existing symlinks or junctions may
refer to other locations within the same workspace; external and dangling
targets are rejected. A new write target is checked against its nearest
existing parent, and a workspace configured through a link uses the physical
root. The check occurs when resolving each tool path; hosts must keep the
filesystem namespace stable during the following file operation. Read-only
subagents also check the physical target against protected control directories.

Managed process IDs live for the lifetime of one `xwork_agent`. They intentionally are not serialized into the session checkpoint because OS process handles cannot be recovered safely after a host restart. Destroying the agent stops and releases every remaining managed process.

`exec_command` treats only exit code `0` as success by default. For a command that is expected to reject invalid input, pass a non-empty `expected_exit_codes` array such as `[1, 2]`; a normal exit matching the array is successful, while timeouts, cancellation, signals, and other abnormal exits remain failures.

Interrupted tool recovery is intentionally at-least-once: if a process stops after a side effect completes but before its result reaches the journal, that call is still pending and may be retried. Permission and hook checks run again. Hosts should favor idempotent operations and transactional `apply_patch` edits; managed OS processes cannot be reattached after restart. Recovery conservatively requires a fresh successful verification command before accepting completion.

## Read-only delegation

`xworkAgentRunReadOnlySubagent()` creates a fresh, non-persistent child session
from the parent session's token policy and clamps it to host-provided turn,
timeout, maximum-output, and final-byte budgets. The child borrows the parent's
model boundary, workspace, and operation-context ancestry. It receives only
`read_file`, `list_files`, and `search_text`; `.git`
and `.xcode` path components are denied even through explicit reads.

Delegation depth is fixed at one. Child agents cannot register a delegation
tool, launch processes, mutate the workspace, save a session, or spill tool
output to artifacts. Oversized inspection results are truncated in memory.
Forwarded events carry `uAgentDepth`, `uDelegationId`, and `uParentAgentTurn`
so a host can record nested model/tool lifecycles without merging them into the
parent scope. The product host remains responsible for deciding when and how
many times the parent may invoke delegation.

## Minimal host setup

```c
xllm_client *client = /* configured provider client */;
xllm_session *session = /* new or loaded session */;
xwork_agent_config config;
xwork_error error;
xwork_agent *agent;
xwork_run_result result = {0};

xworkAgentConfigInit(&config);
config.pClient = client;
config.pSession = session;
config.pCancel = operation_cancel;
config.iTimeout = INT64_C(120000);
config.sWorkspaceRoot = "D:/GIT/project";
config.sSessionPath = "D:/GIT/project/.xcode/session.json";
config.eApprovalMode = XWORK_APPROVAL_AUTO;

agent = xworkAgentCreate(&config, &error);
if (agent && xworkAgentRun(agent, "Inspect the project and fix its tests.", &result, &error) == XWORK_RESULT_OK) {
    puts(result.sFinalText);
}

xworkRunResultUnit(&result);
xworkAgentDestroy(agent);
```

The product host should render `xwork_event` values and install its own approval callback where human confirmation is required.

## Dynamic tools and MCP

Hosts can add tools between agent runs with `xworkAgentRegisterTool()`. Each
definition has a copied `sSource`; use `xworkAgentToolAt()` for discovery and
`xworkAgentUnregisterToolsBySource()` to replace an entire provider registry.
`xworkAgentToolRegistryGeneration()` changes after every successful registry
mutation. Registration and removal are rejected while a run is active, so the
model request always sees a stable tool set.

For MCP stdio, create and connect an `xwork_mcp_client`, then call
`xworkMcpClientRefreshTools(client, agent, &error)`. Proxy names use the form
`mcp__<server>__<tool>` and their source is `mcp:<server>`. The MCP client owns
the subprocess and proxy callback state, so it must outlive every agent using
those tools. Destroy the agent before destroying the client, or explicitly
remove its source first.

The client launches a program and argv directly, never through a shell. Wire
messages are compact UTF-8 JSON-RPC lines; blank output, malformed JSON-RPC,
capture truncation, oversized messages, repeated pagination cursors, duplicate
remote names, and unexpected server exit are reported distinctly. MCP tool
annotations are untrusted by default. A host may opt into `readOnlyHint` only
for a trusted server; otherwise every proxy retains the configured conservative
effect used by approval and permission policy.

`config.pCancel` is borrowed and must outlive the agent. The agent creates a child token and passes it, together with the relative millisecond budget `config.iTimeout`, to every normal and compaction model request. `xworkAgentCancel()` cancels only the child token. A cancelled scope returns `XWORK_RESULT_CANCELLED`; an expired deadline returns `XWORK_RESULT_TIMEOUT` and `XWORK_ERROR_TIMEOUT`, including during provider transport, retry backoff, MCP calls, or a long `exec_command`. Scoped commands use XRT's bounded process runner and terminate the process group when cancellation or a deadline wins.

If startup inspection reports an interrupted durable run, call `xworkAgentResume(agent, &result, &error)` before accepting another prompt. A normal `xworkAgentRun` refuses to append new user input while the durable tail is waiting for a model response or has unresolved tool calls.

For finer control, set `OnPermission` in `xwork_agent_config`. It receives an `xwork_permission_request` for every tool call permitted by the hard read-only ceiling and may return `XWORK_PERMISSION_ALLOW`, `XWORK_PERMISSION_DENY`, or `XWORK_PERMISSION_DEFAULT` to fall back to the approval mode. `OnHook` brackets permitted tool execution; before-tool denial prevents execution, while after-tool denial marks the result failed and explicitly warns that completed side effects are not reversible. Transactional file rollback remains the responsibility of `apply_patch`.

`bRequireVerificationAfterWrite` is enabled by default. When a run mutates the workspace and then tries to finish without a successful `exec_command` after the latest edit, xwork appends a durable verification prompt and continues. `uCompletionVerificationRetries` bounds repeated premature completion attempts without imposing a general Agent turn limit.

## Memory conventions

xwork ships no memory subsystem. Agents that want persistent memory manage it
themselves with the ordinary file tools, following the notes-file convention:

- the agent keeps a `notes.md` (or a `.mdo/notes/` directory) in the workspace,
  structured with headings derived from the retired record taxonomy —
  tasks (in progress / blocked), facts, preferences, and task summaries;
- notes are plain, greppable, human-editable files: writes go through
  `write_file` / `replace_text` and land in the session journal like any other
  tool call, so audit needs no second ledger;
- retrieved notes, search results, and other externally sourced content should
  enter the prompt only through `xllmSessionAddReference()`, which wraps the
  material in an untrusted-reference frame so instructions hidden inside it
  cannot override host policy;
- secrets never go into notes; keep them in host-managed storage. Hosts can
  deny sensitive locations with `xworkPathIsProtected()` in a permission
  callback, the same check the readonly subagent applies to `.git`/`.xcode`.

The former `xllm-memory` library (record store, lexical search, sensitivity
gates) was removed from the tree; its framing text and protected-path check
survive as the two APIs above. Archive tag: `pre-xllm-memory-removal`.
Hosts may handle `XWORK_AGENT_EXTENSION_RECOVERY` to retain a safe failed
request-build, checkpoint or compaction boundary while resources recover.
Opt in explicitly with `bRecoveryHandled` and a delay. Cancellation/deadlines
always win. The runtime never repeats a tool to recover an allocation failure;
its uncertain effects are returned for inspection. Storage recovery does not
send its own notice back into the failed event sink. Unhandled hosts retain
their original bounded error behavior.

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

Hosts can implement `XWORK_AGENT_EXTENSION_RECOVERY` and explicitly opt in at
safe boundaries. `xworkAgentWaitRecovery` retains the caller's pending operation;
it never executes a tool or model. A policy chooses active backoff or retained
waiting, and can probe an external condition without repeating work. Stop and
explicit deadlines always interrupt waiting. Checkpoints, request construction,
compaction commits, and already completed results are retried independently.
Unknown journal outcomes still require explicit receipt recovery.
For a callback already inside the serialized event/hook lock, use the stage
`callback_quiet` or `callback_resource_quiet`: these retain cancellation and
policy waiting without recursively entering the same event sink.

Compaction first keeps the existing safe-prefix/split-turn planning. If a final
candidate has adequate content but missing headings, the session library can
normalize its format without dropping candidate text and applies the same
quality gate again. Short/oversized candidates cannot be padded into acceptance.
Hosts decide how to wait after rejected candidates; the library does not start
unlimited batches of paid summarization calls on its own.

`tools/export_agent_libraries.py --xserver <checkout>` generates the flat XS/TCC
layout from committed canonical sources. `--check` verifies the exact mapping.
The export changes guards, DLL annotations and include spellings, preserving
implementation bodies and leaving the host's XRT/HTTP bridges in place.
