# PtrArray 入门：什么时候该管理对象指针，而不是搬运整个结构体

> 目标：把 `xrtPtrArrayCreate()`、`xrtPtrArrayAppend()`、`xrtPtrArrayInsert()`、`xrtPtrArrayAddAlt()`、`xrtPtrArraySet()`、`xrtPtrArrayRemove()`、`xrtPtrArraySort()` 和 `xrtPtrArrayLock()` / `xrtPtrArrayUnlock()` 讲成第 2 阶段第三条正式容器主线。读完这页后，你应该明确知道：什么时候问题已经从“固定大小结构体数组”变成“对象指针集合”，以及 `1-based` 成员编号、空槽复用、`LOCAL / SHARED` 模式分别意味着什么。

[返回教学文档](README.md)

---

## 1. 为什么 `ptrarray` 应该接在 `array` 后面讲

学完 `array` 以后，最自然的下一步通常就是：

- 我的元素已经不是小结构体了
- 每个对象自己还有独立生命周期
- 我不想插入、删除、排序时搬整个对象本体
- 我需要一组“对象引用槽位”，而不是一块结构体连续内存

这时问题的本质又变了一次：

- `array`
	- 你管理的是结构体本体
- `ptrarray`
	- 你管理的是对象指针

所以这一页真正要解决的是：

- 怎样把一批对象指针放进可增长数组
- 怎样区分“删除成员并压缩索引”和“保留空槽稍后复用”
- 怎样理解当前 owner/shared 双模式
- 为什么排序回调收到的是“指向指针的指针”


## 2. 先把 8 个边界分清楚

### 2.1 `ptrarray` 存的是指针，不是对象本体

创建 `ptrarray` 时，你不需要给：

- `sizeof(结构体)`

因为它内部固定存的是：

- `ptr`

这意味着：

- 插入、删除、排序时搬的是指针
- 对象本体可以很大，也可以分散在堆上
- 容器和对象生命周期天然可以拆开

所以更直接地说：

- 你要管理“小而固定”的结构体本体
	- 优先想 `array`
- 你要管理“对象引用、会独立分配的大对象、会被多处持有的实体”
	- 优先想 `ptrarray`


### 2.2 `ptrarray` 不会替你释放对象

这是这页最重要的边界之一。

`xrtPtrArrayDestroy()` 只会释放：

- 指针数组管理器本身
- 内部那块 `ptr* Memory`

它不会释放：

- 每个指针指向的对象

所以你需要自己决定对象释放时机：

- 先逐个释放对象，再销毁数组
- 或者先把对象移交给别处，再从数组里删掉

不要把 `ptrarray` 想成：

- 带析构器的容器
- 自动接管对象所有权的智能数组


### 2.3 成员编号依然是 **1-based**

这条规则和 `array` 一样：

- 第一个成员编号是 `1`
- `0` 不是有效成员编号

所以这些接口都按 1-based 理解：

- `xrtPtrArrayAppend()` 返回值
- `xrtPtrArrayGet()` / `xrtPtrArraySet()` 的 `iPos`
- `xrtPtrArrayRemove()` / `xrtPtrArraySwap()` 的位置参数

只有内部 `Memory` 数组本身是 0-based：

- `pArr->Memory[0]`
	- 对应第 1 个成员


### 2.4 `xrtPtrArrayInsert()` 传的是零基插入位点，返回的是 1-based 成员编号

当前实现里：

- `xrtPtrArrayInsert(pArr, iPos, pVal)`
	- `iPos = 0`
		- 头部插入
	- `iPos = Count`
		- 末尾追加

函数返回值则是：

- 新成员的 **1-based** 编号

所以最稳的理解和 `array` 一样：

- 参数坐标
	- 更像零基插入位点
- 返回坐标
	- 是正式的 1-based 成员编号


### 2.5 `Remove()` 和 `Set(NULL) + AddAlt()` 是两条完全不同的管理路线

这又是 `ptrarray` 最容易讲浅、也最容易误用的地方。

如果你用：

- `xrtPtrArrayRemove()`

那么效果是：

