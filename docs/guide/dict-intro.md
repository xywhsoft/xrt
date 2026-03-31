# Dict 入门：什么时候该用字典存键值，而不是数组、列表或手搓二叉树

> 目标：把 `xrtDictCreate()`、`xrtDictSet()`、`xrtDictSetPtr()`、`xrtDictGet()`、`xrtDictGetPtr()`、`xrtDictRemove()`、`xrtDictWalk()`、`xrtDictLock()` / `xrtDictUnlock()` 讲成第 2 阶段第六条正式容器主线。读完这页后，你应该明确知道：什么时候问题已经从“顺序容器”变成“按键查值”，为什么这时应该先用 `dict`，以及键复制、值槽位、指针值模式和 shared 边界到底是什么。

[返回教学文档](README.md)

---

## 1. 为什么 `dict` 应该接在 `dynstack` 后面讲

学完前面的容器以后，你已经见过：

- `buffer`
	- 管字节拼装
- `array / ptrarray`
	- 管顺序成员
- `stack / dynstack`
	- 管 LIFO 工作流

但真实程序里很快又会遇到另一类问题：

- 我想按名字取配置项
- 我想按二进制 key 查缓存
- 我想把“键”和“值”稳定绑在一起
- 我不想每次都自己写比较器、树节点和释放逻辑

这时问题的本质已经不是：

- 第几个成员
- 最近压进去的是谁

而是：

- 某个 key 对应的 value 是什么

这就是 `dict` 的位置。


## 2. 先把 9 个边界分清楚

### 2.1 `dict` 是按键查值的有序字典，不是插入顺序容器

`dict` 当前实现基于：

- AVLTree

这意味着它最擅长的是：

- 插入
- 查找
- 删除
- 有序遍历

它不承诺：

- 保留插入顺序
- 像数组一样按位置编号访问

所以更直接地说：

- 你在想“key 对应的值是什么”
	- 先想 `dict`
- 你在想“第 N 个元素”
	- 更可能该去看 `array / ptrarray / list`


### 2.2 key 会被字典内部复制，所以临时缓冲区是安全的

这是 `dict` 最重要、也最容易让人放心的一条边界。

当前实现里，插入新 key 时会：

- 复制 `Key`
- 保存 `KeyLen`
- 保存 `Hash`

所以这类写法是安全的：

- 用栈上 `char sKey[64]`
- `sprintf()` 完以后立刻 `xrtDictSet()`

因为字典会把 key 自己拷进去，不会长期引用你那块临时缓冲区。

但要注意：

- `iKeyLen`
	- 是有效 key 的字节数
	- 通常**不包含**字符串终止符 `\0`


### 2.3 `xrtDictSet()` 返回的是“值槽位指针”，不是值副本

`xrtDictSet()` 的核心心智模型不是：

- “把值传进去”

而是：

- “给我这个 key 对应的值槽位”

也就是说：

- key 不存在
	- 创建新节点
	- 返回新值槽位
	- `*bNewRet = TRUE`
- key 已存在
	- 返回现有值槽位
	- `*bNewRet = FALSE`

这组接口特别适合：

- 计数器
- 缓存命中表
- 结构体值表
- get-or-create 模式


### 2.4 `SetPtr / GetPtr / RemovePtr` 不是 `iItemLength = 0`，而是 `sizeof(ptr)`

这点必须单独强调，因为旧文档里这块说法已经过期了。

当前实现里，如果你要把值当成：

- 一个指针槽

那更稳的创建方式是：

- `xrtDictCreate(sizeof(ptr), XRT_OBJMODE_LOCAL)`

而不是：

- `xrtDictCreate(0, ...)`

因为 `SetPtr / GetPtr / RemovePtr` 这条线，本质上就是把值区当成一个：

- `ptr`

来读写。

所以最稳的经验是：

- 结构体值
	- `sizeof(你的结构体)`
- 指针值
	- `sizeof(ptr)`


### 2.5 `dict` 会自动管 key 内存，但不会替你释放“值指向的对象”

`xrtDictDestroy()` 会帮你处理的是：

