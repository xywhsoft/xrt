---
num: 4
slug: error
title: 错误模型：xerror 全解
volume: 卷一 起步与核心
type: concept
lead: 结构化错误对象、原因链与线程错误槽——XRT 全库统一的失败报告模型。
api: error, error_format, core
---

## 导读

这一章是全书的地基之一。XRT 里几乎每个会失败的函数都用同一套约定报告失败：返回 `false` 或 `NULL`，同时把一个结构化的 `xerror` 对象放进当前执行上下文的错误槽。第 3 章你已经见过 `xrtGetError()` 的调用；本章把这套模型完整讲清楚——错误由哪些字段组成、原因链如何表达"下层到底怎么了"、错误对象的所有权归谁、线程错误槽的生命周期是什么，以及跨线程传递与包装时机的工程纪律。之后的每一章（网络、TLS、任务）都在复用这里的规则：你会反复看到"失败看返回值、详情看错误槽、分层靠原因链"这套组合，所以这一章值得读透。

## 引入

先看一个每个 C 程序员都遇过的场景：你写了一个加载配置的函数。

```c
str load_config(const char* sPath);
```

文件不存在时返回 `NULL`。调用方拿到 `NULL`，然后呢？"文件不存在"和"文件存在但 JSON 非法"和"内存不足"在返回值上完全一样。常见的补救是打印一条日志——但日志是给人看的，上层代码无法继续判断；或者定义一堆负数错误码——但每层的码空间会互相冲突，而且"文件的第几行第几列出错"这种上下文没地方放。

更麻烦的是分层。加载配置的函数内部调用了读文件和解析 JSON：当解析失败时，上层真正需要知道的是"配置加载失败，因为 JSON 第 3 行有语法错误"——两个层次的信息都要保留，而不是只留一句 `parse failed`。

传统方案各有各的残缺。返回 `int` 错误码需要每个模块维护码表，跨层传递时要么丢失上下文，要么码空间互相冲突；直接 `printf` 到 stderr 则把信息焊死在展示层，调用方既拿不到类别也无法分支处理；`errno` 风格的全局变量在多线程下需要平台特判，而且一次只能带一个整数。本质上，这些方案都只回答了"失败了"，没有回答"哪一层、为什么、下一步怎么办"。

这三个问题——**分类、上下文、分层**——就是 XRT 错误模型要解决的。它不用错误码整型，而用一个不可变的对象把结构化信息和人类消息一起携带，并在每个执行上下文里提供一个"当前错误"槽位，让 `false` 返回值与错误详情自动结伴而行。

## 概念

### 一个错误对象里有什么

`xerror` 是不可变、可跨线程持有的对象。创建之后没人能改它的字段，这让"传递"永远安全。每个错误由这些部分组成：

- **通用类别**（`xerrkind`）：跨模块稳定的粗分类，比如 `XERR_TIMEOUT`、`XERR_NOT_FOUND`。它给控制流用——"要不要重试""要不要降级"这类决策只看类别。
- **域**：模块命名空间字符串，比如 `"xrt.json"`、`"example.net"`。同一个数字码在不同域里含义不同，域保证了码不会冲突。
- **模块内错误码**：各域自己从 1 开始编号，供模块精确判断。
- **系统代码、操作名、UTF-8 消息**：分别对接平台 errno 语义、标记出错的操作、给人看的描述。
- **可选数据与原因链**：机器可读的附加信息（比如 JSON 出错位置），以及指向下层错误的链。

一条贯穿始终的纪律：**类别与域给机器判断，消息只用于展示**。不要写解析错误消息字符串的代码——该用类别判断就判断，需要细节就去读结构化字段。

类别集合刻意保持小而稳定，常用的有：

| 分组 | 类别 | 含义 |
| --- | --- | --- |
| 输入不符合契约 | `XERR_ARGUMENT` `XERR_TYPE` `XERR_VALUE` `XERR_RANGE` | 参数为空、类型不符、取值非法、越界 |
| 状态不允许 | `XERR_STATE` | 对象或运行时状态不支持当前操作 |
| 资源 | `XERR_MEMORY` `XERR_IO` `XERR_NOT_FOUND` `XERR_EXISTS` `XERR_PERMISSION` | 内存不足与系统资源错误 |
| 稍后可重试 | `XERR_AGAIN` | 对象仍有效，等待后重试 |
| 等待终止 | `XERR_TIMEOUT` `XERR_CANCELLED` `XERR_CLOSED` | 超时、取消、资源已关闭 |
| 协议与能力 | `XERR_PROTOCOL` `XERR_UNSUPPORTED` `XERR_INTERNAL` | 协议错误、能力缺失、内部不变量破坏 |

