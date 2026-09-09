# xllm Migration Guide

This page explains how to migrate from direct provider calls to xllm, from stateless calls to session, and from temporary RAG plumbing to xllm memory.

[Back to Documentation Center](README.en.md)

## Migration Principle

Do not migrate every capability at once. Move layer by layer:

1. Migrate the minimal chat call first.
2. Organize provider configuration into profiles.
3. Add session.
4. Add tool loop.
5. Add memory/RAG.
6. Add diagnostics and release gate last.

Each layer should have a minimal runnable verification program.

## Migrating from Direct Provider Calls

Previously you may have constructed HTTP requests directly:

```text
business code -> provider SDK/HTTP -> provider response
```

After migration:

```text
business code -> xllm request/turn -> adapter/profile -> provider response -> xllm_response
```

First migrate only single-turn text:

- Create `xllm_runtime`.
- Register the corresponding adapter.
- Create `xllm_profile`.
- Use `xllm_turn_add_user_text` to build input.
- Call `xllm_send_ex`.
- Read output with `xllm_response_get_text`.

Reference: [Minimal Chat Call](case/minimal-chat.en.md)

## Migrating from Hard-Coded Models to Profiles

If base URLs, API keys, and model IDs are scattered through your code, first centralize them into profiles.

Map configuration like this:

| Old Configuration | xllm Field |
| --- | --- |
| Provider name | `xllm_profile.sProvider` |
| Adapter type | `xllm_profile.sAdapter` |
| Base URL | `xllm_profile.sBaseUrl` |
| API key | `xllm_profile.tAuth` |
| Model | `tModels.tText.sModelId` or `tModels.tMultimodal.sModelId` |
| Capabilities | `tCaps.uFlags` |

Do not ignore capability flags. Many runtime errors come from profiles that do not declare real capabilities.

## Migrating from Stateless Calls to Session

If you manually concatenate history into prompts, migrate to `xllm_session`.

Migration steps:

1. Create `xllm_session_options`.
2. Set `sProfileId` and `sSystemPrompt`.
3. Create the session with `xllm_session_create`.
4. Put only the new user input into `xllm_turn` for each round.
5. Call `xllm_session_chat_ex`.
6. Configure compact behavior.

Reference: [Session Chat Case](case/session-chat.en.md)

## Migrating from Hand-Written Tool Protocols to Tool Loop

If you currently ask the model in a prompt to output JSON for tool calls, migrate to the xllm tool APIs.

Migration steps:

1. Declare each tool as `xllm_tool_def`.
2. Add `sToolId`, `sWireName`, `sDescription`, and schema.
3. Declare `XLLM_CAP_TOOL_CALL_OUT` and `XLLM_CAP_TOOL_RESULT_IN` in the profile.
4. Implement `xllm_tool_executor`.
5. Install the executor.
6. Use trace to observe the tool loop.

Reference: [Tool Loop Agent](case/tool-loop-agent.en.md)

## Migrating from Temporary RAG to xllm Memory

If you currently build RAG context with arrays, temporary strings, or an external vector store, migrate gradually to `xllm_memory`.

Migration steps:

1. Start with `XLLM_MEMORY_SCHEME_BUILTIN_SPARSE` and get minimal ingest/search working.
2. Write documents into `XLLM_MEMORY_SCOPE_KNOWLEDGE`.
3. Give records stable `sRecordId` and `sSourceUri`.
4. Search with `xllm_memory_search`.
5. Inject context with `xllm_memory_apply_search_to_request`.
6. Add workspace ingest/sync.
7. Consider embedding, hybrid retrieval, and watcher after the basic path is stable.

Reference: [Workspace RAG](case/workspace-rag.en.md)

## Migrating from Implicit Memory to Explicit Conversation Memory

If you previously relied on the model to "remember" user preferences, switch to explicit memory writes.

Migration steps:

1. Define what information can be stored long-term.
2. Add confirmation for user preferences and personal information.
3. Use `xllm_memory_ingest_text` to write summaries.
4. Later use typed APIs for tasks, facts, and preferences.
5. Search and inject memory before each request.
6. Provide deletion controls.

Reference: [Conversation Memory Case](case/conversation-memory.en.md)

## Migrating from No Diagnostics to Observable Calls

After migrating to xllm, add these early:

- `xllm_error`
- log callback
- trace callback
- memory diagnostics

They make provider, session, tool loop, and memory issues diagnosable.

Reference: [Diagnostics Introduction](guide/diagnostics-intro.en.md)

## Migration Risks

| Risk | Handling |
| --- | --- |
| Provider behavior changes | Verify with mock/smoke first, then run real provider probes. |
| Inaccurate capability declarations | Maintain a profile capability matrix. |
| Growing context size | Add context packing and compact. |
| Tool execution risk | Add permissions, approval, and auditing. |
| Memory noise | Limit writes, set scopes, and use stable sources. |
| Missing files in release bundle | Use downstream smoke verification. |

## Recommended Migration Checklist

1. Minimal chat works.
2. Profiles load from configuration and do not hard-code keys.
3. Every failure path can print `xllm_error`.
4. Session is only used for short-term history.
5. Tool executor validates arguments and permissions.
6. Memory records have `sRecordId` and `sSourceUri`.
7. RAG injection has a budget.
8. Diagnostic logs are redacted.
9. Release bundles pass artifact and downstream verification.
