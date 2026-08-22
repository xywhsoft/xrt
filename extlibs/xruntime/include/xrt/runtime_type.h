#ifndef XRT_RUNTIME_TYPE_H
#define XRT_RUNTIME_TYPE_H

#include <xrt/core.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_OBJECT requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE)

/*
	运行时类型描述只表达 C、XRT 和上层语言都能解释的事实。
	描述对象由声明方持有，注册表只借用，不绑定任何语言语法。
*/
typedef struct xrttype xrttype;
typedef struct xrttypeops xrttypeops;
typedef struct xrtinstanceops xrtinstanceops;
typedef struct xrtobject xrtobject;
typedef struct xrtfunctionsig xrtfunctionsig;
typedef struct xrtparamdesc xrtparamdesc;
typedef struct xrtfielddesc xrtfielddesc;
typedef struct xrtfieldtable xrtfieldtable;
typedef struct xrtmethoddesc xrtmethoddesc;
typedef struct xrtmethodtable xrtmethodtable;
typedef struct xrtprotocol xrtprotocol;
typedef struct xrtprotocolwitness xrtprotocolwitness;
typedef struct xrtprotocolregistry xrtprotocolregistry;
typedef struct xrtenum xrtenum;
typedef struct xrtenumvariant xrtenumvariant;
typedef struct xrttyperegistry xrttyperegistry;



/* 对象访问器接收一个借用的强引用；返回 false 会立即终止本次遍历。 */
typedef bool (*xrtobjectvisitor)(xrtobject* pObject, ptr pContext);



/*
	类型格式化器通过该回调同步写出借用 UTF-8 分块。
	分块只在本次调用期间有效；返回 false 要求格式化器立即停止。
*/
typedef bool (*xrttypewriter)(xstrview Text, ptr pContext);



typedef enum xrttypekind {
	XRT_TYPE_INVALID = 0,
	XRT_TYPE_NULL,
	XRT_TYPE_BOOL,
	XRT_TYPE_SIGNED_INT,
	XRT_TYPE_UNSIGNED_INT,
	XRT_TYPE_FLOAT,
	XRT_TYPE_STRING,
	XRT_TYPE_BYTES,
	XRT_TYPE_TIME,
	XRT_TYPE_POINTER,
	XRT_TYPE_CALLABLE,
	XRT_TYPE_ARRAY,
	XRT_TYPE_LIST,
	XRT_TYPE_SET,
	XRT_TYPE_DICT,
	XRT_TYPE_RECORD,
	XRT_TYPE_HANDLE,
	XRT_TYPE_TYPE,
	XRT_TYPE_FUTURE,
	XRT_TYPE_CLASS,
	XRT_TYPE_ENUM,
	XRT_TYPE_PROTOCOL,
	XRT_TYPE_OPTIONAL,
	XRT_TYPE_WEAK
} xrttypekind;



/* 运行时类型模块稳定错误代码。 */
typedef enum xtypeerror {
	XTYPE_ERROR_DESCRIPTOR = 1,
	XTYPE_ERROR_SIGNATURE,
	XTYPE_ERROR_OPERATION,
	XTYPE_ERROR_REGISTRY,
	XTYPE_ERROR_PROTOCOL,
	XTYPE_ERROR_ENUM
} xtypeerror;



/* 类型标志用于统一生命周期和 ABI 判断，不承担语言级访问控制。 */
#define XRT_TYPE_FLAG_TRIVIAL_COPY	UINT32_C(0x00000001)
#define XRT_TYPE_FLAG_TRIVIAL_DROP	UINT32_C(0x00000002)
#define XRT_TYPE_FLAG_COPYABLE		UINT32_C(0x00000004)
#define XRT_TYPE_FLAG_REFERENCE		UINT32_C(0x00000008)
#define XRT_TYPE_FLAG_NULLABLE		UINT32_C(0x00000010)
#define XRT_TYPE_FLAG_FINAL			UINT32_C(0x00000020)
#define XRT_TYPE_FLAG_RELOCATABLE	UINT32_C(0x00000040)



typedef enum xrtparammode {
	XRT_PARAM_DEFAULT = 0,
	XRT_PARAM_BYVAL,
	XRT_PARAM_BYREF
} xrtparammode;



