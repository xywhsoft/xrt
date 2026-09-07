---
num: 12
slug: hash-xid
title: 哈希与 XID 时间有序 ID
volume: 卷二 数学、随机与标识
type: practice
lead: 确定性哈希、带密钥 SipHash 与 192 位时间有序标识——分桶校验、防碰撞防预计算、可排序主键三件事一次讲透。
api: hash, xid
---

## 导读

卷二收官章把两个"标识"主题放在一起：**哈希**把任意字节折叠成固定宽度指纹，**XID** 为每条记录生成时间有序的唯一标识。它们共享一个关键词——**确定性**：哈希的确定性让校验与分桶可复现，XID 的确定性让"字典序 = 生成时间序"。本章同时划清安全边界：无密钥哈希不抗碰撞攻击，带密钥的 SipHash 才是对外可见键（如哈希表键）的正确选择；而"存储用户密码"两者都不是答案，那是卷八 crypto 模块的领域。读完本章，你会为三个高频问题选对工具：数据一致性校验、对外哈希表键、分布式主键。

## 引入

三个场景。其一：每天同步几 GB 的索引文件，想知道"变没变"——把整个文件传过去比对太贵，算个指纹只传 8 字节。其二：服务端用请求头里的字段做哈希表键，结果攻击者构造碰撞输入，把哈希表退化成链表——这就是哈希碰撞 DoS，答案是给哈希配一把只有你知道的密钥。其三：给分布式系统选主键——自增整数要协调中心，UUID v4 完全随机导致数据库索引碎片化，而"时间在前"的标识天然按创建顺序聚集。

三个场景分别对应 `xrtHash32/64`（确定性指纹）、`xrtSipHash`（带密钥抗碰撞）、XID（时间有序唯一标识）。它们都不是密码学原语——用途边界会在契约区写死。这一章也是卷二的收官：第 10 章的确定性数值、第 11 章的确定性随机序列、本章的确定性指纹，合起来是"工程确定性"的三块基石——凡是写进测试断言的东西，都必须跨平台跨时间可复现。

## 概念

### 确定性哈希：同输入永远同输出

`xrtHash32(pData, iSize)` 与 `xrtHash64` 用固定默认种子——**同一份字节在任何时候、任何进程算出同一个值**。这是校验和、分桶、缓存键的前提。需要跨进程一致但"别让对手预计算"时用显式种子版 `xrtHash32Seed` / `xrtHash64Seed`：多进程分片用同一个种子保持一致，对外场景换种子打乱预计算表。注意"确定性"的另一面：**无密钥的哈希任何人都能算**——它证明"数据是这份"，不能证明"数据来自谁"。

### 带密钥哈希：SipHash-2-4

`xrtSipHash(pData, iSize, Key)` 使用 `xsipkey`（128 位密钥，`xrtSipKey(低64, 高64)` 构造）。不知道密钥就无法构造碰撞——面向不可信输入的哈希表键必须用它。流式接口 `xrtSipHashInit` / `Update` / `Final` 处理分块到达的数据（网络流、大文件），与一次性版本**结果等价**——同样的密钥与数据，两种姿势得到同一个 64 位值。密钥的生命周期遵循第 5 章敏感数据纪律：用完 `xrtSecureZero`。

### XID：时间在前的 192 位标识

```diagram flow
- 结构：64 位微秒时间戳 + 随机与序列位，共 24 字节
- 文本形态：32 字符定长（base62 风格），可直接进 URL / 日志 / 主键
- 有序性：时间在前 ⇒ 字典序 = 生成时间序，索引友好、日志可排序
- 零分配：二进制路径值语义；文本路径写调用方缓冲
```

XID 解决"分布式主键"的三难：不需要协调中心（时间 + 随机 + 序列本地生成）、不碎片化（时间前缀聚集）、可读（32 字符文本）。`xrtXidMake` 生成、`xrtXidWrite` 写入调用方缓冲（容量 `XID_TEXT_CAPACITY`，规范长度 `XID_TEXT_SIZE` 不含结尾零）、`xrtXidParse` 严格解析回二进制（长度或字符集任何偏差都失败并给出**首个非法字节位置**——`xrtXidErrorOffset`）、`xrtXidTime` 以 O(1) 位运算取出生成时刻。批量生成用 `xrtXidMakeMany`（一次取整批随机字节，降低系统调用开销）；`xrtXidCompare` 提供稳定全序——排序与去重的基础原语。与第 11 章的四层随机对照一下分工：XID 内部使用安全随机源保证唯一性，但 XID 本身不是"秘密"——它出现在 URL、日志、数据库里都无妨；需要不可预测的标识（如 capability token）请用第 11 章的 `SecureText`。

