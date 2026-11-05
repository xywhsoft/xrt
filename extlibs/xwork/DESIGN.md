# xwork v3 lifetime and threading contract

This document records the contracts enforced by the v3 object model. It is a
normative companion to `xwork.h`; historical `dev/v1` material is not a source
of current behavior.

## Object lifetimes

| Object | Scope | Owns | Retains / borrows |
| --- | --- | --- | --- |
| `xwork_runtime` | process | task table, catalog, MCP registry, scheduler definitions, artifact registry, and event/audit ledger | no product callbacks |
| `xwork_agent_definition` | immutable configuration generation | copied identity, model and policy strings; bounded defaults | nothing mutable |
| `xwork_agent` | session / named Agent instance | copied workspace paths, compatibility registry, lifetime cancel | retains runtime and definition; optionally retains one aggregate owner for client, session, callbacks and policy data |
| `xwork_run` | one prompt or resume | prompt, copied recovery decisions, child cancel, deadline, event override, result and error | retains agent; watches an optional borrowed caller cancel during its lifetime |

Each immutable tool-catalog entry optionally owns one dynamic callback-owner
reference. Catalog copies acquire independent references before publication;
catalog destruction releases them after the entry can no longer be invoked.
Owner callbacks are thread-safe, non-blocking reference operations and may not
re-enter xwork. If any retain fails, construction releases the complete
candidate and the previously published catalog remains current.

An agent retains its runtime and definition. A run retains its agent. Releasing
the host reference to an agent while a run is active delays physical cleanup
until that run releases it. Agent destruction prevents new runs but does not
invalidate an already claimed run.

Session bindings remain borrowed for lightweight compatibility. A production
host that permits runs, schedules, or background delegation to outlive its own
agent handle supplies the paired `OnOwnerRetain`/`OnOwnerRelease` callbacks.
Creation retains the aggregate owner before publication; final physical agent
destruction releases it after the borrowed dependencies can no longer be used.
Every delegated child takes its own reference, and a failed retain aborts child
creation without changing ownership.

The legacy `xworkAgentCreate` adapter creates a private runtime and immutable
definition, then uses the same implementation as `xworkAgentCreateWithRuntime`.
Its final agent release also releases the private runtime, preserving the old
behavior of stopping remaining tasks.

## Run serialization

An agent has one state mutex and at most one active run. `xworkRunStart` claims
the agent synchronously before starting its worker. A second run, compaction, or
legacy external run window fails without touching the session. The run worker
releases the claim before publishing its terminal state.

`xworkRunWait` may be called repeatedly. Each successful terminal wait returns
an owned copy of `xwork_run_result`; the caller releases it with
`xworkRunResultUnit`. A wait deadline only stops the wait. It does not cancel the
run. `xworkRunCancel` idempotently requests cooperative cancellation. Run destruction asks
an active run to cancel and releases the caller's reference; a worker reference
keeps the object valid even if destruction occurs from an event callback.

The run cancel is a child of the agent lifetime cancel. An optional caller token
is watched only for the run lifetime and forwards cancellation to that child.
The effective deadline is the earlier of the agent and run deadlines. Provider,
MCP, process, and loop boundaries consume the effective run scope.

## Durable recovery boundary

The durable session tail distinguishes a missing tool result from a completed
one, but a process crash can occur between an external side effect and the
journal checkpoint. No local journal can provide exactly-once semantics across
that boundary without cooperation from the external system. xwork exposes the
ambiguity instead of silently replaying it.

Recovery inspection deep-copies every unresolved call and classifies it against
the agent's pinned tool-catalog generation. A known descriptor is automatically
retriable only when its effect set is a subset of `READ`; missing tools and all
write, process, network, external-service, secret, schedule, and delegation
effects require a decision. The immutable snapshot remains readable after the
agent is destroyed.

Resume constructs an owned plan and validates the entire decision set before it
executes the first call. IDs must be present and unique in both the durable
batch and the caller's decisions. A stale decision, duplicate decision, changed
pending call, invalid ABI record, or unresolved effectful call fails with no
prefix execution. The run claim pins the agent catalog while the plan executes.

`RETRY` accepts at-least-once behavior and goes through the normal permission,
hook, cancellation, deadline, truncation, artifact, and event path.
`RECORD_UNCERTAIN` performs no tool or permission callback; it writes a failed
tool result that tells the model to inspect external state. Each result passes
the configured session-save boundary before the next pending call. The runtime
audit stream records a required event or a resolved event with the action,
automatic-retry bit, and uncertainty bit.
The completion-verification gate remains conservative after any resumed run.

