# xwork 迁移指南

> 状态：中文初稿，待审阅。

本文说明如何从直接使用 xllm tool loop 或产品内自定义工具系统迁移到 xwork。

## 从直接使用 xllm Tool Loop 迁移

### 迁移前

```text
product code
  -> xllm request
  -> parse tool call
  -> execute product tool
  -> append result
  -> next xllm request
```

常见问题：

- 工具审批分散。
- 工具结果没有统一 artifact。
- run 状态和恢复需要产品自行维护。
- 多 Agent、远程 worker 和 replay 难以复用。

### 迁移后

```text
product code
  -> xwork runtime/workspace/run
  -> register tools / host services
  -> xwork_run_execute
  -> approval/artifact/checkpoint/persistence
```

步骤：

1. 用 `xwork_runtime` 持有 xllm runtime 或 bootstrap options。
2. 用 `xwork_workspace` 注册项目根目录和 memory。
3. 将现有 tool call schema 映射为 `xwork_tool_def`。
4. 将工具真实执行逻辑迁移为 host service 或 tool executor。
5. 用 policy/approval 替换散落的人工确认逻辑。
6. 将工具输出改为 artifact。
7. 接入 file persistence 或自定义 persistence backend。

## 从产品内自定义工具系统迁移

### 工具定义

把产品工具拆成：

- tool id：给模型调用的稳定名字。
- host service kind：filesystem/process/vcs/network/editor 等副作用类别。
- operation id：host service 内部操作。
- side effect：read-only、workspace write、process、network 等风险类别。
- approval mode：默认、永不、总是、按需。

### 执行逻辑

已有工具执行器可以保留，但应通过以下方式接入：

- `xwork_tool_exec_fn` / `xwork_tool_exec_ex_fn`
- `xwork_host_service`
- `xwork_runtime_invoke_host_service_ex()`

长耗时操作要检查 cancel/interrupt context。

### 输出逻辑

把产品私有输出转换为：

- `xwork_tool_result`
- `xwork_artifact`
- `xwork_artifact_summary`
- `xwork.report.v1` / `xwork.diagnostics.v1`

### 恢复逻辑

不要恢复 live handle。恢复时重新创建 runtime、workspace、tool registry、host services，然后加载 run snapshot 或 task graph/control plane snapshot。

## 迁移顺序建议

1. 先接入 runtime/workspace/run。
2. 再接入一两个只读工具。
3. 接入 policy/approval。
4. 接入 artifact summary。
5. 接入 persistence。
6. 接入 async cancel。
7. 扩展到 multi-agent、remote worker 或 replay。

## 相关文档

- [第一个 xwork 程序](guide/first-xwork-program.md)
- [Tool API](api/api-tools.md)
- [Host Tools API](api/api-host-tools.md)
- [Persistence API](api/api-persistence.md)
