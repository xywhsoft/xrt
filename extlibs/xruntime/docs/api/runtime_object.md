# 运行时对象

`runtime_object` 在 `runtime_type` 的生命周期描述之上提供引用计数堆对象和弱引用。
它面向宿主对象、C 扩展对象及 native-backed 实例，不提供对象负载内部的锁或字段系统。该模块属于 `xruntime` 扩展。

## 启用与依赖

启用 `XRUNTIME_FEATURE_RUNTIME_OBJECT` 会依赖 `XRUNTIME_FEATURE_RUNTIME_TYPE`。公共头文件为：

```c
#include <xrt/runtime_object.h>
```

对象模块只依赖类型与核心内存、错误能力，不依赖动态值、容器、任务或网络模块。

## 对象布局与所有权

`xrtobject` 是不透明控制块。对象头、对齐填充和负载由一次 XRT 堆分配承载；负载地址
满足 `xrttype.InstanceAlign`，包括高于平台默认堆对齐的 32、64 字节布局。

类型描述及其 `InstanceOps`、方法表等借用数据必须覆盖所有强引用及最终 `Drop`；只剩过期弱引用
后，控制块不再访问类型描述。对象负载长度由 `xrtObjectSize` 返回，类型操作只处理
`InstanceSize` 声明的基础实例区域；可变尾随部分由具体类型自行解释和销毁。

```c
xrtobject* Object = xrtObjectCreate(&Type);

if ( Object == NULL ) {
	return false;
}
MyPayload* Payload = (MyPayload*)xrtObjectData(Object);
Payload->Value = 42;
xrtObjectUnref(Object);
```

`xrtObjectCreate` 使用 `InstanceSize`。`xrtObjectCreateSized` 接受不小于该值的真实负载
长度。分配区先清零，再调用 `xrtTypeInitInstance`；初始化失败时立即回收对象且不会调用
`Drop`，因此失败的 `Init` 回调必须自行释放部分取得的资源并设置 XRT 错误。

## 强引用

`xrtObjectCreate` 和成功的 `xrtWeakLock` 各返回一个强引用。`xrtObjectRef` 增加引用，
但调用方在调用期间必须已经持有有效强引用，不能用悬空裸指针复活对象。
`xrtObjectUnref` 消耗一个强引用，允许传入空指针；最后一次释放在当前线程执行一次
类型 `Drop`，随后对象进入不可提升状态。

## 对象值

对象负载和保存对象的强引用槽是两种不同数据。`InstanceOps` 处理 `InstanceSize` 字节的
负载；字段、参数和容器元素中的 `xrtobject*` 槽位由 `xrtObjectValueOps()` 处理。

需要作为普通值保存的类类型应声明 `XRT_TYPE_FLAG_COPYABLE` 和
`XRT_TYPE_FLAG_RELOCATABLE`，并设置：

```c
Type.Ops = xrtObjectValueOps();
Type.InstanceOps = &MyInstanceOps;
```

标准对象值操作支持空值初始化、失败原子的强引用复制、移动、释放、地址比较、地址散列和
强引用追踪。复制与移动会替换目标原值；销毁后槽位恢复为空。比较和散列只在当前进程内有意义。

`xrtObjectType`、`xrtObjectData`、`xrtObjectConstData` 和 `xrtObjectSize` 返回借用信息，
只允许在调用方持有强引用期间使用。XRT 不序列化负载访问，线程安全由具体对象定义。

`xrtObjectRefCount` 返回调用瞬间的强引用数量，`xrtObjectUnique` 判断该数量是否为一。
两者适合实现写时复制、诊断和宿主对象内省；并发环境中结果返回后可能立即变化，
不能代替所有权和同步。传入空对象会设置 `xrt.object` 引用错误。

启用独立的 `runtime_object_graph` 后，对象控制块会增加图归属和终结状态；未启用时这些
字段完全不进入布局。普通最后引用仍自动销毁负载，并会先从所属对象图摘除。

## 弱引用

`xrtweak` 是可放在栈、结构或容器中的小型值。首次使用前必须清零，然后遵守以下值语义：

| 操作 | 契约 |
| --- | --- |
| `xrtWeakInit` | 从可选存活对象初始化空目标 |
| `xrtWeakCopy` | 复制并替换目标，失败时保留原值，自复制为空操作 |
| `xrtWeakMove` | 移动并替换目标且清空源，自移动为空操作 |
| `xrtWeakSet` | 先保留新控制块，再替换并释放旧控制块，失败时保留原值 |
| `xrtWeakUnit` | 释放并清空弱引用，允许空指针和重复清理 |
| `xrtWeakExpired` | 返回当前瞬间的过期状态 |
| `xrtWeakLock` | 尝试取得新强引用，过期时返回空且不设置错误 |

```c
xrtweak Weak = { 0 };

if ( !xrtWeakInit(&Weak, Object) ) {
	return false;
}
xrtobject* Locked = xrtWeakLock(&Weak);
if ( Locked != NULL ) {
	use_object(Locked);
	xrtObjectUnref(Locked);
}
xrtWeakUnit(&Weak);
```

`Expired` 只适合提示和快速分支，其结果返回后对象状态仍可能变化；需要使用对象时必须
调用 `Lock`。不同弱引用值可并发复制、查询、提升和销毁同一控制块。对同一个
`xrtweak` 变量执行 `Init/Move/Set/Unit` 时必须由调用方同步；并发读取的源弱引用也不能
同时被修改。

需要弱引用的 native 句柄统一放入 `xrtobject` 负载，由 `InstanceOps.Drop` 释放，不再为 Value Handle 建立第二套 weakable 标志和控制块。这样宿主对象和 C 扩展对象共享同一强弱生命周期；只需要不可变动态值所有权、且不需要弱引用的裸句柄继续使用 `xrtValueHandle`。

## 错误

`xobjecterror` 定义运行时对象模块的稳定错误代码。参数、类型、长度、引用状态、弱引用值和初始化错误使用稳定域 `xrt.object`：

| 代码 | 常量 |
| --- | --- |
| 1 | `XOBJECT_ERROR_TYPE` |
| 2 | `XOBJECT_ERROR_SIZE` |
| 3 | `XOBJECT_ERROR_REFERENCE` |
| 4 | `XOBJECT_ERROR_WEAK` |
| 5 | `XOBJECT_ERROR_INITIALIZE` |

类型验证和初始化失败会作为原因链接入对象错误。内存不足保留无分配的 `XERR_MEMORY`
错误。弱引用已经过期是正常状态，`xrtWeakLock` 不会覆盖当前错误；需要判断本次调用
是否产生错误时，应先调用 `xrtClearError`。

## 历史资产

旧版 `lib/type.h` 中成熟的强弱引用控制块、一次性析构和对象类型绑定思想被保留。
新版把这些契约从动态值、容器和调用器中独立出来，修复弱状态非原子读取，并在不增加
分配次数的前提下支持过对齐负载。旧版类型测试中的生命周期边界已扩展为常规、OOM、
并发、示例和单头测试。