#define XRT_PARAM_FLAG_OPTIONAL		UINT32_C(0x00000001)
#define XRT_PARAM_FLAG_NAMED_ONLY	UINT32_C(0x00000002)
#define XRT_FUNCTION_FLAG_VARARGS	UINT32_C(0x00000001)
#define XRT_FUNCTION_FLAG_KWARGS	UINT32_C(0x00000002)
#define XRT_METHOD_FLAG_STATIC		UINT32_C(0x00000001)
#define XRT_METHOD_FLAG_VIRTUAL		UINT32_C(0x00000002)
#define XRT_METHOD_FLAG_FINAL		UINT32_C(0x00000004)



/*
	所有值操作只处理 Size 字节的 C ABI 值，不得访问 InstanceSize 负载。
	Init 失败时必须释放已经取得的资源并设置 XRT 错误，调用方不会执行 Drop。
	其他类型操作返回 false 时必须保留目标原值并设置 XRT 错误。
	Move 成功后源值处于已初始化的空状态，仍允许 Drop。
	Format 只读值并同步调用 writer；writer 失败后必须立即停止并返回 false。
*/
struct xrttypeops {
	bool (*Init)(ptr pValue, const xrttype* pType);
	bool (*Copy)(ptr pTarget, const void* pSource, const xrttype* pType);
	bool (*Move)(ptr pTarget, ptr pSource, const xrttype* pType);
	void (*Drop)(ptr pValue, const xrttype* pType);
	bool (*Clone)(ptr pTarget, const void* pSource, const xrttype* pType);
	int (*Compare)(
		const void* pLeft,
		const void* pRight,
		const xrttype* pType
	);
	uint64 (*Hash)(const void* pValue, const xrttype* pType);
	bool (*Format)(
		const void* pValue,
		const xrttype* pType,
		xrttypewriter pWrite,
		ptr pContext
	);
	bool (*Trace)(
		const void* pValue,
		const xrttype* pType,
		xrtobjectvisitor pVisit,
		ptr pContext
	);
};



/* 实例操作只处理引用对象的堆负载，不处理 C ABI 中的对象指针值。 */
struct xrtinstanceops {
	bool (*Init)(ptr pInstance, const xrttype* pType);
	void (*Drop)(ptr pInstance, const xrttype* pType);
	bool (*Trace)(
		const void* pInstance,
		const xrttype* pType,
		xrtobjectvisitor pVisit,
		ptr pContext
	);
};



struct xrtparamdesc {
	xstrview Name;
	const xrttype* Type;
	xrtparammode Mode;
	uint32 Flags;
};



struct xrtfunctionsig {
	uint64 Id;
	xstrview Name;
	size_t ParamCount;
	const xrtparamdesc* Params;
	size_t ReturnCount;
	const xrttype* const* ReturnTypes;
	uint32 Flags;
	/* 扩展元数据不参与签名身份，生命周期由描述符所有者管理。 */
	uint64 UserTag;
	ptr UserData;
};



struct xrtmethoddesc {
	xstrview Name;
	const xrtfunctionsig* Signature;
	ptr Entry;
	uint32 Flags;
};



struct xrtmethodtable {
	size_t Count;
	const xrtmethoddesc* Methods;
};



struct xrttype {
	uint64 Id;
	xrttypekind Kind;
	uint32 Flags;
	xstrview Name;
	xstrview AbiName;
	/* Size/Align 描述值在 C ABI 中的存储形态。 */
	size_t Size;
	size_t Align;
	/* InstanceSize/InstanceAlign 描述引用类型在堆上的数据负载。 */
	size_t InstanceSize;
	size_t InstanceAlign;
	/* Ops 处理 Size 字节的 C ABI 值，InstanceOps 处理堆对象负载。 */
	const xrttypeops* Ops;
	const xrtinstanceops* InstanceOps;
	const xrttype* Base;
	size_t ArgumentCount;
	const xrttype* const* Arguments;
	const xrtfieldtable* Fields;
	const xrtmethodtable* Methods;
	const void* Metadata;
};



typedef struct xrtprotocolrequirement {
	xstrview Name;
	const xrtfunctionsig* Signature;
} xrtprotocolrequirement;



struct xrtprotocol {
	const xrttype* Type;
	size_t RequirementCount;
	const xrtprotocolrequirement* Requirements;
};



