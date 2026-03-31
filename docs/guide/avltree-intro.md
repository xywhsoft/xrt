# AVLTree 入门：什么时候该直接用通用有序树，而不是 Dict、List 或数组

> 目标：把 `xrtAVLTreeCreate()`、`xrtAVLTreeInsert()`、`xrtAVLTreeSearch()`、`xrtAVLTreeRemove()`、`xrtAVLTreeWalk()`、`xrtAVLTreeWalkEx()`、`xrtAVLTreeIterBegin()` / `Next()` / `End()` 讲成第 2 阶段第八条正式容器主线。读完这页后，你应该明确知道：什么时候问题已经不是“字符串 key 的字典”或“整数 key 的列表”，而是“我要自己决定 key 长什么样、排序怎么定义、节点里顺便挂什么业务字段”。  

[返回教学文档](README.md)

---

## 1. 为什么 `avltree` 应该接在 `list` 后面讲

学完 `dict` 和 `list` 以后，很多人会自然产生一个新需求：

- key 不只是字符串
- 也不只是整数
- 我想自己决定比较逻辑
- 我希望节点里直接挂完整业务对象
- 还想保留“有序查找”和“中序遍历天然有序”

这时问题就开始从“专用映射容器”转向：

- 通用有序查找树

这正是 `avltree` 的位置。

它最适合解决的不是：

- 连续索引
- 纯字符串字典
- 纯整数键映射

而是：

- 复合 key
- 自定义排序
- 节点对象和 key 天然是一体的场景


## 2. 先把 10 个边界分清楚

### 2.1 `avltree` 是“你自己定义比较逻辑”的通用有序树

`dict` 和 `list` 都已经替你固定好了 key 模型：

- `dict`
	- 任意字节序列 key
- `list`
	- `int64` key

而 `avltree` 的核心则是：

- 你给一个比较函数
- 树按你的比较函数维护顺序

所以你能表达：

- 单整数 key
- 字符串 key
- 复合结构体 key
- “先按 category，再按 id” 这种多级排序


### 2.2 key 通常就活在节点数据里，不再由容器额外复制

这是它和 `dict / list` 最大的心智差异。

`dict / list` 更像：

- 容器单独持有 key
- 容器再给你一个值槽位

`avltree` 更像：

- 你直接存一个完整节点对象
- 比较函数决定“这个对象按什么规则被找到”

所以很多时候你的节点会长成：

```c
typedef struct {
	int iCategory;
	int iID;
	char sName[32];
} DemoNode;
```

然后比较函数自己决定：

- 先比 `iCategory`
- 再比 `iID`


### 2.3 `xrtAVLTreeInsert()` 返回的是节点数据指针，不是 key 句柄

它和 `dict / list` 的 `Set()` 一样，都带一点“get-or-create”味道，但返回对象不同。

这里返回的是：

- 节点数据区指针

也就是说：

- key 不存在
	- 创建新节点
	- `*bNew = TRUE`
	- 你负责把节点字段填完整
- key 已存在
	- 返回已有节点数据
	- `*bNew = FALSE`

所以最常见模式就是：

- `Insert()`
- 看 `bNew`
- 决定是初始化还是更新


### 2.4 当前主线里 `Create / Init` 都要带 `iMode`

当前正式签名是：

```c
xrtAVLTreeCreate(uint32 iItemLength, AVLTree_CompProc procComp, uint32 iMode)
xrtAVLTreeInit(xavltree objAVLT, uint32 iItemLength, AVLTree_CompProc procComp, uint32 iMode)
```

最常见的选择仍然是：

- `XRT_OBJMODE_LOCAL`
	- 本线程私有快路径
- `XRT_OBJMODE_SHARED`
	- 共享根对象主线


### 2.5 `FreeProc` 只负责释放节点里“额外拥有的资源”

`avltree` 会自动处理：

- 节点对象内存
- 内部平衡结构
- 内置池对象

