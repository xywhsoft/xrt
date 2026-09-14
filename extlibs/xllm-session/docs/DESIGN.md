# xllm-session v3 设计定稿（冲刺版）

状态：**M0-M4 已实施并全绿（2026-09-14）**。库 4212 行（v2 2187 → 净增 ~2000），
测试 134 项断言三连跑全绿；xllm 核心 / xllm-memory 回归全绿。M5 遗留三项：OOM
穷举（库走 libc malloc，需先接可注入分配器）、压缩条目落盘卸载选项、真网冒烟。

实施记录的三处偏差（均为简化，行为更保守）：

1. **split-turn 未实现**——切点在整轮边界吸附（保留完整轮次），超大单轮交由
   L2 梯子截断兜底，不生成轮内前缀摘要（§6.2 注释已标）；
2. **估算离线兜底保留**——从未收到任何 usage 反馈的会话沿用 v2 估算阶梯（纯
   账本/离线工作流可用）；一旦收到第一次反馈，估算永久退出该会话决策链（§4）；
3. **D8 画像取窗经 `uContextWindowTokens=0` 显式启用**（CreateBound 内消费
   `xllmClientGetModelProfile`），零值语义同时覆盖 maxOutput/outputReserve/
   summaryTokens 的画像推荐值。

基线：v2 已落地资产（账本/渲染/两阶段压缩事务/journal+快照+恢复，2187 行，89 项
测试全绿）+ xllm 核心 v3（三方言/精确 usage/画像）+ xrt JSONL 模块（主线
2639487c，已审查为成熟档：三层预算/错误位置/原子写/OOM 穷举）。

前置结论（已论证，不再重开）：

- **词法 token 估算不参与任何决策**——实测偏差 2 倍以上且内容类型相关。决策模型 =
  「服务端精确 usage 反馈 + 有界增量」。v2 中估算进决策链的位置（治理阈值、replay
  摘要门）在本版全部清除。
- 压缩方案 = **Pi compaction**（滚动摘要、keep-recent 切点、工具对原子、序列化标签、
  split-turn、结构化 Markdown）。v2 的两阶段事务外壳（Prepare→宿主调用→Evaluate→
  Commit）保留，Pi 方案装进这个外壳。
- **压缩策略回调化**：内置 Pi 方案本身以 ops 函数表形态暴露，宿主可整体或分阶段
  替换；渲染/事件另有独立钩子面（§9）。

## 0. 冲刺版相对草案改了什么

| # | 决策 | 内容 |
|---|---|---|
| D1 | 双层形态 | 核心层保持**无 client**（v2 资产与 89 项测试全保），新增可选绑定 client 的便捷层 |
| D2 | 计量失效语义 | Load/Recover/Fork 后 fill_exact 置无效，首个真实调用即探测，溢出梯子兜底 |
| D3 | journal 迁移 | 重放改用 `xrtJsonlRead`（完整行前缀）；追加侧维持 `xrtFileAppend`+尺寸回滚 |
| D4 | replay 去估算 | replay 摘要门从 token 估算改为结构章节 + 字节上限（修复 v2 journal.c:119） |
| D5 | 溢出梯子 | compact → 尾窗结构截断 → 单消息超窗显式拒绝，三级递进全部登记 |
| D6 | 回环防护 | 创建期静态校验 + 运行期「连续自动压缩无用户轮 → 停自动、报 OVERFLOW」 |
| D7 | 缓存友好性 | 同代内渲染字节稳定；前缀重写只发生在代际边界；stats 暴露缓存命中学 |
| D8 | 窗口来源 | config 显式值 > 绑定 client 画像（`xllmClientGetModelProfile`）> 拒绝创建 |
| D9 | 摘要成本精确入账 | 元调用 usage 存入摘要对象与 compact journal 事件；min/max 默认取画像 |
| D10 | 压缩策略回调化 | `xllm_compaction_ops` 七阶段 vtable，**阶段级 NULL = 该阶段默认**；默认表即 §6 Pi 方案本体（不是第二套代码） |
| D11 | 渲染与事件钩子 | `xllm_session_hooks`：逐条目渲染变换（工作副本 + SKIP）、摘要消息构造、最终请求后处理、统一事件流 |

新增五章：§7 溢出梯子、§8 缓存友好性、§9 钩子体系、§11 JSONL 基底迁移、§12 开源
经验吸取清单。实施顺序重排：JSONL 迁移提为 M0（已解锁且最小）。

## 1. 定位与边界

xllm-session 是**会话治理扩展库**：管理对话历史账本、上下文精确计量、压缩（默认
Pi 方案 + 可替换策略）、渲染与持久化。

做：轮次账本、精确占用反馈、压缩事务（ops 可换）、渲染钩子（system + 摘要 + 尾窗）、
持久化（journal/快照/恢复/fork）、溢出梯子、事件流。
不做：模型调用本身（xllm 职责）、工具执行与 Agent 循环（xwork 职责）、长期事实记忆
（xllm-memory 职责）、多会话编排（上层应用职责）。

## 2. 集成拓扑（D1：双层）

```text
应用 / xwork（Agent 循环、工具执行）          应用（对话类，最短路径）
    │ 自备调用循环，走核心层                     │ 便捷层
    ▼                                          ▼
┌──────────────────────── xllm-session ───────────────────────┐
│ 便捷层（可选）：绑定 xllm_client（借用），Send/Complete、     │
│   自动压缩的默认元调用（ops.pSummarize 的默认实现）           │
│ ─────────────────────────────────────────────────────────── │
│ 核心层（无 client）：账本/轮次/渲染（钩子）/精确治理/          │
│   压缩事务外壳 + 默认 Pi ops（D10）/journal/快照/恢复/fork    │
└─────────────────────────────────────────────────────────────┘
    │ 渲染请求 / 透传流式回调 / 元调用（便捷层内）
    ▼
xllm v3 核心（三方言 / SSE / 重试 / 诊断 / 画像） → xrt（wire/TLS/引擎）
```

