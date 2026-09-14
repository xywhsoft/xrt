---
num: 52
slug: process
title: 子进程管理
volume: 卷六 进程与并发
type: practice
lead: Shell/Spawn 双族、输出捕获与管道串联、退出状态三态——把外部程序当函数调的工程化姿势。
api: process, signal
---

## 导读

process 模块把"启动外部程序"工程化：**Shell 族**（`xrtProcessShell`——整条命令串走系统 shell，工具调用的省事路径）与 **Spawn 族**（`xrtProcessSpawn`——argv 数组直启，无注入面，不受信任输入的安全路径）；**输出捕获**（`Result.Stdout`——拥有式缓冲，捕获子进程标准输出）；**管道串联**（`xrtProcessPipeline`——多阶段子进程 stdout 自动接下一段 stdin，等价 shell 的 `a | b`）；**退出状态三态**（正常退出+退出码 / 被信号杀死 / 启动失败——`xrtProcessResultSuccess` 区分判定）。环境继承、工作目录、超时杀灭（SIGTERM→SIGKILL 阶梯）在配置里声明。

## 引入

三个场景。场景一：图片处理服务要调外部 `ffmpeg`——最朴素的 `system()` 调用：命令串拼接（输入文件名带空格就是灾难）、无法捕获输出、无法设超时（ffmpeg 卡住服务跟着卡）。场景二：构建工具要跑 `gcc main.c -o app` 并拿到错误输出——需要 stderr 捕获与退出码，`system` 只给一个 int。场景三：文本流水线 `cat | grep | sort`——多阶段进程串联，手工连管道的句柄管理、双端死锁（上游写满阻塞、下游没读）全是坑。

三个场景对应三组能力：Spawn 族（`xrtProcessSpawn(配置)` 返回句柄）+超时配置（场景一的安全版）、双流捕获+三态判定（场景二）、Pipeline 自动连管（场景三）。Shell 与 Spawn 的分界线是**信任**：命令串来自你自己的代码（可信）用 Shell 图省事；来自用户输入（不可信）必须 Spawn+Argv——命令注入与 SQL 注入同族，防线就是"不把输入拼进解释器"。

## 概念

### 双族：Shell 与 Spawn

| 维度 | Shell 族 | Spawn 族 |
| --- | --- | --- |
| 入口 | `xrtProcessShell(命令串, 结果)` | `xrtProcessSpawn(配置)` |
| 解释 | cmd/sh 解释元字符 | 内核直启，零解释 |
| 适用 | 工具调用、可信命令 | 不可信输入、精确控制 |
| 注入面 | 命令串拼接即风险 | 无——参数原样传递 |

配置（`xprocessconfig`）声明运行环境：工作目录、环境变量（增量覆盖第 43 章进程环境）、标准流处理（丢弃/继承/捕获/管道）、超时与杀灭阶梯（超时先发 SIGTERM、宽限期后 SIGKILL——第 49 章收发分工的发送端）。

### 退出状态三态

```diagram flow
- 启动失败：可执行不存在/权限——父进程错误（第 4 章错误槽）
- 正常退出：子进程 exit(code)——退出码进结构体
- 信号终止：被信号杀死（超时杀灭/崩溃）——信号编号进结构体
- 判定：xrtProcessResultSuccess = 正常退出 且 码为 0
```

三态区分的意义：退出码非 0 是**子进程的语义信号**（gcc 报编译错）；被信号杀是**异常终止**（超时、崩溃）——两者的处置完全不同（前者看输出、后者要诊断）。`Success` 是"码 0 且正常"的便捷判定，完整三态在结构体字段里。

### 管道串联

`xrtProcessPipeline(阶段数组, ...)` 把 N 个阶段一次生成并串联——阶段间的 stdout→stdin 是**真实 OS 管道**（数据不经过父进程——吞吐与隔离双优）；末段输出可选捕获。与手工 Spawn 连管的对比：句柄继承配置、两端关闭时机、双端缓冲死锁——三个经典坑全部由实现接管（pipeline 范例注释的原话）。

### 流捕获的生命周期

`Result.Stdout/Stderr` 是**拥有式缓冲**——`xrtProcessResultUnit` 释放（含缓冲）；忘记 Unit 就是每进程一次的泄漏。捕获与管道的组合规则：末段可以捕获（中间段的输出进了管道——父进程看不到，这正是流水线的意义）。

