---
num: 25
slug: string
title: 字符串操作全集
volume: 卷四 文本与结构化数据
type: practice
lead: 视图管线零分配、构建器 O(n) 拼接、查找编辑与格式化——文本处理的三层武器库。
api: string
---

## 导读

卷四从字符串开始。XRT 的文本哲学是第 3 章视图约定的全面兑现：**能零分配就零分配**——Trim、Cut、大小写比较这些操作只搬运 `(指针, 长度)` 对，直到确实需要新内存才分配；**要分配就用构建器**——拼接链 O(n) 总拷贝而不是 Concat 的 O(n²)；**产物一律拥有式**——返回 `str`、`xrtFree` 释放，与第 5 章约定无缝。本章把这三层组织成"视图管线 → 构建器 → 全家桶速查"的递进，查找、编辑、分割、格式化在速查层一次装齐。

## 引入

一个 URL 处理函数要做五件事：去首尾空白、按 `?` 切出路径与查询、路径转小写比较、查询里删掉某些字符、最后拼上前缀。教科书式写法每步 `strdup` 一份——五步五次分配、五次释放，失败路径挂五串清理。而仔细看这五步：前四步**根本不需要新内存**——空白裁剪只是把指针往中间挪、切分只是把一段视图拆成两段视图、大小写比较有专门的 IgnoreCase 版本、删字符可以写进调用方的栈缓冲。只有最后拼前缀真正需要分配。

这就是视图管线的出发点：把"操作文本"与"产生新文本"分开，前者的成本是几次指针加减，后者的次数被压到最少。热路径上（每个请求都跑一遍的代码）这比"每步复制"快一个数量级——不是算法更聪明，是**不做不必要的分配**。

## 概念

### 第一层：视图管线（零分配）

```diagram flow
- Trim：首尾空白裁剪——只挪指针，原字节不动
- Cut：按首处分隔符切两段——前后都是视图，不要的段传 NULL 出参
- FilterTo：剔除集合内字节——写入调用方缓冲，容量不足即失败
- Concat：两视图拼接——管线里第一次真正分配，产物拥有式
```

管线的设计纪律：**越晚分配越好**。每个"不分配"的步骤都在为后面的步骤保留选择（切出来的视图还能再切、再滤、再比）；一旦分配，后续操作就都在副本上进行。判断某个步骤要不要分配的标准很简单：输出的字节集合是否是输入的真子串（Trim/Cut/子串切片都是——零分配可办）还是需要重组（拼接、替换、过滤——分配或调用方缓冲）。

### 第二层：构建器（O(n) 拼接）

拼三段以上不要用 `Concat` 链——每次 Concat 产生一个新中间串，n 段拼接是 O(n²) 总拷贝。`xstrbuf` 构建器内部倍增扩容（与第 15 章容器同策略），追加链的总拷贝是 O(n)。`Take` 的移交语义与 buffer 容器（第 15 章）同构：缓冲所有权直接转交、构建器归零复用。构建器还能 `AppendRepeat`（重复追加——分隔线、缩进的生成器）与格式化追加。

### 第三层：全家桶速查

| 分族 | 函数 | 备注 |
| --- | --- | --- |
| 查找 | `xrtStrFind` / `CaseFind` / `RFind` | 返回位置，未命中 `XRT_NPOS` |
| 字节级 | Any / Byte 查找 | 定位集合内任一字节 |
| 前后缀 | `xrtStrStarts` / `Ends` / `CutPrefix` / `CutSuffix` | 通路判断与视图修剪，各有 Case 变体 |
| 编辑 | `Insert` / `Remove` / `Replace` | 拥有式产物 |
| 分割 | `xrtStrSplit` / `Cut` 迭代式 | Split 到回调，Cut 循环切 |
| 连接 | `Join` / `Repeat` / `Dup` | 拥有式产物 |
| 大小写 | `xrtStrUpper` / `Lower`（含 To 调用方缓冲版）/ Case 系比较 | 忽略大小写用 Case 系专用函数 |
| 裁剪 | `Trim` / `TrimLeft` / `TrimRight` / `Pad` / 截断 | 零分配修剪，Pad 需要缓冲 |
| 度量 | `Length` / `IsEmpty` / 空白判断 | 视图直读 |
| 距离 | `xrtUtf8Distance` / `xrtUtf8Similarity` | 按 Unicode 标量的编辑距离（第 27 章字符集家族） |
| 通配 | `xrtStrGlob` | glob 风格 `*?` 匹配 |
| 格式化 | `xrtFormat` / `FormatV` | printf 规则，拥有式产物 |

