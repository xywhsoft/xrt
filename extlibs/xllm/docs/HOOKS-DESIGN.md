# xllm 生命周期回调与裸写手感（HOOKS 设计稿 v1）

状态：**已实施（2026-09-15，H1-H4 全部落地，测试全绿）**。实施中的两处设计升级：
① pOnResponseBody 升级为两段式——发送前以 sBody=NULL 探询，赋值即离线注入
（跳过网络，回放/假模型零成本）；线上则收包后触发（录制/替换）。
② pOnToolCall 剔除以响应诊断位 bToolCallDropped 可见。目标一句话：**让纯手写 C 直用 xllm 开发 agent
零绕路**——不引 xllm-session、不引任何循环框架，二十几行写出完整 agent 轮次，
且每个生命周期点都能拦截、改写、中止。

## 0. 设计原则

1. **回调是数据缝，不是通知器**：每缝拿到的指针可原址改写；返回 false = 中止
   本次调用（错误码 `XLLM_ERROR_HOOK`，消息标注缝名）。不搞三态枚举——原址
   改写天然免"我改了"标记。
2. **循环永远是宿主的**：xllm 只做"一次模型调用"的全生命周期缝。对话开始 =
   请求组装完成，回复完成 = 响应交付前——xllm 无会话概念，缝定义在调用上。
3. **两级注册**：client 级默认（`xllmClientSetHooks`，借用结构体）+ 请求级覆盖
   （`xllm_request.pHooks`，整结构体替换该次调用的全部缝——回放模式换一套缝
   正是这个语义）。字段级合并不做，避免 userdata 二义。
4. **与流回调正交**：现有 `xllm_stream_callbacks`（增量观察）原样保留；新钩子
   是"结构化数据点"，流是"像素流"。一个调用两者可同时挂。

## 1. 六缝总表（xllm_hooks）

```c
typedef struct xllm_hooks {
    /* 一、发送前：请求结构体已组装完成（校验/归一化之后）。*/
    bool (*pOnRequest)(xllm_client* pClient, xllm_request* pRequest, void* pUserData);
    /* 二、出向 wire：请求体已序列化为 JSON 文本。*/
    bool (*pOnRequestBody)(xllm_client* pClient, xllm_wire* pWire, void* pUserData);
    /* 三、瞬态重试前：观察诊断，否决 = 立即失败。*/
    bool (*pOnRetry)(xllm_client* pClient, const xllm_diagnostics* pDiagnostics,
        uint32_t uNextAttempt, void* pUserData);
    /* 四、入向 wire：原始响应体（非流式=整包 JSON；流式=SSE 全文聚合）。*/
    bool (*pOnResponseBody)(xllm_client* pClient, xllm_wire* pWire, void* pUserData);
    /* 五、流中工具调用成形：arguments 拼接完成的瞬间，早于响应交付。*/
    bool (*pOnToolCall)(xllm_client* pClient, xllm_tool_call* pCall,
        uint32_t uIndex, void* pUserData);
    /* 六、回复完成：响应已组装，交付调用方之前。*/
    bool (*pOnResponse)(xllm_client* pClient, xllm_response* pResponse, void* pUserData);
    void* pUserData;
    uint32_t uReserved[4];
} xllm_hooks;

typedef struct xllm_wire {
    uint32_t uAttempt;        /* 重试序号，从 1 起 */
    char* sBody;              /* NUL 结尾；可原址改写，或整段替换（见所有权） */
    size_t iBodySize;
    size_t iBodyCapacity;     /* 仅出向有效：原址改写的上界 */
    uint32_t uReserved[4];
} xllm_wire;

void xllmClientSetHooks(xllm_client*, const xllm_hooks* pHooks /*借用，NULL=摘除*/);
/* xllm_request 追加一个字段： */
const xllm_hooks* pHooks;     /* 该次调用覆盖 client 级钩子；借用；NULL=用 client 级 */
```