- 所有字典节点
- 所有 key 的内部副本
- AVLTree 和内部节点内存

它不会帮你处理的是：

- 值本身指向的堆对象

所以如果你用的是指针值模式：

- 在销毁字典前
	- 先把每个值指向的对象释放掉

不要把 `dict` 想成：

- 带析构器的智能 map


### 2.6 `LOCAL / SHARED` 和 `Lock / Unlock` 也要按当前主线理解

当前 `dict` 已经进入 owner/shared 双模式主线：

- `xrtDictCreate(..., XRT_OBJMODE_LOCAL)`
	- 本线程默认快路径
- `xrtDictCreate(..., XRT_OBJMODE_SHARED)`
	- 进入 shared root 主线

最稳的理解是：

- 本线程私有字典
	- 用 `LOCAL`
- 跨线程共享根对象
	- 用 `SHARED`
	- 复合操作、稳定遍历、依赖视图一致性的读路径再显式锁

比如：

- 先判断存在
- 再读
- 再写
- 或者长时间遍历

这种路径都更适合先：

- `xrtDictLock()`
- 完成操作
- `xrtDictUnlock()`


### 2.7 `xrtDictWalk()` 的回调返回值语义是“TRUE 中断，FALSE 继续”

这也是旧文档里最容易讲反的一点。

当前实现里，`xrtDictWalk()` 的回调如果返回：

- `TRUE`
	- 会中断遍历
- `FALSE`
	- 会继续遍历

所以不要把它写反。

如果你只是想“把所有项都扫完”，回调里通常应该：

- `return FALSE;`


### 2.8 `DICT_FOREACH` / `DICT_FOREACH_TYPE` 是更适合写教学和业务代码的遍历入口

`xrtDictWalk()` 适合：

- 递归回调式遍历

但在业务代码里，你经常会更想直接写：

```c
DICT_FOREACH_TYPE(pDict, pKey, pVal, DemoValue*)
{
	/* 直接使用 pKey / pVal */
}
```

这比把逻辑拆到单独回调函数里更直观。

不过要注意：

- 普通值模式里，`pVal`
	- 是值槽位指针
- 指针值模式里，`pVal`
	- 通常应该声明成 `ptr*`、`str*` 这类“指向槽位的指针”
	- 再通过 `*pVal` 取出真正的值


### 2.9 `SetWithKey / GetWithKey` 是高级优化路径，不是第一天就该走的主线

`xrtDictSetWithKey()` / `xrtDictGetWithKey()` 的价值是：

- 避免重复计算同一个 key 的 hash

但前提是：

- 你要自己把 `Dict_Key.Hash` 填对

对于大多数正常业务代码，更稳的路线仍然是：

- 先用 `xrtDictSet() / xrtDictGet()`

只有到了热路径、同一 key 被反复查询时，再考虑预计算 key。


## 3. 最小 DEMO：先用结构体值字典做一张运行状态表

第一步不要先上指针值模式。  
先把最常见、也最稳定的主线走通：

- 创建一个 local 字典
- 用字符串 key 管理结构体值
- 用 `bNew` 区分“第一次创建”和“后续更新”

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
	int iCount;
	int iLastCode;
} DemoMetric;


static void procHitMetric(xdict pDict, const char* sKey, int iCode)
{
	bool bNew = FALSE;
	DemoMetric* pMetric = (DemoMetric*)xrtDictSet(pDict, (ptr)sKey, (uint32)strlen(sKey), &bNew);
	if ( pMetric == NULL ) {
		return;
	}

	if ( bNew ) {
		memset(pMetric, 0, sizeof(*pMetric));
	}

	pMetric->iCount++;
	pMetric->iLastCode = iCode;
}