- 该成员被移除
- 后面的成员整体前移
- `Count` 变小
- 后续索引会重新编号

如果你用：

- `xrtPtrArraySet(pArr, iPos, NULL)`
- 之后再 `xrtPtrArrayAddAlt()`

那么效果是：

- 数组长度不变
- 只是留下一个空槽
- `AddAlt()` 会优先把新成员放进这个空槽

所以：

- 你要的是“压缩列表、重新编号”
	- 用 `Remove()`
- 你要的是“稳定槽位、稍后复用”
	- 用 `Set(NULL) + AddAlt()`


### 2.6 `xrtPtrArrayAddAlt()` 只会复用你明确留成 `NULL` 的槽位

`AddAlt()` 不会：

- 自动析构对象
- 自动把“逻辑删除”变成空槽
- 自动理解哪一项算失效

它只做一件事：

- 从前往后找第一个 `NULL`
- 找到就填进去
- 找不到才真正 append

这意味着如果你想用这条路线，就要自己先做好：

- 对象释放
- 槽位置空
- 槽位语义管理


### 2.7 `LOCAL` 和 `SHARED` 不是随便选的装饰参数

当前源码主线里：

- `xrtPtrArrayCreate(XRT_OBJMODE_LOCAL)`
	- 本线程默认快路径
- `xrtPtrArrayCreate(XRT_OBJMODE_SHARED)`
	- 进入 shared root 主线

最稳的理解是：

- 本线程私有数组
	- 用 `LOCAL`
- 共享根对象、跨线程复合操作、需要稳定视图
	- 用 `SHARED`
	- 并在合适的位置显式 `Lock / Unlock`

不要默认一上来全部开 shared。  
shared 解决的是共享所有权语义，不是“以后什么都不用设计”。


### 2.8 `Sort()` 的比较函数收到的是“指向指针的指针”

这是 `ptrarray` 和 `array` 最容易写错的地方之一。

因为底层排的是：

- `ptr`

所以排序回调看到的参数实际上是：

- `const void*`
	- 指向数组元素
- 数组元素本身又是一个指针

所以比较函数通常要写成：

```c
static int procCompareJobPtr(const void* pLeft, const void* pRight)
{
	const DemoJob* pA = *(const DemoJob**)pLeft;
	const DemoJob* pB = *(const DemoJob**)pRight;
	return pB->iPriority - pA->iPriority;
}
```

如果你把它误写成直接把 `pLeft` 当对象本体，排序结果通常就不对了。


## 3. 最小 DEMO：先把它当成对象指针清单

第一步不要先上空槽复用，也不要先上 shared。  
先把最常见、也最稳定的主线走通：

- 创建一个 local 指针数组
- 往里面放几个堆对象
- 按 1-based 遍历
- 最后自己释放每个对象

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
	int iPort;
	str sName;
} DemoConn;


static DemoConn* procCreateConn(int iPort, str sName)
{
	DemoConn* pConn = (DemoConn*)xrtMalloc(sizeof(DemoConn));
	if ( pConn == NULL ) {
		return NULL;
	}

	pConn->iPort = iPort;
	pConn->sName = xrtCopyStr(sName, 0);
	if ( pConn->sName == NULL ) {
		xrtFree(pConn);
		return NULL;
	}

	return pConn;
}


static void procFreeConn(DemoConn* pConn)
{
	if ( pConn == NULL ) {
		return;
	}

	if ( pConn->sName != NULL ) {
		xrtFree(pConn->sName);
	}
	xrtFree(pConn);
}


