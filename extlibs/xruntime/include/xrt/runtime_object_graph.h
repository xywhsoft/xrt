#ifndef XRT_RUNTIME_OBJECT_GRAPH_H
#define XRT_RUNTIME_OBJECT_GRAPH_H

#include <xrt/runtime_object.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH) && !defined(XRUNTIME_FEATURE_RUNTIME_OBJECT)
	#error "XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH requires XRUNTIME_FEATURE_RUNTIME_OBJECT"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)

typedef struct xrtobjectgraph xrtobjectgraph;



/* 对象图模块稳定错误代码。 */
typedef enum xobjectgrapherror {
	XOBJECT_GRAPH_ERROR_ARGUMENT = 1,
	XOBJECT_GRAPH_ERROR_TRACK,
	XOBJECT_GRAPH_ERROR_TRACE,
	XOBJECT_GRAPH_ERROR_STATE,
	XOBJECT_GRAPH_ERROR_ROOTS
} xobjectgrapherror;



/* 根枚举器可补充运行时栈、生成器和宿主状态中的借用根。 */
typedef bool (*xrtobjectrootproc)(
	xrtobjectvisitor pVisit,
	ptr pVisitContext,
	ptr pContext
);



/* 一次成功收集的稳定统计结果。 */
typedef struct xrtobjectgraphresult {
	size_t TrackedCount;
	size_t EdgeCount;
	size_t RootCount;
	size_t CollectedCount;
} xrtobjectgraphresult;



XRT_EXTERN_C_BEGIN



/* 创建和销毁一个不拥有对象强引用的独立对象图。 */
XRT_API xrtobjectgraph* xrtObjectGraphCreate(void);
XRT_API void xrtObjectGraphDestroy(xrtobjectgraph* pGraph);



/* 幂等跟踪对象；同一对象在任意时刻只能属于一个对象图。 */
XRT_API bool xrtObjectGraphTrack(
	xrtobjectgraph* pGraph,
	xrtobject* pObject
);



/* 停止跟踪对象；对象不属于该图时返回 false 且不设置错误。 */
XRT_API bool xrtObjectGraphUntrack(
	xrtobjectgraph* pGraph,
	xrtobject* pObject
);



/* 查询借用对象是否属于图以及图当前跟踪的对象数量。 */
XRT_API bool xrtObjectGraphContains(
	const xrtobjectgraph* pGraph,
	const xrtobject* pObject
);
XRT_API size_t xrtObjectGraphCount(const xrtobjectgraph* pGraph);



/* 在调用方保证对象图静止的安全点收集不可达强引用环。 */
XRT_API bool xrtObjectGraphCollect(
	xrtobjectgraph* pGraph,
	xrtobjectgraphresult* pResult
);



/* 使用可选的宿主根枚举器收集不可达强引用环。 */
XRT_API bool xrtObjectGraphCollectRoots(
	xrtobjectgraph* pGraph,
	xrtobjectrootproc pRoots,
	ptr pContext,
	xrtobjectgraphresult* pResult
);



XRT_EXTERN_C_END

#endif

#endif
