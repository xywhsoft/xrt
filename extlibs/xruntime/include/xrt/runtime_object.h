#ifndef XRT_RUNTIME_OBJECT_H
#define XRT_RUNTIME_OBJECT_H

#include <xrt/runtime_type.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_OBJECT requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT)

typedef struct xrtobject xrtobject;



/* 运行时对象模块稳定错误代码。 */
typedef enum xobjecterror {
	XOBJECT_ERROR_TYPE = 1,
	XOBJECT_ERROR_SIZE,
	XOBJECT_ERROR_REFERENCE,
	XOBJECT_ERROR_WEAK,
	XOBJECT_ERROR_INITIALIZE
} xobjecterror;



/* 弱引用是一个可复制、可移动的控制块引用，不拥有对象的强生命周期。 */
typedef struct xrtweak {
	ptr Control;
} xrtweak;



XRT_EXTERN_C_BEGIN



/* 返回对象强引用槽使用的进程期稳定值操作表。 */
XRT_API const xrttypeops* xrtObjectValueOps(void);



/* 按类型声明的负载大小创建堆对象，并执行类型初始化操作。 */
XRT_API xrtobject* xrtObjectCreate(const xrttype* pType);



/* 创建至少容纳指定字节数的对象，用于尾随数据和 native-backed 对象。 */
XRT_API xrtobject* xrtObjectCreateSized(
	const xrttype* pType,
	size_t iSize
);



/* 增加对象强引用；调用方必须已经持有一个有效强引用。 */
XRT_API xrtobject* xrtObjectRef(xrtobject* pObject);



/* 释放一个强引用；最后一个强引用负责执行一次 Drop。 */
XRT_API void xrtObjectUnref(xrtobject* pObject);



/* 借用对象类型、负载地址和真实负载长度。 */
XRT_API const xrttype* xrtObjectType(const xrtobject* pObject);
XRT_API ptr xrtObjectData(xrtobject* pObject);
XRT_API const void* xrtObjectConstData(const xrtobject* pObject);
XRT_API size_t xrtObjectSize(const xrtobject* pObject);



/* 返回瞬时强引用数量，并判断调用方是否持有唯一强引用。 */
XRT_API size_t xrtObjectRefCount(const xrtobject* pObject);
XRT_API bool xrtObjectUnique(const xrtobject* pObject);



/* 初始化、复制、移动、替换和销毁弱引用值。 */
XRT_API bool xrtWeakInit(xrtweak* pWeak, xrtobject* pObject);
XRT_API bool xrtWeakCopy(xrtweak* pTarget, const xrtweak* pSource);
XRT_API bool xrtWeakMove(xrtweak* pTarget, xrtweak* pSource);
XRT_API bool xrtWeakSet(xrtweak* pWeak, xrtobject* pObject);
XRT_API void xrtWeakUnit(xrtweak* pWeak);



/* 判断弱引用当前是否为空或已经过期；结果只是瞬时状态。 */
XRT_API bool xrtWeakExpired(const xrtweak* pWeak);



/* 成功时返回一个新的强引用；对象已销毁时返回空而不设置错误。 */
XRT_API xrtobject* xrtWeakLock(const xrtweak* pWeak);



XRT_EXTERN_C_END

#endif

#endif
