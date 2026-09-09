# xllm 文档中心

> 面向 xllm 使用者的正式入口。先用 `guide/` 建立使用心智，再用 `api/` 查公开接口，最后用 `case/` 看完整集成案例。

[项目简介](../README.md)

---

## 快速入口

- [API 索引](api/README.md)
- [教程入口](guide/README.md)
- [范例解析](case/README.md)
- [示例说明](EXAMPLES.md)
- [架构说明](ARCHITECTURE.md)
- [最佳实践](BEST_PRACTICES.md)
- [常见问题](FAQ.md)
- [迁移说明](MIGRATION.md)
- [性能与发布说明](PERFORMANCE.md)

## 按目标阅读

### 第一次接触 xllm

1. [项目简介](../README.md)
2. [从零开始写第一个 xllm 程序](guide/first-xllm-program.md)
3. [Provider 与 Profile 入门](guide/provider-profile-intro.md)
4. [Request / Response 入门](guide/request-response-intro.md)
5. [最小聊天调用案例](case/minimal-chat.md)

### 准备做 AI IDE 或 agent 集成

1. [xllm 架构说明](ARCHITECTURE.md)
2. [Session 入门](guide/session-intro.md)
3. [Tool Loop 入门](guide/tool-loop-intro.md)
4. [Memory RAG 入门](guide/memory-rag-intro.md)
5. [AI IDE Agent Loop 案例](case/ai-ide-agent-loop.md)

### 想使用本地记忆和工作区索引

1. [Memory API](api/api-memory.md)
2. [Memory Ingest API](api/api-memory-ingest.md)
3. [Memory Search API](api/api-memory-search.md)
4. [工作区索引入门](guide/workspace-index-intro.md)
5. [Workspace RAG 案例](case/workspace-rag.md)

### 准备交付或消费发布包

1. [发布门禁入门](guide/release-gate-intro.md)
2. [Release API / 工具说明](api/api-release.md)
3. [发布包消费案例](case/release-bundle-consumption.md)

## 文档分区

- `api/`
  公开类型、函数、生命周期、资源归属、错误处理和模块边界。
- `guide/`
  按学习顺序解释什么时候该用什么能力，以及如何从最小程序走到完整集成。
- `case/`
  把 core、session、provider、memory、tool loop 和 release gate 串起来的完整案例。

## 使用建议

- 如果你还不熟悉 xllm，先顺着 `guide/` 读，不要直接从头文件开始。
- 如果你已经知道模块名，去 `api/` 查函数、结构体和调用顺序。
- 如果你要把 xllm 接入一个真实宿主，去 `case/` 看完整链路。
