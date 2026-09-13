# xllm-session v3 设计稿（Pi compaction 方案）

状态：设计稿（待评审）。依赖：xrt（文件/JSON/时间）+ xllm 核心 v3（调用/流式/三方言）。
前置讨论结论（本仓库会话已论证，本设计直接采纳）：

- **词法 token 估算不参与任何决策**——实测偏差 2 倍以上且内容类型相关，校准不收敛；
- 决策模型改为「**服务端精确 usage 反馈 + 有界增量**」：`占用(精确) = usage.prompt_tokens + usage.output_tokens`，
  增量由策略封顶（max_tokens / 用户消息上限 / 工具结果上限），最坏情况可算，估算退出决策链。

## 0. 定位与边界

xllm-session 是**会话治理扩展库**：绑定一个 `xllm_client`（借用），内部管理对话历史、
上下文精确计量、以及 **Pi compaction 方案**的上下文压缩。

做：轮次账本、精确占用反馈、压缩事务（滚动摘要）、渲染（system + 摘要 + 尾窗）、
持久化（日志/快照/恢复）、会话分叉。
不做：模型调用本身（xllm 的职责）、工具执行与 Agent 循环（xwork 的职责）、
长期事实记忆（xllm-memory 的职责）、多会话编排（上层应用的职责）。

## 1. 集成拓扑

```text
应用 / xwork（Agent 循环、工具执行）
    │ 借用 xllm_client
    ▼
xllm-session（本库）
    │ 渲染请求 / 透传流式回调 / 元调用（压缩摘要）
    ▼
xllm v3 核心（三方言 / SSE / 重试 / 诊断）
    ▼
xrt（HTTP/1.1 wire、TLS、引擎）
```

XRT 依赖面（自有桥接，与现状一致）：`JSON_READ / FILE_WHOLE / DIR / PATH / TIME`。

## 2. 对象模型

```text
xllm_session
├── config          窗口/预留/尾窗/阈值/摘要风格/持久化路径
├── client          借用的 xllm_client（本库不拥有、不销毁）
├── ledger          条目账本（v2 继承：全局递增 seq，journal 的重放基准）
│     entry := { seq, kind(user/assistant/tool/system), 消息数据,
│                 turn 归属, 压缩标记 }
├── summary         滚动摘要对象（Pi CompactionEntry 适配）
├── governance      精确占用缓存 + 阈值状态
└── journal         NDJSON 写前日志（v2 继承）
```

摘要对象（Pi 的 CompactionEntry 适配）：

```c
typedef struct xllm_session_summary {
    char* sText;            /* 结构化 Markdown 摘要体 */
    size_t iThroughEntry;   /* 摘要覆盖 [0, throughEntry]，与尾窗切点对齐 */
    uint32_t uGeneration;   /* 摘要代数，每压缩一次 +1 */
    uint64_t uPromptTokensAtBirth; /* 生成它的元调用精确 prompt_tokens */
    uint64_t uOutputTokensAtBirth; /* 元调用 output_tokens（摘要精确成本） */
} xllm_session_summary;
```

关键差异（相对 Pi）：Pi 的 `tokensBefore` 是估算，我们的 `uPromptTokensAtBirth/Output`
是**元调用的服务端精确值**——摘要的真实成本第一次可以被精确记账。

## 3. 上下文计量：精确反馈回路

### 3.1 数据流

```text
每轮真实调用完成后（xllm_response->tUsage 已到手）：
  fill_exact   = usage.uInputTokens + usage.uOutputTokens      ← 服务端真数
  increment_max = max_tokens（调用方设置）
                + 用户消息上限（config，超限策略截断或拒绝）
                + 工具结果上限 × 并发工具数上限（config）
                + 结构开销常数（角色/JSON 包裹，默认 256）
  判据：fill_exact + increment_max > window - reserve → 在本轮边界触发压缩
```

- 窗口 `window`：绑定 client 时优先取模型画像 `uContextWindowTokens`，无画像则
  config 必填显式值——**不再隐式猜测**；