int main(void)
{
	xparray pArr = NULL;
	DemoConn* pConn = NULL;
	uint32 i = 0;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pArr = xrtPtrArrayCreate(XRT_OBJMODE_LOCAL);
	if ( pArr == NULL ) {
		goto cleanup;
	}

	pConn = procCreateConn(7001, "alpha");
	if ( (pConn == NULL) || (xrtPtrArrayAppend(pArr, pConn) == 0) ) {
		if ( pConn != NULL ) {
			procFreeConn(pConn);
			pConn = NULL;
		}
		fprintf(stderr, "append alpha failed\n");
		goto cleanup;
	}
	pConn = NULL;

	pConn = procCreateConn(7002, "beta");
	if ( (pConn == NULL) || (xrtPtrArrayAppend(pArr, pConn) == 0) ) {
		if ( pConn != NULL ) {
			procFreeConn(pConn);
			pConn = NULL;
		}
		fprintf(stderr, "append beta failed\n");
		goto cleanup;
	}
	pConn = NULL;

	pConn = procCreateConn(7003, "gamma");
	if ( (pConn == NULL) || (xrtPtrArrayAppend(pArr, pConn) == 0) ) {
		if ( pConn != NULL ) {
			procFreeConn(pConn);
			pConn = NULL;
		}
		fprintf(stderr, "append gamma failed\n");
		goto cleanup;
	}
	pConn = NULL;

	for ( i = 1; i <= pArr->Count; i++ ) {
		DemoConn* pConn = (DemoConn*)xrtPtrArrayGet(pArr, i);
		printf("[%u] %s => %d\n", i, pConn->sName, pConn->iPort);
	}

	iRet = 0;

cleanup:
	if ( pConn != NULL ) {
		procFreeConn(pConn);
	}
	if ( pArr != NULL ) {
		for ( i = 1; i <= pArr->Count; i++ ) {
			DemoConn* pConn = (DemoConn*)xrtPtrArrayGet(pArr, i);
			if ( pConn != NULL ) {
				procFreeConn(pConn);
			}
		}
		xrtPtrArrayDestroy(pArr);
	}
	xrtUnit();
	return iRet;
}
```

这个最小例子最想让你先记住的是：

1. `ptrarray` 管的是对象指针，不是对象本体
2. 追加和遍历的成员编号仍然是 1-based
3. 销毁容器前，要先处理每个对象的释放
4. 只要对象本体独立分配，插入和排序就不需要搬大块结构体


## 4. 升级 DEMO：把“稳定槽位”和 `AddAlt()` 讲清楚

当你已经会用 `ptrarray` 存对象指针，下一步最值得学的是：

- 不是所有删除都该 `Remove()`
- 有些场景更像“槽位池”

比如：

- 连接槽
- worker 槽
- session 槽
- 任务句柄槽

这些场景经常需要：

- 某个槽位暂时空出来
- 后面新对象优先复用空位
- 不希望每次都让后面全部重新编号

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
	int iWorkerID;
	str sTag;
} DemoWorker;


static DemoWorker* procCreateWorker(int iWorkerID, str sTag)
{
	DemoWorker* pWorker = (DemoWorker*)xrtMalloc(sizeof(DemoWorker));
	if ( pWorker == NULL ) {
		return NULL;
	}

	pWorker->iWorkerID = iWorkerID;
	pWorker->sTag = xrtCopyStr(sTag, 0);
	if ( pWorker->sTag == NULL ) {
		xrtFree(pWorker);
		return NULL;
	}

	return pWorker;
}


static void procFreeWorker(DemoWorker* pWorker)
{
	if ( pWorker == NULL ) {
		return;
	}

	if ( pWorker->sTag != NULL ) {
		xrtFree(pWorker->sTag);
	}
	xrtFree(pWorker);
}


int main(void)
{
	xparray pPool = NULL;
	DemoWorker* pOld = NULL;
	DemoWorker* pWorker = NULL;
	uint32 u32Slot = 0;
	uint32 i = 0;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pPool = xrtPtrArrayCreate(XRT_OBJMODE_LOCAL);
	if ( pPool == NULL ) {
		goto cleanup;
	}

	pWorker = procCreateWorker(1, "worker-a");
	if ( (pWorker == NULL) || (xrtPtrArrayAppend(pPool, pWorker) == 0) ) {
		if ( pWorker != NULL ) {
			procFreeWorker(pWorker);
			pWorker = NULL;
		}
		fprintf(stderr, "append worker-a failed\n");
		goto cleanup;
	}
	pWorker = NULL;

	pWorker = procCreateWorker(2, "worker-b");
	if ( (pWorker == NULL) || (xrtPtrArrayAppend(pPool, pWorker) == 0) ) {
		if ( pWorker != NULL ) {
			procFreeWorker(pWorker);
			pWorker = NULL;
		}
		fprintf(stderr, "append worker-b failed\n");
		goto cleanup;
	}
	pWorker = NULL;

	pWorker = procCreateWorker(3, "worker-c");
	if ( (pWorker == NULL) || (xrtPtrArrayAppend(pPool, pWorker) == 0) ) {
		if ( pWorker != NULL ) {
			procFreeWorker(pWorker);
			pWorker = NULL;
		}
		fprintf(stderr, "append worker-c failed\n");
		goto cleanup;
	}
	pWorker = NULL;

	pOld = (DemoWorker*)xrtPtrArrayGet(pPool, 2);
	procFreeWorker(pOld);
	xrtPtrArraySet(pPool, 2, NULL);

	pWorker = procCreateWorker(20, "worker-new");
	if ( pWorker == NULL ) {
		fprintf(stderr, "create worker-new failed\n");
		goto cleanup;
	}

	u32Slot = xrtPtrArrayAddAlt(pPool, pWorker);
	if ( u32Slot == 0 ) {
		procFreeWorker(pWorker);
		pWorker = NULL;
		fprintf(stderr, "reuse worker slot failed\n");
		goto cleanup;
	}
	pWorker = NULL;
	printf("reuse slot : %u\n", u32Slot);

	for ( i = 1; i <= pPool->Count; i++ ) {
		DemoWorker* pWorker = (DemoWorker*)xrtPtrArrayGet(pPool, i);
		if ( pWorker == NULL ) {
			printf("[%u] (null)\n", i);
		} else {
			printf("[%u] %s => %d\n", i, pWorker->sTag, pWorker->iWorkerID);
		}
	}

	iRet = 0;

cleanup:
	if ( pWorker != NULL ) {
		procFreeWorker(pWorker);
	}
	if ( pPool != NULL ) {
		for ( i = 1; i <= pPool->Count; i++ ) {
			DemoWorker* pWorker = (DemoWorker*)xrtPtrArrayGet(pPool, i);
			if ( pWorker != NULL ) {
				procFreeWorker(pWorker);
			}
		}
		xrtPtrArrayDestroy(pPool);
	}
	xrtUnit();
	return iRet;
}
```

