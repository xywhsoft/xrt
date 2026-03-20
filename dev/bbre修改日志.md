# bbre 修改日志

## 2026-03-20

### 背景

在准备将 `bbre` 长期并入 `xrt` 并独立维护前，对 `bbre` 的实现细节做了一轮复核。  
本次只修复已经确认会影响语义或 allocator 契约的真实问题，不处理纯风格类问题。

### 修复项

1. 修复 `bbre_clone()` 未复制 `group_names` 的问题

- 问题说明：
	`bbre_clone()` 之前只复制了编译后的 `prog`，没有复制 `group_names`。  
	这会导致带捕获组元数据的正则在 clone 后，`bbre_capture_count()` / `bbre_capture_name()` 返回结果不完整或错误。
- 修复方式：
	新增内部辅助逻辑，clone 时同步复制 `group_names`，并保持命名组字符串内容一致。
- 影响范围：
	`dev/regex/bbre.c`
	`lib/regex.h`

2. 修复命名捕获组字符串释放时传给 allocator 的 `old_size` 少 1 的问题

- 问题说明：
	命名捕获组字符串分配时使用 `name_size + 1`（包含结尾 `\\0`），
	但在回滚路径和析构路径中释放时只传了 `name_size`。  
	这违反了 `bbre_alloc` 对 `old_size` 的公开契约，会影响严格校验型 allocator、统计型 allocator 或自定义内存池。
- 修复方式：
	将相关释放路径统一改为传递 `name_size + 1`。
- 影响范围：
	`dev/regex/bbre.c`
	`lib/regex.h`

3. 将 regex 公开类型与 API 名称并入 XRT 命名体系

- 变更说明：
	`xrt.h` 不再公开 `bbre_*` 类型和函数名，改为公开 `xregex`、`xregexbuilder`、`xregexset`、`xregexsetbuilder`、`xregexspan`、`xregexalloc` 以及 `xrtRegex*` / `xrtRegexSet*` API。  
	底层实现仍然使用内部 `bbre` 引擎，公开层只负责命名、类型和参数适配。
- 实现方式：
	在 `xrt.c` 中新增包装层，将公开 API 映射到内部 `bbre_*` 实现；其中 `find` / `captures` / `set matches` 对输出参数做了额外适配，避免直接暴露底层断言约束。
- 影响范围：
	`xrt.h`
	`xrt.c`
	`docs/api/api-regex.md`

### 本次未处理

- `bbre_set_builder_init()` / `bbre_set_builder_destroy()` 中使用 `sizeof(bbre_builder)` 的写法。  
	复核后确认这是“多分配且前后一致”的实现问题，属于整洁性不足，不属于当前必须修复的真实风险项。

### 备注

- 本次修改未进行测试。  
	原因：当前评审阶段按要求着重关注实现与代码质量，暂不走测试流程。
