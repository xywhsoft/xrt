# Orchestrator API

> 状态：中文初稿，待审阅。

Orchestrator API 驱动 `xwork_run` 上的 model turn + tool loop。它把 xllm 模型调用、工具执行、审批暂停、artifact 生成和取消语义串成一个可恢复的 Agent 执行闭环。

## 相关声明

- `xwork_orchestrator_options`
- `xwork_model_stream_mode`
- `xwork_model_event`
- `xwork_model_event_fn`
- `xwork_tool_choice_mode`
- `xwork_run_execute()`
- `xwork_run_execute_async()`
- `xwork_run_submit_approval()`
- `xwork_run_resume()`

## 模块定位

Orchestrator 负责围绕 xllm 组织执行流程。它不负责 provider 协议适配、不内置完整 planner、不直接实现 UI，也不绕过 policy 执行副作用。

## 执行闭环

```text
run_execute
  prepare model input
  call xllm model turn
  forward stream events
  receive model output/tool calls
  evaluate policy
  execute tool or pause for approval
  emit events/artifacts/checkpoints
  continue next turn until final output or budget exhausted
```

## 逐函数说明

### xwork_orchestrator_options_init

初始化 orchestrator options。

**功能：**

准备 `xwork_run_execute` / `xwork_run_execute_async` 使用的 model-turn + tool-loop 配置。

**函数原型：**

```c
XWORK_API void xwork_orchestrator_options_init(xwork_orchestrator_options *pOptions);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；profile id、tool choice、callback user data 等输入在执行期间按对应 API 规则使用。

**补充说明：**

初始化后再设置模型 profile、turn budget、stream callback、tool choice 和执行策略。执行期间同一 run 不允许重入。

**范例代码：**

```c
xwork_orchestrator_options opts;
xwork_orchestrator_options_init(&opts);
opts.iMaxTurns = 8;
```

**相关 API：**

- `xwork_run_execute`
- `xwork_run_execute_async`

---

## Model stream event

`xwork_model_event` 是从 xllm 透传并归一化的模型流式事件。常见字段：

| 字段 | 说明 |
| --- | --- |
| `eType` | 底层 xllm event type。 |
| `sText` | 文本、thinking、refusal 或 error 内容。 |
| `sResponseId` / `sModel` | 响应和模型元数据。 |
| `sToolCallId` / `sToolId` / `sToolName` | tool call 元数据。 |
| `sArgumentsDelta` | tool arguments 增量。 |
| `sArtifactId` / `pArtifactData` | artifact 流式数据。 |

callback 返回 `false` 会取消当前 model turn，并通过 `XWORK_ERROR_CANCELLED` 传播。

取消优先级：

- interrupt / cancel token 检查先于用户 event callback。
- 用户 event callback 返回 `false` 后，当前 model turn 被取消。

## Tool loop 与审批

当模型请求工具时，orchestrator 会：

1. 查找 tool definition。
2. 基于 side effect、approval mode 和 policy 评估是否允许。
3. 对允许的调用执行 tool executor 或 host service。
4. 对需要审批的调用创建 `xwork_approval_request` 并暂停 run。
5. 恢复后使用已保存的 tool arguments 继续执行。

这使 AI IDE 可以在 UI 中审批 patch、command 或 terminal 操作，也允许 claw 根据 profile 自动批准低风险动作。

## Tool choice

orchestrator 支持将工具选择意图传入模型：

| 模式 | 说明 |
| --- | --- |
| `XWORK_TOOL_CHOICE_AUTO` | 模型自行选择。 |
| `XWORK_TOOL_CHOICE_NONE` | 禁止工具调用。 |
| `XWORK_TOOL_CHOICE_REQUIRED` | 要求模型调用某个工具。 |
| `XWORK_TOOL_CHOICE_NAMED` | 指定工具。 |

具体字段在 `xwork_orchestrator_options` 中配置。

## 同步执行

```c
xwork_status status = xwork_run_execute(pRun, &tOptions);
if (status == XWORK_ERROR_PAUSED) {
    /* 查询 approval request，提交审批后 resume。 */
}
```

## 异步执行

异步执行由 Run API 提供：

```c
xwork_run_async *pAsync = NULL;
xwork_run_execute_async(pRun, &tOptions, &pAsync);
```

async cancel 会进入同一套协作式 cancel token 路径，工具执行器和 host service 应检查 context。

## 错误码

- `XWORK_ERROR_INVALID_ARGUMENT`：options、run 或 callback 参数无效。
- `XWORK_ERROR_INVALID_STATE`：run 状态不允许执行，或同一 run 重入执行。
- `XWORK_ERROR_NOT_FOUND`：模型请求未注册工具。
- `XWORK_ERROR_PAUSED`：需要审批或 side-effect-blocking record 模式暂停。
- `XWORK_ERROR_CANCELLED`：cancel token、interrupt、callback false 或 async cancel。
- `XWORK_ERROR_EXTERNAL_FAILURE`：xllm provider、host service、persistence 或 xrt 失败。

## 线程边界

同一 run 同一时间只能有一个 orchestrator 执行入口。异步执行期间，调用方只能通过 async handle 观察、等待或取消，不应直接 mutation run。

## 恢复边界

orchestrator 可以从 run snapshot 恢复 pending tool、approval decision 和 checkpoint 相关状态。恢复需要调用方重新提供 runtime、workspace、tool registry、host service、persistence backend 和 xllm runtime/profile。

## 相关文档

- [Run API](api-run.md)
- [Tool API](api-tools.md)
- [xllm 编排与工具循环](../guide/xllm-orchestrator-intro.md)
- [AI IDE Agent 范例](../case/ai-ide-agent.md)
