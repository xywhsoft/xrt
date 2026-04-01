# MemPool 入门：什么时候该用通用变长池，而不是 `fsmempool`、`memunit` 或直接 `malloc/free`

> 目标：把 `xrtMemPoolCreate()`、`xrtMemPoolInit()`、`xrtMemPoolAlloc()`、`xrtMemPoolFree()`、`xrtMemPoolGC_Mark()`、`xrtMemPoolGC()` 讲成第 2 阶段内存主线里的“通用变长池”专题。读完这页后，你应该明确知道：当前 `mempool` 不是旧文档里的“预设二叉树 1/2 模式”，而是 `cutoff + 16 字节分桶 + BigMM 兜底` 的现行模型；同时也要知道，它虽然能统一管理不同大小的块，但仍然不是“什么都往里丢就行”的无限通用容器。

[返回教学文档](README.md)

---

## 1. 为什么 `mempool` 应该接在 `fsmempool` 后面讲

学完 [MemUnit 入门](memunit-intro.md) 和 [FSMemPool 入门](mempool-fs-intro.md) 以后，你已经知道：

- 固定大小对象为什么适合高频复用
- 单个 `MemUnit` 为什么只负责 256 槽
- `fsmempool` 为什么要用四链表管理很多 `MemUnit`

但真实程序很快还会继续碰到这组问题：

- 这次对象尺寸已经开始发散了
- 同一条业务链里既有 32 字节的小块，也有 200、400、900 字节的数据
- 我不想为每一种尺寸单独建一个 `fsmempool`
- 但我也不想重新退回“全靠 `malloc/free` 顶着”

这时问题就已经从：

- “我要不要正式固定大小对象池”

升级成了：

- “我要不要一个能统一管理多种大小分配的通用池”

这正是 `mempool` 的位置。

可以先把 4 条主线这样粗分：

| 方案 | 最适合的问题 |
|------|--------------|
| `MemUnit` | 我就要直接控制一个 256 槽固定大小单元 |
| `FSMemPool` | 固定大小对象长期复用，想让池自己管单元状态 |
| `MemPool` | 对象尺寸开始发散，但仍希望统一池化和统一 GC |
| `malloc/free` | 简单、偶发、边界明确，或要和外部库直接对接 |


## 2. 先把 9 个边界分清楚

### 2.1 `mempool` 解决的是“变长分配的统一池化”，不是无限通用容器

它能做的是：

- 在同一个池里管理多种大小的块
- 小块走分桶池化路径
- 大块走回退路径
- 统一 `Free()` 和统一 GC

它不能做的是：

- 自动理解你的业务对象图
- 自动帮你决定对象所有权
- 自动跨线程安全共享所有路径
- 自动变成“通用对象数据库”

所以它适合：

- 一条业务链里尺寸发散但生命周期接近
- 想减少零碎 `malloc/free`
- 想保留显式标记回收入口

它不适合：

- 每个块都来自完全不同的生命周期域
- 你根本不想维护 root 或 GC 边界
- 只是偶尔分配几个对象，没必要引入池


### 2.2 当前主线不是旧文档里的“预设二叉树 1/2 模式”

这是这页最重要的纠偏点。

当前源码里的 `mempool` 主线是：

- `iCustom` 表示 fallback cutoff
- `iBucketStep` 固定是 `16`
- `1 .. cutoff` 的大小范围按 16 字节一档做 LUT 分桶
- `> cutoff` 的分配走大块回退路径

也就是：

- `1..16`
- `17..32`
- `33..48`
- ...

一直排到当前池的 `cutoff`。

所以你现在应该把 `mempool` 理解成：

- LUT 分桶池
- 不是旧文档里那个“选 1 或 2，就切到两棵预设二叉树”的模型

头文件里还保留了：

- `FSB_RootNode`
- `left / right`

但它们在当前主线里更接近：

- 兼容旧结构布局的遗留字段

而不是：

- 你今天还应该围绕它们理解分配路径


### 2.3 `iCustom <= 0` 代表默认 cutoff，当前默认值是 `1024`

当前正式签名是：

```c
xrtMemPoolCreate(int iCustom, uint32 iMode)
xrtMemPoolInit(xmempool objMP, int iCustom, uint32 iMode)
```

这里的 `iCustom` 不是旧文档里的“配置 1 / 配置 2”。

当前应按下面规则理解：

- `iCustom <= 0`
	- 使用默认 cutoff
	- 当前默认值是 `1024`
- `iCustom > 0`
	- 直接把它当作自定义 cutoff

举例：

- `xrtMemPoolCreate(0, XRT_OBJMODE_LOCAL)`
	- 当前等价于“1..1024 走分桶池化，1024 以上走回退”
