---
num: 114
slug: xruntime
title: xruntime（一）：类型系统
volume: 卷十一 其他扩展库
type: practice
lead: ABI 名派生的稳定类型 ID、22 种运行时类别、值操作表与单继承、注册表与转换——C、XRT 与宿主语言共享的类型事实层。
api: xruntime-runtime_type, xruntime-runtime_convert
---

## 导读

卷十一最后一族：**xruntime**——建立在 XRT 之上的运行时模型扩展（类型描述/对象/调用/typed 容器），为宿主语言（脚本引擎、插件系统、序列化框架）提供 C 与 XRT 共享的**类型事实层**。本章讲地基：**类型描述**（`xrttype`——Id/Kind/ABI 名/大小对齐/操作表/继承/字段方法元数据的完整描述结构）；**类型身份**（`xrtTypeId(ABI 名)` 派生稳定非零 ID——不用随意分配的整数；`TypeSame` 比较 ID+ABI 名、`TypeIsA` 沿单继承链）；**22 种运行时类别**（INVALID 到 CLASS/ENUM 的稳定存储类别——描述存储不描述语法）；**注册表与转换**（类型注册表的进程级事实集合；`runtime_convert` 的可转换性判定与值转换）。核心纪律：**模块不拥有任何描述符**——全部描述注册与查询期间地址稳定不可修改（借用元数据，进程期生命）。

## 引入

为什么宿主语言需要"类型事实层"？设想你在 C 里嵌一个脚本引擎：脚本里的 `Counter` 类要被 C 侧的调试器打印（需要类型名与字段表）、被序列化框架遍历（需要值操作：复制/比较/散列/析构）、被垃圾回收器追踪（需要强引用枚举）、被另一个脚本模块继承（需要基类链）——**每个能力都需要关于类型的结构化知识**，散落在各处的 `switch(type)` 就是维护地狱。`xrttype` 把这些知识标准化为一张描述表：脚本引擎注册它、XRT 容器消费它、调试器读它——三方共享同一份事实。

设计的两个关键决定值得注意。**其一：ID 由 ABI 名派生**——`xrtTypeId("app.Counter")` 生成稳定 ID，自定义类型的 Id 必须等于该结果——跨模块跨进程的类型等价性有据可查（随意分配的整数在第二个模块加载时就冲突了）。**其二：描述借用不拥有**——模块不持有描述符内存，调用方保证注册与查询期间的地址稳定与不可变——进程期静态描述（C 的天然形态）零负担，动态生成的描述由宿主管理生命。

## 概念

### 类型描述：字段全表

| 字段 | 契约 |
| --- | --- |
| `Id`/`AbiName` | 稳定 ID（必须等于 `xrtTypeId(AbiName)`）与非空跨模块唯一规范名 |
| `Kind` | 22 种稳定存储类别之一 |
| `Name` | 非空显示名（本地化展示可用） |
| `Size`/`Align` | C ABI 值的大小与二次幂对齐 |
| `InstanceSize`/`InstanceAlign` | 堆实例负载的大小与对齐（引用类型分离声明） |
| `Ops` | 值生命周期操作表（复制/比较/散列/格式化/强引用追踪——可选） |
| `InstanceOps` | 实例负载 Init/Drop/Trace（可选——第 115 章对象用） |
| `Base` | 类基类（仅 CLASS 可继承；不能继承 FINAL；深度上限 255） |
| `Arguments`/`Fields`/`Methods`/`Metadata` | 借用的泛型实参/字段表/方法表/自定义元数据 |

**引用类型的硬规则**：`Size == sizeof(ptr)` 且 `Align` 等于指针对齐——引用在 C ABI 里就是一个指针，对象负载由 `InstanceSize` 另行描述（值与实例的两层分离——第 115 章对象模型的地基）。`xrtTypeValidate` 全量校验描述——不合法的描述进不了系统（启动期拦截的又一实例）。

### 22 种类别：存储类别不是语法类别

`xrttypekind` 表达**稳定的运行时存储类别**——"这个值在内存里是什么形态"，不表达语言可见性或语法别名（public/private、typedef 这类是语言层的事）：INVALID/NULL/BOOL/SIGNED_INT/UNSIGNED_INT/FLOAT/STRING/BYTES/TIME/POINTER/CALLABLE/ARRAY/LIST/SET/DICT/RECORD/HANDLE/TYPE/FUTURE/CLASS/ENUM——从原始量到容器到类引用的完整谱系。**XRT 内建类型**（`xrtTypeInt64()`/`xrtTypeString()`/`xrtTypeCallable()` 等）返回进程期规范描述——宿主注册类型也应使用进程期描述（第 117 章容器契约的"指针相等"要求）。

