#ifndef XRT_TYPED_STACK_H
#define XRT_TYPED_STACK_H

#include <xrt/typed_array.h>



#if defined(XRUNTIME_FEATURE_TYPED_STACK) && !defined(XRUNTIME_FEATURE_TYPED_ARRAY)
	#error "XRUNTIME_FEATURE_TYPED_STACK requires XRUNTIME_FEATURE_TYPED_ARRAY"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_STACK)

/* 类型栈复用类型数组的连续存储与元素所有权，只公开后进先出语义。 */
typedef xtypedarray xtypedstack;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个拥有元素值的空类型栈。 */
XRT_API bool xrtTypedStackInit(
	xtypedstack* pStack,
	const xrttype* pItemType
);
XRT_API xtypedstack* xrtTypedStackCreate(const xrttype* pItemType);
XRT_API void xrtTypedStackUnit(xtypedstack* pStack);
XRT_API void xrtTypedStackDestroy(xtypedstack* pStack);



/* 返回借用元素类型、当前深度和容量。 */
XRT_API const xrttype* xrtTypedStackItemType(const xtypedstack* pStack);
XRT_API size_t xrtTypedStackCount(const xtypedstack* pStack);
XRT_API size_t xrtTypedStackCapacity(const xtypedstack* pStack);



/* 清空、预留或裁剪栈存储。 */
XRT_API void xrtTypedStackClear(xtypedstack* pStack);
XRT_API bool xrtTypedStackReserve(xtypedstack* pStack, size_t iCapacity);
XRT_API bool xrtTypedStackTrim(xtypedstack* pStack);



/* 复制压入元素；失败时栈保持原值。 */
XRT_API bool xrtTypedStackPush(
	xtypedstack* pStack,
	const void* pItem
);



/* 弹出栈顶；输出为空时销毁元素，否则移动到已初始化输出值。 */
XRT_API bool xrtTypedStackPop(xtypedstack* pStack, ptr pValue);



/* 按距栈顶深度返回借用值，深度零表示栈顶。 */
XRT_API ptr xrtTypedStackPeek(xtypedstack* pStack, size_t iDepth);
XRT_API const void* xrtTypedStackConstPeek(
	const xtypedstack* pStack,
	size_t iDepth
);
XRT_API ptr xrtTypedStackTop(xtypedstack* pStack);
XRT_API const void* xrtTypedStackConstTop(const xtypedstack* pStack);



/* 深复制类型栈，或比较精确类型、深度和元素顺序。 */
XRT_API xtypedstack* xrtTypedStackClone(const xtypedstack* pStack);
XRT_API bool xrtTypedStackEquals(
	const xtypedstack* pLeft,
	const xtypedstack* pRight
);



XRT_EXTERN_C_END

#endif

#endif