`XERR_NONE` 只表示"没有错误"，不能用于创建错误对象。

### 原因链：分层的失败故事

`xrtErrorWrap` 把一个下层错误"包"进新的上层错误，形成原因链。回到开头的配置例子，链长这样：

```diagram flow
- 下层：JSON 解析失败：xrt.json 域记录行列出错位置
- 包装：配置加载失败：example.config 域 XERR_IO 包住解析错误
- 判断：xrtErrorIs 顶层看到 XERR_IO，沿链可查到语法错误
- 展示：xrtErrorMessage 只读顶层消息，日志层可遍历整链
```

链上的判断不需要手工遍历：`xrtErrorIs(错误, XERR_TIMEOUT)` 沿链查找指定类别，命中返回那条错误的借用指针，未命中返回 `NULL`。"这次失败到底是不是超时引起的"是一行代码的事。`xrtErrorFind` 则按域与码精确查找。

### 包装的时机：错误在边界处获得语义

原因链不等于层层都包。一条实用的纪律是：**在模块边界包装，在模块内部传递**。JSON 模块内部抛出的 `xrt.json` 域错误，穿过"配置加载"这层边界时被包成 `example.config` 域的"配置加载失败"——从此上层的重试与降级策略有了明确的决策点：是针对 `XERR_IO`（重试有意义）还是 `XERR_ARGUMENT`（重试无意义，该报配置错误）。而边界之间的底层调用不必再包一层"调用失败"，否则链条会被无信息的重复节点撑爆，真正的语义反而被稀释。

判断"该不该包"的试金石：如果新的一层能为错误**增加决策信息**（新的域、新的语义类别、新的恢复策略），就包；如果只是转述"下面失败了"，就不包，让原始错误继续上行。

### 错误与日志的分工

错误对象负责"结构化地携带失败"，日志负责"留下运行痕迹"，两者不要互相替代。一个常见的反面模式是在每层都打印一遍错误消息——同一件事在日志里出现三次，排障时反而看不清结构。更好的分工是：错误在调用链上安静传递，在**决策点**处理（重试、降级、最终失败出口），日志只在决策点与关键边界记录，并携带完整的类别与原因链。第 37 章的 Logger 支持把 `xerror` 的结构化字段写进日志记录，本章先记住分工原则即可。

### 线程错误槽：当前错误

每个执行上下文拥有一个当前错误槽。普通线程默认使用线程上下文；协程和任务调度器切换自己的错误槽，所以协程迁移不会污染承载线程，任务之间也互不串扰。槽的语义如下：

```diagram state
未设置 -> 已设置: SetError（槽增加对象引用）
已设置 -> 未设置: ClearError / SetError(NULL)
已设置 -> 未设置: TakeError（取走所有权）
```

`xrtGetError` 只借用，不改变状态。三条关键规则：

- `xrtSetError` **增加**传入对象的引用并替换当前错误；传入 `NULL` 等价于清除。替换或清空时，槽释放自己持有的那份引用。
- `xrtTakeError` 取走当前引用并清空槽——想把错误长期保存（放进日志队列、跨线程传递）时用它，用完记得 `xrtErrorFree`。
- 原生线程退出时仍留在默认槽里的对象会自动释放，扩展线程不必为防泄漏强制清错。

还有一个容易踩的默认：**成功操作不会隐式清除旧错误**。也就是说错误槽里可能留着上一次失败的内容——只有当函数通过返回值报告失败之后，你才有资格去读槽。

### 跨线程与长期保存

`xerror` 不可变，所以对象本身可以安全地跨线程持有；需要小心的只有"谁负责释放"与"槽属于哪个上下文"两件事。想把失败从工作线程带回主线程（比如任务结果里附一条错误），正确姿势是 `xrtTakeError` 取走所有权、随结果一起传递、消费端 `xrtErrorFree`；直接把 `xrtGetError()` 的借用指针存起来是竞态——槽随时可能被替换。协程与任务的错误槽由调度器管理，随执行上下文迁移，不需要你搬运；只有原生线程之间才需要显式携带。引用计数由 `xrtErrorRef`/`xrtErrorFree` 管理，规则与第 5 章的通用引用计数对象一致。

### 常见失败的一步写法

多数时候不需要手工"创建、设置、释放"三步。`xrtSetErrorInfo` 一步完成创建与移交：

