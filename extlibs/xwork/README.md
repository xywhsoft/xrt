# xwork v2

`xwork` is the C agent-runtime layer between `xllm-session` and product hosts such as `xcode`.

The v2 mainline is deliberately small. It implements a reliable governed agent loop, including bounded read-only delegation, before the project grows remote workers or database-backed scheduling again. The previous broad implementation remains under `dev/v1` as design and implementation reference.

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
| `read_file` | Read UTF-8 text with line numbers and pagination. |
| `list_files` | Bounded directory listing with recursion and wildcard filtering. |
| `search_text` | Literal text search with path, wildcard, depth, and result limits. |
| `write_file` | Create, overwrite, or append files, optionally creating parents. |
| `replace_text` | Exact conflict-detecting text replacement. |
| `apply_patch` | Validate and transactionally apply multi-file create/replace/delete edits; each file is replaced atomically and earlier writes roll back on failure. |
| `exec_command` | Run a non-interactive shell command with cwd, timeout, exit metadata, bounded capture, and optional `expected_exit_codes` for negative tests. |
| `start_process` | Start a bounded-capture long-running process and return a stable process ID. |
| `poll_process` | Wait briefly and consume incremental stdout/stderr plus exit status. |
| `write_process` | Write text to process stdin or close stdin. |
| `stop_process` | Interrupt, terminate, kill, or kill the tree and release completed processes. |

Filesystem tools reject paths outside the configured workspace. `exec_command` starts inside the workspace, but it is a real shell and is not an OS sandbox. Hosts that do not fully trust commands should use `XWORK_APPROVAL_CALLBACK` or `XWORK_APPROVAL_READ_ONLY`.

Managed process IDs live for the lifetime of one `xwork_agent`. They intentionally are not serialized into the session checkpoint because OS process handles cannot be recovered safely after a host restart. Destroying the agent stops and releases every remaining managed process.

`exec_command` treats only exit code `0` as success by default. For a command that is expected to reject invalid input, pass a non-empty `expected_exit_codes` array such as `[1, 2]`; a normal exit matching the array is successful, while timeouts, cancellation, signals, and other abnormal exits remain failures.

Interrupted tool recovery is intentionally at-least-once: if a process stops after a side effect completes but before its result reaches the journal, that call is still pending and may be retried. Permission and hook checks run again. Hosts should favor idempotent operations and transactional `apply_patch` edits; managed OS processes cannot be reattached after restart. Recovery conservatively requires a fresh successful verification command before accepting completion.

## Read-only delegation

`xworkAgentRunReadOnlySubagent()` creates a fresh, non-persistent child session
from the parent session's token policy and clamps it to host-provided turn,
timeout, maximum-output, and final-byte budgets. The child borrows the parent's
model boundary, workspace, operation-context ancestry, and optional layered
memory. It receives only `read_file`, `list_files`, and `search_text`; `.git`
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
config.uDeadline = xrtDeadlineAfter(UINT64_C(120000000));
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

`config.pCancel` is borrowed and must outlive the agent. The agent creates a child token and passes it, together with the absolute monotonic `config.uDeadline`, to every normal and compaction model request. `xworkAgentCancel()` cancels only the child token. A cancelled scope returns `XWORK_RESULT_CANCELLED`; an expired deadline returns `XWORK_RESULT_TIMEOUT` and `XWORK_ERROR_TIMEOUT`, including during provider transport, retry backoff, MCP calls, or a long `exec_command`. Scoped commands use XRT's bounded process runner and terminate the process group when cancellation or a deadline wins.

If startup inspection reports an interrupted durable run, call `xworkAgentResume(agent, &result, &error)` before accepting another prompt. A normal `xworkAgentRun` refuses to append new user input while the durable tail is waiting for a model response or has unresolved tool calls.

For finer control, set `OnPermission` in `xwork_agent_config`. It receives an `xwork_permission_request` for every tool call permitted by the hard read-only ceiling and may return `XWORK_PERMISSION_ALLOW`, `XWORK_PERMISSION_DENY`, or `XWORK_PERMISSION_DEFAULT` to fall back to the approval mode. `OnHook` brackets permitted tool execution; before-tool denial prevents execution, while after-tool denial marks the result failed and explicitly warns that completed side effects are not reversible. Transactional file rollback remains the responsibility of `apply_patch`.

`bRequireVerificationAfterWrite` is enabled by default. When a run mutates the workspace and then tries to finish without a successful `exec_command` after the latest edit, xwork appends a durable verification prompt and continues. `uCompletionVerificationRetries` bounds repeated premature completion attempts without imposing a general Agent turn limit.

## Build and test

From the repository root on Windows with GCC available:

```bat
build.bat
```

On Linux or macOS (set `CC` to select GCC, Clang, or a cross compiler):

```sh
sh build.sh
```

Cross builds use `RUN_TESTS=0`; sibling locations and flags are overrideable through
`XLLM_DIR`, `XRT_DIR`, `BUILD_DIR`, `RELEASE_DIR`, `CFLAGS`, `LDFLAGS`, and `LIBS`.

The optimized warning-as-error suite covers operation-deadline propagation, bounded read-only delegation and internal-path isolation, a forced context compaction followed by a multi-turn workflow using the built-in tools, transactional multi-file editing and rollback, managed-process stdin/output, artifact spill, session persistence, interrupted parallel-tool recovery, duplicate-prompt rejection, a rejected workspace escape, dynamic registry replacement, and a real local MCP stdio handshake/discovery/call lifecycle.
