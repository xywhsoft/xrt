# 运行时类型

`runtime_type` 提供 C、XRT 与上层语言共同使用的类型事实、值操作、方法签名、
协议见证和枚举元数据。启用宏为 `XRUNTIME_FEATURE_RUNTIME_TYPE`，只依赖 `core`。

该模块不拥有任何描述符。`xrttype`、方法表、签名、协议、见证和枚举表必须在
全部注册与查询期间保持地址稳定且不可修改。

## 类型身份

`xrtTypeId(AbiName)` 根据非空规范 ABI 名生成稳定的非零 ID。自定义类型的 `Id`
必须等于该结果，不能使用随意分配的整数。

```c
xrttype Type = {
	.Id = xrtTypeId(XRT_STR_LITERAL("app.Counter")),
	.Kind = XRT_TYPE_CLASS,
	.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
	.Name = XRT_STR_INIT("Counter"),
	.AbiName = XRT_STR_INIT("app.Counter"),
	.Size = sizeof(ptr),
	.Align = _Alignof(ptr),
	.InstanceSize = sizeof(int64),
	.InstanceAlign = _Alignof(int64)
};

if ( !xrtTypeValidate(&Type) ) {
	return false;
}
```

`xrtTypeSame` 同时比较规范 ID 与 ABI 名，`xrtTypeIsA` 再沿单继承链判断。继承只
用于 `XRT_TYPE_CLASS`，不能继承 `FINAL` 类型，派生实例的大小和对齐不能小于
任一基类。当前继承深度上限为 255 层。

## 描述字段

| 字段 | 契约 |
| --- | --- |
| `Kind` | `xrttypekind` 中的有效类型类别。 |
| `Name` | 非空显示名，可按本地展示要求命名。 |
| `AbiName` | 非空、跨模块唯一且长期稳定的规范名。 |
| `Size` / `Align` | 一个值在 C ABI 中的大小和二次幂对齐。 |
| `InstanceSize` / `InstanceAlign` | 实例负载的大小和二次幂对齐。 |
| `Ops` | 可选的 C ABI 值生命周期、比较、散列、格式化和强引用追踪操作。 |
| `InstanceOps` | 可选的堆实例负载初始化、销毁和强引用追踪操作。 |
| `Base` | 可选类基类。 |
| `Arguments` | 借用的泛型实参描述数组。 |
| `Fields` | 可选的借用字段表；由 `runtime_field` 验证和查询。 |
| `Methods` | 借用的方法描述表。 |
| `Metadata` | 由声明方解释的借用元数据。 |

引用类型必须使用一个指针作为 C ABI 存储，即 `Size == sizeof(ptr)` 且 `Align`
等于指针对齐；对象负载由 `InstanceSize` 单独描述。

`xrttypekind` 只描述稳定的运行时存储类别，不表达语言可见性或语法别名：

| 常量 | 类别 |
| --- | --- |
| `XRT_TYPE_INVALID` | 无效或未初始化描述。 |
| `XRT_TYPE_NULL` | 零尺寸空值。 |
| `XRT_TYPE_BOOL` | 原生或 32 位 ABI 布尔。 |
| `XRT_TYPE_SIGNED_INT` | 定宽有符号整数。 |
| `XRT_TYPE_UNSIGNED_INT` | 定宽无符号整数。 |
| `XRT_TYPE_FLOAT` | IEEE-754 单精度或双精度浮点。 |
| `XRT_TYPE_STRING` | 由具体描述决定所有权的文本。 |
| `XRT_TYPE_BYTES` | 由具体描述决定所有权的字节序列。 |
| `XRT_TYPE_TIME` | `xtime` 时间值。 |
| `XRT_TYPE_POINTER` | 进程内裸指针。 |
| `XRT_TYPE_CALLABLE` | 可调用值。 |
| `XRT_TYPE_ARRAY` | 定长或连续数组。 |
| `XRT_TYPE_LIST` | 顺序列表。 |
| `XRT_TYPE_SET` | 唯一元素集合。 |
| `XRT_TYPE_DICT` | 键值字典。 |
| `XRT_TYPE_RECORD` | 值语义记录。 |
| `XRT_TYPE_HANDLE` | 外部资源句柄。 |
| `XRT_TYPE_TYPE` | 稳定类型 ID 值。 |
| `XRT_TYPE_FUTURE` | 异步结果引用。 |
| `XRT_TYPE_CLASS` | 可继承对象引用。 |
| `XRT_TYPE_ENUM` | 带可选载荷的枚举值。 |
| `XRT_TYPE_PROTOCOL` | 协议类型事实。 |
| `XRT_TYPE_OPTIONAL` | 可选值组合。 |
| `XRT_TYPE_WEAK` | 弱引用值。 |

