---
num: 3
slug: first
title: 第一个程序：类型、视图与版本
volume: 卷一 起步与核心
type: concept
lead: core.h 精讲——类型别名、视图契约、资源边界、进度回调与引用计数原语。
api: core
---

## 导读

第 2 章把环境跑通了，这一章正式进入 `include/xrt/core.h`——整个内核最底层的头文件。它的内容不多，但每一项都被上层模块反复使用：你写的任何 XRT 程序，签名里迟早会出现 `xstrview`；你读的任何解析器 API，迟早会撞上 `xrtresourcelimits`；你碰的任何不可变共享对象，生命周期都靠 `xrtRefRetain` / `xrtRefRelease`。本章按"类型 → 视图 → 边界 → 进度 → 引用计数"的顺序精讲 core.h 的公开内容，并建立两个贯穿全书的世界观：**字符串不是以零结尾的**，**共享对象不是用分配器管的**。学习策略上，这一章不值得逐条背诵——每个小节的内容都会在后续章节高频复现；值得做的是把两个完整示例亲手跑一遍，把"借用与拥有对照表"和"引用计数状态图"记在脑子里，其余细节留给章末速查与 core 参考页随时回查。

## 引入

先看两段每个 C 程序员都写过的代码。第一段：函数要返回一段文本的一部分——你返回 `char*` 还是拷贝一份？返回指针则长度信息丢了，还要求调用方猜它是否零结尾；拷贝则多一次分配，失败路径又多一条。第二段：一个配置对象被三处共享，谁负责释放？手工约定"最后用完的人放"在 C 里没有语言支持，迟早演变成 use-after-free 或泄漏。第三段更隐蔽：解析一个不受信任的输入——嵌套多深算恶意？多大算攻击？每个解析器自己定一套，审计就要做 N 遍。

这三个问题的行业常见答案是：字符串切片类型 + 引用计数 + 统一资源上限。XRT 在 core.h 里给了它们最朴素的 C 形态——`xstrview` 两字段结构体、`xrtRefRetain`/`xrtRefRelease` 两个原子原语、`xrtresourcelimits` 一组三闸——并让全库所有 API 统一消费这三个约定。理解了这一章，后面任何模块的签名你都能读出"谁拥有什么、防线在哪"。

## 概念

### 从 core.h 读出全库风格

core.h 还教给你一套"读任何 XRT 头文件"的方法。导出函数带 `XRT_API` 标记与中文契约注释——注释写的是**契约**（谁拥有、何时有效、失败时输出参数状态），不是复述参数类型；遇到行为问题先读注释再读参考页。头文件的分段即模块的分层：类型在前、常量居中、函数在后，与官网参考页的排列一致。

再留意两处风格细节。一是**失败时输出无副作用**：所有带输出指针的函数在失败路径上不写入输出对象，调用方的清理代码因此可以统一写"失败就不管输出"；二是**无隐藏分配**：需要分配的结果（如 `xrtFormat` 的返回串）都由返回值携带并明确归属，不会"传一个二级指针进去、悄悄替你分配"。这两条贯穿全库，你在后面任何模块的签名上都能验证。

### 类型别名：全库统一的书写

XRT 用一组短别名统一类型书写，全部定义在 core.h：

| 别名 | 等价于 | 说明 |
| --- | --- | --- |
| `int8` … `uint64` | `int8_t` … `uint64_t` | 定宽整数全家，直接使用 `<stdint.h>` |
| `ptr` | `void*` | 通用指针；回调的用户数据参数都是它 |
| `str` / `cstr` | `char*` / `const char*` | 零结尾字符串；`str` 通常表示"你拥有它，用完 `xrtFree`" |
| `bytes` / `cbytes` | `unsigned char*` 及 const 版 | 二进制数据指针 |
| `xtime` | `int64` | Unix Epoch 微秒绝对时间（第 34 章展开） |
| `xseek` | 枚举 | `XSEEK_START` / `XSEEK_CURRENT` / `XSEEK_END`，文件与通用 IO 共用的移动基准 |

两个容易被老 C 手忽视的细节。其一，`ptr` 而非 `void*` 直书出现在所有回调签名里——用户数据参数统一是它，配合强制转换往返；其二，`xtime` 用**微秒**而非秒或毫秒作单位，且是绝对时间戳——这与系统调用的精度对齐，避免了到处乘除 1000 的口径混乱。时间体系（时钟、时区、休眠）在第 34 章展开，本章只需要记住"看到 `xtime`，想微秒"。

