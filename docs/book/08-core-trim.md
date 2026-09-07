---
num: 8
slug: core-trim
title: 版本、ABI 与模块裁剪
volume: 卷一 起步与核心
type: practice
lead: 正向模块选择与依赖闭包——从一行宏到精确的二进制；版本双口径与交付形态的工程约定。
api: core, memory_stats
---

## 导读

第 2 章你用两行宏把 XRT 编译了进去，本章把这两行宏讲透：`XRT_MODULE_*` 的**正向选择**如何经依赖闭包展开成精确的特性集合，`XRT_EXCLUDE_*` 如何在保留完整功能的同时排除侵入式诊断，编译期的 `XRT_FEATURE_*` 如何被探测。再看版本：编译期宏与 `xrtVersion()` 双口径各管什么、头与库如何核对同版。这些内容平时隐形，一旦出问题（链接失败、体积异常、头库错配）就全是它们——本章把卷一收口在这里，也把第 2 章练习"三档体积对比"的完整答案补上。

## 引入

同一个库要进两种完全不同的宿主：一个几百 KB 的命令行小工具，只做 JSON 配置读取；一个常驻后台服务，要网络、TLS、HTTP 全家。散装依赖的时代这是两份依赖清单、两套构建配置；而且"小工具"那一半往往被迫链接整个传输层，因为依赖树的粒度只有"库"一级。

XRT 的交付粒度是**模块**：91 个公开头文件背后是数百个可裁剪模块，每个模块声明自己的源文件与依赖。你在编译单元里声明需要的根模块，构建体系展开闭包——小工具链接它用到的那几千行，服务链接它的全部，同一个版本、同一套头文件。这个机制的名字叫**正向模块选择**：只声明"要什么"，不声明"不要什么"。

与"反向排除"（全量编译再剔除）相比，正向选择有一个决定性优点：**新增依赖不会悄悄改变你的构建**。反向体系里，某模块今天新依赖了一个传输层，你的"排除清单"没变、产物却胖了一圈；正向体系里，你声明的根没变，闭包怎么变都只影响该根自己的子树。交付的可预测性由此而来——这也是为什么本书从第 2 章开始就只教正向写法。

## 概念

### 依赖闭包：从根模块到特性集合

```diagram flow
- 声明根模块：define XRT_MODULE_REGEX（只写你要的）
- 展开闭包：features.h 按 modules.json 递归带入全部依赖
- 生成特性宏：每个受影响模块得到一个 XRT_FEATURE_* 定义
- 精确实现：只有被选中模块的源码参与编译与链接
```

四个要点。**只声明根**：依赖表由 `config/modules.json` 维护，用户不需要也不应该复制它——这就是"正向选择"的含义。**宏先于包含**：模块宏必须在该翻译单元**第一次**包含 `xrt.h` 或 `xrt/features.h` 之前定义，晚了不生效。**实现一次**：`XRT_IMPLEMENTATION` 在整个工程的一个翻译单元里定义一次（第 2 章坑 1 的根源在此）。**集合一致**：模块化链接时，所有参与同一 XRT 实例的翻译单元应使用一致的模块集合——两个单元各选各的，链接出的实例行为未定义。

`modules.json` 作为单一事实来源还值得多看一眼：每个模块的源文件、内部头、依赖、测试、示例都在清单里，`features.h`（闭包展开的依据）与官网参考页同样从它生成。你在第 2 章看到的"模块域"、第 6 章的素材清单、本章的裁剪行为，全部指向同一份数据——文档、代码、构建三者不会各说各话，这是"单一事实来源"在工程上的兑现。

### 排除宏：全功能 minus 诊断

`XRT_MODULE_ALL` 全量启用的场合，如果只需要排除内存调试这类**侵入式诊断**（它改变全局分配路径，第 6 章），用排除宏而不要手工罗列几百个正向宏：`XRT_EXCLUDE_MEMORY_DEBUG` 会同时排除 `memory_debug` 与其报表模块，但保留 `memory_stats`。排除宏只作用于 `XRT_MODULE_ALL` 的隐式选择——正向选择的场合你本来就没选它们。

这个设计的动机值得展开：诊断模块不是"可有可无的附加品"就是"必须显式关掉的东西"两种状态，没有中间态。`XRT_MODULE_ALL` 的语义是"给我完整生产功能"——而完整生产功能恰恰**不需要**内存调试堆常驻。于是排除宏成了表达"全功能减诊断"的标准写法：CI 的发布构建用它排除调试模块，本地开发构建保留全量。理解了这一层，你就能读懂仓库各示例头的模块宏为什么长那样——它们都是这套语义的实例。