int main(void)
{
	xdict pDict = NULL;
	DemoMetric* pMetric = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pDict = xrtDictCreate(sizeof(DemoMetric), XRT_OBJMODE_LOCAL);
	if ( pDict == NULL ) {
		goto cleanup;
	}

	procHitMetric(pDict, "http_200", 200);
	procHitMetric(pDict, "http_200", 200);
	procHitMetric(pDict, "http_404", 404);

	pMetric = (DemoMetric*)xrtDictGet(pDict, "http_200", 8);
	if ( pMetric != NULL ) {
		printf("http_200 => count=%d last=%d\n", pMetric->iCount, pMetric->iLastCode);
	}

	pMetric = (DemoMetric*)xrtDictGet(pDict, "http_404", 8);
	if ( pMetric != NULL ) {
		printf("http_404 => count=%d last=%d\n", pMetric->iCount, pMetric->iLastCode);
	}

	printf("dict count => %u\n", xrtDictCount(pDict));
	iRet = 0;

cleanup:
	if ( pDict != NULL ) {
		xrtDictDestroy(pDict);
	}
	xrtUnit();
	return iRet;
}
```

这个最小例子最想让你先记住的是：

1. `Set()` 返回的是值槽位指针
2. `bNew` 很适合做 get-or-create 初始化
3. key 会被字典内部复制，所以字符串字面量和临时缓冲都能安全用
4. 结构体值模式最适合做状态表和计数表


## 4. 升级 DEMO：用指针值模式做配置表，并正确释放旧值

当你已经会用结构体值模式以后，下一步最值得学的是：

- 字典里不一定存结构体本体
- 有时你只是想存一个指针值

这时最典型的场景就是：

- 配置表
- 字符串表
- 对象引用缓存

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
	xdict pDict = NULL;
	str sValue = NULL;
	ptr pOldVal = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pDict = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
	if ( pDict == NULL ) {
		goto cleanup;
	}

	sValue = xrtCopyStr("localhost:3306", 0);
	if ( (sValue == NULL) || !xrtDictSetPtr(pDict, "database", 8, sValue, &pOldVal) ) {
		if ( sValue != NULL ) {
			xrtFree(sValue);
			sValue = NULL;
		}
		goto cleanup;
	}
	sValue = NULL;

	sValue = xrtCopyStr("db.internal:3306", 0);
	if ( (sValue == NULL) || !xrtDictSetPtr(pDict, "database", 8, sValue, &pOldVal) ) {
		if ( sValue != NULL ) {
			xrtFree(sValue);
			sValue = NULL;
		}
		goto cleanup;
	}
	if ( pOldVal != NULL ) {
		xrtFree(pOldVal);
		pOldVal = NULL;
	}
	sValue = NULL;

	printf("database => %s\n", (char*)xrtDictGetPtr(pDict, "database", 8));

	DICT_FOREACH_TYPE(pDict, pKey, ppVal, str*)
	{
		printf("%.*s => %s\n", pKey->KeyLen, (char*)pKey->Key, *ppVal);
		xrtFree(*ppVal);
		*ppVal = NULL;
	}

	iRet = 0;

cleanup:
	if ( pOldVal != NULL ) {
		xrtFree(pOldVal);
	}
	if ( sValue != NULL ) {
		xrtFree(sValue);
	}
	if ( pDict != NULL ) {
		xrtDictDestroy(pDict);
	}
	xrtUnit();
	return iRet;
}
```

这个升级例子真正要教会你的事有 6 个：

1. 指针值模式应该按 `sizeof(ptr)` 创建
2. `SetPtr()` 可以把旧值通过 `ppOldVal` 传回来
3. 旧值的释放责任在你自己，不在字典
4. `GetPtr()` 返回的是存储的指针值，不是槽位地址
5. `DICT_FOREACH_TYPE(..., str*)` 里拿到的是“值槽位指针”，所以真正字符串要写 `*ppVal`
6. 销毁字典前，先把每个指针值释放干净


## 5. 二进制 key 和 `SetWithKey()` 应该怎么理解

`dict` 的一大优势是：

- key 不必是字符串

你完全可以用：

- 整数
- 固定长度二进制块
- 自己序列化后的结构

例如：

```c
typedef struct {
	uint32 u32Type;
	uint32 u32ID;
} DemoKey;
```

然后用：

- `xrtDictSet(pDict, &tKey, sizeof(tKey), &bNew);`

但要注意两个边界：

