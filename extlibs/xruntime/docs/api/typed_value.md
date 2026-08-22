# 类型值与动态值转换

`typed_value` 是运行时类型值、类型容器与不透明 `xvalue` 之间的可选适配层。类型事实层、
类型容器和 Value 核心都不反向依赖本模块；只有需要静态值与动态协议数据互操作的程序才启用。

```c
#include <xrt/typed_value.h>
```

## 裁剪层次

| 特性宏 | 依赖 | 能力 |
| --- | --- | --- |
| `XRUNTIME_FEATURE_TYPED_VALUE` | `runtime_type`、`value` | 单个类型值与动态值互转 |
| `XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE` | `runtime_type_string`、`typed_value` | 拥有型 `str` 与动态字符串互转 |
| `XRUNTIME_FEATURE_TYPED_ARRAY_VALUE` | `typed_value`、`typed_array`、`value_container` | 数组单元素操作与稠密数组互转 |
| `XRUNTIME_FEATURE_TYPED_LIST_VALUE` | `typed_value`、`typed_list`、`value_container` | 列表单元素操作与稀疏整数映射互转 |
| `XRUNTIME_FEATURE_TYPED_SET_VALUE` | `typed_value`、`typed_set`、`value_container` | 集合单元素操作与集合互转 |
| `XRUNTIME_FEATURE_TYPED_DICT_VALUE` | `typed_value`、`typed_dict`、`value_container` | 字典单元素操作与字符串键对象互转 |

四个容器桥接互不依赖。只启用数组桥接不会引入列表、集合或字典实现。

## 标量转换

`xrtValueToTyped` 把动态值转换到未初始化的输出槽。成功后输出是一个完整初始化且由调用方
拥有的类型值，最终必须调用 `xrtTypeDropValue`；失败时函数已经清理部分值，调用方不能再
销毁输出。`xrtValueFromTyped` 借用来源类型值并返回一个独立动态值。

默认转换只覆盖能够安全表达的 XRT 内建标量：null、bool、bool32、定宽整数、浮点、time、pointer
和 type ID。整数转换执行精确范围检查；`double` 只有可无损表示时才能转为 `float32`；
`uint64` 或 type ID 超过 `INT64_MAX` 时不能放入当前动态整数。

同时启用 `XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE` 时，`xrtTypeValue()` 表示的所有权槽也属于
内建转换：两个方向都深复制完整 Value 图，不共享可变容器 backing。基础 `typed_value` 不强制
依赖这项较重能力，未启用时仍可由应用转换器自行决定 Handle 类型的表示。

同时启用 `XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE` 时，`xrtTypeString()` 表示的拥有型零结尾 `str` 可以与动态字符串双向复制。动态字符串包含内嵌零时不能无损落入 `str`，转换会以 `XTYPED_VALUE_ERROR_CONVERT` 拒绝。该组合层不会改变基础 `typed_value` 的依赖闭包。

```c
xvalue* Source = xrtValueInt(120);
int8 Small;

if ( !xrtValueToTyped(Source, xrtTypeInt8(), &Small, NULL) ) {
	return false;
}
xrtTypeDropValue(xrtTypeInt8(), &Small);
xrtValueRelease(Source);
```

## 自定义转换

记录、拥有字符串、字节、枚举和应用句柄没有唯一的动态表示，因此不做隐式猜测。调用方可传入
`xvalueconverter`：

```c
typedef bool (*xvaluetotyped)(
	const xvalue* source,
	const xrttype* target_type,
	ptr target,
	ptr context
);

typedef xvalue* (*xvaluefromtyped)(
	const xrttype* source_type,
	const void* source,
	ptr context
);

typedef struct xvalueconverter {
	ptr Context;
	xvaluetotyped ToTyped;
	xvaluefromtyped FromTyped;
} xvalueconverter;
```

- `ToTyped` 接收已经按目标类型初始化的值槽；失败时仍须让该槽可安全销毁，并设置错误。
- `FromTyped` 只借用来源值，成功时返回调用方拥有的 `xvalue`。
- 转换器及 `Context` 只被借用，必须覆盖同步调用过程。
- 内建标量始终使用稳定的安全转换规则；转换器只处理没有内建表示的类型。

这种扩展方式不会修改全局注册表，也不会把 Value 回调塞进基础 `xrttype`，适合不同协议为同一
宿主类型选择不同表示。

## 单元素桥接

容器桥接特性同时提供单元素便利 API。它们负责临时值的对齐、初始化、转换和销毁，调用方不需要
为 `xrttype::Size/Align` 手工申请存储。常见标量和不超过 64 字节、对齐不超过平台自然最大
对齐的小记录使用内联临时槽，不发生堆分配；更大或特殊对齐类型自动回退到分配器。

数组：

```c
xrtTypedArrayPushValue(array, value, converter);
xrtTypedArrayInsertValue(array, index, value, converter);
xrtTypedArraySetValue(array, index, value, converter);
xrtTypedArrayGetValue(array, index, converter);
xrtTypedArrayTakeValue(array, index, converter);
xrtTypedArrayPopValue(array, converter);
xrtTypedArrayFindValue(array, value, converter);
xrtTypedArrayContainsValue(array, value, converter);
```

