# MemUnit 入门：什么时候该直接控制单个 256 槽内存单元，而不是上 `bsmm`、`fsmempool` 或 `mempool`

> 目标：把 `xrtMemUnitCreate()`、`xrtMemUnitAlloc()`、`xrtMemUnitFree()`、`xrtMemUnitFreeIdx()`、`xrtMemUnitGC_Mark()`、`xrtMemUnitGC()` 讲成第 2 阶段内存主线里的底层专题。读完这页后，你应该明确知道：`MemUnit` 到底解决什么问题、它为什么严格限制在单个 256 槽、它和 `bsmm / fsmempool / mempool` 的职责边界是什么，以及什么时候业务代码根本不该直接碰它。

[返回教学文档](README.md)

---

## 1. 为什么还要专门讲 `MemUnit`

学完 [BSMM 入门](bsmm-intro.md) 和 [FSMemPool 入门](mempool-fs-intro.md) 以后，很多人会问：

- 既然已经有更像业务对象池的方案了
- 为什么还要理解一个只有 256 槽的底层单元

答案很直接：

- `MemUnit` 不是“给大多数业务直接起手用的最终方案”
- 它是 XRT 固定大小对象池体系里最底的一层

你可以把这几层先这样分开：

| 层级 | 负责什么 |
|------|----------|
| `MemUnit` | 单个 256 槽固定对象单元 |
| `FSMemPool` | 组织很多 `MemUnit`，做四链表分类和正式对象池管理 |
| `MemPool` | 更广义的通用对象池 / 多尺寸池 |
| `BSMM` | 轻量固定大小块池，不等同于对象级池管理 |

所以讲 `MemUnit` 的目的，不是鼓励你“以后都手写 256 槽页管理”，而是让你把下面这些问题彻底看透：

- 固定大小对象最底层到底怎么分配
- 释放后的槽位为什么能 O(1) 复用
- 为什么对象头里会有 `ItemFlag`
- `FSMemPool` 到底在 `MemUnit` 之上多做了什么


## 2. 先把 9 个边界分清楚

### 2.1 `MemUnit` 就是一个单独的 256 槽页，不会自动扩容

这是这页最重要的前提。

一个 `MemUnit` 最多只能同时承载：

- 256 个固定大小对象

超过这个数量以后：

- `xrtMemUnitAlloc()` 会返回 `NULL`

所以它最适合：

- 你明确知道“一个单元就够”
- 你就是想自己控制一个精确的 256 槽页
- 你在写更高一层的池或缓存组件

它不适合：

- 对象总量不确定
- 业务负载会不断长
- 你不想手动扩展到多个单元

这类问题更接近：

- `fsmempool`
- 或 `mempool`


### 2.2 它仍然只适合固定大小对象

和 `bsmm`、`fsmempool` 一样，`MemUnit` 也只接受：

- 一种固定大小对象

创建时你给它：

- 用户对象大小 `iItemLength`

实际内部会自动再加上：

- `sizeof(MMU_Value)`

也就是对象头那 4 个字节的 `ItemFlag`。

这意味着：

- 返回给业务的是“跳过头部后的用户数据指针”
- 头部仍然属于运行时合同的一部分


### 2.3 当前正式签名里 `Create()` 要带 `iMode`

当前主线签名是：

```c
xrtMemUnitCreate(uint32 iItemLength, uint32 iMode)
```

最常见、也最稳的选择仍然是：

- `XRT_OBJMODE_LOCAL`

也就是：

- 这个单元默认属于当前线程
- 本线程分配 / 释放走快路径
- 错线程访问会被拒绝并设置当前线程错误

所以它不是：

- 一个默认就该跨线程共享的固定页


### 2.4 `Count` 是当前活对象数，`FreeCount` 是可复用槽位数

这一点和 `bsmm` 很不一样。

当前实现里：

- `Alloc()` 成功时
	- `Count++`
- `Free()` 成功时
	- `Count--`

所以 `Count` 表示的是：

- 当前还活着的对象数

另外还有两个紧密相关字段：

- `FreeCount`
	- 当前可复用槽位数
- `FreeOffset`
	- `FreeList` 环形队列头部偏移

还有一个很实用的细节：

- 当 `Count` 降到 0 时
	- 当前实现会把 `FreeCount` 和 `FreeOffset` 一起归零

也就是说，整个单元彻底空了以后：

- 它会回到“像新建时一样”的干净状态


### 2.5 释放复用靠的是 `FreeList` 环形队列

当前实现里，释放并不是：

- 立刻把槽位从数组里“抹掉”

而是会把索引压进：

- `FreeList[256]`

后面再分配时：

- 优先从 `FreeList` 里取回已释放槽位
- 没有可复用槽位时，才继续往新的索引走

所以 `MemUnit` 的核心优势之一就是：