### 值操作表：Ops 的六件事

`Ops` 描述**值**（C ABI 存储的那个东西）的行为：复制（失败原子）、比较（序）、散列（与比较一致）、格式化（调试打印）、强引用追踪（GC 配合——值里若持有对象引用要能枚举）。`InstanceOps` 描述**实例负载**（Init/Drop/Trace）——第 115 章展开。两层操作表对应值/实例的两层分离。

### 注册表：进程级类型事实集合

`xrtTypeRegistryCreate` 族（Add/At/FindId/FindName）——类型描述的注册与按名查找；示例的 `RegistryFindName(AbiName)` 即"按规范名找描述"的注册表消费。注册表不复制描述（借用）——描述的生命周期仍归声明方。

### 转换：runtime_convert

`runtime_convert` 在类型事实之上回答"这个值能变成那个类型吗"：`xrtTypeCanConvert`（可转换性——精确匹配/拓宽）、`xrtTypeCanWiden`（数值拓宽如 int32→int64）、`xrtTypeConvert`（执行转换）、`xrtTypeToString`/`TypeFormat`（值→文本）。转换层的意义：宿主语言的动态类型系统（"把这任意值当 int 用"）有了**声明式基础**——判定与执行分离，错误路径明确。

### xruntime 在扩展库家族中的位置

```diagram flow
- 类型系统（本章）：xrttype 事实层——22 类别/操作表/继承/注册表/转换
- 对象与对象图（第 115 章）：引用计数对象/弱引用/安全点环收集
- 动态调用（第 116 章）：签名/调用帧/多返回值——callable 环境
- typed 容器（第 117 章）：类型描述驱动的容器族——宿主值的容器面
```

## 示例

### 第一个完整程序：类型注册与值操作

下面的程序来自 `examples/runtime/type`——描述、注册、值操作的完整闭环：

```embed path="extlibs/xruntime/examples/runtime/type/main.c" title="extlibs/xruntime/examples/runtime/type/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/type/main.c -lws2_32 -liphlpapi
（输出类型名、稳定 ID、大小、比较与散列结果的自检行）
```

**刚才发生了什么。** ① 类型描述注册进注册表、`RegistryAt(0)` 与 `FindName(AbiName)` 双路取回——**按规范名的类型查找**是宿主模块对接的标准姿势（脚本模块 A 注册 Counter、模块 B 按名取用）。② `xrtTypeCompareValue(&Type, &左, &右, &结果)`——类型描述驱动的**通用值比较**：不需要知道值是什么类型，描述表里的 Ops 说了算（散列同理——动态值入哈希容器的底层）。③ 打印的 Id/Size 来自描述——`TypeValidate` 保证了它们的自洽（引用类型 Size=sizeof(ptr) 等）。**这一行输出就是"类型事实层"的可执行证明**：C 代码拿着描述表操作了它不认识的值。

### 第二个完整程序：转换层

第二个程序来自 `examples/runtime/convert`——转换判定的消费形态：

```embed path="extlibs/xruntime/examples/runtime/convert/main.c" title="extlibs/xruntime/examples/runtime/convert/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/convert/main.c -lws2_32 -liphlpapi
（输出转换判定与执行结果的自检行）
```

**刚才发生了什么。** ① `CanConvert`/`CanWiden` 的判定形态——宿主动态类型的"这值能这么用吗"检查；widen（无损拓宽）与普通 convert（可能有损）是两个判定——精确语义不混淆。② `Convert` 的执行——判定过了再执行，失败路径明确（不是"试试看"的隐式转换）。③ `convert_string` 示例是文本方向的配套（字符串↔类型的桥——`runtime_type_string`/`type_future` 两契约卡的实体）。这套判定/执行分离正是第 26 章数字解析"先验后转"的运行时类型版。

## 契约