内建标量类别具有固定可解释布局：`null` 的大小为零，布尔使用 `bool` 或 32 位
ABI 布尔，有符号和
无符号整数只接受 1、2、4、8 字节，浮点只接受 `float` 或 `double`，时间使用
`xtime`，指针使用 `ptr`，类型 ID 使用 `uint64`。不支持的宽度会在类型验证时被
拒绝，能力查询也不会把损坏的标量描述报告为可比较或可散列。

类型标志：

| 标志 | 含义 |
| --- | --- |
| `XRT_TYPE_FLAG_TRIVIAL_COPY` | 可按 `Size` 直接复制 ABI 值；必须同时声明 `COPYABLE`。 |
| `XRT_TYPE_FLAG_TRIVIAL_DROP` | 销毁不需要执行自定义 `Drop`。 |
| `XRT_TYPE_FLAG_COPYABLE` | `xrtTypeCopyValue` 可用，必须存在平凡或自定义复制路径。 |
| `XRT_TYPE_FLAG_REFERENCE` | C ABI 值是对象或句柄指针。 |
| `XRT_TYPE_FLAG_NULLABLE` | 该类型允许空值。 |
| `XRT_TYPE_FLAG_FINAL` | 禁止继续派生。 |
| `XRT_TYPE_FLAG_RELOCATABLE` | 值可在不调用生命周期回调时按 `Size` 字节搬到另一地址。 |

平凡标志不能与同名自定义 `Copy` 或 `Drop` 同时声明。所有未知标志都会被拒绝。
连续存储容器只接受 `RELOCATABLE` 元素；该标志不代表可复制，也不允许产生第二份值，
它只保证原字节搬迁后旧地址不再使用是安全的。XRT 内建标量均可重定位。

`xrtTypeIsCopyable`、`xrtTypeIsRelocatable`、`xrtTypeIsComparable` 和
`xrtTypeIsHashable` 用于容器和宿主绑定在建立类型时预检能力，不设置错误。

字段能力被独立裁剪。`xrtTypeValidate` 不反向依赖字段模块；启用
`XRUNTIME_FEATURE_RUNTIME_FIELD` 并设置 `Fields` 后，类型发布前还应调用
`xrtTypeFieldsValidate`。字段布局、继承顺序和访问契约见 `runtime_field.md`。

## 函数与方法

`xrtFunctionSigId` 根据参数类型、传递模式、参数标志、所有非空参数名、返回类型和
函数标志生成签名 ID。函数显示名不参与身份。非空参数名表示该参数允许按名称绑定，
因此名称是调用 ABI 的一部分且必须唯一；名称为空的参数只能按位置传递。

`xrtFunctionSigValidate` 只做完整验证，不要求调用方同时计算或使用签名 ID。它检查
参数和返回数组、引用类型、传递模式、标志、名称唯一性以及可选的显式 ID。

参数模式为 `XRT_PARAM_DEFAULT`、`XRT_PARAM_BYVAL` 或 `XRT_PARAM_BYREF`。参数可
声明 `OPTIONAL` 和 `NAMED_ONLY`；函数可声明 `VARARGS` 和 `KWARGS`；方法可声明
`STATIC`、`VIRTUAL` 和 `FINAL`。未知标志不会被忽略。

