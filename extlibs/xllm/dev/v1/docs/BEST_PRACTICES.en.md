# xllm Best Practices

This page collects the practices that most affect stability, maintainability, and safety when integrating xllm.

[Back to Documentation Center](README.en.md)

## Profile Management

Treat a profile as model connection configuration. Do not scatter it through business code.

Recommendations:

- Use stable `sId` values for each profile, such as `fast-text`, `tool-agent`, or `vision`.
- Fill in `sProvider`, `sAdapter`, `sBaseUrl`, and auth explicitly.
- Declare `tCaps.uFlags` accurately for each model.
- Do not hard-code API keys in source code or docs. Use environment variables or secure configuration sources.
- Rerun provider probes or smoke tests after provider capabilities change.

Continue with: [Provider and Profile Introduction](guide/provider-profile-intro.en.md)

## Request Construction

Use `xllm_turn` for simple text requests, and `xllm_request` for advanced control.

Recommendations:

- Create a new `xllm_turn` for each new user input, then call `xllm_turn_reset` after sending.
- Use `xllm_request` only when you need low-level control.
- Call `xllm_validate_request` before sending complex requests.
- Use response format for JSON/schema output instead of relying only on prompts.
- Specify MIME types for multimodal requests.

Continue with: [Request / Response Introduction](guide/request-response-intro.en.md)

## Error Handling

Prefer `_ex` APIs and pass an `xllm_error`.

Recommendations:

- On failure, record `eCode`, `iStatus`, `iHttpStatus`, and `sRequestId`.
- For capability errors, inspect `uRequiredCapability` and `sSelectedModel`.
- If reusing an error object in a loop, call `xllm_error_reset` after handling each failure.
- Call `xllm_error_free` at the end.
- Do not show raw provider errors directly to end users. Redact and format them first.

Continue with: [Diagnostics Introduction](guide/diagnostics-intro.en.md)

## Session Usage

Session keeps short-term history; it does not store long-term memory.

Recommendations:

- Use `xllm_session` for chat windows, IDE panels, and command-line assistants.
- Use `xllm_send_ex` or `xllm_chat_ex` for single-turn tasks.
- Configure `bEnableAutoCompact` and `uKeepRecentTurns`.
- Use compact for long conversations; older history can be summarized or truncated.
- Use session state export/import when restoring across processes.

Do not use session as a long-term user profile or knowledge base.

Continue with: [Session Introduction](guide/session-intro.en.md)

## Tool Loop

The model only proposes a tool call. The host performs the actual execution.

Recommendations:

- Give every tool a stable `sToolId`.
- Keep `sWireName` short and easy for the model to understand.
- Add `tInputSchema` in production.
- The executor must validate `sArgumentsJson`.
- File, command, network, and database tools need permission controls.
- Ask the user before executing high-risk tools.
- Keep tool results short and clear.
- Record tool calls, arguments, execution status, and result summaries.

Continue with: [Tool Loop Introduction](guide/tool-loop-intro.en.md)

## Memory Writes

Long-term memory should be small and accurate.

Recommendations:

- Put project knowledge into `XLLM_MEMORY_SCOPE_KNOWLEDGE`.
- Put user preferences, facts, and tasks into `XLLM_MEMORY_SCOPE_MEMORY`.
- Use a stable `sRecordId` for every record.
- Set `sSourceUri` for traceability and deletion.
- Use `bReplaceExisting = true` when updating the same record.
- Ask for user confirmation before writing preferences or personal information.
- Do not write every conversation sentence into long-term memory.

Continue with: [Conversation Memory Introduction](guide/conversation-memory-intro.en.md)

## Workspace RAG

Workspace indexing must filter before ingesting.

Recommendations:

- Enable hidden-file skipping and `.gitignore`.
- Exclude `.git`, `build`, dependency directories, and cache directories.
- Exclude keys, certificates, `.env`, and large binary files.
- Set `uMaxFileBytes`.
- Use stable `sRecordIdPrefix` and `sSourceUriPrefix`.
- Limit `uMaxHits`, `uMaxCharsPerHit`, and `uMaxTotalChars` when injecting retrieved context.
- Show source URI, chunk ID, and byte range in the UI.

Continue with: [Workspace Index Introduction](guide/workspace-index-intro.en.md)

## Context Packing

More context is not always better.

Recommendations:

- Current user input has the highest priority.
- Keep system prompts short and stable.
- Give memory search results a budget and deduplication.
- Keep only the tool result content needed for the current task.
- Use session compact for old history.
- Label retrieved context as reference material.

Continue with: [Context Packing Introduction](guide/context-packing-intro.en.md)

## Diagnostics and Redaction

Diagnostics should help locate problems without leaking data.

Recommendations:

- Enable detailed trace in development; disable or strictly redact it in production.
- Normal logs should record error codes, request IDs, and status codes, not full bodies.
- Diagnostic bundles should not contain API keys, Authorization headers, cookies, certificates, or `.env` content.
- When debugging tool loops, record tool name, argument summary, and execution status.
- When debugging memory, record record/chunk counts and source URIs.

## Release and Consumption

Before release, verify the package like a downstream consumer would.

Recommendations:

- Run `cmd /c .\build.bat verify-version`.
- Run `cmd /c .\build.bat singlehead`.
- Run `cmd /c .\build.bat release-bundle -VerifyCompile`.
- Run `cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile`.
- Run `cmd /c .\build.bat downstream-smoke`.
- Before a real release, run `cmd /c .\build.bat release-gate -RunDownstream`.

Continue with: [Release Gate Introduction](guide/release-gate-intro.en.md)

## Minimal Safety Checklist

- API keys are not in source code.
- Tool arguments are validated before execution.
- File tools are restricted to the workspace.
- High-risk tools require user confirmation.
- Memory writes have source URIs.
- Workspace indexing filters sensitive files.
- Diagnostic logs are redacted.
- Release bundles are consumed only after checksum verification.
