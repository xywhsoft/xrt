---
num: 30
slug: regex
title: 正则引擎 regex
volume: 卷四 文本与结构化数据
type: practice
lead: 非回溯引擎的最坏线性时间、不可变编译对象与独占匹配器、命名捕获与替换。
api: regex
---

## 导读

`xregex` 是 XRT 的正则引擎，三个工程决策定义了它的性格：**非回溯引擎**——匹配时间与输入长度线性相关，灾难回溯（ReDoS）在结构上不存在，不受信任的文本也能安全匹配；**编译/匹配分离**——`xrtRegexCompile` 产出不可变的引用计数对象（编译一次处处共享），`xregexmatcher` 是每次遍历独占的执行缓存（同一文本反复扫描零重复分配）；**命名捕获**——`(?<name>...)` 按组名读取，替代脆弱的数字编号。本章讲清这三个决策的用法与收益，正则语法本身按标准子集速查。

## 引入

两个真实事故。事故一：日志服务用回溯引擎匹配用户提交的过滤表达式，一个 `^(a+)+$` 风格的模式让单条匹配跑了三十秒——队列积压、服务假死；这类"灾难回溯"是回溯引擎的结构性缺陷，模式写法决定了最坏复杂度。事故二：配置解析里 `(\w+)=(\d+)` 的捕获组靠编号取值——维护者在中间加了一个分组，所有编号右移一位，下游取值全部错位，编译器毫无怨言。

`xregex` 对两个事故都免疫：非回溯引擎的最坏时间是线性的——不存在"坏模式"，只有"慢一点的模式"；命名捕获 `(?<key>\w+)=(?<value>\d+)` 按名字取值——加组、改组都不影响既有取值代码。再加上"编译对象不可变可共享、匹配器独占缓存"的生命周期设计，它就是为服务端长期运行准备的引擎。

## 概念

### 生命周期：编译与匹配两层

```diagram flow
- 编译：xrtRegexCompile(模式) → 不可变对象（引用计数，Retain/Release 共享）
- 匹配器：MatcherCreate(编译对象, 文本) → 独占执行缓存
- 扫描：Find(偏移) 找首个 / Next 推进下一个——循环全扫
- 捕获：CaptureNamed(组名) 读文本与范围
- 收尾：MatcherFree 归还缓存；Release 归还编译对象
```

编译层不可变：同一个 `xregex*` 被多个线程、多个匹配器共享，永不修改——引用计数（第 30 章同款原语）管它的生死。匹配器层独占：每次遍历持有自己的缓存（回溯位置、捕获槽），**同一文本反复扫描时缓存复用**——扫一百遍只初始化一次。两层分离让"编译一次、匹配百万次"的成本模型成立。

### 非回溯：线性保证

回溯引擎在失败时尝试所有路径——某些模式让路径数指数爆炸（ReDoS 的根源）。非回溯引擎（NFA/DFA 派）**同时跟踪所有可能状态**，每个输入字符只处理一遍——最坏时间与输入长度线性相关。工程含义：匹配耗时有了硬上界，可以放心地用在请求路径与不受信任输入上。代价是对某些模式常数略大——对绝大多数模式无感，换来的是"没有坏模式"的确定性。

### 命名捕获与替换

`(?<name>[A-Za-z_]+)=(?<value>\d+)` 定义两个命名组；`xrtRegexMatcherCaptureNamed(匹配器, "value", ...)` 按名读出文本视图与范围。替换 `xrtRegexReplace(编译对象, 文本, 替换, ...)` 支持 `$1`/`$2` 编号引用（命名组同时有隐含编号）；`xrtRegexEscape` 把任意文本转义成"字面匹配"的形式——把用户输入拼进模式前必经的一步。`xrtRegexFullTest` 判"整串完全匹配"，与"找子串"的 Find 语义互补。

### 语法子集速查

| 语法 | 含义 |
| --- | --- |
| `.` `\d` `\w` `\s` | 通配/数字/单词/空白类 |
| `[abc]` `[^abc]` | 字符集与否定集 |
| `*` `+` `?` `{m,n}` | 量词 |
| `(...)` `(?<name>...)` | 分组与命名组 |
| `^` `$` `\b` | 行首行尾与词边界 |
| `a\|b` | 分支 |

语法是主流正则的公共子集——不含回溯引用与前瞻（它们是非回溯引擎不支持的形态，文档写明而不是静默忽略）。

## 示例

### 完整程序：编译、匹配与命名捕获

