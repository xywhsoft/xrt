# xllm v3 核心模块重设计

状态：设计稿（待评审）。范围：仅核心调用层；xllm-session / xllm-memory 暂不考虑，后续按新模型移植。

## 0. 定位与边界

xllm 核心模块 = 所有 Agent 必要功能的共同底座：

1. 统一的单次模型通信能力（一次请求-响应全链路）
2. 统一的工具调用接入能力（声明、流式组装、结果关联回放）
3. 统一的多模态接入能力（文本/图像/音频/文件部件输入）
4. 统一的三方言抽象：Completions / Responses / Anthropic，一套 API
5. SSE 自动处理：回调拿到流式增量，同时自动组装完整思考/正文/工具调用
6. 链路数据：通信诊断、正文、思考、工具调用、token 计数、速度

明确不做（属于 xwork / xllm-session / xllm-memory）：工具执行、会话历史、上下文压缩、长期记忆、Agent 循环、审批。

## 1. 总体架构

```
调用方（xwork / 用户代码）
    │  统一模型 + 事件回调 + 同步/异步两个面
    ▼
┌──────────────────────────────────────────────┐
│ 调用层   xllm_call：生命周期、重试策略、诊断汇聚      │
├──────────────────────────────────────────────┤
│ 方言层   dialect vtable ×3（内部，不进公共 API）     │
│   completions（含 GLM 子方言）/ responses / anthropic │
├──────────────────────────────────────────────┤
│ 流式层   SSE 组帧（event:/data:/id: 字段集）+ 事件归并 │
│         + 响应组装器（有序块 + 便利字段，O(n) 追加）   │
├──────────────────────────────────────────────┤
│ 传输层   连接（TCP/TLS）+ HTTP/1.1 wire + 空闲连接池   │
│         异步状态机，跑在 XRT net engine 上，零调用线程 │
└──────────────────────────────────────────────┘
```

保留不变：错误对象模型（xllm_error + 诊断）、取消/截止语义（xcancel + 绝对 deadline 贯通）、
重试边界（已交付模型事件即不自动重试）、模型画像能力位治理、密钥卫生。

## 2. 数据模型（公共 API）

### 2.1 部件（多模态输入）

```c
typedef enum xllm_part_kind {
    XLLM_PART_TEXT = 0,     /* UTF-8 文本 */
    XLLM_PART_IMAGE,        /* 图像：字节或 URL 引用 */
    XLLM_PART_AUDIO,        /* 音频 */
    XLLM_PART_FILE,         /* 文档/视频等文件 */
    XLLM_PART_NATIVE        /* provider 原生 JSON 部件，原样往返 */
} xllm_part_kind;

typedef struct xllm_part {
    xllm_part_kind eKind;
    char* sText;            /* TEXT：内容；NATIVE：原生 JSON blob */
    char* sNativeType;      /* NATIVE：原生块 type 名，精确回放用 */
    char* sMediaType;       /* "image/png"、"audio/wav" … */
    char* sSourceUrl;       /* 远程 URL 引用（provider 支持时优先） */
    char* sDetail;          /* 图像细节提示：auto/low/high */
    uint8_t* pData;         /* 拥有的字节（image/audio/file） */
    size_t iDataSize;
} xllm_part;
```

构造/析构/添加 API 与现有消息族同构：`xllmPartSetText / SetImageData /
SetImageUrl / SetAudioData / SetFileData / SetNative`。

### 2.2 消息（部件化 + 原生槽位）

```c
typedef struct xllm_message {
    xllm_role eRole;                    /* system/user/assistant/tool */
    xllm_part* pParts;                  /* 内容部件数组（替代单一 sContent） */
    size_t iPartCount, iPartCap;
    char* sToolCallId;                  /* tool 角色：结果关联 */
    xllm_tool_call* pToolCalls;         /* assistant：工具调用 */
    size_t iToolCallCount, iToolCallCap;
    char* sNative;                      /* 整条消息级原生回放 blob（可选） */
} xllm_message;
```