它不会自动知道你的节点里哪些字段还拥有外部资源。

所以如果节点里有：

- `str`
- 堆对象指针
- 额外句柄

就应该设置：

- `objTree->FreeProc`

注意它负责的是：

- 节点里的附加资源

而不是：

- 节点本体内存


### 2.6 `Parent` 只影响查找回退，不会替你联动插入和删除

`Parent` 是 `avltree` 很有价值、也很容易误解的一个特性。

它的核心语义是：

- 本树 `Search()` 没找到
- 再去父树里找

它不是：

- 多层树自动合并
- 写入自动冒泡到父树
- 删除子树节点时顺便删父树

所以它更适合：

- 默认配置 + 局部覆盖
- 全局路由 + 局部路由
- 基础规则 + 项目级补丁


### 2.7 `Walk / WalkEx` 里的回调返回值语义是“TRUE 中断，FALSE 继续”

当前实现里：

- 回调返回 `TRUE`
	- 立即停止遍历
- 回调返回 `FALSE`
	- 继续遍历

这一点和 `dict / list` 当前主线是同一套语义。

所以如果你只是：

- 打印
- 统计
- 累加

那回调里通常应该：

- `return FALSE;`


### 2.8 `AVLTREE_FOREACH` / `IterNext()` 返回的是节点数据指针，不是 `xavltnode`

这一点也必须提前记住。

公开迭代主线给你的通常是：

- 节点数据区

不是：

- 内部树节点头

所以业务代码里最稳的写法通常是：

```c
AVLTREE_FOREACH_TYPE(pTree, pNode, DemoNode*)
{
	/* 直接使用 pNode->字段 */
}
```

只有你真的在做底层结构调试时，才应该去碰：

- `RootNode`
- `xrtAVLTreeGetNodeBase()`
- `xrtAVLTreeGetNodeData()`


### 2.9 shared 模式下，公开 `Walk / Iter` 已自带遍历期合同；显式锁更适合保护你自己的复合访问

`xrtAVLTreeWalk()`、`WalkEx()`、`IterBegin()` 这套公开接口，本身就会进入 owner/shared 合同。

所以显式：

- `xrtAVLTreeLock()`
- `xrtAVLTreeUnlock()`

更适合用在这些场景：

- 你要稳定读取 `RootNode / Count / Parent`
- 你要做多步组合访问
- 你要自己定义一段复合逻辑的稳定边界

不要把它理解成：

- 每次 `Walk()` 前还要额外手锁一次


### 2.10 `NodeCache / objMM / RootNode` 是实现细节，不是业务数据结构接口

公开结构里能看到这些字段，不代表业务代码就应该依赖它们。

更稳的经验是：

- 业务逻辑依赖公开函数和遍历宏
- 调试和诊断时再看内部字段

尤其不要把：

- `NodeCache` 是否为空
- `objMM` 当前页数

写进业务判断里。


## 3. 最小 DEMO：先用复合 key 做一棵本地路由树

第一步不要先上 `Parent`。  
先把 `avltree` 最核心的价值走通：

- 自定义比较逻辑
- 复合 key
- `Insert()` 的 get-or-create 语义
- `Walk()` 的有序遍历

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
	int iCategory;
	int iID;
	char sName[32];
} DemoRoute;

typedef struct {
	int iCategory;
	int iID;
} DemoRouteKey;


static int procCompareRoute(ptr pNode, ptr pKey)
{
	DemoRoute* pRoute = (DemoRoute*)pNode;
	DemoRouteKey* pCmp = (DemoRouteKey*)pKey;

	if ( pRoute->iCategory < pCmp->iCategory ) {
		return -1;
	}
	if ( pRoute->iCategory > pCmp->iCategory ) {
		return 1;
	}
	if ( pRoute->iID < pCmp->iID ) {
		return -1;
	}
	if ( pRoute->iID > pCmp->iID ) {
		return 1;
	}
	return 0;
}