这个升级例子真正要教会你的事有 6 个：

1. `Remove()` 是压缩列表路线，`Set(NULL) + AddAlt()` 是稳定槽位路线
2. `AddAlt()` 只会复用真正为 `NULL` 的槽位
3. 留空槽前，对象释放要你自己做
4. 复用空槽时，`Count` 不会因为“补洞”而额外增长
5. 这条路线很适合连接池、session 槽和 worker 槽
6. 如果你其实不需要稳定槽位，只想让索引始终连续，那就回到 `Remove()`


## 5. 再升级一步：按对象内容排序，但只搬指针

`ptrarray` 和 `array` 的一个关键区别是：

- `array` 排序时搬结构体本体
- `ptrarray` 排序时搬指针

这对“大对象、对象里自己还挂着其他资源”的场景会更自然。

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
	int iPriority;
	str sTitle;
} DemoJob;


static DemoJob* procCreateJob(int iPriority, str sTitle)
{
	DemoJob* pJob = (DemoJob*)xrtMalloc(sizeof(DemoJob));
	if ( pJob == NULL ) {
		return NULL;
	}

	pJob->iPriority = iPriority;
	pJob->sTitle = xrtCopyStr(sTitle, 0);
	if ( pJob->sTitle == NULL ) {
		xrtFree(pJob);
		return NULL;
	}

	return pJob;
}


static void procFreeJob(DemoJob* pJob)
{
	if ( pJob == NULL ) {
		return;
	}

	if ( pJob->sTitle != NULL ) {
		xrtFree(pJob->sTitle);
	}
	xrtFree(pJob);
}


