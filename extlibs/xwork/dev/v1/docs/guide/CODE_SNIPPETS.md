# 教程代码片段维护说明

> 状态：中文初稿，待审阅。

本文件记录教程中的关键 C 片段如何维护，避免文档片段和 `xwork.h`、`examples/` 脱节。

## 片段类型

| 类型 | 标记方式 | 维护规则 |
| --- | --- | --- |
| 可编译完整程序 | 链接 `examples/*.c`，文档中给构建命令 | 必须随 `examples/README.md` 和 CI 示例目标验证。 |
| C 调用片段 | 使用 fenced code block `c` | 必须使用当前 `xwork.h` 中真实类型名、函数名和字段名。 |
| 调用顺序 | 使用 fenced code block `text` | 只表达生命周期顺序，不声明为可编译 C。 |
| JSON contract | 使用 fenced code block `json` | 字段必须来自 host tool contract 或当前实现。 |

## 当前 source of truth

- 最小可编译程序：[`examples/first_xwork_program.c`](../../examples/first_xwork_program.c)。
- 示例构建命令：[`examples/README.md`](../../examples/README.md)。
- 公共 C API：[`xwork.h`](../../xwork.h)。
- host tool JSON contract：[`docs/api/api-host-tools.md`](../api/api-host-tools.md)。
- 文档 review 规则：[`docs/DOCS_REVIEW_CHECKLIST.md`](../DOCS_REVIEW_CHECKLIST.md)。

## 更新规则

- 修改 `xwork.h` public API 后，检查所有 `c` 片段。
- 修改 example 后，检查对应 guide/case 页面和 `examples/README.md`。
- 新增无法编译的概念片段时，优先使用 `text`，不要标为 `c`。
- 本地运行 `tools/check_docs.ps1` 验证链接、版本常量、schema 名称和内置工具名称。
