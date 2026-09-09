#Asynchronous execution and cancellation

>Status: First draft in Chinese, awaiting review.

The run of xwork can be executed synchronously or put into the background worker through `xwork_run_execute_async()`. Asynchronous handles are used for waiting, timed waiting, status query and cancellation.

## Minimum process

```text
xwork_run_execute_async
xwork_run_async_wait_timeout
xwork_run_async_get_status
xwork_run_async_cancel
xwork_run_async_destroy
```

## Example

```c
xwork_run_async *pAsync = NULL;
bool bCompleted = false;
xwork_status status;

status = xwork_run_execute_async(pRun, &tOrchestrator, &pAsync);
if (status != XWORK_OK) {
    return 1;
}

status = xwork_run_async_wait_timeout(pAsync, 1000, &bCompleted);
if (status == XWORK_OK && !bCompleted) {
    xwork_run_async_cancel(pAsync, "timeout");
}

xwork_run_async_destroy(pAsync);
```

## Life cycle constraints

- async handle shallow-copy `xwork_orchestrator_options`.
- callback, user data, profile strings and caller-owned cancel token must survive until the handle is completed or destroyed.
- `pRun` and runtime related objects must live until the async handle is completed or destroyed.
- During async execution, do not directly mutate or destroy the same run.
- The second execute entry of the same run will return `XWORK_ERROR_INVALID_STATE`.

## Cancel propagation

Cancellations are passed via the collaborative path to:

- xllm cancel token.
- orchestrator phase check.
- tool executor context.
- host invoke context.
- local `process.exec` poll/spawn path.

Long-running tools should actively check the context and should not rely solely on external kills.

## Next step

- [Run API](../api/api-run.md)
- [Orchestrator API](../api/api-orchestrator.md)
- [claw Autonomous Agent Example](../case/claw-autonomous-agent.md)