static int procCompareJobPtr(const void* pLeft, const void* pRight)
{
	const DemoJob* pA = *(const DemoJob**)pLeft;
	const DemoJob* pB = *(const DemoJob**)pRight;
	return pB->iPriority - pA->iPriority;
}


int main(void)
{
	xparray pArr = NULL;
	DemoJob* pJob = NULL;
	uint32 i = 0;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pArr = xrtPtrArrayCreate(XRT_OBJMODE_LOCAL);
	if ( pArr == NULL ) {
		goto cleanup;
	}

	pJob = procCreateJob(3, "ship-report");
	if ( (pJob == NULL) || (xrtPtrArrayAppend(pArr, pJob) == 0) ) {
		if ( pJob != NULL ) {
			procFreeJob(pJob);
			pJob = NULL;
		}
		fprintf(stderr, "append ship-report failed\n");
		goto cleanup;
	}
	pJob = NULL;

	pJob = procCreateJob(9, "flush-cache");
	if ( (pJob == NULL) || (xrtPtrArrayAppend(pArr, pJob) == 0) ) {
		if ( pJob != NULL ) {
			procFreeJob(pJob);
			pJob = NULL;
		}
		fprintf(stderr, "append flush-cache failed\n");
		goto cleanup;
	}
	pJob = NULL;

	pJob = procCreateJob(5, "rotate-log");
	if ( (pJob == NULL) || (xrtPtrArrayAppend(pArr, pJob) == 0) ) {
		if ( pJob != NULL ) {
			procFreeJob(pJob);
			pJob = NULL;
		}
		fprintf(stderr, "append rotate-log failed\n");
		goto cleanup;
	}
	pJob = NULL;

	if ( !xrtPtrArraySort(pArr, procCompareJobPtr) ) {
		fprintf(stderr, "sort jobs failed\n");
		goto cleanup;
	}

	for ( i = 1; i <= pArr->Count; i++ ) {
		DemoJob* pJob = (DemoJob*)xrtPtrArrayGet(pArr, i);
		printf("[%u] %s => %d\n", i, pJob->sTitle, pJob->iPriority);
	}

	iRet = 0;

cleanup:
	if ( pJob != NULL ) {
		procFreeJob(pJob);
	}
	if ( pArr != NULL ) {
		for ( i = 1; i <= pArr->Count; i++ ) {
			DemoJob* pJob = (DemoJob*)xrtPtrArrayGet(pArr, i);
			if ( pJob != NULL ) {
				procFreeJob(pJob);
			}
		}
		xrtPtrArrayDestroy(pArr);
	}
	xrtUnit();
	return iRet;
}
```

这段例子里最关键的点只有两个：

1. 排序回调比较的是“指针指向的对象内容”
2. 容器里移动的是指针，不是对象本体


## 6. 什么时候该用 `xrtPtrArrayInit()` / `xrtPtrArrayUnit()`

如果 `ptrarray` 只是你自己对象里的一个成员，通常没必要每次都堆上 `Create()` 一个独立管理器。

这时更自然的写法是：

```c
#include "xrt.h"
#include <string.h>

typedef struct {
	xparray_struct tSessions;
} DemoState;


void procInitState(DemoState* pState)
{
	memset(pState, 0, sizeof(*pState));
	xrtPtrArrayInit(&pState->tSessions, XRT_OBJMODE_LOCAL);
}


void procUnitState(DemoState* pState)
{
	xrtPtrArrayUnit(&pState->tSessions);
}
```

这组 API 的价值和 `buffer`、`array` 一样：

- 管理器结构体由你自己持有
- `ptrarray` 只负责内部那块可增长的 `ptr*` 内存


## 7. Shared 模式该怎么理解

当 `ptrarray` 需要进入跨线程主线时，才考虑：

- `XRT_OBJMODE_SHARED`

这时更稳的理解是：

- `Get()` / `Set()` / `Append()` 这些单步调用
	- 仍然遵守 owner/shared 规则
- 但如果你要做复合操作
	- 例如“先看 `Count`，再遍历，再追加”
	- 或者要稳定读 `Memory`
	- 就应该显式锁

最小模式大概像这样：

```c
#include "xrt.h"

