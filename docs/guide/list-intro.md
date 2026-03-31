# List 入门：什么时候该用整数键列表，而不是数组、字典或连续索引

> 目标：把 `xrtListCreate()`、`xrtListSet()`、`xrtListSetPtr()`、`xrtListGet()`、`xrtListGetPtr()`、`xrtListRemove()`、`xrtListWalk()`、`xrtListLock()` / `xrtListUnlock()` 讲成第 2 阶段第七条正式容器主线。读完这页后，你应该明确知道：什么时候问题已经不是“第几个连续成员”，而是“某个整数 key 上有没有值”；以及 `list` 和 `array / dict` 在键类型、稀疏索引、值槽位和 shared 边界上的区别到底是什么。

[返回教学文档](README.md)

---

## 1. 为什么 `list` 应该接在 `dict` 后面讲

学完 `dict` 以后，最自然的下一步通常就是：

- 我的问题确实是“按键查值”
- 但 key 根本不是字符串，也不是任意二进制
- 我的 key 天生就是整数
- 还可能有负数、跳号和稀疏位置

这时问题就落在 `list` 身上了。

它不是：

- 连续数组
- 插入顺序列表
- 简化版 `ptrarray`

它更像：

- 一个以 `int64` 为键的稀疏整数映射

所以把它放在 `dict` 后面最合适，因为两者核心心智模型很像：

- 都是按 key 找 value

真正不同的是：

- `dict` 的 key 是任意字节序列
- `list` 的 key 固定是 `int64`


## 2. 先把 9 个边界分清楚

### 2.1 `list` 是整数键映射，不是“可自动补洞的数组”

`list` 最适合：

- 稀疏索引
- 大跨度整数 key
- 负数 key
- ID -> value 映射

它不适合：

- 你想要第 1 个、第 2 个、第 3 个连续成员
- 你想按下标频繁顺序扫描整个巨大区间

所以更直接地说：

- 你在想“某个整数 key 对应什么值”
	- 先想 `list`
- 你在想“第 N 个连续成员”
	- 更可能该去看 `array / ptrarray`


### 2.2 `list` 只存实际出现的 key，所以特别适合稀疏索引

当前实现基于：

- AVLTree

这意味着它不会像连续数组那样，为了一个很大的 key 去提前开满中间所有槽位。

例如你只写了：

- `-5`
- `0`
- `1000000`

那它只存：

- 3 个节点

不会因为 `1000000` 就去分配一百万个位置。

这就是它和数组最本质的区别。


### 2.3 `xrtListSet()` 返回的是值槽位指针，不是值副本

这一点和 `dict` 完全同源。

`xrtListSet()` 的核心心智模型是：

- 取到这个整数 key 对应的值槽位

也就是说：

- key 不存在
	- 创建节点
	- 返回新值槽位
	- `*bNewRet = TRUE`
- key 已存在
	- 返回现有值槽位
	- `*bNewRet = FALSE`

所以它特别适合：

- 计数
- 状态表
- get-or-create
- 结构体值映射


### 2.4 指针值模式同样应该按 `sizeof(ptr)` 创建

如果你要让 `list` 存的是：

- 一个指针值

那更稳的创建方式是：

- `xrtListCreate(sizeof(ptr), XRT_OBJMODE_LOCAL)`

然后配合：

- `xrtListSetPtr()`
- `xrtListGetPtr()`
- `xrtListRemovePtr()`

这样心智模型最清楚：

- 值区里就是一个 `ptr`


### 2.5 `list` 不自动释放值指向的对象

和 `dict` 一样，`list` 负责的是：

- 节点
- 键
- 值槽位

它不会替你处理：

- 指针值指向的堆对象

所以如果你存的是：

- 字符串指针
- 会话对象
- 业务对象

在销毁 `list` 之前，记得先把这些对象自己释放掉。


### 2.6 `list` 的遍历顺序按整数 key 升序，不是插入顺序

当前实现基于 AVLTree，所以遍历顺序是：

- 按 key 有序

而不是：

- 你当初插入时的先后顺序

这意味着：

- `-5`
- `0`
- `10`
- `1000`

遍历时会按这个顺序出来。

如果你在意“谁先插入”，那 `list` 就不是那个容器。


### 2.7 `xrtListWalk()` 的回调返回值语义是“TRUE 中断，FALSE 继续”

这点和 `dict` 一样，也必须单独强调。

当前实现里，回调如果返回：

- `TRUE`
	- 中断遍历
- `FALSE`
	- 继续遍历

所以如果你只是想把整个列表扫完，回调里通常应该：

- `return FALSE;`


### 2.8 `LIST_FOREACH` / `LIST_FOREACH_TYPE` 更适合业务代码直写

在教学和业务代码里，很多时候你会更想这样写：

```c
LIST_FOREACH_TYPE(pList, iKey, pVal, DemoValue*)
{
	/* 直接使用 iKey / pVal */
}
```