配套两个"不存在"：没有"布尔三态"，真值就是 C 的 `bool`；没有字符串类型层级，文本要么是零结尾 `str`，要么是带长度的视图（马上讲到）。

### 视图：xstrview 与 xbytesview

这是 XRT 最重要的数据约定。两个结构体形态相同：

```c
typedef struct xstrview { cstr Data; size_t Size; } xstrview;
typedef struct xbytesview { cbytes Data; size_t Size; } xbytesview;
```

头文件注释一句话说清契约：**只借用内存，不拥有数据，也不要求末尾补零**。"能含零字节"这条推论值得单独展开：网络协议与二进制格式里，长度前置、内容任意的字段是常态——HTTP 头里的字节串、TLS 握手里的证书载荷都不保证没有零字节。用零结尾字符串承载它们，要么截断要么转义，两头都是坑；用视图承载，长度就是长度，内容就是内容。你在卷七、卷八会看到所有协议 API 的输入输出都以视图为单位，原因就在这里。三个推论贯穿全书：

- **零结尾不可用**——不能把 `Data` 直接丢给 `printf` 的 `%s`，必须配合 `Size` 用 `%.*s`；
- **生命周期跟着源走**——视图不延长底层内存的寿命，源释放后视图即悬空；
- **能含零字节**——二进制安全，解析器 API 的输入输出几乎全是它。

从 C 字符串字面量构造视图用 `XRT_STR_LITERAL("...")` 宏（`sizeof(s)-1` 自动去掉零结尾）；运行期从缓冲构造直接填两个字段，`XRT_STR_INIT` 用于静态初始化场景。"借用与拥有"的对比值得用一张表钉死：

| 形态 | 拥有者 | 释放责任 | 典型来源 |
| --- | --- | --- | --- |
| `xstrview` / `xbytesview` | 无人拥有 | 无须释放 | 字面量、缓冲的切片、API 借出 |
| `str` / `bytes` | 调用方 | `xrtFree` 恰好一次 | `xrtFormat` 等创建型 API |
| 库内托管对象 | 库与调用方按引用计数 | 归零者析构 | 第 4 章 `xerror`、卷八证书 |

`XRT_NPOS` 是"不存在的位置/长度"哨兵值（`(size_t)-1`），查找类 API 未命中时返回它。视图的生命周期纪律：

```diagram flow
- 源存在：字面量、缓冲、托管字符串都是合法的视图来源
- 借用期间：源必须保持存活且内容不被修改
- 消费即弃：视图本身无须释放，也不应该被存储到比自己源活得久的地方
```

### 资源边界：不受信任输入的公共防线

`xrtresourcelimits` 是所有解析器共用的防 DoS 语言，三道闸：最大嵌套深度 `iMaxDepth`、最大条目数 `iMaxEntries`、最大输入总字节 `iMaxInputBytes`。`xrtResourceLimitsInit` 填入默认值，按需收紧后传给 JSON/XSON/HTTP 等解析 API。一个"深度 128、十万条目"的默认值意味着：恶意构造的万层嵌套文本会在第 128 层被拒之门外，而不是把栈吃穿。

怎么定自己的数值？三问就够：**合法输入最深嵌套多少**（配置文件通常个位数，协议嵌套看规范）、**最多多少条目**（按业务峰值乘一个余量）、**一次请求的输入上限多大**（按内存预算倒推）。把三个答案填进结构体，边界就成了业务规格的代码化——评审时它就是你的防线说明书，而不是散落在各处的 `if` 判断。这也是"公共语言"的另一层含义：同一个结构体从 JSON 换到 XSON、从文件换到网络流，防线语义不变，审计只做一次。

### 进度回调与引用计数

长任务可接受一个进度回调。`xrtprogress` 描述当前进度（已处理字节、当前阶段等字段），`xrtprogressproc` 是回调签名——接受进度结构与用户数据，返回 `false` 即请求中止；`xrtProgressReport` 是库侧的上报入口：实现方在循环的关键位置调用它，把回调判空、参数组装这些样板一次做完。回调里不要做重活——它运行在任务的处理路径上，返回得越快，中止响应越及时。卷四解析大文件、卷十二讲性能时会实际用到它。

引用计数是共享对象的生命周期语言。XRT 把它做成了两个原子原语而非"智能指针"：

```diagram state
计数=1 创建者持有 -> 计数>1 多方共享: xrtRefRetain（新持有者接管前）
计数>1 多方共享 -> 计数=1 创建者持有: xrtRefRelease（放弃一份）
计数=1 创建者持有 -> 析构并释放: 最后一次 xrtRefRelease 归零
计数>1 多方共享 -> 拒绝继续增加: Retain 返回 -1（溢出保护）
```