typedef struct xrtprotocolentry {
	xstrview Name;
	const xrtfunctionsig* Signature;
	ptr Entry;
} xrtprotocolentry;



struct xrtprotocolwitness {
	const xrtprotocol* Protocol;
	const xrttype* ConcreteType;
	size_t EntryCount;
	const xrtprotocolentry* Entries;
};



struct xrtenumvariant {
	xstrview Name;
	int64 Tag;
	const xrttype* PayloadType;
};



struct xrtenum {
	const xrttype* Type;
	size_t VariantCount;
	const xrtenumvariant* Variants;
};



XRT_EXTERN_C_BEGIN



/* 按规范 ABI 名生成稳定的非零类型 ID。 */
XRT_API uint64 xrtTypeId(xstrview AbiName);



/* 按调用形态生成稳定签名 ID；所有非空参数名都参与身份。 */
XRT_API uint64 xrtFunctionSigId(const xrtfunctionsig* pSignature);



/* 检查参数、返回值、标志、名称唯一性和显式签名 ID 是否自洽。 */
XRT_API bool xrtFunctionSigValidate(const xrtfunctionsig* pSignature);



/* 检查类型 ID、结构、生命周期标志和完整继承链是否自洽。 */
XRT_API bool xrtTypeValidate(const xrttype* pType);



/* 类型相等以稳定 ID 和 ABI 名共同判断，指针相同是快速路径。 */
XRT_API bool xrtTypeSame(const xrttype* pLeft, const xrttype* pRight);



/* 判断类型是否等于目标类型或从目标类型派生。 */
XRT_API bool xrtTypeIsA(const xrttype* pType, const xrttype* pTarget);



/* 查询类型值是否支持复制、字节重定位、比较或散列。 */
XRT_API bool xrtTypeIsCopyable(const xrttype* pType);
XRT_API bool xrtTypeIsRelocatable(const xrttype* pType);
XRT_API bool xrtTypeIsComparable(const xrttype* pType);
XRT_API bool xrtTypeIsHashable(const xrttype* pType);



/* 返回指定下标的泛型参数，越界返回空并设置范围错误。 */
XRT_API const xrttype* xrtTypeArgument(const xrttype* pType, size_t iIndex);



/* 沿当前类型和基类查找方法；签名 ID 为零时返回首个同名重载。 */
XRT_API const xrtmethoddesc* xrtTypeFindMethod(
	const xrttype* pType,
	xstrview Name,
	uint64 iSignatureId
);



/* 使用类型操作初始化、复制、移动、销毁或克隆一个 Size 字节值。 */
XRT_API bool xrtTypeInitValue(const xrttype* pType, ptr pValue);
XRT_API bool xrtTypeCopyValue(
	const xrttype* pType,
	ptr pTarget,
	const void* pSource
);
XRT_API bool xrtTypeMoveValue(
	const xrttype* pType,
	ptr pTarget,
	ptr pSource
);
XRT_API void xrtTypeDropValue(const xrttype* pType, ptr pValue);
XRT_API bool xrtTypeCloneValue(
	const xrttype* pType,
	ptr pTarget,
	const void* pSource
);



/* 枚举值直接持有的全部强对象引用；每一个所有权槽位必须访问一次。 */
XRT_API bool xrtTypeTraceValue(
	const xrttype* pType,
	const void* pValue,
	xrtobjectvisitor pVisit,
	ptr pContext
);



/* 初始化、销毁或追踪引用对象的堆实例负载。 */
XRT_API bool xrtTypeInitInstance(const xrttype* pType, ptr pInstance);
XRT_API void xrtTypeDropInstance(const xrttype* pType, ptr pInstance);
XRT_API bool xrtTypeTraceInstance(
	const xrttype* pType,
	const void* pInstance,
	xrtobjectvisitor pVisit,
	ptr pContext
);



/* 使用类型操作比较或散列一个值；成功才写入输出。 */
XRT_API bool xrtTypeCompareValue(
	const xrttype* pType,
	const void* pLeft,
	const void* pRight,
	int* pResult
);
XRT_API bool xrtTypeHashValue(
	const xrttype* pType,
	const void* pValue,
	uint64* pHash
);



