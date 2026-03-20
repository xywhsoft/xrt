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
	在 `xrt.c` 中新增薄包装层，将公开 API 直接映射到内部 `bbre_*` 实现，并保留最小必要的 allocator 结构适配。
- 影响范围：
	`xrt.h`
	`xrt.c`
	`docs/api/api-regex.md`
	`docs/api/api-regex.en.md`
	`singlehead/xrt.h`

4. 将 regex 测试补充到 XRT 自身测试体系

- 变更说明：
	在 `test/` 目录新增 `test/test_regex.h`，按 `xrt` 当前公开 API 增加一组独立测试，并把 regex 注册进统一测试入口与 `container_smoke` 预设。  
	测试覆盖单模式匹配、查找、捕获、builder、自定义 allocator、clone、set、set builder，以及一个内部回归项，用于锁定此前修复过的 clone 捕获组元数据问题。
- 实现方式：
	不引入 `bbre` 上游整套测试框架，而是按 `xrt` 现有测试风格编写轻量测试头文件，直接接入 `test.c` 的测试注册表。
- 影响范围：
	`test/test_regex.h`
	`test.c`

5. 修复融合后 `regex.h` 实现侧缺失的 bbre ABI 前导定义，并收回过度包装

- 变更说明：
	此前 `bbre.h` 的公开声明已并入 `xrt.h` 并改成 xrt 风格命名，但 `bbre.c` 改造得到的 `lib/regex.h` 仍然依赖原始 `bbre_alloc`、`bbre_flags`、`bbre_span` 及一组公开函数原型。  
	缺失这段前导定义后，源码版实现会在编译阶段失去自洽性。
- 实现方式：
	在 `lib/regex.h` 中补回一份最小的 bbre ABI 前导定义，仅供实现文件内部使用；`xrt.h` 继续作为外部公开 API。  
	同时将 `xrt.c` 里的 regex 包装层收回为尽量接近 bbre 原始语义的薄适配，不再为 `find` / `captures` / `set matches` 额外分配临时缓冲区。  
	`singlehead/xrt.h` 同步这一调整。
- 影响范围：
	`lib/regex.h`
	`xrt.c`
	`singlehead/xrt.h`
	`test/test_regex.h`

### 本次未处理

- `bbre_set_builder_init()` / `bbre_set_builder_destroy()` 中使用 `sizeof(bbre_builder)` 的写法。  
	复核后确认这是“多分配且前后一致”的实现问题，属于整洁性不足，不属于当前必须修复的真实风险项。

### 备注

- 本次修改未执行测试用例。  
	仅做了最小化编译验证，用于确认 regex 源码版集成和新增测试头文件可以通过语法与类型检查。
