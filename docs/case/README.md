# XRT 范例解析

> 面向“完整链路、组合能力、真实工程片段”的案例入口。这里与 `docs/guide/` 的区别是：`guide/` 偏教学路径，`case/` 偏完整问题拆解与工程组合。

[返回文档中心](../README.md)

---

## 目录

- [范例解析的定位](#范例解析的定位)
- [当前状态](#当前状态)
- [阅读分层](#阅读分层)
- [主线必读](#主线必读)
- [进阶组合](#进阶组合)
- [高级控制面专题](#高级控制面专题)
- [当前空缺](#当前空缺)
- [与 API 和教学文档的关系](#与-api-和教学文档的关系)

---

## 范例解析的定位

`case/` 目录后续主要承载：

- 一个完整问题如何用 XRT 组合解决
- 多个子系统怎样协同工作
- 从基础运行时一路走到网络、并发、模板、JSON、AI 场景的整条链路

正式案例页应至少覆盖：

- 场景背景
- 目标与约束
- 模块选型和为什么不用别的方案
- 目录结构与关键代码
- 运行步骤
- 常见错误点
- 可以继续扩展的方向


## 当前状态

当前 `case/` 目录已经有一批正式页面，但入口结构之前出现了一个明显问题：

- 前半段案例仍然在补“从简单到完整项目”的主线
- 后半段却逐渐收缩成同一个“本地控制台服务 -> 恢复 / 快照 / 租约 / ownership / journal”主题
- 结果就是案例越来越多，但主线越来越不清晰

这条长链本身不是没有价值，问题在于它不应该继续充当所有读者都要顺着读完的主案例梯度。

因此，这里现在明确改成 3 层入口：

- `主线必读`：第一次系统读案例时应优先走这条线
- `进阶组合`：在主线之后，按你关心的业务方向补读
- `高级控制面专题`：保留那些更重、更长生命周期的恢复 / 共识 / 快照 / 租约 / journal 页面，但不再把它们继续接成无上限主线

如果你想看这套结构背后的原始教学规划，建议同时阅读：

- [教学重建路线图](../guide/ROADMAP.md)


## 阅读分层

建议按下面方式理解 `case/`：

### 1. 主线必读

目标是回答：

- XRT 最常见的模块组合到底怎么落地
- 从小型功能一路走到完整项目，应该怎样逐步升级
- 哪些页面最适合第一次系统读案例

### 2. 进阶组合

目标是回答：

- 同样的基础模块，怎样长成缓存、归档、回温、恢复、跨介质编排这类真实业务功能
- 哪些页面适合在主线之后按专题补读

### 3. 高级控制面专题

目标是回答：

- 更长生命周期的恢复 / 共识 / 快照 / 租约 / journal 问题怎么讲
- 怎样保留这些高价值页面，又不让它们继续挤占主案例梯度

从这一版开始，这条专题线会继续保留，但不再继续作为“主线必读”的自然延长段无限扩张。


## 主线必读

第一次系统阅读案例时，建议先按下面顺序走：

1. [用 xvalue + json 构造配置系统](json-config-system.md)
2. [用 Template 渲染一个 HTML 页面](template-render-html.md)
3. [用 Queue + Future 写一个多生产者 Worker](queue-worker-future.md)
4. [用 Subprocess + File Async 写一个工具链流水线](subprocess-file-async-pipeline.md)
5. [用 XHTTP 走完 URL、代理与 TLS 客户端调用链](xhttp-client-proxy-tls.md)
6. [用 XWS + Queue + Coroutine 写一个双向会话服务骨架](xws-session-queue-coroutine.md)
7. [线程、协程与 Future 协同模型](thread-coroutine-future.md)
8. [用 XRT 写一个最小 HTTP 服务](minimal-http-service.md)
9. [用 XRT 调用一个流式 LLM API](streaming-llm-api.md)
10. [一个完整的 HTTP + JSON + Template 服务链路](http-json-template-chain.md)
11. [用 Charset + Regex + Crypto 导入一个签名规则包](signed-rule-bundle.md)
12. [用 MemPool + AVLTree + List 组织一个会话索引与回收池](session-registry-pool-index.md)
13. [把配置、日志、任务、网络和模板串成一个本地控制台服务](local-console-service.md)

这条主线的目标不是把所有业务变体都读完，而是让你先建立一条稳定的“配置 -> 模板 -> 并发 -> 工具链 -> 网络 -> 服务 -> 完整项目”组合认知。


## 进阶组合

主线之后，建议按专题补读。

### 功能扩展与控制台面板

1. [把本地控制台服务升级成一个子进程探测面板](subprocess-probe-dashboard.md)
2. [把本地控制台服务升级成一个批处理任务面板](batch-task-dashboard.md)
3. [把本地控制台服务升级成一个缓存刷新面板](cache-refresh-dashboard.md)
4. [用 Dict + List + AVLTree 组织一个多租户索引注册表](tenant-index-registry.md)

### 存储、回温与归档

1. [把本地控制台服务升级成一个重试退避面板](retry-backoff-dashboard.md)
2. [把本地控制台服务升级成一个归档面板](archive-dashboard.md)
3. [把本地控制台服务升级成一个冷热分层面板](hot-cold-tier-dashboard.md)
4. [把本地控制台服务升级成一个冷层回温面板](warm-back-dashboard.md)
5. [把本地控制台服务升级成一个冷热滚动归档面板](rolling-tier-archive-dashboard.md)
6. [把本地控制台服务升级成一个自动回温策略面板](auto-warm-policy-dashboard.md)
7. [把本地控制台服务升级成一个冷层回温归档协同面板](warm-archive-coordination-dashboard.md)
8. [把本地控制台服务升级成一个自动回温 + 滚动归档联动面板](auto-warm-rolling-archive-dashboard.md)
9. [把本地控制台服务升级成一个多层存储面板](multi-tier-storage-dashboard.md)
10. [把本地控制台服务升级成一个冷热多级老化面板](hot-cold-multi-level-aging-dashboard.md)
11. [把本地控制台服务升级成一个恢复优先级面板](recovery-priority-dashboard.md)

### 恢复、配额与跨介质编排

1. [把本地控制台服务升级成一个跨介质编排面板](cross-media-orchestration-dashboard.md)
2. [把本地控制台服务升级成一个重链路恢复面板](heavy-recovery-chain-dashboard.md)
3. [把本地控制台服务升级成一个恢复配额面板](recovery-quota-dashboard.md)
4. [把本地控制台服务升级成一个跨机器恢复面板](cross-machine-recovery-dashboard.md)
5. [把本地控制台服务升级成一个分布式编排面板](distributed-orchestration-dashboard.md)


## 高级控制面专题

下面这些页面统一归到“高级控制面专题”。

它们仍然有价值，但用途已经不再是“第一次读案例时一路顺着读下去”，而是：

- 当你明确在做恢复 / 共识 / 快照 / 租约 / journal 这类系统时，再按专题进入
- 当你需要对照某一类长期运行状态字段、调度状态或持久化 checkpoint 时，再按问题定位阅读

### 共识、恢复与快照基线

1. [把本地控制台服务升级成一个持久化协调面板](persistent-coordination-dashboard.md)
2. [把本地控制台服务升级成一个一致性仲裁面板](consensus-arbitration-dashboard.md)
3. [把本地控制台服务升级成一个共识日志复制面板](consensus-log-replication-dashboard.md)
4. [把本地控制台服务升级成一个状态机提交面板](state-machine-commit-dashboard.md)
5. [把本地控制台服务升级成一个日志回放面板](log-replay-dashboard.md)
6. [把本地控制台服务升级成一个快照压缩面板](snapshot-compaction-dashboard.md)

### 接管、handoff 与 ownership

1. [把本地控制台服务升级成一个租约故障切换面板](lease-failover-dashboard.md)
2. [把本地控制台服务升级成一个领导切换恢复面板](leader-handoff-recovery-dashboard.md)
3. [把本地控制台服务升级成一个 compaction 接管面板](compaction-takeover-dashboard.md)
4. [把本地控制台服务升级成一个 ownership 仲裁面板](ownership-arbitration-dashboard.md)
5. [把本地控制台服务升级成一个分布式接管编排面板](distributed-takeover-orchestration-dashboard.md)
6. [把本地控制台服务升级成一个共识式 ownership 面板](consensus-style-ownership-dashboard.md)
7. [把本地控制台服务升级成一个共识式接管恢复面板](consensus-takeover-recovery-dashboard.md)

### 快照调度、租约与 journal

1. [把本地控制台服务升级成一个共识恢复编排面板](consensus-recovery-orchestration-dashboard.md)
2. [把本地控制台服务升级成一个共识快照编排面板](consensus-snapshot-orchestration-dashboard.md)
3. [把本地控制台服务升级成一个快照接管面板](snapshot-takeover-dashboard.md)
4. [把本地控制台服务升级成一个快照续传编排面板](snapshot-continuation-dashboard.md)
5. [把本地控制台服务升级成一个快照持久化面板](snapshot-persistence-dashboard.md)
6. [把本地控制台服务升级成一个快照调度配额面板](snapshot-scheduler-quota-dashboard.md)
7. [把本地控制台服务升级成一个快照多队列调度面板](snapshot-multi-queue-scheduler-dashboard.md)
8. [把本地控制台服务升级成一个快照分布式调度配额面板](snapshot-distributed-scheduler-quota-dashboard.md)
9. [把本地控制台服务升级成一个快照分布式配额仲裁面板](snapshot-distributed-quota-arbitration-dashboard.md)
10. [把本地控制台服务升级成一个快照跨集群调度面板](snapshot-cross-cluster-scheduler-dashboard.md)
11. [把本地控制台服务升级成一个快照租约协调面板](snapshot-lease-coordination-dashboard.md)
12. [把本地控制台服务升级成一个 lease journal 与 ownership migration 面板](lease-journal-ownership-migration-dashboard.md)
13. [把本地控制台服务升级成一个 lease journal compaction 与 ownership migration replay 面板](lease-journal-compaction-migration-replay-dashboard.md)


## 当前空缺

当前 `case/` 真正应该优先处理的，不再是继续把同一条控制面长链往后无限细分，而是：

- 给“高级控制面专题”补一页总览导读，解释这组页面的进入顺序和边界
- 优先做这组高级专题的英文同步，而不是继续追加新的末端页面
- 回到更均衡的案例覆盖，补一些不属于控制面长链的专题方向
  例如：纯工具型程序、纯文件/文本链路、非 HTTP 的长期服务骨架


## 与 API 和教学文档的关系

建议理解为：

- `api/`：模块合同和能力边界
- `guide/`：从零到一教学路线
- `case/`：多个模块如何协同解决一个完整问题

如果你已经知道每个模块的大致 API，但还不知道怎么把它们串起来，就应该优先看这里。