### 一张选型表

| 需求 | 工具 | 不该用 |
| --- | --- | --- |
| 文件/数据一致性校验 | `xrtHash64` | —— |
| 多进程一致分片 | `Hash64Seed` 同种子 | —— |
| 对外不可信输入的表键 | `xrtSipHash`（密钥保密） | 无密钥哈希 |
| 防预计算指纹 | `Hash64Seed` 换种子 | 默认种子 |
| 分布式主键 / 日志关联 | XID（`Make`/`MakeMany` + 文本化） | 自增整数、随机 UUID |
| 存储用户密码 | 卷八 crypto 的密码哈希 | 本章任何函数 |

### 一个先行提醒：指纹不是加密

跑示例前把一个高频误解钉死：哈希是**指纹**不是**加密**。加密可逆（有密钥能还原），指纹不可逆（从 8 字节还原不了 8GB 文件）；加密保密内容，指纹公开内容的存在性。"给文件算指纹后把原文删了"是不可恢复的错误——指纹只能用来**比对**，不能用来**存放**。这个边界想清楚了，下面三组示例的输出才有正确的解读方式。

## 示例

### 完整程序：确定性哈希与显式种子

来自仓库范例 `examples/hash/hash32/main.c`：

```embed path="examples/hash/hash32/main.c" title="examples/hash/hash32/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/hash/hash32/main.c -lws2_32 -liphlpapi
default: 9590B597
seeded : 064A210B
```

**刚才发生了什么。** ① `sizeof(sKey)-1` 显式排除结尾零——哈希的输入是二进制安全的字节串，零结尾只是 C 字符串的约定；多算一个零就是另一份数据、另一个指纹。② 默认种子输出 `9590B597`——跨平台跨版本稳定，你今天算的和昨天的 CI 对得上。③ 同一份数据换种子得完全不同的指纹——种子参与的是同一个确定性函数，"防预计算"的本质是对手不知道你的函数变体。

### 完整程序：SipHash 一次性与流式对拍

来自 `examples/hash/variants/main.c`，验证两种姿势结果等价：

```embed path="examples/hash/variants/main.c" title="examples/hash/variants/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/hash/variants/main.c -lws2_32 -liphlpapi
seed64: 1E3C0D5C77AC4C64
sip:    21D9F0C356D09D0A
stream-equal=yes
```

**刚才发生了什么。** ① `xsipkey` 由两个 64 位字构造——128 位密钥空间，密钥来自安全随机（第 11 章）而非硬编码。② 一次性 `xrtSipHash` 十八字节输入得 `21D9F0C356D09D0A`。③ 流式版把同一份数据**分两块**喂入（`Update` 两次），`Final` 的结果与一次性完全一致——第三行 `stream-equal=yes` 是本章最重要的断言：分块方式不影响指纹，网络流式处理与整块计算可以互相对拍。④ `Hash64Seed` 那行则展示了种子版确定性哈希的典型用法：`"user:42"` 配种子 `0x1234` 的指纹稳定——多进程分片按这个值取模路由，扩缩容时种子不变则路由不变。

### 完整程序：XID 生成、文本化、解析、取时间

来自 `examples/id/xid/main.c`：

```embed path="examples/id/xid/main.c" title="examples/id/xid/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/id/xid/main.c -lws2_32 -liphlpapi
XID: V-OPfAm6...（32 字符定长文本，示例）
Unix microseconds: 17...(微秒时间戳)
```

**刚才发生了什么。** ① 一条龙：`Make` 生成 → `Write` 文本化 → `Parse` 解析回二进制 → `Time` 取时刻——四个函数构成 XID 的读写闭环。② `Parse` 的输入视图长度用 `XID_TEXT_SIZE`（不含结尾零）：任何长度或字符偏差都整体失败，配合 `xrtXidErrorOffset` 能从错误里取出首个非法字节的位置——错误自带定位，不需要解析错误消息。③ 输出的时间戳随运行时刻变化，但**前缀递增有序**的性质不变：先打印的 XID 字典序小于后打印的。