- `xrtMemPoolCreate(256, XRT_OBJMODE_LOCAL)`
	- 当前等价于“1..256 走分桶池化，256 以上走回退”


### 2.4 `<= cutoff` 走池化桶，`> cutoff` 走 `BigMM` 回退

当前分配路径可以先粗看成两段：

#### 第一段：池化分桶路径

如果 `iSize <= iFallbackCutoff`：

- 通过 `FSB_Lut` 算出所属 bucket
- bucket 内部再像 `fsmempool` 一样管理很多 `MemUnit`
- 实际对象仍然由 `MemUnit` 分配

这意味着小块路径仍然继承了：

- 256 槽单元
- 地址复用
- `LL_Idle / LL_Full / LL_Null / LL_Free`

#### 第二段：大块回退路径

如果 `iSize > iFallbackCutoff`：

- 直接走大块分配
- 用 `BigMM` 记录元数据
- 返回给业务的是跳过 `MP_MemHead` 后的用户指针

这意味着 `mempool` 不是“所有大小都硬塞进同一种池单元”。  
它是：

- 小块池化
- 大块兜底


### 2.5 每个 bucket 仍然是“很多 `MemUnit` + 四链表”这一套

虽然 `mempool` 是通用变长池，但小块路径并没有绕开固定大小对象池模型。

每个 bucket 仍然维护：

- `LL_Idle`
- `LL_Full`
- `LL_Null`
- `LL_Free`

所以当前实现的核心不是：

- 每次按大小单独 `malloc`

而是：

- 把相近大小的请求收拢到同一个 bucket
- bucket 再复用很多固定大小 `MemUnit`

这就是为什么：

- `32` 字节和 `31` 字节请求会落到同一档
- `33` 字节请求则会进入下一档


### 2.6 当前正式起点仍然应该是 `XRT_OBJMODE_LOCAL`

和前几页一样，当前正式教学主线仍然建议从：

- `XRT_OBJMODE_LOCAL`

起步。

也就是：

- 这个池默认属于当前线程
- 本线程走快路径
- 错线程访问会被拒绝并设置当前线程错误

所以不要一上来就把它理解成：

- 一个天然跨线程共享的通用分配器


### 2.7 `Free()` 的语义要分清：小块回桶，大块是“释放内存 + 复用元数据槽”

这是 `mempool` 很容易讲歪的一点。

对池化小块来说：

- `xrtMemPoolFree()` 会回到对应 `MemUnit`
- 后续分配很可能直接复用同一地址

对大块回退来说：

- 实际大块内存会立刻 `xrtFree()`
- `LL_BigFree` 复用的是 `MP_BigInfoLL` 这类元数据槽
- 不是把整块大内存一直缓存着不放

所以 `mempool` 的“大块复用”不要理解成：

- “上次那块 4KB 内存下次原样还给我”

更准确的理解是：

- 大块路径复用的是跟踪槽位
- 真正的大内存仍会被及时释放


### 2.8 `GC` 会同时扫 pooled bucket 和大块回退区，但不会替你追 root

当前接口是：

```c
#define xrtMemPoolGC_Mark xrtMemUnitGC_Mark
XXAPI void xrtMemPoolGC(xmempool objMP, bool bFreeMark);
```

当前语义要按下面理解：

- `xrtMemPoolGC(objMP, FALSE)`
	- 回收未标记对象
	- 保留已标记对象
	- 并清掉幸存对象上的 `GC` 位
- `xrtMemPoolGC(objMP, TRUE)`
	- 回收已标记对象

而且这套语义会同时覆盖：

- pooled bucket 里的对象
- `BigMM` 路径上的大块对象

但它仍然不会替你做这件事：

- 自动从 root table、哈希表、链表、图结构里找出还活着的对象

也就是说，`mempool` 的 GC 本质上仍然是：

- 显式标记
- 统一 sweep


### 2.9 这 4 种方案要真正分开看

一个最实用的选型表是：

| 方案 | 最适合的问题 | 不适合的问题 |
|------|--------------|--------------|
| `MemUnit` | 你要直接控制单个 256 槽页 | 数量会跨页、尺寸会发散 |
| `FSMemPool` | 固定大小对象长期复用 | 同一池里混放多种尺寸 |
| `MemPool` | 尺寸发散但仍想统一池化和统一 GC | 简单偶发分配，或生命周期完全无关 |
| `malloc/free` | 简单直观、边界明确、跨库交互 | 高频零碎分配、需要统一收口 |


## 3. 最小 DEMO：先看懂默认 cutoff、桶数量和大块兜底

第一步先别急着上 GC。

先把 `mempool` 最核心的 4 个事实看明白：

- `0` 代表默认 cutoff
- 当前默认 cutoff 是 `1024`
- bucket 步长是 `16`
- 超出 cutoff 的块会走大块兜底

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>


