---
num: 2
slug: intro
title: 认识 XRT 与开发环境
volume: 卷一 起步与核心
type: practice
lead: 内核与扩展库的双层结构、仓库导览、单头集成与第一个程序——把实验环境跑通。
api: core, string
---

## 导读

XRT 不是"又一个工具函数库"，而是一套成体系的跨平台 C 基础设施：从内存、错误、容器、协程，到五后端网络引擎、自研 TLS、HTTP/WebSocket 协议核心。学 XRT 的第一课不是背 API，而是理解它的"内核 + 扩展库"双层结构——这决定了你写的每一行代码站在哪一层上。本章先建立全局图景，然后完成从零到可编译的全部准备：获取源码、认识目录、理解两种集成方式、编译第一个程序、看一眼特性裁剪的威力。读完本章，你就拥有了一个能自由实验的开发环境。

## 引入

假设你要用 C 写一个抓取配置文件并转发数据的小服务。按散装拼库的路线，你需要：一个 JSON 解析库、一个 HTTP 客户端、一个 TLS 库、一个线程池、若干容器……它们来自不同的作者，错误处理风格各异（返回码、errno、setjmp、abort），内存约定各异（谁分配谁释放、引用计数、GC 宏），构建体系各异（CMake、Meson、手工 Makefile）。你花在"缝合"上的时间很快超过业务本身，而且任何一个上游库的安全更新都要你重新评估整条依赖链。

XRT 的答案是把这些能力做成一个**单一技术栈**：统一的错误模型（第 4 章）、统一的内存与所有权约定（第 5 章）、统一的视图数据约定（第 3 章），从底向上贯通到网络与协议层。理解这套统一性，比记住任何一个 API 都重要——这正是本章要建立的全景。

还有一条隐性成本值得点破：散装依赖的**升级路径**是各自为政的——TLS 库爆出漏洞要单独评估，JSON 库换许可证要单独迁移，每个库的测试覆盖参差不一。单一技术栈把这些问题收敛成一次决策：升级一个版本号，全栈的安全补丁与回归测试（仓库自带数千个测试源文件）一起到位。第 8 章会讲版本与兼容的约定，卷十二讲怎么在工程里管理这条升级路径。

## 概念

### 内核与扩展库：严格的双层结构

- **内核**（`src/` + `include/xrt/`，91 个公开头文件、数百个可裁剪模块）：地基（core / error / atomic / memory）、容器、文本与数据、日志与文件进程、协程与异步、五后端网络引擎（IOCP / epoll / kqueue / io_uring / select）、自研 TLS 1.2/1.3 与 X.509、HTTP/1.x 与 WebSocket **协议核心**。
- **扩展库**（`extlibs/`，五个独立产品）：`xhttp`（HTTP 客户端/服务端运行时）、`xws`（WebSocket 运行时）、`xruntime`（动态类型与对象系统）、`xmail`（邮件协议全家桶）、`xssh`（SSH2 协议栈）。

依赖是**严格单向**的：扩展库只调用内核，内核对扩展库零引用。本书卷一到卷九讲内核，卷十、卷十一讲扩展库。这条单向性对使用者的直接好处是"内核能力可以单独成立"——只用内核写一个配置解析小工具，完全不感知扩展库的存在；而扩展库的任何功能，追到最底层都是你已经学过的内核原语。学习顺序因此也自然成立：读完卷一到卷九，扩展库的文档你基本可以直接读。

一个容易混淆的点：内核保留了 HTTP 和 WebSocket 的**协议核心**（解析、封包、语义验证），但"客户端""服务端""路由""连接池"这些高层运行时在 xhttp / xws 扩展库里。看到 `xrtHttp1RequestParse` 是内核；看到 xhttp 的客户端 API 就是扩展库——前缀风格不同，所属产品也不同。

### 仓库结构导览

获取源码后，日常开发最常接触的目录只有五个：

| 目录 | 内容 | 本书视角 |
| --- | --- | --- |
| `include/xrt/` | 内核 91 个公开头文件，导出函数带 `XRT_API` 与中文契约注释 | 全书每一章的"教材原文" |
| `single/` | 单头产物 `xrt.h` / `xrt_decl.h`（自包含全部内核实现） | 本书所有示例的编译依据 |
| `src/` | 内核实现与 `src/internal/` 私有契约头 | 深入细节时的"参考答案" |
| `extlibs/` | 五个扩展库，各自含 src / include / tests / examples / single | 卷十、卷十一的教材 |
| `examples/` | 按模块组织的可运行示例 | 每章配套的"课外读物" |

