# FSMemPool 入门：什么时候该用正式固定大小对象池，而不是 `bsmm`、`memunit` 或通用 `mempool`

> 目标：把 `xrtFSMemPoolCreate()`、`xrtFSMemPoolInit()`、`xrtFSMemPoolAlloc()`、`xrtFSMemPoolFree()`、`xrtFSMemPoolGC_Mark()`、`xrtFSMemPoolGC()` 讲成第 2 阶段内存主线里的第二个正式对象池专题。读完这页后，你应该明确知道：什么时候你的问题已经不只是“反复拿几块固定大小内存”，而是“我要长期维护一批固定大小对象，并且希望池子自己管理单元、空闲状态和 GC 收口”。

[返回教学文档](README.md)

---

## 1. 为什么 `fsmempool` 应该接在 `bsmm` 后面讲

学完 [BSMM 入门](bsmm-intro.md) 以后，你已经知道：

- 固定大小块为什么适合高频创建 / 释放
- 地址复用为什么比频繁 `malloc/free` 更稳定
- 为什么轻量页池能让对象分配更平滑

但很多真实程序很快会继续遇到这几个问题：

- 我不只要“拿到一块内存”，还要维护很多固定大小对象
- 对象数量已经超过单个 256 槽单元
- 我希望池子自己决定哪些单元可继续分配、哪些单元已满、哪些单元应该保留备用
- 我还希望后面能做一轮显式标记回收

这时，问题就已经从：

- “要不要一个轻量固定块池”

升级成了：

- “我要不要一个正式的固定大小对象池管理器”

这正是 `fsmempool` 的位置。

可以先把它理解成：

- `MemUnit`
	- 一个 256 槽的小型固定对象单元
- `BSMM`
	- 帮你管理很多 `MMU_LLNode` 槽位
- `FSMemPool`
	- 把很多 `MemUnit` 组织成一个长期可复用、可 GC、可自动分类的正式对象池


## 2. 先把 9 个边界分清楚

### 2.1 `fsmempool` 仍然只适合“固定大小对象”

这一点和 `bsmm` 一样，仍然是前提。

创建池时你只告诉它：

- 每个对象占多少字节

后面每次分配出来的对象尺寸都相同。

所以它很适合：

- 会话对象
- 语法树节点
- 图节点
- 缓存条目
- 自己维护生命周期的小对象

它不适合：

- 同一个池里混放很多不同尺寸的对象
- 某些对象还要动态拼大字符串或大二进制块

这类问题更接近：

- `mempool`
- 或直接 `malloc/free`


### 2.2 `fsmempool` 的核心不是“再套一层壳”，而是把三层职责分开

这页最重要的理解方式是：

- `MemUnit`
	- 真正存 256 个固定大小对象
	- 每个对象前面带 `ItemFlag`
- `BSMM(arrMMU)`
	- 管很多 `MMU_LLNode`
	- 让池可以不断增减 `MemUnit` 节点，而不是只守着一个单元
- `FSMemPool`
	- 维护 `LL_Idle / LL_Full / LL_Null / LL_Free`
	- 负责把 `MemUnit` 放进正确的工作状态

所以 `fsmempool` 不是“比 `bsmm` 更快”这么简单。  
它真正多出来的是：

- 单元状态管理
- 对象级 `Free()`
- 备用空单元策略
- 标记回收入口


### 2.3 当前正式签名里 `Create / Init` 都要带 `iMode`

当前主线签名是：

```c
xrtFSMemPoolCreate(uint32 iItemLength, uint32 iMode)
xrtFSMemPoolInit(xfsmempool objMM, uint32 iItemLength, uint32 iMode)
```

对大多数教学和业务代码，最稳的起点仍然是：

- `XRT_OBJMODE_LOCAL`

也就是：

- 这个对象池默认属于当前线程
- 本线程走快路径
- 错线程访问会被拒绝并设置当前线程错误

所以不要一上来就把它当成“天然跨线程共享池”。


### 2.4 四条链表才是 `fsmempool` 的真正工作面

当前实现里，池子内部最重要的不是“总共有多少对象”，而是：

- `LL_Idle`
	- 还有空闲槽位的 `MemUnit`
- `LL_Full`
	- 已经满载，暂时不能继续分配的 `MemUnit`