## 示例

### 完整程序：Shell 执行与输出捕获

来自仓库范例 `examples/process/capture/main.c`：

```embed path="examples/process/capture/main.c" title="examples/process/capture/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/process/capture/main.c -lws2_32 -liphlpapi
captured output
```

**刚才发生了什么。** ① `xrtProcessShell("echo captured output", &Result)`——整条命令走系统 shell；平台差异（Windows 的 echo 与 POSIX 的 printf）由范例的条件编译处理，运行时行为一致。② `Result.Stdout` 携带捕获的拥有式缓冲——直接打印验证；`xrtProcessResultSuccess` 判定"正常退出且码 0"。③ `xrtProcessResultUnit` 释放结果——**捕获缓冲跟着结果走**，Unit 是它的唯一出口（忘记 Unit 每次泄漏一块）。④ 这个形态就是"外部程序当函数调"的最短路径：输入命令串、输出（缓冲+状态）两件套。

### 完整程序：多阶段管道

来自 `examples/process/pipeline/main.c`：

```embed path="examples/process/pipeline/main.c" title="examples/process/pipeline/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/process/pipeline/main.c -lws2_32 -liphlpapi
pipeline output
```

**刚才发生了什么。** ① 两个阶段的配置数组（每阶段一个 `xprocessconfig`）一次交给 `xrtProcessPipeline`——生成、连管、启动、汇合全由它完成。② 阶段间的管道是 OS 级的（Windows 段用 findstr、POSIX 段用 tr——平台各取等价工具）；数据流**不经过父进程**——吞吐不受父进程中转拖累。③ `Result.Stdout` 捕获末段输出、`xrtProcessPipelineSuccess` 判定全段成功——任一段失败整条判定失败（流水线的失败语义：全成才算成）。④ 对比手工连管：句柄继承、双端死锁这些坑在范例注释里点名——由实现接管正是 Pipeline 存在的理由。file 范例演示输出重定向到文件（`xrt-process-output.txt`）——捕获与重定向是标准流处理的两种目的地。

## 契约

- **双族选择**：可信命令 Shell 省事、不可信输入必须 Spawn+Argv——注入防线与 SQL 参数化同族。
- **三态语义**：启动失败（父进程错误）/ 正常退出（码）/ 信号杀死（信号）——Success=码 0 且正常。
- **结果配平**：`ResultUnit` 释放含捕获缓冲——有 Result 必有 Unit。
- **超时阶梯**：配置声明超时与杀灭（TERM→宽限→KILL）——卡死的子进程不会拖死父进程。
- **管道语义**：阶段间 OS 管道直连、数据不过父进程；末段可捕获；任一段失败全链失败。
- **环境控制**：工作目录与环境变量在配置声明——子进程的运行环境是显式契约不是继承彩票。

### 从示例到工程：子进程的三个宿主

**工具胶水**（Shell 族主场）：转码、压缩、通知脚本——系统已有成熟命令行工具时，调用它比重写库快且稳；命令串是代码常量（可信）、输出捕获、超时阶梯——三件配置一步到位。**安全边界**（Spawn 族主场）：用户触发的文件处理、插件执行——argv 化杜绝注入、工作目录与环境显式声明、资源（超时+输出上限——`StdoutTruncated` 字段告诉你被截了）三面受控。**数据流水线**（Pipeline 主场）：ETL 步骤、构建链——多阶段串联交给 OS 管道（吞吐优于父进程中转）、全段成功判定（构建语义）。三宿主共同底线：**子进程是边界外的代码**——超时、输出上限、退出三态是每次调用都要想的三件事，图省事跳过任何一件都会在生产以另一种方式补课。

### 结果结构体的完整账本

`xprocessresult` 的字段比"输出+退出码"丰富一截，值得点一遍名。`Status`——三态本体（正常/信号/失败）；`Wait`——等待结局（超时杀灭在这里现身）；`InputWritten`——喂给子进程 stdin 的字节数（写管道可能 partial）；`Stdout/Stderr` 双流各自带尺寸与**截断标记**（`StdoutTruncated`——配置了捕获上限且超限时置位：读到的可能不是全部，判定前必查）；`Duration`——耗时微秒（第 42 章口径——性能观测的现成字段）。把字段当账本读：**每个字段都是一次"曾经可能出问题"的固化**——截断标记防误判、耗时防盲区、双流分离防混读。写自己的封装时照这个完备度设计——结果结构体的字段密度就是 API 作者踩坑密度的化石。

