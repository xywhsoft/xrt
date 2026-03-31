# DynStack 入门：什么时候该用可增长工作栈，而不是固定深度栈

> 目标：把 `xrtDynStackCreate()`、`xrtDynStackInit()`、`xrtDynStackPush()`、`xrtDynStackPushData()`、`xrtDynStackPushPtr()`、`xrtDynStackTop()`、`xrtDynStackPop()`、`xrtDynStackGetPos()` 讲成第 2 阶段第五条正式容器主线。读完这页后，你应该明确知道：什么时候问题仍然是“后进先出”，但最大深度已经不稳定；以及 `dynstack` 和 `stack` 在块管理、判空责任、指针生命周期上的区别到底是什么。

[返回教学文档](README.md)

---

## 1. 为什么 `dynstack` 应该接在 `stack` 后面讲

学完 `stack` 以后，最自然的下一步通常就是：

- 我的问题还是严格 LIFO
- 但最大深度很难事先说准
- 我不想为了极端情况把固定栈开得特别大
- 我需要“继续像栈一样写”，但又不想在深度上被卡死

这时问题的本质并没有从“栈”变成“数组”：

- 你仍然要 LIFO
- 只是上限不再稳定

这就是 `dynstack` 的位置。

它不是：

- 一个支持排序的通用容器
- 一个带共享锁的并发栈
- 一个“随便乱取第 N 个元素”的数组替代品

它更像：

- 一个按块增长的工作栈


## 2. 先把 9 个边界分清楚

### 2.1 `dynstack` 解决的是“深度不稳定的 LIFO”，不是“无限大数组”

`dynstack` 最适合：

- 深度优先搜索
- 解释器 / 解析器状态栈
- 撤销 / 重做栈
- 回滚动作栈
- 深度可能阶段性暴涨的工作流

它不适合：

- 频繁中间插入删除
- 排序
- 需要共享所有权和内建锁的跨线程容器

所以更直接地说：

- 你在想“最近进去的先出来，但深度很难提前算”
	- 先想 `dynstack`
- 你在想“第几个成员、排序、删除、查找”
	- 更可能该去看 `array / ptrarray / dict`


### 2.2 `dynstack` 不是一块连续大内存，而是按 256 元素一块增长

当前实现里，`dynstack` 不是像 `stack` 那样一次性把整块数据都分好。

它的核心结构是：

- `Count`
	- 当前栈深度
- `MMU`
	- 一个 `ptrarray`
	- 里面存放每个内存块的指针

每个块固定存：

- `256` 个元素

所以：

- 前 256 个元素
	- 在第 0 块
- 第 257 到 512 个元素
	- 在第 1 块

这就是为什么它能在不预知最大深度时继续增长。


### 2.3 `MMU` 是内部块管理，不是“你可以随便跨线程共享的公开容器”

实现里 `MMU` 用的是：

- `xparray_struct`

而且初始化时固定走：

- `xrtPtrArrayInit(&objSTK->MMU, XRT_OBJMODE_LOCAL)`

这意味着：

- `dynstack` 当前主线仍然是本地工作栈
- 它没有 `stack` 那样的固定一块内存
- 也没有 `array / ptrarray` 那种面向用户的 owner/shared 公开语义

所以不要把：

- “内部用了 `ptrarray`”

误解成：

- “那它就天然是 shared 容器”


### 2.4 `Push()` 可能触发新块分配，而 `stack` 不会

`stack` 的关键特点是：

- 固定深度
- 满了就失败

`dynstack` 则不同：

- 如果当前块够用
	- 直接写进去
- 如果当前块不够
	- 新分配一个 256 元素块
	- 再把块指针挂进 `MMU`

所以它真正多出来的成本是：

- 多一级块定位
- 偶尔新建块
- 多一个 `MMU` 管理器

而不是“每次 push 都 realloc 整体内存”。


### 2.5 `Top()` / `Pop()` 当前实现不帮你判空

这是这页最重要的边界，也是当前 API 文档最容易讲漏的一点。

当前实现里：

- `xrtDynStackTop()`
	- 直接走 `xrtDynStackGetPos_Unsafe(objSTK, objSTK->Count)`
- `xrtDynStackPop()`
	- 先取 `Top()`
	- 再直接 `Count--`

也就是说：

- 如果 `Count == 0`
	- 你再调 `Top()` 或 `Pop()`
	- 就已经越过了安全边界

