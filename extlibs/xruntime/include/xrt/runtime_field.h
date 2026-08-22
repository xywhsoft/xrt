#ifndef XRT_RUNTIME_FIELD_H
#define XRT_RUNTIME_FIELD_H

#include <xrt/runtime_type.h>

#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD)
	#include <xrt/runtime_object.h>
	#include <xrt/runtime_value.h>
	#include <xrt/typed_dict.h>
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_FIELD) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_FIELD requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD) && !defined(XRUNTIME_FEATURE_RUNTIME_FIELD)
	#error "XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD requires XRUNTIME_FEATURE_RUNTIME_FIELD"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD) && !defined(XRUNTIME_FEATURE_TYPED_DICT)
	#error "XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD requires XRUNTIME_FEATURE_TYPED_DICT"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD) && !defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	#error "XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD requires XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD) && !defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)
	#error "XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD requires XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_FIELD)

/* 字段模块只描述实例布局事实，不承担语言级可见性或赋值策略。 */
#define XRT_FIELD_FLAG_READONLY UINT32_C(0x00000001)



/* 运行时字段模块稳定错误代码。 */
typedef enum xfielderror {
	XFIELD_ERROR_DESCRIPTOR = 1,
	XFIELD_ERROR_LOOKUP,
	XFIELD_ERROR_ACCESS
} xfielderror;



struct xrtfielddesc {
	xstrview Name;
	const xrttype* Type;
	size_t Offset;
	uint32 Flags;
};



struct xrtfieldtable {
	size_t Count;
	const xrtfielddesc* Fields;
};



XRT_EXTERN_C_BEGIN



/* 验证本类型及完整继承链中的字段名称、布局、类型和标志。 */
XRT_API bool xrtTypeFieldsValidate(const xrttype* pType);



/* 返回包含继承字段的总数；字段顺序始终是基类在前、派生类在后。 */
XRT_API size_t xrtTypeFieldCount(const xrttype* pType);



/* 按基类优先的稳定下标返回借用字段，越界时设置范围错误。 */
XRT_API const xrtfielddesc* xrtTypeField(
	const xrttype* pType,
	size_t iIndex
);



/* 沿当前类型和基类按精确名称查找借用字段；未找到不设置错误。 */
XRT_API const xrtfielddesc* xrtTypeFindField(
	const xrttype* pType,
	xstrview Name
);



/* 返回字段在给定类型继承链中的声明类型。 */
XRT_API const xrttype* xrtTypeFieldOwner(
	const xrttype* pType,
	const xrtfielddesc* pField
);



/* 返回实例中的借用字段地址；描述必须属于给定类型的继承链。 */
XRT_API const void* xrtFieldConstData(
	const xrttype* pType,
	const xrtfielddesc* pField,
	const void* pInstance
);
XRT_API ptr xrtFieldData(
	const xrttype* pType,
	const xrtfielddesc* pField,
	ptr pInstance
);



XRT_EXTERN_C_END

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD)

/* 动态字段表是独立对象图节点，载荷拥有按插入顺序保存的 Value 字典。 */
typedef xrtobject xrtdynamicfields;



/* 动态字段迭代器在迭代期间保留字段对象，并检测后续结构修改。 */
typedef struct xrtdynamicfielditer {
	xtypeddictiter Base;
	xrtdynamicfields* Fields;
} xrtdynamicfielditer;



/* 动态字段模块稳定错误代码。 */
typedef enum xdynamicfielderror {
	XDYNAMIC_FIELD_ERROR_ARGUMENT = 1,
	XDYNAMIC_FIELD_ERROR_TYPE,
	XDYNAMIC_FIELD_ERROR_OPERATION,
	XDYNAMIC_FIELD_ERROR_STATE
} xdynamicfielderror;



XRT_EXTERN_C_BEGIN



/* 返回进程期稳定的 xrt.DynamicFields 运行时类型。 */
XRT_API const xrttype* xrtDynamicFieldsType(void);



/* 创建、保留和释放动态字段对象。 */
XRT_API xrtdynamicfields* xrtDynamicFieldsCreate(void);
XRT_API xrtdynamicfields* xrtDynamicFieldsRef(xrtdynamicfields* pFields);
XRT_API void xrtDynamicFieldsUnref(xrtdynamicfields* pFields);



