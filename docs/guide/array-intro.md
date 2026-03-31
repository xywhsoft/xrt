# Array 入门：什么时候该用连续结构体数组，而不是 buffer 和指针数组

> 目标：把 `xrtArrayCreate()`、`xrtArrayAppend()`、`xrtArrayInsert()`、`xrtArrayRemove()`、`xrtArraySort()`、`xrtArrayGet()` 和 `xrtArrayLock()` / `xrtArrayUnlock()` 讲成第 2 阶段第二条正式容器主线。读完这页后，你应该明确知道：什么时候你的问题已经不是“字节拼装”，而是“固定大小元素管理”；以及当前 `Array` 主线里 `1-based` 访问、`0-based` 插入位点、`LOCAL / SHARED` 模式分别意味着什么。

[返回教学文档](README.md)

---

## 1. 为什么 `array` 应该接在 `buffer` 后面讲

学完 `buffer` 以后，最自然的下一步通常就是：

- 我不再只是拼字节了
- 我已经有一批固定结构体元素
- 我想按“第几个成员”来追加、插入、删除、排序、遍历

这时问题的本质已经变了：

- `buffer`
	- 你关心的是字节区
- `array`
	- 你关心的是固定大小元素

所以 `array` 这一页真正要解决的是：

- 怎样把一批同构结构体放进连续内存
- 怎样高效追加和遍历
- 怎样在需要时插入、删除、排序
- 怎样在跨线程时把“本线程快路径”和 shared root 分清楚


## 2. 先把 7 个边界分清楚

### 2.1 `array` 解决的是“固定大小元素管理”，不是“字节拼装”

`array` 很适合：

- 一批 `struct` 记录
- 连续内存遍历
- 排序
- 按索引访问
- 批量追加

`array` 不适合：

- 变长字符串拼装
- 原始二进制帧构造
- 一批大对象指针引用共享

所以更直接地说：

- 你在想“成员 1、成员 2、成员 3”
	- 先想 `array`
- 你在想“这一段字节怎么继续往后拼”
	- 先想 `buffer`


### 2.2 `ItemLength` 决定了数组装的是什么

创建数组时你给的是：

- `sizeof(你的结构体)`

这意味着 `array` 内部存放的是：

- 元素本体

不是：

- 指针数组

这带来两个直接特点：

- 小结构体批量遍历会很顺
- 插入、删除、排序时会移动整块结构体内存

如果你真正想存的是对象指针，而不是结构体本体，那更可能应该去看：

- `ptrarray`


### 2.3 `Get` / `Append` 返回和访问是 **1-based**

`array` 当前主线里，一个最容易踩坑的点就是：

- 成员编号从 `1` 开始

也就是说：

- 第一个元素是 `1`
- `0` 不是有效成员编号

所以这些接口都按 1-based 理解：

- `xrtArrayAppend()` 返回的索引
- `xrtArrayGet()` 的 `iPos`
- `xrtArrayRemove()` 的 `iPos`
- `xrtArraySwap()` 的位置参数


### 2.4 `xrtArrayInsert()` 的 `iPos` 是“插入位点”，不是返回索引语义

这又是第二个最容易混的点。

当前实现里：

- `xrtArrayInsert(pArr, iPos, iCount)`
	- 这里的 `iPos` 更像“零基插入位点”
	- `0` 表示头部前面
	- `Count` 表示尾部追加

而函数返回值是：

- 新元素的 **1-based 首索引**

所以要这样理解：

- `xrtArrayInsert(arr, 0, 1)`
	- 新元素会成为第 1 个
- `xrtArrayInsert(arr, 2, 1)`
	- 会插在“前两个元素之后”

别把“插入位点”和“成员编号”混成一套坐标。


### 2.5 `LOCAL` 和 `SHARED` 是两种完全不同的用法

创建数组时你要给：

- `XRT_OBJMODE_LOCAL`
- `XRT_OBJMODE_SHARED`

最稳的理解是：

- `LOCAL`
	- 默认、本线程快路径
	- 最适合单线程或明确只在拥有线程中使用
- `SHARED`
	- 允许跨线程进入正式 shared root 主线
	- 复合操作、稳定遍历、跨线程写入时要显式锁