static bool procPrintRoute(ptr pNode, ptr pArg)
{
	DemoRoute* pRoute = (DemoRoute*)pNode;
	(void)pArg;

	printf("[%d:%d] %s\n", pRoute->iCategory, pRoute->iID, pRoute->sName);
	return FALSE;
}


static bool procSetRoute(xavltree pTree, int iCategory, int iID, const char* sName)
{
	DemoRouteKey tKey;
	DemoRoute* pRoute = NULL;
	bool bNew = FALSE;

	tKey.iCategory = iCategory;
	tKey.iID = iID;

	pRoute = (DemoRoute*)xrtAVLTreeInsert(pTree, &tKey, &bNew);
	if ( pRoute == NULL ) {
		return FALSE;
	}

	if ( bNew ) {
		memset(pRoute, 0, sizeof(*pRoute));
		pRoute->iCategory = iCategory;
		pRoute->iID = iID;
	}

	strcpy(pRoute->sName, sName);
	return TRUE;
}


int main(void)
{
	xavltree pTree = NULL;
	DemoRouteKey tKey;
	DemoRoute* pFound = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pTree = xrtAVLTreeCreate(sizeof(DemoRoute), procCompareRoute, XRT_OBJMODE_LOCAL);
	if ( pTree == NULL ) {
		goto cleanup;
	}

	if ( !procSetRoute(pTree, 1, 10, "metrics") ) {
		goto cleanup;
	}
	if ( !procSetRoute(pTree, 1, 20, "healthz") ) {
		goto cleanup;
	}
	if ( !procSetRoute(pTree, 2, 10, "jobs") ) {
		goto cleanup;
	}

	tKey.iCategory = 1;
	tKey.iID = 20;
	pFound = (DemoRoute*)xrtAVLTreeSearch(pTree, &tKey);
	if ( pFound != NULL ) {
		printf("found => %s\n", pFound->sName);
	}

	printf("walk:\n");
	xrtAVLTreeWalk(pTree, procPrintRoute, NULL);
	printf("count => %u\n", pTree->Count);
	iRet = 0;

cleanup:
	if ( pTree != NULL ) {
		xrtAVLTreeDestroy(pTree);
	}
	xrtUnit();
	return iRet;
}
```

这个最小例子最想让你先记住的是：

1. key 可以是你临时构造出来的比较结构体
2. 节点里真正保存的是完整业务对象
3. `Insert()` 只负责定位或创建节点，不替你补字段
4. `Walk()` 输出顺序取决于你的比较函数


## 4. 升级 DEMO：用 `Parent + FreeProc` 做“默认配置 + 局部覆盖”

这一步才是 `avltree` 最有教学价值的升级点。

因为它会同时把下面几件事串起来：

- 节点里有自有资源
- 用 `FreeProc` 管释放
- 本地树没有命中时，回退到父树

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
	char sKey[32];
	str sValue;
} DemoSetting;


static int procCompareSetting(ptr pNode, ptr pKey)
{
	DemoSetting* pSetting = (DemoSetting*)pNode;
	return strcmp(pSetting->sKey, (const char*)pKey);
}


static void procFreeSetting(ptr pTree, ptr pNode)
{
	DemoSetting* pSetting = (DemoSetting*)pNode;
	(void)pTree;

	if ( pSetting->sValue != NULL ) {
		xrtFree(pSetting->sValue);
		pSetting->sValue = NULL;
	}
}


static bool procSetSetting(xavltree pTree, const char* sKey, const char* sValue)
{
	DemoSetting* pSetting = NULL;
	str sCopy = NULL;
	bool bNew = FALSE;

	sCopy = xrtCopyStr(sValue, 0);
	if ( sCopy == NULL ) {
		return FALSE;
	}

	pSetting = (DemoSetting*)xrtAVLTreeInsert(pTree, (ptr)sKey, &bNew);
	if ( pSetting == NULL ) {
		xrtFree(sCopy);
		return FALSE;
	}

	if ( bNew ) {
		memset(pSetting, 0, sizeof(*pSetting));
		strcpy(pSetting->sKey, sKey);
	} else if ( pSetting->sValue != NULL ) {
		xrtFree(pSetting->sValue);
		pSetting->sValue = NULL;
	}

	pSetting->sValue = sCopy;
	return TRUE;
}


int main(void)
{
	xavltree pBaseTree = NULL;
	xavltree pChildTree = NULL;
	DemoSetting* pSetting = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pBaseTree = xrtAVLTreeCreate(sizeof(DemoSetting), procCompareSetting, XRT_OBJMODE_LOCAL);
	pChildTree = xrtAVLTreeCreate(sizeof(DemoSetting), procCompareSetting, XRT_OBJMODE_LOCAL);
	if ( (pBaseTree == NULL) || (pChildTree == NULL) ) {
		goto cleanup;
	}

	pBaseTree->FreeProc = procFreeSetting;
	pChildTree->FreeProc = procFreeSetting;
	pChildTree->Parent = pBaseTree;

	if ( !procSetSetting(pBaseTree, "host", "127.0.0.1") ) {
		goto cleanup;
	}
	if ( !procSetSetting(pBaseTree, "port", "8080") ) {
		goto cleanup;
	}
	if ( !procSetSetting(pChildTree, "host", "service.internal") ) {
		goto cleanup;
	}

	pSetting = (DemoSetting*)xrtAVLTreeSearch(pChildTree, "host");
	if ( pSetting != NULL ) {
		printf("host => %s\n", pSetting->sValue);
	}

	pSetting = (DemoSetting*)xrtAVLTreeSearch(pChildTree, "port");
	if ( pSetting != NULL ) {
		printf("port => %s (from parent)\n", pSetting->sValue);
	}

	iRet = 0;

cleanup:
	if ( pChildTree != NULL ) {
		xrtAVLTreeDestroy(pChildTree);
	}
	if ( pBaseTree != NULL ) {
		xrtAVLTreeDestroy(pBaseTree);
	}
	xrtUnit();
	return iRet;
}
```