- `reserve`（Pi 默认 16384）承担输出与安全余量，config 可调；
- 词法估算函数（`xllmEstimate*`）保留在 xllm 核心但**本库不调用**；UI 显示占用也用
  `fill_exact`（上一轮真数），标明"上轮实测"。

### 3.2 估算在方案里唯一的残留位置（及为什么无害）

Pi 的**切点回溯**需要逐消息成本（从最新往回累计到 `keepRecentTokens` 找切割边界）。
逐消息精确 token 数不可得（服务端只报总量），此处用**每消息估算仅做结构性切分**：
- 压缩**是否触发**由精确值决定（3.1）——决策不受估算误差影响；
- 切点只决定"尾窗大致保留多少"，偏 20% 无正确性后果，且下一轮真实调用的 usage
  立即自校正整体占用；
- 文档与注释明确标注此边界，防止未来误把估算重新引入决策。

### 3.3 可选：事前精确探测钩子

`llm_probe_proc` 可注入：宿主可接 llama.cpp `/tokenize`、Anthropic `count_tokens`
或 `max_tokens=1` 探针，在渲染后、发送前获得精确值。默认 NULL（用 3.1 反馈回路已足够）。

## 4. 压缩：Pi compaction 方案适配

### 4.1 触发（Pi 三类原因照搬）

| 原因 | 时机 | 行为 |
|---|---|---|
| threshold | 轮边界（工具结果入账后、下个 assistant 前；新用户消息前），3.1 判据命中 | 自动压缩 |
| overflow | 硬阈值（默认 90%）命中或 provider 报超窗错误 | **先压缩成功再重试本轮**，失败则按策略报错 |
| manual | 调用方显式触发 | `/compact` 等价入口，可带聚焦指令 |

压缩只在**完整轮次边界**执行（v2 规则保留）：未完成的工具调用对不进入候选。

### 4.2 切点规则（Pi 原样）

从最新条目回溯，按估算累计到 `keepRecentTokens`（默认 20000）：
- 合法切点：user / assistant / system-lift 后的普通消息边界；
- **工具结果与其 tool call 原子**——切点落在配对之外；
- 超大单轮（回溯预算耗尽仍在同一轮内）：**split-turn 摘要**（Pi 方案）——该轮
  前缀单独生成一份摘要并入主摘要，`turnPrefix` 记入摘要对象。

### 4.3 候选序列化（Pi 原样 + 精确化）

```
[User]: <文本>
[Assistant thinking]: <推理内容，可配置省略>
[Assistant]: <正文>
[Assistant tool calls]: <name>(<args>)
[Tool result]: <截断至 2000 字符，标注截断量>
```

- 标签防止模型把序列化文本当作待继续的对话（Pi 明确动机）；
- 工具结果 2000 字符截断 + 截断量标注；
- **累计文件/工具操作追踪**（Pi 的 read-files/modified-files）：从本批候选消息的工具
  调用 + 上一代摘要的 details 双向提取，跨多次压缩保留完整操作史（见 4.5 格式）。

### 4.4 元调用与滚动迭代

```
prompt = 摘要指令模板（按风格 general/coding/自定义）
       + 上一代摘要（存在时，作为迭代上下文）        ← Pi 滚动：摘要的摘要
       + 本批候选的序列化文本
调用   = 绑定的 client（同方言同链路；max_tokens = summary 上限）
```

- 重复压缩从上一代 `iThroughEntry` 起算（Pi 的 firstKeptEntryId 语义）：上轮保留的
  尾窗在下轮成为新候选，主摘要持续滚动；
- 元调用的 usage 写入摘要对象（2 节）——摘要成本精确入账；
- 流式回调透传为 NULL（摘要一次性整取）；
- **元调用不计入会话占用缓存**，但其失败/取消按 Pi 事件语义上报（`willRetry`
  仅 overflow 场景为真）。