void procPushSharedHandle(xparray pArr, ptr pHandle)
{
	if ( !xrtPtrArrayLock(pArr) ) {
		return;
	}

	xrtPtrArrayAppend(pArr, pHandle);
	xrtPtrArrayUnlock(pArr);
}
```

尤其要注意的是：

- `xrtPtrArrayGet_Unsafe()` / `xrtPtrArrayGet_Inline()`
	- 不做边界检查
	- 也不替你建立稳定并发视图

所以在 shared 路径里，如果你想用 unsafe / inline，前提通常是：

- 你已经有外部锁
- 或者你非常明确当前访问边界


## 8. 它和 `array`、`list` 的区别

### 8.1 和 `array` 的区别

- `array`
	- 存结构体本体
	- 更适合小而固定的记录
	- 插入/排序时搬的是整个元素
- `ptrarray`
	- 存对象指针
	- 更适合对象本体独立分配的场景
	- 插入/排序时搬的是指针


### 8.2 和 `list` 的区别

- `ptrarray`
	- 连续索引访问快
	- 适合按成员编号管理
	- 中间插入/删除仍然要移动后续指针
- `list`
	- 更偏键值与节点式组织
	- 不再以“第 N 个成员”作为核心心智模型


## 9. 如果只有一条主线程，会卡在哪里

`ptrarray` 不会像 I/O 那样“卡在等待上”，但它会在主线程里消耗：

- 扩容
- `memmove`
- `qsort`
- 大量对象构造和析构

所以最常见的问题不是“线程等待”，而是：

- 应该走槽位复用却反复 `Remove()`
- 应该 `Remove()` 压缩却留下太多洞
- 在热路径里不停扩容
- 把对象生命周期和容器生命周期混在一起


## 10. Windows / Linux 跨平台时最该记住的事

- `ptrarray` 自身是纯内存容器，平台差异主要来自你存进去的对象
- 如果对象里含有文件句柄、socket、线程句柄等平台资源，释放逻辑要自己分平台处理
- 不要把指针值本身当成可跨进程、可跨平台持久化的数据
- shared 模式下的 owner/锁语义应按当前主线理解，不要自行假设“跨线程随便读写都安全”


## 11. 常见错误

### 错误一：以为销毁 `ptrarray` 就会顺便释放所有对象

容器不会替你释放每个元素指向的堆对象。  
对象生命周期必须自己管理。

### 错误二：把 `Remove()` 和“留空槽”混成一回事

`Remove()` 会压缩数组并重排后续索引。  
要留空槽，应该 `Set(NULL)`，再配合 `AddAlt()`。

### 错误三：用了 `AddAlt()`，却从没把失效槽位设成 `NULL`

这样它当然找不到洞，最后只会一直 append。

### 错误四：把 `Sort()` 的比较函数写成比较对象本体地址参数

这里传进来的是“指向指针的指针”，不是对象本体。

### 错误五：缓存 `Memory` 或元素指针后，还继续插入、删除、排序

这些结构性操作会移动内部指针数组。  
缓存视图不能默认长期稳定。

### 错误六：本线程私有场景也默认开 shared

如果没有共享根对象需求，先走 `LOCAL` 更直接。


## 12. 建议继续阅读

- [PtrArray API](../api/api-ptrarray.md)
- [Array 入门：什么时候该用连续结构体数组，而不是 buffer 和指针数组](array-intro.md)
- [Stack 入门：什么时候该用固定深度工作栈，而不是数组或动态栈](stack-intro.md)
- [Stack API](../api/api-stack.md)
- [List API](../api/api-list.md)
- [Math 与 Hash 入门：随机数、约等于和稳定指纹不是一回事](math-hash-intro.md)

---

**一句话总结：** `ptrarray` 最适合承担“把一批对象指针按成员编号管理起来，并在需要时选择压缩删除或保留空槽复用”的角色；先把对象生命周期、槽位语义和 `1-based` 编号看清楚，这个模块就会比直接手搓 `void**` 稳得多。