### 编译期探测：XRT_FEATURE_*

闭包展开后，每个被启用的模块都会定义对应的 `XRT_FEATURE_` 系列宏。公共头里大量用 `#if defined(...)` 做条件声明——这意味着你的代码也能在编译期探测能力：头文件里根据 TLS 特性是否存在提供不同的内联实现，而不必依赖构建系统的传参。第 9 章原子操作的头文件就是首批消费者之一。

### 版本双口径与交付形态

版本有两个口径（第 3 章演示过同源）：**编译期** `XRT_VERSION_MAJOR/MINOR/PATCH` 与 `XRT_VERSION_TEXT`（整数宏可 `#if` 判断，字符串宏用于展示），**运行期** `xrtVersion()` 返回链接进程序的版本串。单头集成时二者必然一致；**链接库**集成时二者可能不一致——头是新版、库是旧版的时候，`xrtVersion()` 是唯一的真相，程序启动时先核对一遍再干活是标准动作。

交付形态两条路：单头（本书示例全部采用，`single/xrt.h` 自包含实现）与源码/库（`config/modules.json` 声明归属，卷十二讲打包）。两条路的 API 完全一致，切换只改构建脚本。选型的实际考量有三个：**起步速度**——单头复制即用，毫无疑问最快；**构建增量**——中大型工程里单头意味着实现单元的重编译成本随库规模增长，源码/库形态让增量编译回归正常；**符号边界**——动态库交付时导出表可控，宿主进程里多个组件各自链接 XRT 的场景也靠库形态隔离。常见演进路径正是"原型用单头、工程化切库"，两条路的代码一行不用改。

## 示例

### 第一个程序：最小模块集与体积对照

第 2 章的三档对比在这里给出完整版本——同一份程序，两档模块集：

```c
/* hello.c —— 两档通吃：模块宏由编译命令 -D 注入 */
#include <xrt.h>
#include <stdio.h>

int main(void)
{
	printf("XRT %s\n", xrtVersion());
	return 0;
}
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION -I single hello.c -lws2_32 -liphlpapi
$ gcc -O1 -DXRT_IMPLEMENTATION -I single hello.c
$ ls -l a.exe a2.exe
-rwxr-xr-x 1 admin 106496 a.exe
-rwxr-xr-x 1 admin  12288 a2.exe
（体积因版本与工具链而异，比值才是重点）
```

**刚才发生了什么。** 模块宏改用 `-D` 从命令行注入——效果与写在源码里完全等价，好处是同一份源码多档编译（CI 里做裁剪矩阵测试的标准姿势）。`-DXRT_MODULE_ALL` 档拉入全部模块（含网络/TLS/HTTP，所以链接了系统网络库）；第二档**一个模块宏都不写**——core 地基永远在场、不可裁剪，`xrtVersion` 属于 core，所以只定义 `XRT_IMPLEMENTATION` 就能编译，也不需要链接系统网络库。数倍量级的差异说明"裁剪"不是省几个函数，而是整棵依赖子树的进出。

### 第二个程序：编译期探测能力

```c
/* probe.c —— 探测闭包展开了哪些特性 */
#define XRT_MODULE_STRING
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>

int main(void)
{
	printf("version=%s\n", xrtVersion());
#if defined(XRT_FEATURE_STRING)
	printf("feature: string = yes\n");
#else
	printf("feature: string = no\n");
#endif
#if defined(XRT_FEATURE_TLS)
	printf("feature: tls = yes\n");
#else
	printf("feature: tls = no\n");
#endif
	return 0;
}
```

```term
$ gcc -O1 -DXRT_MODULE_STRING -DXRT_IMPLEMENTATION -I single probe.c -o probe.exe
$ ./probe.exe
version=2.0.0-dev
feature: string = yes
feature: tls = no
```

**刚才发生了什么。** 只声明了 `XRT_MODULE_STRING`，闭包展开后 `XRT_FEATURE_STRING` 被定义、`XRT_FEATURE_TLS` 没有——探测结果与选择一一对应。这个模式的价值在头文件与模板代码里：能力差异在**编译期**分流，没有运行时开销，也不需要构建系统额外传参。注意探测宏必须在与 `xrt.h` 同一个翻译单元里判断（先包含后判断），跨头文件传递时保持包含顺序稳定。