### 4.5 摘要格式（Pi 结构化 Markdown，风格可配）

coding 预置（Pi 原样）：

```
## Goal
## Constraints & Preferences
## Progress（Done / In Progress / Blocked）
## Key Decisions
## Next Steps
## Critical Context
<read-files>…</read-files> <modified-files>…</modified-files>
```

general 预置：`## 会话目标 / ## 用户偏好与约束 / ## 已确立事实 / ## 进行中话题 /
## 关键决定 / ## 后续`（章节等价，中文场景适配）。质量门为**结构校验**（必选章节
存在 + 长度上限 + 代数单调），不假装理解语义——与 v2 治理哲学一致，但章节清单
对齐 Pi 且可由 config 替换。

### 4.6 提交与渲染

提交原子性（v2 事务语义保留）：`compact` 事件**先写 journal 再推进内存**；失败 =
安全中止（旧摘要与新候选原样保留）。账本条目只标记压缩、不删除（完整史可追溯；
内存卸载见 §6 可选项）。

渲染顺序（Pi 原样）：

```
[system（config 固定段，永不被压缩）]
[摘要消息（作为第二条 system 角色消息注入；三代言都映射到顶层 system）]
[尾窗逐字条目（含保留下来的工具配对）]
[本轮新消息]
```

## 5. 公共 API 草案

```c
typedef struct xllm_session_config {
    uint64_t uContextWindow;       /* 0 = 从绑定 client 的画像取；无画像必须显式 */
    uint32_t uReserveTokens;       /* 默认 16384（Pi 默认） */
    uint32_t uKeepRecentTokens;    /* 默认 20000（Pi 默认） */
    uint32_t uSummaryMaxTokens;    /* 元调用 max_tokens，默认 2048 */
    uint32_t uToolResultCap;       /* 工具结果上限（字节），默认 2000（Pi） */
    uint32_t uUserMessageCap;      /* 用户消息上限（字节），0 = 不限 */
    uint8_t  uSoftThresholdPct;    /* 默认 75 */
    uint8_t  uHardThresholdPct;    /* 默认 90 */
    const char* sSummaryStyle;     /* "general" | "coding" | 自定义模板路径 */
    const char* sJournalPath;      /* 可选，启用日志 */
    const char* sSnapshotPath;     /* 可选，快照路径 */
} xllm_session_config;

void xllmSessionConfigInit(xllm_session_config*);

typedef struct xllm_session_stats {
    uint64_t uFillExact;           /* 上轮实测占用（prompt+output） */
    uint64_t uWindow;
    uint64_t uReserve;
    uint64_t uIncrementMax;
    uint8_t  uSoftPct, uHardPct;   /* 当前占用相对窗口的百分比 */
    uint32_t uSummaryGeneration;
    uint64_t uSummaryTokensExact;  /* 摘要的精确成本（元调用 usage） */
    uint32_t uCompactionCount;
    size_t   iEntryTotal, iEntryActive;
} xllm_session_stats;

xllm_session* xllmSessionCreate(const xllm_session_config*,
    xllm_client* pClient /* 借用 */, xllm_error*);
void xllmSessionDestroy(xllm_session*);

/* 便捷面：渲染 → 调用（回调透传）→ 入账 → 治理判断 → 按需压缩 → 返回 */
xllm_result xllmSessionSend(xllm_session*, const char* sUserText,
    const xllm_stream_callbacks* pCallbacks,
    xllm_response** ppResponse /* 所有权移交 */, xllm_error*);

/* 细粒度面（xwork 等 agent 宿主用） */
xllm_result xllmSessionComplete(xllm_session*,       /* 渲染+调用，不追加历史 */
    const xllm_stream_callbacks*, xllm_response**, xllm_error*);
bool xllmSessionBeginTurn(xllm_session*, uint64_t* puTurn);
bool xllmSessionAddUser(xllm_session*, const char* sText);
bool xllmSessionAddAssistant(xllm_session*, const xllm_response*);
bool xllmSessionAddToolResult(xllm_session*, const char* sCallId, const char* sText);
bool xllmSessionEndTurn(xllm_session*);

/* 压缩事务（手动 / 观察自动压缩） */
bool xllmSessionCompact(xllm_session*, const char* sFocusHint /*可空*/,
    xllm_error*);
bool xllmSessionMaybeCompact(xllm_session*, bool* pbCompact /*出参*/);

bool xllmSessionGetStats(const xllm_session*, xllm_session_stats*);

/* 持久化（v2 语义继承） */
bool xllmSessionSave(xllm_session*, xllm_error*);
xllm_session* xllmSessionLoad(const xllm_session_config*, xllm_client*,
    xllm_error*);
bool xllmSessionRecover(const xllm_session_config*, xllm_client*, xllm_error*);
xllm_session* xllmSessionFork(const xllm_session*, xllm_error*);
```

