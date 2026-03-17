# XRT

<div align="center">

[![License: MIT](https://img.shields.io/badge/license-MIT-yellow.svg)](LICENSE)
[![Language: C](https://img.shields.io/badge/language-C-brightgreen.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Platforms](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue.svg)](#)

**力求卓越的 互联网 + AI 时代跨平台 C 语言基础设施库**

[English](README.en.md) | 简体中文

</div>


## 项目定位

**XRT 不是一个零散函数集合，也不是“拼几个第三方库”的工具箱。**

XRT 的目标，是成为一套真正成体系的 C 语言基础设施：

- 既能作为通用应用程序的基础运行时
- 也能支撑高并发网络服务
- 还能支撑 LLM、AI Agent、流式处理、结构化数据交换这类现代工作负载
- 并且从一开始就面向 **Windows / Linux / macOS** 等多平台环境设计

它希望让 C 语言在以下场景中，获得更接近现代语言的开发体验：

- 通用程序开发
- 跨平台程序开发
- 多线程与协程并发开发
- 高性能网络客户端与服务端开发
- HTTP / WebSocket / TLS / JSON / 模板等完整应用栈开发
- AI Agent、LLM API 调用、流式结果处理与工具编排


## XRT 具备哪些功能

XRT 当前已经形成一套完整主线，而不是若干互不相干的模块。  
如果把它看作一张能力地图，大致可以展开成下面这些层次：

### 1. 基础运行时

这一层负责把 C 项目最常见、最琐碎、最容易出现平台差异的工作统一收口：

- `base`
	- 基础内存分配
	- 错误状态
	- 线程级临时内存
	- runtime 初始化与清理
- `os`
	- 进程启动
	- 打开文件/目录/URL
	- 平台相关系统调用封装
- `string`
	- 查找、替换、分割、拼接、格式化
	- 大小写处理
	- 常用文本变换
- `charset`
	- UTF-8 / UTF-16 / UTF-32 转换
	- BOM 识别
	- 字符集转换基础能力
- `time`
	- 当前时间
	- 日期序列化
	- 时间计算与格式处理
- `file / path`
	- 文件读写
	- 目录扫描
	- 路径拼接、拆分、规范化
- `hash / math / xid`
	- 32/64 位哈希
	- 随机数
	- 分布式唯一 ID

这一层的价值不只是“函数多”，而是让上层模块不必自己重复处理：

- 平台差异
- 编码差异
- 路径差异
- 时间和字符串的基础细节


### 2. 内存管理

XRT 并不满足于直接把 `malloc/free` 包一层，而是提供了多种面向不同场景的内存能力：

- `bsmm`
	- 面向固定结构体的小块分配
- `memunit`
	- 面向小对象页式管理
- `mempool_fs`
	- 面向固定大小对象的快速内存池
- `mempool`
	- 面向通用对象的多级内存池
- runtime 线程态临时内存
	- 解决大量“短期结果字符串/短期缓冲”问题
- 内存跟踪与调试基础
	- 为后续排查泄露、生命周期问题提供基础

除了分配性能本身，XRT 还把 **内存调试** 视为基础设施的一部分。  
它的目标不是只在程序崩溃后“猜哪里有问题”，而是尽量把问题定位到具体代码。

当前这条线重点覆盖：

- **内存泄露定位**
	- 能记录分配来源
	- 能在调试输出中定位到代码文件与行号
	- 便于追踪哪一条分配路径没有被正确释放
- **危险操作识别**
	- 重复释放
	- 非法释放
	- 越界写导致的块破坏
	- 已释放内存继续使用
	- 未初始化/错误生命周期对象进入池结构
	- 错线程访问本线程本地对象
- **对象生命周期排查**
	- 哪个对象还被持有
	- 哪个对象已经进入释放态却仍被访问
	- 哪类容器/池对象在共享与本地语义上被错误使用
- **线程相关内存问题辅助定位**
	- 线程级临时内存
	- 线程级错误槽
	- 线程绑定内存上下文
	- owner-thread / shared-mode 相关误用诊断

这使得 XRT 的内存体系不是单纯“更快地分配”，而是同时在追求：

- 更快的分配性能
- 更清晰的生命周期
- 更强的问题定位能力
- 更可靠的危险操作暴露能力

这使得 XRT 在内存层不是“只有一个池”，而是已经形成：

- 通用分配
- 固定块分配
- 小对象优化
- 线程态短期内存
- 内存调试与危险操作检测

这几条不同用途的主线。


### 3. 数据结构

XRT 已经内建了相当丰富的通用数据结构，而不是把程序员丢回原始数组和链表：

- `buffer`
	- 动态缓冲区
- `array`
	- 连续结构体数组
- `ptrarray`
	- 指针数组
- `stack / dynstack`
	- 固定栈与动态栈
- `llist`
	- 双向链表
- `avltree`
	- 平衡二叉树
- `dict`
	- 字符串键字典
- `list`
	- 整数索引列表

而且这些数据结构不是孤立存在，它们已经和：

- 内存模型
- shared/local 语义
- `xvalue`
- 并发重构后的对象所有权约定

逐步建立联系。


### 4. 并发基础设施

XRT 当前已经不只是“有线程 API”，而是已经形成比较完整的并发底座：

- `thread`
	- 线程创建
	- attach / detach
	- 线程级 runtime 状态
	- 线程级错误信息
	- 线程级临时内存
- `coroutine`
	- 协程创建、恢复、让出、退出
	- 协程调度器
	- sleep / deadline / event wait
	- cleanup 栈
	- join / cancel / result

这意味着 XRT 在并发层已经能同时支撑：

- 多线程服务
- 单线程多协程
- deadline 驱动的异步逻辑
- 线程和协程混合编排


### 5. 现代异步运行时

这是 XRT 最近重构后最重要的一条主线之一，也是它和很多传统 C 库差异最大的地方。

当前已经正式具备：

- `future`
	- 异步结果对象
	- 状态、结果、错误、等待
- `promise`
	- 异步结果生产端
- `task`
	- 统一任务执行抽象
- `wait-source`
	- 统一等待源抽象
- continuation
	- `Then / Catch / Finally`
	- `Current / Engine / Coroutine Scheduler`
- combinator
	- `WhenAny / WhenAll / Race`
- structured concurrency
	- `task group`
	- `group join`
	- `close / reap / destroy`
	- `nested scope`
	- `parent cancel propagation`

这条主线的意义是：

XRT 已经不仅仅有线程、协程、网络，而是开始拥有一套统一的异步运行时语义。


### 6. 网络基础设施

XRT 的网络层不是“简单 socket 封装”，而是已经形成现代网络 runtime 主线：

- `xnet-v2`
	- engine
	- stream
	- dgram
	- listener
	- sync
	- TLS session
- `future / wait-source` 与网络层打通
- stream / dgram / listener 的：
	- 同步等待
	- 协程等待
	- timeout / deadline 等待
	- future bridge

也就是说，网络层已经不仅能“收发数据”，还在统一：

- I/O 等待
- 任务投递
- 取消
- deadline
- future 化结果


### 7. 网络应用层

在网络传输层之上，XRT 已经具备了直接开发应用协议所需的关键能力：

- `http`
	- HTTP 客户端主线
- `httpd`
	- HTTP 服务端主线
- `websocket`
	- WebSocket 客户端与服务端基础能力
- `tls`
	- 内建 TLS session 主线
- `url / http util`
	- URL、Header、Query、表单等基础工具能力

这意味着 XRT 不是“只给 socket，再让你自己重写 HTTP/TLS”，而是已经能直接支撑：

- API 客户端
- HTTPS 服务
- WebSocket 服务
- LLM API 调用
- 流式网络应用


### 8. 结构化数据处理

这也是 XRT 很重要的一条主线，因为现代程序尤其是 AI 应用高度依赖结构化数据。

- `xvalue`
	- 动态值系统
	- array / list / coll / table 等容器型值
	- 更接近脚本语言的数据操作体验
- `json`
	- JSON 解析与生成
	- SAX 模式
	- 结构化交换数据处理

这让 XRT 可以比较自然地处理：

- 配置
- 协议数据
- API 请求与响应
- AI 结构化输出
- 动态模板数据


### 9. 文本、模板与正则

在很多真实业务里，“文本处理”不是边角料，而是非常核心的工作。

XRT 当前已经具备：

- `template`
	- 模板渲染
	- 文本生成
	- HTML/文本输出基础能力
- `regex`
	- 正则匹配
	- 提取
	- 替换与模式识别
- 配合 `string / charset / json / xvalue`
	- 可以形成完整文本处理链路

这一层对于：

- Web 输出
- 配置生成
- 结果格式化
- Agent 文本处理

都很重要。


### 10. 跨平台工程能力

除了功能数量本身，XRT 还有一个经常被低估但非常关键的能力：

它在尽量把这些能力做成 **同一套跨平台主线**。

也就是说，上面这些模块不是只在某一个平台上便利，而是都围绕：

- Windows
- Linux
- macOS
- `gcc / tcc` 等实际开发编译链

去设计和收口。


如果把它压缩成一句话：

**XRT 现在已经不是“有一些基础函数”，而是具备了从底层运行时、内存、容器、并发、异步、网络，到上层 HTTP / TLS / WebSocket / JSON / 模板 / 正则的一整套基础设施能力。**

这不是“能做很多事”的堆砌，而是已经初步形成：

**运行时 + 内存 + 数据结构 + 并发 + 异步 + 网络 + 应用协议 + 数据表达 + 文本处理**

这一整套基础设施闭环。


## 为什么是“互联网 + AI 时代”的库

因为 XRT 解决的问题，不是传统 C 工程里那种“只把 socket 跑起来”。

它面向的是现代程序真正高频出现的组合需求：

- 同一套代码需要稳定运行在 Windows 和 Linux，并尽量保持一致行为
- 一个线程里跑多个协程，做并发等待与 deadline 控制
- 通过统一 `future / task / wait-source` 把线程、协程、网络等待连接起来
- 用 HTTP / TLS / WebSocket 与外部 API 通信
- 用 `xvalue / json` 处理复杂结构化数据
- 用模板与正则生成和提取文本
- 用统一 runtime 支撑 AI Agent 的工具调用、并发请求、流式等待与结果聚合

如果只看单个模块，这些能力并不新鲜；  
但把它们在 **一套轻量、统一、零外部依赖主线** 中打通，就是 XRT 的价值。


## 核心特征

### 1. 高性能

XRT 不是以“功能全但速度慢”为代价换能力。

已经有实测数据证明，XRT 的现代网络主线正在逼近“卓越”这个目标。

来自当前基准文档：

- [XNET Compare Baseline (2026-03-14)](/D:/git/xrt/dev/bench/XNET_COMPARE_20260314.md)
- [Builtin TLS Validation and Benchmark Report (2026-03-15)](/D:/git/xrt/dev/bench/TLS_BENCH_20260315.md)

其中最有代表性的结果包括：

- **Windows TCP echo 总吞吐**
	- `xnet-v1`: `91.7 msg/s`
	- `xnet-v2`: `83178.1 msg/s`
- **Windows TLS echo 总吞吐**
	- `xnet-v1`: `99.9 msg/s`
	- `xnet-v2`: `59343.7 msg/s`
- **Windows UDP burst send**
	- `xnet-v1`: `322825.4 pps`
	- `xnet-v2`: `1718877.6 pps`
- **Debian 13 TCP echo 总吞吐**
	- `xnet-v1`: `36628.1 msg/s`
	- `xnet-v2`: `60240.8 msg/s`
- **Debian 13 UDP burst send**
	- `xnet-v1`: `340122.5 pps`
	- `xnet-v2`: `1355287.2 pps`

TLS 专项基准中：

- **Warm full handshake 平均耗时**
	- `276510.100 us`
- **TLS 1.2 resumed handshake 平均耗时**
	- `34466.544 us`
- **Resume 路径 connect-to-open 延迟**
	- 约 **8.0x 更低**

这些数字并不意味着 XRT 已经在所有平台、所有场景都达到最终最优，但它们至少说明：

- XRT 有明确的性能目标
- XRT 已经建立了持续 benchmark 机制
- XRT 的现代网络主线不是概念实现，而是真正被测量、被比较、被优化的系统


### 2. 轻量化

XRT 的目标不是做成一个巨大的框架，而是做成一套：

- 单头文件可分发
- 零外部运行时依赖
- 可裁剪
- 可嵌入
- 可静态编译
- 可作为 DLL / 组件 / 运行时基础层使用

当前源码树里已经形成：

- `xrt.h` 统一公开 API
- `xrt.c` 统一集成实现
- `singlehead/xrt.h` 单头文件分发形态

它不是把很多外部库原样塞进来，而是把真正需要长期沉淀的能力整合进同一设计体系中。


### 3. 跨平台

XRT 从设计上就不是“某一个平台上的便利脚本库”，而是要成为真正可迁移的基础设施。

它关注的不是简单的“能编过”，而是：

- 尽量统一 Windows / Linux / macOS 的调用体验
- 把线程、时间、文件、路径、网络、字符集等高频差异收敛到运行时内部
- 让上层业务逻辑尽可能摆脱平台分支
- 保持 `gcc / tcc` 等实际开发链路可用，方便快速验证与正式构建

这对 XRT 非常重要，因为它服务的不是单机玩具程序，而是：

- 真正要部署的网络服务
- 需要双平台交付的工具程序
- 需要长期维护的基础设施项目
- 需要接入不同运行环境的 AI / Agent 系统

XRT 的价值之一，就是把“跨平台兼容”从业务代码里剥离出来，收口到基础设施层。


### 4. 功能强大、完备且成体系

很多 C 库看起来功能很多，但本质上只是：

- 某个容器库
- 某个 JSON 库
- 某个网络库
- 某个随机数库
- 某个模板库

把它们堆在一起，不等于形成基础设施。

XRT 更强调的是：

- 模块之间的模型一致性
- 生命周期语义一致
- 线程 / 协程 / future / wait-source 可以互相协同
- 网络层和应用层能直接接到 runtime
- 数据结构、内存管理、结构化数据处理不是孤立模块

也就是说，XRT 追求的是：

**功能强大，不靠拼凑；模块完整，不靠叠加；能力成体系，不靠运气。**


## 相比常见 C 库，XRT 的优势是什么

### 不是只解决一个点，而是解决“整条链路”

和只提供单项能力的库相比，XRT 更像一套可直接支撑项目的底座：

| 方向 | 常见单项库 | XRT 的方式 |
|---|---|---|
| 基础运行时 | 只补某类 helper | 提供基础运行时主线 |
| 跨平台 | 业务代码中充满 `#ifdef` | 尽量把平台差异收口在运行时与系统抽象层 |
| 数据结构 | 单独容器库 | 容器与内存模型、shared/local 语义协同 |
| 并发 | 只有线程或只有协程 | 同时提供线程、协程、future、task、wait-source |
| 网络 | 只提供 socket 封装 | 提供传输层、同步等待、异步等待、TLS、HTTP、WS |
| JSON | 只做 parse / print | 与 `xvalue`、模板、网络层形成闭环 |
| AI 场景 | 需要手工拼装等待与结果模型 | 已具备统一异步运行时基础 |


### 不是“用起来轻”，而是“扩展时也不乱”

很多库在简单示例里很好用，但项目一旦复杂，就会出现：

- 生命周期混乱
- 错误模型混乱
- 同步异步两套接口完全分裂
- 线程和网络之间没有统一抽象

XRT 最近一轮重构的核心方向，就是在提前解决这些问题：

- runtime 全局态 / 线程态拆分
- 协程 runtime 线程绑定
- `future / task / promise / wait-source` 正式化
- `task group / nested scope / parent cancel propagation`
- `xnet` 与 coroutine / future 的统一等待模型

这正是它和“功能很多的库”之间真正的差异。


## 当前规模与成熟度信号

按当前源码树统计，XRT 已经具备：

- `50+` 个实现头文件模块
- `5694` 行公共头文件声明（`xrt.h`）
- `93` 个文档文件
- `59` 个测试文件

同时，项目已经建立了这些工程化资产：

- 主测试流
- 独立专项测试
- 竞态 / 生命周期 / ownership 回归
- bench 基线与专项基准文档
- 单头文件生成链

这并不意味着“所有部分都已经最终定稿”，但足以证明：

**XRT 已经越过了 demo / 原型期，正在向真正的基础设施工程靠拢。**


## 典型适用场景

- 通用 C 应用程序基础运行时
- Windows / Linux 双平台项目底座
- 面向多平台交付的命令行工具、服务端程序与嵌入式运行时组件
- 高并发网络服务
- TLS / HTTP / WebSocket 客户端与服务端
- JSON / 动态值 / 模板驱动的数据应用
- AI Agent 的工具调用、并发请求、deadline 控制、结果聚合
- LLM API 调用、结构化返回处理、流式交互基础层


## 设计原则

XRT 当前已经明确遵循这些方向：

- **高性能**
	- 真正做 benchmark，而不是口头优化
- **轻量化**
	- 不轻易引入外部依赖
- **单主线**
	- 尽量清理旧表面、旧命名、旧包装壳
- **统一模型**
	- 线程、协程、future、网络等待尽量统一
- **跨平台优先**
	- 优先把平台差异消化在基础层，而不是把复杂性甩给业务代码
- **可裁剪**
	- 保持作为基础设施库的灵活性
- **不靠历史包袱维持兼容**
	- 在项目仍处于可重构阶段时，优先做正确的架构决策


## 当前状态说明

XRT 已经具备大量正式能力，但它追求的不是“能用就算完成”，而是“足够强再冻结”。

因此当前更准确的描述是：

- 它已经拥有完整的基础设施蓝图
- 多个主线模块已经进入正式重构后的新架构
- 很多核心能力已经接近生产级，部分已经可以按生产级思路使用
- 但项目仍会继续清理历史表面、统一主线、补齐平台验证与文档体系

这也是 README 强调“力求卓越”的原因：  
XRT 的目标不是做一个“差不多能用”的库，而是把 C 语言基础设施做得更现代、更系统、更可靠。


## 快速开始

最简单的使用方式：

```c
#include "xrt.h"

int main()
{
	xrtInit();

	printf("xrt version: %s\n", XRT_VERSION);
	printf("rand32: %u\n", xrtRand32());

	xrtUnit();
	return 0;
}
```

Windows 下常用：

```bat
gcc -m64 main.c xrt.c -o app.exe -lws2_32 -liphlpapi
```

或使用 TCC 做快速验证：

```bat
tcc -m64 main.c xrt.c -o app.exe -lws2_32 -liphlpapi
```


## 文档

- 总文档入口：[docs/README.md](/D:/git/xrt/docs/README.md)
- 下一阶段蓝图：[dev/XRT_ROADMAP_NEXT.md](/D:/git/xrt/dev/XRT_ROADMAP_NEXT.md)
- 网络基准：[dev/bench/XNET_COMPARE_20260314.md](/D:/git/xrt/dev/bench/XNET_COMPARE_20260314.md)
- TLS 专项验证：[dev/bench/TLS_BENCH_20260315.md](/D:/git/xrt/dev/bench/TLS_BENCH_20260315.md)


## 致谢

XRT 坚持“主线统一、能自己掌控的能力尽量自己掌控”，但这并不意味着闭门造车。  
项目中吸收、移植、参考或受启发于以下优秀开源项目：

| 项目 | 链接 | 在 XRT 中的作用 | 合规说明 |
|---|---|---|---|
| LJSON | [gitee.com/lengjingzju/json](https://gitee.com/lengjingzju/json) | `json` xrt 采用了 JNUM 和 SAX API | 保留版权与许可声明 |
| bbre | [github.com/max-nurzia/bbre](https://github.com/max-nurzia/bbre) | xrt 采用其作为 `regex` 模块的实现 | 保留版权与免责声明 |
| nmhash32x | [github.com/gzm55/hash-garage](https://github.com/gzm55/hash-garage) | xrt 采用其作为 32 位哈希实现 | 保留版权与免责声明 |
| rapidhash | [github.com/Nicoshev/rapidhash](https://github.com/Nicoshev/rapidhash) | xrt 采用其作为 64 位哈希实现 | 保留版权与免责声明 |
| PCG | [github.com/imneme/pcg-c-basic](https://github.com/imneme/pcg-c-basic) | xrt 采用其作为随机数生成器 | 保留版权与免责声明 |
| mongoose | [github.com/cesanta/mongoose](https://github.com/cesanta/mongoose) | 启发了 xrt 的 http 和 TLS 相关实现 | 未使用 mongoose 库的代码 |


## 许可证

XRT 主项目采用 [MIT License](LICENSE)。

但 XRT 内部吸收的第三方实现，仍然分别遵守其各自许可证要求。  
如果你再分发 XRT，除了遵守 XRT 主许可证外，也应同步保留这些第三方组件要求保留的版权声明、免责声明与附加 notice。


## 最后

如果你正在寻找的不是一个“只会做一件事”的小库，  
而是一套能够支撑：

- 基础运行时
- 高并发
- 现代网络
- 结构化数据
- AI / Agent 工作负载

的 **C 语言基础设施主线**，

那么 XRT 正在朝这个方向持续推进。

它不是在追求“功能看起来很多”，  
而是在追求：

**高性能、轻量化、功能强大、完备且成体系，并且力求卓越。**