规则：计数字段必须是 `volatile int32` 且内嵌为对象第一个字段；创建函数返回时计数为 1（创建者那一票）；`xrtRefRelease` 返回 0 的调用方负责析构。全库的不可变共享对象（正则、模板、X.509 证书、TLS 会话……）都建立在这对原语之上。

## 示例

### 完整程序：版本与资源边界

下面的程序来自仓库范例 `examples/core/version_limits/main.c`，一次性演示编译期与运行期的版本口径，以及资源边界的默认值：

```embed path="examples/core/version_limits/main.c" title="examples/core/version_limits/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/core/version_limits/main.c -lws2_32 -liphlpapi
version=2.0.0-dev
limits: depth=128 entries=100000 input=268435456
```

**刚才发生了什么。** `xrtVersion()` 返回运行期版本串，与编译期宏 `XRT_VERSION_MAJOR/MINOR/PATCH` 拼出的结果一致——单头场景二者同源；链接库集成时若不一致，说明头与库版本错配。资源边界的默认值（深度 128、十万条目、256MB 输入）就是上一节三道闸的实数。三条闸门的语义在所有解析器上一致：超限即失败并留下结构化错误（第 4 章），输入的剩余部分不会被继续消费——"要么完整解析、要么带着明确原因失败"，没有中间态。这也意味着防御性代码可以写得非常简单：传入边界，检查返回值，读错误槽，三步结束。

### 完整程序：引用计数管理共享对象

第二个程序来自 `examples/core/reference/main.c`，用 `xrtMalloc` 创建内嵌计数的对象，走完"创建 → 共享 → 归零析构"的完整生命周期：

```embed path="examples/core/reference/main.c" title="examples/core/reference/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/core/reference/main.c -lws2_32 -liphlpapi
value: 42
```

**刚才发生了什么。** 对象把 `volatile int32 RefCount` 内嵌为第一个字段——这是 `xrtRefRetain`/`xrtRefRelease` 的硬性布局要求，它们以指针操作这个字段。创建即持有（计数 1）；第二个持有者接管前先 `xrtRefRetain`；每个持有者放弃时调 `xrtRefRelease`，**返回 0 的那次负责析构**（本例即 `xrtFree`）。`volatile` 不是装饰——计数会被多线程并发增减，去掉它编译器可能把读写缓存进寄存器，计数就再也不可靠了。

两个设计选择值得咀嚼。其一，原语返回**新计数值**而非布尔：调用方可以直接检查溢出保护（Retain 返回 -1）与归零时机（Release 返回 0），不需要额外的"查询计数"接口。其二，析构责任归"归零的那次调用"而非对象自己——这意味着同一个计数骨架可以挂任何析构行为：释放内存、关闭句柄、递归释放子对象。第 4 章的 `xerror` 与卷八的证书对象都是这个模式的具体化，读到那里时回来对照，你会看到同一副骨架长出不同的肉。

## 契约

- **视图三不**：不拥有、不补零、不延长寿命；打印用 `%.*s` 配 `Size`；视图不得存进比源活得久的结构。
- **资源边界**：处理不受信任输入的解析任务必须显式传入边界；默认值只是起点，按业务收紧；超限即失败且不消费剩余输入。
- **引用计数**：计数内嵌为第一个 `volatile int32` 字段；创建返回计数 1；Retain 返回 -1 是溢出保护；Release 归零者析构；析构与创建对称（`xrtMalloc` 对 `xrtFree`）。
- **哨兵**：查找未命中返回 `XRT_NPOS`，判断用 `== XRT_NPOS` 而非符号或大小比较。
- **失败无副作用**：失败路径不写输出参数，无隐藏分配；需要分配的结果经返回值携带并明确归属。
- **时间口径**：`xtime` 为微秒绝对时间戳；寻位统一 `xseek` 三基准。

## 避坑

### 坑 1：把视图当零结尾字符串用

症状：输出尾巴带乱码，或文本截断位置诡异；偶发越界读导致崩溃。

原因：`xstrview` 的 `Data` 只保证 `Size` 个字节有效，其后可能没有零结尾，也可能恰好有——行为取决于源，不可依赖。

```c bad
xstrview View = XRT_STR_LITERAL("hello");
printf("%s\n", View.Data);   /* 越界读：Size 之后没有承诺零结尾 */
```

```c good
xstrview View = XRT_STR_LITERAL("hello");
printf("%.*s\n", (int)View.Size, View.Data);   /* 长度配合数据，二进制安全 */
```

### 坑 2：忘记最后那次 Release（或 Release 多一次）

