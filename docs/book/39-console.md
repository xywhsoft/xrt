---
num: 39
slug: console
title: 控制台与终端检测
volume: 卷五 系统服务
type: practice
lead: stdout/stderr 分流、终端检测与彩色降级、进度刷新不刷屏——CLI 的第一印象。
api: console
---

## 导读

控制台是 CLI 工具与用户的第一接口，console 模块处理它的三个工程问题：**流选择**（正常输出走 stdout、诊断走 stderr——管道组合时互不污染）、**终端检测**（`xrtConsoleIsTerminal`——是交互终端才启用彩色与进度动画，重定向时自动降级为纯文本）、**原地刷新**（进度条/spinner 用回车不换行覆盖——日志与进度同屏时不互相践踏）。小模块，大体验——一个"重定向后满屏转义符"的工具与一个优雅降级的工具，用户信任度天差地别；而两者的代码差距可能只是几行检测分支。

## 引入

三个翻车现场。现场一：`tool > out.txt` 后文件里满是 `\033[32m` 彩色转义序列——作者在写死 ANSI 颜色时没考虑输出目标不是终端。现场二：CI 日志里进度条刷了三千行——spinner 的每次刷新都换行，因为它不知道 stdout 已经被管道接走。现场三：把诊断信息打到了 stdout——用户的 `tool | grep result` 被 warn 混入，脚本解析当场翻车。

三个现场的共同根因：**把控制台当成一个固定的东西，而不是一个有两种流、两种形态、需要运行时适配的环境**。实际上它有两个流（stdout/stderr，语义不同）、两种形态（交互终端/重定向管道，能力不同）——正确的代码在运行时检测并适配。console 模块把这套适配标准化：流选择、终端查询、以及"刷新只在终端生效"的输出策略。

## 概念

先立框架再进细节：本章的概念区按"两个流 → 检测与降级 → 刷新纪律"三层展开，正好对应引入的三个翻车现场——读完概念区，三个现场都应该能口头复述出解法。

### 两个流的语义分工

| 流 | 语义 | 消费者 |
| --- | --- | --- |
| stdout | 程序的**产出**——结果数据、正常输出 | 管道下游（grep/jq/文件） |
| stderr | 程序的**旁白**——诊断、警告、进度 | 人类读者、日志采集 |

分工的纪律（Unix 五十年的沉淀，值得逐字遵守）：**stdout 是机器接口**——格式稳定、可解析、绝不混入人类向内容；**stderr 是人类接口**——诊断随意、彩色无妨。违反分工（诊断进 stdout）不是风格问题，是**破坏了管道组合性**——Unix 哲学的惩罚从来都是迟到的。

### 终端检测与能力降级

`xrtConsoleIsTerminal(流)` 查询目标是不是交互终端——注意按流查询：stdout 可能接管道而 stderr 还在终端上，两者独立判断。能力降级的决策树：彩色——终端开、重定向关；进度动画——终端原地刷新、重定向改为节流打印（每 N 条或每秒一行）；交互提示——终端可等待输入、重定向直接失败退出（管道里没有人在键盘前）。

### 降级决策树

```diagram flow
- 彩色输出：终端开 / 重定向关——转义符只对终端有意义
- 进度动画：终端原地刷新（限速）/ 重定向节流里程碑（每 25%）
- 交互提示：终端可等待输入 / 管道直接失败退出——管道另一端没有人
- 输出宽度：终端按检测宽度折行 / 重定向不折（下游自己排版）
```

### 原地刷新

进度输出的两个姿势：**换行追加**（每条一行——日志式，重定向下唯一正确姿势）与**原地覆盖**（回车不换行——动画式，仅终端）。原地刷新（动画式，仅终端——回车不换行覆盖上一帧）的纪律：刷新频率限速（人眼 10fps 足够，更高是浪费）；退出时打印最终换行（否则下一行 shell 提示符粘在进度条尾巴上）；日志与进度同屏时进度独占最后一行（日志打印前先把进度行清掉——这也是 variants 范例演示的协调形态）。

## 示例

### 完整程序：流分流输出