两列函数值得专门点出：**Case 系**（`CaseFind`、`CaseEqual`、`CaseStarts` 等）一次完成忽略大小写比较，比"先 ToLower 两份再比"少两次分配；**距离/相似度**（字符集家族的 `xrtUtf8Distance`/`xrtUtf8Similarity`，按 Unicode 标量计数）给模糊匹配一个开箱即用的答案，distance 示例演示了"编辑距离 2、相似度 0.6"的读法。

### 与 C 标准库的分工

为什么不直接用 `strstr`/`strdup`？三个硬理由：标准函数以零结尾为前提（二进制安全场景直接出局）、失败路径不报告（`strdup` 的 OOM 只能返回 NULL，无诊断）、产物归属约定不明（谁分配谁释放全靠自觉）。XRT 字符串族全部以 `(视图, 长度)` 为货币、`bool` 返回值加错误槽、拥有式产物配 `xrtFree`——三条都是卷一的约定在文本层的兑现。

## 示例

### 完整程序：视图管线四步一气呵成

来自仓库范例 `examples/string/basic/main.c`：

```embed path="examples/string/basic/main.c" title="examples/string/basic/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/string/basic/main.c -lws2_32 -liphlpapi
alpha.txt
```

**刚才发生了什么。** ① `xrtStrTrim` 返回原字符串的**子视图**——两个指针向中间收拢，原字节一字不动、零分配。② `xrtStrCut` 按首个 `/` 切分：`Name` 收前段 `alpha`，后段不需要所以第二个出参传 `NULL`——不需要的产物不接，这是 Cut 的省事约定。③ `FilterTo` 把 `-` 与 `_` 从视图里剔除、其余写入**调用方的栈缓冲**——即使到了"需要一块可写内存"的步骤，分配权仍在调用方手里（容量不足返回失败而不是偷偷分配）。④ 管线里第一次、也是唯一一次堆分配发生在 `Concat` 拼 `.txt`——产物拥有式、`xrtFree` 释放。四步里三次零分配，这就是"能晚分配就晚分配"的完整示范。

### 完整程序：构建器与移交

来自 `examples/string/builder/main.c`：

```embed path="examples/string/builder/main.c" title="examples/string/builder/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/string/builder/main.c -lws2_32 -liphlpapi
items=ababab
```

**刚才发生了什么。** ① `xrtStrBufInit` 拿栈上句柄开张——构建器内部缓冲按需增长（倍增策略），两次追加 `items=` 与 `ab`×3 之间没有中间产物。② `AppendRepeat` 是重复追加的专用入口——分隔线、缩进、填充这类"一段内容来 n 次"的需求不需要你写循环。③ `Take` 把缓冲**所有权移交**给调用方：没有复制、没有共享，构建器回到空状态可继续复用——与第 15 章 `xrtBufferTake` 完全同构的语义，第 15 章的"攒—凑—取"节奏在字符串世界原样成立。④ 失败路径 `xrtStrBufFree` 归还构建器——容器三约定在这里照常生效。

## 契约

- **视图优先**：修剪、切分、前后缀判断零分配；真子串输出一律视图。
- **拥有式产物**：Concat/Join/Dup/Replace/Format 返回 `str`，恰好一次 `xrtFree`。
- **构建器纪律**：拼三段以上用 `xstrbuf`；`Take` 移交后构建器归零复用；失败路径 `Free`。
- **Case 系**：忽略大小写用 `CaseFind`/`CaseEqual` 专用函数，不先转换再比较。
- **未命中语义**：查找返回 `XRT_NPOS`，判断用 `== XRT_NPOS`（第 3 章哨兵纪律）。
- **二进制安全**：全部函数以 `(视图, 长度)` 为货币，内容可含零字节。

### 从示例到工程：字符串的三种宿主形态

与容器的三宿主（第 23 章）平行，字符串在工程里有三种典型宿主。**请求级临时文本**：视图管线 + 栈缓冲，全程零分配——URL 处理、头部分析这类每请求必跑的代码用这套。**累积型输出**：构建器挂在处理上下文上，跨多次回调持续追加、终局一次 Take——日志行、报表行、序列化输出的标准形态。**长期保存的字段**：解析出的字段要活得比请求久（配置项、缓存键），用 Dup/Join 的拥有式产物挂进容器——归属从此归容器管的那个结构。三种宿主对应"零分配、晚分配、拥有式"三档，与第 5 章内存语言的对应关系一眼可查。

