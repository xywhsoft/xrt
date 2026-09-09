# Replay Agent Run 范例

> 对应源码：`examples/replay_agent_run.c`

这个范例展示 deterministic replay cassette 的基础能力。

## 解决的问题

Agent 运行会混合模型输出、工具副作用和文件系统状态。replay 让这些关键边界可以被记录、重放和审计，用于回归测试、问题复现和安全分析。

## 流程

```text
record model/tool/checkpoint entries
record filesystem snapshot/ref
load cassette into strict replay engine
seek checkpoint
replay following tool entry
run audit replay with changed request
emit divergence report artifact
```

## 关键点

- strict replay 模式下，未记录的副作用应被阻止。
- audit replay 会比较新结果和记录结果，输出 divergence。
- filesystem snapshot/ref 让文件状态成为 replay 输入的一部分。
- divergence report 使用结构化 report artifact 保存。

## Strict 与 Audit

| 模式 | 行为 |
| --- | --- |
| record | 记录 entry/event、hash 和 raw payload。 |
| strict | 遇到第一处不匹配即失败，适合 CI 回归。 |
| audit | 继续执行并累计 divergence，适合安全审计和差异分析。 |

## Hash 和 divergence

- JSON payload 使用 `xwork_replay_hash_json()`，会排序对象 key 并忽略无意义空白。
- 非 JSON 文本使用 `xwork_replay_hash_text()`。
- divergence 会记录 expected/actual kind、key、hash 和 message。
- `xwork_replay_engine_emit_report_artifact()` 可将 divergence result 写成 report artifact。

## 关键 API

| API | 作用 |
| --- | --- |
| `xwork_replay_engine_create()` | 创建 record/strict/audit replay engine。 |
| `xwork_replay_engine_record_entry()` | 记录 cassette entry。 |
| `xwork_replay_engine_record_filesystem_ref()` | 记录 typed filesystem ref。 |
| `xwork_replay_engine_seek_checkpoint()` | 定位到 checkpoint 后的 entry。 |
| `xwork_replay_engine_replay_entry()` | 对比 expected entry。 |
| `xwork_replay_engine_get_first_divergence()` | 查询第一处差异。 |
| `xwork_replay_engine_emit_report_artifact()` | 发出 divergence report artifact。 |

## 适合扩展

- 把 CI 中的 Agent 回归用例保存为 replay cassette。
- 对高风险工具调用启用 side-effect blocking。
- 将 divergence report 展示为测试失败或安全审计报告。