- 释放后地址复用很快
- 分配 / 释放路径都是 O(1)


### 2.6 `Free()` 和 `FreeIdx()` 是两个不同层次的接口

你可以从两个角度释放：

- `xrtMemUnitFree(objUnit, p)`
	- 手里有对象指针
- `xrtMemUnitFreeIdx(objUnit, idx)`
	- 你明确知道对象槽位索引

大多数业务代码更适合：

- `Free()`

`FreeIdx()` 更适合：

- 你自己就在做底层池管理
- 你已经明确掌握索引
- 你想省掉一次从指针回推索引的步骤

所以不要为了“看起来更底层”就默认用 `FreeIdx()`。


### 2.7 对象头里的 `ItemFlag` 既记录状态，也记录索引

每个对象前面的头部里有：

- `MMU_FLAG_USE`
- `MMU_FLAG_GC`
- 低 8 位槽位索引
- 以及上层池下发的高位 Flag 前缀

这就是为什么：

- `Free()` 能从对象指针反推回索引
- `FSMemPool` 能从对象头再找到所属 `MemUnit`

这也意味着 3 个硬边界：

- 不要手工篡改对象头
- 不要把这个头当业务数据
- 不要把其他分配器得到的指针交给 `MemUnit` 去 `Free()`


### 2.8 `GC_Mark / GC` 是显式标记回收，不是自动 root 扫描

当前 `MemUnit` 也支持：

```c
#define xrtMemUnitGC_Mark(...)
xrtMemUnitGC(objUnit, bFreeMark)
```

它能做的是：

- 让你先给仍然要保留的对象打标
- 再在单个 256 槽页里跑一轮回收

它不能做的是：

- 自动发现 root
- 自动遍历你的对象图
- 自动知道哪个对象业务上还活着

所以它更像：

- “一个单元内部的显式 sweep 工具”

而不是：

- “完整 GC 系统”


### 2.9 大多数业务代码不应该把 `MemUnit` 当最终主线

这是这页必须反复强调的点。

如果你的问题是：

- 我有很多固定大小对象
- 数量不稳定
- 想要正式对象池

那优先考虑的通常是：

- `fsmempool`

如果你的问题是：

- 对象尺寸开始发散
- 想做更通用的池

那优先考虑的通常是：

- `mempool`

`MemUnit` 更像：

- 你需要一个可精确控制的 256 槽底层单元
- 你在实现更高一层的池
- 或你明确知道一个页就够


## 3. 最小 DEMO：先看懂 `Count / FreeCount` 和地址复用

第一步先别上 GC。

先把 `MemUnit` 最核心的 3 个行为看明白：

- 单元最多同时装 256 个对象
- `Count` 表示当前活对象数
- 释放后的槽位会通过 `FreeList` 很快复用

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
	int iID;
	int iValue;
} DemoItem;


int main(void)
{
	xmemunit pUnit = NULL;
	DemoItem* pItem1 = NULL;
	DemoItem* pItem2 = NULL;
	DemoItem* pItem3 = NULL;
	DemoItem* pItem4 = NULL;
	DemoItem* pFreed = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pUnit = xrtMemUnitCreate(sizeof(DemoItem), XRT_OBJMODE_LOCAL);
	if ( pUnit == NULL ) {
		goto cleanup;
	}

	pItem1 = (DemoItem*)xrtMemUnitAlloc(pUnit);
	pItem2 = (DemoItem*)xrtMemUnitAlloc(pUnit);
	pItem3 = (DemoItem*)xrtMemUnitAlloc(pUnit);
	if ( (pItem1 == NULL) || (pItem2 == NULL) || (pItem3 == NULL) ) {
		goto cleanup;
	}

	printf("首次分配后 Count=%u, FreeCount=%u\n", pUnit->Count, pUnit->FreeCount);

	pFreed = pItem2;
	if ( !xrtMemUnitFree(pUnit, pItem2) ) {
		goto cleanup;
	}
	pItem2 = NULL;

	printf("释放一个对象后 Count=%u, FreeCount=%u\n", pUnit->Count, pUnit->FreeCount);

	pItem4 = (DemoItem*)xrtMemUnitAlloc(pUnit);
	if ( pItem4 == NULL ) {
		goto cleanup;
	}

	printf("再次分配后 Count=%u, FreeCount=%u\n", pUnit->Count, pUnit->FreeCount);
	printf("被释放地址: %p\n", (void*)pFreed);
	printf("再次分配地址: %p\n", (void*)pItem4);
	printf("这两个地址通常相同，因为单元优先复用 FreeList 里的槽位。\n");

	iRet = 0;

cleanup:
	if ( pUnit != NULL ) {
		xrtMemUnitDestroy(pUnit);
	}
	xrtUnit();
	return iRet;
}
```

这个 DEMO 最值得观察的不是业务值，而是两个内部状态：

- `Count`
	- 活对象数会跟着增减
- `FreeCount`
	- 释放一个对象后增加
	- 被复用后再次减少


## 4. 升级 DEMO：自己维护 root，在单个 256 槽页里做一轮显式 GC

如果你理解了“这是单页级工具”，那下一步最值得看的就是：

- 当你明确知道 root 在哪里时
- 怎么在单个 `MemUnit` 内做一轮显式回收

```c
#include "xrt.h"
#include <stdio.h>