### 一个习惯：先问输出是不是子串

写任何文本函数前先问：**输出是输入的真子串吗？** 是——返回视图，零分配（Trim/Cut/Slice 的世界）；不是但内容有界——写调用方缓冲（FilterTo/To 系）；需要开放长度——拥有式产物或构建器。这个三问习惯能挡掉大部分"顺手 Dup"的浪费与大部分"返回悬空视图"的事故，是本章最值得带走的一句话。

## 避坑

### 坑 1：Concat 链拼接多段

症状：拼 URL、拼 SQL、拼日志的函数在长输入下越来越慢；profiler 显示热点在 memcpy 与分配器。

原因：n 段 Concat 每次复制全部既有内容，总拷贝 O(n²)；中间串的分配与释放还各来 n 次。

```c bad
str s = xrtStrDupView(Prefix);
s = xrtStrConcat((xstrview){ s, strlen(s) }, Host);   /* 复制全部既有内容 */
s = xrtStrConcat((xstrview){ s, strlen(s) }, Path);   /* 又复制一遍 */
s = xrtStrConcat((xstrview){ s, strlen(s) }, Query);  /* 再一遍——O(n²) 已成定局 */
```


```c good
xstrbuf tBuf;
xrtStrBufInit(&tBuf);
xrtStrBufAppend(&tBuf, Prefix);
xrtStrBufAppend(&tBuf, Host);
xrtStrBufAppend(&tBuf, Path);
xrtStrBufAppend(&tBuf, Query);
str s = xrtStrBufTake(&tBuf);     /* 一次移交，总拷贝 O(n) */
```

### 坑 2：视图存过了源的生命周期

症状：与第 3 章坑 3 同族——函数返回的"处理结果"在调用方偶尔变成乱码或空段。

原因：Trim/Cut 产出的视图**借用**原内存；把视图存进结构体、返回给上层、跨线程传递时，源可能已经释放或被复用。

```c bad
xstrview getName(const char* sRaw)
{
	xstrview Text = xrtStrTrim((xstrview){ sRaw, strlen(sRaw) });
	xstrview Name;
	xrtStrCut(Text, XRT_STR_LITERAL("/"), &Name, NULL);
	return Name;   /* Name 指向 sRaw 内部——调用方换掉 sRaw 即悬空 */
}
```

```c good
str getName(const char* sRaw)
{
	xstrview Text = xrtStrTrim((xstrview){ sRaw, strlen(sRaw) });
	xstrview Name;
	xrtStrCut(Text, XRT_STR_LITERAL("/"), &Name, NULL);
	return xrtStrDupView(Name);   /* 要活得久就自己拥有一份 */
}
```

## 练习

### 基础：管线复现

复现主示例四步管线，输出 `alpha.txt`；再改输入为 `"  x/y/z  "`（两个分隔符），先笔算 Cut 取的是哪一段，再运行对拍。

### 进阶：查询串解析器

用 Cut 循环 + Find + CaseEqual 实现 `?a=1&b=2&c=3` 的解析：切出每对键值、忽略大小写匹配键名、值里剔除 `%`——全程只在最后拼输出时分配一次。提示：Cut 的后段视图继续 Cut 就是迭代分割。

### 挑战：日志行拼装器

用构建器实现结构化日志行：时间戳、级别、模块、消息、键值对若干（值可能是字符串或整数）。要求追加链无中间产物、字段间分隔与缩进用 `AppendRepeat`、整行一次 `Take`。验收标准：拼装 1000 行日志的总分配次数与行数同阶（而不是与字段乘积同阶）；输出格式固定可写进断言。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 视图管线 | Trim/Cut/前后缀零分配；分配推迟到 Concat |
| 构建器 | 拼三段以上用 `xstrbuf`；`Take` 移交复用；O(n) 总拷贝 |
| 查找 | Find/CaseFind/RFind；未命中 `XRT_NPOS`；`== XRT_NPOS` 判断 |
| 编辑 | Insert/Remove/Replace 拥有式产物 |
| 分割 | Cut 迭代切 / SplitInit+SplitNext 游标式逐项取 |
| 大小写 | Case 系专用函数，不先转换再比较 |
| 模糊 | xrtUtf8Distance 编辑距离 / 相似度；glob 通配 xrtStrGlob |
| 归属 | 视图借用不拥有；拥有式产物恰好一次 `xrtFree` |