`xrtTypeFindMethod` 沿当前类型和基类查询名称与签名。签名 ID 为零时返回首个
同名重载；没有匹配项是正常结果，不设置错误。`xrtTypeArgument` 返回泛型实参，
越界时设置范围错误。

## 值操作

`xrtTypeInitValue`、`xrtTypeCopyValue`、`xrtTypeMoveValue`、`xrtTypeDropValue` 和
`xrtTypeCloneValue` 统一调用 `xrttypeops`，并且只处理 `Size` 字节的 C ABI 值。
调用者必须传入已经验证且保持不可变的类型描述。

- `Init` 缺省时清零整个 ABI 值；失败回调必须自行释放部分初始化资源并设置错误，调用方不会再执行 `Drop`。
- `Copy` 只接受 `COPYABLE` 类型；自复制成功且不修改值。
- `Move` 成功后源值处于可安全 `Drop` 的空状态；自移动不修改值。
- `Drop` 缺省时为空操作。
- `Clone` 缺省时使用复制路径；源和目标必须不同。
- 失败的 `Copy`、`Move` 或 `Clone` 回调必须保留目标原值并设置 XRT 错误。
- `Copy`、`Clone`、`Compare`、`Hash`、`Format` 和 `Trace` 的输入值是只读的 `const void*`。

`Format` 是可选的底层文本能力。它通过 `xrttypewriter` 同步写出零个或多个借用
UTF-8 分块；分块只在当前 writer 调用期间有效，格式化器和 writer 都不能保存其地址。
writer 返回 `false` 后，格式化器必须立即停止并返回 `false`，不能继续产生输出。
格式化失败应保留 writer 已设置的错误，或者在没有下层错误时自行设置错误。

类型事实层只公开该回调能力，不引入字符串构建依赖。可裁剪的
`runtime_convert_string` 使用它实现 `xrtTypeFormat`、`xrtTypeToString` 和自定义类型到
规范拥有型字符串的显式转换。

`xrtTypeTraceValue` 调用可选的 `Trace` 操作，枚举实例直接拥有的全部 `xrtobject`
强引用。没有 `Trace` 的类型成功返回且不访问对象。追踪必须遵守以下契约：

- 每一个实际拥有强引用的字段或容器槽位访问一次，同一对象被多个槽位拥有时必须重复访问。
- 空槽位不访问，不能把弱引用、借用指针或缓存指针报告为强引用。
- 访问器返回 `false` 时立即停止并返回失败；类型回调不得继续访问。
- 追踪只读负载，不得修改字段、引用计数或对象图。

`Trace` 是类型事实层公开的底层能力，既可供对象图收集器使用，也可供调试器、堆快照
和宿主运行时字段遍历复用。回调失败会包装为 `xrt.type/trace`，其下层错误保留为
原因。

### 实例负载

引用类型同时具有两种不同存储：C ABI 中是 `Size == sizeof(ptr)` 的引用槽，堆上则是
`InstanceSize` 字节的对象负载。二者不能共用生命周期操作。

`xrtTypeInitInstance`、`xrtTypeDropInstance` 和 `xrtTypeTraceInstance` 只调用
`xrtinstanceops`。缺省实例初始化清零 `InstanceSize`，缺省销毁和追踪为空操作。对象创建、
对象终结和对象图收集使用这组 API；字段和泛型容器元素使用值 API。

类引用值可把 `Ops` 设置为 `xrtObjectValueOps()` 并声明 `COPYABLE`。该操作表按标准强引用
语义复制、移动、释放和追踪 `xrtobject*` 槽，同时按进程内对象地址比较与散列。类自身的
构造、析构和成员追踪仍放在 `InstanceOps` 中。

`xrtTypeCompareValue` 和 `xrtTypeHashValue` 优先使用自定义操作。内建 null、布尔、
定宽整数、浮点、时间、指针和 type ID 即使没有 `Ops` 也可直接使用。成功时才
写入输出。

浮点比较采用确定的总序：`+0` 与 `-0` 相等且散列一致，NaN 按符号、载荷位模式
稳定排序。指针按 `uintptr_t` 比较，其顺序和散列只适用于当前进程，不能用于持久化。