/* 注册表只借用唯一且不可变的描述指针；除销毁外允许并发调用。 */
XRT_API xrttyperegistry* xrtTypeRegistryCreate(void);
XRT_API void xrtTypeRegistryDestroy(xrttyperegistry* pRegistry);
XRT_API bool xrtTypeRegistryAdd(
	xrttyperegistry* pRegistry,
	const xrttype* pType
);
XRT_API bool xrtTypeRegistryRemove(
	xrttyperegistry* pRegistry,
	const xrttype* pType
);
XRT_API size_t xrtTypeRegistryCount(const xrttyperegistry* pRegistry);



/* 按稳定类型 ID 顺序返回指定位置的借用描述；并发修改时下标只代表本次调用快照。 */
XRT_API const xrttype* xrtTypeRegistryAt(
	const xrttyperegistry* pRegistry,
	size_t iIndex
);
XRT_API const xrttype* xrtTypeRegistryFindId(
	const xrttyperegistry* pRegistry,
	uint64 iTypeId
);
XRT_API const xrttype* xrtTypeRegistryFindName(
	const xrttyperegistry* pRegistry,
	xstrview AbiName
);



/* 验证协议类型、要求和重载身份完整且唯一。 */
XRT_API bool xrtProtocolValidate(const xrtprotocol* pProtocol);



/* 验证协议见证表完整且每个要求只实现一次。 */
XRT_API bool xrtProtocolWitnessValidate(
	const xrtprotocolwitness* pWitness
);
XRT_API const xrtprotocolentry* xrtProtocolWitnessFind(
	const xrtprotocolwitness* pWitness,
	xstrview Name,
	uint64 iSignatureId
);
XRT_API xrtprotocolregistry* xrtProtocolRegistryCreate(void);
XRT_API void xrtProtocolRegistryDestroy(xrtprotocolregistry* pRegistry);
XRT_API bool xrtProtocolRegistryAdd(
	xrtprotocolregistry* pRegistry,
	const xrtprotocolwitness* pWitness
);
XRT_API bool xrtProtocolRegistryRemove(
	xrtprotocolregistry* pRegistry,
	const xrtprotocolwitness* pWitness
);
XRT_API const xrtprotocolwitness* xrtProtocolRegistryFind(
	const xrtprotocolregistry* pRegistry,
	uint64 iProtocolTypeId,
	uint64 iConcreteTypeId
);
XRT_API size_t xrtProtocolRegistryCount(
	const xrtprotocolregistry* pRegistry
);



/* 按协议类型 ID 和具体类型 ID 顺序返回借用见证；并发修改时下标只代表本次调用快照。 */
XRT_API const xrtprotocolwitness* xrtProtocolRegistryAt(
	const xrtprotocolregistry* pRegistry,
	size_t iIndex
);



/* 验证枚举标签和名称唯一，并查询变体。 */
XRT_API bool xrtEnumValidate(const xrtenum* pEnum);
XRT_API const xrtenumvariant* xrtEnumFindTag(
	const xrtenum* pEnum,
	int64 iTag
);
XRT_API const xrtenumvariant* xrtEnumFindName(
	const xrtenum* pEnum,
	xstrview Name
);



/* XRT 内建标量类型描述在进程期保持稳定。 */
XRT_API const xrttype* xrtTypeNull(void);
XRT_API const xrttype* xrtTypeBool(void);
XRT_API const xrttype* xrtTypeBool32(void);
XRT_API const xrttype* xrtTypeInt8(void);
XRT_API const xrttype* xrtTypeUInt8(void);
XRT_API const xrttype* xrtTypeInt16(void);
XRT_API const xrttype* xrtTypeUInt16(void);
XRT_API const xrttype* xrtTypeInt32(void);
XRT_API const xrttype* xrtTypeUInt32(void);
XRT_API const xrttype* xrtTypeInt64(void);
XRT_API const xrttype* xrtTypeUInt64(void);
XRT_API const xrttype* xrtTypeFloat32(void);
XRT_API const xrttype* xrtTypeFloat64(void);
XRT_API const xrttype* xrtTypeTime(void);
XRT_API const xrttype* xrtTypePointer(void);
XRT_API const xrttype* xrtTypeType(void);



XRT_EXTERN_C_END

#endif

#endif
