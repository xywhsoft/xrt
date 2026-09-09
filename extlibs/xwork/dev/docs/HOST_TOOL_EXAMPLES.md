# xwork Host Tool Example Corpus

This file provides compact request/response examples for builtin host tools.
The field contracts are documented in `docs/HOST_TOOL_CONTRACTS.md`.

Responses shown here are representative. Paths, timestamps, process output,
terminal session ids, and VCS text are host-dependent.

## Filesystem

### `filesystem.read_text`

Request:

```json
{"path":"README.md","offset_bytes":0,"max_bytes":128}
```

Response:

```json
{"ok":true,"path":"README.md","text":"# xwork\n","file_size_bytes":4096,"offset_bytes":0,"bytes_read":128,"next_offset_bytes":128,"remaining_bytes":3968,"truncated":true,"eof":false}
```

### `filesystem.write_text`

Request:

```json
{"path":"notes/agent.txt","text":"hello\n","mode":"overwrite","create_dirs":true}
```

Response:

```json
{"ok":true,"path":"notes/agent.txt","mode":"overwrite","bytes_written":6}
```

### `filesystem.list`

Request:

```json
{"path":"src","recursive":false,"include_hidden":false,"limit":2}
```

Response:

```json
{"ok":true,"path":"src","recursive":false,"include_hidden":false,"count":2,"truncated":true,"entries":[{"path":"src/xwork_core","name":"xwork_core","type":"directory"},{"path":"src/xwork_host","name":"xwork_host","type":"directory"}]}
```

### `filesystem.stat`

Request:

```json
{"path":"xwork.h"}
```

Response:

```json
{"ok":true,"exists":true,"path":"xwork.h","type":"file","size_bytes":65536,"mtime_unix":1760000000}
```

### `filesystem.glob`

Request:

```json
{"path":"tests","pattern":"xwork_*_smoke.c","recursive":false,"limit":4}
```

Response:

```json
{"ok":true,"path":"tests","pattern":"xwork_*_smoke.c","count":4,"truncated":true,"matches":["tests/xwork_core_smoke.c","tests/xwork_host_smoke.c","tests/xwork_persistence_smoke.c","tests/xwork_profile_smoke.c"]}
```

### `filesystem.mkdir`

Request:

```json
{"path":"tmp/agent","recursive":true,"exist_ok":true,"dry_run":false}
```

Response:

```json
{"ok":true,"path":"tmp/agent","created":true,"dry_run":false}
```

### `filesystem.move`

Request:

```json
{"path":"tmp/agent/a.txt","target_path":"tmp/agent/b.txt","overwrite":true,"create_dirs":true,"dry_run":false}
```

Response:

```json
{"ok":true,"path":"tmp/agent/a.txt","target_path":"tmp/agent/b.txt","overwrite":true,"dry_run":false}
```

### `filesystem.delete`

Request:

```json
{"path":"tmp/agent/b.txt","recursive":false,"dry_run":false}
```

Response:

```json
{"ok":true,"path":"tmp/agent/b.txt","deleted":true,"dry_run":false}
```

### `filesystem.apply_patch`

Request:

```json
{"path":"README.md","old_text":"old line\n","new_text":"new line\n","dry_run":true}
```

Response:

```json
{"ok":true,"path":"README.md","dry_run":true,"schema":"xwork.patch_apply_result.v1","applied":false,"file_count":1,"hunk_count":1}
```

Conflict response:

```json
{"ok":false,"error_kind":"patch_conflict","error":"old_text was not found or was ambiguous"}
```

## Process And Terminal

### `process.exec`

Request:

```json
{"command":"echo xwork","cwd":".","timeout_ms":5000,"max_output_bytes":4096,"merge_stderr":false}
```

Response:

```json
{"ok":true,"command":"echo xwork","cwd":".","stdout":"xwork\n","stderr":"","exit_code":0,"truncated":false,"stdout_truncated":false,"stderr_truncated":false,"stdout_byte_count":6,"stderr_byte_count":0,"timed_out":false,"cancelled":false,"stop_reason":"exit"}
```

