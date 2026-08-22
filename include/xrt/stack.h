#ifndef XRT_STACK_H
#define XRT_STACK_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_STACK) && !defined(XRT_FEATURE_ARRAY)
	#error "XRT_FEATURE_STACK requires XRT_FEATURE_ARRAY"
#endif

#if defined(XRT_FEATURE_BLOCK_STACK) && !defined(XRT_FEATURE_ARRAY)
	#error "XRT_FEATURE_BLOCK_STACK requires XRT_FEATURE_ARRAY"
#endif

#if defined(XRT_FEATURE_PTR_FIXED_STACK) && !defined(XRT_FEATURE_FIXED_STACK)
	#error "XRT_FEATURE_PTR_FIXED_STACK requires XRT_FEATURE_FIXED_STACK"
#endif

#if defined(XRT_FEATURE_PTR_STACK) && !defined(XRT_FEATURE_STACK)
	#error "XRT_FEATURE_PTR_STACK requires XRT_FEATURE_STACK"
#endif



#if defined(XRT_FEATURE_FIXED_STACK)

/* 固定栈可借用外部缓冲，也可拥有创建时分配的固定缓冲。 */
typedef struct xfixedstack {
	bytes Data;
	ptr Allocation;
	size_t ItemSize;
	size_t Count;
	size_t Capacity;
} xfixedstack;



XRT_EXTERN_C_BEGIN



/* 在不与栈结构重叠的调用方缓冲上初始化固定容量栈。 */
XRT_API bool xrtFixedStackInit(
	xfixedstack* pStack,
	ptr pMemory,
	size_t iMemorySize,
	size_t iItemSize
);



/* 创建拥有固定容量缓冲的栈。 */
XRT_API xfixedstack* xrtFixedStackCreate(size_t iCapacity, size_t iItemSize);



/* 释放创建时取得的固定缓冲，但不释放栈结构。 */
XRT_API void xrtFixedStackUnit(xfixedstack* pStack);



/* 释放固定缓冲和创建的栈结构。 */
XRT_API void xrtFixedStackDestroy(xfixedstack* pStack);



/* 清空栈内容并保留固定容量。 */
XRT_API void xrtFixedStackClear(xfixedstack* pStack);



/* 返回剩余可压入元素数量。 */
XRT_API size_t xrtFixedStackSpace(const xfixedstack* pStack);



/* 返回指定 0 基位置的可写元素借用地址。 */
XRT_API ptr xrtFixedStackGet(xfixedstack* pStack, size_t iIndex);



/* 返回指定 0 基位置的只读元素借用地址。 */
XRT_API const void* xrtFixedStackConstGet(const xfixedstack* pStack, size_t iIndex);



/* 取得一个未初始化栈顶槽，栈满时失败。 */
XRT_API ptr xrtFixedStackAdd(xfixedstack* pStack);



/* 复制一个元素压入固定栈。 */
XRT_API bool xrtFixedStackPush(xfixedstack* pStack, const void* pItem);



/* 弹出栈顶元素，并可把内容复制到外部输出缓冲。 */
XRT_API bool xrtFixedStackPop(xfixedstack* pStack, ptr pItem);



/* 返回距栈顶指定深度的可写元素，深度 0 表示栈顶。 */
XRT_API ptr xrtFixedStackPeek(xfixedstack* pStack, size_t iDepth);



/* 返回距栈顶指定深度的只读元素，深度 0 表示栈顶。 */
XRT_API const void* xrtFixedStackConstPeek(const xfixedstack* pStack, size_t iDepth);



/* 返回可写栈顶元素借用地址。 */
XRT_API ptr xrtFixedStackTop(xfixedstack* pStack);



/* 返回只读栈顶元素借用地址。 */
XRT_API const void* xrtFixedStackConstTop(const xfixedstack* pStack);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_PTR_FIXED_STACK)

/* 固定指针栈只保存指针值，不拥有指针指向的对象。 */
typedef xfixedstack xptrfixedstack;



XRT_EXTERN_C_BEGIN



/* 在不与栈结构重叠的调用方指针数组上初始化固定容量指针栈。 */
XRT_API bool xrtPtrFixedStackInit(
	xptrfixedstack* pStack,
	ptr* pMemory,
	size_t iCapacity
);



/* 创建拥有指定固定容量的指针栈。 */
XRT_API xptrfixedstack* xrtPtrFixedStackCreate(size_t iCapacity);



/* 释放拥有的指针存储区，但不释放任何指针目标或栈结构。 */
XRT_API void xrtPtrFixedStackUnit(xptrfixedstack* pStack);



/* 释放创建的指针栈结构，但不释放任何指针目标。 */
XRT_API void xrtPtrFixedStackDestroy(xptrfixedstack* pStack);