测试接缝：`xllmSessionCreateForTest(config, call_proc, pUserData)`——注入
`xllm_result (*)(void*, const xllm_request*, xllm_response** , xllm_error*)`
替代真实 client，测试可**脚本化 usage 返回值**（精确占用完全可控）。

## 6. 持久化

继承 v2 已验证的全套（不重新设计）：原子 JSON 快照、写前 NDJSON 日志
（begin_turn / add_message / compact 三类事件 + 全局 seq）、checkpoint 清日志、
重放去重、撕裂尾截断、完整损坏记录显式拒绝、fork 深分叉。

新增记录：`compact` 事件携带 `{generation, throughEntry, summaryText, usage{...}}`，
重放时重建摘要对象与代数。

可选扩展（登记不做默认）：**压缩条目落盘卸载**——compact 事件已含完整信息，内存中
可将已压缩条目降级为轻量占位（修复 v2 审计的"账本无界增长"），config 开关控制。

## 7. 文件布局

```text
extlibs/xllm-session/
├── xllm-session.h        公共 API
├── xllm-session.c        unity 入口
├── xllm-session-xrt.c/h  XRT 桥接（现收全量，收窄为五模块）
└── src/
    ├── session_internal.h
    ├── session_core.c      账本 / 轮次 / 克隆
    ├── session_render.c    序列化标签 / 渲染（system+摘要+尾窗）
    ├── session_govern.c    精确占用缓存 / 阈值判据 / 有界增量
    ├── session_compact.c   Pi 压缩事务（切点/元调用/质量门/提交）
    ├── session_journal.c   NDJSON 日志（继承）
    └── session_persist.c   快照/恢复/fork（继承）
```

## 8. 测试策略

1. **注入式假调用**：全部治理/压缩逻辑离线可测；usage 脚本驱动（占用爬升→触发→
   压缩→占用回落的全生命周期断言）；
2. 压缩事务：候选序列化字节级断言（标签/截断/工具对）、滚动迭代（二代摘要包含
   一代内容）、质量门失败回滚、overflow 先压后试、split-turn 超大轮；
3. 渲染确定性：同账本同摘要 → 相同消息序列；三代言渲染由 xllm 已有测试背书；
4. journal/快照/恢复/fork：v2 用例迁移 + compact 事件重放；
5. 真网冒烟（可选，env 门控）：GLM 云长对话真实压缩一轮，断言元调用 usage 入账。

## 9. 实施顺序

- M1 账本 + 细粒度入账 + 渲染（继承 v2 资产，渲染改为 system+摘要+尾窗三段）；
- M2 精确治理（usage 反馈缓存、有界增量判据、软/硬阈值）；
- M3 Pi 压缩事务（切点/序列化/元调用/滚动摘要/质量门/overflow）；
- M4 便捷面（Send/Complete）+ 画像取窗口 + journal 的 compact 事件；
- M5 加固（OOM 注入、压缩条目卸载选项、真网冒烟）。

M1/M2 可先行交付（不含压缩的"简单版"即此前 demo-chat 的诉求），M3 是本设计的核心增量。
