---
num: 5
slug: memory
title: 内存基础：全局堆、对齐与分配器
volume: 卷一 起步与核心
type: practice
lead: 五个入口统一全库动态内存——分配、扩容、复制、安全清零与整体替换分配器。
api: memory, core
---

## 导读

第 3 章建立了"拥有与借用"的语言，本章给出"拥有"的物理出口：XRT 全库的动态内存都走五个入口——`xrtMalloc` / `xrtCalloc` / `xrtRealloc` / `xrtMemDup` / `xrtFree`，外加一个敏感场景专用的 `xrtSecureZero`。收口的价值不在语法糖，而在于它让三件不可能的事变成可能：**整体替换底层分配器**（对接自定义池或宿主运行时）、**逐笔观测**（第 6 章的统计与调试）、**精确故障注入**（验证失败路径）。读完本章你写的每一块内存都站在统一的出口上。

## 引入

一个真实的困境：你的程序要嵌入别人的大型系统，对方要求所有内存走它的分配器（它按模块记账、限额、回收）；同时你的代码已经写了几十处 `malloc`/`free`。逐处替换是灾难——于是你定义宏、批量替换、祈祷没有漏网。另一个困境：测试时你想知道"如果第 1000 次分配失败，程序能不能正确回滚"——散装的 `malloc` 根本没有注入点。

两个困境指向同一个设计答案：内存入口必须**收口**，且收口要发生在库的层面而不是编码纪律层面。XRT 把全库动态内存收敛到五个函数，替换、观测、注入因此都是全局开关级的一行操作。

收口还带来第三笔账：**对齐与布局的统一保证**。五个入口的返回指针按最大基本对齐分配，可以直接存放任何标量、结构乃至 SIMD 向量，你不需要为"这个缓冲要放 float 数组，要不要 aligned_alloc"这类问题分层讨论；而所有 XRT 容器（卷三）内部也走同一批入口——容器里的元素对齐由库保证，不依赖你传入的分配器碰巧做了什么。这一章建立的出口，是后面每一卷的容器、解析器、网络缓冲共同的物理地基。

## 概念

### 五个入口的语义

| 入口 | 语义 | 关键细节 |
| --- | --- | --- |
| `xrtMalloc` | 分配（拥有式） | 与标准 malloc 语义一致 |
| `xrtCalloc` | 按元素数 × 大小分配并清零 | 溢出检查与标准一致 |
| `xrtRealloc` | 扩容/缩容 | **失败返回 NULL 且原块仍有效** |
| `xrtMemDup` | 复制一段内存为新的拥有式分配 | 分配 + memcpy 一步完成 |
| `xrtFree` | 释放 | **允许传 NULL**，无需判空 |
| `xrtSecureZero` | 安全清零 | 不会被编译器当作死存储删除 |

前五个都是拥有式约定：**谁取得返回值谁负责释放，恰好一次**。`xrtFree(NULL)` 合法这一点写起来很小，读起来很值——清理代码从此不用层层判空。

### 分配器替换：一次决策，全局生效

```diagram flow
- 业务代码：xrtMalloc 等五个入口
- 全局收口：唯一通道，携带可替换的分配器表
- 分配器表：xrtSetAllocator 在首次分配前替换
- 系统堆：默认后端；可换成宿主分配器/自定义池
```

`xallocator` 结构体携带分配、扩容、释放三个回调与上下文指针；`xrtSetAllocator` 把整张表换掉，`xrtGetAllocator` 复制当前表。**替换只允许在首次分配之前**——之后表被永久锁定，再次替换返回失败。这个"锁早"的约定换来了一个宝贵性质：运行期间分配器恒定，所有分配路径都不必考虑"分配器中途变了"的竞态，也不需要给热路径加锁。

### 位置族：给分配带上出生地

`xrtMallocAt` / `xCallocAt` / `xReallocAt` / `xFreeAt` / `xMemDupAt` 是携带 `__FILE__` / `__LINE__` 的调试入口——普通的 `xrtMalloc` 宏在启用调试模块时会展开到 At 族，让每笔分配带位置。它是第 6 章泄漏检测的基石，本章只需知道"位置信息是收口免费附赠的"。

