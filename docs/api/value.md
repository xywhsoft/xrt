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