int main(void)
{
	xmempool pPool = NULL;
	char* sSmall = NULL;
	char* sMedium = NULL;
	char* sBig = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pPool = xrtMemPoolCreate(0, XRT_OBJMODE_LOCAL);
	if ( pPool == NULL ) {
		goto cleanup;
	}

	printf("fallback cutoff = %u\n", pPool->iFallbackCutoff);
	printf("bucket step     = %u\n", pPool->iBucketStep);
	printf("bucket count    = %u\n", pPool->iBucketCount);

	sSmall = (char*)xrtMemPoolAlloc(pPool, 32);
	sMedium = (char*)xrtMemPoolAlloc(pPool, 240);
	sBig = (char*)xrtMemPoolAlloc(pPool, 2048);
	if ( (sSmall == NULL) || (sMedium == NULL) || (sBig == NULL) ) {
		goto cleanup;
	}

	strcpy(sSmall, "small bucket block");
	strcpy(sMedium, "medium bucket block");
	strcpy(sBig, "big fallback block");

	printf("32 bytes   -> %p\n", (void*)sSmall);
	printf("240 bytes  -> %p\n", (void*)sMedium);
	printf("2048 bytes -> %p\n", (void*)sBig);
	printf("arrMMU.Count = %u\n", pPool->arrMMU.Count);
	printf("BigMM.Count  = %u\n", pPool->BigMM.Count);
	printf("2048 大于默认 cutoff，所以它会进入 BigMM 回退路径。\n");

	xrtMemPoolFree(pPool, sSmall);
	xrtMemPoolFree(pPool, sMedium);
	xrtMemPoolFree(pPool, sBig);
	sSmall = NULL;
	sMedium = NULL;
	sBig = NULL;

	iRet = 0;

cleanup:
	if ( pPool != NULL ) {
		xrtMemPoolDestroy(pPool);
	}
	xrtUnit();
	return iRet;
}
```

这个 DEMO 里最值得盯住的不是字符串内容，而是这 3 个内部量：

- `iFallbackCutoff`
	- 当前池的小块上限
- `arrMMU.Count`
	- pooled bucket 一侧已经开过多少 `MMU_LLNode`
- `BigMM.Count`
	- 大块回退元数据已经登记过多少槽位


## 4. 升级 DEMO：小对象和大 payload 混在同一个池里，然后做一轮显式 GC

真正把 `mempool` 和 `fsmempool` 拉开距离的，是这一类场景：

- 小对象结构体本身尺寸不大
- 但对象挂着长度发散的 payload
- 你又希望这两类内存仍然归同一个池统一收口

下面这个例子里：

- `DemoDoc` 结构体本身会走 pooled bucket
- `sPayload` 有的很小，有的已经超过自定义 cutoff
- root 只保留 `0 -> 1`
- `2 -> 3` 没有 root 指向
- 一轮 `GC(FALSE)` 之后，未标记的结构体和 payload 都会被回收

```c
#include "xrt.h"
#include <stdio.h>

typedef struct DemoDoc DemoDoc;
struct DemoDoc {
	int iID;
	char* sPayload;
	DemoDoc* pNext;
};


static char* MakePayload(xmempool pPool, int iSize, char cFill)
{
	char* sText = (char*)xrtMemPoolAlloc(pPool, (uint32)iSize);
	int i = 0;

	if ( sText == NULL ) {
		return NULL;
	}

	for ( i = 0; i < (iSize - 1); ++i ) {
		sText[i] = cFill;
	}
	sText[iSize - 1] = '\0';
	return sText;
}


static void MarkReachable(DemoDoc* pDoc)
{
	while ( pDoc != NULL ) {
		xrtMemPoolGC_Mark(pDoc);
		if ( pDoc->sPayload != NULL ) {
			xrtMemPoolGC_Mark(pDoc->sPayload);
		}
		pDoc = pDoc->pNext;
	}
}