- 核心层不知道 client 存在：渲染产出 `xllm_request`，谁调用、怎么调用是宿主的事。
- 便捷层是薄壳（预计 ~170 行）：绑定 client（借用，不拥有不销毁），Send =
  渲染→调用→入账→治理→按需压缩→返回；自动压缩元调用 = 绑定 client 的普通调用
  （= 默认 ops.pSummarize）。
- 两阶段压缩事务在核心层：宿主拿到 `xllmCompactionPrompt()` 自行调用模型再
  `Evaluate+Commit`——这本身就是测试接缝（假调用注入 usage 脚本），也是宿主自带
  摘要模型时的手动通道（此时 ops.pSummarize 置 NULL，自动路径退化为「提示不可用，
  请手动驱动」）。

XRT 依赖面（桥接宏，M0 起收窄为）：`JSON_READ / JSON_WRITE / JSONL_READ /
FILE_WHOLE / PATH / TIME`。`DIR` 若快照目录自动创建仍需要则保留，M0 时定案。

## 3. 对象模型

```text
xllm_session
├── config          窗口/预留/尾窗/阈值/摘要风格/字节上限/持久化路径
├── client          借用的 xllm_client（便捷层，可空）
├── ops             借用的压缩策略表（可空 = 全默认，D10）
├── hooks           借用的渲染/事件钩子（可空 = 无钩子，D11）
├── ledger          条目账本（v2 继承：全局递增 seq、turn 归属、PINNED/SYNTHETIC 标记）
├── summary         滚动摘要对象（Pi CompactionEntry 适配，见下）
├── governance      fill_exact 缓存 + 有效性位 + 有界增量 + 阈值状态 + 回环计数
└── journal         写前日志（v2 继承，重放迁 JSONL，见 §11）
```

摘要对象：

```c
typedef struct xllm_session_summary {
    char*    sText;                 /* 结构化 Markdown 摘要体 */
    uint64_t uThroughSequence;      /* 摘要覆盖 [0, throughSequence]，与尾窗切点对齐 */
    uint32_t uGeneration;           /* 摘要代数，每压缩一次 +1（渲染前缀稳定性的代际锚点） */
    uint64_t uPromptTokensAtBirth;  /* 元调用精确 prompt_tokens（含缓存字段） */
    uint64_t uOutputTokensAtBirth;  /* 元调用 output_tokens（摘要精确成本） */
} xllm_session_summary;
```

关键差异（相对 Pi）：Pi 的 `tokensBefore` 是估算；我们的 AtBirth 是**元调用的服务端
精确值**——摘要的真实成本第一次可以被精确记账并写入 journal。

## 4. 上下文计量：精确反馈回路（D2）

### 4.1 数据流

```text
每轮真实调用完成后（xllm_response->tUsage 已到手）：
  fill_exact    = usage.uInputTokens + usage.uOutputTokens      ← 服务端真数
  cache_hit     = usage.uCachedInputTokens                       ← 仅观测（D7）
  increment_max = max_tokens（调用方设置）
               + 用户消息字节上限 → token 折算上界（config，见 4.3）
               + 工具结果字节上限 × 并发工具数上限（config）
               + 结构开销常数（角色/JSON 包裹，默认 256 token）
  判据：fill_exact + increment_max > window - reserve → 在本轮边界触发压缩
```

- 窗口来源优先级（D8）：config 显式 > 绑定 client 画像
  （`xllmClientGetModelProfile` → `uContextWindowTokens/uMaxOutputTokens/
  uRecommendedOutputReserveTokens`）> 两者皆无 → **创建失败**，不隐式猜测；
- `reserve` 默认取画像 `uRecommendedOutputReserveTokens`，无画像 16384（Pi 默认）；
- `xllmEstimate*` 保留在 xllm 核心（UI 可选用），本库不调用。
- 判据命中后的动作由 `ops.pShouldCompact`（§9.2）裁决：默认 = 执行压缩；宿主可
  否决（本Turn 不压）——**判据与动作分离**，策略可换、账本不变。

### 4.2 失效语义

`fill_exact` 是「上一次真实调用」的观测值，三种情况失效：

| 事件 | 处理 |
|---|---|
| Load / Recover / Fork | `bFillExactValid = false`；stats 报 UNKNOWN；**不触发软压缩**（无据可依） |
| 压缩提交后 | `bFillExactValid = false`；下一轮真实调用重新标定（压缩后渲染必然变化） |
| 配置变更（窗口/预留/尾窗） | 同上 |

首个失效后的真实调用**就是探测**：正常则 usage 到手恢复有效；若 provider 报超窗
错误 → 走 §7 溢出梯子（先压后试）。不引入估算兜底，最坏代价一次失败调用。

### 4.3 字节上限的 token 折算（仅入账上界，非决策）

用户消息/工具结果按**字节上限**（确定性、入账时一次定型）配置，增量预算中按
`字节/4` 折算为 token 上界参与 `increment_max`——这是**最坏情况包络**而非估算值：
宁可高估增量提前一轮压缩，也不让决策依赖内容相关误差。

## 5. 估算在方案里唯一的残留位置