所以不要默认一股脑全开成 shared。  
如果它就是本线程私有数组，先用 `LOCAL`。


### 2.6 元素指针不是“永远稳定”的

这是容器页里必须反复强调的边界。

这些操作都可能让内部连续内存发生变化：

- `xrtArrayAlloc()`
- `xrtArrayInsert()`
- `xrtArrayAppend()`
- `xrtArrayRemove()`
- `xrtArraySort()`

所以你从 `xrtArrayGet()` 拿到的元素指针：

- 适合立刻读写
- 不适合长期缓存然后假设永远不变

一旦数组扩容、插入、删除、排序，那些旧指针都可能失效或指向别的元素。


### 2.7 `Get_Unsafe` / `Get_Inline` 只适合你已经完全确认边界时

`xrtArrayGet()` 的价值是：

- 帮你做合法性检查

而：

- `xrtArrayGet_Unsafe()`
- `xrtArrayGet_Inline()`

都更偏：

- 你已经保证索引有效
- 你在热路径里想减少检查开销

如果边界还没站稳，先不要急着走 unsafe 路线。


## 3. 最小 DEMO：先用 `LOCAL` 模式做一个结构体清单

第一步不要先上 shared，也不要先上排序。  
先把最常见、也最稳定的主线走通：

- 创建一个本线程数组
- 预分配一点空间
- 追加若干结构体
- 按 1-based 遍历输出

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
	int iID;
	char sName[32];
	int iPriority;
} DemoTask;


