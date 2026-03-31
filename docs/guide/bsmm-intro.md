# BSMM 入门：什么时候该用固定大小块池，而不是 `malloc/free`、数组或更重的对象池

> 目标：把 `xrtBsmmCreate()`、`xrtBsmmInit()`、`xrtBsmmAlloc()`、`xrtBsmmFree()`、`xrtBsmmUnit()` 讲成第 2 阶段第一条正式内存主线。读完这页后，你应该明确知道：什么时候问题已经不是“要不要再 `malloc` 一次”，而是“我要反复创建和释放一大批同尺寸对象，而且希望地址复用、分配稳定、整批清空也足够便宜”。  

[返回教学文档](README.md)

---

## 1. 为什么 `bsmm` 应该在容器之后讲

学完 `array / dict / list / avltree` 以后，很多人会自然遇到另一个层面的问题：

- 容器能装数据了
- 但数据对象本身还是在反复 `malloc/free`
- 同一种对象会被频繁创建、销毁、再创建
- 大多数对象大小都完全一样

这时真正的瓶颈就开始从：

- “我该把对象放进哪个容器”

转到：

- “这些对象本身该怎么分配”

这正是 `bsmm` 的位置。

它最适合解决的是：

- 同尺寸对象反复分配与释放
- 地址复用
- 分批清空
- 把一类对象的生命周期集中交给一个小管理器

它不适合解决的是：

- 可变长度内存
- 顺序存储和随机索引
- 多种尺寸混在一起的大池子


## 2. 先把 9 个边界分清楚

### 2.1 `bsmm` 只适合“同一种尺寸”的对象

这是它最核心的前提。

创建时你只告诉它一件事：

- 每个成员占多少字节

后面每次 `Alloc()` 出来的块，尺寸都一样。

所以它特别适合：

- AST 节点
- 粒子对象
- 任务节点
- 临时 token

它不适合：

- 一会儿要 24 字节
- 一会儿要 300 字节
- 一会儿还要可变字符串

这种场景更接近：

- `mempool`
- 或直接 `malloc/free`


### 2.2 `Count` 不是“当前活着的对象数”，而是“已经开出来的槽位数”

这点必须先讲清楚。

当前实现里：

- `Alloc()` 如果复用空闲链表
	- `Count` 不会增加
- `Free()` 只是把块挂回 `LL_Free`
	- `Count` 也不会减少

所以 `Count` 更接近：

- 这一个 `bsmm` 到目前为止，真正开过多少个槽位

而不是：

- 现在业务上还有多少对象在活着


### 2.3 `Free()` 不会把页还给系统，只会先挂回空闲链表

这也是 `bsmm` 和普通 `free()` 最大的直觉差异。

调用：

- `xrtBsmmFree()`

以后，当前块会先进入：

- `LL_Free`

下次 `Alloc()` 时优先复用。

真正释放整批页和空闲链表，发生在：

- `xrtBsmmUnit()`
- `xrtBsmmDestroy()`


### 2.4 释放后的指针立刻失效，而且下一次 `Alloc()` 很可能就会复用同一地址

这既是优点，也是最容易踩坑的地方。

优点是：

- 地址复用快
- 分配稳定
- 不必频繁向系统申请小块内存

风险是：

- 你一旦 `Free()` 以后还继续拿旧指针访问
- 很可能访问到“已经被新对象复用”的地址

所以 `bsmm` 的释放规则必须比一般 `free()` 更严格：

- 释放后立刻置空
- 不要跨层偷偷缓存裸指针


### 2.5 `bsmm` 按 256 槽一页增长

当前实现里，当现有页用完时，会新开一页：

- `ItemLength * 256`

这意味着它有两个稳定特征：

- 小批量对象分配很便宜
- 对象数量上来以后，增长仍然比较平滑

所以它适合“会不断长一点、再反复复用”的固定对象池。


### 2.6 当前主线里 `Create / Init` 都要带 `iMode`

当前正式签名是：

```c
xrtBsmmCreate(uint32 iItemLength, uint32 iMode)
xrtBsmmInit(xbsmm objBSMM, uint32 iItemLength, uint32 iMode)
```

最常见选择仍然是：

- `XRT_OBJMODE_LOCAL`

对大多数对象池教学来说，这也是最稳的起点。


### 2.7 `bsmm` 没有公开“按业务顺序遍历”的合同，`GetPtr_Inline()` 只是调试和特殊路径工具

它不是数组。

虽然当前实现提供了：

- `xrtBsmmGetPtr_Inline(objBSMM, iIdx)`

但这个索引表示的是：

- 某个已经开出来的槽位位置

它不是：

- 当前第几个活对象
- 业务顺序
- 稳定句柄

因为一旦槽位被释放再复用：

- 同一个 `iIdx`
	- 可能对应另一批对象

所以最稳的经验是：