要点：
- 纯文本即单 TEXT 部件；`xllmRequestAddTextMessage` 便捷函数保留。
- 工具结果消息 = role:tool + sToolCallId + parts（Anthropic 映射为 user 的
  tool_result 块，由方言层负责）。
- `sNative` / `XLLM_PART_NATIVE` 保证异构 provider 历史无损往返
  （Anthropic thinking signature、Responses 加密 reasoning item）。

### 2.3 工具

```c
typedef struct xllm_tool {
    char* sName;
    char* sDescription;
    char* sParametersJson;   /* JSON Schema */
    bool bStrict;
} xllm_tool;
```

不变；工具选择/并行开关不变。

### 2.4 请求

```c
typedef struct xllm_request {
    xllm_message* pMessages; …
    xllm_tool* pTools; …
    char* sModel;
    char* sReasoningEffort;          /* 方言映射：effort / reasoning{} / thinking{} */
    uint32_t uReasoningBudgetTokens; /* Anthropic budget_tokens；0 = 不启用 */
    uint32_t uMaxOutputTokens;
    double fTemperature; bool bHasTemperature;
    double fTopP;          bool bHasTopP;
    char* sStop;                    /* 停止序列（单个；数组可后续扩展） */
    bool bParallelToolCalls;
    xllm_tool_choice eToolChoice; char* sNamedTool;
    xllm_json_mode eJsonMode;       /* NONE / OBJECT（结构化输出）*/
    bool bStream;                   /* 默认 true；false = 非流式请求 */
    /* 逃生舱：逐请求扩展 */
    const xllm_header* pExtraHeaders; size_t iExtraHeaderCount;
    char* sExtraBodyJson;           /* 原始 JSON 对象，浅合并进请求体顶层 */
    /* 控制流（沿用） */
    xcancel* pCancel;               /* 借用 */
    uint64_t uDeadline;             /* 绝对单调 µs；UINT64_MAX 关闭 */
} xllm_request;
```

### 2.5 响应（有序块 + 便利字段）

```c
typedef enum xllm_block_kind {
    XLLM_BLOCK_TEXT = 0, XLLM_BLOCK_REASONING, XLLM_BLOCK_TOOL_CALL
} xllm_block_kind;

typedef struct xllm_block {
    xllm_block_kind eKind;
    char* sText;             /* TEXT / REASONING */
    size_t iToolIndex;       /* TOOL_CALL → tResponse->pToolCalls[i] */
    char* sNative;           /* 原生回放（thinking signature 等） */
} xllm_block;

typedef enum xllm_finish {
    XLLM_FINISH_STOP = 0, XLLM_FINISH_LENGTH, XLLM_FINISH_TOOL_CALLS,
    XLLM_FINISH_CONTENT_FILTER, XLLM_FINISH_REFUSAL, XLLM_FINISH_OTHER
} xllm_finish;

typedef struct xllm_usage {
    uint64_t uInputTokens, uOutputTokens, uTotalTokens;
    uint64_t uCachedInputTokens;   /* cached_tokens / cache_read_input_tokens */
    uint64_t uCacheWriteTokens;    /* cache_creation_input_tokens */
    uint64_t uReasoningTokens;
} xllm_usage;

typedef struct xllm_stats {        /* 速度与链路计量（响应终态可得） */
    xllm_usage tUsage;
    uint64_t uConnectMs, uFirstByteMs, uFirstTokenMs, uTotalMs;
    double fOutputTokensPerSec;    /* 输出速率（含推理 token） */
    uint64_t uRequestBytes, uResponseBytes;
    uint32_t uAttempts; bool bReusedConnection;
} xllm_stats;

typedef struct xllm_response {
    char* sId; char* sModel; char* sRequestId;
    xllm_block* pBlocks; size_t iBlockCount;     /* 有序内容块 */
    char* sContent;          /* 便利：全部 TEXT 块连接 */
    char* sReasoningContent; /* 便利：全部 REASONING 块连接 */
    char* sRefusal;          /* 安全拒答原文（非空时 eFinish=REFUSAL） */
    xllm_tool_call* pToolCalls; …
    xllm_finish eFinish; char* sFinishRaw;
    xllm_usage tUsage;
    uint32_t uHttpStatus;
    xllm_diagnostics tDiagnostics;   /* 沿用并扩展（见 §6） */
    xllm_stats tStats;
} xllm_response;
```