所以这组接口的正确用法是：

- 先判断 `objSTK->Count > 0`
- 再调 `Top()` 或 `Pop()`

同理：

- `TopPtr()`
- `PopPtr()`

也要遵守这个规则。


### 2.6 `GetPos()` 是安全版，`GetPos_Unsafe()` 才是不检查边界

这组接口要和 `Top/Pop` 分开理解。

- `xrtDynStackGetPos()`
	- 会检查 `iPos`
	- 越界返回 `NULL`
- `xrtDynStackGetPos_Unsafe()`
	- 不检查边界

而位置坐标仍然是：

- **1-based**
- 并且从栈底开始

所以：

- 第一个入栈元素
	- `GetPos(1)`
- 当前栈顶
	- `Top()`


### 2.7 `PushData()` 仍然是整块浅拷贝，不会替你处理深拷贝

和 `stack` 一样：

- `PushData()` 会把 `ItemLength` 这一整块字节复制进去

它不会替你处理：

- 结构体里的字符串深拷贝
- 句柄复制
- 对象引用计数

所以如果元素里有独立资源：

- 你仍然要自己定义复制和释放策略


### 2.8 指针栈只是保存指针值，对象释放责任仍然在你手里

如果你用：

- `xrtDynStackPushPtr()`
- `xrtDynStackTopPtr()`
- `xrtDynStackPopPtr()`

那它和 `stack` 一样，含义就是：

- 栈里保存的是指针值
- 对象本体在别处

适合：

- 清理动作
- 资源句柄
- 延迟释放对象

不适合：

- 以为容器会自动释放对象


### 2.9 `dynstack` 会延迟释放块，不是“pop 一个就缩一格内存”

当前实现里，`Pop()` / `PopPtr()` 出栈后会检查：

- `(MMU.Count << 8) > (Count + 288)`

满足时才释放最后一个块。

这条策略真正想解决的是：

- 避免在 256 边界附近反复 push / pop 时不停分配 / 释放

所以更稳的理解是：

- `dynstack` 会增长
- 也会回收
- 但回收是带滞后的，不是每次都立刻缩


## 3. 最小 DEMO：先把它当成“深度不稳定的结构体工作栈”

第一步不要先上指针栈。  
先把最常见、也最稳定的主线走通：

- 创建动态结构体栈
- `Push()` 压入工作帧
- 始终在 `Count > 0` 时才 `Top/Pop`
- 用 `while (Count > 0)` 跑完整个 LIFO 过程

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
	int iNode;
	int iDepth;
	char sLabel[32];
} DemoFrame;