Pi 的切点回溯需要逐消息成本（从最新往回累计到 `keepRecentTokens` 找切割边界）。
逐消息精确 token 数不可得（服务端只报总量），此处用每消息估算**仅做结构性切分**：

- 压缩**是否触发**由精确值决定（4.1）——决策不受估算误差影响；
- 切点只决定「尾窗大致保留多少」，偏 20% 无正确性后果，下一轮真实 usage 立即自校正；
- 文档与注释标注此边界，防止估算被重新引入决策。
- 自定义切点策略（`ops.pPlan`，§9.2）可完全绕开估算回溯（例如按轮数切）。

## 6. 压缩：默认方案（Pi compaction）

§6 描述的是**默认策略**的行为契约；每个阶段的可替换接缝在 §9.2 的 ops 表中逐一
对应（阶段名互相引用）。

### 6.1 触发

| 原因 | 时机 | 行为 |
|---|---|---|
| threshold | 轮边界（工具结果入账后、下个 assistant 前；新用户消息前），4.1 判据命中 | `ops.pShouldCompact` 裁决后执行压缩 |
| overflow | 硬阈值（默认 90%）命中或 provider 报超窗错误 | **先压缩成功再重试本轮**（§7 梯子；不经 pShouldCompact——正确性兜底不可否决） |
| manual | 调用方显式触发 | `/compact` 等价入口，可带聚焦指令（不受回环计数限制） |

压缩只在**完整轮次边界**执行：未完成的工具调用对不进入候选。回环防护见 §7.3。

### 6.2 切点规则（Pi 原样；对应 ops.pPlan）

从最新条目回溯，按估算累计到 `keepRecentTokens`（默认 20000，画像
`uRecommendedSummaryTokens` 可覆盖）：

- 合法切点：user / assistant / system-lift 后的普通消息边界；
- **工具结果与其 tool call 原子**——切点落在配对之外；
- 超大单轮（回溯预算耗尽仍在同一轮内）：**split-turn 摘要**——该轮前缀单独生成
  一份摘要并入主摘要。

### 6.3 候选序列化（Pi 原样；对应 ops.pSerialize）

```
[User]: <文本>
[Assistant thinking]: <推理内容，可配置省略>
[Assistant]: <正文>
[Assistant tool calls]: <name>(<args>)
[Tool result]: <截断至 uToolResultCapBytes 字节，标注截断量>
```

标签防止模型把序列化文本当作待继续的对话；工具结果截断量标注；**累计文件/工具操作
追踪**（read-files/modified-files）：从本批候选的工具调用 + 上一代摘要 details 双向
提取，跨多次压缩保留完整操作史（进 6.5 格式 Critical Context 节）。

### 6.4 元调用与滚动迭代（对应 ops.pBuildPrompt / pSummarize）

```
prompt = 摘要指令模板（general / coding / 自定义）
       + 上一代摘要（存在时，作为迭代上下文）      ← Pi 滚动：摘要的摘要
       + 本批候选的序列化文本
调用   = ops.pSummarize：默认（便捷层）= 绑定 client 非流式调用；
         核心层手动路径 = 宿主自行执行 CompactionPrompt
         max_tokens = uSummaryMaxTokens（默认取画像 uRecommendedSummaryTokens）
```

- 重复压缩从上一代 `uThroughSequence` 起算：上轮尾窗在下轮成为新候选，主摘要滚动；
- 元调用 usage 写入摘要对象（§3）与 compact journal 事件（D9）；
- 流式回调透传 NULL（摘要一次性整取）；元调用**不计入会话占用缓存**；
- 元调用每次携带新 UUID 路由键（头 `xllm-routing-key`，one-off 命名空间隔离）；
  `store:false` 由 xllm 公共 wire 路径自动携带——缓存/路由口径见上游
  `extlibs/xllm/docs/CACHE-POLICY.md`（GAP-CACHE-HINT v2）。

### 6.5 摘要格式与质量门（对应 ops.pEvaluate）

coding 预置（Pi 原样）：`## Goal / ## Constraints & Preferences / ## Progress
(Done/In Progress/Blocked) / ## Key Decisions / ## Next Steps / ## Critical
Context`（内嵌 `<read-files>/<modified-files>`）。general 预置为中文等价章节。

质量门（替换 v2 八章节门，去估算化）：**必选章节存在（位掩码）+ 摘要字节上限
（`uSummaryMaxBytes`，默认 32 KiB）+ 代数单调**。结构校验不假装理解语义；
`xllmCompactionEvaluateSummary()` 公共 API 内部调用**当前 ops 的 pEvaluate**
（预览/提交一致性保留，v2 资产），字段改为字节。

### 6.6 提交与渲染

提交原子性（v2 事务语义保留）：compact **先写 journal 再推进内存**；失败 = 安全中止
（旧摘要与新候选原样保留）。账本条目只标记压缩、不删除；已压缩条目内存卸载为轻量
占位（config 开关，修复 v2 审计的「账本无界增长」）。提交完成经 `ops.pOnCommitted`
与事件流双通道上报。

渲染顺序（钩子接入点见 §9.3）：

```
[PINNED 条目（config 固定段/system，永不被压缩）]   ← hooks.pRenderMessage
[摘要消息]                                          ← hooks.pRenderSummary
[尾窗逐字条目（含保留下来的工具配对）]               ← hooks.pRenderMessage
[本轮新消息]                                        ← hooks.pRenderMessage
（全部完成）                                        ← hooks.pRenderComplete
```