其余目录：`tools/` 是构建工具链；`tests/` 是测试套件（卷十二专门讲怎么读）；`config/modules.json` 是模块清单——全库的单一事实来源；`dev/` 存放基准与归档。

### 两种集成方式

从源码到可执行程序，单头路径只需四步：

```diagram flow
- 选模块：define XRT_MODULE_ALL 或列出所需模块宏
- 启用实现：define XRT_IMPLEMENTATION 让头文件长出函数体
- 包含单头：include xrt.h（编译时 -I 指向 single/ 目录）
- 编译链接：gcc main.c 即可，不链接任何 XRT 库文件
```

工程化阶段可以切换到第二条路：用构建工具打出静态库或动态库，或把内核源码纳入你的构建系统，头文件路径统一为 `<xrt/xxx.h>`。这条路在卷十二展开。本书示例统一走单头路径——它的好处是"复制一个文件就开始"，而且裁剪行为一目了然。

## 示例

### 第一个完整程序

下面是本书的第一个完整程序。它用 `xrtFormat`（printf 规则格式化，返回 XRT 托管字符串）拼一句话，用完调 `xrtFree` 释放——这两条调用已经用上了内核的统一内存收口：

```c
/* hello.c —— 本书第一个程序 */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>

int main(void)
{
	str s = xrtFormat("Hello, XRT %s!", xrtVersion());
	if ( s == NULL ) {
		return 1;
	}
	printf("%s\n", s);
	xrtFree(s);
	return 0;
}
```

```term
$ gcc -O1 -I single hello.c -lws2_32 -liphlpapi
$ ./a.exe
Hello, XRT 2.0.0-dev!
```

**刚才发生了什么。** 两行宏出现在包含之前：`XRT_MODULE_ALL` 选择启用全部模块（也可以只列需要的，见下节）；`XRT_IMPLEMENTATION` 让这个编译单元"长出"函数体——单头模式下实现与声明在同一个文件里，靠这个宏切换。`xrtFormat` 的返回值类型是 `str`（XRT 托管字符串，第 3 章展开类型别名），**谁取得返回值谁负责 `xrtFree`**——这是全库统一的所有权约定，第 5 章会完整展开。

顺带观察失败路径的写法：`xrtFormat` 分配失败返回 `NULL`，程序检查后直接返回非零退出码。真实工程中这里还应该读取线程错误槽打印原因——那是第 4 章的内容，本章保持程序最小。还有一个细节值得现在养成习惯：`main` 返回 `0` 之外的值表示失败，本书所有示例遵守"零成功、非零失败"的约定，与 XRT API 的 `bool` 返回值方向一致。

编译命令里的 `-lws2_32 -liphlpapi` 是 Windows 平台的网络系统库——即使本程序没写网络代码，XRT 内核的网络模块在启用全模块时引用了它们；Linux/macOS 不需要这两个参数。这条命令在本书会反复出现，第 8 章会解释为什么裁剪后链接依赖也会变小。

### 裁剪的威力：同一个程序，两种模块集

把第一行宏换成只含所需的模块，其余一字不改：

```c
/* hello-min.c —— 只启用 string 模块及其依赖闭包 */
#define XRT_MODULE_STRING
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>

int main(void)
{
	str s = xrtFormat("Hello, XRT %s!", xrtVersion());
	if ( s == NULL ) {
		return 1;
	}
	printf("%s\n", s);
	xrtFree(s);
	return 0;
}
```

```term
$ gcc -O1 -I single hello.c -lws2_32 -liphlpapi
$ gcc -O1 -I single hello-min.c -lws2_32 -liphlpapi
$ ls -l a.exe a2.exe
-rwxr-xr-x 1 admin 106496 a.exe      ← XRT_MODULE_ALL
-rwxr-xr-x 1 admin  24064 a2.exe     ← XRT_MODULE_STRING（数值因工具链与版本而异）
```

