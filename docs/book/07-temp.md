---
num: 7
slug: temp
title: 临时内存：arena 与作用域
volume: 卷一 起步与核心
type: practice
lead: 分配是指针前移、释放是无事发生、回收是整段退回——解析与请求处理的标准内存姿态。
api: temp, memory
---

## 导读

第 5 章解决"长期拥有的内存"，本章解决它的对偶问题：**大量短命对象**。解析一帧 JSON 会产生上百个小片段，处理一个 HTTP 请求会拼几十个临时串——逐块 `xrtMalloc`/`xrtFree` 不仅慢，更糟的是失败路径要逐块清理，代码量爆炸。XRT 的答案是 arena（竞技场）：一段内存整体分配，内部"分配"只是指针前移，"释放"什么都不做，"回收"整段退回。全库的解析器、协议栈、协程调度全部内建 arena——理解本章，你就理解了 XRT 数据面的内存姿态。

## 引入

动手写过解析器的人都有这段记忆：递归下降到第三层，已经持有十几个临时缓冲；任何一层失败，都要把外层已建的缓冲逐个释放——每个 `return` 前面挂一串 `Free`，漏一个就是泄漏，多释放一个就是崩溃。而且这些对象的生命周期其实高度整齐：**它们都活到"这一帧解析结束"为止**。为一个整齐的生命周期付出逐块管理的代价，是纯粹的浪费。

arena 把这个观察变成机制：既然"一起生、一起死"，那就"一起分配、一起回收"。管理成本从 O(分配次数) 降到 O(作用域数)，失败路径的清理代码从一串 `Free` 变成一个 `End`。顺带还有一笔性能账：逐块分配要过分配器（即便是第 21 章的内存池也有簿记成本），arena 的分配只是一次指针加法——数据面热路径上百万次的临时分配，差距是量级性的。

## 概念

### 心智模型：三个动词

```diagram flow
- 分配：指针前移，返回区间起点——零元数据、零碎片
- 释放：不做任何事——中间状态从不逐块归还
- 回收：整段退回到书签——一个作用域一次
```

没有逐块释放，所以没有空闲块碎片；没有每块的簿记，所以分配接近数组寻址的成本。代价同样明确：**单个对象无法提前归还**——arena 只按作用域回收。判断一个场景适不适合 arena，就看对象生命周期是否整齐：解析一帧、处理一个请求、渲染一屏，都整齐；缓存、配置、会话这些长命对象不整齐，仍归第 5 章的拥有式分配管。

一条贯穿全书的分工线由此成形：**拥有式管"点名要长期活的"，arena 管"这一轮处理的"**。你会在后面每一卷看到这对组合：JSON 解析器在 arena 里搭临时树（第 28 章）、网络请求在 arena 里拼响应（卷七）、协程内建独立 arena（第 45 章）——arena 是 XRT 数据面的默认姿态，拥有式是例外而非常规。写代码时先问"这块内存能跟着作用域一起退回吗"，能就用 arena，再考虑别的。

### 作用域：书签与嵌套

`xrtTempBegin` 在当前水位打一个书签（`xtempmark`），之后的分配都落在书签之后；`xrtTempEnd` 把水位整段退回书签。书签可以嵌套——里层先退，外层后退，天然匹配递归下降与调用栈的形状。

### 提升：让结果活得比作用域久

arena 的经典难题：子作用域里拼好的**结果**（比如格式化好的一行日志）需要存活，但**过程量**（中间缓冲、临时片段）该全部退回。`xrtTempEndStr` / `xrtTempEndDup` 在回收区间之前把指定内容复制到父级——结果存活、其余退回，一步两得。这是"arena 不能选择性保留"的反例出口，也是本章示例的主角。

### 默认 arena 与显式 arena

两条使用路线。**默认 arena**：每个线程/协程自带一个（协程的 arena 随协程创建与回收，见卷六），`xrtTemp` 直接从当前上下文的 arena 分配，`xrtTempClear` 清空——适合"函数里随手拿块临时内存"。**显式 arena**（`xtemparena`）：`xrtTempInit` 创建、`xrtTempUnit` 销毁，嵌入请求对象、解析器、帧循环这些"有自己的生命周期"的结构里。显式 arena 的配置是三元组：常规块大小、保留水位（`Reset` 后留在手头的内存量）、总上限——超过上限的分配失败，防止失控请求把进程吃穿。密钥/令牌在 arena 里待过的场景，收尾用 `Secure` 系（`xrtTempSecureReset` / `xrtTempSecureUnit`）：先安全擦除用户区字节再回收，接上第 5 章 `xrtSecureZero` 的语义。

