# xllm API 索引

> 当前正式 API 参考入口。这里按“你会怎样使用 xllm”组织，而不是按源码文件顺序堆放。

[返回文档中心](../README.md)

---

## 1. 基础类型与运行时

| 模块 | 文档 | 说明 |
| --- | --- | --- |
| Types | [types.md](types.md) | 基础枚举、错误码、日志级别、trace 类型和公共对象约定 |
| Core Runtime | [api-core.md](api-core.md) | runtime、profile、adapter 注册、同步/异步聊天入口 |
| Request / Response | [api-request-response.md](api-request-response.md) | 请求、输出项、JSON 输出、tool call、artifact 和资源释放 |
| Diagnostics | [api-diagnostics.md](api-diagnostics.md) | log callback、trace callback、错误对象和诊断事件 |

## 2. Provider 与模型能力

| 模块 | 文档 | 说明 |
| --- | --- | --- |
| Providers | [api-providers.md](api-providers.md) | 内置 provider adapter、capability、profile 配置和限制判断 |
| Release | [api-release.md](api-release.md) | 版本、发布包、校验脚本和消费侧验证入口 |

## 3. Session 与工具调用

| 模块 | 文档 | 说明 |
| --- | --- | --- |
| Session | [api-session.md](api-session.md) | 短期对话历史、compact、state export/import 和 session chat |
| Tools | [api-tools.md](api-tools.md) | tool 定义、tool choice、同步/异步 tool executor 和 tool result |

## 4. Memory 与 RAG

| 模块 | 文档 | 说明 |
| --- | --- | --- |
| Memory | [api-memory.md](api-memory.md) | memory store 生命周期、scope、profile、record/chunk 基础概念 |
| Memory Ingest | [api-memory-ingest.md](api-memory-ingest.md) | 文本、文件、目录、工作区、会话摘要、task/fact/preference 入库 |
| Memory Search | [api-memory-search.md](api-memory-search.md) | search、list、debug、context apply、删除和生命周期维护 |
| Memory Workspace | [api-memory-workspace.md](api-memory-workspace.md) | workspace sync、status、health check、change set |
| Memory Watcher | [api-memory-watcher.md](api-memory-watcher.md) | file event queue、watcher bridge、pump、worker |
| Memory Bridge | [api-memory-bridge.md](api-memory-bridge.md) | opt-in session + memory 组合调用，不替代宿主策略 |

## 推荐阅读顺序

1. [types.md](types.md)
2. [api-core.md](api-core.md)
3. [api-request-response.md](api-request-response.md)
4. [api-providers.md](api-providers.md)
5. [api-session.md](api-session.md)
6. [api-tools.md](api-tools.md)
7. [api-memory.md](api-memory.md)
8. [api-memory-ingest.md](api-memory-ingest.md)
9. [api-memory-search.md](api-memory-search.md)
10. [api-memory-bridge.md](api-memory-bridge.md)

## 与教程 / 案例的边界

本目录只负责 API 合同、调用顺序和资源归属。想先理解“什么时候该用”，请读 [教程入口](../guide/README.md)。想看完整组合，请读 [范例解析](../case/README.md)。

## API 文档编写标准

每个 API 模块页必须参考 `D:\git\xrt\docs\api\api-time.md` 的粒度编写。不能只做概览，也不能只列函数名。

新页面请从 [API_PAGE_TEMPLATE.md](API_PAGE_TEMPLATE.md) 开始填充。

每个模块页至少包含：

- 常量、宏、枚举和结构体说明。
- 按功能分组的 API 目录。
- 每个公开函数的独立小节。
- 函数原型、参数、返回值、资源归属、补充说明和范例代码。
- 常见错误、相关 API、相关教程和相关案例。

每个函数小节至少包含：

- **功能**：这个函数解决什么问题，什么时候使用。
- **函数原型**：从公开头文件复制的准确 C 原型。
- **参数**：逐个解释输入/输出方向、是否可为 `NULL`、生命周期、所有权、单位、范围和默认值。
- **返回值**：成功/失败语义、错误对象行为、是否可能产生部分结果。
- **资源归属**：谁分配、谁释放、用哪个 `reset/free/destroy` 函数清理。
- **补充说明**：调用顺序、线程安全、provider 或 memory scheme 差异、兼容性注意事项。
- **范例代码**：尽量给出可直接学习的小代码段；复杂流程可链接到 `case/`。

只有当一个 API 页覆盖了分配给该模块的所有公开 `XLLM_API` 函数，并且每个函数都有上述内容时，才可以在 `XLLM_DOCUMENTATION_SPEC.md` 中标记完成。