来自仓库范例 `examples/console/output/main.c`：

```embed path="examples/console/output/main.c" title="examples/console/output/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/console/output/main.c -lws2_32 -liphlpapi
$ ./a.exe
（stdout）service started
（stderr）example diagnostic
$ ./a.exe 2>/dev/null
（stdout）service started
（stderr 行被丢弃：stdout 保持纯净可管道）
```

**刚才发生了什么。** ① 正常输出走 stdout、诊断走 stderr——`2>/dev/null` 的对照实验直接验证了分流。② 管道组合性：`./a.exe | grep service` 只会命中 stdout 行——机器接口与人类接口互不越界。③ 这个分流与第 37/38 章的 Logger 汇流：Logger 的控制台 Sink 同样遵守"INFO 以上 stdout、ERROR 以下 stderr"或按配置——控制台模块是它的底层流约定。

### 完整程序：终端检测与降级

来自 `examples/console/variants/main.c`：

```embed path="examples/console/variants/main.c" title="examples/console/variants/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/console/variants/main.c -lws2_32 -liphlpapi
$ ./a.exe | cat
write-ok=1
is-terminal=0
```

**刚才发生了什么。** ① 写入接口在两种流上都工作（`write-ok=1`）。② `is-terminal=0`——管道重定向下检测返回假；在真终端跑同一程序会得到 1（真终端行为无法在本输出块中展示——检测随目标而变是它的本义）。③ 这一行 if 就是全部降级逻辑的地基：`if ( xrtConsoleIsTerminal(stdout) ) { 彩色+动画 } else { 纯文本+节流 }`——variants 范例的其余部分演示了各输出变体（宽度、对齐、前缀）在不同流上的行为。

### 三个容易被忽视的终端行为

**行缓冲 vs 全缓冲**：stdout 连终端时行缓冲（每行即刷）、连管道时变全缓冲（攒满才刷）——这就是"管道下 stdout 输出迟迟不出现、stderr 反而先出来"的原因（stderr 永远无缓冲）。对拍类工具要么关键输出前 `fflush(stdout)`，要么进度走 stderr。**宽度检测**：`xrtConsoleIsTerminal` 之外还有宽度查询——按终端宽度折行的表格在管道下应切到固定宽度（80 列传统）；宽度读取失败（非终端）用默认值。**编码假设**：本章输出假设 UTF-8 终端（第 27 章主线）；检测到传统 Windows 代码页控制台时的转码是宿主层的责任——跨平台工具建议 README 里写明"建议 UTF-8 终端"，比在工具里做全套转码务实。三个行为都属于"不知道就会在某天撞上"的暗礁——知道存在比记住细节重要。

## 契约

- **流分工**：stdout=产出（机器接口）、stderr=旁白（人类接口）；诊断绝不进 stdout。
- **终端检测**：彩色/动画/交互按 `IsTerminal` 结果降级；重定向下纯文本。
- **刷新纪律**：原地刷新限速、终态换行、与日志协调最后一行。
- **管道友好**：输出格式稳定可 grep/jq；进度信息走 stderr 或节流。

### 从示例到工程：CLI 体验三原则

**原则一：默认安静**——成功时少说话（结果输出之外，stderr 只在有事时开口）；`-v` 递增详细度，而不是默认啰嗦加 `-q` 静默。**原则二：错误可行动**——stderr 上的每条错误都带"接下来怎么办"（缺文件→提示路径、参数错→提示用法）；与第 4 章错误链配合：原因链逐层展开成人类可读的行动指引。**原则三：退出码是接口**——0 成功、非零按错误类别区分（与第 4 章的通用错误类别对齐，参数错/IO 错/超时各占一段取值区间）；脚本靠退出码分支，不靠解析输出文本猜。三原则合起来：CLI 的"用户体验"不是花哨，是**可预测**——管道、脚本、人类三方都能预测它的行为。

### 与第 37/38 章的关系