1. 如果结构体里有未定义填充字节，直接拿整个内存当 key 可能不稳
2. `SetWithKey()` 并不会替你自动计算 `Hash`

所以最稳的经验是：

- 普通场景
	- 直接 `Set / Get`
- 热路径、重复 key
	- 再考虑 `SetWithKey / GetWithKey`
- 结构体 key
	- 先保证字节布局稳定，或先序列化成字符串/二进制块


## 6. Shared 模式和遍历该怎么落地

当 `dict` 进入跨线程主线时，更稳的理解是：

- 单步 `Get / Set / Remove`
	- 仍然遵守 owner/shared 合同
- 但遍历和复合操作
	- 最好先锁

最小模式大概像这样：

```c
#include "xrt.h"

void procDumpSharedDict(xdict pDict)
{
	if ( !xrtDictLock(pDict) ) {
		return;
	}

	DICT_FOREACH_TYPE(pDict, pKey, pVal, ptr)
	{
		(void)pKey;
		(void)pVal;
	}

	xrtDictUnlock(pDict);
}
```

这段示例真正想表达的是：

- shared 解决的是“能进入正式共享合同”
- 不是说“以后所有多步操作都自动稳定”


## 7. 它和 `list`、`array` 的区别

### 7.1 和 `array` 的区别

- `array`
	- 核心心智模型是“第几个成员”
- `dict`
	- 核心心智模型是“某个 key 对应什么值”


### 7.2 和 `list` 的区别

- `list`
	- key 是整数
	- 更像“整数键到值”的映射
- `dict`
	- key 是任意字节序列
	- 更适合字符串 key、二进制 key、对象标识


## 8. 如果只有一条主线程，会卡在哪里

`dict` 本身不会像网络 I/O 那样“卡在等待上”，但它会在主线程里消耗：

- hash 计算
- AVL 插入 / 查找 / 删除
- key 复制
- 大量节点分配
- 全表遍历

所以最常见的问题不是“主线程在等”，而是：

- key 构造方式不稳
- 值释放责任没讲清
- 大表上反复 walk
- 本该缓存 `Dict_Key` 的热路径还在重复算 hash


## 9. Windows / Linux 跨平台时最该记住的事

- 字符串 key 长度通常不包含 `\0`
- 二进制 key 如果直接来自结构体，要警惕对齐和填充差异
- `Hash` 在 32/64 位平台上的实现路径不同，自己预计算 key 时要和当前平台一致
- 指针值模式里，指针本身不能当成可持久化、可跨进程数据


## 10. 常见错误

### 错误一：把 `Set()` 当成“直接传值进去”

它返回的是值槽位指针。  
你要自己把内容写进去。

### 错误二：指针值模式还按旧文档写成 `xrtDictCreate(0, ...)`

当前实现里更稳的做法是：

- `xrtDictCreate(sizeof(ptr), ...)`

### 错误三：以为字典会替你释放值指向的对象

字典只自动管理 key 和节点。  
值对象仍然由你自己释放。

### 错误四：把 `xrtDictWalk()` 的回调返回值写反

当前实现里：

- `TRUE`
	- 中断遍历
- `FALSE`
	- 继续遍历

### 错误五：直接拿带填充字节的结构体当 key

这样在跨编译器、跨平台、跨构建条件时很容易埋坑。

### 错误六：shared 模式里长时间遍历却不显式锁

如果你依赖遍历视图稳定，应该把锁边界说清楚。


## 11. 建议继续阅读

- [Dict API](../api/api-dict.md)
- [List 入门：什么时候该用整数键列表，而不是数组、字典或连续索引](list-intro.md)
- [AVLTree API](../api/api-avltree.md)
- [List API](../api/api-list.md)
- [DynStack 入门：什么时候该用可增长工作栈，而不是固定深度栈](dynstack-intro.md)
- [用 xvalue + json 构造配置系统](../case/json-config-system.md)

---

**一句话总结：** `dict` 最适合承担“把任意 key 稳定映射到一个值槽位或指针值”的角色；先把 key 长度、值生命周期、指针值模式和遍历语义看清楚，它会比手搓树和乱用数组稳得多。