这个升级例子真正要教会你的事有 6 个：

1. key 可以是字符串，但节点里仍然是你自己的完整结构体
2. `FreeProc` 负责释放节点里的 `sValue`
3. `Parent` 只负责查找回退
4. 子树命中优先于父树命中
5. 更新已有节点时，旧资源需要先自己释放
6. `avltree` 特别适合做“默认值 + 覆盖”的层级模型


## 5. `Walk`、`WalkEx` 和迭代器该怎么选

这三套接口解决的是不同问题。

### 5.1 只想“把整棵树扫一遍”

优先：

- `xrtAVLTreeWalk()`

它最适合：

- 统计
- 打印
- 查找第一个满足条件的节点


### 5.2 想在前序 / 中序 / 后序三个阶段挂逻辑

优先：

- `xrtAVLTreeWalkEx()`

它更适合：

- 导出树结构
- 调试树形关系
- 递归算法需要前中后阶段分工的场景


### 5.3 想在业务代码里直接写顺序循环

优先：

- `AVLTREE_FOREACH`
- `AVLTREE_FOREACH_TYPE`

最小写法像这样：

```c
AVLTREE_FOREACH_TYPE(pTree, pNode, DemoRoute*)
{
	printf("%d %d %s\n", pNode->iCategory, pNode->iID, pNode->sName);
}
```

这比拆一个单独回调更直观。


## 6. Shared 模式和显式锁怎么落地

如果你只是正常调用：

- `Insert`
- `Search`
- `Remove`
- `Walk`
- `IterBegin / Next / End`

那它们都会进入当前 owner/shared 合同。

显式锁最适合用在：

