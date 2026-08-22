#ifndef XRT_RUNTIME_VALUE_H
#define XRT_RUNTIME_VALUE_H

#include <xrt/runtime_type.h>
#include <xrt/value.h>

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)
	#include <xrt/runtime_object.h>
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS)
	#include <xrt/runtime_object_graph.h>
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE)
	#include <xrt/runtime_call.h>
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)
	#include <xrt/runtime_type_future.h>
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_OBJECT)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT requires XRUNTIME_FEATURE_RUNTIME_OBJECT"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) && !defined(XRT_FEATURE_VALUE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT requires XRT_FEATURE_VALUE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_CALL)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE requires XRUNTIME_FEATURE_RUNTIME_CALL"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE requires XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE) && \
	!defined(XRT_FEATURE_VALUE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE requires XRT_FEATURE_VALUE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_OBJECT)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK requires XRUNTIME_FEATURE_RUNTIME_OBJECT"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK) && !defined(XRT_FEATURE_VALUE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK requires XRT_FEATURE_VALUE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE) && \
	!defined(XRT_FEATURE_VALUE_GRAPH)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE requires XRT_FEATURE_VALUE_GRAPH"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE requires XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE requires XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS requires XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	#error "XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS requires XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS)

/* 运行时 Value 桥接模块稳定错误代码。 */
typedef enum xruntimevalueerror {
	XRUNTIME_VALUE_ERROR_TYPE = 1,
	XRUNTIME_VALUE_ERROR_OBJECT,
	XRUNTIME_VALUE_ERROR_CALLABLE,
	XRUNTIME_VALUE_ERROR_FUTURE,
	XRUNTIME_VALUE_ERROR_WEAK,
	XRUNTIME_VALUE_ERROR_OWNERSHIP,
	XRUNTIME_VALUE_ERROR_TRACE,
	XRUNTIME_VALUE_ERROR_ROOTS
} xruntimevalueerror;



XRT_EXTERN_C_BEGIN



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT)

/* 增加对象强引用并创建拥有该引用的 Value Handle。 */
XRT_API xvalue* xrtValueRuntimeObject(xrtobject* pObject);



/* 把对象强引用移交给 Value Handle，成功时清空来源槽。 */
XRT_API xvalue* xrtValueRuntimeObjectTake(xrtobject** pObject);



/* 判断值是否是运行时对象桥接值，不把动态字典对象误判为类对象。 */
XRT_API bool xrtValueIsRuntimeObject(const xvalue* pValue);



/* 返回 Value Handle 借用的运行时对象，值负责维持强生命周期。 */
XRT_API xrtobject* xrtValueGetRuntimeObject(const xvalue* pValue);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE)

/* 同步进度桥只借用 callable Value，调用方必须覆盖整个操作生命周期。 */
typedef struct xrtprogresscall {
	xvalue* Callback;
	bool InvokeFailed;
} xrtprogresscall;

/* 增加 callable 引用并创建拥有该引用的 Value Handle。 */
XRT_API xvalue* xrtValueCallable(xrtcallable* pCallable);



/* 把 callable 引用移交给 Value Handle，成功时清空来源槽。 */
XRT_API xvalue* xrtValueCallableTake(xrtcallable** pCallable);



/* 判断值是否是 callable 桥接值。 */
XRT_API bool xrtValueIsCallable(const xvalue* pValue);



/* 返回 Value Handle 借用的 callable。 */
XRT_API xrtcallable* xrtValueGetCallable(const xvalue* pValue);



/* 返回 callable 借用的函数签名，动态 callable 返回空。 */
XRT_API const xrtfunctionsig* xrtValueCallableSignature(
	const xvalue* pValue
);



/* 调用一个 callable Value，调用契约与 xrtCallableInvoke 一致。 */
XRT_API bool xrtValueInvoke(
	const xvalue* pCallable,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
);



/* 初始化同步进度桥；null Value 与空指针都表示不报告进度。 */
XRT_API void xrtProgressCallInit(
	xrtprogresscall* pContext,
	xvalue* pCallback
);



/* 把进度转发为 callable(processed, total, output) 并读取 bool 结果。 */
XRT_API bool xrtProgressCallInvoke(
	const xrtprogress* pProgress,
	ptr pUserData
);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)
/* 增加 Future 引用并创建拥有该引用的 Value Handle。 */
XRT_API xvalue* xrtValueFuture(xfuture* pFuture);



/* 把 Future 引用移交给 Value Handle，成功时清空来源槽。 */
XRT_API xvalue* xrtValueFutureTake(xfuture** pFuture);



/* 判断值是否是 Future 桥接值。 */
XRT_API bool xrtValueIsFuture(const xvalue* pValue);



/* 返回 Value Handle 借用的 Future。 */
XRT_API xfuture* xrtValueGetFuture(const xvalue* pValue);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK)

/* 复制弱引用并创建拥有该控制块引用的 Value Handle。 */
XRT_API xvalue* xrtValueWeak(const xrtweak* pWeak);



/* 把弱引用移交给 Value Handle，成功时把来源变为空弱引用。 */
XRT_API xvalue* xrtValueWeakTake(xrtweak* pWeak);



/* 判断值是否是运行时弱引用桥接值。 */
XRT_API bool xrtValueIsWeak(const xvalue* pValue);



/* 把 Value 中的弱引用复制到已初始化目标，并替换目标原值。 */
XRT_API bool xrtValueGetWeak(
	const xvalue* pValue,
	xrtweak* pWeak
);



/* 查询 Value 中的弱引用是否为空或已经过期。 */
XRT_API bool xrtValueWeakExpired(const xvalue* pValue);



/* 成功时从 Value 中的弱引用取得一个新的对象强引用。 */
XRT_API xrtobject* xrtValueWeakLock(const xvalue* pValue);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)

/* 返回拥有一个 xvalue 引用的 C ABI 槽所使用的进程期类型描述。 */
XRT_API const xrttype* xrtTypeValue(void);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)

/*
	枚举完整 Value 所有权图实际持有的运行时对象强引用。
	共享 Value 外壳和容器 backing 只追踪一次，重复拥有槽仍按实际引用计数保留。
	访问器返回 false 时必须设置错误；未设置时函数报告运行时追踪状态错误。
	成功返回会恢复调用前错误，不保留访问器在成功路径留下的临时错误。
*/
XRT_API bool xrtValueTraceRuntimeObjects(
	const xvalue* pValue,
	xrtobjectvisitor pVisit,
	ptr pContext
);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS)

/* 使用一个外部 Value 所有权图作为显式根执行对象图收集。 */
XRT_API bool xrtObjectGraphCollectValueRoot(
	xrtobjectgraph* pGraph,
	const xvalue* pRoot,
	xrtobjectgraphresult* pResult
);



/* 使用一组外部 Value 所有权图作为显式根执行对象图收集。 */
XRT_API bool xrtObjectGraphCollectValueRoots(
	xrtobjectgraph* pGraph,
	const xvalue* const* pRoots,
	size_t iRootCount,
	xrtobjectgraphresult* pResult
);

#endif



XRT_EXTERN_C_END

#endif

#endif
