# Pattern

Pattern 是面向大量结构化字节模式的编译式匹配器。它只负责完整字符串
匹配、顺序捕获和模式选择，不包含 HTTP、路由处理函数或正则表达式语义。

## 模式语法

- 字面字段按字节精确匹配。
- `{name}` 捕获一个非空字段，不能跨越配置中的分隔字节。
- `prefix{name}`、`{name}suffix` 与 `prefix{name}suffix` 捕获同一字段中
  前后缀之间的非空字节；前后缀按字节精确匹配。
- `{*name}` 捕获剩余文本，只能位于模式末尾，并允许空值。
- `{{` 和 `}}` 在字面字段中分别表示 `{` 和 `}`。
- 捕获名必须是 ASCII 标识符，同一模式内不能重名。
- 一个字段至多包含一个普通捕获；`{a}-{b}` 当前是编译错误，尾捕获不能
  与任何字面量混合。
- 匹配始终锚定完整输入，不执行路径规范化、URL 解码或大小写折叠。

默认分隔符为 `/`。`xpatternconfig.Separators` 是分隔字节集合；模式中的
分隔符仍要求精确匹配。设置为 `/.` 后，`/file/{name}.{ext}` 可以分别捕获
文件名和扩展名，但 `/` 与 `.` 不会互相替代。

## 一次性提取

```c
xstrview Captures[2];
size_t iCount;

if ( xrtPatternExtract(
	XRT_STR_LITERAL("/repo/{owner}/{name}"),
	Text,
	Captures,
	2u,
	&iCount
) == XPATTERN_MATCH ) {
	/* Captures[0] 和 Captures[1] 借用 Text。 */
}
```

`xrtPatternExtract` 在模式、参数和捕获容量有效时不分配内存，适合偶发
匹配；错误报告本身仍可能创建 `xerror`。同一模式被重复使用时，应通过
`xrtPatternCompile` 编译一次。

## 批量编译与匹配

`xrtPatternCompileMany` 将多条模式编译成不可变对象。`xrtPatternLookup`
只返回获胜模式；`xrtPatternMatch` 还按出现顺序写入捕获视图。编译对象没有
可变 matcher 缓存，可由多个线程同时查询。

模式按第一个不同字段决定特异度：字面字段优先于混合捕获，混合捕获优先于
整字段捕获，整字段捕获优先于尾捕获。多个混合捕获同时命中时，固定字节
总数更多者优先；总数相同时，前缀更长者优先。结构无法区分且优先级相同的模式会产生
`XPATTERN_ERROR_CONFLICT`；不同优先级可以显式替换这种同结构模式。优先级
不会令参数模式反超可同时命中的字面模式。

## 动态模式

`xpatternbuilder` 复制并缓存模式解析结果。`Add`、`Set`、`Remove` 只修改
Builder；`xrtPatternBuilderCompile` 生成新的不可变快照。旧快照不会观察到
后续修改，因而可以由上层使用 generation 或 RCU 风格生命周期安全发布。

Builder 不保证并发安全。未修改的 Builder 再次编译会返回缓存快照的新
引用；批量追加具有事务性。

## 所有权

- 编译对象通过 `xrtPatternRef`、`xrtPatternRelease` 管理。
- Builder 通过 `xrtPatternBuilderFree` 释放。
- 模式和捕获名由 Builder/编译对象复制。
- `xpatternspec.Value` 不转移所有权。
- 捕获 `xstrview` 只在输入文本仍然有效时有效。

完整函数列表见 [Pattern API reference](pattern-reference.md)。