/* 清空固定指针栈，但不释放任何指针目标。 */
XRT_API void xrtPtrFixedStackClear(xptrfixedstack* pStack);



/* 返回固定指针栈剩余容量。 */
XRT_API size_t xrtPtrFixedStackSpace(const xptrfixedstack* pStack);



/* 返回指定 0 基位置的指针值。 */
XRT_API ptr xrtPtrFixedStackGet(const xptrfixedstack* pStack, size_t iIndex);



/* 压入一个可为空的指针值。 */
XRT_API bool xrtPtrFixedStackPush(xptrfixedstack* pStack, ptr pValue);



/* 弹出指针值；输出为空表示只删除栈顶。 */
XRT_API bool xrtPtrFixedStackPop(xptrfixedstack* pStack, ptr* pValue);



/* 返回距栈顶指定深度的指针值，深度 0 表示栈顶。 */
XRT_API ptr xrtPtrFixedStackPeek(const xptrfixedstack* pStack, size_t iDepth);



/* 返回栈顶指针值；合法空值与错误通过错误状态区分。 */
XRT_API ptr xrtPtrFixedStackTop(const xptrfixedstack* pStack);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_STACK) || defined(XRT_FEATURE_BLOCK_STACK)

#include <xrt/array.h>

#endif



#if defined(XRT_FEATURE_STACK)



/* 动态栈复用连续数组存储，结构性修改可能改变元素地址。 */
typedef xarray xstack;



XRT_EXTERN_C_BEGIN



/* 初始化使用默认对齐的空动态栈。 */
XRT_API bool xrtStackInit(xstack* pStack, size_t iItemSize);



/* 初始化显式过对齐动态栈。 */
XRT_API bool xrtStackInitAligned(xstack* pStack, size_t iItemSize, size_t iAlignment);



/* 创建使用默认对齐的空动态栈。 */
XRT_API xstack* xrtStackCreate(size_t iItemSize);



/* 创建显式过对齐动态栈。 */
XRT_API xstack* xrtStackCreateAligned(size_t iItemSize, size_t iAlignment);



/* 释放动态栈元素内存，但不释放栈结构。 */
XRT_API void xrtStackUnit(xstack* pStack);



/* 释放动态栈全部资源和栈结构。 */
XRT_API void xrtStackDestroy(xstack* pStack);



/* 清空动态栈并保留容量。 */
XRT_API void xrtStackClear(xstack* pStack);



/* 保证动态栈至少具有指定元素容量。 */
XRT_API bool xrtStackReserve(xstack* pStack, size_t iCapacity);



/* 将动态栈容量裁剪到当前深度。 */
XRT_API bool xrtStackTrim(xstack* pStack);



/* 返回指定 0 基位置的可写元素借用地址。 */
XRT_API ptr xrtStackGet(xstack* pStack, size_t iIndex);



/* 返回指定 0 基位置的只读元素借用地址。 */
XRT_API const void* xrtStackConstGet(const xstack* pStack, size_t iIndex);



/* 取得一个未初始化栈顶槽。 */
XRT_API ptr xrtStackAdd(xstack* pStack);



/* 复制一个元素压入动态栈。 */
XRT_API bool xrtStackPush(xstack* pStack, const void* pItem);



/* 弹出栈顶元素，并可把内容复制到外部输出缓冲。 */
XRT_API bool xrtStackPop(xstack* pStack, ptr pItem);



/* 返回距栈顶指定深度的可写元素，深度 0 表示栈顶。 */
XRT_API ptr xrtStackPeek(xstack* pStack, size_t iDepth);



/* 返回距栈顶指定深度的只读元素，深度 0 表示栈顶。 */
XRT_API const void* xrtStackConstPeek(const xstack* pStack, size_t iDepth);



/* 返回可写栈顶元素借用地址。 */
XRT_API ptr xrtStackTop(xstack* pStack);



/* 返回只读栈顶元素借用地址。 */
XRT_API const void* xrtStackConstTop(const xstack* pStack);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_BLOCK_STACK)

#define XRT_BLOCK_STACK_ITEMS_MAX		256u
#define XRT_BLOCK_STACK_BYTES_DEFAULT	16384u



/*
 * 分块栈只移动块索引，不移动块内元素。
 * Blocks 的元素类型属于内部实现，调用方只能读取其 Count 和 Capacity 做诊断。
 */
typedef struct xblockstack {
	xarray Blocks;
	size_t ItemSize;
	size_t Count;
	size_t Capacity;
	size_t BlockItems;
	size_t Alignment;
} xblockstack;



XRT_EXTERN_C_BEGIN



/* 使用自动块尺寸初始化默认对齐分块栈。 */
XRT_API bool xrtBlockStackInit(xblockstack* pStack, size_t iItemSize);



