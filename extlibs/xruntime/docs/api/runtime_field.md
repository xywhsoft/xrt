# 运行时字段

`runtime_field` 提供类与记录的公开字段元数据、继承查询和安全地址定位。启用宏为
`XRUNTIME_FEATURE_RUNTIME_FIELD`，依赖 `runtime_type`。实现不分配内存、不维护全局状态，
描述符由声明方持有并在使用期间保持地址稳定且不可修改。

带多个命名载荷的枚举变体使用一个 `XRT_TYPE_RECORD` 作为 `PayloadType`，记录的
字段继续由本模块验证和查询；枚举元数据不另建一套重复字段表。

```c
#include <xrt/runtime_field.h>
```

## 分层边界

字段层只描述 C、XRT 与上层语言都能理解的事实：名称、值类型、负载偏移和只读属性。
它不决定 public/private 等语言可见性，不实现属性访问器，不隐式复制字段，也不替对象图
推断强弱引用。

`xrtinstanceops.Trace` 是对象负载直接强引用的精确热路径；`xrttypeops.Trace` 只处理字段
或容器元素中的 C ABI 值。引用类型的 `xrttype::Size` 描述对象指针槽位，`InstanceSize`
描述对象负载。宿主或代码生成器应从同一份字段信息分别生成字段表、访问代码、实例 `Drop` 和
实例 `Trace`，而不是在收集期间反射扫描字段。

## 描述符

```c
xrtfielddesc Fields[] = {
	{ XRT_STR_INIT("id"), xrtTypeUInt64(), offsetof(user, Id), 0u },
	{
		XRT_STR_INIT("enabled"), xrtTypeBool(),
		offsetof(user, Enabled), XRT_FIELD_FLAG_READONLY
	}
};
xrtfieldtable Table = { 2u, Fields };

Type.Fields = &Table;
```

`xrtfielddesc::Type` 描述字段在 C ABI 中的值形态，因此布局验证使用该类型的 `Size` 和
`Align`，而不是 `InstanceSize`。`XRT_FIELD_FLAG_READONLY` 是供宿主绑定、调试器和通用
工具解释的事实；底层 `xrtFieldData` 仍返回原始可写地址，便于构造、反序列化和运行时内部
实现。访问策略由上一层执行。

字段表允许为空。静态字段、计算属性、语言可见性和注解属于对应语言或独立属性扩展，
不进入通用实例存储字段契约。

## 验证

`xrtTypeFieldsValidate` 先验证类型与完整继承链，再检查：

- 字段表数量与数组指针匹配；
- 字段名称非空且在完整继承链中唯一；
- 字段类型是有效的运行时类型；
- 标志均已定义；
- 偏移满足字段对齐，存储区间位于声明类型负载中；
- 派生类字段不覆盖基类负载，局部字段互不重叠。

字段隐藏被明确拒绝，使 `xrtTypeFindField` 在 C、XRT 和外部宿主间保持唯一含义。若上层需要
同名属性覆盖，应由属性或方法派发表达，不应制造两个同名存储槽位。

`xrtTypeValidate` 只验证基础类型描述；启用字段模块并提供 `Fields` 后，发布或注册类型前还应
调用一次 `xrtTypeFieldsValidate`。这条边界避免基础类型模块反向依赖可裁剪的字段模块。

## 查询

`xrtTypeFieldCount` 返回本类型与全部基类的字段总数。`xrtTypeField` 的稳定顺序是基类字段在前、
派生类字段在后，每个表内部保持声明顺序。`xrtTypeFindField` 按精确字节名称查询；未找到是正常
结果，不设置错误。`xrtTypeFieldOwner` 返回字段的声明类型。

```c
const xrtfielddesc* Field = xrtTypeFindField(
	&Type, XRT_STR_LITERAL("id")
);

if ( Field == NULL ) {
	return false;
}
printf("offset=%zu\n", Field->Offset);
```

查询函数零分配，并以最多 256 层的有界继承遍历避免损坏描述造成无限循环。正常查询假定类型
已经通过验证；它们仍会拒绝空参数、损坏的字段表、计数溢出和越界下标。

## 数据访问

`xrtFieldConstData` 和 `xrtFieldData` 要求字段描述符的准确地址属于给定类型或其基类字段表，
随后返回 `instance + Offset` 的借用地址。来自另一份等值表的描述符也会被拒绝，避免调用方用
未经验证的偏移访问负载。

```c
uint64 Id = *(const uint64*)xrtFieldConstData(&Type, Field, &Value);
```

调用方必须传入至少具有 `Type.InstanceSize` 字节且满足 `Type.InstanceAlign` 的有效实例负载。
地址只在实例存活且布局不变时有效。只读标志不会阻止运行时内部调用 `xrtFieldData`。

## 动态字段

需要在运行期增加、删除或枚举属性时，启用 `XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD` 并使用 `xrtdynamicfields`。动态字段是独立对象图节点，不占用静态实例偏移，也不改变 `xrtfieldtable` 的不可变布局契约。完整 API、所有权和自引用回收说明见 `docs/api/runtime_dynamic_field.md`。

## 线程与性能

字段描述符完全不可变，所有验证和查询函数均可在多个线程并发调用。模块没有锁、堆分配、
缓存或隐藏注册表。验证用于类型发布边界，复杂度最坏为 `O(F^2 + D)`；常规计数、下标和名称
查询为 `O(F + D)`，字段地址定位为 `O(F + D)`。生成代码应直接使用已知偏移，不在业务热路径
重复反射查询。

## 错误

字段错误使用稳定域 `xrt.field`：

| 代码 | 常量 | 含义 |
| --- | --- | --- |
| 1 | `XFIELD_ERROR_DESCRIPTOR` | 字段表、类型、名称、布局、标志或继承契约无效。 |
| 2 | `XFIELD_ERROR_LOOKUP` | 空查询参数、字段计数溢出或下标越界。 |
| 3 | `XFIELD_ERROR_ACCESS` | 字段不属于类型或实例负载为空。 |

无效字段类型产生的 `xrt.type` 错误会作为 `xrt.field/validate` 的原因保留。未找到字段不覆盖
当前执行上下文中的错误。

## 历史资产

本模块复用旧版 `lib/type.h` 的字段名称、类型和偏移元数据思想，以及
`test/test_runtime_type.h` 已验证的类型布局边界；它去掉旧版语言策略与底层布局混杂的问题，
并补充继承重名、基类覆盖、对齐、越界、重叠、准确描述符归属、单头和裁剪测试。
