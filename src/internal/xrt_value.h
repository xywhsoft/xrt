#ifndef XRT_INTERNAL_VALUE_H
#define XRT_INTERNAL_VALUE_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_VALUE)

/* Public ownership-mutating entries delegate to a private body so every
 * early return balances the outer transition. Nested calls use the same
 * domain, but mutations remain concurrent with each other: this is not a
 * replacement for the caller's normal same-container synchronization. */
#define XRT_VALUE_MUTATION_RETURN(Type, Call) \
	XRT_OWNERSHIP_MUTATION_RETURN(Type, (Type)0, Call)
#define XRT_VALUE_MUTATION_RETURN_VOID(Call) XRT_OWNERSHIP_MUTATION_RETURN_VOID(Call)

#define XRT_VALUE_FLAG_STATIC		0x0001u
#define XRT_VALUE_FLAG_OWNED_DATA	0x0002u
#define XRT_VALUE_FLAG_BUSY			0x0004u
#define XRT_VALUE_FLAG_FINALIZING	0x0008u
#define XRT_VALUE_FLAG_OWNERSHIP_CLEARED 0x0010u
#define XRT_VALUE_FLAG_PHASED_DROP 0x0020u



typedef struct xvaluebacking xvaluebacking;



/* 动态值外壳固定为紧凑标量或一个 backing 指针。 */
struct xvalue {
	volatile int32 RefCount;
	/* 外壳资源结束后仍由弱引用保持分配，初始自持一个弱引用。 */
	volatile int32 WeakCount;
	uint16 Type;
	uint16 Flags;
	/* 调用者可选绑定的不可变语义类型身份；零表示未绑定。 */
	uint64 TypeId;
	/* 可选语义值身份策略；函数和用户数据随外壳 Clone 传播。 */
	xvalueidentityhash IdentityHash;
	xvalueidentityequal IdentityEqual;
	ptr IdentityUserData;
	/* Resident or code-pinned adapter; Clone carries the policy, not a stale
	 * handle address. The adapter reads the cloned handle from its own shell. */
	xvalueownershiptrace OwnershipTrace;
	/* Written only under exclusive ownership freeze; weak promotion reads it
	 * inside mutation admission. Never copied to a new Value shell. */
	const void* OwnershipClaim;
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

xrtownershipref __xrtValueBackingOwnership(const xvalue* pValue);
const xrtownershipadapterv1* __xrtValueBackingOwnershipAdapterV1(xrtownershipref Reference);
bool __xrtValueObjectClaimReceiver(xvalue* pValue, const void* pToken);
void __xrtValueObjectRestoreReceiver(xvalue* pValue, const void* pToken);
bool __xrtValueObjectOwnershipPolicyMatches(const xvalue* pValue, const xvalueobjectownershipv1* pPolicy);
const xrtownershipadapterv1* __xrtValueObjectOwnershipAdapterV1(
	xrtownershipref Reference, const xvalueobjectownershipv1* pPolicy);

/* New deep-clone backing inherits immutable capabilities, never a finalizer. */
bool __xrtValueObjectLifetimeCopy(xvalue* pTarget, const xvalue* pSource);

/* Release with the caller's own mutation active on entry/return. A NULL
 * scope is only for a nonterminal Clear under exclusive graph freeze. */
void __xrtValueContainerRelease(xvalue* pValue, xrtownershipscope* pMutation);



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