typedef struct DemoNode DemoNode;
struct DemoNode {
	int iID;
	DemoNode* pNext;
};


static void MarkReachable(DemoNode* pNode)
{
	if ( pNode == NULL ) {
		return;
	}

	xrtMemUnitGC_Mark(pNode);
	MarkReachable(pNode->pNext);
}


int main(void)
{
	xmemunit pUnit = NULL;
	DemoNode* arrNode[6] = { 0 };
	int iFreed = 0;
	int i = 0;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pUnit = xrtMemUnitCreate(sizeof(DemoNode), XRT_OBJMODE_LOCAL);
	if ( pUnit == NULL ) {
		goto cleanup;
	}

	for ( i = 0; i < 6; ++i ) {
		arrNode[i] = (DemoNode*)xrtMemUnitAlloc(pUnit);
		if ( arrNode[i] == NULL ) {
			goto cleanup;
		}

		arrNode[i]->iID = i;
		arrNode[i]->pNext = NULL;
	}

	arrNode[0]->pNext = arrNode[1];
	arrNode[1]->pNext = arrNode[2];
	arrNode[3]->pNext = arrNode[4];

	printf("GC 前 Count=%u\n", pUnit->Count);

	MarkReachable(arrNode[0]);
	iFreed = xrtMemUnitGC(pUnit, FALSE);

	printf("GC 回收了 %d 个对象\n", iFreed);
	printf("GC 后 Count=%u\n", pUnit->Count);
	printf("根链 0 -> 1 -> 2 会保留；未标记链 3 -> 4 会被回收。\n");

	iRet = 0;

cleanup:
	if ( pUnit != NULL ) {
		xrtMemUnitDestroy(pUnit);
	}
	xrtUnit();
	return iRet;
}
```

这里要记住 4 件事：

1. `GC(FALSE)` 是回收未标记对象。
2. 被保留对象上的 `GC` 位会在 sweep 时被自动清掉。
3. 这只是单个 256 槽页内的 sweep，不会替你找 root。
4. 如果对象数量已经可能跨页，这时更自然的主线通常已经是 `fsmempool`。


## 5. 什么时候该直接用 `MemUnit`

### 5.1 适合直接用

如果你满足下面这些条件，直接上 `MemUnit` 是合理的：

- 你明确只需要一个 256 槽单元
- 你在写底层组件，而不是一般业务模块
- 你需要精确观察 `Count / FreeCount / FreeOffset`
- 你想自己控制索引和对象头行为
- 你知道后面可能要把它包成更高一层池

### 5.2 不适合直接用

如果你一开始就知道下面这些事情成立，就别硬上：

- 数量会超过 256
- 想要自动扩展到多个单元
- 想把 GC、单元分类、备用页策略都交给库
- 你只是想“有个好用的固定对象池”

这时更合适的是：

- `fsmempool`

### 5.3 再往上升级

如果对象尺寸已经不固定，问题已经变成：

- 小对象、大对象、可变大小块混着来

那就该看：

- `mempool`


## 6. 常见错误

### 6.1 把 `MemUnit` 当“无限增长的小对象池”

错。  
它就是单个 256 槽页，不会自动扩容。

### 6.2 释放后还继续用旧指针

错。  
槽位很可能马上被复用，旧指针应立刻失效。

### 6.3 业务代码默认直接用 `FreeIdx()`

不推荐。  
除非你就是在写底层页管理，否则大多数时候直接 `Free()` 更稳。

### 6.4 把 `GC()` 理解成自动扫描整个对象图

错。  
它只做你已经显式标记过的单页 sweep。

### 6.5 忘了这层默认仍然有 owner-thread 边界

错。  
当前教学主线仍应从 `XRT_OBJMODE_LOCAL` 起步。


## 7. 建议继续阅读

- [MemUnit API](../api/api-memunit.md)
- [FSMemPool 入门：什么时候该用正式固定大小对象池，而不是 `bsmm`、`memunit` 或通用 `mempool`](mempool-fs-intro.md)
- [MemPool 入门：什么时候该用通用变长池，而不是 `fsmempool`、`memunit` 或直接 `malloc/free`](mempool-intro.md)
- [BSMM 入门：什么时候该用固定大小块池，而不是 `malloc/free`、数组或更重的对象池](bsmm-intro.md)
- [MemPool API](../api/api-mempool.md)