| # | 缝 | 时机 | 可改数据 | 典型用法 |
|---|---|---|---|---|
| 1 | `pOnRequest` | 校验后、序列化前（**对话开始**） | 整个 `xllm_request`：消息增删改、模型/温度/预算、extraBody、headers（借用存储须活到调用结束） | 注入系统段、脱敏、按会话动态改 max_tokens、审计 |
| 2 | `pOnRequestBody` | 每次尝试发送前 | JSON 文本原址改写（≤iBodyCapacity）或整段替换 | 方言补丁、抓包录制、计费网关 |
| 3 | `pOnRetry` | 瞬态失败后、退避前 | 只观察；false=停止重试立即失败 | 重试预算策略、限流观察 |
| 4 | `pOnResponseBody` | 收完整包后、解析前 | 原始字节同上规则 | **回放注 canned 响应**、字节级日志 |
| 5 | `pOnToolCall` | 流中每个 tool call 的 arguments 拼完瞬间 | `sArgumentsJson`/`sName` 可替换指针（原串由库释放）；false=从响应中剔除该调用 | 参数改写、**流中提前起跑工具执行**（模型还在生成后续调用）、审批钩子 |
| 6 | `pOnResponse` | 组装完成、交付前（**回复完成**） | 整个 `xllm_response`：content/blocks/usage | 记账、内容改写、红线过滤 |

触发时序（一次调用，含一次重试）：

```
pOnRequest ──► pOnRequestBody(#1) ──► [流事件… pOnToolCall×n] ──(瞬态失败)
                    ▲                                              │
                    └──────── pOnRetry(同意) ◄─────────────────────┘
pOnResponseBody(#1) 解析 ──► pOnResponse ──► 交付
（失败尝试不触发四/五/六；成功尝试五在流中、四在包尾、六在组装后）
```

## 2. 所有权与线程契约

- **原址改写**永远安全（出向缓冲给了容量上界）。
- **整段替换**：`pWire->sBody = 自分配串` 并更新 `iBodySize`（≤ 原容量时也可只换
  指针）；旧串由库释放，新串库按默认分配器（`free`）释放——自定义分配器构建下
  宿主须用配套分配。`pOnToolCall` 换串同理。
- 线程：同步 `Complete` 在调用线程触发；`Start` 异步在引擎 worker 触发（与流回调
  同纪律：不阻塞、线程安全、除当前调用外可自由调库）。
- 禁重入当前调用：缝内不得再对该调用 Start/Wait/Cancel。
- 缝内抛错（false）：连接按取消语义回收，`xllm_error` = `XLLM_ERROR_HOOK` +
  缝名；流回调的取消返回 false 语义不变（二者独立）。

## 3. 裸写手感的地基：xllm_history（消息账本容器）

裸写 agent 的第一痛点不是循环（循环就二十行），是**历史数组的深拷贝与组装**
（demo-chat 手搓了 65 行还带出过 UAF）。补一个薄容器（~180 行，零策略——所有
治理留给 session 层，这里只有机械）：

```c
typedef struct xllm_history xllm_history;

xllm_history* xllmHistoryCreate(void);
void xllmHistoryDestroy(xllm_history*);
size_t xllmHistoryCount(const xllm_history*);
const xllm_message* xllmHistoryAt(const xllm_history*, size_t i);   /* 借用 */

bool xllmHistoryAdd(xllm_history*, const xllm_message*);            /* 深拷贝 */
bool xllmHistoryAddText(xllm_history*, xllm_role, const char* sContent);
bool xllmHistoryAddFromResponse(xllm_history*, const xllm_response*); /* 文本+思考+工具调用 */
bool xllmHistoryAddToolResult(xllm_history*, const char* sCallId, const char* sContent);
bool xllmHistoryRemove(xllm_history*, size_t i, size_t iCount);     /* 手滑窗用 */
bool xllmHistoryAppendInto(const xllm_history*, xllm_request*);     /* 全量灌进请求 */
```

刻意不做：token 估算、压缩、journal（session 层职责）；不做树/分支（同上）。

## 4. 裸写 agent 的标准形（设计验收：这段代码就是手感）