把两个示例连起来看，模块体系的完整图景就齐了：命令行的 `-D` 决定"选什么"（交付），`XRT_FEATURE_` 探测反映"选到了什么"（能力），`xrtVersion()` 报告"是谁在提供服务"（版本）。三者在同一个翻译单元里闭环——你不需要在构建脚本之外维护任何一份"本工程启用了什么"的清单，宏与探测就是清单本身，编译器替你核对一致性。

## 契约

- **宏先于包含**：模块宏与 `XRT_IMPLEMENTATION` 都必须在首次包含 `xrt.h`/`features.h` 前定义。
- **实现恰好一次**：一个工程一个实现翻译单元；其余单元只包含声明。
- **集合一致**：同一 XRT 实例的所有翻译单元使用一致的模块集合。
- **排除宏边界**：`XRT_EXCLUDE_*` 只抑制 `XRT_MODULE_ALL` 的隐式选择，不影响正向声明。
- **版本核对**：链接库集成时启动即比对 `xrtVersion()` 与编译期宏；不一致视为部署错误。

## 避坑

### 坑 1：模块宏定义在包含之后

症状：明明定义了宏，行为却像没定义——链接缺失或特性未启用，且没有明确报错。

原因：宏展开发生在包含点，晚于 `#include` 的定义对已完成的包含无效。

```c bad
#include <xrt.h>          /* 先包含：此刻闭包已按"无选择"展开 */
#define XRT_MODULE_STRING /* 太晚了——这个翻译单元不会启用 string */
```

```c good
#define XRT_MODULE_STRING /* 先声明 */
#define XRT_IMPLEMENTATION
#include <xrt.h>
```

### 坑 2：两个翻译单元各选各的模块

症状：运行期诡异行为——某个对象在某些调用路径下正常、另一些路径崩溃；或链接期符号缺失/重复。

原因：单元 A 选了 `XRT_MODULE_NET_TCP`、单元 B 只选 core——B 编译时看到的结构与 A 布局不一致（条件声明差异），同一 XRT 实例内部出现了两种世界观。

```c bad
/* a.c */                /* b.c */
#define XRT_MODULE_ALL   #define XRT_MODULE_STRING
#include <xrt.h>         #include <xrt.h>
/* 两个单元模块集合不一致：链接到同一实例，布局分歧 */
```

```c good
/* common.h —— 全工程唯一的模块声明点 */
#define XRT_MODULE_ALL
/* common.c: #define XRT_IMPLEMENTATION 后 include（唯一实现单元） */
/* a.c / b.c: 直接 include common.h，集合恒一致 */
```

## 练习

### 基础：探测三连

对"无模块宏"、"XRT_MODULE_STRING"、"XRT_MODULE_ALL"三档分别编译 probe.c，记录 `tls` 特性的探测结果并用一句话解释差异来源。

### 进阶：裁剪矩阵

给 hello.c 做五档编译（无模块 / string / regex / string+regex / all），记录每档是否需要 `-lws2_32` 与二进制大小，画成对照表。提示：`-D` 注入模块宏，输出文件用 `-o` 区分。

### 挑战：为小工具选型

一个只需"读 JSON 配置 + 打日志"的 CLI 工具：选出最小模块集，验证编译链接通过、功能正常，并记录最终二进制大小与 `XRT_MODULE_ALL` 档的比值。验收标准：模块集有依据（每个宏一句话理由）、比值低于四分之一、功能验证输出正常。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 正向选择 | 只声明根模块（`XRT_MODULE_` 系列）；依赖闭包自动展开，交付可预测 |
| 宏时序 | 全部宏先于首次包含 `xrt.h` / `features.h`，晚定义一律无效 |
| 实现 | `XRT_IMPLEMENTATION` 全工程一次，放在唯一的实现翻译单元里 |
| 排除 | `XRT_EXCLUDE_MEMORY_DEBUG` 等只抑制 `XRT_MODULE_ALL` 的隐式全量选择 |
| 探测 | 闭包定义 `XRT_FEATURE_` 系列；编译期分流零运行时开销 |
| 版本 | 编译期宏可 `#if`；链接库集成启动先核对 `xrtVersion()` 再干活 |
| 交付 | 单头（`single/`）与源码/库（modules.json）同 API；原型单头、工程切库 |
| 三件套 | `-D` 选交付、`XRT_FEATURE_` 探能力、`xrtVersion()` 核版本——同单元闭环 |