两条路线的选择标准是"谁拥有这批临时内存的生命周期"。线程整体一轮就清，默认 arena 最省心；请求对象有自己的进入与退出（还可能被池复用），就给 它配显式 arena，请求处理完 `Reset` 留水位、对象销毁时 `Unit` 归还全部。两种 arena 的 API 形态一致（`Alloc`/`Dup`/`Begin`/`End` 全家），切路线不改业务代码结构。

## 示例

### 第一个程序：作用域与提升的最小样本

先看一个 20 行的最小样本，把"书签—回收—提升"三件事各自说清：

```c
/* temp_mini.c —— 默认 arena 的作用域与提升 */
#define XRT_MODULE_TEMP_MEMORY
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	xtemparena* pArena = xrtTempCurrent();
	xtempmark tScope = xrtTempBegin(pArena);
	char* sTemp = (char*)xrtTemp(16);
	char* sKeep;

	memcpy(sTemp, "scoped", 7);
	/* 提升：回收前把结果复制到父级水位 */
	sKeep = xrtTempEndStr(&tScope, (xstrview){ sTemp, 7 });
	printf("kept=%s\n", sKeep != NULL ? sKeep : "(null)");
	xrtTempClear();
	return 0;
}
```

```term
$ gcc -O1 -DXRT_MODULE_TEMP_MEMORY -DXRT_IMPLEMENTATION -I single temp_mini.c -lws2_32 -liphlpapi
$ ./a.exe
kept=scoped
```

**刚才发生了什么。** `Begin` 后的 `sTemp` 活在书签区间内；`EndStr` 在退回区间前把 7 个字节复制到父级水位并返回新指针——`sTemp` 从此失效，`sKeep` 存活到 `Clear`。短短五行就把 arena 的两条核心纪律走了一遍：过程量随作用域退回、结果靠提升存活。

### 第二个完整程序：作用域、提升与显式 arena

来自仓库范例 `examples/memory/temp/main.c`，一次走完默认 arena 的作用域与提升、显式 arena 的创建与安全收尾：

```embed path="examples/memory/temp/main.c" title="examples/memory/temp/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/memory/temp/main.c -lws2_32 -liphlpapi
outer=outer inner=inner
outer_after_scope=outer
promoted=promoted
blocks=1 retained=1024 current=96 peak=96
```

**刚才发生了什么。** ① `xrtTempCurrent` 取当前线程的默认 arena，`xrtTemp(32)` 在父级水位分配——它不属于任何书签区间，`Clear` 之前一直有效。② `Begin` 打书签后分配 `inner`，打印验证两块共存；`End` 整段回收。③ 再打印 `outer`：完好——书签只回退自己区间之后的内存，父级分配不受影响，这就是"嵌套作用域"的安全保证。④ 第二个作用域演示**提升**：`xrtTempStr` 拼好 `"promoted"`，`xrtTempEndStr` 在回收前把它复制进父级——第三行输出证明结果存活，而区间内的其他分配（如果有）全部退回。⑤ 显式 arena 段：三元组 `{1024, 512, 2048}` 创建，`Alloc`/`Dup`/`EndDup` 各走一遍，`xrtTempGet` 读回统计——`blocks=1` 说明所有分配落在一个常规块内，`retained=1024` 是块持有量，`current=96` 与 `peak=96` 是当前与峰值水位。⑥ 最后 `SecureReset` → `Reset` → `Trim(0)` → `SecureUnit`：安全擦除、常规回收、归还全部保留块、销毁——显式 arena 的完整收尾序列。

值得注意的细节：`End` 之后 `sInner` 指针仍然"存在"（C 的指针不会消失），但指向的区间已被回收——继续使用就是悬空访问，本章坑 1 专门讲这个。另外 `Clear` 只出现在默认 arena 段的末尾：**谁管理作用域边界，谁负责回收**，函数中间不清、不清到底也没关系（线程退出时默认 arena 统一回收），但长跑循环里要警惕累积，见坑 2。

## 契约