```c
/* 纯 xllm 的完整 agent 轮次——无 session、无任何框架，~30 行 */
static bool on_request(xllm_client* c, xllm_request* r, void* ud) {
    xllmRequestAddTextMessage(r, XLLM_ROLE_SYSTEM, "你是墨斗。");  /* 对话开始前改写 */
    return true;                                                    /* false=拦截 */
}
static bool on_tool_call(xllm_client* c, xllm_tool_call* call, uint32_t i, void* ud) {
    /* 流中提前起跑：arguments 一成形就开始执行（可与模型生成重叠） */
    early_dispatch((agent_state*)ud, call);
    return true;
}

xllm_history* h = xllmHistoryCreate();
xllm_hooks hk = { .pOnRequest = on_request, .pOnToolCall = on_tool_call, .pUserData = st };
xllmClientSetHooks(client, &hk);

for (;;) {
    xllm_request req; xllm_response* resp = NULL;
    xllmRequestInit(&req);
    xllmHistoryAppendInto(h, &req);            /* 历史 + tools + model */
    xllmRequestAddTool(&req, "read_file", ...);
    xllmRequestSetModel(&req, "ornith-35b");
    if (xllmClientComplete(client, &req, &stream_cbs, &resp, &err) != XLLM_RESULT_OK) break;
    xllmHistoryAddFromResponse(h, resp);
    if (resp->iToolCallCount == 0) { printf("%s", resp->sContent); xllmResponseDestroy(resp); break; }
    for (size_t i = 0; i < resp->iToolCallCount; ++i) {
        char* out = run_tool(&resp->pToolCalls[i]);        /* 宿主执行 */
        xllmHistoryAddToolResult(h, resp->pToolCalls[i].sId, out); /* 配对入账 */
        free(out);
    }
    xllmResponseDestroy(resp); xllmRequestUnit(&req);
}
```

配套发一个 `examples/agent_loop.c`（此形全文 + 双工具），作为"裸写即最佳实践"
的活文档。

## 5. 回放配方（Tier-3 回放免费实现）

- **录制**：client 级挂 `pOnRequestBody` + `pOnResponseBody`，把 `(attempt, body)`
  落盘——字节级真值，含方言序列化结果。
- **回放**：换一套钩子，`pOnResponseBody` 注入录制的原始字节（SSE 文本原样回灌，
  解析路径与线上完全一致 → 确定性）；`pOnRequestBody` 里断言出向与录制一致
  （提示词漂移检测）。
- mdo 的 `OnWire` 缝即此二缝；session 的压缩元调用经绑定 client 自动被同套缝
  覆盖（录制回放连压缩也确定性）。

## 6. 实施清单

| 项 | 内容 | 预估 |
|---|---|---|
| H1 | `xllm_hooks`/`xllm_wire` 类型 + `xllmClientSetHooks` + `request.pHooks` + 六缝分发（transport/client 两处接线） | ~200 行 |
| H2 | `xllm_history` 容器 | ~180 行 |
| H3 | 测试：六缝各一（改写生效/中止语义/两级覆盖/重试缝/工具剔除/wire 替换）+ history 深拷贝往返 | ~300 行 |
| H4 | `examples/agent_loop.c` + README 手感节 | ~150 行 |

ABI：纯加法（新类型/新函数/request 尾部一个借用指针字段）。分发起点：Complete
与 Start 共用的请求提交路径，保证两条路行为一致。

## 7. 明确不做

- 不做循环/调度/审批逻辑进 xllm（缝足够表达，策略属宿主）；
- 不做缝的字段级合并（整结构体覆盖，语义单一）；
- 不动流回调签名（已稳定，增量语义够用）；
- 不给 wire 缝加 headers 改写（出向头是结构化面，`pOnRequest` 已可改
  `pExtraHeaders`；字节级留给 body）。

## 8. 开放问题（已裁决）

1. `pOnToolCall` 剔除调用（返回 false）时，是否在响应 diagnostics 里记一位
   `bToolCallDropped`，让宿主知道配对账变了？→ **记**：`bToolCallDropped` 已实施
2. `xllm_wire` 入向是否补 `uHttpStatus`？→ **补**：已实施
3. history 范围灌入 → **不做**：Remove + AppendInto 已可表达。