**摘要角色默认值修正**（读 v2 代码定案）：v2 将摘要注入为 **user 角色 + 连续性前导**
（「Compacted session state. Treat this as authoritative continuity…」，core.c:574
注释说明动机：尾窗以 assistant/tool 开头时，provider 要求首条非 system 消息是 user，
摘要即合法桥）。默认沿用 v2 的 user-bridge（provider 安全）；Pi 原味的「第二条
system」注入经 `pRenderSummary` 钩子可选（Anthropic 方言下多余 system 会折叠进
顶层 system 块，尾窗首条 assistant 时非法——文档明示此差异）。

## 7. 溢出梯子与回环防护（D5/D6）

### 7.1 三级梯子

provider 报超窗错误（或硬阈值命中）时按序执行，每级之间重渲染重试一次：

| 级 | 动作 | 语义 |
|---|---|---|
| L1 | 压缩（§6 完整事务，走当前 ops） | 常规路径，绝大多数在此恢复 |
| L2 | 尾窗结构截断：从切点再向前按**轮边界**丢弃最旧尾窗条目（工具对原子），至 `keepRecentTokens/2` 下限；在丢弃位置注入一条 `SYNTHETIC` system 条目「[更早轮次已截断]」 | 摘要+尾窗仍超窗时（如单轮超大） |
| L3 | 单条消息自身超窗：`AddUser/AddToolResult` 入账时按字节上限预检，直接拒绝并报 `XLLM_ERROR_LIMIT` | 结构性不可能装下，永不进渲染 |

- L2 是**显式代际事件**：截断后 `uGeneration+1`、journal 记 `truncate` 事件
  （`{from_sequence,to_sequence,reason:"overflow_l2"}`），渲染前缀重写仅在此允许；
- L2 截断位置由 session 核心按轮边界规则确定，**不经 ops 定制**（梯子是正确性
  机器，v1 不开放）；L1 仍走当前 ops 全管线；
- 梯子各级失败不静默降级：L1 失败（摘要质量门不过/元调用失败）→ 进 L2；
  L2 到下限仍超 → 报 OVERFLOW 给宿主决策（换模型/改配置），**不再自动循环**。

### 7.2 与 v2 pressure 枚举的关系

`PRUNE`（v2 的工具结果字节裁剪）保留为入账时行为；`COMPACT` = L1；`OVERFLOW` =
梯子穷尽。枚举不动，语义在文档对齐。

### 7.3 回环防护（D6）

- **创建期静态校验**：`keepRecentTokens + reserve + summaryMaxTokens > window` →
  配置非法，创建失败（结构性保证一次压缩至少能腾出 summaryMax 空间）；
- **运行期计数**：连续自动压缩（threshold/overflow 引发）之间若无任何 user 条目
  入账 → 计数 +1；计数 ≥ 2 停止自动压缩，pressure 置 OVERFLOW 交宿主
  （典型成因：keepRecent 配得过大或窗口画像失真）；
- 手动 compact 不受计数限制（用户明确要求）。

## 8. 缓存友好性（D7）

GLM/OpenAI 自动 prompt 缓存与 Anthropic cache_control 都依赖**稳定前缀**。本库规则：

1. **同代字节稳定**：同一 `uGeneration` 内，历史部分渲染结果逐字节确定——消息序
   只追加、不重排；工具结果截断在 **Add 时一次定型**（v2 的 `uToolPruneBytes` 语义），
   禁止渲染期动态截断；
2. **前缀重写只发生在代际边界**：compact（6.6）/ L2 截断（7.1）使 `uGeneration+1`，
   代价是一次缓存全失效，按「每次压缩一次」摊销；
3. **观测**：stats 暴露最近一轮 `uCachedInputTokens/uCacheWriteTokens`（usage 已有
   字段），宿主可据此验证前缀稳定性（连续轮命中即生效）；
4. system 固定段放最前且内容不变（Pi 原样），它是缓存锚点；
5. **渲染钩子（§9.3）是唯一放宽出口**：`pRenderMessage` 允许非确定性输出，违反
   纯度契约的代价是**缓存命中率下降而非正确性损失**——契约写在头文件，库不强制。

本库不负责 cache_control 注点（方言层职责，xllm 核心后续扩展）；只保证「相同账本
状态 → 相同渲染字节」（无钩子或纯钩子时），这使注点策略可安全上移。

## 9. 钩子体系（D10/D11，新增）

三条契约线，互不混用：

| 线 | 结构 | 性质 | 换掉什么 |
|---|---|---|---|
| 压缩策略 | `xllm_compaction_ops` | **策略替换**（决定「怎么做压缩」） | §6 默认方案的任意阶段 |
| 渲染钩子 | `xllm_session_hooks` 前三项 | **轻变换**（决定「模型看到什么」） | 渲染输出的形态 |
| 事件流 | `xllm_session_hooks.pOnEvent` | **纯观测**（知道「发生了什么」） | 无（不可影响行为） |

### 9.1 通用契约

- **借用制**：ops/hooks 指针及其内部函数指针由宿主持有，须长于 session 生命周期；
  Fork 继承指针；**不持久化**（函数指针不落盘），Load/Recover 后宿主负责重挂；
- **阶段级 NULL = 默认**：ops 表任何一个函数指针为 NULL，该阶段用内置默认——
  宿主可整体换、可只换一环（例如仅替换 pSummarize 用自己的小模型）；
- **同线程同步**：全部在调用线程（便捷层 Send 内或宿主显式调用）同步执行；
- **禁止重入**：钩子内不得调用 session 变更 API（Add/BeginTurn/Compact/Checkpoint
  等）；只读 API（GetStats/GetConfig/PendingToolCall*）允许；