/* 返回字段数量、容量，并管理字段表存储。 */
XRT_API size_t xrtDynamicFieldsCount(const xrtdynamicfields* pFields);
XRT_API size_t xrtDynamicFieldsCapacity(const xrtdynamicfields* pFields);
XRT_API bool xrtDynamicFieldsClear(xrtdynamicfields* pFields);
XRT_API bool xrtDynamicFieldsReserve(
	xrtdynamicfields* pFields,
	size_t iCapacity
);
XRT_API bool xrtDynamicFieldsTrim(xrtdynamicfields* pFields);



/* 查询字段；Get 返回只读借用，GetRef 保留同一 Value，Copy 深复制完整图。 */
XRT_API bool xrtDynamicFieldsHas(
	const xrtdynamicfields* pFields,
	xstrview Name
);
XRT_API const xvalue* xrtDynamicFieldsGet(
	const xrtdynamicfields* pFields,
	xstrview Name
);
XRT_API xvalue* xrtDynamicFieldsGetRef(
	const xrtdynamicfields* pFields,
	xstrview Name
);
XRT_API xvalue* xrtDynamicFieldsCopy(
	const xrtdynamicfields* pFields,
	xstrview Name
);
XRT_API bool xrtDynamicFieldsStoredName(
	const xrtdynamicfields* pFields,
	xstrview Name,
	xstrview* pStoredName
);



/* 三种写入都隔离完整 Value 图；Take 成功移交来源，New 总是消费临时值。 */
XRT_API bool xrtDynamicFieldsSet(
	xrtdynamicfields* pFields,
	xstrview Name,
	const xvalue* pValue
);
XRT_API bool xrtDynamicFieldsSetTake(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue** ppValue
);
XRT_API bool xrtDynamicFieldsSetNew(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue* pValue
);



/* Ref 写入保留同一 Value 身份，供语言对象和显式共享图使用。 */
XRT_API bool xrtDynamicFieldsSetRef(
	xrtdynamicfields* pFields,
	xstrview Name,
	const xvalue* pValue
);
XRT_API bool xrtDynamicFieldsSetRefTake(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue** ppValue
);
XRT_API bool xrtDynamicFieldsSetRefNew(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue* pValue
);



/* 删除并释放字段，或把字段值移交给调用方。 */
XRT_API bool xrtDynamicFieldsRemove(
	xrtdynamicfields* pFields,
	xstrview Name
);
XRT_API xvalue* xrtDynamicFieldsTake(
	xrtdynamicfields* pFields,
	xstrview Name
);



/* 按稳定插入顺序或逆序迭代借用名称和值；启动函数负责初始化迭代器。 */
XRT_API bool xrtDynamicFieldsIterBegin(
	xrtdynamicfields* pFields,
	xrtdynamicfielditer* pIterator
);
XRT_API bool xrtDynamicFieldsIterRBegin(
	xrtdynamicfields* pFields,
	xrtdynamicfielditer* pIterator
);
XRT_API const xvalue* xrtDynamicFieldsIterNext(
	xrtdynamicfielditer* pIterator,
	xstrview* pName
);
XRT_API void xrtDynamicFieldsIterEnd(xrtdynamicfielditer* pIterator);



/* 事务合并或深复制动态字段对象。 */
XRT_API bool xrtDynamicFieldsMerge(
	xrtdynamicfields* pTarget,
	const xrtdynamicfields* pSource,
	bool bReplace
);
XRT_API xrtdynamicfields* xrtDynamicFieldsClone(
	const xrtdynamicfields* pFields
);



/* 构造便于语言绑定使用的名称、值、二元项数组和独立 Object。 */
XRT_API xvalue* xrtDynamicFieldsKeys(const xrtdynamicfields* pFields);
XRT_API xvalue* xrtDynamicFieldsValues(const xrtdynamicfields* pFields);
XRT_API xvalue* xrtDynamicFieldsItems(const xrtdynamicfields* pFields);
XRT_API xvalue* xrtDynamicFieldsToValue(const xrtdynamicfields* pFields);



/* 从 Value Object 深复制名称和值并创建独立动态字段对象。 */
XRT_API xrtdynamicfields* xrtDynamicFieldsFromValue(const xvalue* pValue);



XRT_EXTERN_C_END

#endif

#endif