The generic xllm-session executor intentionally has no xwork policy metadata.
Hosts that drive `xllmSessionRunWithTools` directly must preflight pending calls
themselves instead of using its null-prompt continuation as crash recovery.

## Runtime task storage

Task entries are individually allocated, reference-counted objects. The runtime
stores pointers, so growing or compacting its table cannot move an entry while a
capture or delegation worker uses it. Lookup takes a temporary entry reference;
removal drops the table reference; final release performs process/delegation
cleanup exactly once.

Construction holds a private caller reference and keeps the entry undiscoverable
until its process capture or delegation worker is fully initialized. Reclaiming
completed entries detaches them while holding the runtime lock and performs
their potentially blocking cleanup after releasing that lock.

Task IDs are monotonic within a runtime and remain addressable after the
launching agent is destroyed. This permits a replacement agent to poll or stop a
background process in a later user turn. Runtime finalization detaches the table
under its lock, then reaps entries outside the table mutation path.

Process, agent, and scheduled entries use the same explicit state enum. Each
entry records owner agent/run, optional parent, owner session, revision, and Unix
creation/start/end timestamps. Process status is refreshed under the entry lock;
agent workers and scheduled-task APIs publish through that same lock. Invalid
scheduled transitions leave the entry unchanged.

Task snapshots deep-copy strings and sort by stable ID. They are immutable and
reference-counted, so callers can render them after a task is released. Output
reads are owned byte/string copies with independent absolute cursors and explicit
dropped-prefix metadata. Wait observes the state machine without consuming
output. A fixed per-task event window records creation, state transitions,
cancellation requests, restoration, and notice consumption under the same
revision counter. Event reads resume after a caller revision and report history
loss rather than silently presenting a discontinuous stream. Cancel is
idempotent for terminal work; release rejects pending or running work.

Persistence restores metadata rather than executable resources. A pending
scheduled definition remains pending, while a process, subagent, or executing
scheduled callback that was live at shutdown becomes `LOST`. Terminal states
remain terminal. Duplicate IDs and malformed records fail before publication.
Recurrence, timezone, due-time selection, and misfire policy belong to the
scheduler layer and are intentionally outside this task-table contract.

## Runtime scheduler

The scheduler is runtime-owned but host-driven. Registration copies a complete
definition without starting a thread or performing I/O. The host passes an
explicit Unix-microsecond instant to `xworkRuntimeClaimDueSchedule`. One call
claims at most one occurrence and creates a RUNNING `XWORK_TASK_SCHEDULED`
entry. The host uses the claim's copied agent profile and input to run its normal
agent path, then completes the task with the existing scheduled-task API. The
returned next-wake value is zero when no clock wake is required. In particular,
a queue-one definition blocked by an active task is reconsidered when the host
observes that task's terminal transition.

Minute and hour frequencies are elapsed-time intervals. Daily and weekly
frequencies retain the anchor timestamp's local wall-clock fields. UTC and fixed
offset conversion are deterministic. System-local conversion follows the host
timezone database: a nonexistent DST wall time is skipped, and an ambiguous wall
time uses the configured earlier/later fold. Weekly masks use Sunday as bit zero.
Equal due instants use the schedule ID as a stable tie breaker.

Misfire policy is explicit. `SKIP` advances beyond the supplied time,
`RUN_ONCE` coalesces a backlog to its latest occurrence, and `CATCH_UP` exposes
oldest occurrences up to the configured and hard limit before skipping the rest.
Overlap policy either skips occurrences while the concurrency bound is full or
retains one cursor for later. Cursor and counters advance only after the unified
task has been created and entered RUNNING state, so a task-table allocation
failure is retriable without losing the occurrence.

`xwork_schedule_info` is the persistence record: it includes the complete copied
definition, definition generation, next occurrence, last claim, and diagnostic
counters. Restore validates the cursor against the recurrence and copies the
record. Executions that were active across restart are recovered separately as
`LOST` task metadata and are not counted as live overlap. Registry entries keep
stable addresses until runtime destruction because public info and claim strings
are borrowed. Unregister therefore leaves a tombstone, and `uMaxSchedules` is a
bound on lifetime registrations. The host must serialize borrowed data before
releasing its runtime reference.

The scheduler mutex protects definition registration, cursors, counters, and
active-task ID sets. Scheduler operations may acquire the runtime task-table lock
and then an individual task-state lock while holding it. Task paths never acquire
the scheduler mutex, which preserves one lock order. No product callback or agent
execution occurs while the scheduler lock is held.

