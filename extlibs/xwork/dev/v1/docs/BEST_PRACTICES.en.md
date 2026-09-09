# xwork best practices

>Status: First draft in Chinese, awaiting review.

This article gives default recommendations for xwork integration, focusing on security, artifact, recovery and profile selection.

## Host Tool Policy is tightened by default

- Filesystem root enforcement is enabled by default.
- File writing, move, delete, and apply_patch are approved or dry-run by default.
- process.exec configures allow/deny pattern.
- destructive command intercepted before spawn.
- network defaults to deny-by-default, and is only allowed for explicit host allowlist.
- Remote workers cannot bypass workspace path, network and capability policies.

## Files and Patches

- Prefer using the dry-run path of `filesystem.apply_patch` to generate reviewable artifacts.
- The UI should display file/hunk/add/delete statistics for patch artifacts.
- Large file reading uses `offset_bytes` and `max_bytes`.
- Explicit `mode=overwrite/append/create` before writing, don't rely on implicit default.
- Paths outside the workspace should be denied by the host policy and not by the tool executor itself.

## Process and Terminal

- Set `timeout_ms` on `process.exec`.
- Set `timeout_stop` for long tasks and clarify the interrupt/terminate/kill/kill_tree strategy.
- Limit `stdin_text`, number of env entries, and output bytes.
- Use the terminal session tool when interaction is required, do not stuff interactive commands into the disposable `process.exec`.
- The persistent terminal artifact is only an audit record and cannot be used as a live session handle.

## Artifact Design

- The list page uses artifact summary and does not load the complete content by default.
- Use output class `file-content` for file contents.
- Use output class `file-change` or patch artifact for file changes.
- Use `xwork.terminal_state.v1` for terminal state.
- Use `xwork.diagnostics.v1` for diagnostics.
- Use `xwork.report.v1` for plan/progress/final.

## Recovery and idempotence

- Re-register runtime, workspace, tools, host services and profiles before recovery.
- The pending tool may be re-executed after recovery, and the tool should be as idempotent as possible.
- Use approval, checkpoint, replay or caller lock protection for non-idempotent side effects.
- It is normal behavior for multi-agent tasks of READY/RUNNING/BLOCKED to revert to PENDING.
- After a remote worker's in-flight assignment reverts to orphaned, it should be determined by product retry/cancel/manual inspect.

## Replay combination suggestions

- CI regression uses strict replay.
- Security audit uses audit replay to collect more divergence.
- Side-effect blocking can be enabled in record mode for high-risk side effects.
- JSON payload preferentially uses normalized JSON hash to avoid false positives caused by meaningless blanks.

## Profile selection

- AI IDE starts from the `xcode` profile by default, and then overrides the product UI approval policy.
- CLI autonomous agent starts from `xclaw` profile by default, and then tightens the network and command policy.
- Don't think of profile as the final security boundary; profile is just a collection of default configurations.

## Related documents

- [Policy / Approval API](api/api-policy-approval.md)
- [Host Tools API](api/api-host-tools.md)
- [Artifact API](api/api-artifacts.md)
- [Replay API](api/api-replay.md)
