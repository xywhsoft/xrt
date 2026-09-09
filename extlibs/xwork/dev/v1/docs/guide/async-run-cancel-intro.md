# 异步执行与取消

> 状态：中文初稿，待审阅。

xwork 的 run 可以同步执行，也可以通过 `xwork_run_execute_async()` 放入后台 worker。异步 handle 用于等待、限时等待、查询状态和取消。

## 最小流程

```text
xwork_run_execute_async
xwork_run_async_wait_timeout
xwork_run_async_get_status
xwork_run_async_cancel
xwork_run_async_destroy
```

## 示例

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

## 生命周期约束

- async handle shallow-copy `xwork_orchestrator_options`。
- callback、user data、profile strings 和 caller-owned cancel token 必须活到 handle 完成或销毁。
- `pRun` 和 runtime 相关对象必须活到 async handle 完成或销毁。
- async 执行期间，不要直接 mutation 或 destroy 同一个 run。
- 同一 run 的第二个 execute 入口会返回 `XWORK_ERROR_INVALID_STATE`。

## 取消传播

取消会通过协作式路径传递给：

- xllm cancel token。
- orchestrator phase check。
- tool executor context。
- host invoke context。
- local `process.exec` poll/spawn 路径。

长耗时工具应主动检查 context，不应只依赖外部 kill。

## 下一步

- [Run API](../api/api-run.md)
- [Orchestrator API](../api/api-orchestrator.md)
- [claw 自主 Agent 范例](../case/claw-autonomous-agent.md)