```c
xrtSetErrorInfo(XERR_ARGUMENT, "app.config", 1, "path is empty");
```

需要动态消息时用 `error_format` 模块的 `xrtSetErrorFormat`（printf 规则，拒绝带写入副作用的 `%n`，构造失败时保留无分配的 `XERR_MEMORY`）。两个 Helper 创建的都是不可变对象并直接移交给当前上下文。携带源码位置可以用 `xrtErrorBuildAt`，之后经 `xrtErrorFile`、`xrtErrorLine`、`xrtErrorColumn` 读回。

## 示例

### 完整程序：创建、包装、查询一条原因链

下面的程序来自仓库范例 `examples/core/error/main.c`，完整演示所有权模型下的一次错误生命周期。先读代码，再看它做了什么：

```embed path="examples/core/error/main.c" title="examples/core/error/main.c"
```

```term
$ gcc -O1 -I single impl.c examples/core/error/main.c -lws2_32 -liphlpapi
error: request failed
timeout cause: yes
```

**刚才发生了什么。** 程序分四步走。第一步 `xrtErrorCreate` 按"类别 + 域 + 码 + 消息"创建下层错误（网络超时），返回**拥有式**指针——谁创建谁负责释放。第二步 `xrtErrorWrap` 把它包成上层的 `XERR_IO`；Wrap 内部对原因**增加引用**，所以创建方随即 `xrtErrorFree` 掉自己那份，引用计数归零与否交给链管理。第三步 `xrtSetError` 放入线程槽——注意 SetError 也会增加引用，槽有自己的那份，创建方再释放自己的一份，此刻对象有两个引用者（链与槽）。第四步用 `xrtGetError` 借用读取：`xrtErrorMessage` 永不返回 `NULL`（未设置错误时返回 `(no error)`），`xrtErrorIs` 沿链查到 `XERR_TIMEOUT` 命中。最后 `xrtClearError` 清空槽并释放槽持有的引用。

### 完整程序：printf 风格一步构造

第二个程序来自 `examples/core/error_format/main.c`，展示动态消息的便捷构造：

```embed path="examples/core/error_format/main.c" title="examples/core/error_format/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/core/error_format/main.c -lws2_32 -liphlpapi
file does not exist: app.json
```

与 `xrtErrorCreate` 的区别在于消息来源：Create 接受整条字符串，Format 接受 printf 格式串与可变参数，内部复用字符串格式化模块渲染后创建——错误消息可以携带运行时上下文（这里是文件名），调用方不必先自己拼缓冲区。

注意第二个程序没有出现一次 `Free`：`xrtSetErrorFormat` 与 `xrtSetErrorInfo` 一样，创建后直接**移交**给当前上下文，创建方不再持有引用，最后的 `xrtClearError` 释放槽持有的那一份。对比第一个程序"创建 → 设置 → 自己 Free"的三步，一步写法不只是省行数——它消除了"忘记释放自己那份"这一整类错误（本章坑 2）。日常代码里，除非要把错误存进结构或跨线程携带，优先用 Helper。

## 契约

- **所有权**：`Create`/`Wrap` 返回拥有式指针，最终必须正好一次 `Free`；`Wrap` 对原因加引用；`SetError` 对传入对象加引用；`GetError` 只借用。
- **线程**：`xerror` 不可变、可跨线程持有；每个执行上下文独立一个错误槽；要跨线程传递先用 `TakeError` 取走。
- **错误口径**：`false`/`NULL` 返回值宣告失败，错误槽提供详情；成功不清槽，读槽前必须先看到失败返回值。
- **展示与判断分离**：控制流只看类别（必要时域+码）；消息、位置等只用于展示与日志。
- **扩展数据**：需要结构化附加数据的模块应为 `Data` 定义稳定格式，不能要求调用方解析展示消息；`xrtErrorBuildAt` 携带源码位置，`xrtErrorFile`/`xrtErrorLine`/`xrtErrorColumn` 读回。
- **最小依赖**：Core 错误 API 不依赖容器、字符串或 printf 运行时；动态格式化是独立的 `error_format` 便利模块。

## 避坑

### 坑 1：释放借用的错误

症状：偶发崩溃或堆损坏，往往出现在"读一下错误再清理"的代码附近。

原因：`xrtGetError` 返回的是借用指针，槽随时可能替换或清空并释放那份引用；对借用指针调用 `xrtErrorFree` 是重复释放。

