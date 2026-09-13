# xllm-session

`xllm-session` 是 xllm 之上的上下文治理层：轮次账本、输入预算、软裁剪、压缩事务与持久化。它不执行模型调用——那是 `xllm` 核心的职责。

依赖：兄弟目录的 [`xllm`](../xllm)（核心调用层）与 XRT 单头（文件原子写、目录、JSON 读取、时间）。

## 当前能力

- GLM-5.1 的 200K context / 128K 单次输出上限默认配置，调用方可显式覆盖
- 独立的输出上限与输出预留；短上下文可使用完整输出上限，长上下文动态收缩本轮 `max_tokens`
- `context - output_reserve - safety_reserve` 输入预算
- 75% 软裁剪、95% 压缩的默认压力策略
- 保留工具调用/工具结果关联的旧工具输出裁剪
- 只在完整 Agent 轮次边界推进的压缩事务
- 未完成工具调用隔离，禁止压缩不完整轮次
- 原子 JSON 快照、进程重启加载及摘要连续性
- 先写后确认的增量 NDJSON 日志、快照检查点和快照后重放恢复
- 崩溃残尾自动截断、日志序号去重，以及完整损坏记录的显式拒绝
- 会话深分叉，保留摘要、序号与压缩检查点，并隔离父子分支的后续状态

详细边界和最小示例见 [docs/CONTEXT-GOVERNANCE.md](docs/CONTEXT-GOVERNANCE.md)。

## 构建

Windows 运行 `build.bat`，POSIX shell 运行 `./build.sh`（可经 `XLLM_DIR`/`XRT_DIR` 覆盖依赖位置）。

## 模块边界

```text
xcode CLI
    -> xwork             Agent 循环、工具执行、审批、循环保护
        -> xllm-session  本库：上下文账本、预算、裁剪、持久化、压缩
        -> xllm-memory   长期记忆
        -> xllm          一次模型调用、SSE、provider 适配
            -> xrt       核心 HTTP/1.1 wire、TLS、future、网络运行时
```