**刚才发生了什么。** `XRT_MODULE_STRING` 只启用字符串模块；它依赖的地基（core、error 等）由 features.h 里的**依赖闭包**自动展开，不需要你手工罗列——"选一个模块，带上它需要的全部"就是闭包的含义。体积差异随版本与工具链变化，上表数值仅示意；亲手跑一次 `ls -l`，你会对"按需交付"有直观感受。顺带注意第二个收益：裁剪不只是省体积——链接的系统库、初始化的模块表、甚至攻击面都跟着缩小，嵌入到别的小工具里时这三点同样值钱。模块宏的完整机制在第 8 章展开。

## 契约

- **单向依赖**：扩展库只调内核；内核代码不会出现对 xhttp/xws 的引用。
- **单头宏约定**：`XRT_IMPLEMENTATION` 在整个工程里只能出现在**一个**编译单元；模块宏必须定义在包含 `<xrt.h>` 之前。
- **依赖闭包**：启用一个 `XRT_MODULE_` 系列宏会自动带上其全部依赖；手工罗列容易漏，交给闭包。
- **版本口径**：编译期宏与 `xrtVersion()` 运行期函数同源（第 3 章演示），链接库集成时用来核对头与库是否同版。
- **示例口径**：本书示例以仓库根目录为工作目录、Windows + GCC 输出为准；Linux/macOS 去掉两个链接参数即可，行为一致处不再注明。

## 避坑

### 坑 1：把 XRT_IMPLEMENTATION 放进头文件或多处定义

症状：链接器报大量"符号重复定义"（multiple definition，重复的是 XRT 实现符号）。

原因：`XRT_IMPLEMENTATION` 让包含它的编译单元生成函数体；两个单元都定义 = 两份实现。

```c bad
/* xrt_util.h —— 自己的公共头 */
#define XRT_IMPLEMENTATION     /* 被多个 .c 包含 → 多份实现 */
#include <xrt.h>
```

```c good
/* xrt_util.c —— 唯一的实现单元 */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
/* xrt_util.h 里只放 #include <xrt.h>（不带两个宏） */
```

### 坑 2：凭记忆写模块宏

症状：编译报"隐式声明"，或编译通过但链接报未定义符号——明明包含了头文件却找不到函数。

原因：模块宏选错了集合——启用的模块不覆盖用到的 API。宏名凭记忆写最容易出这种错，尤其内核的地基部分（core/error）永在、没有单独模块宏，不存在"再补一个地基宏"的写法。

```c bad
#define XRT_MODULE_JSON        /* 只启用了 JSON——它不依赖字符串格式化 */
#define XRT_IMPLEMENTATION
#include <xrt.h>
/* 后面调用 xrtFormat → string 未启用，链接失败 */
```

```c good
#define XRT_MODULE_STRING      /* 查 include/xrt/features.h 或第 8 章速查表 */
#define XRT_IMPLEMENTATION
#include <xrt.h>
```

## 练习

### 基础：跑通并修改

编译运行 `hello.c`，把输出改成两句问候（调用两次 `xrtFormat`，注意两次 `xrtFree`）。

### 进阶：最小模块集

`hello.c` 只用了格式化与版本查询。把它裁剪到能通过编译的最小模块集，记录你最终启用的宏与二进制大小。提示：从 `XRT_MODULE_STRING` 开始，看链接器还缺什么。

### 挑战：三档体积对比

分别用 `XRT_MODULE_STRING`、`XRT_MODULE_REGEX`、`XRT_MODULE_ALL` 编译同一份程序，记录三档二进制大小并解释差异来源（正则比字符串多带了什么？全部模块比单个模块大多少倍？）。验收标准：一张三行对照表 + 两句分析；`XRT_MODULE_ALL` 档不得小于单模块档。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 双层结构 | 内核（`include/xrt/`）+ 五扩展库（`extlibs/`）；依赖严格单向 |
| 单头三步 | 模块宏 → `XRT_IMPLEMENTATION` → `#include <xrt.h>`（`-I single`） |
| 实现宏 | 全工程只出现一次，放在唯一的 `.c` 里 |
| 裁剪 | `XRT_MODULE_*` 选模块，依赖闭包自动展开；宏名以 features.h 为准 |
| 托管字符串 | `xrtFormat` 返回 `str`，用完 `xrtFree` |
| 协议边界 | HTTP/WS 协议核心在内核（http、http1、websocket 模块）；运行时在 xhttp/xws |
