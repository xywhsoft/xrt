# xllm v2

`xllm` 是纯 C 的单次模型调用层。它负责把统一请求转换为提供方请求，通过 xrt HTTP/1.1 客户端传输，增量解析 SSE，并将文本、推理内容、多工具调用和 token usage 归一化。

它不执行工具、不维护会话、不压缩上下文，也不运行 Agent 循环。这些职责分别属于 `xwork` 与 `xllm-session`。

## 当前能力

- OpenAI-compatible 与 GLM 请求方言
- 流式 HTTP/SSE，支持任意网络分片
- provider 边界严格校验 JSON，拒绝宽松解析器可接受的畸形 SSE/非流式响应
- 文本与推理内容增量事件
- 多个工具调用交错返回及 arguments 增量拼接
- GLM `tool_stream` 与保留式思考（完整重放 `reasoning_content`）
- assistant/tool 历史消息及 `tool_call_id` 关联
- 并行工具调用、named/auto/required/none tool choice
- 输入、输出、缓存和推理 token usage
- 非 SSE JSON 响应回退
- HTTP、认证、限流、超时、取消、协议和解析错误归一化
- 基于 XRT 核心 TCP/TLS 与 HTTP/1 wire API 的独立 keep-alive 连接
- 请求级传输阶段、系统错误、连接复用、耗时和字节统计
- 借用式 `xcancel` 与绝对单调 deadline，贯通取消和可中断重试退避
- 仅在尚未交付模型事件时执行的有界瞬态重试，并支持 `Retry-After`

`xllm-session` 当前还提供：

- GLM-5.1 的 200K context / 128K 单次输出上限默认配置，调用方仍可显式覆盖
- 独立的输出上限与输出预留；短上下文可使用完整输出上限，长上下文动态收缩本轮 `max_tokens`
- `context - output_reserve - safety_reserve` 输入预算
- 75% 软裁剪、95% 压缩的默认压力策略
- 保留工具调用/工具结果关联的旧工具输出裁剪
- 只在完整 Agent 轮次边界推进的压缩事务
- 未完成工具调用隔离，禁止压缩不完整轮次
- 原子 JSON 快照、进程重启加载及摘要连续性
- 先写后确认的增量 NDJSON 日志、快照检查点和快照后重放恢复
- 崩溃残尾自动截断、日志序号去重，以及完整损坏记录的显式拒绝
- 会话深分叉，保留摘要、序号与压缩检查点，并隔离父子分支的后续状态

`xllm-memory` 当前还提供：

- 显式、可追溯的长期 fact / preference / task / summary / knowledge 记录
- namespace、source URI、actor、reason、trust、sensitivity、record/store revision 与内容指纹
- 每次写入、替换、无变化和删除的 mutation receipt
- 原子本地 JSON 持久化、重启校验和 namespace 隔离
- 不依赖模型的确定性词法检索与 UTF-8 精确短语检索
- 默认过滤过期和 sensitive/secret 记录，并将有来源的结果渲染为不可信参考上下文

详细边界和最小示例见 [docs/MEMORY.md](docs/MEMORY.md)。

## 模块边界

```text
xcode CLI
    -> xwork          Agent 循环、工具执行、审批、循环保护
        -> xllm-session  上下文账本、预算、裁剪、持久化、压缩
        -> xllm-memory   显式长期记录、来源、检索、审计回执
        -> xllm          一次模型调用、SSE、provider 适配
            -> xrt       核心 HTTP/1.1 wire、TLS、future、网络运行时
```

## 最小调用