- 业务逻辑不要把 `bsmm` 当数组
- `GetPtr_Inline()` 只留给调试或极特殊扫描逻辑


### 2.8 `bsmm` 只管理原始内存块，不帮你做对象初始化和析构

它负责的是：

- 分配一块固定大小的内存
- 复用这块内存
- 批量清空这些页

它不负责：

- 给结构体清零
- 释放结构体里挂着的字符串或子对象
- 自动调用析构函数风格的清理逻辑

所以如果对象里有额外资源：

- 释放前先自己清理
- 再 `xrtBsmmFree()`


### 2.9 `bsmm` 是轻量块池，不是 `fsmempool` 或通用池的替代品

一个很实用的粗分法是：

- `bsmm`
	- 一种固定尺寸对象
	- 想要极轻的页 + 空闲链表复用
- `fsmempool`
	- 固定尺寸对象
	- 但池本身是更正式、更完整的独立组件
- `mempool`
	- 需要可变尺寸分配
- `array`
	- 需要顺序存储和密集索引


## 3. 最小 DEMO：先看懂“释放后复用”和 `Count` 的真实含义

第一步不要先上复杂场景。  
先把 `bsmm` 的核心行为看清楚：

- 创建 local 管理器
- 分配 3 个对象
- 释放其中 1 个
- 再分配 1 个
- 观察地址是否复用、`Count` 是否变化

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
	int iID;
	int iValue;
} DemoNode;


int main(void)
{
	xbsmm pPool = NULL;
	DemoNode* pNode1 = NULL;
	DemoNode* pNode2 = NULL;
	DemoNode* pNode3 = NULL;
	DemoNode* pNode4 = NULL;
	DemoNode* pFreedSlot = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pPool = xrtBsmmCreate(sizeof(DemoNode), XRT_OBJMODE_LOCAL);
	if ( pPool == NULL ) {
		goto cleanup;
	}

	pNode1 = (DemoNode*)xrtBsmmAlloc(pPool);
	pNode2 = (DemoNode*)xrtBsmmAlloc(pPool);
	pNode3 = (DemoNode*)xrtBsmmAlloc(pPool);
	if ( (pNode1 == NULL) || (pNode2 == NULL) || (pNode3 == NULL) ) {
		goto cleanup;
	}

	pNode1->iID = 1;
	pNode2->iID = 2;
	pNode3->iID = 3;

	printf("count after 3 alloc => %u\n", pPool->Count);

	pFreedSlot = pNode2;
	xrtBsmmFree(pPool, pNode2);
	pNode2 = NULL;

	pNode4 = (DemoNode*)xrtBsmmAlloc(pPool);
	if ( pNode4 == NULL ) {
		goto cleanup;
	}
	pNode4->iID = 4;
	pNode4->iValue = 400;

	printf("count after reuse => %u\n", pPool->Count);
	printf("reuse happened     => %d\n", (pNode4 == pFreedSlot));
	printf("node4 id/value     => %d / %d\n", pNode4->iID, pNode4->iValue);
	iRet = 0;

cleanup:
	if ( pPool != NULL ) {
		xrtBsmmDestroy(pPool);
	}
	xrtUnit();
	return iRet;
}
```

这个最小例子最想让你先记住的是：

1. `Free()` 后下次 `Alloc()` 很可能复用旧地址
2. `Count` 表示已开槽位数，不是当前活对象数
3. 释放后旧指针必须立刻失效
4. `bsmm` 非常适合“反复生成同尺寸对象”


## 4. 升级 DEMO：把 `bsmm` 内嵌到请求级 scratch 里，一次请求结束后整批清空

当你已经看懂地址复用以后，下一步最值得学的是：

- 有些对象根本不需要单个回收
- 它们天然就是“这一批结束后一起清空”

这时 `bsmm` 非常适合做：

- request scratch
- parser scratch
- batch scratch

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct DemoToken {
	int iKind;
	int iStart;
	int iLength;
	struct DemoToken* pNext;
} DemoToken;

typedef struct {
	xbsmm_struct TokenPool;
	DemoToken* pHead;
	DemoToken* pTail;
} DemoLexerScratch;


static bool procPushToken(DemoLexerScratch* pScratch, int iKind, int iStart, int iLength)
{
	DemoToken* pToken = (DemoToken*)xrtBsmmAlloc(&pScratch->TokenPool);
	if ( pToken == NULL ) {
		return FALSE;
	}

	memset(pToken, 0, sizeof(*pToken));
	pToken->iKind = iKind;
	pToken->iStart = iStart;
	pToken->iLength = iLength;

	if ( pScratch->pTail != NULL ) {
		pScratch->pTail->pNext = pToken;
	} else {
		pScratch->pHead = pToken;
	}
	pScratch->pTail = pToken;
	return TRUE;
}


static void procPrintTokens(DemoLexerScratch* pScratch)
{
	for ( DemoToken* pCur = pScratch->pHead; pCur != NULL; pCur = pCur->pNext ) {
		printf("kind=%d start=%d len=%d\n",
			pCur->iKind,
			pCur->iStart,
			pCur->iLength);
	}
}


int main(void)
{
	DemoLexerScratch tScratch;
	bool bPoolInit = FALSE;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	memset(&tScratch, 0, sizeof(tScratch));
	xrtBsmmInit(&tScratch.TokenPool, sizeof(DemoToken), XRT_OBJMODE_LOCAL);
	bPoolInit = TRUE;

	if ( !procPushToken(&tScratch, 1, 0, 3) ) {
		goto cleanup;
	}
	if ( !procPushToken(&tScratch, 2, 4, 5) ) {
		goto cleanup;
	}
	if ( !procPushToken(&tScratch, 3, 10, 2) ) {
		goto cleanup;
	}

	printf("token slots => %u\n", tScratch.TokenPool.Count);
	procPrintTokens(&tScratch);

	/* 一次请求处理结束后，整批清空 scratch */
	xrtBsmmUnit(&tScratch.TokenPool);
	bPoolInit = FALSE;
	tScratch.pHead = NULL;
	tScratch.pTail = NULL;
	iRet = 0;

cleanup:
	if ( bPoolInit ) {
		xrtBsmmUnit(&tScratch.TokenPool);
	}
	xrtUnit();
	return iRet;
}
```

