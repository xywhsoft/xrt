# Stack 入门：什么时候该用固定深度工作栈，而不是数组或动态栈

> 目标：把 `xrtStackCreate()`、`xrtStackPush()`、`xrtStackPushData()`、`xrtStackPushPtr()`、`xrtStackTop()`、`xrtStackPop()`、`xrtStackGetPos()` 讲成第 2 阶段第四条正式容器主线。读完这页后，你应该明确知道：什么时候问题本质上是“后进先出、最大深度已知”，为什么这时应该先用 `stack`，以及它和 `array / ptrarray / dynstack` 的边界到底是什么。

[返回教学文档](README.md)

---

## 1. 为什么 `stack` 应该接在 `ptrarray` 后面讲

学完 `buffer / array / ptrarray` 以后，你已经看过了：

- 字节拼装
- 固定大小元素连续管理
- 对象指针集合

但还有一类问题，它们最核心的不是“第几个成员”，而是：

- 最后进入的状态，要最先退出
- 最近压进去的工作项，要最先处理
- rollback / 回溯 / 递归展开时，需要严格 LIFO

这类问题的本质就是：

- 栈

而 `xrtStack` 的位置非常明确：

- 最大深度已知
- 想要一次分配
- 想要最短路径的 LIFO 容器
- 不想为了“不一定会发生的扩容”付出额外复杂度

所以它很适合放在 `ptrarray` 后面、`dynstack` 前面来讲。


## 2. 先把 8 个边界分清楚

### 2.1 `stack` 解决的是“后进先出”，不是通用顺序容器

`stack` 最适合：

- 解析器状态栈
- 回溯路径
- DFS 工作栈
- rollback / cleanup 栈

`stack` 不适合：

- 频繁中间插入删除
- 排序
- 按键查找
- 把它当成通用数组改来改去

所以更直接地说：

- 你在想“最近进去的那个，先出来”
	- 先想 `stack`
- 你在想“第 N 个成员、排序、删除、遍历管理”
	- 更可能该去看 `array / ptrarray`


### 2.2 `xrtStackCreate()` 一次分配固定深度，不会自动扩容

当前实现里：

- `xrtStackCreate(iMaxCount, iItemLength)`
	- 会一次性分配头部和整块栈内存
- `MaxCount`
	- 就是你的最大深度

后续如果栈满：

- `xrtStackPush()`
	- 返回 `NULL`
- `xrtStackPushData()` / `xrtStackPushPtr()`
	- 返回 `0`

不会发生：

- 自动扩容
- 自动搬移
- 自动切换到动态模式

所以如果你一开始就不知道最大深度，或者深度可能明显波动，那下一页通常就该看：

- `dynstack`


### 2.3 `stack` 的内存是连续的，`xrtStackDestroy` 本质上就是 `xrtFree`

`stack` 和前面几个容器有一个很大的实现差别：

- 头部
- 栈数据区

是一次性连续分配出来的。

所以：

- `xrtStackDestroy`
	- 直接就是 `xrtFree`

这也是为什么 `stack` 很轻：

- 没有单独的内部 buffer 释放流程
- 没有 owner/shared 元数据
- 没有动态扩容路径


### 2.4 `xrtStackPush()` 和 `xrtStackPushData()` 是两种不同压栈方式

这两个接口要明确分开理解：

- `xrtStackPush()`
	- 给你一个新栈槽的指针
	- 你自己把内容写进去
- `xrtStackPushData()`
	- 把你现成的一份结构体按 `ItemLength` 整块复制进去

所以：

- 你想原地填写新元素
	- 用 `Push()`
- 你已经有一份完整结构体
	- 用 `PushData()`

还有一个非常重要的补充是：

- `PushData()` 是整块浅拷贝

如果你的结构体里自己带指针、句柄、堆对象，它不会替你做深拷贝。


### 2.5 `Top()` / `Pop()` 返回的是栈内存里的槽位指针，不是独立副本

这条边界非常重要。

对于结构体栈：

- `xrtStackTop()`
- `xrtStackPop()`

返回的都是：

- 栈内部那一格槽位的地址

这意味着：

- 立刻读取、立刻处理
	- 没问题
