# Value

`value.h` 提供面向通用 C 程序、FFI 和动态宿主的值模型。新实现把数据值与运行时对象分开：基础层只包含安全标量和显式句柄，容器层复用 Array、IntMap、Map、Set，并通过独立外壳和共享 backing 实现写时复制。

## 裁剪与依赖

```text
value -> hash64 -> core
value_container -> value + ptr_array + int_map + map + set
value_collection -> value_container
value_graph -> value_container
```

```c
#define XRT_FEATURE_VALUE
#define XRT_FEATURE_VALUE_CONTAINER
#define XRT_FEATURE_VALUE_COLLECTION
#define XRT_FEATURE_VALUE_GRAPH
```

只需要标量、字符串或 native 句柄时不启用 `VALUE_CONTAINER`。只需要基础增删改查时不启用 `VALUE_COLLECTION`；数组批量操作、映射合并和 Set 代数可以独立裁掉。

## 核心契约

对象终结职责由最后一次 backing 原子释放唯一取得，而非各外壳释放前的引用数快照。
带终结器的身份对象被公开迭代器引用时，迭代器同时拥有 backing 和真实来源外壳；
`End` 先清空游标状态、释放 backing 槽，再释放外壳槽，保证最后一次终结获得真实
receiver。普通无终结器 COW 容器仍只拥有 backing，不改变源外壳的弱引用寿命。
拥有图按这两个实际槽分别计边，内部检查用的临时视图不得冒充拥有外壳。

`xrtValueObjectFinalizerBindOwned(object, finalize, context, trace, release)` 在唯一外壳和
backing 上原子接管上下文，不分配；失败不更改对象、不消费上下文、不调用任何回调。
非空上下文要求显式 Trace。Finalize 先借用对象和字段，全部 backing 字段释放后才
调用 Release 一次。Finalize 内可枚举字段；其新建游标若继续持有 backing，字段及
上下文释放也随之延后，但不会重新运行 Finalize。运行中的终结拒绝图检查。
Release 若释放最后一份代码租约，Release 本身须常驻；XRT 不会自动保活任意回调代码。
旧 `FinalizerBind` 的回调仍自行管理其上下文，不转成新接口的拥有式接管合同。

- `xvalue` 是不透明结构，调用方不得依赖内部布局。
- `NULL` 表示失败或缺失，语言 null 使用 `xrtValueNull()` 单例表示。
- null 和 bool 是不可变单例；其他值使用原子外壳引用计数。
- 字符串与字节默认复制；`StringTake` 和 `BytesTake` 接管 XRT 分配的内存。
- `Take` 的来源指针槽必须独立存在，不能位于准备接管的字符串、字节或句柄内存中。
- 裸指针不拥有目标；句柄必须提供释放器，策略描述必须比值活得更久。
- 句柄 `Hash` 与 `Equal` 必须同时提供或同时省略；两者必须满足相等值哈希一致。
- 句柄克隆器失败时必须设置错误，并且不得在输出中遗留资源；未提供克隆器的句柄深拷贝报告 `XERR_UNSUPPORTED`。
- Getter 和 Hash 失败时不改输出；输出区间不得覆盖 Value 外壳或其拥有的字符串、字节、句柄首地址。
- 句柄 `Hash`、`Equal`、`Clone` 和最终释放回调不得读取、保留或再次释放正在参与回调的 Value；这些重入以 `XERR_STATE` 拒绝。
- 标量不可变。`Retain` 共享同一身份，`Clone` 对标量等价于 `Retain`。
- 非静态值可在发布前用 `xrtValueTypeIdBind` 一次性绑定调用者定义的非零语义类型身份；重复绑定同一值成功，冲突绑定报告 `XERR_STATE`。该身份随浅克隆和深克隆传播，但不改变 XRT 的类别、相等、哈希或序列化语义。
- 精确 Getter 不做文本解析或隐式类型转换，类型错误报告 `XERR_TYPE`。
- `xrtValueHash` 只接受可哈希标量，并与数值相等规则保持一致；Pointer 和 Handle 的哈希仅在当前进程内有效，不可持久化或跨进程比较。
- `xrtValueScalarEqual` 比较标量内容；有符号整数、无符号整数与可无损转换的浮点数按精确数值等价，所有 NaN 互相等价。

`xrtValueTruthy` 使用稳定的动态值真值口径：null、false、数值零、空字符串、空字节和空容器为 false；其他值为 true。Time、Pointer 和 Handle 表示已经存在的值对象，因此即使其内部数值或地址为零也为 true。

## 类型

`xvaluetype` 是稳定的动态值类别，不等同于可扩展的 `xrttype` 运行时类型描述：

| 常量 | 含义 |
| --- | --- |
| `XVALUE_INVALID` | 非法类型，只用于报告空指针或无效输入。 |
| `XVALUE_NULL` | 语言级 null 单例。 |
| `XVALUE_BOOL` | 布尔单例。 |
| `XVALUE_INT` | 有符号 64 位整数。 |
| `XVALUE_FLOAT` | 双精度浮点数。 |
| `XVALUE_STRING` | 允许内嵌零、带额外末尾零的字节字符串。 |
| `XVALUE_BYTES` | 不附加文本语义的二进制块。 |
| `XVALUE_TIME` | Unix Epoch 微秒时间。 |
| `XVALUE_POINTER` | 不拥有目标的裸指针。 |
| `XVALUE_HANDLE` | 由 `xvaluehandleops` 管理的原生句柄。 |
| `XVALUE_ARRAY` | 0 基稠密数组。 |
| `XVALUE_INT_MAP` | `int64` 键稀疏映射。 |
| `XVALUE_SET` | 保持首次插入顺序的标量集合。 |
| `XVALUE_OBJECT` | 保持首次插入顺序的字符串键对象。 |
| `XVALUE_UINT` | 无符号 64 位整数。 |

### `xvaluetype`

动态值类型保持紧凑稳定，语言运行时类型在独立模块扩展。

```c
typedef enum xvaluetype {
	XVALUE_INVALID = -1,
	XVALUE_NULL = 0,
	XVALUE_BOOL,
	XVALUE_INT,
	XVALUE_FLOAT,
	XVALUE_STRING,
	XVALUE_BYTES,
	XVALUE_TIME,
	XVALUE_POINTER,
	XVALUE_HANDLE,
	XVALUE_ARRAY,
	XVALUE_INT_MAP,
	XVALUE_SET,
	XVALUE_OBJECT,
	XVALUE_UINT
} xvaluetype;
```

| 值 | 语义 |
|---|---|
| `XVALUE_INVALID` | 无效 |
| `XVALUE_NULL` | 空值 |
| `XVALUE_BOOL` | 布尔 |
| `XVALUE_INT` | 有符号整数 |
| `XVALUE_FLOAT` | 浮点 |
| `XVALUE_STRING` | 字符串 |
| `XVALUE_BYTES` | BYTES |
| `XVALUE_TIME` | 时间 |
| `XVALUE_POINTER` | POINTER |
| `XVALUE_HANDLE` | HANDLE |
| `XVALUE_ARRAY` | 数组形态 |
| `XVALUE_INT_MAP` | 有符号整数映射形态 |
| `XVALUE_SET` | 集合形态 |
| `XVALUE_OBJECT` | 对象形态 |
| `XVALUE_UINT` | 无符号整数 |

### `xvaluehandleops`

句柄策略是静态不可变描述，其生命周期必须覆盖全部关联值。

```c
typedef struct xvaluehandleops {
	xvaluehandleclone Clone;
	xvaluehandledrop Drop;
	xvaluehandlehash Hash;
	xvaluehandleequal Equal;
} xvaluehandleops;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Clone` | `xvaluehandleclone` | Clone |
| `Drop` | `xvaluehandledrop` | Drop |
| `Hash` | `xvaluehandlehash` | Hash |
| `Equal` | `xvaluehandleequal` | Equal |

### `xvaluekeytype`

通用迭代键区分数组索引、稀疏整数键、对象字符串键和无键集合。

```c
typedef enum xvaluekeytype {
	XVALUE_KEY_NONE = 0,
	XVALUE_KEY_INDEX,
	XVALUE_KEY_INT,
	XVALUE_KEY_STRING
} xvaluekeytype;
```

| 值 | 语义 |
|---|---|
| `XVALUE_KEY_NONE` | 无 |
| `XVALUE_KEY_INDEX` | 索引 |
| `XVALUE_KEY_INT` | 有符号整数 |
| `XVALUE_KEY_STRING` | 字符串键 |

### `xvalueiterresult`

三态推进结果显式区分元素、正常结束和迭代错误。

```c
typedef enum xvalueiterresult {
	XVALUE_ITER_ERROR = -1,
	XVALUE_ITER_END = 0,
	XVALUE_ITER_ITEM = 1
} xvalueiterresult;
```

| 值 | 语义 |
|---|---|
| `XVALUE_ITER_ERROR` | 失败 |
| `XVALUE_ITER_END` | END |
| `XVALUE_ITER_ITEM` | 已产出 |

### `xvaluemergepolicy`

映射批量合并时对已有键采用明确且互斥的处理策略。

```c
typedef enum xvaluemergepolicy {
	XVALUE_MERGE_KEEP = 0,
	XVALUE_MERGE_REPLACE,
	XVALUE_MERGE_ERROR
} xvaluemergepolicy;
```

| 值 | 语义 |
|---|---|
| `XVALUE_MERGE_KEEP` | KEEP |
| `XVALUE_MERGE_REPLACE` | REPLACE |
| `XVALUE_MERGE_ERROR` | 失败 |

### `xvalue`

动态值结构保持不透明，所有权通过 Retain、Release 和 Take 系列表达。

```c
typedef struct xvalue xvalue;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xvalueidentityhash`

语义值哈希器只借用已经绑定 TypeId 的容器值。回调可以通过只读 Value API 观察该值及其字段，也可以递归哈希字段，但不得修改、保留或释放输入值。

```c
typedef uint64 (*xvalueidentityhash)(const xvalue* pValue, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xvalueidentityequal`

语义值相等器只借用同一 TypeId 和同一策略域中的两个容器值。回调可以 递归比较字段，但不得修改、保留或释放任一输入值。

```c
typedef bool (*xvalueidentityequal)(
	const xvalue* pLeft,
	const xvalue* pRight,
	ptr pUserData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xvaluehandleclone`

句柄克隆器创建独立句柄；失败时必须设置错误且不得在输出中遗留资源。

```c
typedef bool (*xvaluehandleclone)(ptr pHandle, ptr* pClone, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xvaluehandledrop`

句柄释放器销毁 Value 独占的一个句柄。

```c
typedef void (*xvaluehandledrop)(ptr pHandle, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xvaluehandlehash`

句柄哈希器必须与相等器成对提供、保持一致且不得重入父 Value。

```c
typedef uint64 (*xvaluehandlehash)(ptr pHandle, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xvaluehandleequal`

句柄相等器只借用两个句柄，必须与哈希器成对提供且不得重入父 Value。

```c
typedef bool (*xvaluehandleequal)(ptr pLeft, ptr pRight, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xvalueobjectfinalizer`

Object finalizers borrow the last live object shell before its owned fields are released.  The callback may inspect or mutate fields, but it must not retain, clone or release the borrowed object itself.  A finalizer is attached to the shared object backing and therefore runs exactly once, when the final backing owner is released.

```c
typedef void (*xvalueobjectfinalizer)(xvalue* pObject, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

## 标量

```c
xvalue* xrtValueNull(void);
xvalue* xrtValueBool(bool value);
xvalue* xrtValueInt(int64 value);
xvalue* xrtValueUInt(uint64 value);
xvalue* xrtValueFloat(double value);
xvalue* xrtValueString(xstrview text);
xvalue* xrtValueStringTake(str* text, size_t size);
xvalue* xrtValueBytes(xbytesview data);
xvalue* xrtValueBytesTake(bytes* data, size_t size);
xvalue* xrtValueTime(xtime time);
xvalue* xrtValuePointer(ptr pointer);