## Runtime artifacts and event/audit replay

The artifact registry is runtime-owned and append-only. A writer reserves one
lifetime slot and monotonic ID under the observability mutex, performs directory
creation, atomic file replacement, and SHA-256 hashing without that mutex, builds
all metadata, then publishes one complete entry. Any failure before publication
releases the reservation and deletes the just-written file. The registry cap
counts lifetime publications because metadata addresses remain stable for
borrowed `xwork_artifact_info` values. Runtime destruction releases metadata and
leaves the ordinary workspace files in place.

Artifact metadata joins source, agent, run, task, media type, size, digest, and a
workspace-relative path under one stable ID. Public reads copy the absolute path
while holding the registry lock, open the file after unlocking, require its type
and size still to match the publication, and return an owned bounded chunk. A
caller can compare the published digest when end-to-end content verification is
required. Missing or size-changed data fails as I/O instead of returning a
misleading partial artifact.

The event/audit ledger uses the same observability mutex and a separate bounded
ring. All agent events receive a schema version, monotonic runtime event ID, Unix
timestamp, and execution lineage before their existing callback runs. Artifact
publication and unified task state revisions publish directly into this ledger.
Tool events additionally retain the declared effect bitset, whether an effect was
applied, and any artifact correlation ID. Live callbacks continue to receive the
original ephemeral payload; replay entries deep-copy every string and cap text
with a head/tail marker.

Retention is bounded by record count, total copied bytes, and per-event text
bytes. Eviction and events that cannot be retained increment a cumulative drop
counter while still consuming their global ID, so later snapshots expose an
observable cursor gap. A snapshot retains immutable event entries and therefore
outlives the runtime. It reports its next cursor, latest assigned ID, dropped
count, and whether the requested interval has a discontinuity.

No filesystem I/O or product callback runs under the observability mutex. Task
paths may take a task-state lock and then the observability mutex; artifact and
event readers never acquire a task, scheduler, agent, or callback lock. This
one-way order keeps replay publication independent from runtime control paths.

Background delegation retains its parent agent because it still borrows the
parent session configuration, model boundary, tool callbacks, and policy data.
It also retains the exact parent tool-catalog and subagent-roster generations
selected at launch, so an idle parent may publish a later generation without
racing a background composition.
The delegation worker holds a separate task-entry reference. A completed worker
may release the last agent/runtime reference from its own thread; XRT thread
objects are detachable, so final cleanup never waits on the current thread.

## Tool catalog generations

The runtime owns one current immutable `xwork_tool_catalog`. Runtime mutations
deep-copy descriptor strings into a complete candidate and swap one pointer
under the runtime mutex. Each catalog is reference-counted, so a snapshot stays
readable after later publication and even after runtime destruction.

An agent pins one catalog generation. A new agent starts from the runtime's
current generation and may publish private compatibility generations while it
is idle. Runtime updates do not silently rewrite existing agents;
`xworkAgentAdoptRuntimeToolCatalog` is the explicit between-run boundary. Agent
mutations and adoption share the agent state mutex with run claim, so an active
run always observes one fixed generation. Adoption replaces the complete agent
catalog; hosts reapply any agent-private source overlays explicitly.

`xworkRuntimeReplaceToolsBySource` and
`xworkAgentReplaceToolsBySource` validate collision and duplicate rules, copy
all retained and replacement entries, then publish once. Any validation or
allocation failure or configured catalog-limit rejection leaves the previous
generation current. MCP refresh uses
this batch path and retains retired proxy storage until client destruction, so
older agent generations never point at a freed proxy while the documented MCP
client lifetime is respected.

## Runtime MCP registry and lazy schema exposure

The v3 runtime owns registered MCP server records and their stdio or Streamable
HTTP transports. Registration is a cold operation: it deep-copies configuration
but performs no I/O. Server objects have stable addresses until runtime
destruction. Disable or unregister changes availability and stops the transport,
while retained proxy
callbacks remain valid and fail closed. This avoids binding a process-wide
integration to one short-lived session agent.

`xworkRuntimeReplaceMcpServersBySource` prepares and validates a complete
source-owned batch before taking the runtime registry lock. Identical records
reuse their client, connection, and schema generation. Changed and removed
records are locked before publication, retired together, stopped after the
registry lock is released, and retained fail-closed until runtime destruction.
An allocation, collision, active request, or limit failure preserves the
previous batch. `xworkRuntimeDisconnectMcpServer` closes only the current
transport; it preserves the enabled definition so a later selected operation
can reconnect it.