## 3. 方言层（内部 vtable，公共面只有 provider 枚举 + 画像）

```c
typedef struct xllm_dialect_ops {
    const char* sName;
    const char* sPath;                       /* "/chat/completions" 等，相对 base */
    void (*BuildAuth)(xllm_client*, xllm_headers*);
    bool (*BuildRequest)(const xllm_client*, const xllm_request*,
                         const xllm_profile_view*, xllm_buf*, xllm_error*);
    bool (*DecodeSseEvent)(xllm_call*, const xllm_sse_event*);   /* 字段集已组好 */
    bool (*DecodeJsonBody)(xllm_call*, xstrview);
    void (*MapHttpError)(xllm_call*, uint32_t, xstrview);
    bool (*Finalize)(xllm_call*);
    bool (*IsRetryableProviderError)(const xllm_error*);
} xllm_dialect_ops;
```

- 公共 API 不暴露 vtable；`xllm_provider` 枚举扩为：
  `OPENAI_COMPAT(0) / GLM / OPENAI_RESPONSES / ANTHROPIC`。
  GLM 内部走 completions 方言 + GLM 微调位（thinking 对象、tool_stream、
  reasoning_content 回放）。
- 模型画像决定 completions 方言的新模型参数名（max_completion_tokens /
  developer 角色——沿用今日已落地的两个能力位）。

### 3.1 三方言映射表（请求侧）

| 统一概念 | Completions | Responses | Anthropic |
|---|---|---|---|
| 端点 | POST …/chat/completions | POST …/responses | POST …/v1/messages |
| 认证 | Authorization: Bearer | Bearer | x-api-key + anthropic-version: 2023-06-01 |
| system 消息 | messages[]（画像可 developer） | instructions / input developer | 顶层 system 字符串（多条拼接） |
| 文本部件 | content 字符串 / 数组 | input_text | text 块 |
| 图像字节 | image_url{url:"data:…;base64,"} | input_image | image{source:{base64,media_type}} |
| 图像 URL | image_url{url} | input_image{image_url} | image{source:{url}} |
| assistant 工具调用 | tool_calls[] | function_call items | tool_use 块 |
| 工具结果 | role:tool + tool_call_id | function_call_output | user 的 tool_result 块 |
| 工具声明 | function{name,desc,parameters,strict} | {type:function,…} | {name,desc,input_schema} |
| tool_choice | auto/none/required/{name} | 同左 | auto/none/any/{type:tool,name} |
| 推理控制 | reasoning_effort | reasoning{effort} | thinking{type:enabled,budget_tokens} |
| 输出上限 | max_tokens ｜ max_completion_tokens（画像） | max_output_tokens | max_tokens（必填，画像兜底） |
| 流式开关 | stream + stream_options.include_usage | stream | stream |
| JSON 模式 | response_format{json_object} | text{format} | 无（结构化靠工具约束） |

### 3.2 三方言映射表（流式事件侧）

| 统一事件 | Completions | Responses | Anthropic |
|---|---|---|---|
| TEXT_DELTA | choices[].delta.content | response.output_text.delta | content_block_delta.text_delta |
| REASONING_DELTA | （无，仅 usage 计数） | response.reasoning_summary_text.delta | content_block_delta.thinking_delta |
| TOOL_CALL_DELTA | delta.tool_calls[i].function.arguments 等增量 | response.function_call_arguments.delta | content_block_delta.input_json_delta |
| USAGE | usage-only 尾块 | response.completed{usage} | message_start{input} + message_delta{output} |
| RESPONSE_DONE | [DONE] | response.completed | message_stop |
| 失败终态 | 流中 error 对象 | response.failed / response.incomplete | event:error（含 overloaded） |
| 块边界 | 无 | output_item.added | content_block_start / stop |