```c
int64 Left = 7;
int64 Right = 11;
int Compare;
uint64 Hash;

if ( !xrtTypeCompareValue(xrtTypeInt64(), &Left, &Right, &Compare) ||
	 !xrtTypeHashValue(xrtTypeInt64(), &Left, &Hash) ) {
	return false;
}
```

## 类型注册表

`xrtTypeRegistryCreate` 创建借用型注册表。`xrtTypeRegistryAdd`、
`xrtTypeRegistryRemove`、`xrtTypeRegistryCount`、`xrtTypeRegistryAt`、
`xrtTypeRegistryFindId` 和 `xrtTypeRegistryFindName` 可并发调用；
`xrtTypeRegistryDestroy` 必须与其他操作互斥。

注册表按 ID 排序，ID 与名称查询为 `O(log n)`，插入和移除为 `O(n)`。重复添加
同一描述指针是幂等成功；同一规范身份的第二份描述会返回 `XERR_EXISTS`，防止
注册表借用生命周期变得不明确。移除要求传入注册时的准确指针。查询未命中和
移除未注册描述都返回空或 `false`，不设置错误。

`xrtTypeRegistryAt` 按类型 ID 升序返回借用描述，适合反射、调试器和模块清单。
每次调用只保证读取当时的线程安全快照；并发增删会改变后续下标，调用方若需要
一致遍历，应在外部禁止修改。越界返回 `XERR_RANGE`。

```c
xrttyperegistry* Registry = xrtTypeRegistryCreate();

if ( (Registry == NULL) || !xrtTypeRegistryAdd(Registry, &Type) ) {
	xrtTypeRegistryDestroy(Registry);
	return false;
}
const xrttype* First = xrtTypeRegistryAt(Registry, 0);
const xrttype* Found = xrtTypeRegistryFindName(Registry, Type.AbiName);
bool Removed = xrtTypeRegistryRemove(Registry, &Type);
xrtTypeRegistryDestroy(Registry);
```

## 协议见证

`xrtprotocol` 声明协议类型及方法要求，`xrtprotocolwitness` 把每个要求映射到具体
类型入口。`xrtProtocolValidate` 可在尚未构造见证前独立验证协议类型、要求名称、
签名和重载身份；同名同签名的重复要求会被拒绝。`xrtProtocolWitnessValidate` 再
验证每个 entry 的名称、签名和入口，要求名称和签名同时匹配，并保证每项有且只有
一个非空入口。`xrtProtocolWitnessFind` 用于底层直接查询入口，损坏的 entry 会作为
协议错误返回而不会被解引用。

协议注册表以“协议类型 ID、具体类型 ID”为键。`xrtProtocolRegistryCount` 返回当前
见证数量，`xrtProtocolRegistryFind` 按协议和具体类型查找；线程规则与类型注册表相同。
同一见证指针重复添加是幂等成功；同一键的另一份见证返回 `XERR_EXISTS`；移除
要求准确指针；未命中不设置错误。`xrtProtocolRegistryAt` 按这两个 ID 的升序返回
借用见证，快照与越界语义和 `xrtTypeRegistryAt` 相同。

协议注册表必须由应用或语言运行时显式创建、持有和销毁。类型层不提供隐藏的默认
全局注册表；这保证不同运行时实例、测试环境和动态模块可以隔离生命周期，也使协议
注册能力能够独立裁剪。

```c
xrtprotocol Protocol = { &ProtocolType, 0, NULL };
xrtprotocolwitness Witness = { &Protocol, &Type, 0, NULL };
xrtprotocolregistry* Registry = xrtProtocolRegistryCreate();

if ( !xrtProtocolValidate(&Protocol) ||
	 (Registry == NULL) ||
	 !xrtProtocolRegistryAdd(Registry, &Witness) ) {
	xrtProtocolRegistryDestroy(Registry);
	return false;
}
xrtProtocolRegistryRemove(Registry, &Witness);
xrtProtocolRegistryDestroy(Registry);
```

## 枚举元数据

