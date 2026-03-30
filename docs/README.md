# XRT 文档中心

> XRT 当前正式文档入口。API 参考已统一收纳到 `docs/api/`，为后续零基础教学文档、范例解析文档和专题说明留出独立结构。

[English](README.en.md) | [项目简介](../../README.md)

---

## 文档结构

当前 `docs/` 目录分为 3 个层次：

### 1. 文档中心

- 当前页 [README.md](README.md)

负责：

- 说明文档结构
- 提供统一入口
- 指向当前正式主线

### 2. API 参考

- [API 索引](api/README.md)

负责：

- 类型
- 基础运行时
- 线程与协程
- future / task / promise / wait-source
- 内存与容器
- 数据与文本
- 当前网络主线 API

### 3. 辅助文档

负责：

- 架构
- 裁剪宏
- 迁移
- FAQ
- 最佳实践
- 示例
- 性能说明
- 零基础教学
- 范例解析

为了避免 API 文档、教学文档、案例文档彼此混杂，当前结构明确约定为：

- `docs/api/`：API 合同、模块能力、当前主线说明
- `docs/guide/`：零基础教学、循序渐进教程、概念解释
- `docs/case/`：完整案例、组合用法、实战拆解

后续新增文档时，应优先判断它属于“合同说明”还是“教学/案例”，再决定落在哪个子目录。


## 当前主线入口

### 项目定位

- 根目录 [README.md](../../README.md)

### API 参考

- [API 索引](api/README.md)

当前最值得优先阅读的 API 主线：

- [Base 基础运行时](api/api-base.md)
- [Value 动态类型系统](api/api-value.md)
- [JSON 处理库](api/api-json.md)
- [XSON 处理库](api/api-xson.md)
- [Thread 线程管理库](api/api-thread.md)
- [Coroutine 协程运行时](api/api-coroutine.md)
- [Future / Task / Promise](api/api-future-task-promise.md)
- [XNet v2 网络主线](api/api-xnet-v2.md)
- [Network TLS](api/api-network-tls.md)
- [XHTTP](api/api-xhttp.md)
- [XHTTPD](api/api-xhttpd.md)
- [XWS](api/api-xws.md)

### 架构与基线

- [架构设计](ARCHITECTURE.md)
- [裁剪宏定义说明](裁剪宏定义说明.md)

### 辅助文档

- [迁移指南](MIGRATION.md)
- [最佳实践](BEST_PRACTICES.md)
- [常见问题](FAQ.md)
- [示例项目](EXAMPLES.md)
- [性能说明](PERFORMANCE.md)
- [教学文档](guide/README.md)
- [范例解析](case/README.md)


## 当前文档重建规则

当前文档正在按以下规则重建：

- 先完整生成中文版本
- 中文审阅通过后，再统一同步英文版本
- API 文档统一放到 `docs/api/`
- 已淘汰的旧网络/TLS 文档迁移到 `dev/` 归档
- 已存在且仍有效的文档，也必须逐篇和当前代码重新对齐
- 缺失但主线已存在的文档，必须补齐后才能算当前阶段完成


## 历史文档说明

以下文档不再属于当前正式入口：

- 旧网络模块族
- 旧 `nettls`

它们已经从 `docs/` 迁出，后续统一在 `dev/` 中以归档文档形式保留。

---

## 当前建议阅读顺序

如果你是第一次进入 XRT 当前主线，建议按这个顺序阅读：

1. [项目简介](../../README.md)
2. [架构设计](ARCHITECTURE.md)
3. [API 索引](api/README.md)
4. [示例说明](EXAMPLES.md)
5. [迁移指南](MIGRATION.md)

如果你准备直接写代码，建议优先阅读：

1. [Base](api/api-base.md)
2. [Value](api/api-value.md)
3. [JSON](api/api-json.md)
4. [XSON](api/api-xson.md)
5. [Thread](api/api-thread.md)
6. [Coroutine](api/api-coroutine.md)
7. [Future / Task / Promise](api/api-future-task-promise.md)
8. [XNet v2](api/api-xnet-v2.md)

如果你是第一次接触 XRT，建议再接着读：

1. [runtime 与线程附加](guide/runtime-thread-attach.md)
2. [xvalue 与 JSON 入门](guide/xvalue-json-intro.md)
3. [协程、future、task 入门](guide/coroutine-future-task-intro.md)
4. [xnet-v2 与 TLS 主线](guide/xnet-v2-tls-intro.md)
5. [流式 LLM API 教学](guide/streaming-llm-api-intro.md)
6. [XRT 子进程与工具执行入门](guide/subprocess-intro.md)

如果你想直接看完整组合案例，建议优先阅读：

1. [最小 HTTP 服务](case/minimal-http-service.md)
2. [线程、协程与 future 分工](case/thread-coroutine-future.md)
3. [流式 LLM API](case/streaming-llm-api.md)
4. [HTTP + JSON + template 完整链路](case/http-json-template-chain.md)
5. [xnet-v2 Stream Wait-Source 实战](case/xnet-stream-wait-source.md)