SSE 组帧层统一产出 `xllm_sse_event { sEvent; sData; sId; }` 字段集
（Anthropic 用 event: 行；Responses 用 data 内 type 字段；Completions 只用 data）。
当前实现把 event:/id:/retry: 静默丢弃——v3 组帧层收集它们。

### 3.3 终止原因归一

| 统一 | 各方言原始值 |
|---|---|
| STOP | stop / end_turn / stop_sequence |
| LENGTH | length / max_tokens / response.incomplete(reason=max_output_tokens) |
| TOOL_CALLS | tool_calls / tool_use / function_call |
| CONTENT_FILTER | content_filter |
| REFUSAL | refusal 非空（全方言） |

## 4. 流式与组装

- 事件模型扩展为 7 种：RESPONSE_START / TEXT_DELTA / REASONING_DELTA /
  TOOL_CALL_DELTA / BLOCK_META（块开始/结束，携带块类型与序号）/ USAGE /
  RESPONSE_DONE。文本类事件携带块序号，UI 可按到达顺序渲染交错内容。
- 组装器维护 (指针,长度) 构建器（不再对 NUL 结尾反复 strlen），修复现有
  O(n²) 追加；便利字段 sContent/sReasoningContent 在完成时一次连接。
- 工具调用组装沿用 index 路由增量拼接 + 收尾整体 JSON 校验。
- 严格性沿用：逐事件 JSON 校验、畸形即整体失败零部分响应、[DONE]/message_stop
  哨兵、错误事件直接终态。

## 5. 并发与生命周期（去除每调用一线程）

```c
xllm_client* xllmClientCreate(const xllm_client_config*, xllm_error*);
/* config 新增：pNetEngine（借用共享引擎；NULL=自建，沿用）/
   uMaxIdleConnections（默认 4，替代单连接槽） */

xllm_call* xllmCallStart(client, request, callbacks, error);  /* 提交到引擎 */
xfuture*  xllmCallFuture(call);        /* 可选：集成到宿主事件循环 */
xllm_result xllmCallWait(call, &response, &error);  /* 等待 future，零线程 */
bool xllmCallCancel(call);
void xllmCallDestroy(call);

xllm_result xllmComplete(client, request, callbacks, &response, &error);
/* = Start + Wait + 重试循环；重试退避可中断（沿用） */
```

- 传输层重写为异步状态机：连接→写请求→读头→读体→收尾，全部由引擎
  worker 上的流事件/续接驱动；`xllmCallWait` 只等 future。
- 重试循环挂在完成续接上，策略不变（含 bModelDataDelivered 边界、
  Retry-After、可中断退避、方言级 provider 错误分类钩子）。
- 同步语义完全保留；并发 N 调用 = N 个引擎内异步对象，而非 N 线程。

## 6. 遥测（诊断扩展）

`xllm_diagnostics` 在现有 30+ 字段基础上增加：
- uFirstTokenMs（首个模型增量，区别于 uFirstByteMs）
- 方言名 / provider request id（已有）/ 块计数
- `xllm_stats`（§2.5）：usage、TTFB/首 token/总时长、输出 tokens/s、
  字节、尝试数、连接复用——从响应与 USAGE 事件均可取。

## 7. 文件布局（src/）

| 文件 | 职责 | 预估行数 |
|---|---|---|
| xllm_parts.c | 部件/消息/请求值模型 | ~500 |
| xllm_dialect.c | 方言注册表 + 查询（provider→ops） | ~120 |
| dialect_completions.c | Completions 方言（含 GLM 微调位） | ~700 |
| dialect_responses.c | Responses 方言 | ~650 |
| dialect_anthropic.c | Anthropic 方言 | ~750 |
| xllm_sse.c | SSE 组帧（字段集组装，任意分片） | ~200 |
| xllm_assemble.c | 块/工具/便利字段组装器（O(n)） | ~350 |
| xllm_call.c | 调用状态机 + 重试 + 诊断汇聚 | ~700 |
| xllm_transport.c | 连接池 + HTTP wire（异步化重写） | ~700 |
| xllm_profile.c | 画像（沿用 + 新内置条目） | ~200 |
| xllm_core.c | 错误/工具函数（去三层重复，单份） | ~450 |