- 长期缓存这个指针，然后继续 `Push()`
	- 就可能被后续写入覆盖

所以最稳的经验是：

- `Pop()` 后如果还要长期保留内容
	- 立刻复制出去


### 2.6 `GetPos()` 的位置是 **1-based 且从栈底开始**

这又是一个很容易混的点。

当前实现里：

- `xrtStackGetPos(stk, 1)`
	- 取的是第一个入栈的元素，也就是栈底
- `xrtStackTop(stk)`
	- 取的是最后一个入栈的元素，也就是栈顶

所以：

- `GetPos()` 适合做检查、调试、快照遍历
- 真正的 LIFO 操作还是 `Top / Pop`

不要把 `GetPos(1)` 理解成“离栈顶最近的那个”。


### 2.7 指针栈只是保存指针值，对象生命周期仍然由你自己管

如果你用：

- `xrtStackPushPtr()`
- `xrtStackTopPtr()`
- `xrtStackPopPtr()`

那这条线的含义就变成：

- 栈里存的是指针值
- 对象本体在别处

它非常适合：

- cleanup 栈
- 延迟释放列表
- 资源关闭顺序管理

但它同样不会：

- 自动释放对象
- 自动接管对象所有权

最稳的写法通常是：

- `PushPtr()` 时决定“压入什么资源”
- `PopPtr()` 后由你自己执行 `free / close / destroy`

另外，指针栈最干净的写法通常是：

- `xrtStackCreate(maxDepth, sizeof(ptr))`

这样语义最直接，不容易让读者误会。


### 2.8 `stack` 没有 owner/shared 语义，本身不是线程安全容器

这点和 `array / ptrarray` 很不一样。

`stack` 当前实现里没有：

- `LOCAL / SHARED`
- `Lock / Unlock`
- owner check

所以最稳的理解是：

- 它是普通容器
- 默认就是单线程、本地工作栈

如果你要跨线程共享一个 `stack`：

- 要么自己加外部同步
- 要么更直接地换成更合适的模型


## 3. 最小 DEMO：先用结构体栈做一条最小 LIFO 主线

第一步不要先上指针栈。  
先把最常见、也最稳定的主线走通：

- 创建固定深度结构体栈
- `Push()` 获得槽位并填写内容
- `Top()` 看栈顶
- `Pop()` 按 LIFO 退出

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
	int iDepth;
	char sLabel[32];
} DemoFrame;


