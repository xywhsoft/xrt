# 文档 Review Checklist

> 状态：中文初稿，待审阅。

本清单用于 review xwork 正式文档变更。新增 public API、示例、持久化格式、remote protocol、profile 默认策略或内置工具时，必须同时检查相关文档。

## 链接与结构

- 根 README 能进入 `docs/README.md`、API、教程和范例索引。
- `docs/` 中的相对链接没有断链。
- `docs/` 可以链接 `dev/docs/` 作为内部参考，但正文必须能独立说明用户需要理解的事实。
- 新增文档已加入对应索引：`docs/README.md`、`docs/api/README.md`、`docs/guide/README.md` 或 `docs/case/README.md`。
- 中文主稿稳定前不生成英文 `.en.md`；生成英文后需保持同名、同层级和同结构。

## API 一致性

- API 页面中的类型名、函数名、枚举名、错误码和字段名来自当前 `xwork.h`。
- 每个 API 页面说明 ownership、生命周期、reset/destroy 规则和线程边界。
- 每个 API 页面说明典型调用顺序和恢复边界。
- persistence 文档中的版本号与 `XWORK_PERSISTENCE_FORMAT_VERSION` 一致。
- remote worker 文档中的协议版本与 `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT` 一致。
- schema 名称和内置工具名称与 `xwork.h` 中的常量一致。

## 代码与示例

- 完整可运行示例放在 `examples/`，文档只保留短片段或调用顺序。
- 可复制执行的构建命令必须在本地或 CI 中验证。
- 伪代码片段必须明确标注为 `text` 或说明其是调用顺序，不伪装成可编译 C。
- 每个新增 example 必须补充 `examples/README.md` 和对应 case 或 guide 入口。
- 示例输出、artifact、store 目录和清理方式与源码行为一致。

## 语言与术语

- 中文文档优先使用统一术语：运行时、工作区、工具、编排器、审批、产物、持久化、远程 worker、回放。
- 保留 public C 标识符、JSON 字段名、schema 名称和工具名英文原文。
- 不把内部开发计划作为用户理解的前置条件。
- 不把 xwork 描述成完整产品、云服务或内置网络服务。

## 自动检查

提交前运行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\check_docs.ps1
```

如果修改了示例，还应按 `examples/README.md` 中的命令编译并运行相关程序。