int main(void)
{
	xdynstack pStack = NULL;
	DemoFrame* pFrame = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pStack = xrtDynStackCreate(sizeof(DemoFrame));
	if ( pStack == NULL ) {
		goto cleanup;
	}

	pFrame = (DemoFrame*)xrtDynStackPush(pStack);
	if ( pFrame == NULL ) {
		fprintf(stderr, "push root failed\n");
		goto cleanup;
	}
	pFrame->iNode = 1;
	pFrame->iDepth = 0;
	strcpy(pFrame->sLabel, "root");

	pFrame = (DemoFrame*)xrtDynStackPush(pStack);
	if ( pFrame == NULL ) {
		fprintf(stderr, "push parse-config failed\n");
		goto cleanup;
	}
	pFrame->iNode = 10;
	pFrame->iDepth = 1;
	strcpy(pFrame->sLabel, "parse-config");

	pFrame = (DemoFrame*)xrtDynStackPush(pStack);
	if ( pFrame == NULL ) {
		fprintf(stderr, "push open-http failed\n");
		goto cleanup;
	}
	pFrame->iNode = 11;
	pFrame->iDepth = 2;
	strcpy(pFrame->sLabel, "open-http");

	while ( pStack->Count > 0 ) {
		pFrame = (DemoFrame*)xrtDynStackTop(pStack);
		printf("top   => node=%d depth=%d label=%s\n", pFrame->iNode, pFrame->iDepth, pFrame->sLabel);

		pFrame = (DemoFrame*)xrtDynStackPop(pStack);
		printf("pop   => node=%d depth=%d label=%s\n", pFrame->iNode, pFrame->iDepth, pFrame->sLabel);
	}

	iRet = 0;

cleanup:
	if ( pStack != NULL ) {
		xrtDynStackDestroy(pStack);
	}
	xrtUnit();
	return iRet;
}
```

这个最小例子最想让你先记住的是：

1. `dynstack` 的核心仍然是 LIFO，不是“无限数组”
2. `Top/Pop` 前一定先看 `Count`
3. `Push()` 失败通常意味着分配新块失败
4. 写法上它和 `stack` 很像，只是容量边界从“固定”变成“按块增长”


## 4. 升级 DEMO：跨过 256 边界，把“按块增长”讲清楚

当你已经会正常使用 `dynstack`，下一步最值得学的是：

- 它到底怎么跨过固定深度
- 为什么第 257 个元素并不会把前 256 个整体搬走

最直接的方式就是：

- 压入超过 256 个元素
- 观察 `MMU.Count`
- 再用 `GetPos()` 验证前后两段都还能正确访问

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
	int iIndex;
} DemoItem;


int main(void)
{
	xdynstack pStack = NULL;
	DemoItem tItem = {0};
	DemoItem* pCheck = NULL;
	uint32 i = 0;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pStack = xrtDynStackCreate(sizeof(DemoItem));
	if ( pStack == NULL ) {
		goto cleanup;
	}

	for ( i = 0; i < 300; i++ ) {
		tItem.iIndex = (int)(i + 1);
		if ( xrtDynStackPushData(pStack, &tItem) == 0 ) {
			fprintf(stderr, "push item failed at %u\n", i + 1);
			goto cleanup;
		}
	}

	printf("count      : %u\n", pStack->Count);
	printf("mmu blocks : %u\n", pStack->MMU.Count);
	printf("mmu alloc  : %u\n", pStack->MMU.AllocCount);

	pCheck = (DemoItem*)xrtDynStackGetPos(pStack, 1);
	printf("pos 1      : %d\n", pCheck->iIndex);

	pCheck = (DemoItem*)xrtDynStackGetPos(pStack, 256);
	printf("pos 256    : %d\n", pCheck->iIndex);

	pCheck = (DemoItem*)xrtDynStackGetPos(pStack, 257);
	printf("pos 257    : %d\n", pCheck->iIndex);

	pCheck = (DemoItem*)xrtDynStackGetPos(pStack, 300);
	printf("pos 300    : %d\n", pCheck->iIndex);

	iRet = 0;

cleanup:
	if ( pStack != NULL ) {
		xrtDynStackDestroy(pStack);
	}
	xrtUnit();
	return iRet;
}
```

这个升级例子真正想教会你的事有 5 个：

1. `dynstack` 不是靠整体扩容，而是靠新增块继续长
2. 第 257 个元素会落到新块，不会把前 256 个整体搬家
3. `MMU.Count` 更像“当前已经持有多少块”
4. `GetPos()` 仍然按 1-based、从栈底数
5. 只要你需要“LIFO + 深度不可预知”，这条线会比固定栈稳得多


## 5. 指针版 DEMO：把 rollback 栈升级成“可增长清理栈”

当你的问题从“结构体工作帧”切到“资源回滚”，并且回滚动作数量很难预估时，最自然的升级就是：

- 指针版 `dynstack`

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xdynstack pCleanup = NULL;
	str sItem = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pCleanup = xrtDynStackCreate(sizeof(ptr));
	if ( pCleanup == NULL ) {
		goto cleanup;
	}

	for ( int i = 0; i < 3; i++ ) {
		sItem = xrtFormat("cleanup-step-%d", i + 1);
		if ( (sItem == NULL) || (xrtDynStackPushPtr(pCleanup, sItem) == 0) ) {
			if ( sItem != NULL ) {
				xrtFree(sItem);
				sItem = NULL;
			}
			fprintf(stderr, "push cleanup item failed\n");
			goto cleanup;
		}
		sItem = NULL;
	}

	while ( pCleanup->Count > 0 ) {
		sItem = (str)xrtDynStackPopPtr(pCleanup);
		printf("rollback => %s\n", sItem);
		xrtFree(sItem);
	}

	iRet = 0;