- **描述借用**：模块不拥有描述符；注册与查询期间地址稳定不可修改；进程期静态描述零负担。
- **ID 派生**：自定义类型 Id 必须等于 `xrtTypeId(AbiName)`；TypeSame 比较 Id+ABI 名双因子。
- **继承规则**：仅 CLASS 可继承；不能继承 FINAL；派生大小对齐不小于任一基类；深度上限 255。
- **引用硬规则**：引用类型 Size=sizeof(ptr)、Align=指针对齐；负载由 InstanceSize 分离描述。
- **类别语义**：22 种是稳定存储类别——不表达语言可见性与语法别名。
- **操作两层**：Ops（值：复制/比较/散列/格式化/追踪）与 InstanceOps（实例：Init/Drop/Trace）分离。
- **校验先行**：TypeValidate 全量校验——不合法描述不进系统。
- **内建描述**：`xrtTypeInt64()` 等返回进程期规范描述——容器操作要求描述指针相等（第 117 章）。
- **注册表**：Add/Remove/At/FindName；不复制描述。
- **转换**：CanConvert/CanWiden 判定与 Convert 执行分离；ToString/Format 文本方向。
- **裁剪**：`XRUNTIME_FEATURE_RUNTIME_TYPE` 只依赖 core——最小地基。

## 避坑

### 坑 1：自定义类型随手分配 Id

症状：两个模块各自 `Id = 42` 的类型相遇——TypeSame 撞车，容器与调用层行为错乱。

原因：Id 的稳定性来自 **ABI 名派生**——同规范名必同 Id（跨模块跨进程）；随意整数没有这个保证。

```c bad
xrttype Type = { .Id = 42u, ... };   /* 随手分配：撞车预定 */
```

```c good
xrttype Type = {
	.Id = xrtTypeId(XRT_STR_LITERAL("app.Counter")),
	.AbiName = XRT_STR_INIT("app.Counter"),   /* 名与 Id 配套 */
	...
};
```

### 坑 2：描述表放在栈上注册

症状：注册表取回的描述悬空——栈帧消亡后描述内存失效。

原因：注册借用不复制——描述必须**进程期稳定**（静态存储或宿主管理的长命堆）。

```c bad
void register_types(void) {
	xrttype T = { ... };            /* 栈上 */
	xrtTypeRegistryAdd(&Reg, &T);   /* 返回即悬空 */
}
```

```c good
static const 类型方法表 kCounterMethods[] = { ... };
static xrttype Type = { ... };        /* 静态存储 */
/* 或宿主类型对象持有描述（与类型生命一致） */
```

### 坑 3：引用类型声明了非指针 Size

症状：Validate 拒绝，或后续对象/容器层访问越界。

原因：引用类型的 C ABI 存储恒为一个指针——Size/Align 必须如实声明；负载大小走 InstanceSize/InstanceAlign（两层分离不可混）。

```c bad
.Id = ..., .Kind = XRT_TYPE_CLASS,
.Size = sizeof(counter), .Align = _Alignof(counter),  /* 负载大小误入值字段 */
```

```c good
.Kind = XRT_TYPE_CLASS,
.Size = sizeof(ptr), .Align = _Alignof(ptr),           /* 值=指针 */
.InstanceSize = sizeof(counter), .InstanceAlign = _Alignof(counter),  /* 负载 */
```

## 练习

### 基础：内建类型巡检

打印 `xrtTypeInt64()`/`xrtTypeString()`/`xrtTypeCallable()` 等内建描述的 Kind/Size/Align——对照类别表。验收标准：每类的存储形态与表一致；TypeValidate 全过。

### 进阶：三型继承链

Base(FINAL=false)→Middle→Leaf 三层 CLASS：验证 TypeIsA 的链判定（Leaf is-a Base）、FINAL 继承拒绝、深度边界。验收标准：is-a 三组判定正确；非法继承在 Validate/注册期拒绝。

### 挑战：脚本枚举类型

定义带载荷的 ENUM 类型（Ok/Error/WithValue 三 case）+ Metadata 携带 case 名表；实现 `print_value(Type, Value)` 通用打印（switch Kind→分派格式化）。验收标准：枚举值打印 case 名；打印器对五种内建类型通用（零类型特定代码）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 定位 | C/XRT/宿主语言共享的类型事实层——类型知识的单一来源 |
| ID 派生 | `xrtTypeId(AbiName)` 稳定非零；TypeSame=Id+ABI 名双因子 |
| 22 类别 | 稳定存储类别（值形态）——非语言语法 |
| 两层大小 | 值（Size/Align）与实例（InstanceSize/Align）分离；引用类型值=指针 |
| 操作两层 | Ops（值）/InstanceOps（实例）——对应两层大小 |
| 继承 | 仅 CLASS；FINAL 不可继承；深度 255；is-a 沿单链 |
| 描述借用 | 注册不复制；进程期稳定；Validate 先行 |
| 内建描述 | `xrtTypeInt64()` 族进程期规范——容器要求指针相等 |
| 注册表 | Add/Remove/At/FindName——按规范名对接 |
| 转换 | CanConvert/CanWiden 判定、Convert 执行、ToString 文本 |
