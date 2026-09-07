# BOOK_SPEC v1.0 — 程序设计教程源格式与质量门禁

教程网页（wwwroot/book/ch*.html）由 `tools/gen_web_book.py` 从本目录的 markdown 源生成。
**手工编辑 wwwroot 下的章节页面是禁止的**——那次"头重脚轻"的漂移正是这样产生的。
本章规范同时是 `tools/check_book.py` 的判定依据：门禁红线全部可机器判定，FAIL 的章节不算完成。

---

## 1. 文件与命名

- 每章一个源文件：`docs/book/NN-slug.md`，渲染目标 `wwwroot/book/chNN-slug.html`。
  NN 必须与 `docs/book/order.json` 中的章号一致（order.json 由 `init-order` 从现有页面生成，迁移期间保持全书排序的唯一事实来源）。
- 章号与 slug 迁移期间沿用现有页面，避免外部链接失效。

## 2. Front matter

文件头部以两行 `---` 包围的键值块：

```
---
num: 3
slug: error
title: 错误模型：xerror 全解
volume: 卷一 起步与核心
type: concept
lead: 一句话导语，出现在页头。
api: error, error_format, core
---
```

| 字段 | 必填 | 说明 |
| --- | --- | --- |
| num / slug / title / volume / lead / type | 是 | 页头与导航全部由此生成 |
| type | 是 | `concept` 概念章 / `practice` 实操章 / `composition` 组合章 / `project` 实战章 / `intro` 卷导言 |
| api | 是 | 逗号分隔的 ref 页名；页尾"API 参考手册"链接区与真实计数由生成器从 search-index.json 计算，**手写计数必然漂移，禁止** |

## 3. 章节骨架（h2 固定八节，顺序固定）

```markdown
## 导读     本章在本卷中的位置，读者将获得什么
## 引入     从一个真实场景提出问题（"为什么需要它"），不从 API 开始
## 概念     核心模型与图示；h3 小节自由
## 示例     完整可运行程序 2 个起步；每个 h3 小节一个程序，渐进递进
## 契约     错误、所有权、线程规则；与 docs/api 契约卡同源
## 避坑     每个坑一个 h3；含 症状 / 原因 / 错误-正确代码对照
## 练习     h3 必须为 基础 / 进阶 / 挑战 三级
## 速查     一张 markdown 表，渲染为本章小结
```

`intro` 型只需 `## 导读`（卷导言）；`project` 型骨架在阶段 3 定义。
渲染约定：导读、避坑、练习、速查不带节号；引入/概念/示例/契约自动编号 N.1–N.4。

## 4. 代码与程序

四种代码围栏：

    ```c                  手写片段（短小、聚焦）
    ```c bad / ```c good  避坑章的错误/正确对照，成对出现
    ```embed path="examples/..." [lines="a-b"] [title="标题"]
    ```term              终端块：`$ ` 开头的行为命令，其余为真实输出

**完整程序的定义**（计入门禁）：`embed` 块；或含 `int main(` 的 ```c 块。
每个完整程序所在 h3 小节内**必须**跟一个 ```term 块（首行 `$ 编译命令`，后接预期输出）。
优先嵌入 `examples/` 仓库源码——教程不维护第二份代码；嵌入路径必须存在，且该示例必须能通过仓库编译。

## 5. 图示

声明式围栏，两种：

    ```diagram state
    CONNECTING -> OPEN: 连接建立
    OPEN -> CLOSING: Close 请求
    ```

    ```diagram flow
    - 名称：一句说明
    ```

状态机渲染为节点-迁移列表，流程渲染为编号步骤。禁止用 ASCII 字符画图。

## 6. 行内标记

`` `code` ``、`**加粗**`、`[文字](chNN-slug.md)`（跨章引用，仅允许指向 order.json 中的章节）。
外部链接直接写 URL。图表：标准 markdown 管道表。

## 7. 事实性红线（check_book.py 逐条判定）

1. **API 幻觉 0 容忍**：正文中出现的 `xrt[A-Z]\w*` 函数、`X[A-Z0-9_]{2,}` 常量、`x[a-z]\w{2,}` 类型，
   必须存在于 include/xrt/*.h 符号表（由 gen_web_api.scan_all_headers 提供）。
2. **嵌入路径必须存在**；`lines` 区间必须落在文件行数内。
3. **跨章引用目标必须存在**于 order.json。
4. **api 列表的 ref 页必须存在**。
5. term 输出必须与示例头部注释的"预期输出"一致（check 抽取比对）。

## 8. 深度门禁（按 type）

| 指标 | practice | composition | concept | project | intro |
| --- | --- | --- | --- | --- | --- |
| 八节骨架 | 全 | 全 | 全 | 阶段3定 | 仅导读 |
| 讲解字数（去代码去标记） | ≥4000 | ≥6000 | ≥6000 | ≥8000 | ≥2000 |
| 完整程序 + term | ≥2 | ≥2 | ≥2 | ≥2 | — |
| 图示 | ≥1 | ≥1 | ≥1 | ≥1 | — |
| 避坑（含 bad/good 对照） | ≥2 | ≥2 | ≥2 | ≥2 | — |
| 练习三级 | 必须 | 必须 | 必须 | 必须 | — |

## 9. 防漂移生产纪律

- **素材包驱动**：每章动笔前运行 `python tools/gen_web_book.py material <slug>`，
  把输出的素材包（相关 examples 源码、docs/api 契约、符号清单）放入生成上下文。
  禁止脱离素材包凭记忆写章节。
- **小批量**：每批 3–5 章，批后 `check_book.py check` 全绿才进下一批。
- **金标准锚定**：`04-error.md`（概念章）与 `65-net-tcp.md`（实操章）是金标准；
  后续每批的写作规范以本 SPEC + 金标准度量为准，不参考未过门禁的章节。
- **漂移检测**：`check_book.py report` 输出全书度量表；连续 3 章讲解字数低于其类型目标的 80% 即标红，
  标红后停止推进，回到金标准重新校准。
- **CI**：check + 被嵌入示例编译 + wwwroot 链接校验进入 CI，任何回退变红。

## 10. 变更流程

改规范 → 改 SPEC → 改 check → 跑全量门禁。规范之外不存在"口头标准"。
