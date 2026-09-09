# Tools, approvals and artifacts

>Status: First draft in Chinese, awaiting manual review.

The key risk in real agent development is not "whether the model can return tool calls", but that tool execution will have real side effects. xwork uniformly models tools, side effects, approvals, and products to prevent these logics from being scattered at the product layer.

## Tool layering

| Level | Description |
| --- | --- |
| Tool definition | Tool name, description, parameter schema and risk flags exposed to the model. |
| Tool executor | Execution function provided by xwork internally or by the caller. |
| Host service | The bridging layer that actually executes file, process, terminal, VCS, and editor actions on the product side. |
| Policy gate | Determine whether to allow, deny, or require approval before side effects occur. |
| Artifact | Leaves structured results that can be queried, persisted, and audited after execution. |

## Built-in host tool category

- `filesystem.*`: read, write, column directory, stat, glob, mkdir, move, delete, apply_patch.
- `process.*`: command execution and interactive terminal session.
- `vcs.*`: read-only version control queries such as status, diff, log, branch, etc.
- `editor.*`: editor buffer related bridge boundary.

## artifact type

xwork generates typed artifacts for common tool results:

- File content output artifact.
- patch artifact, including `xwork.patch_apply_result.v1` and `xwork.patch_file_summary.v1`.
- command artifact, including stdout/stderr, exit code, truncation flags, and I/O statistics.
- terminal state/inventory artifact.
- diagnostics report artifact.
- Generic `xwork.report.v1` reporting artifact.

## Common JSON requests and responses

Read file request:

```json
{"path":"README.md","offset_bytes":0,"max_bytes":4096}
```

A successful response contains:

```json
{"ok":true,"path":"README.md","text":"...","file_size_bytes":1234,"bytes_read":1234,"eof":true}
```

Execute command request:

```json
{"command":"git status --short","cwd":"D:/git/project","timeout_ms":10000,"include_events":true}
```

A successful response contains:

```json
{"ok":true,"command":"git status --short","stdout":"","stderr":"","exit_code":0,"timed_out":false,"cancelled":false}
```

Read terminal status request:

```json
{"session_id":"terminal-1","after_seq":0,"max_events":100}
```

A successful response uses `xwork.terminal_state.v1` and contains the session id, output window, event cursor and whether there are more events.

The failure response should contain `ok:false`, `error_kind` and optionally `error` to facilitate structured judgment by UI and replay.

## Design Constraints

- File, process, network and terminal operations must go through policy.
- A resumable run does not resume the live OS process handle or local terminal session.
- Persistence artifacts are audit and recovery trails and are not equivalent to active resource handles.
- The product layer can customize the host service, but it should not bypass xwork's strategy and event model.

## Related documents

- [API Reference Index](../api/README.md)
- [Host Tools API](../api/api-host-tools.md)
- [AI IDE Agent Example](../case/ai-ide-agent.md)
- [claw Autonomous Agent Example](../case/claw-autonomous-agent.md)