列表：

```c
xrtTypedListSetValue(list, key, value, converter);
xrtTypedListAppendValue(list, value, &key, converter);
xrtTypedListGetValue(list, key, converter);
xrtTypedListTakeValue(list, key, converter);
xrtTypedListFindValue(list, value, &key, converter);
xrtTypedListContainsValue(list, value, converter);
```

集合：

```c
xrtTypedSetAddValue(set, value, converter);
xrtTypedSetGetValue(set, value, converter);
xrtTypedSetHasValue(set, value, converter);
xrtTypedSetRemoveValue(set, value, converter);
xrtTypedSetTakeValue(set, value, converter);
```

字典：

```c
xrtTypedDictSetValue(dict, key, value, converter);
xrtTypedDictGetValue(dict, key, converter);
xrtTypedDictTakeValue(dict, key, converter);
```

`GetValue` 和 `TakeValue` 返回调用方拥有的动态值。`TakeValue` 先完成转换，再删除来源元素；转换
失败时容器保持不变。用户转换器执行期间容器进入回调门禁，因此不能利用转换回调重入并改变被
借用的元素。返回 `false`、`NULL` 或 `SIZE_MAX` 时，可通过当前 XRT 错误区分转换失败和正常未找到。

## 容器映射

| 类型容器 | 动态 Value |
| --- | --- |
| `xtypedarray` | `XVALUE_ARRAY` |
| `xtypedlist` | `XVALUE_INT_MAP` |
| `xtypedset` | `XVALUE_SET` |
| `xtypeddict` | `XVALUE_OBJECT` |

```c
xtypedarray* xrtTypedArrayFromValue(
	const xvalue* source,
	const xrttype* item_type,
	const xvalueconverter* converter
);
xvalue* xrtTypedArrayToValue(
	const xtypedarray* array,
	const xvalueconverter* converter
);

xtypedlist* xrtTypedListFromValue(
	const xvalue* source,
	const xrttype* item_type,
	const xvalueconverter* converter
);
xvalue* xrtTypedListToValue(
	const xtypedlist* list,
	const xvalueconverter* converter
);

xtypedset* xrtTypedSetFromValue(
	const xvalue* source,
	const xrttype* item_type,
	const xvalueconverter* converter
);
xvalue* xrtTypedSetToValue(
	const xtypedset* set,
	const xvalueconverter* converter
);

xtypeddict* xrtTypedDictFromValue(
	const xvalue* source,
	const xrttype* item_type,
	const xvalueconverter* converter
);
xvalue* xrtTypedDictToValue(
	const xtypeddict* dict,
	const xvalueconverter* converter
);
```

`FromValue` 先创建独立结果，再逐项转换；任一转换、复制、散列或分配失败都会销毁完整临时结果，
不会修改来源，也不会返回半成品。`ToValue` 同样构造独立动态容器。列表保留完整 `int64` 键，
字典键按长度复制，允许内嵌零，不执行 `strlen`。

动态容器来源通过 backing 快照读取，因此转换器修改原动态容器不会破坏本次遍历；本次结果仍反映
转换开始时的内容。类型容器来源不提供写时复制，调用用户转换器期间会进入回调门禁；转换器重入
同一类型容器的任何 API 都以 `XERR_STATE` 失败。array、set、dict 以及支持容量概念的动态目标
容器会按来源数量预留容量；基于有序树的稀疏 `int64` 映射没有无意义的 Reserve 操作。

转换是语义深转换，不是类型容器对象身份装箱。需要把宿主对象放入 Value 时使用
`runtime_value` 的对象桥接；需要共享类型容器对象身份时，应把容器作为 `xrtobject` 负载。

## 线程与错误

转换函数只读来源并新建结果；来源容器仍须遵守各自并发规则。用户转换器同步执行；对动态来源的
修改采用快照语义，对当前类型来源的重入被明确拒绝。

错误域为 `xrt.typed-value`：

| 代码 | 常量 | 含义 |
| --- | --- | --- |
| 1 | `XTYPED_VALUE_ERROR_ARGUMENT` | 空来源、输出或非法调用参数 |
| 2 | `XTYPED_VALUE_ERROR_TYPE` | 类型不匹配、无默认表示或需要转换器 |
| 3 | `XTYPED_VALUE_ERROR_RANGE` | 数值不能被目标表示 |
| 4 | `XTYPED_VALUE_ERROR_CONVERT` | 用户转换器或值生命周期失败 |
| 5 | `XTYPED_VALUE_ERROR_CONTAINER` | 动态容器类别或容器操作失败 |

下层错误作为原因保留；OOM 最终保持 `XERR_MEMORY`。失败回收期间，即使用户类型的销毁操作
写入了次生错误，也不会覆盖最初的转换失败及其原因链。

## 历史资产

本模块承接旧版 `lib/value.h` 中 typed array/list/set/dict 与动态值往返的有效能力，同时修订旧版
把转换操作塞进全局类型描述、使用所有者模式参数和失败原因不明确的问题。旧版已经验证的整数
键、字符串键长度、集合去重、完整失败清理和 typed `xvalue` 用例继续作为测试来源。