来自仓库范例 `examples/text/regex/main.c`——从配置文本里提取 `width=128` 与 `height=72`：

```embed path="examples/text/regex/main.c" title="examples/text/regex/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/text/regex/main.c -lws2_32 -liphlpapi
width = 128
height = 72
```

**刚才发生了什么。** ① `xrtRegexCompile` 编译一次得到不可变对象——本例局部使用、结尾 `Release`；长期场景挂在全局（启动编译、退出释放），每次匹配只借引用。② 匹配器创建后 `Find` 从偏移 0 找首个匹配——`(?<key>...)` 捕获组名对。③ `CaptureNamed("key")` 与 `CaptureNamed("value")` 按名读出文本视图——比 `$1`/`$2` 的编号取值抗重构。④ 循环里 `Next` 推进到下一个匹配——`width` 与 `height` 各扫一遍输出；匹配器的缓存在两次匹配间复用。⑤ 整个过程零"灾难回溯"风险——非回溯引擎对任何模式都是线性扫描。

### 完整程序：替换与转义

来自 `examples/text/regex_replace/main.c`——把 `name=value` 批量改成 `name: value`：

```embed path="examples/text/regex_replace/main.c" title="examples/text/regex_replace/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/text/regex_replace/main.c -lws2_32 -liphlpapi
width: 128 height: 72
```

**刚才发生了什么。** ① 同样的命名组模式，这次走 `xrtRegexReplace`——`$1` 与 `$2` 引用捕获组（命名组有隐含编号 1、2）。② 替换产物是拥有式字符串（`xrtFree` 释放）。③ 这个示例没有用 `Escape`，但把它记在这里：**把用户输入拼进模式前必须 `xrtRegexEscape`**——用户输入 `a(b` 里的括号是语法字符，不转义就是非法模式或错误匹配（坑 2 的主角）。

## 契约

- **线性保证**：非回溯引擎，最坏匹配时间与输入长度线性相关——不存在灾难回溯模式。
- **两层生命周期**：编译对象不可变、引用计数共享；匹配器独占缓存、每次遍历一个。
- **命名捕获**：`(?<name>...)` 定义、`CaptureNamed` 读取；编号引用 `$1` 同时可用。
- **转义纪律**：动态内容进模式前必经 `xrtRegexEscape`。
- **语义区分**：`Find` 找子串、`FullTest` 判整串——按需求选，不要用 `^...$` 模拟。
- **语法子集**：主流公共子集；回溯引用与前瞻不支持——文档写明而非静默。

### 从示例到工程：正则的三个宿主

**验证层**：输入合法性检查（用户名规则、SKU 格式）——模式静态编译为全局，每请求只建匹配器、`FullTest` 判整串。验证模式的三个纪律：尽量锚定（`^...$` 或 FullTest 语义）、字符集白名单优于黑名单、失败消息带上格式说明。**提取层**：从半结构化文本抽数据（日志字段、配置行）——命名捕获 + `Next` 全扫，提取结果进值树或业务结构；这是第 35 章文本管线的中间站之一。**替换层**：模板化改写（格式转换、脱敏）——`Replace` 的捕获引用把重组逻辑留在替换串里。三层都适用同一条性能纪律：**编译在启动、匹配在热路径**——坑 1 的 good 形态是三层共同的代码骨架。

### 与字符串族、通配的分工

第 25 章的三种文本匹配工具什么时候让位给正则？**字面查找**（找固定的 `ERROR`）用 `xrtStrFind`——不需要正则的开销；**简单通配**（`*.log`）用 `xrtStrGlob`——更简单更快；**结构模式**（"三个数字一段、用横线连接"）才是正则的领地。口诀：**模式能用一句话说清就能用字符串工具；需要"形状描述"才上正则**。滥用正则的代价不只是性能——可读性、可维护性、转义陷阱（坑 2）全在等着；用对了工具的代码，评审时自己会解释自己。

### 一个测试观：模式也是代码

正则模式是浓缩的代码——它该享受代码的待遇。三个习惯：**模式命名**——`static const char* PATTERN_SKU = "..."` 加一句注释说明意图，不要散落在调用点；**边界用例集**——每个模式配"应命中/不应命中/边界"三组样例（第 26 章边界值思想的移植），模式修改时回归；**性能抽样**——用第 6 章统计或简单计时对代表性输入跑一遍，确认无常数意外。第 11 章的可复现随机（固定种子生成畸形输入）给模式的模糊测试提供了现成工具——这些纪律在第 105 章测试体系会再次系统化。

