# 类型数组

`typed_array` 在原始 `xarray` 连续存储之上增加运行时元素类型、完整值生命周期和对象图
追踪。启用宏为 `XRUNTIME_FEATURE_TYPED_ARRAY`，依赖 `array` 与 `runtime_type`。

四类类型容器共享的所有权、失败原子性、回调重入和迭代失效规则见
[类型容器共同契约](typed_containers.md)。

```c
#include <xrt/typed_array.h>
```

## 分层

`xarray` 只管理固定大小字节槽，不处理槽内资源；`xtypedarray` 借用一个不可变 `xrttype`，
并让初始化、复制、移动、销毁、比较和追踪统一经过 `xrtType*Value`。该结构本身不包含引用
计数、所有者模式或隐藏注册表。

C 用户可直接在栈上使用：

```c
xtypedarray Values;
int64 Value = 7;

if ( !xrtTypedArrayInit(&Values, xrtTypeInt64()) ||
	 !xrtTypedArrayPush(&Values, &Value) ) {
	return false;
}
xrtTypedArrayUnit(&Values);
```

`xrtTypedArrayCreate/Destroy` 只管理普通 `xtypedarray` 结构，不创建 `xrtobject`。需要对象身份、
弱引用和循环回收时，把 `xtypedarray` 作为对象负载并使用后述 `InstanceOps`，避免第二套引用计数。

## 元素契约

初始化要求元素类型：

- 通过 `xrtTypeValidate`，并且 `Size != 0`；
- 声明 `XRT_TYPE_FLAG_COPYABLE`；
- 声明 `XRT_TYPE_FLAG_RELOCATABLE`。

连续数组扩容、插入、删除、交换和反转会搬迁值字节，因此不可重定位值在初始化时直接被拒绝。
搬迁只改变唯一值的地址，不产生副本；元素的复制和销毁仍严格调用类型操作。

类型描述由调用方持有，必须比数组及数组所属对象存活更久。元素类型的 `Init`、`Copy`、
`Move`、`Drop`、`Compare` 和 `Trace` 回调期间，同一数组的任何公开 API 都会以
`XERR_STATE` 拒绝；其他数组不受影响。元素借用地址在任何可能改变数量或容量的操作后失效。

`Data/ConstData` 返回当前活动元素组成的连续区借用，长度始终为 `Count`。空数组允许返回空
指针或仅用于零长度区间的实现地址；调用方不得访问容量内尚未初始化的槽。可写借用只允许按元素
类型正常修改已经初始化的值，不能用字节覆盖绕过值的 `Drop/Copy/Move` 生命周期。该入口用于
批量 I/O、队列和编解码器直接消费连续值，任何可能改变数组数量或容量的操作都会使旧借用失效。

可选的 `runtime_type_future` 模块提供拥有 `xfuture*` 消费端引用的标准类型描述。类型数组可以直接使用 `xrtTypeFuture()`，元素复制、移动和销毁会精确维护 Future 引用，并且不需要启用动态 Value。完整契约见 [Future 运行时类型](runtime_type_future.md)。

## 操作

`Reserve` 预留容量；`Resize` 的增长部分逐项执行 `Init`，缩小部分逆序执行 `Drop`；`Trim`
收缩容量；`Clear` 销毁元素但保留容量。

`Push`、`Insert` 和 `Set` 使用类型复制语义，失败时数组保持原值。来源允许是同一数组内的
准确活动元素起始地址；实现先记录元素下标，在扩容或移动后重新定位，不分配临时元素。备用
容量、过对齐分配填充和部分元素地址都不是有效来源。

`Take` 和 `Pop` 把元素移动到调用方已经初始化的同类型输出值，再从数组删除。输出不得与数组
存储相交。移动失败时数组和输出保持原值。

`Find` 使用类型比较操作并返回第一个下标；查询值遵循与复制来源相同的完整值边界规则。未命中
返回 `SIZE_MAX` 且不设置错误。元素类型不可比较时返回 `SIZE_MAX` 并设置
`XERR_UNSUPPORTED`。`Contains` 是其布尔便利层。

`Append` 要求元素类型身份相同，允许自追加，并在任一元素复制失败时销毁本轮新增元素、恢复
原数量。`Clone` 深复制数组结构和每一个元素值；`Concat` 把两个同类型数组拼接为新的深副本，
失败时不修改任一输入数组。

`Equals` 比较精确元素类型、数量、顺序和逐项内容。类型或数量不同只表示不相等，不设置错误；
元素类型不支持比较或比较回调失败时返回 `false` 并设置错误，因此需要区分失败的调用方应先
`xrtClearError()`，再检查 `xrtGetError()`。

## API 索引