int main(void)
{
	xstack pStack = NULL;
	DemoFrame* pFrame = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pStack = xrtStackCreate(8, sizeof(DemoFrame));
	if ( pStack == NULL ) {
		goto cleanup;
	}

	pFrame = (DemoFrame*)xrtStackPush(pStack);
	if ( pFrame == NULL ) {
		fprintf(stderr, "push root failed\n");
		goto cleanup;
	}
	pFrame->iDepth = 0;
	strcpy(pFrame->sLabel, "root");

	pFrame = (DemoFrame*)xrtStackPush(pStack);
	if ( pFrame == NULL ) {
		fprintf(stderr, "push parse-config failed\n");
		goto cleanup;
	}
	pFrame->iDepth = 1;
	strcpy(pFrame->sLabel, "parse-config");

	pFrame = (DemoFrame*)xrtStackPush(pStack);
	if ( pFrame == NULL ) {
		fprintf(stderr, "push start-http failed\n");
		goto cleanup;
	}
	pFrame->iDepth = 2;
	strcpy(pFrame->sLabel, "start-http");

	pFrame = (DemoFrame*)xrtStackTop(pStack);
	printf("top before pop : depth=%d label=%s\n", pFrame->iDepth, pFrame->sLabel);

	while ( (pFrame = (DemoFrame*)xrtStackPop(pStack)) != NULL ) {
		printf("leave frame    : depth=%d label=%s\n", pFrame->iDepth, pFrame->sLabel);
	}

	iRet = 0;

cleanup:
	if ( pStack != NULL ) {
		xrtStackDestroy(pStack);
	}
	xrtUnit();
	return iRet;
}
```

这个最小例子最想让你先记住的是：

1. `stack` 的核心不是“第几个成员”，而是严格 LIFO
2. `Push()` 返回的是新槽位地址，你自己负责把内容写进去
3. `Top()` 只看不弹，`Pop()` 会把深度减 1
4. `Pop()` 拿到的是栈内槽位地址，应该立刻消费，不要长期缓存


## 4. 升级 DEMO：用 `PushData()` 和 `GetPos()` 做检查点快照

当你已经会用 `Push()` 以后，下一步最值得学的是：

- 如果我手里已经有一份完整结构体
- 或者我想在不破坏栈的情况下检查整个路径
- 该怎么写

这时就要把：

- `xrtStackPushData()`
- `xrtStackGetPos()`

接进同一条主线。

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
	int iStep;
	int iScore;
} DemoCheckpoint;


int main(void)
{
	xstack pStack = NULL;
	DemoCheckpoint tItem = {0};
	DemoCheckpoint* pTop = NULL;
	uint32 i = 0;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pStack = xrtStackCreate(4, sizeof(DemoCheckpoint));
	if ( pStack == NULL ) {
		goto cleanup;
	}

	for ( i = 0; i < 4; i++ ) {
		tItem.iStep = (int)(i + 1);
		tItem.iScore = (int)((i + 1) * 10);
		if ( xrtStackPushData(pStack, &tItem) == 0 ) {
			fprintf(stderr, "push checkpoint failed at %u\n", i + 1);
			goto cleanup;
		}
	}

	if ( xrtStackPushData(pStack, &tItem) != 0 ) {
		fprintf(stderr, "overflow check failed\n");
		goto cleanup;
	}

	printf("snapshot from bottom to top:\n");
	for ( i = 1; i <= pStack->Count; i++ ) {
		DemoCheckpoint* pItem = (DemoCheckpoint*)xrtStackGetPos(pStack, i);
		printf("  pos=%u step=%d score=%d\n", i, pItem->iStep, pItem->iScore);
	}

	pTop = (DemoCheckpoint*)xrtStackTop(pStack);
	printf("current top: step=%d score=%d\n", pTop->iStep, pTop->iScore);
	iRet = 0;

cleanup:
	if ( pStack != NULL ) {
		xrtStackDestroy(pStack);
	}
	xrtUnit();
	return iRet;
}
```

这个升级例子真正想教会你的事有 5 个：

1. `PushData()` 适合“我已经有完整结构体”的场景
2. 栈满时它会返回 `0`，不会偷偷扩容
3. `GetPos()` 的位置是从栈底开始的 1-based 编号
4. `Top()` 适合看当前工作点，不破坏栈
5. 只要最大深度已知，固定栈会比“先上动态容器”更直接


## 5. 指针栈怎么用：把资源释放顺序交给 LIFO

当你的问题从“结构体检查点”切到“资源回收顺序”时，最自然的升级就是：

- `PushPtr()`
- `PopPtr()`

例如：

- 初始化多个模块
- 中途失败要反向回滚
- 或者程序退出时要按相反顺序释放资源

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xstack pCleanup = NULL;
	str sItem = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pCleanup = xrtStackCreate(8, sizeof(ptr));
	if ( pCleanup == NULL ) {
		goto cleanup;
	}

	sItem = xrtCopyStr("open-log-file", 0);
	if ( (sItem == NULL) || (xrtStackPushPtr(pCleanup, sItem) == 0) ) {
		if ( sItem != NULL ) {
			xrtFree(sItem);
			sItem = NULL;
		}
		goto cleanup;
	}
	sItem = NULL;

	sItem = xrtCopyStr("start-worker-pool", 0);
	if ( (sItem == NULL) || (xrtStackPushPtr(pCleanup, sItem) == 0) ) {
		if ( sItem != NULL ) {
			xrtFree(sItem);
			sItem = NULL;
		}
		goto cleanup;
	}
	sItem = NULL;

	sItem = xrtCopyStr("bind-http-listener", 0);
	if ( (sItem == NULL) || (xrtStackPushPtr(pCleanup, sItem) == 0) ) {
		if ( sItem != NULL ) {
			xrtFree(sItem);
			sItem = NULL;
		}
		goto cleanup;
	}
	sItem = NULL;

	printf("rollback order:\n");
	while ( (sItem = (str)xrtStackPopPtr(pCleanup)) != NULL ) {
		printf("  cleanup => %s\n", sItem);
		xrtFree(sItem);
	}

	iRet = 0;