int main(void)
{
	xrtGlobalData* pCore = NULL;
	xarray pArr = NULL;
	uint32 u32Pos = 0;
	uint32 i = 0;
	int iRet = 1;

	(void)pCore;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	pArr = xrtArrayCreate(sizeof(DemoTask), XRT_OBJMODE_LOCAL);
	if ( pArr == NULL ) {
		goto cleanup;
	}

	if ( !xrtArrayAlloc(pArr, 8) ) {
		fprintf(stderr, "prealloc task array failed\n");
		goto cleanup;
	}

	u32Pos = xrtArrayAppend(pArr, 1);
	((DemoTask*)xrtArrayGet(pArr, u32Pos))->iID = 101;
	strcpy(((DemoTask*)xrtArrayGet(pArr, u32Pos))->sName, "load-config");
	((DemoTask*)xrtArrayGet(pArr, u32Pos))->iPriority = 1;

	u32Pos = xrtArrayAppend(pArr, 1);
	((DemoTask*)xrtArrayGet(pArr, u32Pos))->iID = 102;
	strcpy(((DemoTask*)xrtArrayGet(pArr, u32Pos))->sName, "warm-cache");
	((DemoTask*)xrtArrayGet(pArr, u32Pos))->iPriority = 2;

	u32Pos = xrtArrayAppend(pArr, 1);
	((DemoTask*)xrtArrayGet(pArr, u32Pos))->iID = 103;
	strcpy(((DemoTask*)xrtArrayGet(pArr, u32Pos))->sName, "start-http");
	((DemoTask*)xrtArrayGet(pArr, u32Pos))->iPriority = 3;

	printf("count       : %u\n", pArr->Count);
	printf("alloc count : %u\n", pArr->AllocCount);

	for ( i = 1; i <= pArr->Count; i++ ) {
		DemoTask* pTask = (DemoTask*)xrtArrayGet(pArr, i);
		printf("[%u] id=%d name=%s priority=%d\n", i, pTask->iID, pTask->sName, pTask->iPriority);
	}

	iRet = 0;

cleanup:
	if ( pArr != NULL ) {
		xrtArrayDestroy(pArr);
	}
	xrtUnit();
	return iRet;
}
```

这个最小例子最想让你先记住的是：

1. `LOCAL` 模式就是默认主线
2. `Append` 返回的是 1-based 新成员编号
3. `xrtArrayAlloc()` 适合在已知大概数量时先预分配
4. `array` 的遍历天然就是“第 1 个到第 N 个”


## 4. 升级 DEMO：把插入、删除和排序接成一条管理主线

当你已经会追加结构体以后，下一步真正要学的是：

- 中间插入
- 删除成员
- 按规则排序

这时 `array` 才真正和“只是 append 几个结构体”拉开层级。

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
	int iScore;
	char sName[32];
} DemoStudent;


static int procCompareStudentScore(const void* pA, const void* pB)
{
	const DemoStudent* pLeft = (const DemoStudent*)pA;
	const DemoStudent* pRight = (const DemoStudent*)pB;
	return pRight->iScore - pLeft->iScore;
}


static void procPrintStudents(xarray pArr, const char* sTitle)
{
	uint32 i = 0;

	printf("%s\n", sTitle);
	for ( i = 1; i <= pArr->Count; i++ ) {
		DemoStudent* pStu = (DemoStudent*)xrtArrayGet(pArr, i);
		printf("  [%u] %s => %d\n", i, pStu->sName, pStu->iScore);
	}
}


int main(void)
{
	xrtGlobalData* pCore = NULL;
	xarray pArr = NULL;
	uint32 u32Pos = 0;
	int iRet = 1;

	(void)pCore;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	pArr = xrtArrayCreate(sizeof(DemoStudent), XRT_OBJMODE_LOCAL);
	if ( pArr == NULL ) {
		goto cleanup;
	}

	u32Pos = xrtArrayAppend(pArr, 1);
	strcpy(((DemoStudent*)xrtArrayGet(pArr, u32Pos))->sName, "Alice");
	((DemoStudent*)xrtArrayGet(pArr, u32Pos))->iScore = 85;

	u32Pos = xrtArrayAppend(pArr, 1);
	strcpy(((DemoStudent*)xrtArrayGet(pArr, u32Pos))->sName, "Charlie");
	((DemoStudent*)xrtArrayGet(pArr, u32Pos))->iScore = 92;

	u32Pos = xrtArrayInsert(pArr, 1, 1);
	strcpy(((DemoStudent*)xrtArrayGet(pArr, u32Pos))->sName, "Bob");
	((DemoStudent*)xrtArrayGet(pArr, u32Pos))->iScore = 88;

	u32Pos = xrtArrayAppend(pArr, 1);
	strcpy(((DemoStudent*)xrtArrayGet(pArr, u32Pos))->sName, "David");
	((DemoStudent*)xrtArrayGet(pArr, u32Pos))->iScore = 79;

	procPrintStudents(pArr, "before sort:");

	if ( !xrtArraySort(pArr, procCompareStudentScore) ) {
		fprintf(stderr, "sort students failed\n");
		goto cleanup;
	}

	procPrintStudents(pArr, "after sort:");

	if ( !xrtArrayRemove(pArr, pArr->Count, 1) ) {
		fprintf(stderr, "remove last student failed\n");
		goto cleanup;
	}

	procPrintStudents(pArr, "after remove last:");
	iRet = 0;

cleanup:
	if ( pArr != NULL ) {
		xrtArrayDestroy(pArr);
	}
	xrtUnit();
	return iRet;
}
```

这个升级例子最关键的教学点有 6 个：

1. `Insert` 传的是插入位点，不是 1-based 成员编号
2. `Remove` 删除的是 1-based 成员编号范围
3. `Sort` 会移动整个数组内存布局
4. 排序前拿到的元素指针，排序后不该继续假设仍然指向同一成员
5. 连续结构体数组特别适合这类批量整理场景
6. 如果你真正需要的是对象引用排序、独立对象生命周期或槽位复用，那下一页通常该看 `ptrarray`


## 5. Shared 模式该怎么理解

当数组需要进入跨线程主线时，才考虑：

- `XRT_OBJMODE_SHARED`

当前主线里更稳的理解是：

- 简单本线程用法
	- 继续用 `LOCAL`
- 共享根对象、跨线程访问、复合操作
	- 用 `SHARED`
	- 并在需要稳定视图时显式锁

最小模式大概像这样：

```c
#include "xrt.h"

void procPushSharedItem(xarray pArr)
{
	uint32 u32Pos = 0;

	if ( !xrtArrayLock(pArr) ) {
		return;
	}

	u32Pos = xrtArrayAppend(pArr, 1);
	if ( u32Pos != 0 ) {
		/* 这里写入第 u32Pos 个元素 */
	}

	xrtArrayUnlock(pArr);
}
```

