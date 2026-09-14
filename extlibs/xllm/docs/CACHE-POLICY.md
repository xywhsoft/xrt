# 缓存与路由策略（GAP-CACHE-HINT v2）

状态：**定稿（2026-09-14）**。本文是 xllm 缓存/路由语义的唯一权威口径；v1 的
"禁写缓存"目标经实测证伪后废除。字段形状**文档冻结、代码惰性**——未触发前不进
公共头文件。

## 0. 重定性（立此存照）

- **`store:false` = wire 对齐，不是缓存控制**。OpenAI 语义里 `store` 管请求/响应
  数据留存，与自动前缀缓存互不相干；GLM/qwen 接受即忽略；llama.cpp 的复用由
  服务端 `--cache-reuse` 与槽位策略全权决定。pi 每请求携带 `store:false` 是数据
  留存对齐，照做即可。
- **"禁写缓存"作为目标废除**。现有方言栈不存在真正的禁写机制；v1 设计的
  `bDisablePromptCache` 是背后无机制的旋钮。唯一"写了有代价"的 Anthropic，其
  缓存本就是断点 opt-in——不加断点即未写，无需开关。

**已落地（随 v2 定稿实施）**：

1. `store:false` 上提到请求构建公共路径（completions 与 Responses 两个序列化器，
   Anthropic 天然无此字段）；调用方 extraBody 自带 `"store"` 键时让位，避免浅合并
   出重复键。真网验收：GLM 云与本地 qwen 均接受（live_store_check 往返成功）。
2. xllm-session 压缩元调用每次携带**新 UUID 路由键**（私有头
   `xllm-routing-key`，经现成 `pExtraHeaders` 机制，零 xllm 字段）——one-off
   提示进独立命名空间，不占用会话亲和槽（pi "全新 routing session id" 的落地半）。

## 1. R1：路由/缓存命名空间键（双向键）

语义：一个请求级字符串键。**同键 = 同命名空间**（正向亲和：路由型后端上同会话
落同槽 = 缓存命中）；**每次新键 = 隔离**（one-off 提示不污染会话缓存）。隔离与
亲和是同一机制的两个方向。

方言映射（评估修正版）：

| 方言 | 映射 | 备注 |
|---|---|---|
| OpenAI-compat | 私有 header | 真 OpenAI 端点官方有 `prompt_cache_key`，届时优先官方字段；私有 header 对杂牌兼容服务器最安全（未知 body 字段可能 4xx，未知 header 一律忽略） |
| GLM | 待证 | 触发条件见 §4；有官方路由头则用官方 |
| Responses | 同 OpenAI-compat | |
| **Anthropic** | **no-op** | `metadata.user_id` 官方语义是滥用监控，从未承诺缓存亲和——Anthropic 的唯一真杠杆是 R2 断点。不给无机制的字段找语义 |

消费者约定：xllm-session 元调用 = 新 UUID（已落地）；宿主常规轮次 = 稳定会话键
（宿主经 `pExtraHeaders` 自理，xllm 不加字段直至触发条件成立）。

## 2. R2：Anthropic 前缀钉扎（唯一的真金白银）

`bPinPrefixCache`（request 级 bool，**触发后才进头文件**）：Anthropic 序列化器
自动放 `cache_control: ephemeral` 断点，位置三处——最后一个 system 块、最后一个
tools 定义、摘要 user-bridge 之后（第三处恰好是压缩前缀的结构缝合线，一处设计
吃两个机制）。断点上限 4，用 3 留 1 余量。

经济账：缓存写 1.25×、读 0.1×——省钱只在**重复轮次**成立；元调用绝不钉扎（默认
无断点，天然不钉）。前缀需过最小可缓存长度（约 1024 token），短会话钉了也无费。

其他方言置位 = 诊断位标 no-op。读侧验证闭环现成：`uCachedInputTokens` /
`uCacheWriteTokens` 已在 usage 里。

## 3. 明确不做清单

- 不做 `bDisablePromptCache`（撒谎旋钮）；
- `store:false` 不升一等字段（wire 细节不是语义，留在序列化器里）；
- 不为"禁写"引入任何配置面。

## 4. 诊断位与触发条件

R4 诊断位（`xllm_diagnostics` 追加 applied/no-op，每机制 1 bit）**跟随各机制的
落地批次**，不预置——防完成度膨胀的锚。

- **R2 触发** = 首次真实 Anthropic 使用；
- **R1 触发** = 实测到路由效应存在：① GLM 官方路由头行为（查文档+实验）；
  ② 自建 llama-server 多槽下元调用是否劣化主对话 TTFT。注：`llama-server`
  多槽（`--parallel`）的槽位选择即最长公共前缀匹配，元调用天然落别的槽——
  先试加槽，不行再上第二实例/端口（物理隔离），而不是 xllm 旋钮。

## 5. 字段形状（文档冻结）

```c
/* xllm_request 追加（触发各自条件后才实体化进 xllm.h） */
char* sRoutingKey;      /* R1: 路由/缓存命名空间键；NULL = 无 */
bool  bPinPrefixCache;  /* R2: Anthropic 前缀钉扎；其他方言 no-op */
```

加法纪律保证后补字段零破坏，故不提前入库——避免死字段与"置位即静默 no-op"
（与 R4 要防的是同一类问题）。

## 6. 验收标准

1. `xllmClientBuildRequestJson` 按方言核对映射与断点位置（离线）；
2. 诊断位如实反映 applied / no-op；
3. Anthropic 实测：钉扎后 `uCachedInputTokens > 0` 且多轮费用下降；
4. 自建 llama-server 场景：压缩元调用与主对话 TTFT 互不劣化（本栈唯一可现场
   验证的"缓存健康"指标，读侧 `uFirstTokenMs` 现成）。

已完成项的验收记录：store wire 三断言（completions/responses 携带、anthropic
不含、extraBody 覆盖不重复）进 xllm 测试套件；元调用 UUID 头断言进 session
测试套件；GLM/qwen 真网接受证据见 §0。
