# xllm 范例解析

> 面向“多个模块如何串成一个完整程序”的案例入口。这里与 `guide/` 的区别是：`guide/` 先建立使用心智，`case/` 直接展示完整问题的解决方案。

[返回文档中心](../README.md)

---

## 主线必读

第一次系统阅读案例时，建议先按下面顺序走：

1. [最小聊天调用](minimal-chat.md)
2. [带短期历史的 Session Chat](session-chat.md)
3. [按模型能力选择 Provider 路径](provider-capability-selection.md)
4. [用 Tool Loop 写一个最小 Agent](tool-loop-agent.md)
5. [用 Memory 做工作区 RAG](workspace-rag.md)
6. [显式写入 Conversation Memory](conversation-memory.md)
7. [AI IDE / xwork Agent Loop 集成](ai-ide-agent-loop.md)
8. [发布包消费和验证](release-bundle-consumption.md)

## 阅读建议

- 想先跑通 provider 调用，从 `minimal-chat.md` 开始。
- 想做 IDE / agent，至少读到 `ai-ide-agent-loop.md`。
- 想做长期记忆，先读 `workspace-rag.md`，再读 `conversation-memory.md`。
- 想查函数细节时回 [API 索引](../api/README.md)。