- `LL_Null`
	- 已清空但被保留作备用的 `MemUnit`
	- 最多保留 1 个
- `LL_Free`
	- `MMU_LLNode` 自己的空槽位
	- 用来复用已经释放掉的单元编号 / Flag

这四条链表决定了池子的三个关键行为：

- 新分配优先走哪里
- 空单元是销毁还是保留备用
- 老单元的标识和 Flag 如何复用


### 2.5 `Alloc()` 的顺序是：`Idle -> Null -> Free -> New`

当前实现里，分配逻辑大致是：

1. 先看 `LL_Idle`
2. 如果没有可分配单元
	- 优先把 `LL_Null` 的备用空单元拿回来
3. 如果没有备用空单元
	- 再尝试复用 `LL_Free` 里的 `MMU_LLNode`
4. 这些都没有
	- 才真正创建新的 `MemUnit`

所以 `fsmempool` 的重点不是“每次都重新建单元”，而是：

- 尽量复用已有单元和已有编号

这也意味着它虽然底层每个 `MemUnit` 仍是 256 槽，但整个池：

- 没有 256 个对象的总量上限


### 2.6 `Free()` 不是普通 `free()`，它依赖对象头里的 `ItemFlag`

`xrtFSMemPoolFree(objMM, p)` 会先读对象前面那 4 字节头部：

- 这里记录了所属 `MemUnit` 的 Flag
- 也记录了该对象在单元里的槽位索引

所以这里有两个硬边界：

- `p` 必须真的是这个池分配出来的对象
- 不能把 `poolA` 的对象交给 `poolB` 去 `Free()`

否则你不是“释放失败”这么简单，而是在触碰错误对象头。

最稳的习惯仍然是：

- 对象谁分配，谁定义释放边界
- 释放后立刻把裸指针置空


### 2.7 `LL_Null` 只保留一个备用空单元，这是“降抖动”策略，不是泄漏

很多人第一次看实现会觉得：

- 单元都清空了
- 为什么不全部销毁

答案是当前实现故意保留：

- 最多 1 个全空 `MemUnit`

原因很实际：

- 某些业务负载会在临界点上下抖动
- 如果每次刚空就销毁、下一波又立刻重建
- 会把对象池优势重新抹掉

所以：

- `LL_Null` 是用空间换稳定性
- 它是策略性的保留，不是失控泄漏


### 2.8 `GC_Mark / GC` 只是“显式标记回收入口”，不是自动追根扫描器

`fsmempool` 的 GC 能力很好用，但一定别神化。

当前接口是：

```c
#define xrtFSMemPoolGC_Mark xrtMemUnitGC_Mark
XXAPI void xrtFSMemPoolGC(xfsmempool objMM, bool bFreeMark);
```

它能做的是：

- 让你先给仍然存活的对象打标记
- 再对整个池做一轮回收

它不能做的是：

- 自动替你找到 root
- 自动遍历你的业务图结构
- 自动理解哪个对象“逻辑上还活着”

所以 `fsmempool` 的 GC 更适合：

- 你自己已经有 root table
- 你能明确写出 `MarkReachable()`
- 你只是想把“回收池里未标记对象”这一步交给库


### 2.9 这 4 种方案要分开看

一个很实用的选型表是：

| 方案 | 最适合的问题 | 不适合的问题 |
|------|--------------|--------------|
| `MemUnit` | 就是一组 256 槽固定对象，想直接操作最底层单元 | 想长期维护很多单元和状态分类 |
| `BSMM` | 轻量固定大小块复用，业务自己管理对象生命周期 | 需要对象级状态、正式池管理和 GC |
| `FSMemPool` | 固定大小对象长期复用，想要四链表分类和显式 GC | 同一池里混放很多不同尺寸对象 |
| `MemPool` | 通用尺寸分配和更广义对象池 | 只是一种固定大小对象，没必要上更重方案 |


## 3. 最小 DEMO：先看懂“超过 256 也能继续分配”和“释放后槽位复用”

第一步先别急着上 GC。

先把 `fsmempool` 最核心的两个行为看懂：

- 单个 `MemUnit` 只有 256 槽，但整个池没有 256 总量上限
- 释放一个对象以后，下次分配很可能直接复用原地址

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
	int iID;
	char sName[32];
} DemoJob;


