# xllm-memory

`xllm-memory` 是 xllm 之上的长期记忆层：显式、可追溯的记录存储与确定性检索。它不做模型调用、不管理会话——分别是 `xllm` 与 `xllm-session` 的职责。

依赖：兄弟目录的 [`xllm`](../xllm)（核心调用层）与 XRT 单头（文件原子写、目录、JSON 读取、时间）。

## 当前能力

- 显式、可追溯的长期 fact / preference / task / summary / knowledge 记录
- namespace、source URI、actor、reason、trust、sensitivity、record/store revision 与内容指纹
- 每次写入、替换、无变化和删除的 mutation receipt
- 原子本地 JSON 持久化、重启校验和 namespace 隔离
- 不依赖模型的确定性词法检索与 UTF-8 精确短语检索
- 默认过滤过期和 sensitive/secret 记录，并将有来源的结果渲染为不可信参考上下文

详细边界和最小示例见 [docs/MEMORY.md](docs/MEMORY.md)。

## 构建

Windows 运行 `build.bat`，POSIX shell 运行 `./build.sh`（可经 `XLLM_DIR`/`XRT_DIR` 覆盖依赖位置）。

## 模块边界

```text
xcode CLI
    -> xwork             Agent 循环、工具执行、审批、循环保护
        -> xllm-session  上下文账本、预算、裁剪、持久化、压缩
        -> xllm-memory   本库：显式长期记录、来源、检索、审计回执
        -> xllm          一次模型调用、SSE、provider 适配
            -> xrt       核心 HTTP/1.1 wire、TLS、future、网络运行时
```