- **分配纪律**：字符串出参由被调方 `malloc`、库以 `free` 释放（与库内部 libc 纪律
  一致）；
- **失败语义**：回调返回 false → session 以 `XLLM_ERROR_HOOK`（新增错误码）报错，
  Message 标注阶段名；压缩管线中的失败按 §6.6 安全中止（compact 事件不写 journal）。

### 9.2 压缩策略表 `xllm_compaction_ops`（D10）

```c
typedef enum xllm_compact_decision {
    XLLM_COMPACT_NO = 0,     /* 本轮不自动压缩（下个轮边界重新评估） */
    XLLM_COMPACT_YES         /* 执行压缩 */
} xllm_compact_decision;

typedef struct xllm_compaction_plan {
    uint64_t uThroughSequence;    /* 候选 = (上一代 through, 本值]，须配对完整 */
    uint32_t uReserved[4];
} xllm_compaction_plan;

typedef struct xllm_compaction_ops {
    /* 一、触发裁决（仅 threshold 路径；overflow 梯子不咨询） */
    xllm_compact_decision (*pShouldCompact)(xllm_session*,
        const xllm_session_stats* /*借用*/, void* pUserData);
    /* 二、切点（§6.2；产出配对完整的候选区间） */
    bool (*pPlan)(xllm_session*, uint64_t uPrevThrough,
        xllm_compaction_plan* pPlan, void* pUserData);
    /* 三、候选序列化（§6.3；产出 NUL 结尾文本，malloc） */
    bool (*pSerialize)(xllm_session*, uint64_t uFrom, uint64_t uTo,
        char** psText, void* pUserData);
    /* 四、摘要提示构造（§6.4；产出 NUL 结尾 prompt，malloc） */
    bool (*pBuildPrompt)(xllm_session*, const char* sPrevSummary /*可空*/,
        const char* sCandidates, char** psPrompt, void* pUserData);
    /* 五、元调用（§6.4；默认=便捷层绑定 client；NULL 且未绑定 client
          → 自动压缩不可用，事件流提示，宿主走手动两阶段） */
    bool (*pSummarize)(xllm_session*, const char* sPrompt,
        char** psSummary /*malloc*/, xllm_usage* pUsageOut /*可空，D9*/, void* pUserData);
    /* 六、质量门（§6.5；xllmCompactionEvaluateSummary 公共 API 即此入口） */
    bool (*pEvaluate)(xllm_session*, const char* sSummary,
        xllm_compaction_quality* pQuality /*出入参*/, void* pUserData);
    /* 七、提交观测（只读通知；不得变更会话） */
    void (*pOnCommitted)(xllm_session*, const xllm_session_summary*, void* pUserData);
    void* pUserData;
    uint32_t uReserved[4];
} xllm_compaction_ops;

const xllm_compaction_ops* xllmSessionDefaultCompactionOps(void);
bool xllmSessionSetCompactionOps(xllm_session*, const xllm_compaction_ops* /*NULL=恢复默认*/);
```

- **默认表即 Pi 本体**：`xllmSessionDefaultCompactionOps()` 返回内置 §6 实现的
  函数表；「部分覆写」惯用法 = 拷贝默认表、替换需要的函数指针、Set 回去——
  不存在第二套默认代码路径（NULL 阶段与默认表同一函数）；
- **与两阶段事务的关系**：`PrepareCompaction` 内部顺序调用 ops 二→三→四产出
  prompt；宿主手动拿 prompt 自行调用（= 宿主自己充当阶段五）再 `Evaluate+Commit`。
  自动路径（MaybeCompact/Send 内）则阶段五也走 ops（默认 = 绑定 client）；
- **journal 只存数据**：compact 事件记录 plan/序列化结果所及的 seq 范围、摘要文本
  与 usage——replay **不调用 ops**（重放只验数据，ops 非确定性不破坏持久化）；
- pPlan 产出的区间若破坏工具配对完整性，Commit 前校验拒绝（`XLLM_ERROR_HOOK`）；
- pShouldCompact 只否决 threshold 自动触发；manual 与 overflow L1 不受否决
  （用户明确要求 / 正确性兜底）。

### 9.3 渲染钩子（D11）

```c
typedef enum xllm_render_action {
    XLLM_RENDER_KEEP = 0,     /* 工作副本未被实质修改，按原条目渲染 */
    XLLM_RENDER_MODIFIED,     /* 采用修改后的工作副本 */
    XLLM_RENDER_SKIP          /* 本条目不进本次渲染 */
} xllm_render_action;

typedef struct xllm_session_hooks {
    /* 逐条目变换：收到已克隆的工作副本（含 parts），调用方（session）负责善后。
       契约：同一条目（同 seq 同内容）应产出相同结果——违反只损缓存命中（§8 规则 5），
       不损正确性。SKIP 破坏工具配对完整性 → 渲染报错 XLLM_ERROR_PROTOCOL。 */
    xllm_render_action (*pRenderMessage)(xllm_session*, uint64_t uSequence,
        uint64_t uTurn, uint32_t uEntryFlags /*PINNED/SYNTHETIC*/,
        xllm_message* pWork /*工作副本*/, void* pUserData);
    /* 摘要消息构造：pWork 预填默认形态（user-bridge，§6.6）；可改角色/改写/追加；
       返回 false = 本次渲染不注入摘要（罕见，例如宿主想隐藏压缩痕迹做对照实验）。 */
    bool (*pRenderSummary)(xllm_session*, const xllm_session_summary*,
        xllm_message* pWork /*预填默认*/, void* pUserData);
    /* 最终请求后处理：全部消息就绪后调用；允许临时性内容（时间戳/头信息），
       建议追加在尾部以保护前缀缓存（§8）；返回 false = 渲染失败。 */
    bool (*pRenderComplete)(xllm_session*, xllm_request* pRequest, void* pUserData);
    /* 统一事件流（见 9.4）；纯观测，返回值忽略 */
    void (*pOnEvent)(xllm_session*, const xllm_session_event* pEvent, void* pUserData);
    void* pUserData;
    uint32_t uReserved[4];
} xllm_session_hooks;

bool xllmSessionSetHooks(xllm_session*, const xllm_session_hooks* /*NULL=摘除*/);
```