### 什么时候真的需要换分配器

三种典型场景值得提前认识。**宿主嵌入**：你的模块跑在别人的进程里（游戏引擎、脚本运行时），宿主要求所有内存走它的记账——把宿主的分配函数包成三回调，启动最早期替换，本章第二个示例就是完整模板。**配额与限额**：多租户服务按模块限额，自定义分配器在上下文里累计字节数、超限返回 `NULL`——配合第 4 章错误模型，超限行为与普通 OOM 一致，业务代码无需特判。**实测对比**：性能调优时想 A/B 对比不同后端（第 22 章的内存池就是现成候选），替换一行即可切换。共同前提都一样：因为"首次分配前锁定"，替换必须发生在 `main` 的最早期——放在配置读取之后都可能太晚。

## 示例

### 完整程序：五个入口走一遍

来自仓库范例 `examples/core/memory/main.c`——分配、扩容、复制、安全清零、释放的完整生命周期：

```embed path="examples/core/memory/main.c" title="examples/core/memory/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/core/memory/main.c -lws2_32 -liphlpapi
value=42 copied=yes
```

**刚才发生了什么。** ① `xrtCalloc(4, 1)` 分配并清零，写入标记值 42。② `xrtRealloc` 扩到 16 字节——语义要点在失败侧：返回 `NULL` 时**原块仍有效**，本例为简洁直接退出，健壮写法见本章坑 1。③ `xrtMemDup` 把静态源复制成独立拥有式缓冲，`memcmp` 验证逐字节一致；注意失败路径先释放已持有的 `pValues` 再退出——"失败不留半成品"从第一章就开始实践，这也是第 4 章错误模型与本章所有权约定协同的标准形状。④ `xrtSecureZero` 清零两块缓冲：它针对的是编译器的"死存储删除"优化——普通 `memset` 在随后的 `free` 之前可能被判定无意义而整段删掉，敏感数据（密钥、令牌）就此泄漏到复用的堆内存里；`xrtSecureZero` 用易失性写或屏障保证清零真实发生。⑤ 释放顺序与持有顺序相反，`xrtFree` 无需判空。

### 完整程序：替换分配器并观察锁定

来自 `examples/core/allocator_tour/main.c`——实现一个计数分配器，在首次分配前替换，验证"锁早"约定：

```embed path="examples/core/allocator_tour/main.c" title="examples/core/allocator_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/core/allocator_tour/main.c -lws2_32 -liphlpapi
allocator: swap -> at-family alloc/calloc/dup/realloc ok
allocator: locked after first alloc -> swap rejected ok
```

**刚才发生了什么。** ① 计数分配器仍路由到系统堆，只是记账——`exampleAlloc` 等三个回调填进 `xallocator` 表，`main` 一进来就 `xrtSetAllocator`。② 替换后用 At 族做一轮分配，全走新表。③ 第二次替换被拒绝：首次分配之后表已锁定，调用返回失败而不是静默忽略——错误信号明确，"分配器恒定"的不变量成立。这个模式就是嵌入宿主运行时的标准接法：把宿主的分配函数包成三回调，启动最早期完成替换。

顺带读一遍范例的实现细节，有三个值得偷师的地方。回调签名全部携带上下文指针（`exampleAlloc(ptr pContext, size_t iSize)`）——分配器自己的状态不放全局变量，多个分配器实例互不干扰。计数状态放在结构体里、由上下文带回——这与第 3 章"回调用户数据"的模式完全同构，你会反复看到。最后注意替换发生在任何 XRT 调用之前：`main` 的第一件事就是 `xrtSetAllocator`，连 `printf` 都排在后面——"最早期"不是修辞，是硬性时序。

## 契约

