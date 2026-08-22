#ifndef XRT_RUNTIME_CALL_H
#define XRT_RUNTIME_CALL_H

#include <xrt/runtime_type.h>
#include <xrt/value.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_CALL) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_CALL requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_CALL) && !defined(XRT_FEATURE_VALUE)
	#error "XRUNTIME_FEATURE_RUNTIME_CALL requires XRT_FEATURE_VALUE"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_CALL)

#define XRT_CALL_RESULT_INLINE_COUNT 4u



typedef struct xrtcallable xrtcallable;
typedef struct xrtcallframe xrtcallframe;
typedef struct xrtcallresult xrtcallresult;



/* 动态调用模块稳定错误代码。 */
typedef enum xcallerror {
	XCALL_ERROR_CALLABLE = 1,
	XCALL_ERROR_SIGNATURE,
	XCALL_ERROR_FRAME,
	XCALL_ERROR_RESULT,
	XCALL_ERROR_ENTRY,
	XCALL_ERROR_REFERENCE
} xcallerror;



/*
	调用帧只借用 Self、参数、名称、值和上下文，不接管任何资源。
	位置参数包含传入的全部参数，超出固定形参的部分即为变长参数。
*/
struct xrtcallframe {
	xvalue* Self;
	const xrtfunctionsig* Signature;
	size_t ArgumentCount;
	xvalue* const* Arguments;
	size_t KeywordCount;
	const xstrview* KeywordNames;
	xvalue* const* KeywordValues;
	ptr Context;
};



/*
	调用结果拥有其中的 Value 引用；零初始化与 XRT_CALL_RESULT_INIT 都是有效状态。
	字段用于无分配初始化，调用方不得直接修改字段。
*/
struct xrtcallresult {
	size_t Count;
	size_t OverflowCapacity;
	xvalue* Inline[XRT_CALL_RESULT_INLINE_COUNT];
	xvalue** Overflow;
};



#define XRT_CALL_RESULT_INIT { 0 }



/* 动态入口可以并发和重入调用，具体环境是否支持并发由入口实现决定。 */
typedef bool (*xrtcallproc)(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
);



/* 环境释放器在最后一个 callable 引用释放时执行一次。 */
typedef void (*xrtcalldrop)(ptr pEnvironment);



XRT_EXTERN_C_BEGIN



/* 检查调用帧结构、参数绑定、可选参数、变长参数和关键字参数是否符合有效签名。 */
XRT_API bool xrtCallFrameValidate(const xrtcallframe* pFrame);



/* 按原始位置返回借用参数，越界时报告范围错误。 */
XRT_API xvalue* xrtCallFrameArgument(
	const xrtcallframe* pFrame,
	size_t iIndex
);



/* 按完整名称返回借用关键字值；未提供时返回空而不设置错误。 */
XRT_API xvalue* xrtCallFrameKeyword(
	const xrtcallframe* pFrame,
	xstrview Name
);



/* 按有效签名的形参下标返回位置或关键字传入的借用值；可选参数缺失时返回空。 */
XRT_API xvalue* xrtCallFrameParameter(
	const xrtcallframe* pFrame,
	size_t iIndex
);



/* 初始化一个新的调用结果；已经持有资源的结果必须先 Unit。 */
XRT_API void xrtCallResultInit(xrtcallresult* pResult);



/* 释放结果值但保留溢出容量，结果可以继续复用。 */
XRT_API void xrtCallResultClear(xrtcallresult* pResult);



/* 释放结果值和溢出存储。 */
XRT_API void xrtCallResultUnit(xrtcallresult* pResult);



/* 返回调用结果数量。 */
XRT_API size_t xrtCallResultCount(const xrtcallresult* pResult);



/* 返回指定下标借用的结果值。 */
XRT_API xvalue* xrtCallResultGet(
	const xrtcallresult* pResult,
	size_t iIndex
);



/* 增加值引用后替换已有下标或追加到末尾，不允许创建稀疏结果。 */
XRT_API bool xrtCallResultSet(
	xrtcallresult* pResult,
	size_t iIndex,
	const xvalue* pValue
);



/* 移交值引用后替换已有下标或追加到末尾，成功时清空来源槽。 */
XRT_API bool xrtCallResultSetTake(
	xrtcallresult* pResult,
	size_t iIndex,
	xvalue** pValue
);



/* 增加值引用后追加一个结果。 */
XRT_API bool xrtCallResultPush(
	xrtcallresult* pResult,
	const xvalue* pValue
);



/* 移交值引用后追加一个结果。 */
XRT_API bool xrtCallResultPushTake(
	xrtcallresult* pResult,
	xvalue** pValue
);



/* 释放目标原值并把完整结果所有权移动到目标。 */
XRT_API bool xrtCallResultMove(
	xrtcallresult* pTarget,
	xrtcallresult* pSource
);



/* 创建不可变 callable；签名可以为空，非空签名及其引用必须覆盖 callable 生命周期。 */
XRT_API xrtcallable* xrtCallableCreate(
	const xrtfunctionsig* pSignature,
	xrtcallproc pEntry,
	ptr pEnvironment,
	xrtcalldrop pDropEnvironment
);



/* 增加或释放 callable 引用，最后一个引用负责释放环境。 */
XRT_API xrtcallable* xrtCallableRef(xrtcallable* pCallable);
XRT_API void xrtCallableUnref(xrtcallable* pCallable);



/* 返回拥有一个 callable 强引用的 C ABI 槽类型描述。 */
XRT_API const xrttype* xrtTypeCallable(void);



/* 返回 callable 借用的签名；动态 callable 可以返回空。 */
XRT_API const xrtfunctionsig* xrtCallableSignature(
	const xrtcallable* pCallable
);



/* 返回 callable 的稳定签名 ID；动态 callable 返回零。 */
XRT_API uint64 xrtCallableSignatureId(const xrtcallable* pCallable);



/*
	验证调用帧并通过临时结果执行入口。
	失败时清理入口的部分结果并保留调用方原结果，成功时原子替换结果。
*/
XRT_API bool xrtCallableInvoke(
	const xrtcallable* pCallable,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
);



XRT_EXTERN_C_END

#endif

#endif