### 与卷六的衔接：进程也是并发单元

本章在卷六开卷不是排序巧合——**进程是最古老的并发单元**。与线程的对照：进程隔离强（地址空间独立——崩溃不传染）、启动贵（fork/exec 代价）；线程共享内存（通信零拷贝但需同步——第 53 章）、启动便宜。选型由隔离需求定：跑不可信/易崩代码（插件、第三方工具）用进程；协作计算用线程+本章之后的并发体系。进程与其他并发单元的交互点：Pipeline 的阶段间并发（OS 管道做通道——进程版的 Channel）；超时杀灭与取消体系（第 54 章令牌配 SIGTERM——外部进程的"可取消等待"形态）；捕获缓冲的 Unit 配平（与值树、令牌同款纪律）。**进程、线程、协程是三种并发粒度**——本章最粗、第 55 章最细，中间的地板在下一章。

## 避坑

### 坑 1：用户输入拼进 Shell 命令串

症状：安全审计报命令注入——输入 `; rm -rf /` 或 `$(curl ...)` 时命令被解释执行。

原因：Shell 族把命令串交给 shell 解释——元字符（`;`、`|`、`$`、反引号）都是攻击面；用户数据与命令语法混在同一层。

```c bad
char sCmd[512];
snprintf(sCmd, sizeof(sCmd), "convert %s -resize 100 %s", sUserInput, sOut);
xrtProcessShell(sCmd, &Result);   /* 输入含 ";" 时注入任意命令 */
```

```c good
/* argv 在 xprocessconfig 的字段声明；Spawn 返回句柄、等待后取结果 */
xprocess* P = xrtProcessSpawn(&Config);   /* 参数原样传递——零解释 */
```

### 坑 2：忘记 ResultUnit 泄漏捕获缓冲

症状：长期运行的服务缓慢增长——每次外部调用泄一块；第 6 章统计的活跃字节阶梯上涨。

原因：`Result.Stdout` 是拥有式缓冲、`ResultUnit` 是它唯一出口——Result 的使用路径没有配平。

```c bad
if ( xrtProcessShell(sCmd, &Result) && xrtProcessResultSuccess(&Result) ) {
	Use(Result.Stdout, Result.Size);   /* 用完直接丢——捕获缓冲泄漏 */
}
```

```c good
if ( xrtProcessShell(sCmd, &Result) ) {
	if ( xrtProcessResultSuccess(&Result) ) {
		Use(Result.Stdout, Result.Size);
	}
	xrtProcessResultUnit(&Result);   /* 成败路径都释放 */
}
```

## 练习

### 基础：三态制造机

分别构造三种结局的子进程（正常码 0、正常码非 0、被信号杀）——`xrtProcessShell` 各跑一次，打印 Result 的三态字段与 Success 判定。

### 进阶：带超时的调用器

`run(命令, 超时秒数)`：超时走 TERM→KILL 阶梯；输出捕获；返回三态。用它调用一个 sleep 命令验证超时路径的信号字段。

### 挑战：安全转码服务

用 Spawn 族实现图片转码接口：文件名与参数全部 argv 化（零拼接）、输出捕获、超时阶梯、并发上限（第 22 章信号量或任务池）。验收标准：注入测试集（二十个恶意文件名）零执行；超时进程必被回收（无僵尸）；并发限制生效；转码结果与退出码对得上。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 双族 | Shell（命令串+结果一步、可信）/ Spawn（配置→句柄、不受信输入） |
| 三态 | 启动失败 / 正常退出（码）/ 信号杀死（信号）；Success=码 0 且正常 |
| 结果配平 | ResultUnit 释放含捕获缓冲——有 Result 必 Unit |
| 管道 | Pipeline 阶段数组一次串联；OS 管道直连；末段可捕获 |
| 超时 | 配置声明 TERM→宽限→KILL 阶梯——发送端在第 49 章对面 |
| 环境 | 工作目录/环境变量配置化——运行环境是契约不是彩票 |