- **谁 reset 谁负责**：作用域由 `Begin`/`End` 成对管理；默认 arena 的 `Clear` 由"拥有这轮处理"的成对管理者调用，业务函数内部不 `Clear`。
- **书签只管自己**：`End` 回收到书签，不影响更早的分配；嵌套按后进先出退回。
- **提升即复制**：`EndStr`/`EndDup` 的结果属于父级水位，与原区间无关联；返回前完成复制，无悬空窗口。
- **上限即防线**：显式 arena 三元组的上限超限即分配失败（第 4 章错误类别 `XERR_MEMORY`），与资源边界一样是业务规格的代码化。
- **敏感数据**：arena 待过密钥就用 `Secure` 系收尾；普通 `Reset` 不擦除用户区字节。

## 避坑

### 坑 1：把作用域内的分配存到作用域外

症状：跨函数传出的临时指针，调用方使用时内容"随机变化"或被后来的分配覆盖；复现依赖后续分配序列。

原因：`End` 回收了区间，之后的任何 arena 分配都可能复用这块内存——指针仍指向合法地址，但内容已不属于你。

```c bad
const char* build_label(void)
{
	xtempmark tScope = xrtTempBegin(xrtTempCurrent());
	char* s = (char*)xrtTemp(32);
	fill(s);
	xrtTempEnd(&tScope);
	return s;   /* 返回时区间已回收：调用方拿到的是待复用内存 */
}
```

```c good
const char* build_label(void)
{
	xtempmark tScope = xrtTempBegin(xrtTempCurrent());
	char* s = (char*)xrtTemp(32);
	fill(s);
	/* 提升到父级：复制后回收，返回值存活 */
	return xrtTempEndStr(&tScope, (xstrview){ s, strlen(s) + 1 });
}
```

### 坑 2：长跑循环里从不清理默认 arena

症状：服务的常驻线程内存缓慢上涨；`xrtTempGet` 看到 `current` 只增不减。

原因：默认 arena 的设计预期是"每轮处理有人 `Clear`"——如果每一轮都只分配不清，arena 会一直膨胀到上限（显式配置时）或持续增长（默认上限较宽时）。

```c bad
while ( running ) {
	char* sLine = (char*)xrtTemp(4096);   /* 每轮分配，从不 Clear */
	process(sLine);
}
```

```c good
while ( running ) {
	xtempmark tScope = xrtTempBegin(xrtTempCurrent());
	char* sLine = (char*)xrtTemp(4096);
	process(sLine);
	xrtTempEnd(&tScope);                  /* 每轮整段退回：水位恒定 */
}
```

## 练习

### 基础：作用域三段验证

参照范例写出三段输出：作用域内两块共存、`End` 后父级存活、提升结果存活。三行输出与范例逐字一致即完成。

### 进阶：递归下降的 arena 化

写一个递归函数解析括号表达式（如 `((a)(b))`）：每层递归 `Begin`/`End`，层内用 `xrtTemp` 存放本层片段，最内层结果用 `EndStr` 逐层提升到顶层。提示：提升发生在"本层返回前"，递归的自然结构就是嵌套书签。

### 挑战：给显式 arena 做上限测试

创建三元组为 `{1024, 512, 2048}` 的显式 arena，用第 6 章的统计或 `xrtTempGet` 验证：分配到接近上限时继续分配返回 `NULL` 且线程错误槽有 `XERR_MEMORY`；`Reset` 后恢复分配能力；`Trim(0)` 后 `retained` 归零。验收标准：三个断言全部打印 `ok`。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 心智模型 | 分配=指针前移；释放=无事；回收=整段退回；管理成本 O(作用域数) |
| 作用域 | `Begin` 打书签、`End` 退回；嵌套后进先出 |
| 提升 | `EndStr` / `EndDup`：结果复制到父级，过程量退回 |
| 默认 arena | `xrtTempCurrent` / `xrtTemp` / `xrtTempClear`；协程自带 |
| 显式 arena | `Init`/`Unit` 嵌入对象；三元组（块/水位/上限） |
| 敏感收尾 | `SecureReset` / `SecureUnit` 先擦除再回收 |
| 适配判断 | 生命周期整齐用 arena；长命对象用第 5 章拥有式 |
| 分工口诀 | arena 管这一轮的，拥有式管点名要长期活的 |