console 模块是 Logger 控制台 Sink 的底层约定提供者（分层的最底层是操作系统的两个流，本章是查询与约定的封装，Logger 在其上装配策略）：Sink 的 stdout/stderr 选择（INFO→stdout、ERROR→stderr 的常见策略）基于本章的流分工；Sink 的彩色文本格式在重定向下自动降级（IsTerminal 检测在 Sink 内部完成——你配了彩色文本 Sink，管道下它自己变纯文本）。理解了这个分层，日志配置里的"彩色"选项就不再是魔法开关——它只是把你手写的 if 分支（坑 1 的 good 形态）下沉到了库。反过来，直接用 console 模块写 CLI 输出时，你也拥有与 Logger 同套的检测能力——工具与服务的控制台行为全库一致。

## 避坑

### 坑 1：写死彩色转义（重定向下灾难）

症状：重定向文件里满屏 `\033[..m`；下游解析脚本被转义符破坏。

原因：颜色决策写死在输出语句里，没有走"终端才彩色"的检测分支。

```c bad
printf("\033[32mOK\033[0m done\n");   /* 无条件彩色——管道下灾难 */
```

```c good
if ( xrtConsoleIsTerminal(stdout) ) {
	printf("\033[32mOK\033[0m done\n");   /* 终端：彩色 */
} else {
	printf("OK done\n");                   /* 管道：纯文本 */
}
```

### 坑 2：进度条刷爆 CI 日志（原地刷新的管道退化）

症状：CI 的日志文件里进度行上万条；有用信息被 spinner 淹没。

原因：原地刷新（回车覆盖）在非终端下退化为普通换行——每帧一行；检测分支没覆盖这个场景。

```c bad
while ( working ) {
	printf("\rprogress: %d%%", pct);   /* 非终端下 \r 不覆盖——每帧一行 */
	fflush(stdout);
}
```

```c good
bool bTerm = xrtConsoleIsTerminal(stderr);
while ( working ) {
	if ( bTerm ) {
		fprintf(stderr, "\rprogress: %d%%", pct);   /* 终端：覆盖 */
	} else if ( pct >= next_milestone ) {
		fprintf(stderr, "progress: %d%%\n", pct);   /* 管道：节流里程碑 */
		next_milestone += 25;
	}
}
if ( bTerm ) { fprintf(stderr, "\n"); }   /* 终态换行 */
```

## 练习

### 基础：分流三连（重定向实验）

写程序向 stdout 打结果、stderr 打进度，分别用 `> file`（stdout 落盘）、`2> file`（stderr 落盘）、`| grep`（只接 stdout）三种重定向验证各流各收到什么——三个实验各写一行结论。

### 进阶：优雅降级的进度条（终端动画与管道里程碑）

实现进度输出：终端彩色 spinner + 百分比原地刷新（10fps 限速）；重定向时每 25% 打一行里程碑；结束时终端打印终态行。用 `| cat` 与真终端各跑一遍对照。

### 挑战：管道友好的报告工具（三原则的完整落地）

实现 `report`：结果表（stdout，格式稳定、列对齐）、进度与统计（stderr，带里程碑节流）、`--quiet` 模式（stderr 静默，只剩错误）。验收标准：`report | grep 关键字` 精确命中结果行且不含任何进度残留；`report > out.txt` 产物零转义符零进度行、可直接进 diff；`--quiet` 下 stderr 只剩错误行；三种退出码（成功/参数错/IO 错）各自正确。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 流分工 | stdout 产出（机器）/ stderr 旁白（人类）；诊断不进 stdout |
| 终端检测 | `xrtConsoleIsTerminal(流)`；彩色动画交互按此降级 |
| 刷新 | 原地覆盖限速 10fps、终态换行、重定向下改节流里程碑行 |
| 管道友好 | stdout 输出可 grep/jq；进度走 stderr 或按里程碑节流 |
| 与 Logger | 控制台 Sink 遵守同套流约定——模块是底层地基 |
| 缓冲差异 | stdout 终端行缓冲/管道全缓冲；stderr 永不缓冲——对拍注意 |
| CLI 三原则 | 默认安静 / 错误可行动 / 退出码是接口——可预测胜过花哨 |