cleanup:
	if ( sItem != NULL ) {
		xrtFree(sItem);
	}
	if ( pCleanup != NULL ) {
		while ( pCleanup->Count > 0 ) {
			sItem = (str)xrtDynStackPopPtr(pCleanup);
			xrtFree(sItem);
		}
		xrtDynStackDestroy(pCleanup);
	}
	xrtUnit();
	return iRet;
}
```

这个例子真正要表达的是：

1. 指针版 `dynstack` 适合“动作数量可能越来越多”的回滚栈
2. 栈里放的是指针值，不是对象副本
3. `PopPtr()` 前一样要先检查 `Count`
4. 真正的对象释放仍然由你自己负责


## 6. 什么时候该用 `xrtDynStackInit()` / `xrtDynStackUnit()`

如果 `dynstack` 只是你自己对象里的一个成员，通常没必要每次都单独堆上 `Create()` 一个管理器。

这时更自然的写法是：

```c
#include "xrt.h"
#include <string.h>

typedef struct {
	xdynstack_struct tUndo;
	xdynstack_struct tRedo;
} DemoEditor;


void procInitEditor(DemoEditor* pEditor)
{
	memset(pEditor, 0, sizeof(*pEditor));
	xrtDynStackInit(&pEditor->tUndo, sizeof(ptr));
	xrtDynStackInit(&pEditor->tRedo, sizeof(ptr));
}


void procUnitEditor(DemoEditor* pEditor)
{
	xrtDynStackUnit(&pEditor->tUndo);
	xrtDynStackUnit(&pEditor->tRedo);
}
```

这组 API 的价值和前面的容器一样：

- 管理器结构体由你自己持有
- `dynstack` 只负责内部块和 `MMU`


## 7. 它和 `stack`、`array`、`ptrarray` 的区别

### 7.1 和 `stack` 的区别

- `stack`
	- 固定深度
	- 一次分配
	- 满了就失败
- `dynstack`
	- 深度可继续长
	- 按块增长
	- 需要多一级块管理


### 7.2 和 `array / ptrarray` 的区别

- `array / ptrarray`
	- 更偏通用顺序容器
	- 核心心智模型是“第几个成员”
- `dynstack`
	- 核心心智模型仍然是 LIFO
	- `GetPos()` 只是辅助检查，不是主要操作路径


## 8. 如果只有一条主线程，会卡在哪里

`dynstack` 自身不会等待 I/O，但它会暴露三类常见问题：

- 忘了判空就 `Top/Pop`
- 把 `GetPos_Unsafe()` 当普通访问接口乱用
- 元素很多时，在 256 块边界附近反复读写却没理解块释放滞后

所以它最需要强调的不是“并发”，而是：

- 安全边界
- 元素生命周期
- 块式增长和回收的心智模型


## 9. Windows / Linux 跨平台时最该记住的事

- `dynstack` 自身是纯内存容器，平台差异主要来自你压进去的元素
- 如果是指针栈，`sizeof(ptr)` 可能随平台位数变化，创建时直接写 `sizeof(ptr)` 最稳
- 栈里存放的指针值不能当成可持久化、可跨进程数据
- 元素里如果有平台资源句柄，释放动作仍然要你自己按平台处理


## 10. 常见错误

### 错误一：把 `Top()` / `Pop()` 当成会自动判空的安全接口

当前实现不是这样。  
调用前先检查 `Count > 0`。

### 错误二：以为 `dynstack` 会整体 `realloc` 一块更大的连续内存

它实际是按 256 元素分块增长，不是整块搬迁。

### 错误三：把 `GetPos()` 当主操作路径，最后把栈写成数组

如果你主要在做随机位置管理，通常已经选错容器了。

### 错误四：用了 `PushData()`，却以为里面的动态成员也会自动深拷贝

`PushData()` 只是字节级复制。

### 错误五：把指针压进 `dynstack` 后，就以为容器会自动释放对象

不会。  
对象释放责任仍然在你手里。

### 错误六：把 `dynstack` 当成共享线程安全容器

当前主线没有公开 shared / lock 语义。  
默认按本地工作栈理解。


## 11. 建议继续阅读

- [DynStack API](../api/api-dynstack.md)
- [Stack 入门：什么时候该用固定深度工作栈，而不是数组或动态栈](stack-intro.md)
- [PtrArray 入门：什么时候该管理对象指针，而不是搬运整个结构体](ptrarray-intro.md)
- [List API](../api/api-list.md)
- [Task Group 入门：从统一等待走到结构化收口](task-group-intro.md)

---

**一句话总结：** `dynstack` 最适合承担“仍然是严格 LIFO，但最大深度说不准”的工作流；先把按块增长、`Top/Pop` 判空责任和指针生命周期看清楚，它会比盲目开一个超大固定栈稳得多。