这比单独拆一个 walk 回调更直观。

不过仍然要注意：

- 普通值模式里，`pVal`
	- 是值槽位指针
- 指针值模式里，`pVal`
	- 通常应该写成 `ptr*`、`str*`
	- 再通过 `*pVal` 取出真正的值


### 2.9 `LOCAL / SHARED` 和锁边界要按当前主线理解

`list` 当前也已经进入 owner/shared 双模式：

- `xrtListCreate(..., XRT_OBJMODE_LOCAL)`
	- 本线程默认快路径
- `xrtListCreate(..., XRT_OBJMODE_SHARED)`
	- shared root 主线

最稳的理解是：

- 本线程私有映射
	- 用 `LOCAL`
- 跨线程共享根对象
	- 用 `SHARED`
	- 遍历、复合更新、稳定视图再显式 `Lock / Unlock`


## 3. 最小 DEMO：先用结构体值模式做一张稀疏整数表

第一步不要先上指针值模式。  
先把最常见、也最稳定的主线走通：

- 创建一个 local 列表
- 用几个跨度很大的整数 key 写入结构体值
- 再按 key 读回和统计

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
	int iStatus;
	char sTag[32];
} DemoSlot;


static void procSetSlot(xlist pList, int64 iKey, const char* sTag, int iStatus)
{
	bool bNew = FALSE;
	DemoSlot* pSlot = (DemoSlot*)xrtListSet(pList, iKey, &bNew);
	if ( pSlot == NULL ) {
		return;
	}

	if ( bNew ) {
		memset(pSlot, 0, sizeof(*pSlot));
	}

	pSlot->iStatus = iStatus;
	strcpy(pSlot->sTag, sTag);
}


int main(void)
{
	xlist pList = NULL;
	DemoSlot* pSlot = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pList = xrtListCreate(sizeof(DemoSlot), XRT_OBJMODE_LOCAL);
	if ( pList == NULL ) {
		goto cleanup;
	}

	procSetSlot(pList, -5, "left-boundary", 10);
	procSetSlot(pList, 0, "center", 20);
	procSetSlot(pList, 1000, "far-slot", 30);

	pSlot = (DemoSlot*)xrtListGet(pList, 1000);
	if ( pSlot != NULL ) {
		printf("key 1000 => status=%d tag=%s\n", pSlot->iStatus, pSlot->sTag);
	}

	printf("exists -5  => %d\n", xrtListExists(pList, -5));
	printf("exists 999 => %d\n", xrtListExists(pList, 999));
	printf("count      => %u\n", xrtListCount(pList));
	iRet = 0;

cleanup:
	if ( pList != NULL ) {
		xrtListDestroy(pList);
	}
	xrtUnit();
	return iRet;
}
```

这个最小例子最想让你先记住的是：

1. `list` 适合整数 key，不要求连续
2. `Set()` 返回的是值槽位指针
3. 稀疏索引不会导致中间空洞被整体分配出来
4. 负 key、零和大跨度 key 都是正常用法


## 4. 升级 DEMO：用指针值模式做对象表，并正确替换旧值

当你已经会用结构体值模式以后，下一步最值得学的是：

- 有时 value 不是结构体本体
- 而是一个对象指针

这时最典型的场景就是：

- 会话表
- 连接表
- 任务句柄表

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
	int iID;
	str sName;
} DemoUser;


static DemoUser* procCreateUser(int iID, str sName)
{
	DemoUser* pUser = (DemoUser*)xrtMalloc(sizeof(DemoUser));
	if ( pUser == NULL ) {
		return NULL;
	}

	pUser->iID = iID;
	pUser->sName = xrtCopyStr(sName, 0);
	if ( pUser->sName == NULL ) {
		xrtFree(pUser);
		return NULL;
	}

	return pUser;
}


static void procFreeUser(DemoUser* pUser)
{
	if ( pUser == NULL ) {
		return;
	}

	if ( pUser->sName != NULL ) {
		xrtFree(pUser->sName);
	}
	xrtFree(pUser);
}


int main(void)
{
	xlist pList = NULL;
	DemoUser* pUser = NULL;
	ptr pOldVal = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pList = xrtListCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
	if ( pList == NULL ) {
		goto cleanup;
	}

	pUser = procCreateUser(101, "Alice");
	if ( (pUser == NULL) || !xrtListSetPtr(pList, 101, pUser, &pOldVal) ) {
		if ( pUser != NULL ) {
			procFreeUser(pUser);
			pUser = NULL;
		}
		goto cleanup;
	}
	pUser = NULL;

	pUser = procCreateUser(101, "Alice-v2");
	if ( (pUser == NULL) || !xrtListSetPtr(pList, 101, pUser, &pOldVal) ) {
		if ( pUser != NULL ) {
			procFreeUser(pUser);
			pUser = NULL;
		}
		goto cleanup;
	}
	if ( pOldVal != NULL ) {
		procFreeUser((DemoUser*)pOldVal);
		pOldVal = NULL;
	}
	pUser = NULL;

	LIST_FOREACH_TYPE(pList, iKey, ppUser, DemoUser**)
	{
		printf("key=%lld name=%s\n", (long long)iKey, (*ppUser)->sName);
		procFreeUser(*ppUser);
		*ppUser = NULL;
	}

	iRet = 0;

cleanup:
	if ( pOldVal != NULL ) {
		procFreeUser((DemoUser*)pOldVal);
	}
	if ( pUser != NULL ) {
		procFreeUser(pUser);
	}
	if ( pList != NULL ) {
		xrtListDestroy(pList);
	}
	xrtUnit();
	return iRet;
}
```