- `pRenderMessage` 覆盖全部四段（PINNED/摘要除外——摘要有专属钩子/尾窗/本轮新消息），
  每条目收到的是**克隆副本**，MODIFIED 后库直接采用（含 parts 修改，如剔除
  REASONING 部件、脱敏替换）；典型用法：隐藏历史推理、密钥脱敏、按条目过滤；
- `SetHooks` 任意时刻可调，下次渲染生效；不推进代数（钩子不属于账本事实，
  前缀若因换钩子变化，代价同样是缓存失效而非正确性）；
- 钩子渲染路径在 BuildRequest 内；v2 的 prune-裁剪逻辑（`xllm_session__pruned_content`）
  先于钩子应用于条目副本——钩子看到的是「已裁剪」文本，符合「Add/入账时定型」纪律。

### 9.4 事件流

```c
typedef enum xllm_session_event_type {
    XLLM_SESSION_EVENT_TURN_BEGIN = 1, TURN_END,
    ENTRY_ADDED,                    /* uSeqTo = 新条目 seq */
    FILL_UPDATED,                   /* usage 入账，fill_exact 刷新/失效 */
    PRESSURE_CHANGED,               /* uSeqFrom = 新 pressure 值（枚举数值） */
    COMPACT_PREPARE, COMPACT_PLAN, COMPACT_PROMPT, COMPACT_SUMMARY,
    COMPACT_EVALUATE,               /* sText = 质量门结论 */
    COMPACT_COMMIT, COMPACT_ABORT,  /* sText = 摘要预览/中止阶段名 */
    LADDER_TRUNCATE,                /* uSeqFrom..uSeqTo = 被丢弃区间 */
    JOURNAL_RECORD,                 /* uSeqTo = journal_sequence */
    CHECKPOINT_SAVED, SESSION_RECOVERED, SESSION_FORKED
} xllm_session_event_type;

typedef struct xllm_session_event {
    xllm_session_event_type eType;
    uint64_t uTurn;
    uint64_t uSeqFrom, uSeqTo;
    const xllm_session_stats* pStats;   /* 调用栈上的临时快照，仅本次回调有效 */
    const char* sText;                  /* 可空：摘要预览/标签/结论 */
} xllm_session_event;
```

单回调覆盖全部生命周期——UI（demo-chat 类）拿一个入口即可驱动占用仪表、压缩进度、
梯子提示；事件在动作**发生点**同步发出（COMPACT_* 六相覆盖 §6 管线全程），不发
「将然」事件（除 PREPARE），保证观测与账本一致。

## 10. 公共 API（v2 全保留 + 冲刺增量）

v2 现有面（`xllm-session.h` 153 行）**全部保留、签名不动**：Create/Fork/Destroy、
BeginTurn/AddMessage/AddText/AddAssistantResponse/AddToolResult、GetTail/
PendingToolCall*、GetStats/BuildRequest、PrepareCompaction/CompactionPrompt/
EvaluateSummary/CommitCompaction/CompactionDestroy、Save/Load、EnableJournal/
Checkpoint/Recover。§9.2/§9.3 已给出 ops/hooks 声明。其余增量：

```c
/* --- config 增量字段（拼在 v2 结构尾部，ConfigInit 给默认值） --- */
uint32_t uKeepRecentTokens;     /* 尾窗切点预算，默认 20000 */
uint32_t uSummaryMaxBytes;      /* 摘要字节上限（质量门），默认 32 KiB */
uint32_t uUserMessageCapBytes;  /* 用户消息字节上限，0 = 不限（L3 预检） */
uint32_t uToolResultCapBytes;   /* 工具结果字节上限（Add 时定型），默认 2000 */
uint32_t uToolResultTotalCapBytes; /* 单轮工具结果累计上限，0 = 不限 */
uint64_t uJournalMaxBytes;      /* journal 重放预算，默认 64 MiB */
const char* sSummaryStyle;      /* "general" | "coding" | 自定义模板路径 */
const char* sSnapshotPath;      /* 便捷层默认快照路径 */

/* --- stats 增量字段 --- */
uint64_t uFillExact;            /* 上轮实测 prompt+output；无效时 saturate 最大值 */
uint64_t uIncrementMax;         /* 本轮最坏增量包络（4.3） */
uint64_t uCachedInputTokens;    /* 上轮缓存命中（观测） */
uint64_t uSummaryTokensExact;   /* 当前摘要精确成本（元调用 usage） */
uint32_t uSummaryGeneration;    /* 摘要代数 */
uint32_t uAutoCompactStreak;    /* 回环计数（7.3） */
bool     bFillExactValid;       /* 4.2 失效语义 */

/* --- 便捷层（D1） --- */
xllm_session* xllmSessionCreateBound(const xllm_session_config*,
    xllm_client* pClient /*借用*/, xllm_error*);
xllm_result xllmSessionSend(xllm_session*, const char* sUserText,
    const xllm_stream_callbacks* /*可空*/, xllm_response** /*所有权移交*/, xllm_error*);
xllm_result xllmSessionComplete(xllm_session*,  /*渲染+调用，不追加历史*/
    const xllm_stream_callbacks*, xllm_response**, xllm_error*);
bool xllmSessionMaybeCompact(xllm_session*, bool* pbCompact /*出参*/);
/* xllmSessionSend = BeginTurn+AddUser+Complete+AddAssistant+EndTurn
   + MaybeCompact（ops 全管线，元调用默认走绑定 client）+ usage 入账 */

/* --- 溢出梯子（核心层） --- */
bool xllmSessionOverflowLadder(xllm_session*, xllm_error*);  /* L1→L2，L3 在 Add 预检 */
```