An enabled runtime server contributes only `tool_search` and `tool_load` to a
new agent. Summary search never starts a server. Server-specific search performs
the first connection and caches only a compact name/description index;
`tool_load` adds one complete schema by publishing a new immutable agent catalog
generation. The current batch plan retains the prior catalog, so loading inside
a run is safe and the new schema becomes visible at the next model boundary.
Allow/deny filtering occurs before caching or exposure.

MCP `2026-07-28` uses `server/discover`, required per-request `_meta`, complete
result types, cache metadata, and stateless Streamable HTTP POST routing headers.
Malformed `x-mcp-header` annotations exclude one tool; primitive parameter
headers are derived from the cached schema. A server-declared `HeaderMismatch`
is the only tool-call error that refreshes the cache and retries once.
`2025-11-25` uses the distinct `initialize`/`initialized` lifecycle and, on
HTTP, a negotiated session, bounded SSE GET resume, cancellation notification,
and DELETE termination. Runtime definitions default to the modern era; the
compatibility client defaults to the final legacy revision. The implementation
does not infer an era from a version string or silently fall back across wire
contracts. Registration never initializes the shared HTTP engine. Startup and
discovery use the independent startup timeout; subsequent list and call requests
use the request timeout. HTTP response bytes, SSE resumes, tools, and headers all
have explicit bounds.

## Subagent roster generations and scopes

An agent owns one immutable `xwork_subagent_catalog`. Complete replacement and
the corresponding `agent` tool catalog are prepared before publication and then
swapped together under the agent state mutex. A run claim rejects roster
mutation. The `agent` executor retains both generations before releasing that
mutex, which gives foreground and background delegation a coherent roster/tool
pair without holding a lock across model or tool callbacks.

Definitions own copies of identity, prompt, model, reasoning, tool, and skill
metadata. Tool filtering preserves parent catalog order and rebinds built-in
callbacks to the child agent. The selected set must be contained by the
definition effect bitset. Read-only mode further clamps that bitset to `READ`;
the child receives a delegation tool only when the definition also permits
delegation, names `agent`, includes `AGENT_DELEGATION`, and has remaining depth.
Permission, approval, and hook callbacks continue through the parent policy
chain, so a definition cannot widen an existing host decision.

Every root agent creates one reference-counted delegation scope with maximum
active children, maximum depth, and monotonic delegation IDs. Descendants share
it. Entering a foreground or background delegation consumes one active slot
until the child finishes. A definition may narrow the remaining branch depth;
that narrowed value is copied into the child and cannot be widened by a later
descendant definition. The child cancel is an XRT child of the current operation
cancel and its deadline is the earlier of the operation and definition
deadlines. The background task entry owns that cancel while the worker borrows
it, and entry cleanup waits for the worker before destroying it.

Children share the process runtime but receive a fresh session and immutable
agent definition. Context, input, output, turn, time, and final-report limits may
only tighten parent limits. Child events preserve the originating parent run,
delegation ID, parent turn, and depth. Only the bounded final report crosses
back into the parent's tool result; child messages and reasoning stay in the
child session.

Catalogs own descriptor strings. Execute functions and callback user data are
borrowed; their source owner must outlive every runtime, agent, or explicit
snapshot containing them. Enumeration through a catalog snapshot is safe on
any thread and returns strings borrowed from that snapshot.

## Tool execution contract

`uEffects` is a conservative bitset rather than an ordered severity. Policy
evaluates the complete set, and read-only mode accepts only descriptors whose
set is contained by `READ`. Every descriptor with another effect must provide
`OnDescribePermissions`. That side-effect-free callback copies typed resources
through `xwork_permission_resource_writer`; it cannot give the host borrowed
module memory. Permission requests expose all resources at once, and malformed
resource discovery or an unknown policy decision fails closed. A request also
identifies its request, agent, run, tool call, and immutable catalog generation,
and borrows the effective cancellation token and deadline. The tool callback
receives the same execution scope. Zero is the compatible no-deadline value for
a manually initialized public tool context.

`OnExecuteV2` similarly emits text, status, and an optional image through an
opaque host writer. Each write copies synchronously, the descriptor or agent
byte ceiling is checked before growth, and finalization requires both a status
and nonempty text. A descriptor selects exactly one of the legacy executor and
the writer executor. This keeps the legacy built-ins usable while giving TCC
modules an allocator-neutral ABI. `XWORK_ABI_VERSION` is 6; the current version
also includes versioned task, schedule, observability, subagent, and aggregate
agent-owner lifetime records.