公共头仍为单头 xllm.h（约 500 行）。

## 8. 测试与验证

1. 每方言一套录制流量 fixtures（真实 provider 抓包的请求/事件序列），
   回环服务器按 fixture 回放 → 统一模型断言（字节级 + 语义级）。
2. 任意分片（1..13 字节步进）× 三方言事件流；畸形拒绝；撕裂截断。
3. OOM 注入：按分配序号逐个失败，断言零泄漏可恢复（对齐主库标准）。
4. fuzz：三方言 SSE/JSON 解码器 + URL + 部件模型。
5. TLS 真链路：XRT 自建 TLS listener（自签证书 + 系统信任库注入），
   补齐 TLS 与连接复用零覆盖。
6. 并发压测：共享引擎上 16 路并发调用 + 取消对撞。

## 9. 迁移路径（顺序即依赖序）

- M1 值模型重写（parts/blocks/请求/响应）+ 组装器 + SSE 组帧 —— ✅ 已完成
  （行为门禁：现有测试迁移到新模型语义）。
- M2 方言框架 + Completions（含 GLM 微调位、两个能力位）—— ✅ 已完成。
- M3 传输去调用线程 + 连接池 —— ✅ 已完成（初为惰性/内联形态，后被 §10-1
  的引擎级异步取代：watch 链状态机 + 引擎定时器看门狗 + `xllmCallFuture`；
  空闲连接池默认 4、上限 8）。
- M4 Anthropic 方言 —— ✅ 已完成（thinking signature 捕获仍留 M6）。
- M5 Responses 方言 —— ✅ 已完成（音频/文件输入部件未映射，显式报错）。
- M6 加固期 —— 大部分完成：连接复用/并发/OOM 注入/真网对拍（llama.cpp 三方言）/
  TLS 全链路回环（自签证书夹具 + 服务端 TLS 模块进桥接头）全部落地；
  仍缺 fuzz harness（需 clang 环境）、内置画像数字核验。
- xllm-session / xllm-memory 在 M1 后另行移植（不在本设计范围；
  已知：session 消息克隆已补 parts/native，纯文本会话不受影响）。

## 10. 后续开发计划（2026-09-12 更新）

1. **引擎级事件驱动异步 + `xllmCallFuture`** ✅ 已完成（2026-09-13）：
   传输层为 watch 链状态机（dial→send→read 增量泵），全部推进发生在
   引擎 worker 的 future-watch 回调；每次操作堆分配 watch 节点并由
   release 释放（嵌入式存储不可在自身通知内复用）。`xllmCallFuture`
   暴露借用 future；Wait 等待 future（截止/取消竞争经看门狗定时器
   `xrtNetEngineSchedule` + 取消 watch 双保险，等待侧超时统一传输结果
   语义防误重试）。共享引擎注入 `pConfig->pNetEngine`（借用，多 client
   共享，销毁 client 不停引擎）。验收达成：6 路并发 Start 先提交后等待
   全通过；同步语义与既有测试不变（236/89/25 全绿 ×3 连跑 + live 真网）。
2. fuzz harness 三件套（SSE 组帧 / 三方言解码器 / URL），随 CI 的
   clang 环境接入。
3. 内置 GPT/Claude 画像（核验数字后）。
4. session 深度 v3 化（blocks 消费、账本治理）。

### 经验教训（API 使用备忘）

- **`xrtFutureWaitFor(future, 相对微秒)` vs `xrtFutureWaitUntilCancel
  (future, 绝对 deadline, cancel)`**：曾把 `xrtDeadlineAfter()` 的绝对
  截止当相对时长传给 `WaitFor`，300ms 静默读变成约 2.8 天等待，表象为
  "TLS 接收永久卡顿"。排查路径：最小复现 → gdb 栈（本例行号不可靠）→
  库内插桩打印实参。凡新增等待调用，先确认参数语义再写。

版本：v3.0.0（破坏性：消息模型部件化；公共 API 命名与约定保持延续）。