测试接缝：`xllmSessionCreateForTest(config, call_proc, pUserData)` 注入
`xllm_result (*)(void*, const xllm_request*, xllm_response**, xllm_error*)` 替代
client——usage 可脚本化（占用爬升→触发→压缩→回落全生命周期离线断言）；配合自定义
ops 表，压缩管线每阶段都可离线脚本化。

## 11. 持久化：JSONL 基底迁移（D3/D4）

v2 全套语义继承（写前日志、checkpoint 清日志、重放去重、撕裂尾截断、完整损坏记录
显式拒绝、fork 深分叉），实现层迁移：

### 11.1 重放路径（M0）

```text
v2：xrtFileReadAll → 手搓 memchr 切行 → 逐条 xrtJsonParse → 手搓校验
v3：xrtFileReadAll → 定位最后一个 '\n'（撕裂尾单独截断，见下）
    → 完整行前缀喂 xrtJsonlRead（REJECT_EMPTY_LINES，预算 = uJournalMaxBytes）
    → 逐条 replay 校验（seq 严格 +1、turn 递进、compact 单调 —— v2 逻辑原样）
```

- **为何撕裂尾必须留在 session 层**：xrtJsonlRead 把无换行尾行当合法记录（JSONL
  通用语义），而写前日志语义要求丢弃不完整尾记录——「找最后 `\n`、只喂前缀、
  超出部分截断回滚」三步保留；
- **空白行策略**：journal 是单写者机器写出的，出现空白行即损坏——用
  `XJSONL_READ_REJECT_EMPTY_LINES` 严格拒绝（保持 v2 行为）；
- **收益**：行切分/CRLF/预算/错误位置（record 索引进错误对象）全部委托核心模块，
  session 删除 ~60 行手搓分帧代码；新增 `uJournalMaxBytes` 预算封顶（v2 无界读入）。

### 11.2 追加路径（维持手搓）

xrt JSONL 模块**无 append API**（全量 DOM 形态，已登记为设计边界）。追加 =
单记录手工序列化 + `xrtFileAppend` + 失败按原尺寸回滚（v2 纪律，含 sequence 溢出
检查）。未来若核心库补 `xrtJsonlAppendFile` 便利函数则替换，接口不变。

### 11.3 journal 记录格式（version 1 → 2）

```jsonc
// begin_turn / add_message：不变（v1 兼容）
{"format":"xllm-session-journal","version":2,"journal_sequence":N,
 "operation":"compact","through_sequence":S,"generation":G,
 "summary":"...","usage":{"prompt_tokens":P,"output_tokens":O}}
{"operation":"truncate","from_sequence":S1,"to_sequence":S2,
 "reason":"overflow_l2"}                                    // 新事件（7.1 L2）
```

重放兼容：version 1 文件按 v1 字段重放（compact 无 usage/generation，重建时
AtBirth 置 0、generation 由重放计数补齐）；version 2 拒绝未知 operation。
**replay 摘要门去估算**：v2 的 `xllmEstimateTextTokens(summary) > max` 检查改为
「章节位掩码 + uSummaryMaxBytes」（D4，与 6.5 质量门同一函数，即默认
ops.pEvaluate 的数据面——注意 replay 不调 ops 函数，只复用其结构校验的纯函数体）。

### 11.4 快照

单一 JSON 文档（非 JSONL）：维持 v2（JSON 序列化 + 原子替换写）。增量字段：
摘要对象五元组、governance 失效位、代数。快照版本同步 +1，向后可读。
**ops/hooks 不落盘**：快照只存 `bCustomOps` 布尔（诊断用，标记该会话曾以自定义
策略压缩过），恢复后由宿主重挂（§9.1）。

## 12. 开源经验吸取清单

| 来源 | 经验 | 落点 |
|---|---|---|
| Pi coding agent | compaction 全套（切点/标签/滚动/split-turn） | §6 默认策略 + §9.2 默认表 |
| OpenAI Codex CLI | 会话即 append-only JSONL，resume = 重放；文件人可读 | §11 形态确认（v2 已同构）；标签化序列化保人可读 |
| Claude Code | 轮边界自动压缩 + `/compact` 手动入口 | §6.1 三触发 |
| Gemini CLI | checkpoint 树状分叉 | v2 Fork 保留，加代数与钩子继承 |
| Anthropic/OpenAI prompt caching | 前缀稳定 = 命中率 | §8 五规则（钩子放宽出口） |
| 各家工具结果治理 | 大结果截断 + 截断量标注 | Add 时一次定型（§8 规则 1） |
| 行业共识 | 未完成工具对不可切 | §6.2 原子性 + ops.pPlan 校验 |
| 各家「摘要提示不可换」的痛点 | 摘要策略强绑定宿主逻辑，难以 A/B | §9.2 整条管线可换（策略实验位） |