```c bad
xerror* pError = xrtGetError();
printf("%s\n", xrtErrorMessage(pError));
xrtErrorFree(pError);   /* 释放了借用——槽清空时二次释放 */
```

```c good
/* 借用即取即用；确要长期持有就取走所有权 */
printf("%s\n", xrtErrorMessage(xrtGetError()));
/* 或：xerror* pKeep = xrtTakeError(); ... xrtErrorFree(pKeep); */
```

### 坑 2：Wrap 之后忘记释放自己的那份原因引用

症状：内存缓慢增长；泄漏检测工具报告 `xerror` 对象只增不减。

原因：`xrtErrorWrap` 对原因**增加**引用后，创建方原来的那份引用仍在，必须由创建方释放，否则原因对象永远不会归零。

```c bad
xerror* pCause = xrtErrorCreate(XERR_TIMEOUT, "net", 1, "timeout");
xerror* pError = xrtErrorWrap(pCause, XERR_IO, "client", 2, "request failed");
/* 少了 xrtErrorFree(pCause)：原因对象泄漏 */
```

```c good
xerror* pCause = xrtErrorCreate(XERR_TIMEOUT, "net", 1, "timeout");
xerror* pError = xrtErrorWrap(pCause, XERR_IO, "client", 2, "request failed");
xrtErrorFree(pCause);
```

### 坑 3：用"槽里有错误"当作失败信号

症状：函数明明成功返回了，上层却报告旧错误；错误处理逻辑时对时错。

原因：成功操作不隐式清除旧错误，槽里可能残留上一次失败的对象。把"读槽非空"当失败判据，等于用陈旧状态做决策。

```c bad
call_something();              /* 返回 true，成功 */
if ( xrtGetError() != NULL ) { /* 误判：这是上一次失败留下的 */
	recover();
}
```

```c good
if ( !call_something() ) {     /* 先看返回值宣告失败 */
	report(xrtErrorMessage(xrtGetError()));
}
```

## 练习

### 基础：创建并读取

写一个程序：用 `xrtSetErrorInfo` 设置一个 `XERR_ARGUMENT` 错误，打印类别对应的消息，再 `xrtClearError` 清空后确认 `xrtErrorMessage(xrtGetError())` 返回 `(no error)`。

### 进阶：三层原因链

构造"`服务启动失败 → 配置加载失败 → JSON 第 N 行语法错误`"三层原因链：最底层用 `xrtErrorWrap` 之外的 `xrtErrorCreate`，中间层与顶层用 Wrap。用一次 `xrtErrorIs` 查询最底层的类别并打印命中结果。提示：每层 Wrap 后立刻释放自己那份原因引用，避免泄漏。

### 挑战：带位置的错误日志

实现 `log_failure(const char* sFile, int iLine)` 宏：失败路径调用 `xrtErrorBuildAt`（自动携带调用点源码位置），随后打印"文件:行号 + 消息 + 原因链每层消息"。验收标准：构造一次两层原因链，输出中能看到两个层次的消息与调用点文件行号；用 `xrtErrorFile`/`xrtErrorLine` 读回的位置与宏展开处一致。完成后回到本章开头的配置加载场景，把它改写成返回结构化错误的版本，你会得到一个可以直接放进项目的失败处理骨架。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 创建 | `xrtErrorCreate`（拥有式）/ `xrtSetErrorInfo`（一步移交）/ `xrtSetErrorFormat`（printf 消息） |
| 分层 | `xrtErrorWrap` 包原因并加引用；`xrtErrorIs` 按类别沿链查；`xrtErrorFind` 按域与码查 |
| 槽语义 | `SetError` 加引用替换；`TakeError` 取走所有权；`ClearError` 清空并释放；`GetError` 只借用 |
| 判定时机 | 返回值宣告失败后才能读槽；成功不清槽 |
| 一步构造 | `xrtSetErrorInfo` / `xrtSetErrorFormat` 创建即移交，无泄漏窗口 |
| 跨线程 | 对象不可变可跨线程；携带先 `TakeError`，消费端 `Free` |
| 包装纪律 | 模块边界包装、内部传递；新层带不来新语义就不包 |
| 源码位置 | `xrtErrorBuildAt` 写入，`xrtErrorFile`/`xrtErrorLine`/`xrtErrorColumn` 读回 |
| 所有权 | 创建方必有一次 `Free`；借用不释放；跨线程先 `TakeError` |
| 展示 | `xrtErrorMessage` 永不 `NULL`；类别给控制流，消息给人 |
