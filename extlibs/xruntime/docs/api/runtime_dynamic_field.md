# 运行时动态字段

`runtime_dynamic_field` 为宿主对象、脚本对象和扩展对象提供独立的动态字段节点。启用宏为 `XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD`，直接建立在 `runtime_field`、`typed_dict`、`runtime_object_graph` 和 `runtime_value_trace` 之上。该模块属于 `xruntime` 扩展。

```c
#include <xrt/runtime_field.h>
```

## 分层定位

静态字段由 `xrtfielddesc` 描述固定实例布局；动态字段由 `xrtdynamicfields` 保存运行期名称和值。二者共享“字段”概念，但不混合存储：

- 静态字段零分配、按偏移访问，适合编译器生成的类和记录布局。
- 动态字段复制名称、保持插入顺序，适合运行期增加、删除和枚举属性。
- 动态字段表自身是 `xrt.DynamicFields` 对象图节点，而不是藏在类对象中的不可见 Value 字典。
- 载荷直接复用 `xtypeddict<Value>`，不重复实现哈希表、迭代器或类型生命周期。

该边界使宿主对象只需持有一个动态字段对象强引用。字段值中的运行时对象由 `xrtTypeValue()` 和 `xrtValueTraceRuntimeObjects()` 继续追踪，自引用和互引用由对象图在安全点回收。

## 创建与容量

```c
xrtdynamicfields* Fields = xrtDynamicFieldsCreate();

if ( Fields == NULL ) {
	return false;
}
xrtDynamicFieldsReserve(Fields, 16u);
xrtDynamicFieldsUnref(Fields);
```

`Create/Ref/Unref` 管理对象强引用。`Count/Capacity/Reserve/Trim/Clear` 对应类型字典的容量和清理能力。动态字段对象不是线程安全容器；同一对象的并发读写由调用方同步。

## 读取与写入

`xrtDynamicFieldsGet` 返回只读借用，字段删除、替换或字段对象释放后失效。`xrtDynamicFieldsGetRef` 返回同一 Value 的新引用，适合语言对象、观察者和其他需要保留别名语义的路径。`xrtDynamicFieldsCopy` 返回深复制的独立 Value 图。字段缺失是正常结果，三种读取都返回 `NULL` 且不设置错误。

```c
xvalue* Value = xrtValueString(XRT_STR_LITERAL("xrt"));

if ( !xrtDynamicFieldsSetTake(
	Fields, XRT_STR_LITERAL("name"), &Value
) ) {
	xrtValueRelease(Value);
	return false;
}
```

三种写入语义为：

- `Set` 深复制完整来源图，成功和失败都不改变来源，包括嵌套容器别名。
- `SetTake` 先构造独立图，提交成功后才释放并清空来源；OOM 和参数错误保持来源不变。
- `SetNew` 无论成功失败都消费临时值，适合单行构造。

动态字段故意隔离 Value 容器 backing。这样每个字段节点追踪到的对象边都对应它实际拥有的引用，不会因两个字段共享不可见 backing 而重复计算对象图入边。运行时对象值的外壳会复制，但对象身份保持不变。

标量、字符串和字节值的深复制只增加不可变 Value 外壳引用，不复制字符串或字节数据；只有容器图和可克隆 Handle 需要建立独立所有权。即使调用方在 `SetTake` 前保留了来源 Value，字段内对象边与字段外别名也仍能被收集器准确区分，字段外别名会继续构成外部根。

`SetRef/SetRefTake/SetRefNew` 是显式共享路径。它们把同一个 Value 外壳保存在字段中，后续对该 Value 的原地修改能被其他引用观察到；该路径不会伪装成深复制，适合 xlang `object` 动态字段和有意共享的宿主对象。普通 `Set/SetTake/SetNew` 仍隔离完整可变 Value 图。

`Remove` 删除并释放字段；`Take` 把字段拥有的 Value 移交给调用方。`StoredName` 返回内部规范名称借用。名称使用完整 `xstrview` 字节边界，允许空名称和内嵌零，不依赖 `strlen`。

## 迭代与集合

`IterBegin/IterRBegin` 按正向或逆向插入顺序启动迭代。迭代器保留字段对象，因此调用方可以释放原始引用。结构修改会使旧迭代器失效，下一次 `IterNext` 返回 `NULL` 并设置 `XERR_STATE`；自然结束不设置错误。始终调用 `IterEnd` 释放迭代器持有的对象引用。

`Keys/Values/Items` 是反射、脚本宿主和 FFI 常用的高层入口：

- `Keys` 返回字段名数组。
- `Values` 返回独立字段值数组。
- `Items` 返回 `[name, value]` 二元数组的数组。

三个函数都保持字段插入顺序，结果由调用方通过 `xrtValueRelease` 释放。

## 合并与克隆

`xrtDynamicFieldsMerge(target, source, replace)` 先为目标已有字段建立 COW 工作快照，再深复制来源新增或替换的完整 Value 图，最后一次性提交工作字典；任一分配、克隆或迭代失败都保持两端不变。目标快照在提交时替代旧存储，不会留下额外 backing 共享。`replace == false` 保留目标冲突值，`replace == true` 使用来源值。

`xrtDynamicFieldsClone` 深复制名称和 Value 图，返回新的字段对象。容器值不共享 backing；运行时对象值仍指向相同对象身份并各自拥有强引用。

