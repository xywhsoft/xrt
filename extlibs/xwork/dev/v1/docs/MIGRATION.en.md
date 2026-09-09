#xwork migration guide

>Status: First draft in Chinese, awaiting review.

This article explains how to migrate to xwork from using the xllm tool loop directly or an in-product custom tool system.

## Migrate from using xllm Tool Loop directly

### Before migration

```text
product code
  -> xllm request
  -> parse tool call
  -> execute product tool
  -> append result
  -> next xllm request
```

FAQ:

- Tool approval is decentralized.
- Tool results have no unified artifacts.
- Run status and recovery require product self-maintenance.
- Multiple Agents, remote workers and replay are difficult to reuse.

### After migration

```text
product code
  -> xwork runtime/workspace/run
  -> register tools / host services
  -> xwork_run_execute
  -> approval/artifact/checkpoint/persistence
```

step:

1. Use `xwork_runtime` to hold xllm runtime or bootstrap options.
2. Register the project root directory and memory with `xwork_workspace`.
3. Map the existing tool call schema to `xwork_tool_def`.
4. Migrate the actual execution logic of the tool to host service or tool executor.
5. Replace scattered manual confirmation logic with policy/approval.
6. Change tool output to artifact.
7. Access file persistence or custom persistence backend.

## Migrating from in-product custom tool systems

### Tool definition

Break down the product tools into:

- tool id: a stable name given to the model.
- host service kind: filesystem/process/vcs/network/editor and other side effect categories.
- operation id: host service internal operation.
- Side effect: risk categories such as read-only, workspace write, process, network, etc.
- approval mode: default, never, always, on demand.

### Execution logic

Existing tool actuators can be retained, but should be accessed in the following ways:

- `xwork_tool_exec_fn` / `xwork_tool_exec_ex_fn`
- `xwork_host_service`
- `xwork_runtime_invoke_host_service_ex()`

Long-running operations should check cancel/interrupt context.

### Output logic

Convert product private output to:

- `xwork_tool_result`
- `xwork_artifact`
- `xwork_artifact_summary`
- `xwork.report.v1` / `xwork.diagnostics.v1`

### Recovery logic

Do not restore the live handle. During recovery, re-create the runtime, workspace, tool registry, and host services, and then load the run snapshot or task graph/control plane snapshot.

## Migration order suggestions

1. First connect to runtime/workspace/run.
2. Connect one or two more read-only tools.
3. Access policy/approval.
4. Access artifact summary.
5. Access persistence.
6. Access async cancel.
7. Expand to multi-agent, remote worker or replay.

## Related documents

- [First xwork Program](guide/first-xwork-program.md)
- [Tool API](api/api-tools.md)
- [Host Tools API](api/api-host-tools.md)
- [Persistence API](api/api-persistence.md)