## Deterministic tool scheduling

Before a multi-call response executes, xwork snapshots the immutable catalog
and evaluates every side-effect-free permission descriptor once. Path resources
are resolved against the workspace for comparison while permission callbacks
still receive the descriptor's original value. Planning scans the model's call
order and assigns each call to the earliest wave after all conflicting earlier
calls. Reads of the same path do not conflict; a write conflicts with the same
path and every ancestor/descendant path; process, schedule, and delegation
effects are global within their kind. An explicit serial group always orders
its members. Unknown tools and resources that cannot be normalized are planned
conservatively.

Tools are batch-serial against every other call by default. A host opts a
descriptor into resource-aware concurrency with
`xworkAgentConfigureToolConcurrency` or the runtime equivalent. Built-in tools
are opted in; global effects and resource conflicts still serialize them.
`uMaxParallelTools` bounds each wave (default 4, hard limit 64). Permission,
approval, hook, and event callbacks are serialized through a callback lock;
opted-in tool bodies may execute concurrently.

`xworkAgentPlanToolBatch` exposes the immutable sequence/wave/resource plan for
tests and diagnostics. Execution stores each outcome at its original sequence.
`xllm-session` appends synchronous results only after the batch returns and in
the assistant response's original tool-call order, regardless of worker finish
order. Long-running tools continue to return a stable task ID instead of
occupying a scheduler worker.

## Native discovery contract

`read`, `ls`, `glob`, and `grep` form the portable read-only inspection set.
The latter three never spawn an external program. Directory and result ordering
uses raw UTF-8 bytes on every platform, emitted paths use `/`, glob matching is
case-sensitive by default, and `ignore_case` performs explicit ASCII folding.
`?` consumes one valid UTF-8 scalar, `*` stays within a segment, and only a
complete `**` segment crosses directories.

Every input path first passes the lexical workspace boundary and then resolves
physically. An existing root that resolves outside the workspace is rejected;
recursive scans do not follow links or special entries. This is a read boundary,
not an operating-system sandbox against a concurrently hostile filesystem.
Hidden paths require an explicit option. Recursive scans skip common repository
metadata, dependency, and cache directories, while a caller may inspect such a
directory directly as the requested root.

Traversal depth, visited entries, visited files, file bytes, aggregate grep
bytes, pattern bytes, previews, and returned results all have fixed ceilings.
Text search validates UTF-8 and rejects NUL-containing or known binary files.
Matching covers the complete line before a bounded preview is centered on the
first match. Cancellation and the effective monotonic deadline are checked at
directory, file-read-chunk, and line boundaries. Partial discovery caused by a
documented scan ceiling is marked in the result; allocation, I/O, cancellation,
deadline, and regex execution failures remain distinct runtime results.

## Threading boundary

- runtime task-table structure is guarded by the runtime mutex;
- runtime catalog publication is guarded by the runtime mutex; immutable
  generations need no read lock;
- agent run ownership is guarded by the agent state mutex;
- agent catalog adoption/publication uses the same state mutex as run claim;
- subagent roster and its `agent` tool publish under that same state mutex;
- a delegation retains a coherent roster/tool pair and shares a separately
  locked tree concurrency/depth scope;
- run state/result publication is guarded by the run mutex and condition;
- each task has a state lock; process capture streams retain their independent
  capture lock;
- artifact metadata and the event/audit ring are guarded by the observability
  mutex; immutable event snapshots retain their entries without that lock;
- callbacks execute without runtime, agent-state, or run locks held;
- permission, approval, hook, and event callbacks are serialized by the agent
  callback mutex; opted-in tool executors may run concurrently;
- `xllm_session` remains single-writer: the one-active-run rule is the xwork
  enforcement boundary.

Reference operations require the caller to hold a live reference. Hosts may
call `xworkRunWait`, `xworkRunCancel`, and `xworkRunGetInfo` concurrently while
the run remains alive. Creation, start, and destruction are lifecycle
operations and must be serialized by the host.

Task readers use `xworkRuntimeTaskSnapshot` for immutable metadata and
`xworkRuntimeReadTaskOutput` for owned cursor-based chunks. Legacy
`xworkAgentToolAt` still follows its owning-thread rule; cross-thread tool readers
use `xworkAgentToolCatalogSnapshot` and enumerate that owned generation.