xvalue* xrtValueRetain(const xvalue* value);
void xrtValueRelease(xvalue* value);
xvalue* xrtValueClone(const xvalue* value);
xvaluetype xrtValueType(const xvalue* value);
uint64 xrtValueTypeId(const xvalue* value);
bool xrtValueTypeIdBind(xvalue* value, uint64 type_id);
bool xrtValueTypeIdRebind(xvalue* value, uint64 type_id);
cstr xrtValueTypeName(xvaluetype type);
bool xrtValueIs(const xvalue* value, xvaluetype type);
bool xrtValueIsNumber(const xvalue* value);
bool xrtValueIsContainer(const xvalue* value);
bool xrtValueTruthy(const xvalue* value);

bool xrtValueGetBool(const xvalue* value, bool* output);
bool xrtValueGetInt(const xvalue* value, int64* output);
bool xrtValueGetUInt(const xvalue* value, uint64* output);
bool xrtValueGetFloat(const xvalue* value, double* output);
bool xrtValueGetString(const xvalue* value, xstrview* output);
bool xrtValueGetBytes(const xvalue* value, xbytesview* output);
bool xrtValueGetTime(const xvalue* value, xtime* output);
bool xrtValueGetPointer(const xvalue* value, ptr* output);

bool xrtValueHash(const xvalue* value, uint64* output);
bool xrtValueScalarEqual(const xvalue* left, const xvalue* right);
```

字符串视图允许内嵌零，并额外保证 `Data[Size]` 是零。二进制视图只承诺长度内字节有效。

`StringTake` 会把缓冲调整到 `size + 1` 并写入末尾零，失败时来源指针和内容不变。
`BytesTake` 不增加隐藏终止字节。两者只接受由 XRT 分配器取得的内存；成功后来源
指针被清空，Value 在最后释放时销毁该内存。

`xrtValueTypeId` 对未绑定值和空指针返回零。`xrtValueTypeIdBind` 只接受非静态值与
非零身份；调用方必须在把值外壳发布给其他线程之前完成首次绑定。类型身份属于宿主
语义元数据，不会被 XRT 当作结构内容；COW 外壳克隆和 Value Graph 深克隆会保留它。
`xrtValueTypeIdRebind` 仅供已经完成语义验证的唯一拥有外壳替换身份；冲突的共享外壳
以 `XERR_STATE` 拒绝，调用方需要先创建独立 COW 外壳，不能重新解释其他持有者的值。

### `xrtValueNull`

返回进程期不可变的 null 单例。

```c
xvalue* xrtValueNull(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 进程期单例，不必释放 | — |

#### 错误

- 无 — 单例不失败

#### 范例

[basic](../../examples/value/basic/main.c) · null 单例

```c
		(xrtValueType(xrtValueNull()) != XVALUE_NULL)
```

### `xrtValueBool`

返回进程期不可变的布尔单例。

```c
xvalue* xrtValueBool(bool bValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `bValue` | 输入 | — | 布尔值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 进程期单例，不必释放 | — |

#### 错误

- 无 — 单例不失败

#### 范例

[basic](../../examples/value/basic/main.c) · 布尔单例

```c
	xvalue* pTrue = xrtValueBool(true);
```

### `xrtValueInt`

创建不可变的 64 位整数值。

```c
xvalue* xrtValueInt(int64 iValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iValue` | 输入 | — | 整数值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- 无 — 标量创建不失败

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 整数

```c
		xvalue* pA = xrtValueInt(1);
```

### `xrtValueUInt`

创建不可变的 64 位无符号整数值。

```c
xvalue* xrtValueUInt(uint64 iValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iValue` | 输入 | — | 无符号值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- 无 — 标量创建不失败

#### 范例

[object_tour](../../examples/value/object_tour/main.c) · 无符号整数

```c
		xvalue* pU = xrtValueUInt(UINT64_C(4294967296));
```

### `xrtValueFloat`

创建不可变的双精度浮点值。

```c
xvalue* xrtValueFloat(double fValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | — | 浮点值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- 无 — 标量创建不失败

#### 范例

[basic](../../examples/value/basic/main.c) · 浮点

```c
		xrtValueFloat(2.0),
```

### `xrtValueString`

复制字节并创建带末尾零但允许内嵌零的字符串值。

```c
xvalue* xrtValueString(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 文本视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 字符串

```c
	(void)xrtValueArrayAppendNew(pArray, xrtValueString(SV("a")));
```

### `xrtValueStringTake`

接管 XRT 字符串并清空独立来源槽；来源槽不得位于被接管内存中。

```c
xvalue* xrtValueStringTake(str* pText, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pText` | 输入/输出 | 非空、独立来源槽 | 拥有的字符串 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 所有权或快照状态非法
- `XERR_MEMORY` — 分配失败

#### 范例

[ownership](../../examples/value/ownership/main.c) · 接管字符串

```c
	pText = xrtValueStringTake(&sText, 5);
```

### `xrtValueBytes`

复制任意字节并创建二进制值。

```c
xvalue* xrtValueBytes(xbytesview Data)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | 借用 | 二进制视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[basic](../../examples/value/basic/main.c) · 二进制

```c
		xrtValueBytes((xbytesview){ arrBytes, sizeof(arrBytes) }),
```

### `xrtValueBytesTake`

接管 XRT 二进制块并清空独立来源槽；来源槽不得位于被接管内存中。

```c
xvalue* xrtValueBytesTake(bytes* pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入/输出 | 非空、独立来源槽 | 拥有的字节块 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 所有权或快照状态非法
- `XERR_MEMORY` — 分配失败

#### 范例

[ownership](../../examples/value/ownership/main.c) · 接管二进制

```c
	pBytes = xrtValueBytesTake(&pData, 3);
```

### `xrtValueTime`

创建使用 Unix Epoch 微秒表示的时间值。

```c
xvalue* xrtValueTime(xtime Time)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Time` | 输入 | — | Unix 微秒 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- 无 — 标量创建不失败

#### 范例

[basic](../../examples/value/basic/main.c) · 时间

```c
		xrtValueTime((xtime)1234567),
```

### `xrtValuePointer`

创建不拥有目标生命周期的裸指针值。

```c
xvalue* xrtValuePointer(ptr pPointer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPointer` | 输入 | — | 指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- 无 — 标量创建不失败

#### 范例

[basic](../../examples/value/basic/main.c) · 裸指针

```c
		xrtValuePointer(&iMarker)
```

### `xrtValueGetBool`

精确读取布尔值，类型不匹配时失败。

```c
bool xrtValueGetBool(const xvalue* pValue, bool* pResult)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、BOOL | 源值 |
| `pResult` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[basic](../../examples/value/basic/main.c) · 读布尔

```c
		!xrtValueGetBool(pTrue, &bTrue) ||
```

### `xrtValueGetInt`

精确读取整数值，类型不匹配时失败。

```c
bool xrtValueGetInt(const xvalue* pValue, int64* pResult)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、INT | 源值 |
| `pResult` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[basic](../../examples/value/basic/main.c) · 读整数

```c
		!xrtValueGetInt(arrValues[0], &iVersion) ||
```

### `xrtValueGetUInt`

精确读取无符号整数值，类型不匹配时失败。

```c
bool xrtValueGetUInt(const xvalue* pValue, uint64* pResult)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、UINT | 源值 |
| `pResult` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[object_tour](../../examples/value/object_tour/main.c) · 读无符号

```c
		(void)xrtValueGetUInt(pU, &uValue);
```

### `xrtValueGetFloat`

精确读取浮点值，类型不匹配时失败。

```c
bool xrtValueGetFloat(const xvalue* pValue, double* pResult)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、FLOAT | 源值 |
| `pResult` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[basic](../../examples/value/basic/main.c) · 读浮点

```c
		!xrtValueGetFloat(arrValues[1], &fVersion) ||
```

### `xrtValueGetString`

借用字符串视图，值释放后视图失效。

```c
bool xrtValueGetString(const xvalue* pValue, xstrview* pResult)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、STRING | 源值 |
| `pResult` | 输出 | 非空 | 接收视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 读字符串

```c
		(void)xrtValueGetString(xrtValueArrayGet(pArray, i), &Text);
```

### `xrtValueGetBytes`

借用二进制视图，值释放后视图失效。

```c
bool xrtValueGetBytes(const xvalue* pValue, xbytesview* pResult)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、BYTES | 源值 |
| `pResult` | 输出 | 非空 | 接收视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[basic](../../examples/value/basic/main.c) · 读二进制

```c
		!xrtValueGetBytes(arrValues[3], &Data) ||
```

### `xrtValueGetTime`

精确读取时间值。

```c
bool xrtValueGetTime(const xvalue* pValue, xtime* pResult)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、TIME | 源值 |
| `pResult` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[basic](../../examples/value/basic/main.c) · 读时间

```c
		!xrtValueGetTime(arrValues[4], &Time) ||
```

### `xrtValueGetPointer`

精确读取不拥有目标的裸指针。

```c
bool xrtValueGetPointer(const xvalue* pValue, ptr* pResult)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、POINTER | 源值 |
| `pResult` | 输出 | 非空 | 接收指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[basic](../../examples/value/basic/main.c) · 读指针

```c
		!xrtValueGetPointer(arrValues[5], &pPointer) ||
```

### `xrtValueTruthy`

按 xlang 语义返回值的真值。

```c
bool xrtValueTruthy(const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 源值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 真值 | — |
| `false` | 非真值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[basic](../../examples/value/basic/main.c) · 真值

```c
		!xrtValueTruthy(arrValues[2]) ||
```

## Native Handle

```c
typedef struct xvaluehandleops {
	xvaluehandleclone Clone;
	xvaluehandledrop Drop;
	xvaluehandlehash Hash;
	xvaluehandleequal Equal;
} xvaluehandleops;

xvalue* xrtValueHandleTake(
	ptr* handle,
	const xvaluehandleops* ops,
	ptr user_data
);
bool xrtValueGetHandle(
	const xvalue* value,
	ptr* handle,
	const xvaluehandleops** ops,
	ptr* user_data
);
```

`Drop` 必须提供，空句柄也会按同一策略进入 `Drop`。`Clone` 只供 Value Graph 深克隆
使用；普通 `xrtValueClone` 只增加不可变句柄 Value 的外壳引用。`Hash` 和 `Equal`
可以同时省略，此时句柄不能加入 Value Set，也不能与另一个句柄执行标量或结构
内容比较。直接调用 Hash/Equal 时，正在参与策略回调的 Handle Value 都进入忙状态。

策略结构是不可变静态描述；`user_data` 与策略生命周期都必须覆盖关联的全部 Value。
只读句柄 Value 可跨线程发布，因此 Hash、Equal 和 Clone 回调若可能并发执行，回调
自身必须线程安全。完整用法见 `examples/value/handle/main.c`。

### `xrtValueHandleTake`

接管句柄并清空独立来源槽；Hash 和 Equal 必须同时提供或同时省略。

```c
xvalue* xrtValueHandleTake(ptr* pHandle, const xvaluehandleops* pOps, ptr pUserData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandle` | 输入/输出 | 非空、独立来源槽 | 拥有的句柄 |
| `pOps` | 输入 | 同时提供或同时为空 | 句柄策略 |
| `pUserData` | 输入 | 任意值 | 策略数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_VALUE` — Hash 与 Equal 只提供一个
- `XERR_STATE` — 所有权或快照状态非法

#### 范例

[containers_lifo](../../examples/value/containers/lifo/main.c) · 接管句柄

```c
	pValue = xrtValueHandleTake(&pHandle, &tOps, NULL);
```

### `xrtValueGetHandle`

借用句柄及其策略数据。

```c
bool xrtValueGetHandle(const xvalue* pValue, ptr* pHandle, const xvaluehandleops** pOps, ptr* pUserData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、HANDLE | 源值 |
| `pHandle` | 输出 | 允许空 | 接收句柄 |
| `pOps` | 输出 | 允许空 | 接收策略表 |
| `pUserData` | 输出 | 允许空 | 接收策略数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[handle](../../examples/value/handle/main.c) · 借用句柄

```c
		!xrtValueGetHandle(pLeft, &pReadHandle, &pReadOps, NULL) ||
```

### `xrtValueTakeHandle`

取走句柄资源并把所有共享外壳可见的资源状态清空；策略仍保留到值释放。

```c
bool xrtValueTakeHandle(xvalue* pValue, ptr* pHandle)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、HANDLE | 源值 |
| `pHandle` | 输出 | 非空 | 接收句柄 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 句柄已被取走

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 取走句柄

```c
			xrtValueTakeHandle(pHandleValue, (ptr*)&pBack) &&
```

## 容器与所有权

```c
xvalue* xrtValueArray(void);
xvalue* xrtValueIntMap(void);
xvalue* xrtValueSet(void);
xvalue* xrtValueObject(void);
xvalue* xrtValueObjectLifo(void);

size_t xrtValueCount(const xvalue* value);
size_t xrtValueCapacity(const xvalue* value);
bool xrtValueReserve(xvalue* value, size_t capacity);
bool xrtValueTrim(xvalue* value);
size_t xrtValueIntMapTrim(xvalue* value, size_t retain_empty);
bool xrtValueClear(xvalue* value);
```

`Capacity` 和 `Reserve` 适用于 Array、Set 和 Object。树形 IntMap 没有可承诺的
连续容量，调用二者报告 `XERR_UNSUPPORTED`；`Trim` 和 `Clear` 支持四种容器。
需要取得 IntMap 实际释放的空闲页数时使用 `xrtValueIntMapTrim`，并可显式保留
指定数量的空闲节点。

`xrtValueObjectLifo` 创建与普通 Object 完全相同的字符串键对象，但在 `Clear` 或
最后一个共享 backing 释放时，按键的当前插入顺序逆序释放仍由对象拥有的值。它用于
类字段、资源作用域等必须按构造逆序析构的场景。读取、`ObjectAt`、迭代和序列化顺序
仍是正向首次插入顺序；替换或删除会立即释放旧值，`Take` 会立即移交值。替换不改变
键位置，删除后重新插入则作为新的尾部键。`Clone`、写时复制、深克隆和以该对象为
目标的失败原子批量操作始终保留该析构策略。

每个写入操作有三种所有权手感：

- `Append/Set/Add`：成功时增加传入值引用。
- `...Take`：只在成功时移交并清空来源指针。
- `...New`：无论成功失败都消费临时值，适合 `xrtValueObjectSetNew(obj, key, xrtValueInt(200))`。

容器 `Take` 的来源槽必须是独立 `xvalue*` 变量，不能覆盖目标或来源 Value 外壳。
失败保持来源槽不变；`New` 明确放弃这一回滚能力并始终消费临时值。

同一个值覆盖同一个 Array 槽、IntMap 键或 Object 键是成功的无操作：借用写入不会增加净引用，`Take` 写入会消费调用方移交的那一个额外引用。失败的 `Take` 保持来源指针不变。

### Array

```c
bool xrtValueArrayResolve(
	const xvalue* array,
	int64 index,
	size_t* resolved
);
xvalue* xrtValueArrayGet(const xvalue* array, size_t index);
xvalue* xrtValueArrayAt(const xvalue* array, int64 index);
xvalue* xrtValueArrayEdit(xvalue* array, size_t index);

bool xrtValueArrayAppend(xvalue* array, const xvalue* item);
bool xrtValueArrayAppendTake(xvalue* array, xvalue** item);
bool xrtValueArrayAppendNew(xvalue* array, xvalue* item);
bool xrtValueArrayInsert(xvalue* array, size_t index, const xvalue* item);
bool xrtValueArrayInsertTake(xvalue* array, size_t index, xvalue** item);
bool xrtValueArrayInsertNew(xvalue* array, size_t index, xvalue* item);
bool xrtValueArraySet(xvalue* array, size_t index, const xvalue* item);
bool xrtValueArraySetTake(xvalue* array, size_t index, xvalue** item);
bool xrtValueArraySetNew(xvalue* array, size_t index, xvalue* item);

bool xrtValueArrayRemove(xvalue* array, size_t index, size_t count);
xvalue* xrtValueArrayTake(xvalue* array, size_t index);
xvalue* xrtValueArrayPop(xvalue* array);
bool xrtValueArraySwap(xvalue* array, size_t left, size_t right);
```

数组使用 0 基 `size_t` 位置。`ArrayAt` 支持 `-1` 表示最后一项；需要在写入、删除或交换中使用负索引时，先以 `ArrayResolve` 转成 0 基位置。解析只接受现有元素，越界报告 `XERR_RANGE`，失败时不修改输出。

`ArrayEdit` 只接受子容器。子项是标量时报告 `XERR_TYPE`，不会先分离父 backing；子容器仅由当前父槽持有时直接返回，存在其他引用时才克隆子外壳。

### IntMap 与 Object

```c
xvalue* xrtValueIntMapGet(const xvalue* map, int64 key);
xvalue* xrtValueIntMapEdit(xvalue* map, int64 key);
bool xrtValueIntMapSet(xvalue* map, int64 key, const xvalue* item);
bool xrtValueIntMapSetTake(xvalue* map, int64 key, xvalue** item);
bool xrtValueIntMapSetNew(xvalue* map, int64 key, xvalue* item);
bool xrtValueIntMapHas(const xvalue* map, int64 key);
bool xrtValueIntMapRemove(xvalue* map, int64 key);
xvalue* xrtValueIntMapTake(xvalue* map, int64 key);

xvalue* xrtValueObjectGet(const xvalue* object, xstrview key);
xvalue* xrtValueObjectEdit(xvalue* object, xstrview key);
xvalue* xrtValueObjectAt(
	const xvalue* object,
	size_t index,
	xstrview* key
);
bool xrtValueObjectSet(
	xvalue* object,
	xstrview key,
	const xvalue* item
);
bool xrtValueObjectSetTake(
	xvalue* object,
	xstrview key,
	xvalue** item
);
bool xrtValueObjectSetNew(
	xvalue* object,
	xstrview key,
	xvalue* item
);
bool xrtValueObjectHas(const xvalue* object, xstrview key);
bool xrtValueObjectRemove(xvalue* object, xstrview key);
xvalue* xrtValueObjectTake(xvalue* object, xstrview key);
```

IntMap 的负数是普通 `int64` 键，并按整数升序迭代。Object 键按完整字节匹配，允许内嵌零，并保持键第一次插入的位置；替换值不改变顺序。`Get`/`Has`/`Remove`/`Take` 的键缺失是正常结果，不设置“缺失”错误。两类 `Edit` 与 Array 一样只返回可变子容器。

### Set

```c
bool xrtValueSetAdd(xvalue* set, const xvalue* item);
bool xrtValueSetAddTake(xvalue* set, xvalue** item);
bool xrtValueSetAddNew(xvalue* set, xvalue* item);
bool xrtValueSetHas(const xvalue* set, const xvalue* item);
bool xrtValueSetRemove(xvalue* set, const xvalue* item);
xvalue* xrtValueSetTake(xvalue* set, const xvalue* item);
```

Set 只接受可哈希不可变标量。有符号整数 `42`、无符号整数 `42` 与浮点数 `42.0` 等价且哈希一致；超过 `INT64_MAX` 的无符号整数仍保持完整精度。重复加入保持原规范值和首次插入顺序。`SetTake` 返回集合中实际保存的规范值，不一定是查询指针。

### `xrtValueRetain`

增加值外壳引用并返回原指针。

```c
xvalue* xrtValueRetain(const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 目标值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[ownership](../../examples/value/ownership/main.c) · 外壳引用

```c
	pRetained = xrtValueRetain(pText);
```

### `xrtValueRelease`

释放值外壳引用，允许传入空指针。

```c
void xrtValueRelease(xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 释放引用

```c
		xrtValueRelease(pOwned);
```

### `xrtValueClone`

标量增加引用，容器创建共享 backing 的独立 COW 外壳。

```c
xvalue* xrtValueClone(const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 源值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[collections](../../examples/value/collections/main.c) · 浅克隆

```c
	pMerged = xrtValueClone(pLeft);
```

### `xrtValueDeepClone`

递归复制全部可克隆内容。

```c
xvalue* xrtValueDeepClone(const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 源值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_UNSUPPORTED` — 含不可克隆的句柄值
- `XERR_MEMORY` — 分配失败

#### 范例

[graph](../../examples/value/graph/main.c) · 深克隆

```c
	pCopy = xrtValueDeepClone(pRoot);
```

### `xrtValueClear`

清空容器并释放其中持有的全部值引用。

```c
bool xrtValueClear(xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入/输出 | 非空、容器 | 目标值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 清空容器

```c
	xrtValueClear(pArray);
```

### `xrtValueReserve`

保证容器至少可容纳指定数量的元素。

```c
bool xrtValueReserve(xvalue* pValue, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入/输出 | 非空、容器 | 目标值 |
| `iCapacity` | 输入 | — | 期望容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量溢出
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 预留容量

```c
	(void)xrtValueReserve(pArray, 100u);
```

### `xrtValueTrim`

释放容器多余容量，保留现有元素。

```c
bool xrtValueTrim(xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入/输出 | 非空、容器 | 目标值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 收缩容量

```c
	(void)xrtValueTrim(pArray);
```

### `xrtValueCount`

返回任一基础容器的元素数。

```c
size_t xrtValueCount(const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 目标值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 元素数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 元素数

```c
	for ( size_t i = 0; i < xrtValueCount(pArray); i++ ) {
```

### `xrtValueCapacity`

返回 Array、Set 或 Object 的当前预留容量；IntMap 不承诺连续容量。

```c
size_t xrtValueCapacity(const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 目标值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 当前容量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 容量查询

```c
	printf("reserved-cap>=%zu", xrtValueCapacity(pArray));
```

### `xrtValueArray`

创建空的稠密动态值数组。

```c
xvalue* xrtValueArray(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 创建数组

```c
	xvalue* pArray = xrtValueArray();
```

### `xrtValueArrayResolve`

把现有数组元素的正负索引解析为 0 基位置，失败时保持输出不变。

```c
bool xrtValueArrayResolve(const xvalue* pArray, int64 iIndex, size_t* pResolved)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空、ARRAY | 目标数组 |
| `iIndex` | 输入 | — | 正负索引 |
| `pResolved` | 输出 | 非空 | 接收 0 基位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界

#### 范例

[containers](../../examples/value/containers/main.c) · 解析索引

```c
		 !xrtValueArrayResolve(pMutableTags, -1, &iLast) ||
```

### `xrtValueArrayGet`

返回数组指定 0 基索引处借用的值。

```c
xvalue* xrtValueArrayGet(const xvalue* pArray, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空、ARRAY | 目标数组 |
| `iIndex` | 输入 | < 元素数 | 0 基索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 值借用（来源存活期间有效） | — |
| `NULL` | 不存在或越界 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 按位借用

```c
		(void)xrtValueGetString(xrtValueArrayGet(pArray, i), &Text);
```

### `xrtValueArrayAt`

支持负数倒序索引，越界时返回空指针。

```c
xvalue* xrtValueArrayAt(const xvalue* pArray, int64 iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空、ARRAY | 目标数组 |
| `iIndex` | 输入 | — | 正负索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 值借用（来源存活期间有效） | — |
| `NULL` | 不存在或越界 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 倒序索引借用

```c
	(void)xrtValueGetString(xrtValueArrayAt(pArray, -1), &Text);
```

### `xrtValueArrayEdit`

返回已经沿 COW 路径分离的可变子容器，标量子项报告类型错误。

```c
xvalue* xrtValueArrayEdit(xvalue* pArray, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `iIndex` | 输入 | < 元素数 | 0 基索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已分离的可变子容器借用 | — |
| `NULL` | 标量子项或越界 | `XERR_TYPE` / `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界

#### 范例

[graph](../../examples/value/graph/main.c) · 可变子容器

```c
	pCopyChild = xrtValueArrayEdit(pCopy, 0);
```

### `xrtValueArrayAppend`

增加引用后向数组末尾加入值。

```c
bool xrtValueArrayAppend(xvalue* pArray, const xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `pItem` | 输入 | 非空 | 要加入的值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[graph](../../examples/value/graph/main.c) · 追加引用

```c
		 !xrtValueArrayAppend(pRoot, pChild) ||
```

### `xrtValueArrayAppendTake`

成功时把来源引用移交给数组并清空来源。

```c
bool xrtValueArrayAppendTake(xvalue* pArray, xvalue** pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `pItem` | 输入/输出 | 非空 | 来源槽，成功后清空 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 追加移交

```c
		(void)xrtValueArrayAppendTake(pArray, &pB);
```

### `xrtValueArrayAppendNew`

无论成功失败都消费临时值，适合单行构造与加入。

```c
bool xrtValueArrayAppendNew(xvalue* pArray, xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `pItem` | 输入 | 拥有 | 临时值，总是被消费 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 追加消费

```c
	(void)xrtValueArrayAppendNew(pArray, xrtValueString(SV("a")));
```

### `xrtValueArrayInsert`

增加引用后在指定位置插入值。

```c
bool xrtValueArrayInsert(xvalue* pArray, size_t iIndex, const xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `iIndex` | 输入 | <= 元素数 | 插入位置 |
| `pItem` | 输入 | 非空 | 要插入的值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界
- `XERR_MEMORY` — 分配失败

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 插入引用

```c
		(void)xrtValueArrayInsert(pArray, 1u, pOwned);   /* 借用 */
```

### `xrtValueArrayInsertTake`

成功时把来源引用移交到指定插入位置。

```c
bool xrtValueArrayInsertTake(xvalue* pArray, size_t iIndex, xvalue** pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `iIndex` | 输入 | <= 元素数 | 插入位置 |
| `pItem` | 输入/输出 | 非空 | 来源槽，成功后清空 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界
- `XERR_MEMORY` — 分配失败

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 插入移交

```c
		(void)xrtValueArrayInsertTake(pArray, 1u, &pA);
```

### `xrtValueArrayInsertNew`

无论成功失败都消费临时值并在指定位置插入。

```c
bool xrtValueArrayInsertNew(xvalue* pArray, size_t iIndex, xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `iIndex` | 输入 | <= 元素数 | 插入位置 |
| `pItem` | 输入 | 拥有 | 临时值，总是被消费 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界
- `XERR_MEMORY` — 分配失败

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 插入消费

```c
		(void)xrtValueArrayInsertNew(pArray, 0u, xrtValueInt(0));
```

### `xrtValueArraySet`

增加引用后替换旧值；同一指针是引用平衡的成功无操作。

```c
bool xrtValueArraySet(xvalue* pArray, size_t iIndex, const xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `iIndex` | 输入 | < 元素数 | 0 基索引 |
| `pItem` | 输入 | 非空 | 新值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界
- `XERR_MEMORY` — 分配失败

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 替换引用

```c
		(void)xrtValueArraySet(pArray, 1u, pOwned);      /* 借用替换 */
```

### `xrtValueArraySetTake`

成功时把来源引用移交到指定位置。

```c
bool xrtValueArraySetTake(xvalue* pArray, size_t iIndex, xvalue** pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `iIndex` | 输入 | < 元素数 | 0 基索引 |
| `pItem` | 输入/输出 | 非空 | 来源槽，成功后清空 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界
- `XERR_MEMORY` — 分配失败

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 替换移交

```c
		(void)xrtValueArraySetTake(pArray, 1u, &pOwned); /* 移交替换 */
```

### `xrtValueArraySetNew`

无论成功失败都消费临时值并替换指定位置。

```c
bool xrtValueArraySetNew(xvalue* pArray, size_t iIndex, xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `iIndex` | 输入 | < 元素数 | 0 基索引 |
| `pItem` | 输入 | 拥有 | 临时值，总是被消费 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界
- `XERR_MEMORY` — 分配失败

#### 范例

[containers](../../examples/value/containers/main.c) · 替换消费

```c
		 !xrtValueArraySetNew(
			pMutableTags,
			iLast,
			xrtValueString(XRT_STR_LITERAL("http"))
		 ) ) {
```

### `xrtValueArrayRemove`

删除数组区间并释放其中的值。

```c
bool xrtValueArrayRemove(xvalue* pArray, size_t iIndex, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `iIndex` | 输入 | < 元素数 | 区间起点 |
| `iCount` | 输入 | — | 删除数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 删除区间

```c
	(void)xrtValueArrayRemove(pArray, 1u, 1u);          /* 删掉 x2 */
```

### `xrtValueArrayTake`

从数组移交指定值，调用方获得一个引用。

```c
xvalue* xrtValueArrayTake(xvalue* pArray, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `iIndex` | 输入 | < 元素数 | 0 基索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 移交出的值引用 | — |
| `NULL` | 越界 | `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 移交元素

```c
	pTaken = xrtValueArrayTake(pArray, 0u);
```

### `xrtValueArrayPop`

从数组末尾移交一个值。

```c
xvalue* xrtValueArrayPop(xvalue* pArray)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 移交出的值引用 | — |
| `NULL` | 数组为空 | `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 数组为空

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 弹出末尾

```c
	pTaken = xrtValueArrayPop(pArray);
```

### `xrtValueArraySwap`

交换两个数组元素。

```c
bool xrtValueArraySwap(xvalue* pArray, size_t iLeft, size_t iRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `iLeft` | 输入 | < 元素数 | 左索引 |
| `iRight` | 输入 | < 元素数 | 右索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界

#### 范例

[array_tour](../../examples/value/array_tour/main.c) · 交换元素

```c
	(void)xrtValueArraySwap(pArray, 2u, 3u);
```

### `xrtValueArrayExtend`

失败原子地把来源数组全部追加到目标数组，允许来源与目标相同。

```c
bool xrtValueArrayExtend(xvalue* pTarget, const xvalue* pSource)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTarget` | 输入/输出 | 非空、ARRAY | 目标数组 |
| `pSource` | 输入 | 非空、ARRAY | 来源数组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 结果尺寸溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[collections_batch](../../examples/value/collections/batch/main.c) · 批量追加

```c
		 !xrtValueArrayExtend(pLeft, pRight) ) {
```

### `xrtValueArrayConcat`

创建按左右顺序连接的新数组。

```c
xvalue* xrtValueArrayConcat(const xvalue* pLeft, const xvalue* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、ARRAY | 左数组 |
| `pRight` | 输入 | 非空、ARRAY | 右数组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[collections_batch](../../examples/value/collections/batch/main.c) · 连接

```c
	pJoined = xrtValueArrayConcat(pLeft, pRight);
```

### `xrtValueObject`

创建保持首次插入顺序的字符串键对象。

```c
xvalue* xrtValueObject(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[collections](../../examples/value/collections/main.c) · 创建对象

```c
	xvalue* pDefaults = xrtValueObject();
```

### `xrtValueObjectLifo`

创建保持首次插入顺序、最终按逆插入顺序释放拥有值的字符串键对象。

```c
xvalue* xrtValueObjectLifo(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[containers_lifo](../../examples/value/containers/lifo/main.c) · 创建 LIFO 对象

```c
	xvalue* pObject = xrtValueObjectLifo();
```

### `xrtValueObjectGet`

返回对象字符串键借用的值，键按完整字节匹配。

```c
xvalue* xrtValueObjectGet(const xvalue* pObject, xstrview Key)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pObject` | 输入 | 非空、OBJECT | 目标对象 |
| `Key` | 输入 | 借用 | 键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 值借用（来源存活期间有效） | — |
| `NULL` | 不存在或越界 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- 键缺失返回 `NULL` 且不设置错误

#### 范例

[graph](../../examples/value/graph/main.c) · 按键借用

```c
			xrtValueObjectGet(
				pChild,
				XRT_STR_LITERAL("count")
			),
```

### `xrtValueObjectAt`

按首次插入顺序返回借用的键和值；替换已有键不会改变顺序。

```c
xvalue* xrtValueObjectAt(const xvalue* pObject, size_t iIndex, xstrview* pKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pObject` | 输入 | 非空、OBJECT | 目标对象 |
| `iIndex` | 输入 | < 元素数 | 0 基位置 |
| `pKey` | 输出 | 允许空 | 接收键视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 值借用（来源存活期间有效） | — |
| `NULL` | 不存在或越界 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_RANGE` — 索引越界

#### 范例

[containers_lifo](../../examples/value/containers/lifo/main.c) · 按位借用

```c
		 (xrtValueObjectAt(pObject, 0, &Key) == NULL) ) {
```

### `xrtValueObjectEdit`

返回已经沿 COW 路径分离的可变子容器，标量子项报告类型错误。

```c
xvalue* xrtValueObjectEdit(xvalue* pObject, xstrview Key)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pObject` | 输入/输出 | 非空、OBJECT | 目标对象 |
| `Key` | 输入 | 借用 | 键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已分离的可变子容器借用 | — |
| `NULL` | 标量子项或键缺失 | `XERR_TYPE` / 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- 键缺失返回 `NULL` 且不设置错误

#### 范例

[containers](../../examples/value/containers/main.c) · 可变子容器

```c
	pMutableTags = xrtValueObjectEdit(
		pCopy,
		XRT_STR_LITERAL("tags")
	);
```

### `xrtValueObjectHas`

判断对象键是否存在。

```c
bool xrtValueObjectHas(const xvalue* pObject, xstrview Key)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pObject` | 输入 | 非空、OBJECT | 目标对象 |
| `Key` | 输入 | 借用 | 键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是存在 | — |
| `false` | 不是存在 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[object_tour](../../examples/value/object_tour/main.c) · 键存在判断

```c
	printf("has=%d ", xrtValueObjectHas(pObject, SV("k")) ? 1 : 0);
```

### `xrtValueObjectSet`

增加引用后设置对象键值；同一指针不分离且保留首次键位置。

```c
bool xrtValueObjectSet(xvalue* pObject, xstrview Key, const xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pObject` | 输入/输出 | 非空、OBJECT | 目标对象 |
| `Key` | 输入 | 借用 | 键 |
| `pItem` | 输入 | 非空 | 新值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[object_tour](../../examples/value/object_tour/main.c) · 设置引用

```c
		(void)xrtValueObjectSet(pObject, SV("k"), pOwned);
```

### `xrtValueObjectSetTake`

成功时把来源引用移交到对象键。

```c
bool xrtValueObjectSetTake(xvalue* pObject, xstrview Key, xvalue** pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pObject` | 输入/输出 | 非空、OBJECT | 目标对象 |
| `Key` | 输入 | 借用 | 键 |
| `pItem` | 输入/输出 | 非空 | 来源槽，成功后清空 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[containers](../../examples/value/containers/main.c) · 设置移交

```c
		 !xrtValueObjectSetTake(
			pResponse,
			XRT_STR_LITERAL("tags"),
			&pTags
		 ) ) {
```

### `xrtValueObjectSetNew`

无论成功失败都消费临时值并设置对象键。

```c
bool xrtValueObjectSetNew(xvalue* pObject, xstrview Key, xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pObject` | 输入/输出 | 非空、OBJECT | 目标对象 |
| `Key` | 输入 | 借用 | 键 |
| `pItem` | 输入 | 拥有 | 临时值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[collections](../../examples/value/collections/main.c) · 设置消费

```c
		 !xrtValueObjectSetNew(
			pDefaults,
			XRT_STR_LITERAL("timeout"),
			xrtValueInt(30)
		 ) ||
```

### `xrtValueObjectRemove`

删除对象键并释放对应值。

```c
bool xrtValueObjectRemove(xvalue* pObject, xstrview Key)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pObject` | 输入/输出 | 非空、OBJECT | 目标对象 |
| `Key` | 输入 | 借用 | 键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除 | — |
| `false` | 键缺失 | 不设错误 |

#### 错误

- 键缺失返回 `false` 且不设置错误；参数非法 `XERR_ARGUMENT`

#### 范例

[object_tour](../../examples/value/object_tour/main.c) · 删除键

```c
	printf("removed=%d ", xrtValueObjectRemove(pObject, SV("k")) ? 1 : 0);
```

### `xrtValueObjectTake`

移交对象键对应值，缺失时返回空指针。

```c
xvalue* xrtValueObjectTake(xvalue* pObject, xstrview Key)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pObject` | 输入/输出 | 非空、OBJECT | 目标对象 |
| `Key` | 输入 | 借用 | 键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 移交出的值引用 | — |
| `NULL` | 键缺失 | 不设错误 |

#### 错误

- 键缺失返回 `NULL` 且不设置错误；参数非法 `XERR_ARGUMENT`

#### 范例

[object_tour](../../examples/value/object_tour/main.c) · 移交键值

```c
	pTaken = xrtValueObjectTake(pObject, SV("k2"));
```

### `xrtValueObjectMerge`

按冲突策略失败原子地合并两个对象，并保留目标已有键位置。

```c
bool xrtValueObjectMerge(xvalue* pTarget, const xvalue* pSource, xvaluemergepolicy Policy)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTarget` | 输入/输出 | 非空、OBJECT | 目标对象 |
| `pSource` | 输入 | 非空、OBJECT | 来源对象 |
| `Policy` | 输入 | — | 冲突策略 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_VALUE` — 冲突策略拒绝重叠键
- `XERR_MEMORY` — 分配失败

#### 范例

[collections](../../examples/value/collections/main.c) · 合并对象

```c
		 !xrtValueObjectMerge(
			pDefaults,
			pOptions,
			XVALUE_MERGE_REPLACE
		 ) ||
```

### `xrtValueObjectFinalizerBindTake`

消耗一个对象拥有引用，成功绑定后返回同一引用，失败则释放输入并返回 NULL。
失败不安装新回调，也不接管借用上下文；回滚可能执行对象原有的终结器和字段
Drop，原始绑定错误（或调用前已经存在的主错误）不会被这些清理覆盖。
接口不创建额外的上下文分配，也不提供回调代码保活或并发发布安全点。
共享 backing 上的绑定仍按原合同拒绝；不能借此撤销调用方此前已经发布的别名。

```c
xvalue* xrtValueObjectFinalizerBindTake(xvalue* object,
    xvalueobjectfinalizer finalize, ptr context);
```

### `xrtValueObjectFinalizerBind`

为对象绑定最终释放拥有的值时执行的终化器。

```c
bool xrtValueObjectFinalizerBind(xvalue* pObject, xvalueobjectfinalizer pFinalizer, ptr pUserData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pObject` | 输入/输出 | 非空、OBJECT | 目标对象 |
| `pFinalizer` | 输入 | 非空 | 终化器 |
| `pUserData` | 输入 | 任意值 | 终化器数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 绑定终化器

```c
		(void)xrtValueObjectFinalizerBind(pObject, onFinalize, NULL);
```

### `xrtValueIntMap`

创建空的 int64 键稀疏映射。

```c
xvalue* xrtValueIntMap(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[collections_batch](../../examples/value/collections/batch/main.c) · 创建整数映射

```c
	xvalue* pDefaults = xrtValueIntMap();
```

### `xrtValueIntMapGet`

返回稀疏整数键借用的值，缺失是正常结果。

```c
xvalue* xrtValueIntMapGet(const xvalue* pMap, int64 iKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空、INT_MAP | 目标映射 |
| `iKey` | 输入 | — | 键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 值借用（来源存活期间有效） | — |
| `NULL` | 不存在或越界 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- 键缺失返回 `NULL` 且不设置错误

#### 范例

[collections_batch](../../examples/value/collections/batch/main.c) · 按键借用

```c
		 !xrtValueGetInt(xrtValueIntMapGet(pDefaults, 1), &iValue) ) {
```

### `xrtValueIntMapEdit`

返回已经沿 COW 路径分离的可变子容器，标量子项报告类型错误。

```c
xvalue* xrtValueIntMapEdit(xvalue* pMap, int64 iKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空、INT_MAP | 目标映射 |
| `iKey` | 输入 | — | 键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已分离的可变子容器借用 | — |
| `NULL` | 标量子项或键缺失 | `XERR_TYPE` / 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- 键缺失返回 `NULL` 且不设置错误

#### 范例

[object_tour](../../examples/value/object_tour/main.c) · 可变子容器

```c
		pSlot = xrtValueIntMapEdit(pMap, 3);
```

### `xrtValueIntMapHas`

判断整数键是否存在。

```c
bool xrtValueIntMapHas(const xvalue* pMap, int64 iKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空、INT_MAP | 目标映射 |
| `iKey` | 输入 | — | 键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是存在 | — |
| `false` | 不是存在 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[object_tour](../../examples/value/object_tour/main.c) · 键存在判断

```c
	printf("set=%d ", xrtValueIntMapHas(pMap, 2) ? 1 : 0);
```

### `xrtValueIntMapSet`

增加引用后设置整数键值；同一指针是引用平衡的成功无操作。

```c
bool xrtValueIntMapSet(xvalue* pMap, int64 iKey, const xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空、INT_MAP | 目标映射 |
| `iKey` | 输入 | — | 键 |
| `pItem` | 输入 | 非空 | 新值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[object_tour](../../examples/value/object_tour/main.c) · 设置引用

```c
		(void)xrtValueIntMapSet(pMap, 1, pOwned);
```

### `xrtValueIntMapSetTake`

成功时把来源引用移交到整数键。

```c
bool xrtValueIntMapSetTake(xvalue* pMap, int64 iKey, xvalue** pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空、INT_MAP | 目标映射 |
| `iKey` | 输入 | — | 键 |
| `pItem` | 输入/输出 | 非空 | 来源槽，成功后清空 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[object_tour](../../examples/value/object_tour/main.c) · 设置移交

```c
		(void)xrtValueIntMapSetTake(pMap, 2, &pOwned);       /* 移交 */
```

### `xrtValueIntMapSetNew`

无论成功失败都消费临时值并设置整数键。

```c
bool xrtValueIntMapSetNew(xvalue* pMap, int64 iKey, xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空、INT_MAP | 目标映射 |
| `iKey` | 输入 | — | 键 |
| `pItem` | 输入 | 拥有 | 临时值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[collections_batch](../../examples/value/collections/batch/main.c) · 设置消费

```c
		 !xrtValueIntMapSetNew(pDefaults, 1, xrtValueInt(30)) ||
```

### `xrtValueIntMapRemove`

删除整数键并释放对应值。

```c
bool xrtValueIntMapRemove(xvalue* pMap, int64 iKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空、INT_MAP | 目标映射 |
| `iKey` | 输入 | — | 键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除 | — |
| `false` | 键缺失 | 不设错误 |

#### 错误

- 键缺失返回 `false` 且不设置错误；参数非法 `XERR_ARGUMENT`

#### 范例

[object_tour](../../examples/value/object_tour/main.c) · 删除键

```c
	(void)xrtValueIntMapRemove(pMap, 1);
```

### `xrtValueIntMapTake`

移交整数键对应值，缺失时返回空指针。

```c
xvalue* xrtValueIntMapTake(xvalue* pMap, int64 iKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空、INT_MAP | 目标映射 |
| `iKey` | 输入 | — | 键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 移交出的值引用 | — |
| `NULL` | 键缺失 | 不设错误 |

#### 错误

- 键缺失返回 `NULL` 且不设置错误；参数非法 `XERR_ARGUMENT`

#### 范例

[containers_indexed](../../examples/value/containers/indexed/main.c) · 移交键值

```c
	pTaken = xrtValueIntMapTake(pMap, -7);
```

### `xrtValueIntMapMerge`

按冲突策略失败原子地合并两个整数键映射。

```c
bool xrtValueIntMapMerge(xvalue* pTarget, const xvalue* pSource, xvaluemergepolicy Policy)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTarget` | 输入/输出 | 非空、INT_MAP | 目标映射 |
| `pSource` | 输入 | 非空、INT_MAP | 来源映射 |
| `Policy` | 输入 | — | 冲突策略 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_VALUE` — 冲突策略拒绝重叠键
- `XERR_MEMORY` — 分配失败

#### 范例

[collections_batch](../../examples/value/collections/batch/main.c) · 合并映射

```c
		 !xrtValueIntMapMerge(
			pDefaults,
			pOverrides,
			XVALUE_MERGE_REPLACE
		 ) ||
```

### `xrtValueIntMapTrim`

释放 IntMap 空闲节点池页并返回实际释放页数。

```c
size_t xrtValueIntMapTrim(xvalue* pMap, size_t iRetainEmpty)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空、INT_MAP | 目标映射 |
| `iRetainEmpty` | 输入 | — | 保留页数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际释放页数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[object_tour](../../examples/value/object_tour/main.c) · 裁剪节点池

```c
	printf("trim=%zu\n", xrtValueIntMapTrim(pMap, 0));
```

## COW 与线程

`xrtValueClone` 为容器创建独立外壳并共享 backing，复杂度为 `O(1)`。首次有效写入先浅拷贝 backing，失败时可见内容不变。越界或标量 Edit、同值覆盖、缺失删除、重复 Set 加入、空容器清空和不增加容量的 Reserve 都不会无意义地分离 backing。`ArrayEdit`、`IntMapEdit` 和 `ObjectEdit` 只分离需要修改的嵌套路径。

外壳和 backing 引用计数是原子的，但 Value 容器不内置锁。同一外壳的并发 API 调用、写入和生命周期结束必须由调用方同步；需要多线程独立修改时，先为每个执行者创建 `O(1)` Clone。句柄策略若可能从不同外壳并发执行，回调本身必须线程安全。

值图禁止形成强引用环，避免引用计数泄漏；共享 DAG 合法。环检测前 32 个唯一 backing 使用栈内去重，大图才按需分配 Set，因此共享子图按唯一 backing 数量线性访问，不会因重复边指数展开。深度超过 `XRT_VALUE_DEPTH_MAX`、已有递归环或新增反向边报告 `XERR_VALUE`。具有身份的宿主对象图由独立运行时对象模块处理。

子值 `Drop`、Value Set 的 Handle `Hash`/`Equal` 回调不得重入正在操作的父 Value。父外壳与底层容器在回调期间同时进入忙状态，`Clone`、读取、写入和再次释放都以 `XERR_STATE` 拒绝。

## 快照迭代

```c
bool xrtValueIterBegin(const xvalue* value, xvalueiter* iterator);
bool xrtValueIterRBegin(const xvalue* value, xvalueiter* iterator);
xvalueiter* xrtValueIterCreate(const xvalue* value);
xvalueiter* xrtValueIterRCreate(const xvalue* value);
xvalue* xrtValueIterNext(xvalueiter* iterator, xvaluekey* key);
xvalueiterresult xrtValueIterAdvance(
	xvalueiter* iterator,
	xvaluekey* key,
	xvalue** value
);
void xrtValueIterEnd(xvalueiter* iterator);
void xrtValueIterDestroy(xvalueiter* iterator);
```

`xvaluekeytype` 说明每次迭代返回的键：

| 常量 | `xvaluekey` 内容 |
| --- | --- |
| `XVALUE_KEY_NONE` | Set 元素没有独立键。 |
| `XVALUE_KEY_INDEX` | Array 使用 `Index`。 |
| `XVALUE_KEY_INT` | IntMap 使用 `Integer`。 |
| `XVALUE_KEY_STRING` | Object 使用借用的 `String`。 |

迭代器持有 backing 快照。`xrtValueIterBegin` 按稳定正序推进；`xrtValueIterRBegin` 按完全相反的稳定顺序推进。迭代开始后修改原值会触发 COW，旧迭代顺序和借用键仍然有效。Object 字符串键借用快照 backing，迭代结束后失效。键输出不得覆盖迭代器，Begin 输出不得覆盖 Value 外壳；别名错误不会推进快照。Begin 和 RBegin 只能用于未活动或已经 End 的迭代器。该公开层供 JSON、可选数据格式、模板和动态宿主使用，不需要访问任何私有容器结构。

`xrtValueIterNext` 是不隔离当前错误的最短快速路径，返回空值时调用方需按自身上下文判断结束。需要严格区分结果时使用 `xrtValueIterAdvance`：`XVALUE_ITER_ITEM` 表示写出一个借用元素，`XVALUE_ITER_END` 表示正常结束，`XVALUE_ITER_ERROR` 表示失败且当前错误已更新。成功项和正常结束均保留调用前已有错误；空指针或未活动迭代器属于错误。

`Create/RCreate` 提供相同语义的拥有式入口，适合 FFI、语言运行时和不保存公开结构布局的消费者。成功后必须用 `xrtValueIterDestroy` 结束并释放；`Destroy(NULL)` 是空操作。栈上固定存储仍优先使用 `Begin/End`，不会产生一次迭代器分配。

### `xrtValueIterBegin`

启动稳定顺序快照；输出不得覆盖 Value，且不能已处于活动状态。

```c
bool xrtValueIterBegin(const xvalue* pValue, xvalueiter* pIterator)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、容器 | 目标值 |
| `pIterator` | 输出 | 非空 | 接收迭代器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_EXISTS` — 迭代器已处于活动状态
- `XERR_MEMORY` — 分配失败

#### 范例

[containers](../../examples/value/containers/main.c) · 启动快照

```c
	if ( !xrtValueIterBegin(pCopy, &tIterator) ) {
```

### `xrtValueIterRBegin`

启动稳定逆序快照；输出不得覆盖 Value，且不能已处于活动状态。

```c
bool xrtValueIterRBegin(const xvalue* pValue, xvalueiter* pIterator)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、容器 | 目标值 |
| `pIterator` | 输出 | 非空 | 接收迭代器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_EXISTS` — 迭代器已处于活动状态
- `XERR_MEMORY` — 分配失败

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 启动逆序快照

```c
		(void)xrtValueIterRBegin(pArray, &rIter);
```

### `xrtValueIterNext`

返回下一借用值及其键；键输出不得覆盖迭代器，正常结束返回空指针。

```c
xvalue* xrtValueIterNext(xvalueiter* pIterator, xvaluekey* pKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入/输出 | 已启动 | 目标迭代器 |
| `pKey` | 输出 | 允许空 | 接收键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 借用值；遍历结束为 `NULL` | — |
| `NULL` | 遍历结束 | 不设错误 |

#### 错误

- 遍历结束返回 `NULL` 且不设置错误；参数非法 `XERR_ARGUMENT`

#### 范例

[containers](../../examples/value/containers/main.c) · 下一元素

```c
	while ( (pItem = xrtValueIterNext(&tIterator, &Key)) != NULL ) {
```

### `xrtValueIterAdvance`

三态推进快照；成功项写入借用值，正常结束不设置错误。

```c
xvalueiterresult xrtValueIterAdvance(xvalueiter* pIterator, xvaluekey* pKey, xvalue** ppValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入/输出 | 已启动 | 目标迭代器 |
| `pKey` | 输出 | 允许空 | 接收键 |
| `ppValue` | 输出 | 非空 | 接收借用值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XVALUE_ITER_ITEM` | 已产出 | — |
| `XVALUE_ITER_END` | 遍历结束 | 不设错误 |
| `XVALUE_ITER_ERROR` | 失败 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 迭代器未启动或已结束

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 三态推进

```c
		while ( xrtValueIterAdvance(&rIter, &rKey, &pItem) == XVALUE_ITER_ITEM ) {
```

### `xrtValueIterEnd`

结束迭代并释放 backing 快照。

```c
void xrtValueIterEnd(xvalueiter* pIterator)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入/输出 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已结束 | — |

#### 错误

- 无 — 结束不失败

#### 范例

[containers](../../examples/value/containers/main.c) · 结束快照

```c
	xrtValueIterEnd(&tIterator);
```

### `xrtValueIterCreate`

创建按稳定正序推进的拥有式快照迭代器；调用方必须 Destroy。

```c
xvalueiter* xrtValueIterCreate(const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、容器 | 目标值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有式迭代器 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 创建拥有式迭代器

```c
		xvalueiter* pIter = xrtValueIterCreate(pArray);
```

### `xrtValueIterRCreate`

创建按稳定逆序推进的拥有式快照迭代器；调用方必须 Destroy。

```c
xvalueiter* xrtValueIterRCreate(const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空、容器 | 目标值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有式迭代器 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 创建逆序迭代器

```c
		xvalueiter* pRIter = xrtValueIterRCreate(pArray);
```

### `xrtValueIterDestroy`

结束并释放拥有式迭代器；允许传入空指针。

```c
void xrtValueIterDestroy(xvalueiter* pIterator)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 销毁迭代器

```c
		xrtValueIterDestroy(pIter);
```

## 批量操作与集合代数

```c
bool xrtValueArrayExtend(xvalue* target, const xvalue* source);
xvalue* xrtValueArrayConcat(const xvalue* left, const xvalue* right);

bool xrtValueIntMapMerge(
	xvalue* target,
	const xvalue* source,
	xvaluemergepolicy policy
);
bool xrtValueObjectMerge(
	xvalue* target,
	const xvalue* source,
	xvaluemergepolicy policy
);

bool xrtValueSetMerge(xvalue* target, const xvalue* source);
xvalue* xrtValueSetUnion(const xvalue* left, const xvalue* right);
xvalue* xrtValueSetIntersection(const xvalue* left, const xvalue* right);
xvalue* xrtValueSetDifference(const xvalue* left, const xvalue* right);
xvalue* xrtValueSetSymmetricDifference(
	const xvalue* left,
	const xvalue* right
);
bool xrtValueSetIsSubset(
	const xvalue* left,
	const xvalue* right,
	bool proper
);
bool xrtValueSetIsSuperset(
	const xvalue* left,
	const xvalue* right,
	bool proper
);
bool xrtValueSetIsDisjoint(
	const xvalue* left,
	const xvalue* right
);
bool xrtValueSetEqual(
	const xvalue* left,
	const xvalue* right
);
```

Array Extend、IntMap/Object Merge 和 Set Merge 都是失败原子操作：全部准备完成后才一次提交，OOM、冲突或环检测失败不会暴露部分结果；来源与目标可以是同一个值。映射合并必须显式选择策略：

- `XVALUE_MERGE_KEEP`：保留目标已有值，只加入缺失键。
- `XVALUE_MERGE_REPLACE`：来源覆盖已有值，但 Object 中已有键的位置不变。
- `XVALUE_MERGE_ERROR`：任一键冲突即报告 `XERR_EXISTS`，目标保持不变。

Object 的来源新键按来源顺序追加。Set 运算直接复用通用 Set 已压实的失败原子实现：并集保持左集合顺序并追加右侧独有元素；交集和差集保持左集合顺序；对称差集先放左侧独有元素，再放右侧独有元素。`IsSubset` 和 `IsSuperset` 支持普通或严格关系判断；`IsDisjoint` 判断是否没有共同元素；`SetEqual` 在不启用值图模块时也能直接比较两个标量集合。

空来源批量操作、映射共享同一 backing 且没有实际替换、Set 子集合并和自身 Keep/Replace 合并都是不分配、不分离 backing 的成功操作。空目标接收非空 Array、IntMap、Object 或 Set 时直接共享来源 backing，再由 COW 保证后续修改隔离。Union/Intersection 等同一集合或空集合恒等式也走 `O(1)` Clone 或直接创建空结果，不复制元素。

Value Set 的 Handle `Hash`/`Equal` 策略执行期间，左右 Value 外壳和底层 Set 同时进入忙状态；回调不能读取、Clone、释放或修改任一参与外壳。映射批量提交释放被替换旧值时，目标外壳同样保持忙状态。关系不成立是正常的 `false`；非法参数、类型、忙状态和分配失败分别报告结构化错误。

### `xrtValueSet`

创建空的可哈希动态值集合。

```c
xvalue* xrtValueSet(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[collections](../../examples/value/collections/main.c) · 创建集合

```c
	xvalue* pLeft = xrtValueSet();
```

### `xrtValueSetAdd`

增加引用后把可哈希标量或显式身份容器加入集合。

```c
bool xrtValueSetAdd(xvalue* pSet, const xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入/输出 | 非空、SET | 目标集合 |
| `pItem` | 输入 | 非空 | 要加入的值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[containers_indexed](../../examples/value/containers/indexed/main.c) · 加入引用

```c
		 !xrtValueSetAdd(pSet, pQuery) ) {
```

### `xrtValueSetAddTake`

成功时消费来源引用；重复元素同样视为成功。

```c
bool xrtValueSetAddTake(xvalue* pSet, xvalue** pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入/输出 | 非空、SET | 目标集合 |
| `pItem` | 输入/输出 | 非空 | 来源槽，成功后清空 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[set_tour](../../examples/value/set_tour/main.c) · 加入移交

```c
		(void)xrtValueSetAddTake(pA, &pOwned);
```

### `xrtValueSetAddNew`

无论成功失败都消费临时值并尝试加入集合。

```c
bool xrtValueSetAddNew(xvalue* pSet, xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入/输出 | 非空、SET | 目标集合 |
| `pItem` | 输入 | 拥有 | 临时值，总是被消费 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[collections](../../examples/value/collections/main.c) · 加入消费

```c
		 !xrtValueSetAddNew(pLeft, xrtValueString(XRT_STR_LITERAL("read"))) ||
```

### `xrtValueSetHas`

判断等价值是否在集合中。

```c
bool xrtValueSetHas(const xvalue* pSet, const xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空、SET | 目标集合 |
| `pItem` | 输入 | 非空 | 查找值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是存在 | — |
| `false` | 不是存在 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[containers_indexed](../../examples/value/containers/indexed/main.c) · 成员判断

```c
		 !xrtValueSetHas(pSet, pQuery) ) {
```

### `xrtValueSetRemove`

删除等价值并释放集合持有的引用。

```c
bool xrtValueSetRemove(xvalue* pSet, const xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入/输出 | 非空、SET | 目标集合 |
| `pItem` | 输入 | 非空 | 查找值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除 | — |
| `false` | 不存在 | 不设错误 |

#### 错误

- 不存在返回 `false` 且不设置错误；参数非法 `XERR_ARGUMENT`

#### 范例

[set_tour](../../examples/value/set_tour/main.c) · 删除成员

```c
	printf("removed=%d\n", xrtValueSetRemove(pB, xrtValueInt(3)) ? 1 : 0);
```

### `xrtValueSetTake`

移交集合中的规范值，缺失时返回空指针。

```c
xvalue* xrtValueSetTake(xvalue* pSet, const xvalue* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入/输出 | 非空、SET | 目标集合 |
| `pItem` | 输入 | 非空 | 查找值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 移交出的值引用 | — |
| `NULL` | 不存在 | 不设错误 |

#### 错误

- 不存在返回 `NULL` 且不设置错误；参数非法 `XERR_ARGUMENT`

#### 范例

[set_tour](../../examples/value/set_tour/main.c) · 移交成员

```c
	pTaken = xrtValueSetTake(pA, xrtValueInt(2));
```

### `xrtValueSetMerge`

失败原子地把来源集合中的缺失元素追加到目标集合。

```c
bool xrtValueSetMerge(xvalue* pTarget, const xvalue* pSource)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTarget` | 输入/输出 | 非空、SET | 目标集合 |
| `pSource` | 输入 | 非空、SET | 来源集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[collections](../../examples/value/collections/main.c) · 合并集合

```c
		 !xrtValueSetMerge(pMerged, pRight) ||
```

### `xrtValueSetEqual`

判断两个集合是否拥有相同元素。

```c
bool xrtValueSetEqual(const xvalue* pLeft, const xvalue* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、SET | 左集合 |
| `pRight` | 输入 | 非空、SET | 右集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是相同 | — |
| `false` | 不是相同 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[collections](../../examples/value/collections/main.c) · 集合相等

```c
		 !xrtValueSetEqual(pMerged, pUnion) ) {
```

### `xrtValueSetUnion`

创建两个集合的并集，结果先保持左集合顺序。

```c
xvalue* xrtValueSetUnion(const xvalue* pLeft, const xvalue* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、SET | 左集合 |
| `pRight` | 输入 | 非空、SET | 右集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[collections](../../examples/value/collections/main.c) · 并集

```c
	pUnion = xrtValueSetUnion(pLeft, pRight);
```

### `xrtValueSetIntersection`

创建两个集合的交集，结果保持左集合顺序。

```c
xvalue* xrtValueSetIntersection(const xvalue* pLeft, const xvalue* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、SET | 左集合 |
| `pRight` | 输入 | 非空、SET | 右集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[set_tour](../../examples/value/set_tour/main.c) · 交集

```c
	pResult = xrtValueSetIntersection(pA, pB);
```

### `xrtValueSetDifference`

创建左集合相对右集合的差集。

```c
xvalue* xrtValueSetDifference(const xvalue* pLeft, const xvalue* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、SET | 左集合 |
| `pRight` | 输入 | 非空、SET | 右集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[set_tour](../../examples/value/set_tour/main.c) · 差集

```c
	pResult = xrtValueSetDifference(pA, pB);
```

### `xrtValueSetSymmetricDifference`

创建两个集合的对称差集，右侧独有元素追加在左侧独有元素之后。

```c
xvalue* xrtValueSetSymmetricDifference(const xvalue* pLeft, const xvalue* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、SET | 左集合 |
| `pRight` | 输入 | 非空、SET | 右集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_MEMORY` — 分配失败

#### 范例

[set_tour](../../examples/value/set_tour/main.c) · 对称差集

```c
	pResult = xrtValueSetSymmetricDifference(pA, pB);
```

### `xrtValueSetIsSubset`

判断左集合是否为右集合的子集，可选择严格子集。

```c
bool xrtValueSetIsSubset(const xvalue* pLeft, const xvalue* pRight, bool bProper)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、SET | 左集合 |
| `pRight` | 输入 | 非空、SET | 右集合 |
| `bProper` | 输入 | — | 是否严格 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是子集 | — |
| `false` | 不是子集 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[set_tour](../../examples/value/set_tour/main.c) · 子集判断

```c
		xrtValueSetIsSubset(pA, pBig, false) ? 1 : 0,
```

### `xrtValueSetIsSuperset`

判断左集合是否为右集合的超集，可选择严格超集。

```c
bool xrtValueSetIsSuperset(const xvalue* pLeft, const xvalue* pRight, bool bProper)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、SET | 左集合 |
| `pRight` | 输入 | 非空、SET | 右集合 |
| `bProper` | 输入 | — | 是否严格 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是超集 | — |
| `false` | 不是超集 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[set_tour](../../examples/value/set_tour/main.c) · 超集判断

```c
		xrtValueSetIsSuperset(pBig, pSmall, false) ? 1 : 0);
```

### `xrtValueSetIsDisjoint`

判断两个集合是否没有任何共同元素。

```c
bool xrtValueSetIsDisjoint(const xvalue* pLeft, const xvalue* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、SET | 左集合 |
| `pRight` | 输入 | 非空、SET | 右集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是无共同元素 | — |
| `false` | 不是无共同元素 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[collections](../../examples/value/collections/main.c) · 互斥判断

```c
		 !xrtValueSetIsDisjoint(pLeft, pRight) ) {
```

## 值图

```c
xvalue* xrtValueDeepClone(const xvalue* value);
bool xrtValueEqual(const xvalue* left, const xvalue* right);
```

深拷贝与源图完全隔离，并保留同一个 Value 外壳在结果图中的共享身份。不同 COW
外壳即使暂时共享 backing，仍表示两个独立的可修改位置，因此分别深克隆。null、
bool 和其他不可变标量只增加引用，复杂度为 `O(1)` 且不分配；拥有句柄只有提供
`Clone` 策略时才能深拷贝，避免把同一 native 资源伪装成两个独立所有者。Clone
失败却遗留非空输出时，库会防御性调用 `Drop` 回收，但回调仍违反契约。

结构相等比较内容而不是共享拓扑：一侧重复引用同一子值、另一侧放置两个内容相等
的独立子值，结果仍然相等。Object 不比较插入顺序，Array 比较顺序，Set 复用通用
Set 的等价元素关系；有符号整数、无符号整数与可无损转换的浮点数仍按精确数值相等。两个不同 Handle 必须
具有相同的策略与 `user_data`，并提供 `Equal`，否则报告 `XERR_TYPE`。

深克隆和结构相等各使用 32 项栈内身份表，小图不会为遍历状态分配；更大的图才按需
创建 Map。深克隆按源 Value 身份去重并保留 DAG，结构相等按已经验证相等的值对去重，
因此共享 DAG 不会按重复路径指数展开。结构相等不修改输入，但大图记忆表可能报告
`XERR_MEMORY`。从根开始最多访问 `XRT_VALUE_DEPTH_MAX` 层值；下一层报告
`XERR_VALUE`。

Handle Clone/Equal 和嵌套 Set 的 Hash/Equal 执行期间，当前 Handle、当前容器以及从
根到当前位置的活动祖先都进入忙状态。回调不得对这些 Value 执行读取、Clone、写入
或释放；同一外壳的跨线程调用仍由调用方同步。

### `xrtValueType`

返回值类型，空指针返回 `INVALID`。

```c
xvaluetype xrtValueType(const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 目标值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 类型枚举 | 当前类型 | — |

#### 错误

- 无 — 空指针返回 `INVALID` 不设错

#### 范例

[basic](../../examples/value/basic/main.c) · 类型查询

```c
		(xrtValueType(arrValues[2]) != XVALUE_STRING) ||
```

### `xrtValueTypeName`

返回稳定的类型名称。

```c
cstr xrtValueTypeName(xvaluetype Type)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Type` | 输入 | — | 类型枚举 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 静态类型名称 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[basic](../../examples/value/basic/main.c) · 类型名称

```c
	printf("time type: %s\n", xrtValueTypeName(xrtValueType(arrValues[4])));
```

### `xrtValueIs`

判断值是否具有指定类型。

```c
bool xrtValueIs(const xvalue* pValue, xvaluetype Type)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 目标值 |
| `Type` | 输入 | — | 类型枚举 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是该类型 | — |
| `false` | 不是该类型 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[basic](../../examples/value/basic/main.c) · 类型判断

```c
		!xrtValueIs(arrValues[2], XVALUE_STRING) ||
```

### `xrtValueIsNumber`

判断值是否为整数或浮点数。

```c
bool xrtValueIsNumber(const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 目标值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是数值 | — |
| `false` | 不是数值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[basic](../../examples/value/basic/main.c) · 数值判断

```c
		!xrtValueIsNumber(arrValues[0]) ||
```

### `xrtValueIsContainer`

判断值是否为四种基础容器之一。

```c
bool xrtValueIsContainer(const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 目标值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是容器 | — |
| `false` | 不是容器 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[basic](../../examples/value/basic/main.c) · 容器判断

```c
		xrtValueIsContainer(arrValues[0]) ||
```

### `xrtValueIsWeakRef`

判断值是否是由 `xrtValueWeakRef` 创建的弱引用。

```c
bool xrtValueIsWeakRef(const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 目标值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是弱引用 | — |
| `false` | 不是弱引用 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 弱引用判断

```c
	printf("weak: is=%d", xrtValueIsWeakRef(pWeak) ? 1 : 0);
```

### `xrtValueTypeId`

返回调用者绑定的不透明语义类型身份；未绑定或空指针返回零。

```c
uint64 xrtValueTypeId(const xvalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 目标值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 绑定的身份；未绑定为 0 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 语义类型身份

```c
			(unsigned long long)xrtValueTypeId(pObject));
```

### `xrtValueTypeIdBind`

为值绑定不透明语义类型身份。

```c
bool xrtValueTypeIdBind(xvalue* pValue, uint64 iTypeId)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入/输出 | 非空 | 目标值 |
| `iTypeId` | 输入 | 非零 | 身份标识 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_VALUE` — 身份为零或已绑定

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 绑定身份

```c
		(void)xrtValueTypeIdBind(pObject, 77u);
```

### `xrtValueTypeIdRebind`

仅在值外壳唯一拥有时，把既有语义类型身份替换为新的非零身份。

```c
bool xrtValueTypeIdRebind(xvalue* pValue, uint64 iTypeId)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入/输出 | 非空、唯一拥有 | 目标值 |
| `iTypeId` | 输入 | 非零 | 新身份 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 外壳非唯一拥有
- `XERR_VALUE` — 未绑定或新身份为零

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 重绑身份

```c
		(void)xrtValueTypeIdRebind(pObject, 88u);
```

### `xrtValueIdentityBind`

为容器绑定自定义哈希与相等函数，使其可加入集合。

```c
bool xrtValueIdentityBind(xvalue* pValue, xvalueidentityhash pHash, xvalueidentityequal pEqual, ptr pUserData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入/输出 | 非空、容器 | 目标值 |
| `pHash` | 输入 | 非空 | 哈希函数 |
| `pEqual` | 输入 | 非空 | 相等函数 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配
- `XERR_STATE` — 容器已绑定或非空

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 绑定身份函数

```c
				xrtValueIdentityBind(pSet, intHash, intEqual, NULL) ? 1 : 0);
```

### `xrtValueHash`

为可哈希标量或显式身份容器计算一致哈希；指针和句柄哈希只在当前进程内有效。

```c
bool xrtValueHash(const xvalue* pValue, uint64* pHash)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 目标值 |
| `pHash` | 输出 | 非空 | 接收哈希 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[basic](../../examples/value/basic/main.c) · 一致哈希

```c
		!xrtValueHash(arrValues[0], &iIntHash) ||
```

### `xrtValueEqual`

按数值与标量内容判断相等；不可比较句柄和容器报告类型错误。

```c
bool xrtValueEqual(const xvalue* pLeft, const xvalue* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空 | 左值 |
| `pRight` | 输入 | 非空 | 右值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是相等 | — |
| `false` | 不是相等 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[graph](../../examples/value/graph/main.c) · 深度相等

```c
		 !xrtValueEqual(pRoot, pCopy) ||
```

### `xrtValueScalarEqual`

按标量数值判断相等。

```c
bool xrtValueScalarEqual(const xvalue* pLeft, const xvalue* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空 | 左值 |
| `pRight` | 输入 | 非空 | 右值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是相等 | — |
| `false` | 不是相等 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[basic](../../examples/value/basic/main.c) · 标量相等

```c
		!xrtValueScalarEqual(arrValues[0], arrValues[1]) ||
```

### `xrtValueWeakRef`

为动态 Value 创建一个拥有独立生命周期的弱引用值。

```c
xvalue* xrtValueWeakRef(const xvalue* pTarget)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTarget` | 输入 | 非空 | 目标值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新值（引用 1），用后 `xrtValueRelease` | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 目标是静态标量单例
- `XERR_MEMORY` — 分配失败

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 创建弱引用

```c
	pWeak = xrtValueWeakRef(pTarget);
```

### `xrtValueWeakRefExpired`

判断弱引用目标是否已经结束强生命周期。

```c
bool xrtValueWeakRefExpired(const xvalue* pWeak)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWeak` | 输入 | 非空、WEAK | 弱引用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是过期 | — |
| `false` | 不是过期 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 过期判断

```c
		xrtValueWeakRefExpired(pWeak) ? 1 : 0);
```

### `xrtValueWeakRefLock`

尝试提升弱引用；过期时返回进程期 null 单例。

```c
xvalue* xrtValueWeakRefLock(const xvalue* pWeak)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWeak` | 输入 | 非空、WEAK | 弱引用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 提升后的强引用（过期时为 null 单例） | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TYPE` — 值类型与操作不匹配

#### 范例

[iter_weak](../../examples/value/iter_weak/main.c) · 提升弱引用

```c
	pLocked = xrtValueWeakRefLock(pWeak);
```

## 错误

| 错误 | 典型原因 |
| --- | --- |
| `XERR_ARGUMENT` | 空必需参数、非法视图、Take 来源槽自别名、Getter/Hash/索引输出覆盖拥有内存、迭代输出别名、句柄策略不完整 |
| `XERR_TYPE` | Getter 类型不匹配、容器传给标量 Hash/Equal、两个不同句柄缺少结构比较策略 |
| `XERR_STATE` | 引用计数耗尽、Hash/Equal/Clone/Drop 回调重入活动 Value、失效迭代器、Clone 回调失败却未报告错误 |
| `XERR_MEMORY` | Value 外壳、字符串、字节、backing、事务准备或大图记忆表分配失败 |
| `XERR_RANGE` | 字符串终止字节、容量、负索引解析或容器索引计算溢出 |
| `XERR_VALUE` | 值图强引用环、非法已有环或递归深度超过上限 |
| `XERR_EXISTS` | 使用 `XVALUE_MERGE_ERROR` 时映射键冲突 |
| `XERR_UNSUPPORTED` | IntMap Reserve，或深克隆拥有句柄但策略没有 Clone |

完整可执行示例：

- `examples/value/basic/main.c`：全部基础标量、Getter、类型查询、Hash 与标量相等。
- `examples/value/ownership/main.c`：String/Bytes Take、Retain、Clone 与统一释放。
- `examples/value/handle/main.c`：Native Handle 策略、接管、借用、Hash 与相等。
- `examples/value/containers/main.c`：Array、Object、负索引、嵌套 COW 与快照迭代。
- `examples/value/containers/indexed/main.c`：IntMap、Set、数值等价和 Take 所有权。
- `examples/value/containers/lifo/main.c`：LIFO Object 的正向访问与逆序资源析构。
- `examples/value/collections/main.c`：Object 覆盖合并、Set 合并与完整关系判断。
- `examples/value/collections/batch/main.c`：Array 扩展/连接和 IntMap 冲突策略。
- `examples/value/graph/main.c`：DAG 身份保留深克隆、结构相等和修改隔离。

### 迭代器物理拥有视图

`xrtValueIterOwnership` 为栈上或堆上迭代器提供只读物理视图。迭代器有一个
唯一的 End/Destroy 拥有者；它只拥有创建时保留的 backing 快照，不拥有源
Value 壳、当前借用元素或键。原生 Retain 和 COW Clone 的语义不变，源容器
修改或释放后，迭代器仍访问原快照。活动迭代器不可复制为第二个拥有者。

空指针返回空视图；零初始化或已 End 的迭代器是没有子边的唯一节点。
检查不会推进游标，也不调用析构；失败不修改检查结果或引用图。调用方必须
保证全图静止及代码和数据驻留。本视图不是并发收集、安全点或模块卸载许可。
`value_iterator_ownership_tests` 覆盖四种容器、正逆序、栈/堆游标、共享元素、
COW/原生别名、释放源之后的遍历及检查分配失败。

### 两阶段对象生命周期

`xrtValueObjectFinalizerPrepareOwned` 必须在唯一外壳和 backing 上、发布引用
之前调用。它立即接管上下文及其真实拥有边，保留字段全部释放之后的 Release
尾部，但暂不激活用户 Finalizer。`xrtValueObjectFinalizerCommit` 在构造成功
后激活一次；构造期间创建的 Retain、Clone 或原生游标不妨碍这次提交。
提交借用一个活对象，不分配、不调用回调、不消费输入；重复、未准备或正在
终结的对象提交失败且不改变原状态。

若构造失败，原生别名可以延长已初始化字段和上下文的生命，但不会使完整
用户析构被执行。最后一个别名释放后仍按字段、上下文顺序结束所有权。
已提交对象继续遵守原终结合同，包括 Finalizer 内创建的游标延迟字段和
上下文释放。Prepare/Commit 不是并发字段修改或图收集接口；修改须由调用方
串行化，持有独立拥有引用的并发 Retain/Release 不受影响。

准备后对象具有引用身份；原有禁止复制终结责任的 COW 分裂规则不变。
因此游标持有 backing 时，要求分裂 backing 的字段修改仍按原合同拒绝，
不能把“允许共享后的生命周期提交”解释为“允许修改不可变快照”。

## 可复制的 backing 生命周期能力

`xrtValueObjectLifetimeBindOwned` 把不可变上下文交给独立的共享生命周期节点，
不创建终结职责。绑定要求唯一外壳/backing；分配或校验失败不修改对象、不运行
回调、不消费上下文。成功后 backing 拥有该节点，节点拥有上下文及 Trace 描述
的真实强边；非空上下文必须提供 Trace，Release 始终必需。

浅 Clone 共享 backing；COW 分裂、DeepClone 的新 Object backing 各增加一次
生命周期节点引用。节点计数对应实际 backing 边，不把每个别名外壳误算成一个
上下文 owner。Release 仅在最后一个关联 backing 的字段及 finalizer context
全部释放后执行。代码租约适合放在这个节点，普通可复制对象不需要伪造空析构。
回调地址必须保持有效；Release 若可能释放最后代码引用，本身须为常驻函数。

`xrtValueObjectConstructionPrepare` 标记没有用户析构的普通构造事务；不会使
对象变为禁止 COW 分裂的引用身份。其 pending 状态随普通 backing 分裂/复制
传播，`xrtValueObjectConstructionCommit` 无分配地提交一次，也可提交通过
FinalizerPrepareOwned 准备的实际析构。原 FinalizerCommit 仍只接受真正的
owned finalizer；有用户终结职责的共享 backing 仍不能分裂或复制该职责。
DeepClone 继承生命周期能力但不复制终结职责，语言的 clone 策略须由上层定义。

空 Object 合并也从目标准备 backing，保留目标生命周期和构造状态，不借来源
backing 的快速路径替换它们。源数据字段仍按正常合并策略复制/共享。
这不是环收集器或原生并发图安全点；字段/构造状态修改及 ownership inspection
继续遵守既有同步要求，独立 backing 对同一生命周期节点的引用释放是原子的。

## 普通 Value 的物理接管适配器

`xrtValueOwnershipAdapterV1` 在调用方已持有 ownership freeze 时识别本 XRT
实例的普通 Value 外壳和四类容器 backing。它不调用未知 Count、Trace 或用户
策略，也不读取外部 descriptor 的尾部。自定义 handle、identity、finalizer、
lifetime、构造中对象，以及借用 blob 内存仍返回 NULL；包括上下文为 NULL
但代码来自外部的析构器。弱引用 Handle 是已知内建策略，不提供强拥有边。

适配器遵循 core 中 V1 的完整图保有与两阶段接管合同。普通强引用仍按原语义
保有；Claim 暂时使 WeakRefLock 返回 null，但 WeakRefExpired 仍反映实际强
生命周期。撤销 Restore 后可以重新提升。Clear 提交隔离并按物理 backing
断开元素强边，不经 COW EnsureUnique、不分配，所有目标须仍由事务实际保有。
有外部别名可达的 backing 不得 Clear。Drop 在 freeze 外归还事务引用。

该接口不自动寻找环，也不会把未建模的类析构/原生 callable 默认为空清理。
专项 `ownership_adapter_tests` 保留了普通终态析构，验证实际 COW 共享、重复边、
弱准入恢复、无分配清理及在回调之前拒绝自定义策略。

## 显式认证的类与原生 Handle 生命周期

`xrtValueObjectOwnershipBindV1` 仅接受尚未发布的 prepared 对象，绑定不可变的
`xvalueobjectownershipv1` 策略身份；收集器必须用同一授权策略调用
`xrtValueObjectOwnershipAdapterV1`。非空 Trace 并不意味着允许接管。
`xrtValueObjectIdentityBindV1` 把同一代码生命周期覆盖的 hash/equal 纳入认证；
普通 identity 绑定不会隐式获得这种权限。COW 副本继承实际 lifetime 拥有边。

非终态 Finalize 借用事务实际保有的 receiver，先物理撤销析构职责，再在 freeze
之外调用 `FinalizeChecked`；返回 false 即失败，即使没有错误对象。终态 RC
路径仍使用 void `Finalize`，两者必须表达同一语言职责，绝不会各调用一次。
撤销事务不恢复已执行职责；字段与 context/lifetime 在重验证和全部语义析构后
才机械释放。真正的事务 backing Hold 有独立记账，不伪造强计数，也不把它误当
COW 别名；第二个真实外壳/游标仍遵守原有共享与借用 receiver 限制。

`xrtValueHandleOwnershipAdapterV1` 只识别明确授权的 resident Ops/Trace 对，
Clear 隔离实际 payload，Finish 在 freeze 之外运行一次机械 Drop。它不认证
payload 指向的 callable 或任意用户对象；每个传递子节点仍须独立准入。

普通末次释放同样区分物理修改和语义析构：已认证的类先在短 mutation 内归零并
隔离节点，然后在该 scope 外调用真实 receiver 的 Finalize；字段、context、
copyable lifetime 的拥有引用仍保持到各自实际释放边界。原生游标结束先脱离
自己的 backing/receiver 槽，再运行子释放。未知 legacy finalizer/lifetime 不
获得这一认证；外部调用方自己持有的 mutation 不会被库自动暂停。

`xrtValueHandleOwnershipBindPhased` 是独立的显式末次 Drop 合同，要求不可变的
Ops/Trace 和真实代码生命周期自行协调载荷变化与活动。它与 trace-only 的
`xrtValueHandleOwnershipBind` 都不自动授权图收集；后者保留保守的 Drop scope。
DeepClone 继承同一 Drop 合同。xruntime 已知 callable/Future Value 包装层使用
phased 绑定，任意 Object/Weak/native 包装策略不会因有 Trace 而自动升级。

普通 String/Bytes 有两种物理拥有形式：`xrtValueString/Bytes` 把副本内联保存在
同一个 Value 分配中，Take 入口则拥有单独分配。`OWNED_DATA` 只表示后者，不能
据此拒绝前者。普通适配器识别本实例的内联地址或明确的独立拥有标志，仍不接纳
未知外部借用字节。`ownership_adapter_tests` 保留原分母，新增 200 个容器图、
1200 个内联/空/Take 的物理字符串和字节值，以及 Clone 后的 2400 个实际拥有槽，
覆盖清理与恢复、精确重复边计数、无分配事务及内存平衡。不可变标量的 Clone
保有原节点，不创建第二个物理节点。

### 托管快照游标

`xrtValueCursorCreate/RCreate` 返回不透明的 `xvaluecursor`，以
`xrtValueCursorRetain/Release` 管理真实引用；它与可放在栈上的独占
`xvalueiter` 是不同的物理合同，旧布局及 Begin/End/Create/Destroy 不变。
游标内联保存迭代状态，实际拥有 backing 快照；带析构的 identity 对象另外
保有原对象 shell。普通 COW 源不被保有，当前 item/key 均为借用。

`xrtValueCursorAdvance` 保留三态返回与调用前错误隔离，调用者在整个操作期间
保有引用并串行推进。计数/追踪和 `xrtValueCursorOwnershipAdapterV1` 在推进
及其错误处理尾部拒绝图准入；引用变化参与 mutation domain。
`xrtValueCursorOwnership` 报告实际引用和内联拥有槽，没有额外的虚构迭代节点。
适配器 Hold/Drop 是实际引用，Claim/Restore 精确匹配 token，Clear 只移动
拥有槽，Finish 在 freeze 之外结束快照并释放子引用，可重复而不重复释放。
所有传递子节点仍需独立准入，尤其是析构对象的 shell/backing/lifetime；
获得游标适配器本身不等于拥有完整循环回收或模块卸载权限。