int main(void)
{
	xfsmempool pPool = NULL;
	DemoJob* arrJob[260] = { 0 };
	DemoJob* pFreed = NULL;
	DemoJob* pReused = NULL;
	int i = 0;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pPool = xrtFSMemPoolCreate(sizeof(DemoJob), XRT_OBJMODE_LOCAL);
	if ( pPool == NULL ) {
		goto cleanup;
	}

	for ( i = 0; i < 260; ++i ) {
		arrJob[i] = (DemoJob*)xrtFSMemPoolAlloc(pPool);
		if ( arrJob[i] == NULL ) {
			goto cleanup;
		}

		arrJob[i]->iID = i;
		xrtFormat(arrJob[i]->sName, sizeof(arrJob[i]->sName), "job-%d", i);
	}

	printf("已成功分配 260 个对象，这说明池总量不会卡在单个 MemUnit 的 256 槽上。\n");

	pFreed = arrJob[10];
	xrtFSMemPoolFree(pPool, pFreed);
	arrJob[10] = NULL;

	pReused = (DemoJob*)xrtFSMemPoolAlloc(pPool);
	if ( pReused == NULL ) {
		goto cleanup;
	}

	printf("被释放地址: %p\n", (void*)pFreed);
	printf("再次分配地址: %p\n", (void*)pReused);
	printf("这两个地址通常会相同，因为池会优先复用空闲槽位。\n");

	iRet = 0;

cleanup:
	if ( pPool != NULL ) {
		xrtFSMemPoolDestroy(pPool);
	}
	xrtUnit();
	return iRet;
}
```

这个 DEMO 最值得观察的不是输出内容，而是它背后的结论：

- `fsmempool` 不是单个 256 槽单元
- 它是很多 `MemUnit` 的总管理器
- 释放后的对象地址随时可能立刻被复用


## 4. 升级 DEMO：自己维护 root，然后让 `GC(FALSE)` 回收未标记对象

真正把 `fsmempool` 和 `bsmm` 拉开距离的，是这一条：

- 你可以显式标记仍然活着的对象
- 然后让池统一回收未标记对象

下面这个例子用一个很小的对象图来演示：

- `0 -> 1 -> 2` 是还活着的链
- `3 -> 4` 没有 root 指向
- 一轮 `MarkReachable()` 之后
- `xrtFSMemPoolGC(pool, FALSE)` 会把未标记对象收掉

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

	xrtFSMemPoolGC_Mark(pNode);
	MarkReachable(pNode->pNext);
}


int main(void)
{
	xfsmempool pPool = NULL;
	DemoNode* arrNode[5] = { 0 };
	DemoNode* pRoot = NULL;
	DemoNode* pNewA = NULL;
	DemoNode* pNewB = NULL;
	int i = 0;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pPool = xrtFSMemPoolCreate(sizeof(DemoNode), XRT_OBJMODE_LOCAL);
	if ( pPool == NULL ) {
		goto cleanup;
	}

	for ( i = 0; i < 5; ++i ) {
		arrNode[i] = (DemoNode*)xrtFSMemPoolAlloc(pPool);
		if ( arrNode[i] == NULL ) {
			goto cleanup;
		}

		arrNode[i]->iID = i;
		arrNode[i]->pNext = NULL;
	}

	arrNode[0]->pNext = arrNode[1];
	arrNode[1]->pNext = arrNode[2];
	arrNode[3]->pNext = arrNode[4];

	pRoot = arrNode[0];

	MarkReachable(pRoot);
	xrtFSMemPoolGC(pPool, FALSE);

	pNewA = (DemoNode*)xrtFSMemPoolAlloc(pPool);
	pNewB = (DemoNode*)xrtFSMemPoolAlloc(pPool);
	if ( (pNewA == NULL) || (pNewB == NULL) ) {
		goto cleanup;
	}

	printf("root 链保留了节点 0, 1, 2。\n");
	printf("未标记链 3 -> 4 已被 GC 回收，后续分配通常会优先复用这些槽位。\n");
	printf("新对象地址 A: %p\n", (void*)pNewA);
	printf("新对象地址 B: %p\n", (void*)pNewB);

	iRet = 0;

cleanup:
	if ( pPool != NULL ) {
		xrtFSMemPoolDestroy(pPool);
	}
	xrtUnit();
	return iRet;
}
```