int main(void)
{
	xmempool pPool = NULL;
	DemoDoc* arrDoc[4] = { 0 };
	DemoDoc* pRoot = NULL;
	int i = 0;
	int iRet = 1;
	const int arrPayloadSize[4] = { 48, 96, 192, 320 };

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pPool = xrtMemPoolCreate(128, XRT_OBJMODE_LOCAL);
	if ( pPool == NULL ) {
		goto cleanup;
	}

	for ( i = 0; i < 4; ++i ) {
		arrDoc[i] = (DemoDoc*)xrtMemPoolAlloc(pPool, sizeof(DemoDoc));
		if ( arrDoc[i] == NULL ) {
			goto cleanup;
		}

		arrDoc[i]->iID = i;
		arrDoc[i]->pNext = NULL;
		arrDoc[i]->sPayload = MakePayload(pPool, arrPayloadSize[i], (char)('A' + i));
		if ( arrDoc[i]->sPayload == NULL ) {
			goto cleanup;
		}
	}

	arrDoc[0]->pNext = arrDoc[1];
	arrDoc[2]->pNext = arrDoc[3];
	pRoot = arrDoc[0];

	printf("GC 前 BigMM.Count=%u\n", pPool->BigMM.Count);
	printf("cutoff=128，所以 192 和 320 这两个 payload 会走大块回退路径。\n");

	MarkReachable(pRoot);
	xrtMemPoolGC(pPool, FALSE);

	printf("GC 后仍保留 root 链 0 -> 1，以及它们的 payload。\n");
	printf("未标记链 2 -> 3 及其 payload 已被回收。\n");

	iRet = 0;

cleanup:
	if ( pPool != NULL ) {
		xrtMemPoolDestroy(pPool);
	}
	xrtUnit();
	return iRet;
}
```

这段 DEMO 最想让你记住 4 件事：

1. `DemoDoc` 本体和 `sPayload` 都在同一个池里，但尺寸路径可能不同。
2. root 只标记结构体还不够，结构体拥有的 payload 也要标。
3. `GC(FALSE)` 才是最常见的 mark-sweep 路线。
4. `mempool` 能统一收口 pooled 小块和 fallback 大块，但不会自动帮你遍历对象图。


## 5. 它和 `fsmempool`、`MemUnit`、`malloc/free` 的区别

### 5.1 和 `MemUnit` 的区别

- `MemUnit`
	- 就是一个固定大小、单页、256 槽单元
- `MemPool`
	- 先按大小分 bucket
	- bucket 里再组织很多 `MemUnit`
	- 超大块还会走 `BigMM`

所以 `MemUnit` 更像底层部件，`mempool` 才是变长分配的完整方案。

### 5.2 和 `fsmempool` 的区别

- `fsmempool`
	- 整个池只有一种固定尺寸
	- 适合对象结构稳定的场景
- `mempool`
	- 同一个池里允许多种尺寸
	- 更适合 payload、文本块、混合对象尺寸

如果你的对象尺寸完全稳定，优先仍然是：

- `fsmempool`

### 5.3 和 `malloc/free` 的区别

- `malloc/free`
	- 简单直接
	- 不需要池对象
	- 也没有统一 GC 收口
- `mempool`
	- 需要你维护池边界
	- 但换来统一分配、统一释放路径和统一 GC 入口

如果只是偶尔分配几块内存，就别硬上池。  
如果同一业务链里一直在零碎地分配可变大小块，`mempool` 才开始显得值。


## 6. 如果只有一条主线程，会卡在哪里

这页虽然是内存主题，但也必须把“单主线程”的问题说清楚。

`mempool` 不能解决的是：

- 网络等待
- 文件 I/O 阻塞
- 一条主线程本身的调度瓶颈

它能缓解的是：

- 高频变长分配带来的碎片和抖动
- 零散小块与中块混合创建 / 释放时的成本波动
- 一批相关对象难以统一收口的问题

也就是说，`mempool` 解决的是：

- 内存组织问题

不是：

- 并发调度问题

如果你的瓶颈是：

- 主线程在等待网络或磁盘

那要回到：

- [多任务总论](multitask-overview.md)


## 7. 常见错误

### 7.1 继续按旧文档理解成“配置 1 / 配置 2 二叉树”

错。  
当前主线是 cutoff + 16 字节分桶 + LUT。

### 7.2 以为 `LL_BigFree` 会缓存整块大内存

错。  
当前复用的是大块元数据槽，真正的大内存会被及时释放。

### 7.3 只标记结构体，不标记结构体拥有的 payload

错。  
`mempool` 不会替你自动追踪对象图。

### 7.4 把 `mempool` 当成自动跨线程共享分配器

错。  
当前正式主线仍然应该先从 `XRT_OBJMODE_LOCAL` 理解。

### 7.5 尺寸稳定却仍然硬上 `mempool`

不推荐。  
如果对象尺寸固定，`fsmempool` 更简单，也更贴近问题本身。


## 8. 建议继续阅读

- [MemPool API](../api/api-mempool.md)
- [FSMemPool 入门：什么时候该用正式固定大小对象池，而不是 `bsmm`、`memunit` 或通用 `mempool`](mempool-fs-intro.md)
- [MemUnit 入门：什么时候该直接控制单个 256 槽内存单元，而不是上 `bsmm`、`fsmempool` 或 `mempool`](memunit-intro.md)
- [BSMM 入门：什么时候该用固定大小块池，而不是 `malloc/free`、数组或更重的对象池](bsmm-intro.md)
- [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)