这段示例真正要表达的是：

- shared 不是“我以后什么都不用管”
- shared 是“我进入了正式共享所有权主线”
- 复合操作和稳定遍历时，锁语义要自己明确


## 6. `Get`、`Get_Unsafe`、`Get_Inline` 怎么选

先记最稳的顺序：

- 新手或边界还没站稳
	- `xrtArrayGet()`
- 已经在热路径里，并且你确信索引永远合法
	- 再考虑 `xrtArrayGet_Unsafe()` 或 `xrtArrayGet_Inline()`

一个非常实用的经验是：

- 正在写教学页、业务页、控制逻辑
	- 先用安全版
- 真到性能热点再优化
	- 才切 unsafe / inline


## 7. 它和 `buffer`、`ptrarray` 的区别

### 7.1 和 `buffer` 的区别

- `buffer`
	- 一整块字节区
	- 你自己定义字节含义
- `array`
	- 一整批固定大小元素
	- 你直接按成员编号操作


### 7.2 和 `ptrarray` 的区别

- `array`
	- 存结构体本体
	- 缓存友好
	- 排序/插入时要搬整个结构体
- `ptrarray`
	- 存指针
	- 更适合大对象、共享对象、对象生命周期独立管理
	- 排序时搬的是指针，不是对象本体


## 8. 如果只有一条主线程，会卡在哪里

`array` 这条线通常不会像网络 I/O 那样“卡在等待上”，但它会在主线程里消耗：

- 扩容
- `memmove`
- `qsort`
- 遍历

所以最常见的问题不是“阻塞等待”，而是：

- 中间频繁插入导致不断搬移内存
- 大数组反复排序
- 该预分配的时候没预分配
- 不该缓存元素指针的时候缓存了


## 9. Windows / Linux 跨平台时最该记住的事

- `array` 的行为主要取决于元素大小和内存布局，不依赖平台容器实现
- 如果结构体里有平台相关字段，`sizeof(struct)` 可能不同
- 想跨平台稳定序列化时，不要直接把内存布局当协议格式
- shared 模式下的锁和 owner 语义要按当前主线理解，不要自行假设“跨线程随便改都安全”


## 10. 常见错误

### 错误一：把 `0` 当成第一个成员编号

当前 `Get / Remove / Swap` 这类访问语义是 1-based。  
`0` 不是有效成员编号。

### 错误二：把 `Insert` 的 `iPos` 当成 1-based 成员编号

它更像零基插入位点。  
返回值才是 1-based 的新成员首索引。

### 错误三：缓存元素指针后还继续插入、删除、排序

一旦内部连续内存被移动，那些旧指针就不可靠了。

### 错误四：本线程私有数组也一上来就开 shared

shared 不是默认答案。  
如果它只在一个线程里使用，先走 local。

### 错误五：在 shared 模式里做复合操作却不显式锁

尤其是“先取 count，再取元素，再追加/删除”的组合路径，应该先把并发边界说清楚。

### 错误六：结构体里自己带动态内存，但销毁数组时忘了先释放元素内部资源

`array` 只负责这块连续数组本身，不会替你释放每个元素里的独立堆对象。


## 11. 建议继续阅读

- [Array API](../api/api-array.md)
- [Buffer 入门：什么时候你需要一块会长大的字节区，而不是数组和字符串](buffer-intro.md)
- [PtrArray 入门：什么时候该管理对象指针，而不是搬运整个结构体](ptrarray-intro.md)
- [PtrArray API](../api/api-ptrarray.md)
- [Math 与 Hash 入门：随机数、约等于和稳定指纹不是一回事](math-hash-intro.md)
- [Template 入门：什么时候该用模板，而不是拼字符串](template-intro.md)

---

**一句话总结：** `array` 最适合承担“把一批固定大小结构体放进连续内存，并按成员编号去追加、插入、删除、排序”的角色；先把 1-based 访问、插入位点和 shared 边界看清楚，这个模块就会非常顺手。 