cleanup:
	if ( sItem != NULL ) {
		xrtFree(sItem);
	}
	if ( pCleanup != NULL ) {
		while ( (sItem = (str)xrtStackPopPtr(pCleanup)) != NULL ) {
			xrtFree(sItem);
		}
		xrtStackDestroy(pCleanup);
	}
	xrtUnit();
	return iRet;
}
```

这段例子里最关键的教学点只有 4 个：

1. 指针栈只负责保存资源句柄或对象指针
2. 真正的释放动作仍然由你自己执行
3. LIFO 很适合“初始化顺序”和“清理顺序”天然相反的场景
4. 这条线特别适合错误回滚和分阶段启动


## 6. 它和 `array`、`ptrarray`、`dynstack` 的区别

### 6.1 和 `array` / `ptrarray` 的区别

- `array / ptrarray`
	- 更强调“第几个成员”
	- 适合通用顺序管理
- `stack`
	- 更强调“最近压入的那个，先处理”
	- 适合 LIFO 工作流


### 6.2 和 `dynstack` 的区别

- `stack`
	- 固定深度
	- 一次分配
	- 没有自动扩容
	- 已知上限时最直接
- `dynstack`
	- 可继续增长
	- 适合最大深度事先说不准的场景

如果你只是因为“懒得估最大深度”就直接跳过 `stack`，很多本来更简单的场景会被你写复杂。


## 7. 如果只有一条主线程，会卡在哪里

`stack` 本身不会等待 I/O，也没有复杂同步，但它会在主线程里暴露两类问题：

- 深度估错导致溢出
- 把栈槽指针拿出去长期缓存，后面又继续写入

所以 `stack` 这条线真正要防的不是“阻塞”，而是：

- 边界没算清
- 生命周期没说清
- 明明该用动态栈，却硬塞进固定深度


## 8. Windows / Linux 跨平台时最该记住的事

- `stack` 自身是纯内存容器，行为基本不依赖平台
- 如果栈元素里含有平台资源句柄，释放动作还是要你自己按平台处理
- `sizeof(ptr)` 在不同平台可能不同，指针栈创建时应直接写 `sizeof(ptr)`
- 不要把栈内指针值当成可持久化、可跨进程的数据


## 9. 常见错误

### 错误一：把 `stack` 当成通用数组来用

如果你一直在想“我要删中间那个”，通常就已经选错容器了。

### 错误二：以为栈满时会自动扩容

固定栈不会自动扩。  
满了就是 `NULL` / `0`。

### 错误三：`Pop()` 后长期保存返回指针，又继续 `Push()`

这个指针指向的是栈内部槽位，不是独立副本。  
后续写入可能覆盖它。

### 错误四：把 `GetPos(1)` 当成栈顶

`GetPos(1)` 是栈底，不是栈顶。  
栈顶请用 `Top()`。

### 错误五：把指针压进栈后，就以为容器会替你释放对象

指针栈只保存指针值。  
对象释放仍然由你自己负责。

### 错误六：跨线程共享同一个 `stack` 却不加外部同步

`stack` 当前没有 owner/shared 和锁语义。  
默认按单线程本地工作栈理解。


## 10. 建议继续阅读

- [Stack API](../api/api-stack.md)
- [DynStack 入门：什么时候该用可增长工作栈，而不是固定深度栈](dynstack-intro.md)
- [DynStack API](../api/api-dynstack.md)
- [PtrArray 入门：什么时候该管理对象指针，而不是搬运整个结构体](ptrarray-intro.md)
- [Array 入门：什么时候该用连续结构体数组，而不是 buffer 和指针数组](array-intro.md)
- [Task Group 入门：从统一等待走到结构化收口](task-group-intro.md)

---

**一句话总结：** `stack` 最适合承担“最大深度已知、严格后进先出”的工作流；先把固定深度、栈槽生命周期和资源回滚顺序这三件事看清楚，这个容器会非常稳。