- 你要稳定查看 `Count / RootNode / Parent`
- 你要自己组合多步逻辑

最小模式大概像这样：

```c
#include "xrt.h"
#include <stdio.h>

void procInspectSharedTree(xavltree pTree)
{
	if ( !xrtAVLTreeLock(pTree) ) {
		return;
	}

	printf("count=%u root=%p parent=%p\n",
		pTree->Count,
		(void*)pTree->RootNode,
		(void*)pTree->Parent);

	xrtAVLTreeUnlock(pTree);
}
```

这段示例真正想表达的是：

- 显式锁保护的是你自己的复合访问边界
- 不是说 `Walk()` 前必须再手锁一次


## 7. 它和 `dict`、`list`、`avltree-base` 的区别

### 7.1 和 `dict` 的区别

- `dict`
	- 容器复制 key
	- 更适合字符串 / 二进制 key
- `avltree`
	- key 规则由比较函数决定
	- 更适合完整业务节点对象


### 7.2 和 `list` 的区别

- `list`
	- key 固定是 `int64`
	- 更适合整数 ID 映射
- `avltree`
	- key 形状完全自定义
	- 更适合复合 key、多级排序


### 7.3 和 `avltree-base` 的区别

- `avltree-base`
	- 更底层
	- 更适合你自己接管节点内存
- `avltree`
	- 自带节点池、回调释放、Parent 与迭代主线
	- 更适合大多数业务场景


## 8. 如果只有一条主线程，会卡在哪里

`avltree` 不会像网络和文件 I/O 那样等待外部事件，但它会在主线程里消耗：

- 比较函数执行
- 节点分配
- 平衡调整
- 全树 walk

所以最常见的问题不是“阻塞在等”，而是：

- 比较函数写得不稳定
- 旧资源释放没说清
- 本来该用 `dict / list` 的简单场景，却提前上了通用树


## 9. Windows / Linux 跨平台时最该记住的事

- 比较函数不要依赖 `long` 宽度，明确使用 `int32 / int64 / uint32` 这类固定宽度类型
- 如果 key 是结构体，先把比较逻辑写成逐字段比较，不要偷懒做整块减法
- 节点里的指针字段不能当成可持久化或跨进程数据
- `shared` 模式下如果你直接碰公开字段，要先自己定义稳定锁边界


## 10. 常见错误

### 错误一：比较函数直接做大整数减法

这在大值范围下可能溢出。  
更稳的是逐字段比较后返回 `-1 / 0 / 1`。

### 错误二：把 `Insert()` 当成“只会创建新节点”

它也可能返回已存在节点。  
所以 `bNew` 必须一起看。

### 错误三：以为 `Parent` 会自动联动写入和删除

不会。  
它只影响查找回退。

### 错误四：把 `Walk()` 回调返回值写反

当前实现里：

- `TRUE`
	- 中断遍历
- `FALSE`
	- 继续遍历

### 错误五：以为 `FreeProc` 需要释放节点本体

不需要。  
节点本体仍由 `avltree` 自己管理。

### 错误六：业务代码直接依赖 `NodeCache / objMM`

这些是实现细节，不是你该绑定的业务合同。


## 11. 建议继续阅读

- [AVLTree API](../api/api-avltree.md)
- [List 入门：什么时候该用整数键列表，而不是数组、字典或连续索引](list-intro.md)
- [Dict 入门：什么时候该用字典存键值，而不是数组、列表或手搓二叉树](dict-intro.md)
- [AVLTree Base API](../api/api-avltree-base.md)
- [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)

---

**一句话总结：** `avltree` 最适合承担“按你自己定义的比较规则，把完整业务节点组织成一棵有序查找树”的角色；先把比较函数、`Insert()` 的 get-or-create 语义、`Parent` 查找回退和 `FreeProc` 释放边界看清楚，它会比把复杂 key 硬塞进 `dict / list` 更稳。
