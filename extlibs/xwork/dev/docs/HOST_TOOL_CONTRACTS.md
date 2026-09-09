# xwork Host Tool Contracts

This document records the stable v1 JSON request/response shape for builtin
local host tools. All responses are JSON text stored in
`xwork_tool_result.sOutputText`; `xwork_tool_result.sVisibleSummary` contains a
short human-readable status.

## Common Rules

- Path inputs are workspace-relative unless the host is configured otherwise.
- When `xwork_local_host_options.bEnforceFilesystemRoot` is true, filesystem and editor-buffer paths must stay under `sDefaultWorkingDirectory`.
- Failure responses include `ok:false`, an `error_kind`, and an `error` string where available.
- Bounded operations may return `truncated:true` or pagination metadata.
- If the runtime has `xwork_runtime_options::pReplayEngine`, host service calls
  are replay-aware. Record mode stores host request/result payloads in the
  replay cassette; strict/audit replay modes return recorded output without
  invoking the real host service.
- Side-effect-blocking record mode returns `XWORK_ERROR_PAUSED` before running
  filesystem/process/host service operations.
- Orchestrator execution may synthesize artifacts from successful builtin tool results.

## Filesystem

### `filesystem.read_text`

Request fields:

- `path`: required path.
- `offset_bytes`: optional byte offset.
- `max_bytes`: optional read limit.

Success response fields:

- `ok:true`
- `path`
- `text`
- `file_size_bytes`
- `offset_bytes`
- `bytes_read`
- `next_offset_bytes`
- `remaining_bytes`
- `truncated`
- `eof`

### `filesystem.write_text`

Request fields:

- `path`: required path.
- `text`: required content.
- `mode`: optional `overwrite`, `append`, or `create`.
- `create_dirs`: optional boolean.

Success response fields:

- `ok:true`
- `path`
- `mode`
- `bytes_written`

### `filesystem.list`

Request fields:

- `path`: optional directory path.
- `recursive`: optional boolean.
- `include_hidden`: optional boolean.
- `limit`: optional maximum item count.

Success response fields:

- `ok:true`
- `path`
- `recursive`
- `include_hidden`
- `count`
- `truncated`
- `entries`

Each entry includes at least path/name/type metadata and may include size and
mtime metadata when available.

### `filesystem.stat`

Request fields:

- `path`: required path.

Success response fields:

- `ok:true`
- `exists`
- `path`
- `type`
- `size_bytes`
- `mtime_unix`

### `filesystem.glob`

Request fields:

- `path`: optional root path.
- `pattern`: required glob pattern.
- `recursive`: optional boolean.
- `include_hidden`: optional boolean.
- `limit`: optional maximum match count.

Success response fields:

- `ok:true`
- `path`
- `pattern`
- `count`
- `truncated`
- `matches`

### `filesystem.mkdir`

Request fields:

- `path`: required path.
- `recursive`: optional boolean.
- `exist_ok`: optional boolean.
- `dry_run`: optional boolean.

Success response fields include `ok:true`, `path`, `created`, and `dry_run`.

### `filesystem.move`

Request fields:

- `path`: required source path.
- `target_path`: required destination path.
- `overwrite`: optional boolean.
- `recursive`: optional boolean.
- `create_dirs`: optional boolean.
- `dry_run`: optional boolean.

Success response fields include `ok:true`, source/target paths, overwrite
metadata, and `dry_run`.

### `filesystem.delete`

Request fields:

- `path`: required path.
- `recursive`: optional boolean for directories.
- `dry_run`: optional boolean.

Success response fields include `ok:true`, `path`, `deleted`, and `dry_run`.

### `filesystem.apply_patch`

The v1 builtin patch tool is an exact single-file replacement helper.

Request fields:

- `path`: required target path.
- `old_text`: required text to replace.
- `new_text`: required replacement text.
- `dry_run`: optional boolean.

Success response fields include `ok:true`, `path`, `dry_run`, and patch apply
metadata using `xwork.patch_apply_result.v1` / `xwork.patch_file_summary.v1`.
Conflict responses return a structured error when `old_text` is not found or is
ambiguous.

## Process And Terminal

### `process.exec`

Request fields:

- `command`: required command string.
- `cwd`: optional working directory.
- `stdin_text`: optional stdin content.
- `timeout_ms`: optional timeout.
- `timeout_stop`: optional `interrupt`, `terminate`, `kill`, or `kill_tree`.
- `env`: optional environment object/list depending on caller encoder.
- `max_output_bytes`: optional output cap.
- `merge_stderr`: optional boolean.
- `include_events`: optional boolean.
- `use_terminal`: optional boolean.
- `terminal_cols` / `terminal_rows`: optional terminal size hints.

Success response fields:

- `ok:true`
- `command`
- `cwd`
- `stdout`
- `stderr`
- `exit_code`
- `truncated`
- `stdout_truncated`
- `stderr_truncated`
- `stdout_byte_count`
- `stderr_byte_count`
- `timed_out`
- `cancelled`
- `stop_reason`
- `events` when requested

Terminal mode also returns `use_terminal`, terminal size metadata, and
`terminal_output_captured`. Captured terminal text is platform-dependent.

### Interactive Terminal Session Tools

Tools:

- `process.start_terminal`
- `process.terminal_read`
- `process.terminal_write`
- `process.terminal_resize`
- `process.terminal_stop`
- `process.list_terminals`

Terminal state responses use schema `xwork.terminal_state.v1`; inventory
responses use schema `xwork.terminal_inventory.v1`.

Important response fields:

- `session_id`
- `session_index`
- `session_name`
- `running`
- `done`
- `stdin_closed`
- `output_text`
- `output_bytes`
- `event_end_seq`
- `has_more_events`
- `event_stream_done`

`process.terminal_write` supports `include_state:true`, `after_seq`,
`max_events`, and `write_eof:true` so callers can write and immediately observe
post-write state.

`process.list_terminals` supports filtering/pagination by `session_name`,
`running`, `done`, `after_session_index`, and `limit`.

## VCS

### `vcs.status`

Returns a command/report style JSON result with repository status and dirty
metadata.

### `vcs.diff`

Request fields:

- `staged`: optional boolean.
- `limit`: optional output limit.

Returns diff text plus command artifact metadata when invoked through the
orchestrator.

### `vcs.log`

Request fields:

- `limit`: optional commit count.

Returns bounded log text.

### `vcs.branch`

Returns current branch and dirty state metadata.

## Editor Buffer

### `editor.open_buffer`

Request fields:

- `path`: required path.

Returns a buffer snapshot containing buffer id, path, dirty state, and text
metadata.

### `editor.apply_edit`

Request fields:

- `buffer_id` or `path`: target buffer.
- `old_text`: text to replace.
- `new_text`: replacement text.
- `dry_run`: optional boolean.

Edits are policy/approval controlled by the orchestrator and can be persisted as
buffer/file-change artifacts.