这里要记住 3 件事：

1. 每一轮 GC 前，你都要重新标记当前还活着的对象。
2. `GC(FALSE)` 的意思是回收未标记对象，这才是最常见的 mark-sweep 形式。
3. `GC_Mark()` 只是把位打到对象头上，不会帮你自动找 root。


## 5. 什么时候该从 `bsmm` 升到 `fsmempool`

一个最实用的判断方式是：

### 5.1 继续用 `bsmm`

如果你满足下面这些条件，就先别升：

- 你只想反复拿固定大小块
- 对象初始化和释放流程都很简单
- 你自己就能完全掌握哪些块活着
- 不需要显式 GC
- 不需要正式区分“可分配单元 / 满载单元 / 备用空单元”

### 5.2 升到 `fsmempool`

如果你已经出现这些特征，升级就很值：

- 对象长期存在，不是一批一批立刻清空
- 对象数量会跨过很多个 256 槽单元
- 你希望池子自己维护单元状态
- 你想做显式 root 标记和统一回收
- 你希望对象池接口更接近“正式对象管理器”，而不是裸内存块池

### 5.3 再往上升到 `mempool`

如果问题已经变成：

- 不同对象尺寸差异很大
- 同一业务里各种长度的块都要放进一个通用池

那就别硬拿 `fsmempool` 做变通了。  
这时应该直接看：

- `mempool`


## 6. 线程、主线程和跨平台边界

虽然这页是内存主题，不是并发主题，但仍有 4 个边界必须说清楚：

### 6.1 `fsmempool` 解决的是分配抖动，不是主线程阻塞

它不会把同步代码自动变成异步。  
它能解决的是：

- 高频对象创建 / 释放的成本和波动

它解决不了的是：

- I/O 阻塞
- 网络等待
- 单主线程调度瓶颈

这部分问题还是要回到：

- [多任务总论](multitask-overview.md)

### 6.2 默认从 `XRT_OBJMODE_LOCAL` 起步最稳

这在 Windows 和 Linux 都一样。  
如果你的对象池本质上属于某一个线程，就不要默认把它做成 shared。

### 6.3 对象池不是同步原语

它不会替你解决：

- 生产者 / 消费者
- 跨线程交接
- 生命周期竞争

如果对象需要在线程间流动，更常见的主线仍然是：

- `queue`
- 明确所有权
- 到目标线程再 `Free()`

### 6.4 对象头和 `ItemFlag` 是实现合同的一部分，别自己越界踩

当前实现依赖对象前面的 4 字节头部。  
所以不管在 Windows 还是 Linux：

- 不要把 `p - 4` 当业务数据
- 不要手工篡改 `ItemFlag`
- 不要把池对象和外部分配器对象混着释放


## 7. 常见错误

### 7.1 把 `fsmempool` 当“自动跨线程共享池”

错。  
当前正式主线仍然应该先按 owner-thread local 理解。

### 7.2 把 `GC()` 理解成“自动扫描整个程序”

错。  
它只是给你显式标记回收入口，不会替你找 root。

### 7.3 把 `poolA` 的对象拿给 `poolB` 去 `Free()`

错。  
`Free()` 是靠对象头里的 `ItemFlag` 找回所属单元的。

### 7.4 以为对象清空后所有 `MemUnit` 都会立刻销毁

错。  
当前实现会保留 1 个 `LL_Null` 备用空单元，用来降低分配抖动。

### 7.5 以为它能替代所有对象池

错。  
它只适合固定大小对象；一旦尺寸开始发散，就该看 `mempool`。


## 8. 建议继续阅读

- [FSMemPool API](../api/api-mempool-fs.md)
- [MemUnit 入门：什么时候该直接控制单个 256 槽内存单元，而不是上 `bsmm`、`fsmempool` 或 `mempool`](memunit-intro.md)
- [MemPool 入门：什么时候该用通用变长池，而不是 `fsmempool`、`memunit` 或直接 `malloc/free`](mempool-intro.md)
- [BSMM 入门：什么时候该用固定大小块池，而不是 `malloc/free`、数组或更重的对象池](bsmm-intro.md)
- [MemUnit API](../api/api-memunit.md)
- [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)