```c
#include "xllm.h"

xllm_client_config config;
xllm_request request;
xllm_response* response = NULL;
xllm_error error;
xllm_client* client;

xllmClientConfigInit(&config);
config.sBaseUrl = getenv("XLLM_BASE_URL");
config.sApiKey = getenv("XLLM_API_KEY");
config.sModel = getenv("XLLM_MODEL");
config.eProvider = XLLM_PROVIDER_GLM;
config.uMaxOutputTokens = 131072u;
config.uMaxAttempts = 3u;

client = xllmClientCreate(&config, &error);
xllmRequestInit(&request);
xllmRequestAddTextMessage(&request, XLLM_ROLE_USER, "分析当前项目");

if ( xllmClientComplete(client, &request, NULL, &response, &error) == XLLM_RESULT_OK ) {
    printf("%s\n", response->sContent);
}

xllmResponseDestroy(response);
xllmRequestUnit(&request);
xllmClientDestroy(client);
```

API key 只从调用方配置传入；库不会把 key 写入请求 JSON、日志或错误对象。

## 生命周期

- `xllmRequestAddMessage()`、`xllmRequestAddTool()` 会复制输入数据。
- `xllmClientStart()` 会复制并序列化请求；返回后调用方可以释放请求。
- `xllmCallWait()` 只能调用一次，成功时把 `xllm_response*` 所有权交给调用方。
- `xllmCallCancel()` 可由控制线程触发；主线程仍应调用 `xllmCallWait()`，之后再销毁 call。
- `xllmClientStart()` / `xllmCallWait()` 保持单次尝试；同步便捷入口 `xllmClientComplete()` 才执行配置的瞬态重试。
- `xllm_client` 必须比其创建的全部 call 存活更久。
- XRT2 不需要进程级初始化；每个 `xllm_client` 持有自己的网络引擎、解析器、TLS verifier 与一条空闲连接。

## 构建与测试

```bat
build.bat
```

Linux/macOS（也可通过 `CC` 指定 Clang 或交叉编译器）：

```sh
sh build.sh
```

交叉编译时设置 `RUN_TESTS=0`，并可通过 `XRT_DIR`、`BUILD_DIR`、
`RELEASE_DIR`、`CFLAGS`、`LDFLAGS` 和 `LIBS` 覆盖默认值。

构建会生成 `release/xllm.o`、`release/xllm-session.o` 与 `release/xllm-memory.o`，并运行 provider、会话预算/恢复、memory 写入/检索/审计与持久化测试。

## 诊断与重试

调用方可通过 `xllmRequestSetCancel()` 与 `xllmRequestSetDeadline()` 为一次完整调用（包括重试退避）绑定借用式取消令牌和绝对 `xrtClock()` 截止时间。取消令牌必须存活到 `xllmClientComplete()` 返回，或异步 call 完成并销毁。取消返回 `XLLM_RESULT_CANCELLED`；deadline 返回 `XLLM_RESULT_TIMEOUT`，不会继续自动重试。

```c
xcancel* operation = xrtCancelCreate();
xllmRequestSetCancel(&request, operation);
xllmRequestSetDeadline(&request, xrtDeadlineAfter(UINT64_C(120000000)));
result = xllmClientComplete(client, &request, NULL, &response, &error);
xrtCancelDestroy(operation);
```

成功响应和错误对象尾部都包含 `xllm_diagnostics`。其中保留尝试次数、是否耗尽重试、xrt 传输错误与阶段、系统错误、HTTP 是否已开始、是否已交付模型事件、连接复用、单调时钟里程碑、耗时和字节数。provider request id 仍分别通过 `xllm_response.sRequestId` 与 `xllm_error.sRequestId` 暴露。

`xllmClientComplete()` 默认最多尝试 3 次，使用有上限的指数退避，并识别 HTTP 408/409/425/429/500/502/503/504、限流和瞬态网络错误。只要已经解析并交付任一模型 SSE 事件，就不会自动重试，从而避免重复文本或工具调用。`uMaxAttempts = 1` 可关闭重试；超时仍按每次 HTTP 尝试计算。

## 当前边界

- 当前输入内容为文本；图片、文件等多模态输入后续扩展。
- 全局限流、跨任务退避和配额策略仍由 xwork/宿主负责；xllm 只处理单次 provider 调用内的短暂失败。
- 目前以 chat-completions 兼容协议为首个闭环，后续 provider adapter 不改变上层请求/响应模型。
