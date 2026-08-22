#ifndef XRT_RUNTIME_OBJECT_INTERNAL_H
#define XRT_RUNTIME_OBJECT_INTERNAL_H

#include <xrt/runtime_object.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT)

/* 对象图关闭时，对象控制块不包含任何收集器状态。 */
struct xrtobject {
	volatile int32 StrongCount;
	volatile int32 WeakCount;
	const xrttype* Type;
	size_t Size;
	size_t PayloadOffset;
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	volatile int32 State;
	struct xrtobjectgraph* Graph;
	xrtobject* GraphPrevious;
	xrtobject* GraphNext;
#endif
	uint8 Storage[1];
};



/* 供内建引用对象类型描述复用的进程期值操作表。 */
extern const xrttypeops __xrtObjectValueOperations;



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)

#define XRT_OBJECT_STATE_ACTIVE 0
#define XRT_OBJECT_STATE_FINALIZING 1
#define XRT_OBJECT_STATE_FINALIZED 2



/* 对象图终结流程复用的内部生命周期操作。 */
bool __xrtObjectBeginFinalize(xrtobject* pObject);
void __xrtObjectCancelFinalize(xrtobject* pObject);
void __xrtObjectDropPayload(xrtobject* pObject);
void __xrtObjectEndFinalize(xrtobject* pObject);



/* 普通最后引用释放时，由对象图实现摘除仍被跟踪的对象。 */
void __xrtObjectGraphDetach(xrtobject* pObject);

#endif

#endif

#endif