`xrtenum` 为 `XRT_TYPE_ENUM` 描述唯一的变体名称、`int64` 标签和可选 payload
类型。`xrtEnumValidate` 拒绝空名称、重复名称、重复标签和无效 payload 身份。
`xrtEnumFindTag` 与 `xrtEnumFindName` 线性查询；未命中不设置错误。

payload 只有一个统一的 `PayloadType`。无载荷时为 `NULL`，单值载荷直接指向该值
类型，多字段载荷指向一个 `XRT_TYPE_RECORD`，并由可裁剪的 `runtime_field` 查询
记录字段。枚举层不重复维护字段名称、偏移和类型，避免两套字段模型产生差异。

```c
xrtenumvariant Variants[] = {
	{ XRT_STR_INIT("ok"), 0, xrtTypeInt64() },
	{ XRT_STR_INIT("error"), 1, xrtTypePointer() }
};
xrtenum Enum = { &EnumType, 2, Variants };

if ( !xrtEnumValidate(&Enum) ) {
	return false;
}
```

## 内建类型

以下函数返回进程期稳定、不可修改的借用描述：`xrtTypeNull`、`xrtTypeBool`、
`xrtTypeBool32`、`xrtTypeInt8`、`xrtTypeUInt8`、`xrtTypeInt16`、`xrtTypeUInt16`、`xrtTypeInt32`、
`xrtTypeUInt32`、`xrtTypeInt64`、`xrtTypeUInt64`、`xrtTypeFloat32`、
`xrtTypeFloat64`、`xrtTypeTime`、`xrtTypePointer` 和 `xrtTypeType`。

`xrtTypeBool()` 使用 C `bool` 存储；`xrtTypeBool32()` 使用 `int32` 存储，供表达式
结果和外部语言 ABI 使用。两者身份不同，但都只表达真假语义；32 位输入的任意非零值
均视为真，转换输出规范化为 `1`，比较与散列也按规范真假值执行。

`XRT_TYPE_STRING`、`XRT_TYPE_BYTES`、容器和引用对象没有无条件内建描述。
这些类别的 C ABI 存储与所有权必须由具体模块决定，例如字符串既可能是借用
`xstrview`，也可能是拥有内存的 `str`。把其中任一种冒充全局唯一描述会让复制和
销毁契约产生歧义。声明方应提供规范 `xrttype`；与动态 Value 的转换由
`typed_value` 的内建标量路径或 `xvalueconverter` 承担。

Future 的拥有型消费端引用由可选的 `runtime_type_future` 模块提供，见
[Future 运行时类型](runtime_type_future.md)。基础 `runtime_type` 因此仍不反向依赖并发体系。

拥有型零结尾 `str` 由可选的 `runtime_type_string` 模块提供，见
[String 运行时类型](runtime_type_string.md)。借用视图和带内嵌零的字符串仍应使用不同描述。

## 错误

参数、描述、签名、操作、注册、协议和枚举错误使用稳定域 `xrt.type`：

| 代码 | 常量 |
| --- | --- |
| 1 | `XTYPE_ERROR_DESCRIPTOR` |
| 2 | `XTYPE_ERROR_SIGNATURE` |
| 3 | `XTYPE_ERROR_OPERATION` |
| 4 | `XTYPE_ERROR_REGISTRY` |
| 5 | `XTYPE_ERROR_PROTOCOL` |
| 6 | `XTYPE_ERROR_ENUM` |

内存分配失败保留 `XERR_MEMORY`。正常的“未找到”和“尚未注册”结果不会覆盖当前
执行上下文中的错误；需要区分旧错误时，应在操作前调用 `xrtClearError`。

## 历史资产

旧版 `lib/type.h` 中有价值的固定宽度类型、生命周期、比较、散列、协议和枚举
概念保留在本模块；字符串转换、装箱、动态值容器和调用器分别归入其对应模块，
不再让类型事实层承担无关职责。旧版 `test/test_runtime_type.h` 的类型宽度、边界和
行为断言已经转化为普通、OOM、并发、示例和单头测试。