症状：前者内存只增不减（泄漏检测在第 6 章会抓到）；后者计数过早归零，对象被析构后仍在被使用（崩溃或数据损坏，且难以复现）。

原因：引用计数的约定是"每个持有者恰好一次 Release"，创建的那一票也是持有。

```c bad
example_object* pKeep = exampleCreate(42);
example_object* pShare = pKeep;
xrtRefRetain(&pShare->RefCount);     /* 共享成立 */
/* 用完只释放了一份——计数停在 1，对象永不析构 */
xrtRefRelease(&pKeep->RefCount);
```

```c good
example_object* pObject = exampleCreate(42);
xrtRefRetain(&pObject->RefCount);     /* 第二票：共享成立（两个持有者） */
xrtRefRelease(&pObject->RefCount);    /* 第一个持有者放弃 */
if ( xrtRefRelease(&pObject->RefCount) == 0 ) {
	/* 第二次归零：这次调用负责析构——Retain 与 Release 严格配对 */
}
```

注：正确心智模型是"**每个执行路径上，Retain 与 Release 严格配对**"——包括错误路径。错误路径少 Release 一样泄漏，多 Release 一样悬空。

### 坑 3：把视图存进活得比源久的结构

症状：功能正常跑了一阵子后偶发乱码或崩溃，堆栈指向完全无关的模块；复现条件飘忽（取决于源缓冲何时被复用或释放）。

原因：视图只是借用——源的生命周期结束（缓冲复用、托管字符串释放、栈帧返回）后，视图悬空。把它存进全局表、长生命周期对象或另一个线程的队列，等于埋了一颗定时炸弹。

```c bad
xstrview gCached;              /* 全局缓存 */
void load(const char* sJson)
{
	str s = xrtFormat("%s", sJson);
	gCached = (xstrview){ s, strlen(s) };   /* 视图指向托管字符串 */
	xrtFree(s);                             /* 源已释放，gCached 从此悬空 */
}
```

```c good
str gOwned = NULL;             /* 想活得久就自己拥有一份 */
void load(const char* sJson)
{
	xrtFree(gOwned);
	gOwned = xrtFormat("%s", sJson);   /* 拷贝进拥有式存储 */
}
/* 程序退出前 xrtFree(gOwned) 恰好一次 */
```

## 练习

### 基础：类型自检

写程序打印 `sizeof(int8/int32/int64/ptr)` 与 `XRT_NPOS` 的值（参考第 2 章 types 程序的形态），确认与你平台的预期一致。扩展一步：用 `XRT_STR_LITERAL` 构造一个视图并按本章的打印姿势输出它，亲手体会一次"长度与数据分离"的感觉。

### 进阶：手工遍历视图

构造 `xstrview` 指向 `"XRT-Program-Design"`，不用任何字符串函数，手工循环找出第一个 `-` 的位置，打印它前后两个子串（用 `%.*s`）。提示：位置类型用 `size_t`，未找到时约定返回 `XRT_NPOS`；比较时注意"未找到"要写成 `== XRT_NPOS` 而不是 `< 0`——`size_t` 没有负数。

### 挑战：并发引用计数

给 `examples/core/reference/main.c` 的对象加上多线程压力：4 个线程各 Retain/Release 一百万次后，主线程做最后一次 Release。验收标准：程序稳定运行且"value"打印成功（对象未被提前析构）；把 `volatile` 去掉重跑对比（记录是否出现异常退出，理解编译器优化的影响——只观察，不要求稳定复现）。做完后回答一个问题：为什么压力测试里对象恰好析构一次，而不是零次或两次？用"Retain 与 Release 严格配对"的规则推一遍。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 类型别名 | `int8..uint64` / `ptr` / `str,cstr` / `bytes,cbytes` / `xtime`（微秒）/ `xseek` |
| 视图契约 | 只借用、不补零、可含零字节；打印 `%.*s`；构造 `XRT_STR_LITERAL`；不得存进比源久的结构 |
| 哨兵 | 查找未命中 `XRT_NPOS`（`(size_t)-1`）；判断用 `== XRT_NPOS`，禁止符号比较 |
| 资源边界 | `xrtResourceLimitsInit` 三闸：深度/条目/字节；按业务规格收紧，超限即整体失败 |
| 引用计数 | 首字段 `volatile int32`；创建=1；Retain/Release 配对；归零者析构 |
| 进度回调 | `xrtprogressproc` 返回 `false` 请求中止；回调保持轻量、快速返回 |
| 版本 | 编译期宏与 `xrtVersion()` 同源；错配即头与库不同版 |