这个升级例子真正要教会你的事有 6 个：

1. 指针值模式应该用 `sizeof(ptr)` 创建
2. `SetPtr()` 可以把旧值通过 `ppOldVal` 交还给你
3. 旧对象的释放责任在你自己
4. `GetPtr()` 返回的是存储值，不是槽位指针
5. `LIST_FOREACH_TYPE(..., DemoUser**)` 里拿到的是值槽位地址
6. 销毁 `list` 前，先把每个对象释放干净


## 5. 遍历和 shared 模式该怎么落地

如果只是本线程私有列表，最稳的路线就是：

- `LOCAL`
- 直接 `Set / Get / Walk`

如果进入 shared root，再遇到：

- 遍历
- 依赖稳定视图的多步读写
- 迭代器

就应该先把锁边界说清楚。

最小模式大概像这样：

```c
#include "xrt.h"

void procDumpSharedList(xlist pList)
{
	if ( !xrtListLock(pList) ) {
		return;
	}

	LIST_FOREACH_TYPE(pList, iKey, pVal, ptr)
	{
		(void)iKey;
		(void)pVal;
	}

	xrtListUnlock(pList);
}
```

这段示例想表达的是：

- shared 不是“无锁下一切都天然稳定”
- shared 是“进入正式共享合同以后，你可以自己定义稳定访问边界”


## 6. 它和 `array`、`dict` 的区别

### 6.1 和 `array` 的区别

- `array`
	- 适合连续成员编号
	- 第 1 个、第 2 个、第 3 个
- `list`
	- 适合任意整数 key
	- `-5`、`0`、`1000`


### 6.2 和 `dict` 的区别

- `dict`
	- key 是任意字节序列
	- 更适合字符串 key、二进制 key
- `list`
	- key 固定是 `int64`
	- 更适合整数 ID、位置索引、稀疏编号


## 7. 如果只有一条主线程，会卡在哪里

`list` 本身不会像 I/O 那样等待，但它会在主线程里消耗：

- AVL 插入 / 查找 / 删除
- 大量节点分配
- 全表 walk

所以最常见的问题不是“线程阻塞”，而是：

- 本来该用数组却误用了稀疏树结构
- 值对象释放责任没说清
- 遍历回调返回语义写反


## 8. Windows / Linux 跨平台时最该记住的事

- `list` 的 key 是 `int64`，不要混成平台相关的 `long`
- 指针值模式里，`sizeof(ptr)` 会随平台位数变化，创建时直接写 `sizeof(ptr)` 最稳
- 指针值不能当成可持久化、可跨进程数据
- shared 模式下遍历和复合操作的稳定性，要靠你自己定义锁边界


## 9. 常见错误

### 错误一：把 `list` 当成“支持洞位的数组”

它的核心仍然是树映射，不是连续数组。

### 错误二：指针值模式不按 `sizeof(ptr)` 创建

当前主线里，指针值模式应明确给：

- `sizeof(ptr)`

### 错误三：把 `xrtListWalk()` 回调返回值写反

当前实现里：

- `TRUE`
	- 中断遍历
- `FALSE`
	- 继续遍历

### 错误四：以为 `list` 会替你释放值指向的对象

不会。  
对象释放责任仍然在你自己。

### 错误五：明明 key 连续又很密，却还用 `list`

这种场景通常数组更直接、更省。

### 错误六：shared 模式里长时间遍历却不显式锁

如果你依赖稳定视图，就应该先锁。


## 10. 建议继续阅读

- [List API](../api/api-list.md)
- [AVLTree 入门：什么时候该直接用通用有序树，而不是 Dict、List 或数组](avltree-intro.md)
- [Dict 入门：什么时候该用字典存键值，而不是数组、列表或手搓二叉树](dict-intro.md)
- [Array 入门：什么时候该用连续结构体数组，而不是 buffer 和指针数组](array-intro.md)
- [AVLTree API](../api/api-avltree.md)
- [用 xvalue + json 构造配置系统](../case/json-config-system.md)

---

**一句话总结：** `list` 最适合承担“把任意 `int64` key 稳定映射到一个值槽位或指针值”的角色；先把稀疏索引、指针值模式、遍历语义和 shared 边界看清楚，它会比把整数 key 硬塞进数组稳得多。