## 契约

- **确定性**：`Hash32/64` 与 SipHash 同输入同密钥必同输出，跨平台跨版本稳定（分块方式不影响）。
- **安全边界**：无密钥哈希不抗碰撞攻击；对外表键用 SipHash 且密钥保密；**密码存储不归本章**（卷八密码哈希）。
- **密钥纪律**：`xsipkey` 属敏感数据——安全随机生成、用完 `xrtSecureZero`。
- **XID 有序性**：字典序 = 生成时间序；`Compare` 提供稳定全序；快速连续生成的**单次比较方向**不构成承诺（微秒时钟决定先后）。
- **解析严格性**：`Parse` 只接受定长合法文本；失败时输出不写入，错误偏移可从线程错误读取。
- **批量生成**：`MakeMany` 一次生成连续存储的一批，降低系统调用次数。

## 避坑

### 坑 1：用无密钥哈希做对外哈希表键

症状：服务端哈希表在特定构造的请求下性能崩塌（哈希碰撞 DoS）；表越长劣化越明显。

原因：默认种子的哈希函数是公开的——攻击者可以离线构造大量同指纹输入，把你的表退化成链表。

```c bad
uint64 iKey = xrtHash64(Header.Value, Header.Size);  /* 公开函数 */
Bucket = TableFind(iKey);                             /* 碰撞可被构造 */
```

```c good
uint64 iKey = xrtSipHash(Header.Value, Header.Size, gTableKey);  /* 密钥保密 */
Bucket = TableFind(iKey);
```

### 坑 2：XID 解析把结尾零算进长度

症状：从文本（文件、URL 参数）读回 XID 时解析失败，错误偏移指向最后一格之后；单步调试又"看起来字符串没问题"。

原因：`Parse` 输入是**恰好 32 个字符**的视图（`XID_TEXT_SIZE`）；传 `strlen+1` 或用 `XID_TEXT_CAPACITY` 当长度，把结尾零当成了第 33 个字符。

```c bad
char arrText[XID_TEXT_CAPACITY];
load(arrText);
if ( !xrtXidParse((xstrview){ arrText, XID_TEXT_CAPACITY }, &Value) ) {
	/* 长度 33：必然失败——结尾零不是文本的一部分 */
}
```

```c good
char arrText[XID_TEXT_CAPACITY];
load(arrText);
if ( !xrtXidParse((xstrview){ arrText, XID_TEXT_SIZE }, &Value) ) {
	size_t iOffset = 0;
	xrtXidErrorOffset(xrtGetError(), &iOffset);   /* 首个非法字节位置 */
}
```

## 练习

### 基础：指纹对拍

对同一段文本分别计算 `Hash32` 与 `Hash32Seed(…, 0x12345678)`，打印两个值；改动一个字节重算，验证指纹完全改变（雪崩效应的直观体验）。把两次改动的差值也算出来——平均而言约一半的比特翻转。

### 进阶：流式校验器

写 `hash_stream(path)`：分块读文件（每块 4KB）喂 SipHash 流式接口，返回指纹；对同一个文件再用一次性接口整读对拍。提示：两种姿势必须一致，这是你写增量校验的信心来源。

### 挑战：日志关联与排序

写一个小工具：生成 100 个 XID（`MakeMany` 批量），先乱序存进数组，再按 `Compare` 排序，最后逐个提取时间断言单调不减；模拟两条日志用同一 XID 文本关联（`Parse` 回二进制再 `Equal` 判断）。验收标准：排序后断言全部通过；同一文本解析出的两个二进制 `Equal` 为真、与另一条 XID 为假。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 确定性指纹 | `Hash32` / `Hash64` 默认种子；跨平台稳定 |
| 显式种子 | `Hash32Seed` / `Hash64Seed`：一致分片或防预计算 |
| 抗碰撞 | `SipHash` + 保密密钥；对外表键必用 |
| 流式 | `SipHashInit/Update/Final` 与一次性等价 |
| XID 结构 | 24 字节 = 时间在前；32 字符文本定长 |
| XID 闭环 | `Make/Write/Parse/Time`；解析错误带偏移 |
| 有序性 | 字典序 = 生成序；`Compare` 稳定全序 |
| 密码存储 | 不用本章任何函数——那是卷八 crypto 密码哈希的领域 |
