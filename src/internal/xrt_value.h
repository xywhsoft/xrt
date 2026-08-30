#ifndef XRT_INTERNAL_VALUE_H
#define XRT_INTERNAL_VALUE_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_VALUE)

#define XRT_VALUE_FLAG_STATIC		0x0001u
#define XRT_VALUE_FLAG_OWNED_DATA	0x0002u
#define XRT_VALUE_FLAG_BUSY			0x0004u



typedef struct xvaluebacking xvaluebacking;



/* 动态值外壳固定为紧凑标量或一个 backing 指针。 */
struct xvalue {
	volatile int32 RefCount;
	uint16 Type;
	uint16 Flags;
	/* 调用者可选绑定的不可变语义类型身份；零表示未绑定。 */
	uint64 TypeId;
	union {
		bool Bool;
		int64 Int;
		uint64 UInt;
		double Float;
		xtime Time;
		ptr Pointer;
		struct {
			cbytes Data;
			size_t Size;
		} Blob;
		struct {
			ptr Data;
			const xvaluehandleops* Ops;
			ptr UserData;
		} Handle;
		xvaluebacking* Backing;
	} Data;
};



/* 为内部模块创建指定类型的零初始化值外壳。 */
xvalue* __xrtValueCreate(xvaluetype Type);



/* 判断类型是否属于基础容器。 */
bool __xrtValueContainerType(xvaluetype Type);



/* 不报告错误地计算已验证可哈希值。 */
uint64 __xrtValueHashKnown(const xvalue* pValue);



/* 不报告错误地比较两个已验证可哈希值。 */
bool __xrtValueEqualKnown(const xvalue* pLeft, const xvalue* pRight);



/* 在用户策略回调期间保护一组 Value 外壳，重复值只保护一次。 */
bool __xrtValueCallbackProtect(
	const xvalue* const* pValues,
	size_t iCount
);



/* 解除一组 Value 外壳的用户策略回调保护。 */
void __xrtValueCallbackUnprotect(
	const xvalue* const* pValues,
	size_t iCount
);



#if defined(XRT_FEATURE_VALUE_CONTAINER)

/* 释放一个值外壳持有的容器 backing。 */
void __xrtValueContainerRelease(xvalue* pValue);



/* 为容器创建共享 backing 的独立外壳。 */
xvalue* __xrtValueContainerClone(const xvalue* pValue);



/* 返回容器真值使用的元素数量。 */
size_t __xrtValueContainerCount(const xvalue* pValue);



/* 借用 Value Set 的底层集合，供集合关系和图层复用通用 Set 实现。 */
const xset* __xrtValueSetItems(const xvalue* pValue);



/* 查询 Object 是否采用逆插入顺序释放拥有值。 */
bool __xrtValueObjectDropsReverse(const xvalue* pValue);



#if defined(XRT_FEATURE_VALUE_COLLECTION)

/* 把准备容器的完整 backing 原子提交给同类型目标。 */
bool __xrtValueContainerCommit(xvalue* pTarget, xvalue* pPrepared);



/* 消费通用 Set 运算结果并包装成 Value Set。 */
xvalue* __xrtValueSetAdopt(xset* pItems);

#endif

#endif

#endif

#endif