/* 使用指定元素对齐和每块元素数初始化分块栈。 */
XRT_API bool xrtBlockStackInitLayout(
	xblockstack* pStack,
	size_t iItemSize,
	size_t iAlignment,
	size_t iBlockItems
);



/* 创建使用自动块尺寸的默认对齐分块栈。 */
XRT_API xblockstack* xrtBlockStackCreate(size_t iItemSize);



/* 创建使用指定元素对齐和每块元素数的分块栈。 */
XRT_API xblockstack* xrtBlockStackCreateLayout(
	size_t iItemSize,
	size_t iAlignment,
	size_t iBlockItems
);



/* 释放全部数据块和块索引，但不释放栈结构。 */
XRT_API void xrtBlockStackUnit(xblockstack* pStack);



/* 释放分块栈全部资源和创建的栈结构。 */
XRT_API void xrtBlockStackDestroy(xblockstack* pStack);



/* 清空分块栈并保留已经分配的数据块。 */
XRT_API void xrtBlockStackClear(xblockstack* pStack);



/* 保证分块栈至少具有指定元素容量，失败时保持原状态。 */
XRT_API bool xrtBlockStackReserve(xblockstack* pStack, size_t iCapacity);



/* 释放当前深度不再需要的数据块，保留轻量块索引缓存。 */
XRT_API bool xrtBlockStackTrim(xblockstack* pStack);



/* 返回指定 0 基位置的可写元素借用地址。 */
XRT_API ptr xrtBlockStackGet(xblockstack* pStack, size_t iIndex);



/* 返回指定 0 基位置的只读元素借用地址。 */
XRT_API const void* xrtBlockStackConstGet(const xblockstack* pStack, size_t iIndex);



/* 取得一个未初始化栈顶槽，既有活动元素地址保持稳定。 */
XRT_API ptr xrtBlockStackAdd(xblockstack* pStack);



/* 浅复制一个完整可读元素压入分块栈，来源可以是任一活动元素。 */
XRT_API bool xrtBlockStackPush(xblockstack* pStack, const void* pItem);



/* 弹出栈顶元素；调用方必须保证可选输出不与该栈的任何数据块重叠。 */
XRT_API bool xrtBlockStackPop(xblockstack* pStack, ptr pItem);



/* 返回距栈顶指定深度的可写元素，深度 0 表示栈顶。 */
XRT_API ptr xrtBlockStackPeek(xblockstack* pStack, size_t iDepth);



/* 返回距栈顶指定深度的只读元素，深度 0 表示栈顶。 */
XRT_API const void* xrtBlockStackConstPeek(const xblockstack* pStack, size_t iDepth);



/* 返回可写栈顶元素借用地址。 */
XRT_API ptr xrtBlockStackTop(xblockstack* pStack);



/* 返回只读栈顶元素借用地址。 */
XRT_API const void* xrtBlockStackConstTop(const xblockstack* pStack);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_PTR_STACK)

/* 指针栈只保存指针值，不拥有指针指向的对象。 */
typedef xstack xptrstack;



XRT_EXTERN_C_BEGIN



/* 初始化空指针栈。 */
XRT_API bool xrtPtrStackInit(xptrstack* pStack);



/* 创建空指针栈。 */
XRT_API xptrstack* xrtPtrStackCreate(void);



/* 释放指针存储区但不释放指针目标。 */
XRT_API void xrtPtrStackUnit(xptrstack* pStack);



/* 释放指针栈结构但不释放指针目标。 */
XRT_API void xrtPtrStackDestroy(xptrstack* pStack);



/* 清空指针栈但不释放指针目标。 */
XRT_API void xrtPtrStackClear(xptrstack* pStack);



/* 保证指针栈至少具有指定容量。 */
XRT_API bool xrtPtrStackReserve(xptrstack* pStack, size_t iCapacity);



/* 将指针栈容量裁剪到当前深度。 */
XRT_API bool xrtPtrStackTrim(xptrstack* pStack);



/* 返回指定 0 基位置的指针值。 */
XRT_API ptr xrtPtrStackGet(const xptrstack* pStack, size_t iIndex);



/* 压入一个可为空的指针值。 */
XRT_API bool xrtPtrStackPush(xptrstack* pStack, ptr pValue);



/* 弹出指针值；输出为空表示只删除栈顶。 */
XRT_API bool xrtPtrStackPop(xptrstack* pStack, ptr* pValue);



/* 返回距栈顶指定深度的指针值，深度 0 表示栈顶。 */
XRT_API ptr xrtPtrStackPeek(const xptrstack* pStack, size_t iDepth);



/* 返回栈顶指针值；合法空值与错误通过错误状态区分。 */
XRT_API ptr xrtPtrStackTop(const xptrstack* pStack);



XRT_EXTERN_C_END

#endif

#endif