## Value 转换

`xrtDynamicFieldsToValue` 生成按字段插入顺序保存的独立 Value Object，`xrtDynamicFieldsFromValue` 从 Value Object 深复制名称和值并创建新的字段对象。两者用于 JSON、XSON、调试器和语言桥接，不改变字段对象或来源 Value；非 Object 输入会报告 `xrt.dynamic-field` 类型错误。

## API 索引

| API | 契约 |
| --- | --- |
| `xrtDynamicFieldsType` | 返回进程期稳定的 `xrt.DynamicFields` 类型描述借用。 |
| `xrtDynamicFieldsCreate` | 创建空字段对象，失败返回 `NULL`。 |
| `xrtDynamicFieldsRef` | 增加对象强引用并返回原指针。 |
| `xrtDynamicFieldsUnref` | 释放一个强引用，接受 `NULL`。 |
| `xrtDynamicFieldsCount` | 返回字段数量；对象无效时返回零并设置错误。 |
| `xrtDynamicFieldsCapacity` | 返回当前字典容量。 |
| `xrtDynamicFieldsClear` | 释放全部字段值并保留可复用存储。 |
| `xrtDynamicFieldsReserve` | 预留至少指定字段容量，失败不改变可见内容。 |
| `xrtDynamicFieldsTrim` | 释放多余容量，不改变字段顺序和值。 |
| `xrtDynamicFieldsHas` | 判断精确名称是否存在；缺失不设置错误。 |
| `xrtDynamicFieldsGet` | 返回字段值的只读借用；缺失返回 `NULL`。 |
| `xrtDynamicFieldsGetRef` | 返回字段中同一 Value 的新引用；缺失返回 `NULL`。 |
| `xrtDynamicFieldsCopy` | 返回字段完整 Value 图的独立所有权。 |
| `xrtDynamicFieldsStoredName` | 返回内部规范字段名借用，字段修改后可能失效。 |
| `xrtDynamicFieldsSet` | 深复制来源图并失败原子地设置字段。 |
| `xrtDynamicFieldsSetTake` | 成功后消费并清空来源槽，失败保持来源不变。 |
| `xrtDynamicFieldsSetNew` | 无论成功失败都消费传入的临时 Value。 |
| `xrtDynamicFieldsSetRef` | 保留同一 Value 身份并设置字段。 |
| `xrtDynamicFieldsSetRefTake` | 成功后把同一 Value 身份移交给字段并清空来源槽。 |
| `xrtDynamicFieldsSetRefNew` | 无论成功失败都消费共享 Value 临时值。 |
| `xrtDynamicFieldsRemove` | 删除并释放字段；缺失返回 `false` 且不设置错误。 |
| `xrtDynamicFieldsTake` | 删除字段并把 Value 所有权移交给调用方。 |
| `xrtDynamicFieldsIterBegin` | 启动正向插入顺序迭代并保留字段对象。 |
| `xrtDynamicFieldsIterRBegin` | 启动逆向插入顺序迭代并保留字段对象。 |
| `xrtDynamicFieldsIterNext` | 返回下一字段值借用和可选名称；自然结束不设置错误。 |
| `xrtDynamicFieldsIterEnd` | 结束迭代并释放字段对象保留，可用于已结束迭代器。 |
| `xrtDynamicFieldsMerge` | 事务合并来源，按 `replace` 选择冲突策略。 |
| `xrtDynamicFieldsClone` | 深复制字段对象和完整 Value 图。 |
| `xrtDynamicFieldsKeys` | 返回按插入顺序排列的字段名 Value 数组。 |
| `xrtDynamicFieldsValues` | 返回按插入顺序排列的独立字段值数组。 |
| `xrtDynamicFieldsItems` | 返回按插入顺序排列的 `[name, value]` 数组。 |
| `xrtDynamicFieldsToValue` | 深复制全部字段为独立 Value Object。 |
| `xrtDynamicFieldsFromValue` | 从 Value Object 深复制创建动态字段对象。 |

## 错误

`xdynamicfielderror` 定义稳定错误代码，错误域为 `xrt.dynamic-field`：

| 代码 | 常量 | 含义 |
| --- | --- | --- |
| 1 | `XDYNAMIC_FIELD_ERROR_ARGUMENT` | 空对象、空值槽或非法名称视图。 |
| 2 | `XDYNAMIC_FIELD_ERROR_TYPE` | 对象不是精确的 `xrt.DynamicFields` 类型。 |
| 3 | `XDYNAMIC_FIELD_ERROR_OPERATION` | 分配、复制、移动、合并或集合构造失败。 |
| 4 | `XDYNAMIC_FIELD_ERROR_STATE` | 引用、载荷或迭代状态无效。 |

下层 `xrt.object`、`xrt.typed-dict`、`xrt.runtime-value` 和 `xrt.value` 错误作为原因保留，OOM 的最终种类保持 `XERR_MEMORY`。

## 历史资产

该模块复用了旧 XRT 的 Value 表、复制键、插入顺序和容器边界，并补齐通用对象的 get/set/remove/clear/keys/values/items 能力。动态字段不再是对象负载中的普通 `xvalue` 表，也不再由收集器递归猜测其内部所有权；字段表和运行时对象引用通过公开类型契约进入对象图。

范例：`examples/runtime/dynamic_field/main.c`。