Expected timeout/error response shape:

```json
{"ok":false,"command":"long-running-command","cwd":".","stdout":"","stderr":"","exit_code":-1,"timed_out":true,"cancelled":false,"stop_reason":"timeout","error_kind":"timeout","error":"process timed out"}
```

### `process.start_terminal`

Request:

```json
{"command":"cmd /c echo ready","name":"agent-shell","terminal_cols":100,"terminal_rows":30}
```

Response:

```json
{"schema":"xwork.terminal_state.v1","ok":true,"session_id":"terminal-1","session_index":1,"session_name":"agent-shell","running":true,"done":false,"terminal_cols":100,"terminal_rows":30,"output":"ready\n"}
```

### `process.terminal_read`

Request:

```json
{"session_id":"terminal-1","max_bytes":4096}
```

Response:

```json
{"schema":"xwork.terminal_state.v1","ok":true,"session_id":"terminal-1","running":true,"done":false,"output":"","eof":false}
```

### `process.terminal_write`

Request:

```json
{"session_id":"terminal-1","input_text":"echo next\n"}
```

Response:

```json
{"schema":"xwork.terminal_state.v1","ok":true,"session_id":"terminal-1","running":true,"done":false,"bytes_written":10,"output":"next\n"}
```

### `process.terminal_resize`

Request:

```json
{"session_id":"terminal-1","terminal_cols":120,"terminal_rows":40}
```

Response:

```json
{"schema":"xwork.terminal_state.v1","ok":true,"session_id":"terminal-1","running":true,"terminal_cols":120,"terminal_rows":40}
```

### `process.terminal_stop`

Request:

```json
{"session_id":"terminal-1","stop":"terminate"}
```

Response:

```json
{"schema":"xwork.terminal_state.v1","ok":true,"session_id":"terminal-1","running":false,"done":true,"removed":true,"stop_reason":"terminate"}
```

### `process.list_terminals`

Request:

```json
{"include_done":false}
```

Response:

```json
{"schema":"xwork.terminal_inventory.v1","ok":true,"count":1,"sessions":[{"session_id":"terminal-1","session_name":"agent-shell","running":true,"done":false}]}
```

## VCS

### `vcs.status`

Request:

```json
{"path":"."}
```

Response:

```json
{"ok":true,"path":".","stdout":" M README.md\n","exit_code":0}
```

### `vcs.diff`

Request:

```json
{"path":".","cached":false,"stat":true}
```

Response:

```json
{"ok":true,"path":".","stdout":" README.md | 2 +-\n","exit_code":0}
```

### `vcs.log`

Request:

```json
{"path":".","limit":3,"oneline":true}
```

Response:

```json
{"ok":true,"path":".","stdout":"abc1234 update xwork\n","exit_code":0}
```

### `vcs.branch`

Request:

```json
{"path":"."}
```

Response:

```json
{"ok":true,"path":".","stdout":"main\n","exit_code":0}
```

## Editor

### `editor.open_buffer`

Request:

```json
{"path":"README.md","max_bytes":4096}
```

Response:

```json
{"ok":true,"path":"README.md","buffer_id":"README.md","text":"# xwork\n","file_size_bytes":4096,"truncated":false}
```

### `editor.apply_edit`

Request:

```json
{"path":"README.md","old_text":"old line\n","new_text":"new line\n","dry_run":true}
```

Response:

```json
{"ok":true,"path":"README.md","dry_run":true,"schema":"xwork.patch_apply_result.v1","applied":false,"file_count":1,"hunk_count":1}
```

## Diagnostics

Diagnostics are usually synthesized from `process.exec` results when the command
looks like a build or test command.

Representative artifact content:

```json
{"schema":"xwork.diagnostics.v1","source":"process.exec","status":"failed","diagnostic_count":1,"items":[{"severity":"error","message":"example compiler error"}]}
```