这个升级例子真正要教会你的事有 5 个：

1. `bsmm` 很适合和业务 scratch 结构绑定
2. 对象尺寸固定时，批量创建比反复 `malloc/free` 更直接
3. 如果这一批对象天然同生共死，可以直接整批 `Unit()`
4. `bsmm` 不负责清零，所以对象写入前最好自己初始化
5. 如果你已经决定“这一批一起死”，往往没必要逐个 `Free()`


## 5. 它和 `malloc/free`、`array`、`fsmempool` 的区别

### 5.1 和 `malloc/free` 的区别

- `malloc/free`
	- 尺寸灵活
	- 更通用
- `bsmm`
	- 只做同尺寸对象
	- 更适合重复复用


### 5.2 和 `array` 的区别

- `array`
	- 关心顺序和索引
- `bsmm`
	- 关心对象块能否反复复用


### 5.3 和 `fsmempool` 的区别

- `bsmm`
	- 更轻
	- 更像“页 + 空闲链表”的固定块池
- `fsmempool`
	- 是更完整的固定大小对象池主线
	- 适合继续往更正式的池管理策略升级


## 6. 如果只有一条主线程，会卡在哪里

`bsmm` 自己不会等待外部事件，但主线程里仍然会消耗在：

- 新页分配
- 对象初始化
- 对象内部资源清理
- 大量指针追踪

所以最常见的问题不是“线程阻塞”，而是：

- 本来对象尺寸不固定，却硬塞进 `bsmm`
- 把 `Count` 当成当前活对象数
- 释放后还偷偷用旧指针


## 7. Windows / Linux 跨平台时最该记住的事

- 固定大小对象的结构体布局要自己保证稳定，不要依赖编译器偶然的填充方式做持久化
- 指针地址不能当成可持久化或跨进程数据
- `sizeof(ptr)`、对齐和结构体填充在 32/64 位平台上会变化，池里对象大小要按真实结构体重新计算
- 一批对象里如果有文件句柄、线程句柄这类平台资源，释放顺序要先处理资源，再回收到 `bsmm`


## 8. 常见错误

### 错误一：把 `Count` 当成“当前活对象数”

当前实现里它更接近：

- 已经开出来的槽位数

### 错误二：释放后继续使用旧指针

这在 `bsmm` 里尤其危险，因为下一次分配很可能直接复用同一地址。

### 错误三：把 `bsmm` 当成数组

`GetPtr_Inline()` 不是稳定业务索引接口。

### 错误四：把可变长度对象硬塞进固定块池

这类对象更适合：

- `mempool`
- 或普通动态分配

### 错误五：以为 `bsmm` 会帮你清理对象里的附加资源

不会。  
它只管理原始块内存。

### 错误六：每次循环都重新 `Create()` / `Destroy()`

如果是长期反复出现的同类对象，更稳的是复用管理器。


## 9. 建议继续阅读

- [BSMM API](../api/api-bsmm.md)
- [FSMemPool API](../api/api-mempool-fs.md)
- [MemUnit API](../api/api-memunit.md)
- [MemPool API](../api/api-mempool.md)
- [AVLTree 入门：什么时候该直接用通用有序树，而不是 Dict、List 或数组](avltree-intro.md)

---

**一句话总结：** `bsmm` 最适合承担“反复创建和释放同尺寸对象”的轻量块池角色；先把 `Count` 语义、地址复用、释放失效和整批 `Unit()` 的边界看清楚，它会比无脑 `malloc/free` 更稳、更省。