```c
bool xrtTypedArrayInit(xtypedarray* array, const xrttype* item_type);
xtypedarray* xrtTypedArrayCreate(const xrttype* item_type);
void xrtTypedArrayUnit(xtypedarray* array);
void xrtTypedArrayDestroy(xtypedarray* array);

const xrttype* xrtTypedArrayItemType(const xtypedarray* array);
size_t xrtTypedArrayCount(const xtypedarray* array);
size_t xrtTypedArrayCapacity(const xtypedarray* array);
ptr xrtTypedArrayData(xtypedarray* array);
const void* xrtTypedArrayConstData(const xtypedarray* array);

bool xrtTypedArrayReserve(xtypedarray* array, size_t capacity);
bool xrtTypedArrayResize(xtypedarray* array, size_t count);
bool xrtTypedArrayTrim(xtypedarray* array);
void xrtTypedArrayClear(xtypedarray* array);

ptr xrtTypedArrayGet(xtypedarray* array, size_t index);
const void* xrtTypedArrayConstGet(const xtypedarray* array, size_t index);
bool xrtTypedArrayPush(xtypedarray* array, const void* item);
bool xrtTypedArrayInsert(xtypedarray* array, size_t index, const void* item);
bool xrtTypedArraySet(xtypedarray* array, size_t index, const void* item);
bool xrtTypedArrayRemove(xtypedarray* array, size_t index, size_t count);
bool xrtTypedArrayTake(xtypedarray* array, size_t index, ptr value);
bool xrtTypedArrayPop(xtypedarray* array, ptr value);

bool xrtTypedArraySwap(xtypedarray* array, size_t left, size_t right);
bool xrtTypedArrayReverse(xtypedarray* array);
size_t xrtTypedArrayFind(const xtypedarray* array, const void* item);
bool xrtTypedArrayContains(const xtypedarray* array, const void* item);

bool xrtTypedArrayAppend(xtypedarray* target, const xtypedarray* source);
xtypedarray* xrtTypedArrayClone(const xtypedarray* array);
xtypedarray* xrtTypedArrayConcat(
	const xtypedarray* left,
	const xtypedarray* right
);
bool xrtTypedArrayEquals(
	const xtypedarray* left,
	const xtypedarray* right
);

bool xrtTypedArrayTypeValidate(const xrttype* type);
const xrtinstanceops* xrtTypedArrayInstanceOps(void);
```

## 对象负载

`xrtTypedArrayInstanceOps()` 返回进程期稳定的共享实例操作表。对象数组类型描述应满足：

```c
const xrttype* Arguments[] = { xrtTypeInt64() };
xrttype ArrayType = {
	.Id = xrtTypeId(XRT_STR_LITERAL("app.array<int64>")),
	.Kind = XRT_TYPE_ARRAY,
	.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
	.Name = XRT_STR_INIT("array<int64>"),
	.AbiName = XRT_STR_INIT("app.array<int64>"),
	.Size = sizeof(ptr),
	.Align = _Alignof(ptr),
	.InstanceSize = sizeof(xtypedarray),
	.InstanceAlign = _Alignof(xtypedarray),
	.Ops = xrtObjectValueOps(),
	.InstanceOps = xrtTypedArrayInstanceOps(),
	.ArgumentCount = 1u,
	.Arguments = Arguments
};
```

在同时启用 `runtime_object` 时，`xrtObjectValueOps()` 负责数组对象引用槽；数组
`InstanceOps` 根据唯一泛型实参初始化负载、销毁全部元素，并逐项调用
`xrtTypeTraceValue`。因此数组元素持有的对象引用会成为对象图中的准确边，不需要动态值旁路表。

集成测试同时覆盖模块化与单头构建：一个 `array<自身对象引用>` 保存两个自身强引用
时，对象图会观察到两个独立槽位和两条边；释放外部根后，该自环能够被完整回收。

发布描述前调用 `xrtTypedArrayTypeValidate`。它检查类型类别、引用布局、泛型实参、负载大小、
对齐和准确实例操作表。

## 失败与线程

结构操作不是同一数组上的并发 API；多个线程可只读访问稳定数组，但任何写入都需要调用方同步。
模块不在元素回调期间持锁。扩容 OOM、元素初始化失败和元素复制失败均保持原有元素与所有权。

`xtypedarrayerror` 是错误域 `xrt.typed-array` 的稳定代码：

| 代码 | 常量 | 含义 |
| --- | --- | --- |
| 1 | `XTYPED_ARRAY_ERROR_ARGUMENT` | 空参数、未初始化数组或非法别名。 |
| 2 | `XTYPED_ARRAY_ERROR_TYPE` | 元素或对象数组类型不满足能力契约。 |
| 3 | `XTYPED_ARRAY_ERROR_RANGE` | 下标、区间或数量溢出。 |
| 4 | `XTYPED_ARRAY_ERROR_OPERATION` | 存储或元素生命周期操作失败。 |
| 5 | `XTYPED_ARRAY_ERROR_STATE` | 公开结构与元素类型布局不一致。 |

下层 `xrt.type`、`xrt.array` 或用户复制错误作为原因保留；OOM 的最终错误种类仍为
`XERR_MEMORY`。

## 历史资产

本模块保留旧版 `lib/typed_container.h` 中“元素生命周期由类型统一驱动”、自追加、克隆、
查找和对象图追踪等体系化能力，同时删除每个容器重复的 `RefCount/HeapOwned`、旧所有者模式、
1 基下标、静默 `void` 复制回调和失败后部分修改。底层扩容、过对齐、自引用定位和 OOM 原子性
直接复用已经压实的 `xarray` 实现与测试边界。
