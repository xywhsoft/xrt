# XRT 范例解析

> 面向“多个模块如何串成一个完整程序”的案例入口。这里与 `guide/` 的区别是：`guide/` 解决学习顺序和心智模型，`case/` 直接展示完整问题的组合方式。

[返回文档中心](../README.md)

---

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


## 代表性扩展示例

在读完 `local-console-service.md` 之后，建议只按当前问题挑一篇继续看，不需要再顺着几十个变体逐页往后读：

1. [把本地控制台服务升级成一个子进程探测面板](subprocess-probe-dashboard.md)
	需要把外部 CLI、worker、future、页面和快照串成同一条后台链路时看这篇。
2. [把本地控制台服务升级成一个冷热分层面板](hot-cold-tier-dashboard.md)
	需要把热数据、冷数据、页面和文件快照拆成稳定分层时看这篇。
3. [把本地控制台服务升级成一个跨介质编排面板](cross-media-orchestration-dashboard.md)
	需要在本地文件、包文件和外部工具链之间做正式恢复编排时看这篇。


## 阅读建议

- 先顺着主线读到 `local-console-service.md`，建立完整项目骨架。
- 再按当前业务只补一篇扩展示例，避免把大量同类变体当成必读主线。
- 想查函数和结构细节时回 [API 索引](../api/README.md)。
- 想补概念解释和选型理由时回 [教学入口](../guide/README.md)。