- **拥有式**：五个入口返回的指针恰好释放一次；`xrtFree(NULL)` 合法；清理代码不需要判空层层包裹。
- **Realloc 失败保全**：失败返回 `NULL`，原块内容与有效性不变；成功后旧指针作废，不得再引用。
- **SecureZero 不可省略**：敏感内存清零必须用它；`memset` 可能被优化删除，且这个风险只在 release 构建暴露。
- **分配器锁定**：`xrtSetAllocator` 仅首次分配前生效；运行期分配器恒定，热路径无锁；"最早期"指 `main` 的第一件事。
- **对齐**：入口按最大基本对齐分配，返回指针可用于任何标量、结构与向量类型；容器元素对齐由库保证。
- **回调形态**：分配器三回调与全库回调一致——首参上下文指针携带状态，不放全局变量。

## 避坑

### 坑 1：Realloc 直接覆盖原指针

症状：偶发内存泄漏（失败分支），压力测试或 OOM 注入时泄漏量放大。

原因：`xrtRealloc` 失败返回 `NULL` 且**原块仍有效**——用返回值直接覆盖原指针，失败瞬间原指针丢失，那块内存再也无人释放。

```c bad
pValues = (unsigned char*)xrtRealloc(pValues, 4096);
if ( pValues == NULL ) {
	return 1;   /* 原块指针已被覆盖——4096 失败前的那块永远泄漏 */
}
```

```c good
unsigned char* pNew = (unsigned char*)xrtRealloc(pValues, 4096);
if ( pNew == NULL ) {
	xrtFree(pValues);   /* 原块仍有效：按失败路径正常清理 */
	return 1;
}
pValues = pNew;         /* 成功才接管新指针 */
```

### 坑 2：用 memset 清零密钥

症状：安全审计在堆转储或后续复用的内存里发现密钥残片；release 构建比 debug 构建更容易出现。

原因：编译器看到 `memset(key, 0, n)` 之后紧接 `free(key)`，判定这次清零是"死存储"并整段删除——标准允许的优化，却把敏感数据留在了堆里。

```c bad
memset(pSecretKey, 0, iKeySize);
xrtFree(pSecretKey);   /* memset 大概率被优化删除——密钥残留在堆内存 */
```

```c good
xrtSecureZero(pSecretKey, iKeySize);
xrtFree(pSecretKey);   /* 清零动作保证真实发生 */
```

## 练习

### 基础：五入口清单

参照 `examples/core/memory/main.c` 写一个程序：Calloc 一段、Realloc 扩容并验证旧内容保留、MemDup 复制并 memcmp 验证、最后逆序释放。输出全部验证结果。

### 进阶：计数分配器

实现一个"字节计数"分配器（记录当前活跃字节数），替换后跑一段分配/释放混合代码，结尾打印活跃字节数与泄漏判断。提示：三个回调里用上下文指针携带计数状态；分配加、释放减；`realloc` 回调收到旧指针时先减旧尺寸——旧尺寸怎么拿？最简单的办法是分配时额外多分配一个头部存尺寸，回调里读回。做完你会更体会"分配器是一个完整后端"的含义。

### 挑战：验证锁定时序

写程序验证 `xrtSetAllocator` 的锁定边界：先替换成功，做一次分配，再替换必须失败。然后回答：如果 XRT 允许运行期替换，你的计数分配器需要加什么保护？（提示：想想两个线程同时分配时，分配器表本身的读写是否需要锁。）验收标准：两个替换的返回值一真一假，输出与注释一致。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 五入口 | Malloc / Calloc / Realloc / MemDup / Free（NULL 合法，清理免判空） |
| 拥有式 | 谁取得谁释放，恰好一次；失败路径同样要清理 |
| Realloc | 失败 `NULL` 且原块有效——先接新指针再覆盖 |
| 敏感清零 | `xrtSecureZero`，永不依赖 `memset` |
| 分配器 | `xrtSetAllocator` 首次分配前替换；此后永久锁定；替换放 `main` 第一件事 |
| 位置族 | `xrtMallocAt` 等携带 `__FILE__`/`__LINE__`，调试模块的基石 |
| 换后端场景 | 宿主嵌入（记账）、配额限额（超限同 OOM）、后端 A/B 对比 |