反面教材（明确不吸取）：按字符数滑窗截断（丢结构、缓存全失效）；估算驱动的提前
压缩（误差不可控）；渲染期动态改历史（破坏确定性）；把观测回调做成可变更行为的
回调（观测与策略必须分线，§9 三契约线）。

## 13. 文件布局

```text
extlibs/xllm-session/
├── xllm-session.h          公共 API（v2 + §9/§10 增量）
├── xllm-session.c          unity 入口
├── xllm-session-xrt.c/h    XRT 桥接（M0 收窄 + JSONL_READ）
└── src/
    ├── xllm_session_internal.h
    ├── xllm_session_core.c     账本/轮次/克隆/fork/事件流分发（v2 继承+事件）
    ├── xllm_session_render.c   渲染管线/渲染钩子接入/字节稳定性（v2 拆出）
    ├── xllm_session_govern.c   精确占用/失效语义/有界增量/回环计数（新）
    ├── xllm_session_compact.c  压缩管线驱动 + 默认 Pi ops 实现（ops 表即函数目录）
    ├── xllm_session_journal.c  journal（追加维持 + 重放迁 xrtJsonl）
    ├── xllm_session_persist.c  快照/恢复（v2 继承 + 新字段）
    └── xllm_session_easy.c     便捷层 Send/Complete/MaybeCompact + pSummarize 默认实现（~170 行）
```

## 14. 测试策略

1. **注入式假调用**（CreateForTest）：usage 脚本驱动占用全生命周期；失效语义
   （Load/Recover/Fork 后 UNKNOWN→首调恢复→超窗→梯子）专项；
2. 压缩事务：候选序列化字节级断言（标签/截断/工具对）、滚动迭代（二代含一代）、
   质量门失败回滚、split-turn、回环计数触发/复位；
3. 梯子：L1 恢复、L1 失败进 L2、L2 到下限报 OVERFLOW、L3 入账拒绝、truncate 事件
   重放、截断后渲染含 SYNTHETIC 标记；
4. 渲染确定性：同账本同摘要 → 逐字节相同（跨代只允许在代际事件后变化）；
5. **钩子契约（新）**：
   - 默认等价性三态：NULL ops / NULL 阶段 / DefaultOps 整表，输出逐字节相同；
   - 自定义 ops 全链路：脚本化 pSummarize 注入坏摘要 → pEvaluate 拒绝 → 安全中止
     → journal 无 compact 事件；pPlan 破坏工具配对 → 拒绝；
   - pRenderMessage 三动作：KEEP/MODIFIED（含剔 REASONING 部件）/SKIP；SKIP 破坏
     配对 → XLLM_ERROR_PROTOCOL；钩子确定性违规场景仅断言缓存观测字段恶化；
   - pRenderSummary 改 system 注入（Anthropic 方言合法性差异单测）；
   - 事件序列断言：一次 Send 的完整事件序（TURN_BEGIN→ENTRY_ADDED→FILL_UPDATED
     →[PRESSURE→COMPACT 六相]→TURN_END）次序与字段；
   - 重入禁令：钩子内调用变更 API → debug 断言拦截（release 返回错误）；
   - 生命周期：fork 继承 ops/hooks、Recover 后重挂、bCustomOps 落快照；
6. journal/JSONL：v2 用例迁移 + 撕裂尾/空白行/超预算/v1→v2 兼容重放；
7. 便捷层：Send 全链路（假 client）、MaybeCompact 边界；
8. OOM 穷举（仓标准）+ 真网冒烟（env 门控）：GLM 云长对话真实压缩一轮，断言元调用
   usage 与缓存字段入账。

## 15. 实施顺序（冲刺排期）

| 里程碑 | 内容 | 预估 |
|---|---|---|
| M0 | JSONL 基座迁移：重放改 xrtJsonlRead、replay 门去估算、journal v2 字段、桥接收窄+JSONL_READ、uJournalMaxBytes | −60/+120 行 |
| M1 | 精确治理：fill_exact 缓存、失效语义、有界增量、画像取窗（D8）、stats 增量、事件流骨架（pOnEvent + 基础事件） | +260 行 |
| M2 | Pi 压缩事务：**管线按 ops 表装配**（默认实现即表内函数）、切点/序列化/元调用/滚动/质量门（字节制）、创建期静态校验、compact 六相事件 | +490 行 |
| M3 | 梯子 + 回环防护运行期 + truncate 事件 + 渲染钩子（pRenderMessage/pRenderSummary/pRenderComplete + 配对校验） | +220 行 |
| M4 | 便捷层 + pSummarize 默认实现（绑定 client）+ journal/快照新字段 | +180 行 |
| M5 | 加固：OOM 穷举、压缩条目卸载选项、真网冒烟 | +测试 ~850 行 |

M0 最小且已解锁（JSONL 模块在主线）；M1/M2 是核心增量；M2 完成时 ops 表与默认
Pi 方案同时交付（回调化不是后补，是管线的组织方式）；M4 完成后即可替换
demo-chat 的手搓滑窗。每里程碑独立可测、可交付、可停。

## 16. 兼容与迁移

- v2 公共 API 签名零破坏（增量字段全部拼尾部 + ConfigInit 默认值）；
- journal v1 可重放（11.3）；快照向后可读，向前拒绝；
- v2 估算函数 `xllmSessionComputeSafetyReserve/OutputReserve`、`xllmEstimate*`
  保留导出（demo-chat 在用），内部调用清零；
- ops/hooks 全部可选且 NULL 安全：不设置任何回调的会话行为 = 本设计的默认方案
  （对 v2 消费者而言行为面只增不减）；
- 89 项 v2 测试全保绿为每里程碑准入条件。
