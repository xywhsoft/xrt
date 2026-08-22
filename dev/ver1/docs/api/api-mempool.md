# MemPool 通用内存池

> 可变大小内存池，当前主线是 `cutoff + 16 字节分桶 + BigMM 大块兜底`，而不是历史文档里的“预设二叉树 1/2 模式”。

[English](api-mempool.en.md) | [中文](api-mempool.md) | [返回索引](README.md)

---

## 目录

- [概述](#概述)
- [当前主线并发合同](#当前主线并发合同)
- [核心模型](#核心模型)
- [数据结构](#数据结构)
- [内存池操作](#内存池操作)
- [内存分配与释放](#内存分配与释放)
- [GC 回收](#gc-回收)
- [与其他内存方案对比](#与其他内存方案对比)
- [最佳实践](#最佳实践)

---

## 概述

MemPool（Memory Pool）是 XRT 当前主线里的通用变长内存池。

它的定位不是：

- 取代所有 `malloc/free`
- 自动理解业务对象图
- 自动跨线程共享一切路径

它真正解决的是：

- 同一条业务链里有很多不同大小的分配
- 小块希望池化复用
- 大块又不能强塞进同一类固定大小对象池
- 仍然希望统一 `Free()` 和统一 GC 入口

如果你已经明确对象尺寸固定，更合适的通常仍然是：

- [FSMemPool](api-mempool-fs.md)

如果你只是偶尔分配一两块内存，也未必需要上池。


## 当前主线并发合同

当前 `MemPool` 不应再按“默认无条件共享”的旧习惯理解。

当前正式教学和 API 主线应按下面规则理解：

- 默认最稳的起点是 `XRT_OBJMODE_LOCAL`
- owner-thread local 仍然是主要快路径
- 错线程修改不会静默破坏内部结构，而会被拒绝并设置当前线程错误
- `shared` 属于需要明确确认的合同，不应默认把所有路径都当成天然跨线程安全


## 核心模型

### 1. 当前模型是 LUT 分桶，不是历史预设树

当前源码中的 `MemPool` 主线是：

- `iCustom <= 0`
	- 使用默认 fallback cutoff
	- 当前默认值是 `1024`
- `iCustom > 0`
	- 直接把它当成自定义 cutoff
- `1 .. cutoff`
	- 按 `16` 字节步长分桶
- `> cutoff`
	- 走大块兜底路径

也就是：

- `1..16`
- `17..32`
- `33..48`
- ...

一直排到 `cutoff`。

因此当前应把 `MemPool` 理解成：

- `FSB_Lut` 做 size -> bucket 查找
- bucket 内部再复用很多 `MemUnit`
- 大块回退到 `BigMM`

而不是：

- “选择 1 或 2 两种预设二叉树配置”

### 2. 当前默认参数

当前头文件中的核心常量是：

```c
#define XRT_MEMPOOL_STEP_SIZE			16u
#define XRT_MEMPOOL_CUTOFF_DEFAULT		1024u
#define XRT_MEMPOOL_CLASS_COUNT_DEFAULT	(XRT_MEMPOOL_CUTOFF_DEFAULT / XRT_MEMPOOL_STEP_SIZE)
```

所以：

- 默认步长 `16`
- 默认 cutoff `1024`
- 默认 bucket 数 `64`

### 3. 小块与大块的职责边界

#### pooled bucket 路径

当请求大小满足：

- `1 <= iSize <= iFallbackCutoff`

当前实现会：

1. 通过 `FSB_Lut` 找到 bucket
2. 在 bucket 里获取一个可分配 `MemUnit`
3. 最终通过 `MemUnit` 返回用户块

#### BigMM 回退路径

当请求大小满足：

- `iSize > iFallbackCutoff`

当前实现会：

1. 分配 `sizeof(MP_MemHead) + iSize`
2. 在 `BigMM` 中登记一条 `MP_BigInfoLL`
3. 返回跳过 `MP_MemHead` 后的用户指针

大块路径的复用重点是：

- `LL_BigFree` 复用大块元数据槽

不是：

- 一直缓存整块大内存


## 数据结构

### MP_MemHead

大块回退内存头。

**定义：**

```c
typedef struct {
	uint32 Index;	// BigMM 的块索引
	uint32 Flag;	// 符合 MemUnit 标准的 Flag
} MP_MemHead;
```

---

### MP_BigInfoLL

大块信息链表节点。

**定义：**

```c
typedef struct MP_BigInfoLL {
	uint32 Index;				// BigMM 槽位索引
	uint32 Size;				// 申请内存的大小
	ptr Ptr;					// 实际大块地址（含 MP_MemHead）
	struct MP_BigInfoLL* Next;	// 用于 LL_BigFree 的单链表
} MP_BigInfoLL;
```

**说明：**

- `Ptr == NULL` 表示该大块槽位当前空闲
- 释放大块后，槽位节点会挂回 `LL_BigFree`

---

### FSB_Item

单个大小区间的 bucket 管理结构。

**定义：**

```c
typedef struct FSB_Item {
	uint32 MinLength;			// 支持最小的内存长度
	uint32 MaxLength;			// 支持最大的内存长度
	MMU_LLNode* LL_Idle;		// 还有空闲槽位的 MemUnit 链表
	MMU_LLNode* LL_Full;		// 已满的 MemUnit 链表
	MMU_LLNode* LL_Null;		// 全空备用 MemUnit（最多保留 1 个）
	MMU_LLNode* LL_Free;		// 已释放的 MMU_LLNode 链表
	struct FSB_Item* left;		// legacy tree pointer (unused in LUT mode)
	struct FSB_Item* right;		// legacy tree pointer (unused in LUT mode)
} FSB_Item;
```

**说明：**

- 当前主线里 `left / right` 已不是查找路径核心
- 当前核心查找结构是 `FSB_Lut`

---

### xmempool_struct

通用内存池结构。

**定义：**

```c
typedef struct {
	xrtOwnerInfo Owner;			// 所有权信息
	FSB_Item* FSB_Memory;		// bucket 数组
	FSB_Item* FSB_RootNode;		// 兼容旧布局的别名，启用时指向第一个 bucket
	uint32* FSB_Lut;			// size -> bucket lookup table (0..iFallbackCutoff)
	uint32 iBucketStep;			// bucket 步长，当前默认 16
	uint32 iBucketCount;		// bucket 数量
	uint32 iFallbackCutoff;		// pooled bucket 最大处理大小
	xbsmm_struct arrMMU;		// 管理 MMU_LLNode
	xbsmm_struct BigMM;			// 管理 MP_BigInfoLL
	MP_BigInfoLL* LL_BigFree;	// 已释放的大块元数据槽
} xmempool_struct, *xmempool;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `Owner` | owner-thread / shared 合同状态 |
| `FSB_Memory` | 所有 bucket 的连续数组 |
| `FSB_RootNode` | 兼容旧结构布局的别名 |
| `FSB_Lut` | 大小到 bucket 的查找表 |
| `iBucketStep` | bucket 步长，当前默认 `16` |
| `iBucketCount` | 当前 bucket 总数 |
| `iFallbackCutoff` | 小块池化上限 |
| `arrMMU` | 管理所有 `MMU_LLNode` |
| `BigMM` | 管理所有 `MP_BigInfoLL` |
| `LL_BigFree` | 空闲大块元数据槽链表 |


## 内存池操作

### xrtMemPoolCreate

创建通用内存池。

**函数原型：**

```c
XXAPI xmempool xrtMemPoolCreate(int iCustom, uint32 iMode);
```

**参数：**

- `iCustom`
	- `<= 0`：使用默认 cutoff，当前默认值是 `1024`
	- `> 0`：使用自定义 cutoff
- `iMode`
	- 所有权模式，教学与业务代码通常从 `XRT_OBJMODE_LOCAL` 起步

**返回值：**

- 成功：内存池对象指针
- 失败：`NULL`

**示例：**

```c
#include "xrt.h"
#include <stdio.h>


int main(void)
{
	xmempool pDefault = NULL;
	xmempool pCustom = NULL;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pDefault = xrtMemPoolCreate(0, XRT_OBJMODE_LOCAL);
	pCustom = xrtMemPoolCreate(256, XRT_OBJMODE_LOCAL);

	printf("default cutoff = %u\n", pDefault->iFallbackCutoff);
	printf("custom cutoff  = %u\n", pCustom->iFallbackCutoff);

	xrtMemPoolDestroy(pDefault);
	xrtMemPoolDestroy(pCustom);
	xrtUnit();
	return 0;
}
```

---

### xrtMemPoolDestroy

销毁内存池。

**函数原型：**

```c
XXAPI void xrtMemPoolDestroy(xmempool objMP);
```

**说明：**

- 释放所有 bucket 里的 `MemUnit`
- 释放 `FSB_Lut` / `FSB_Memory`
- 释放所有大块回退内存
- 释放内存池对象本身

---

### xrtMemPoolInit

初始化内存池（用于栈上或嵌入式场景）。

**函数原型：**

```c
XXAPI void xrtMemPoolInit(xmempool objMP, int iCustom, uint32 iMode);
```

**参数：**

- `objMP`：外部维护的内存池结构体
- `iCustom`：fallback cutoff 配置
- `iMode`：所有权模式

**示例：**

```c
#include "xrt.h"
#include <stdio.h>


int main(void)
{
	xmempool_struct tPool;
	void* pData = NULL;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	xrtMemPoolInit(&tPool, 128, XRT_OBJMODE_LOCAL);
	pData = xrtMemPoolAlloc(&tPool, 64);
	printf("alloc = %p\n", pData);
	xrtMemPoolFree(&tPool, pData);
	xrtMemPoolUnit(&tPool);

	xrtUnit();
	return 0;
}
```

---

### xrtMemPoolUnit

释放内存池内部资源（不释放结构体本身）。

**函数原型：**

```c
XXAPI void xrtMemPoolUnit(xmempool objMP);
```

**说明：**

- 与 `xrtMemPoolInit()` 配合使用
- 释放内部资源，但不释放外部结构体本身


## 内存分配与释放

### xrtMemPoolAlloc

从内存池分配一块内存。

**函数原型：**

```c
XXAPI ptr xrtMemPoolAlloc(xmempool objMP, uint32 iSize);
```

**参数：**

- `objMP`：内存池对象
- `iSize`：请求大小（字节）

**返回值：**

- 成功：用户数据指针
- 失败：`NULL`

**当前主线分配策略：**

1. `iSize == 0`
	- 直接失败
2. `iSize <= iFallbackCutoff`
	- 通过 `FSB_Lut` 找 bucket
	- bucket 内获取 `MemUnit`
	- 从 `MemUnit` 返回用户块
3. `iSize > iFallbackCutoff`
	- 分配大块
	- 在 `BigMM` 中登记元数据
	- 返回跳过 `MP_MemHead` 后的用户指针

**示例：**

```c
#include "xrt.h"
#include <stdio.h>


int main(void)
{
	xmempool pPool = NULL;
	void* pSmall = NULL;
	void* pMedium = NULL;
	void* pBig = NULL;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pPool = xrtMemPoolCreate(256, XRT_OBJMODE_LOCAL);
	pSmall = xrtMemPoolAlloc(pPool, 32);
	pMedium = xrtMemPoolAlloc(pPool, 240);
	pBig = xrtMemPoolAlloc(pPool, 300);

	printf("32  -> %p\n", pSmall);
	printf("240 -> %p\n", pMedium);
	printf("300 -> %p (fallback)\n", pBig);

	xrtMemPoolFree(pPool, pSmall);
	xrtMemPoolFree(pPool, pMedium);
	xrtMemPoolFree(pPool, pBig);
	xrtMemPoolDestroy(pPool);

	xrtUnit();
	return 0;
}
```

---

### xrtMemPoolFree

释放从内存池申请的内存。

**函数原型：**

```c
XXAPI void xrtMemPoolFree(xmempool objMP, ptr ptr);
```

**参数：**

- `objMP`：内存池对象
- `ptr`：要释放的用户指针

**当前主线释放策略：**

#### pooled bucket 对象

- 通过对象头里的 `ItemFlag`
	- 找回所属 `MemUnit`
	- 找回槽位索引
- 释放回对应单元
- 再根据单元状态调整 `LL_Idle / LL_Full / LL_Null / LL_Free`

#### BigMM 大块对象

- 回退到 `MP_MemHead`
- 通过 `Index` 找到 `MP_BigInfoLL`
- 真正的大块内存会立刻释放
- 但 `MP_BigInfoLL` 槽位会挂回 `LL_BigFree`

**硬边界：**

- 必须把对象交回原来的池
- 不要跨池 `Free()`
- 不要把其他分配器得到的指针交给 `xrtMemPoolFree()`


## GC 回收

### xrtMemPoolGC_Mark

将一块内存标记为使用中。

**宏定义：**

```c
#define xrtMemPoolGC_Mark	xrtMemUnitGC_Mark
```

**函数原型：**

```c
void xrtMemPoolGC_Mark(ptr pVal);
```

**说明：**

- 只是设置对象头上的 `GC` 标记位
- 不会替你自动遍历对象图

---

### xrtMemPoolGC

执行一轮 GC。

**函数原型：**

```c
XXAPI void xrtMemPoolGC(xmempool objMP, bool bFreeMark);
```

**参数：**

- `objMP`：内存池对象
- `bFreeMark`
	- `FALSE`：回收未标记对象，并清掉幸存对象的 `GC` 位
	- `TRUE`：回收已标记对象

**说明：**

- pooled bucket 路径会对所有 bucket 逐个做 sweep
- BigMM 大块路径也会按相同标记语义处理
- 这是显式标记回收，不是自动 root 扫描

**示例：**

```c
#include "xrt.h"
#include <stdio.h>

typedef struct DemoNode DemoNode;
struct DemoNode {
	int iID;
	char* sPayload;
	DemoNode* pNext;
};


static void MarkReachable(DemoNode* pNode)
{
	while ( pNode != NULL ) {
		xrtMemPoolGC_Mark(pNode);
		if ( pNode->sPayload != NULL ) {
			xrtMemPoolGC_Mark(pNode->sPayload);
		}
		pNode = pNode->pNext;
	}
}


int main(void)
{
	xmempool pPool = NULL;
	DemoNode* pA = NULL;
	DemoNode* pB = NULL;
	DemoNode* pC = NULL;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pPool = xrtMemPoolCreate(128, XRT_OBJMODE_LOCAL);

	pA = (DemoNode*)xrtMemPoolAlloc(pPool, sizeof(DemoNode));
	pB = (DemoNode*)xrtMemPoolAlloc(pPool, sizeof(DemoNode));
	pC = (DemoNode*)xrtMemPoolAlloc(pPool, sizeof(DemoNode));

	pA->sPayload = (char*)xrtMemPoolAlloc(pPool, 64);
	pB->sPayload = (char*)xrtMemPoolAlloc(pPool, 192);
	pC->sPayload = (char*)xrtMemPoolAlloc(pPool, 320);

	pA->pNext = pB;
	pB->pNext = NULL;
	pC->pNext = NULL;

	MarkReachable(pA);
	xrtMemPoolGC(pPool, FALSE);

	xrtMemPoolDestroy(pPool);
	xrtUnit();
	return 0;
}
```


## 与其他内存方案对比

| 特性 | MemPool | FSMemPool | MemUnit | malloc/free |
|------|---------|-----------|---------|-------------|
| 分配大小 | 可变 | 固定 | 固定 | 可变 |
| 小块池化 | ✅ | ✅ | ✅ | ❌ |
| 大块兜底 | ✅ | ❌ | ❌ | ✅ |
| 统一 GC 入口 | ✅ | ✅ | ✅ | ❌ |
| 最适合的问题 | 尺寸发散但生命周期相关 | 固定大小对象长期复用 | 单页级底层单元 | 简单通用分配 |
| 当前默认主线 | `XRT_OBJMODE_LOCAL` | `XRT_OBJMODE_LOCAL` | `XRT_OBJMODE_LOCAL` | 无池对象 |

### 选择建议

- 对象尺寸固定：优先 `FSMemPool`
- 只需要底层单页控制：看 `MemUnit`
- 尺寸开始发散：看 `MemPool`
- 场景简单或要与第三方库交互：直接 `malloc/free`


## 最佳实践

### 1. 默认先从 `xrtMemPoolCreate(0, XRT_OBJMODE_LOCAL)` 起步

这通常意味着：

- 默认 cutoff `1024`
- bucket 步长 `16`
- 已经足够覆盖大量日常小块和中块场景

### 2. 只有在尺寸分布很明确时，再收紧 cutoff

比如：

- 你明确知道大多数块都在 `256` 以内
- 想更严格地区分 pooled 范围和大块兜底范围

这时再考虑：

- `xrtMemPoolCreate(256, XRT_OBJMODE_LOCAL)`

### 3. 一个生命周期域尽量用一个池

比如：

- 一次请求
- 一次脚本执行
- 一轮批处理任务

这样销毁时可以统一收口，不容易漏掉零碎分配。

### 4. 做显式 GC 时，记得标记“对象本体 + 对象拥有的池内资源”

只标对象本体，不标它拥有的池内字符串 / payload，是最常见错误之一。

### 5. 不要跨池释放，不要错线程乱改

当前主线里：

- 跨池释放会触碰错误对象头
- 错线程修改会被拒绝


## 相关文档

- [FSMemPool 固定大小内存池](api-mempool-fs.md)
- [MemUnit 内存单元管理](api-memunit.md)
- [BSMM 块结构内存管理](api-bsmm.md)
- [MemPool 入门：什么时候该用通用变长池，而不是 `fsmempool`、`memunit` 或直接 `malloc/free`](../guide/mempool-intro.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#mempool-通用内存池)

</div>