## 避坑

### 坑 1：每请求重新编译模式

症状：热路径 CPU 异常；profiler 显示热点在 `xrtRegexCompile`——每次请求都把同一个模式完整编译一遍。

原因：编译/匹配两层分离的意图被无视——编译是重操作（构建自动机），匹配是轻操作；重操作进了循环。

```c bad
bool handle(const char* sFilter)
{
	xregex* pRegex = xrtRegexCompile((xstrview){ sFilter, strlen(sFilter) });
	/* ... 匹配一次 ... */
	xrtRegexRelease(pRegex);   /* 每请求编译+释放——自动机白建了 */
}
```

```c good
/* 启动时编译一次，全局共享 */
static xregex* gFilter;
void init(void) { gFilter = xrtRegexCompile(XRT_STR_LITERAL("...")); }
bool handle(const char* sText)
{
	/* 只建匹配器（轻），编译对象借引用 */
	xregexmatcher* M = xrtRegexMatcherCreate(gFilter, Text);
	/* ... Find/Next ... */
	xrtRegexMatcherFree(M);
}
```

### 坑 2：用户输入直接拼进模式

症状：用户输入含 `(`、`*`、`[` 时编译失败或行为诡异；更糟的是精心构造的输入让模式语义完全偏离（注入类漏洞）。

原因：正则语法字符没有转义——用户文本被当成了模式的一部分。

```c bad
xstrview UserInput = ...;
xstrbuf Pat;
xrtStrBufInit(&Pat);
xrtStrBufAppend(&Pat, XRT_STR_LITERAL("^"));
xrtStrBufAppend(&Pat, UserInput);          /* "(admin)" 进模式——语法字符！ */
xrtStrBufAppend(&Pat, XRT_STR_LITERAL("$"));
str sPat = xrtStrBufTake(&Pat);
xregex* R = xrtRegexCompile((xstrview){ sPat, strlen(sPat) });   /* 编译失败或注入 */
```

```c good
xstrbuf Pat;
xrtStrBufInit(&Pat);
xrtStrBufAppend(&Pat, XRT_STR_LITERAL("^"));
str sEscaped = xrtRegexEscape(UserInput, NULL);
xrtStrBufAppend(&Pat, (xstrview){ sEscaped, strlen(sEscaped) });  /* 转义后是纯字面量 */
xrtFree(sEscaped);
xrtStrBufAppend(&Pat, XRT_STR_LITERAL("$"));
```

## 练习

### 基础：命名提取（命名捕获的手感）

复现主示例：对 `"width=128 height=72 depth=24"` 全扫输出三组键值；再给不存在的键（如 `color`）验证扫描自然跳过。

### 进阶：日志过滤器

用命名组匹配 `[WARN] 2026-04-02 disk 90%`——级别、日期、模块、数值四个组各自捕获；统计一段日志里 WARN 与 ERROR 的计数差。提示：日期组捕获后再用第 39 章时间解析（如果有）或保持字符串即可。

### 挑战：搜索高亮器

实现 `highlight(文本, 查询词)`：查询词经 `Escape` 转义后编译为"整词匹配"模式（加 `\b` 边界），全文 Find 出所有命中位置并输出带 `<<>>` 标记的文本。验收标准：查询词含正则语法字符（`(`、`*`、`[`）时行为仍是字面匹配；连续命中不重叠；100KB 文本 1000 个命中的耗时线性可测。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 线性保证 | 非回溯引擎——最坏线性，无 ReDoS，不受信任文本可安全匹配 |
| 两层模型 | 编译对象不可变共享（Retain/Release）/ 匹配器独占缓存（Create/Free） |
| 成本纪律 | 编译一次全局共享；循环里只建匹配器 |
| 捕获 | `(?<name>...)` + `CaptureNamed`；`$1`/`$2` 编号引用在替换里用 |
| 转义 | 动态内容进模式前 `xrtRegexEscape`（返回拥有式串）——字面匹配的唯一安全通道 |
| 语义 | Find 子串 / FullTest 整串 / Next 推进 / Replace 替换 |
| 三宿主 | 验证层（锚定+白名单）/ 提取层（命名捕获全扫）/ 替换层（捕获引用重组） |
| 工具分工 | 字面查找用 StrFind / 简单通配用 StrGlob / 形状描述才上正则 |
| 模式纪律 | 模式命名 + 边界用例集 + 性能抽样——模式也是代码 |
