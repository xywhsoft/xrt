# XRT 范例解析

> 面向“完整链路、组合能力、真实工程片段”的案例入口。这里与 `docs/guide/` 的区别是：`guide/` 偏教学路径，`case/` 偏完整问题拆解与工程组合。

[返回文档中心](../README.md)

---

## 目录

- [范例解析的定位](#范例解析的定位)
- [当前状态](#当前状态)
- [目标案例梯度](#目标案例梯度)
- [现有页面如何使用](#现有页面如何使用)
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

当前 `case/` 目录已经有一批可读页面，但整体仍处于重建中：

- 现有页面更像“专题说明”而不是“完整教学案例”
- 适合快速理解模块如何组合
- `value + template + file` 这条页面输出主线已经补出正式案例页
- `file + path + value + json` 这条配置系统主线已经补出正式案例页
- `xhttpd + xvalue + json + future` 这条服务端主线已经补出正式案例页
- `xhttpd + xvalue + json + template` 这条服务端组合主线已经补出正式案例页
- `thread + queue + future` 这条并发主线已经补出正式案例页
- `subprocess + file_async + future` 这条工具链主线已经补出正式案例页
- `xurl + xhttp + proxy + TLS` 这条客户端主线已经补出正式案例页
- `xws + queue + coroutine` 这条双向会话主线已经补出正式案例页
- `xvalue + json + xhttp/xws + future/coroutine` 这条 AI 客户端主线已经补出正式案例页
- `charset + regex + crypto + file` 这条签名规则包导入主线已经补出正式案例页
- `mempool + avltree + list` 这条会话索引与回收池主线已经补出正式案例页
- `config + logging + thread + queue + xhttpd + template` 这条完整小项目主线已经补出正式案例页
- `config + logging + thread + queue + subprocess + future + xhttpd + template` 这条子进程探测面板主线已经补出正式案例页
- `config + logging + queue + task group + future + xhttpd + template` 这条批处理任务面板主线已经补出正式案例页
- `dict + list + hash + queue + thread + xhttpd + template` 这条缓存刷新面板主线已经补出正式案例页
- `dict + list + avltree + hash` 这条多租户索引注册表主线已经补出正式案例页
- `dict + list + queue + thread + time + hash + xhttpd + template` 这条重试退避面板主线已经补出正式案例页
- `dict + list + queue + thread + path + file + xhttpd + template` 这条归档面板主线已经补出正式案例页
- `dict + list + queue + thread + time + path + file + hash + xhttpd + template` 这条冷热分层面板主线已经补出正式案例页
- `dict + list + queue + thread + time + path + file + hash + xhttpd + template` 这条冷层回温面板主线已经补出正式案例页
- `dict + list + list + queue + thread + time + path + file + hash + xhttpd + template` 这条冷热滚动归档面板主线已经补出正式案例页
- `dict + list + list + queue + thread + time + path + file + hash + xhttpd + template` 这条自动回温策略面板主线已经补出正式案例页
- `dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` 这条冷层回温归档协同面板主线已经补出正式案例页
- `dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` 这条自动回温 + 滚动归档联动面板主线已经补出正式案例页
- `dict + dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` 这条多层存储面板主线已经补出正式案例页
- `dict + dict + dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` 这条冷热多级老化面板主线已经补出正式案例页
- `dict + dict + dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` 这条恢复优先级面板主线已经补出正式案例页
- `dict + dict + list + list + queue + thread + future + subprocess + file_async + path + file + hash + xhttpd + template` 这条跨介质编排面板主线已经补出正式案例页
- `dict + dict + list + list + list + queue + thread + task group + future + subprocess + file_async + path + file + hash + xhttpd + template` 这条重链路恢复面板主线已经补出正式案例页
- `dict + list + list + list + queue + thread + task group + future + subprocess + file_async + time + path + file + hash + xhttpd + template` 这条恢复配额面板主线已经补出正式案例页
- `dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file_async + time + path + file + hash + xhttpd + template` 这条跨机器恢复面板主线已经补出正式案例页
- `dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file_async + time + path + file + hash + xhttpd + template` 这条分布式编排面板主线已经补出正式案例页
- `dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + value + json + time + path + hash + xhttpd + template` 这条持久化协调面板主线已经补出正式案例页
- `dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + value + json + time + path + hash + xhttpd + template` 这条一致性仲裁面板主线已经补出正式案例页
- `dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + value + json + time + path + hash + xhttpd + template` 这条共识日志复制面板主线已经补出正式案例页
- `dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + value + json + time + path + hash + xhttpd + template` 这条状态机提交面板主线已经补出正式案例页
- `dict + list + list + queue + thread + task group + future + file + file_async + path + value + json + time + hash + xhttpd + template` 这条日志回放面板主线已经补出正式案例页
- `dict + list + list + queue + thread + task group + future + file + file_async + path + value + json + time + xhttpd + template` 这条快照压缩面板主线已经补出正式案例页
- `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 这条租约故障切换面板主线已经补出正式案例页
- `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 这条领导切换恢复面板主线已经补出正式案例页
- `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 这条 compaction 接管面板主线已经补出正式案例页
- `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 这条 ownership 仲裁面板主线已经补出正式案例页
- `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 这条分布式接管编排面板主线已经补出正式案例页
- `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 这条共识式 ownership 面板主线已经补出正式案例页
- `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 这条共识式接管恢复面板主线已经补出正式案例页
- `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 这条共识恢复编排面板主线已经补出正式案例页
- `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 这条共识快照编排面板主线已经补出正式案例页
- 还没有形成从简单到复杂的案例阶梯
- 还不能覆盖 XRT 全部公开模块

如果你想看后续正式教学顺序，建议同时阅读：

- [教学重建路线图](../guide/ROADMAP.md)


## 目标案例梯度

后续正式案例建议按下面顺序重建：

1. 配置系统：`file + path + value + json`
2. 模板渲染页面：`value + template + file`
3. 多任务 worker：`thread + queue + future`
4. 子进程与异步文件流水线：`subprocess + file_async + future`
5. HTTP 客户端调用链：`xurl + http util + xhttp + TLS + proxy`
6. HTTP 服务：`xhttpd + json + template + value`
7. WebSocket 会话服务：`xws + coroutine + queue`
8. 流式 LLM API：`xhttp/xws + future + coroutine + json`
9. 完整小项目：把配置、日志、任务、网络和模板串成一个可运行程序
10. 批处理任务面板：`queue + task group + future + xhttpd + template`
11. 缓存刷新面板：`dict + list + hash + queue + thread + xhttpd + template`
12. 多租户索引注册表：`dict + list + avltree + hash`
13. 重试退避面板：`dict + list + queue + thread + time + hash + xhttpd + template`
14. 归档面板：`dict + list + queue + thread + path + file + xhttpd + template`
15. 冷热分层面板：`dict + list + queue + thread + time + path + file + hash + xhttpd + template`
16. 冷层回温面板：`dict + list + queue + thread + time + path + file + hash + xhttpd + template`
17. 冷热滚动归档面板：`dict + list + list + queue + thread + time + path + file + hash + xhttpd + template`
18. 自动回温策略面板：`dict + list + list + queue + thread + time + path + file + hash + xhttpd + template`
19. 冷层回温归档协同面板：`dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template`
20. 自动回温 + 滚动归档联动面板：`dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template`
21. 多层存储面板：`dict + dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template`
22. 冷热多级老化面板：`dict + dict + dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template`
23. 恢复优先级面板：`dict + dict + dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template`
24. 跨介质编排面板：`dict + dict + list + list + queue + thread + future + subprocess + file_async + path + file + hash + xhttpd + template`
25. 重链路恢复面板：`dict + dict + list + list + list + queue + thread + task group + future + subprocess + file_async + path + file + hash + xhttpd + template`
26. 恢复配额面板：`dict + list + list + list + queue + thread + task group + future + subprocess + file_async + time + path + file + hash + xhttpd + template`
27. 跨机器恢复面板：`dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file_async + time + path + file + hash + xhttpd + template`
28. 分布式编排面板：`dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file_async + time + path + file + hash + xhttpd + template`
29. 持久化协调面板：`dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + value + json + time + path + hash + xhttpd + template`
30. 一致性仲裁面板：`dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + value + json + time + path + hash + xhttpd + template`
31. 共识日志复制面板：`dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + value + json + time + path + hash + xhttpd + template`
32. 状态机提交面板：`dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + value + json + time + path + hash + xhttpd + template`
33. 日志回放面板：`dict + list + list + queue + thread + task group + future + file + file_async + path + value + json + time + hash + xhttpd + template`
34. 快照压缩面板：`dict + list + list + queue + thread + task group + future + file + file_async + path + value + json + time + xhttpd + template`
35. 租约故障切换面板：`dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template`
36. 领导切换恢复面板：`dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template`
37. compaction 接管面板：`dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template`
38. ownership 仲裁面板：`dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template`
39. 分布式接管编排面板：`dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template`
40. 共识式 ownership 面板：`dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template`
41. 共识式接管恢复面板：`dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template`
42. 共识恢复编排面板：`dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template`
43. 共识快照编排面板：`dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template`

这条梯度的目标不是只展示“能跑”，而是展示：

- 为什么这样拆模块
- 哪一层适合线程，哪一层适合协程
- 哪一层应该统一进 future / wait-source
- 如何把基础模块一路升级成完整功能


## 现有页面如何使用

当前已有页面里，正式案例和过渡页面并存，可以按下面顺序开始阅读：

1. [用 xvalue + json 构造配置系统](json-config-system.md)
2. [用 Template 渲染一个 HTML 页面](template-render-html.md)
3. [用 Queue + Future 写一个多生产者 Worker](queue-worker-future.md)
4. [用 Subprocess + File Async 写一个工具链流水线](subprocess-file-async-pipeline.md)
5. [用 XHTTP 走完 URL、代理与 TLS 客户端调用链](xhttp-client-proxy-tls.md)
6. [用 XWS + Queue + Coroutine 写一个双向会话服务骨架](xws-session-queue-coroutine.md)
7. [线程、协程与 Future 协同模型](thread-coroutine-future.md)
8. [xnet-v2 Stream Wait-Source 实战](xnet-stream-wait-source.md)
9. [用 XRT 写一个最小 HTTP 服务](minimal-http-service.md)
10. [用 XRT 调用一个流式 LLM API](streaming-llm-api.md)
11. [一个完整的 HTTP + JSON + Template 服务链路](http-json-template-chain.md)
12. [用 Charset + Regex + Crypto 导入一个签名规则包](signed-rule-bundle.md)
13. [用 MemPool + AVLTree + List 组织一个会话索引与回收池](session-registry-pool-index.md)
14. [把配置、日志、任务、网络和模板串成一个本地控制台服务](local-console-service.md)
15. [把本地控制台服务升级成一个子进程探测面板](subprocess-probe-dashboard.md)
16. [把本地控制台服务升级成一个批处理任务面板](batch-task-dashboard.md)
17. [把本地控制台服务升级成一个缓存刷新面板](cache-refresh-dashboard.md)
18. [用 Dict + List + AVLTree 组织一个多租户索引注册表](tenant-index-registry.md)
19. [把本地控制台服务升级成一个重试退避面板](retry-backoff-dashboard.md)
20. [把本地控制台服务升级成一个归档面板](archive-dashboard.md)
21. [把本地控制台服务升级成一个冷热分层面板](hot-cold-tier-dashboard.md)
22. [把本地控制台服务升级成一个冷层回温面板](warm-back-dashboard.md)
23. [把本地控制台服务升级成一个冷热滚动归档面板](rolling-tier-archive-dashboard.md)
24. [把本地控制台服务升级成一个自动回温策略面板](auto-warm-policy-dashboard.md)
25. [把本地控制台服务升级成一个冷层回温归档协同面板](warm-archive-coordination-dashboard.md)
26. [把本地控制台服务升级成一个自动回温 + 滚动归档联动面板](auto-warm-rolling-archive-dashboard.md)
27. [把本地控制台服务升级成一个多层存储面板](multi-tier-storage-dashboard.md)
28. [把本地控制台服务升级成一个冷热多级老化面板](hot-cold-multi-level-aging-dashboard.md)
29. [把本地控制台服务升级成一个恢复优先级面板](recovery-priority-dashboard.md)
30. [把本地控制台服务升级成一个跨介质编排面板](cross-media-orchestration-dashboard.md)
31. [把本地控制台服务升级成一个重链路恢复面板](heavy-recovery-chain-dashboard.md)
32. [把本地控制台服务升级成一个恢复配额面板](recovery-quota-dashboard.md)
33. [把本地控制台服务升级成一个跨机器恢复面板](cross-machine-recovery-dashboard.md)
34. [把本地控制台服务升级成一个分布式编排面板](distributed-orchestration-dashboard.md)
35. [把本地控制台服务升级成一个持久化协调面板](persistent-coordination-dashboard.md)
36. [把本地控制台服务升级成一个一致性仲裁面板](consensus-arbitration-dashboard.md)
37. [把本地控制台服务升级成一个共识日志复制面板](consensus-log-replication-dashboard.md)
38. [把本地控制台服务升级成一个状态机提交面板](state-machine-commit-dashboard.md)
39. [把本地控制台服务升级成一个日志回放面板](log-replay-dashboard.md)
40. [把本地控制台服务升级成一个快照压缩面板](snapshot-compaction-dashboard.md)
41. [把本地控制台服务升级成一个租约故障切换面板](lease-failover-dashboard.md)
42. [把本地控制台服务升级成一个领导切换恢复面板](leader-handoff-recovery-dashboard.md)
43. [把本地控制台服务升级成一个 compaction 接管面板](compaction-takeover-dashboard.md)
44. [把本地控制台服务升级成一个 ownership 仲裁面板](ownership-arbitration-dashboard.md)
45. [把本地控制台服务升级成一个分布式接管编排面板](distributed-takeover-orchestration-dashboard.md)
46. [把本地控制台服务升级成一个共识式 ownership 面板](consensus-style-ownership-dashboard.md)
47. [把本地控制台服务升级成一个共识式接管恢复面板](consensus-takeover-recovery-dashboard.md)
48. [把本地控制台服务升级成一个共识恢复编排面板](consensus-recovery-orchestration-dashboard.md)
49. [把本地控制台服务升级成一个共识快照编排面板](consensus-snapshot-orchestration-dashboard.md)

这些页面当前适合：

- 快速确认一个组合方向是否已经进入主线
- 先拿到模块串联方式
- 在正式案例重写前作为结构草稿使用


## 当前空缺

当前仍缺少正式案例的方向包括：

- 继续往下补快照接管这类更重的业务变体案例


## 与 API 和教学文档的关系

建议理解为：

- `api/`：模块合同和能力边界
- `guide/`：从零到一教学路线
- `case/`：多个模块如何协同解决一个完整问题

如果你已经知道每个模块的大致 API，但还不知道怎么把它们串起来，就应该优先看这里。
