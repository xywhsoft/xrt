---
num: 116
slug: xruntime-call
title: xruntime（三）：动态调用
volume: 卷十一 其他扩展库
type: practice
lead: 签名与调用帧、参数的位置/关键字双轨、内联前四的多返回值、不可变 callable 与原子性调用序——宿主函数调用的标准模型。
api: xruntime-runtime_call, xruntime-runtime_type
---

## 导读

类型（113）与对象（114）之后，动态调用电墨登场：脚本要调 C 函数、C 要回调脚本函数、绑定层要在两者之间传参——`runtime_call` 提供统一的调用模型。三件套：**签名**（`xrtfunctionsig`——参数名/类型/返回类型的借用声明；命名参数参与签名身份）；**调用帧**（`xrtcallframe`——Self/位置参数/关键字参数/上下文的借用组装；`FrameValidate` 全量校验：重复关键字/必需参数/未知关键字/varargs 标志）；**callable**（`xrtCallableCreate(签名, 入口, 环境, 环境析构)`——创建后不可变、引用计数原子；环境由 callable 持有）。**结果**（`xrtcallresult`——前四个内联零分配、Set/Push 引用语义、Take 转移）。`Invoke` 的六步原子序列（复制帧→验证→临时结果→失败清理→数量检查→一次替换）保证**失败调用不破坏调用方原结果**。关键边界：只提供类型明确的动态入口——不通用调用任意 C ABI（无 libffi 依赖）。

## 引入

"动态调用"的朴素做法是 `void* fn(void** args)` ——丢失全部类型信息，每个绑定手写转型，错误在运行深处爆炸。第二条歧路是 libffi 式通用调用——平台 ABI 的深渊（cdecl/stdcall/fastcall、结构体返回约定……契约文档明说"C 无法在没有准确原型或 libffi 的前提下安全通用调用任意函数"）。xruntime 的第三条路：**类型明确的动态入口** `xrtcallproc`——编译器、绑定生成器或 FFI 模块为每个原始函数生成一个适配器（适配器有准确原型、调原始函数零 UB），动态层只见适配器。**签名在动态层流通**：参数名/类型/返回类型都是声明——入口拿到的是"已验证的帧 + 有效签名"，转型在适配器里静态完成。

**命名参数的设计**值得注意：`KeywordNames`/`KeywordValues` 是**借用的名称+值数组**——不是字典！"调用方不需要为了传递 kwargs 构造字典"——热路径零分配的又一次贯彻（对比脚本语言的常规做法：kwargs 先建 Map 再传）。

## 概念

### 签名与调用帧

```diagram flow
- 签名：xrtfunctionsig——参数（名/类型/可选性）与返回类型数组（借用声明方，生命覆盖 callable）
  ——非空参数名参与签名身份且必须唯一；空名参数只能按位置传
- 帧：xrtcallframe——Self（可选接收者）/Arguments（位置，超形参部分为 varargs）
  /KeywordNames+KeywordValues（一一对应）/Context（调用链自解释）
- 校验：FrameValidate——数组完整性/空值/重复关键字/必需参数/重复传参/未知关键字/标志组合
```

**参数读取三入口**：`FrameArgument`/`FrameKeyword`（原始传参视图）、`FrameParameter`（按有效签名形参下标完成位置/关键字选择——**普通入口首选**；可选参数未提供返回 NULL 不设错）。

### 结果：内联与引用语义

`xrtcallresult` 拥有其中的 xvalue 引用：**前四个结果内联**（零结果数组分配）；第五个起轻量指针数组（不建动态 Value 容器）。操作族：`Set`/`Push`（增引用）、`SetTake`/`PushTake`（转移并清空来源槽）、`Clear`（释放值保留容量——热路径复用）、`Unit`（全释放）、`Move`（整体转移）。**只允许替换现有下标或追加末尾**——不允许稀疏写入（旧版为填空洞构造多个 null 的教训——返回数量始终明确）。

### callable 与 Invoke 六步

```diagram state
未创建 -> 可用: Create(签名, 入口, 环境, 析构)——不可变；环境 callable 持有
调用中: 复制帧+补签名 → 验证 → 私有临时结果调入口
调用中 -> 失败: 释放全部部分结果 + 入口错误包 xrt.call 原因链（调用方原结果无损）
调用中 -> 成功: 有签名则查精确返回数量 → 一次性替换调用方结果
```

**入口可并发可重入**——环境是否并发安全由入口实现负责（契约划界）。callable 类型（`xrtTypeCallable()`）：拥有一个强引用的稳定 C ABI 槽——Copy/Clone 增引用替换、Move 转移清空、Drop 释放——callable 作为**值**在容器/字段流通（第 115 章对象值同款模式）。

### 原始 ABI 边界

旧版 callable 保存 cdecl/stdcall/fastcall 指针与动态入口的混合——本模块**只提供一种类型明确的动态入口**。绑定生成器生成 `xrtcallproc` 适配器调原始函数：这条边界**避免未定义行为**，也**不把平台 ABI 成本施加给基础调用层**（裁剪与可移植的双赢）。

### 调用层的分层位置

```diagram flow
- 签名层：xrtfunctionsig——参数/返回的借用声明（命名参与身份）
- 帧层：xrtcallframe——Self/位置/关键字/上下文的借用组装 + Validate 全量校验
- callable 层：不可变对象 + 原子引用；环境由其持有
- 结果层：xrtcallresult——前四内联、引用/转移语义、只替换或追加
- 边界层：类型明确的单一动态入口——原始函数经适配器接入
```

## 示例

### 第一个完整程序：callable 创建与调用

下面的程序来自 `examples/runtime/call`——签名、帧、调用的完整闭环：

```embed path="extlibs/xruntime/examples/runtime/call/main.c" title="extlibs/xruntime/examples/runtime/call/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/call/main.c -lws2_32 -liphlpapi
type=callable
7 + 5 = 12
```

**刚才发生了什么。** ① 创建侧：签名（两 int64 参数、int64 返回——借用声明）+ 加法入口 + 环境组合出 callable——`xrtTypeCallable()->Name` 打印 `callable`：**callable 本身是个有类型的值**（第 114 章类型谱系里的 CALLABLE 类）。② 调用侧：帧组装两个位置参数 → `xrtCallableInvoke(pCallable, &Frame, &Result)` → `xrtCallResultGet(&Result, 0)` 取首结果 → `xrtValueGetInt` 解引用——四步从动态结果回到静态 int64。③ `7 + 5 = 12` 的背后是完整六步序列：帧验证过、临时结果原子替换——**任何一步失败你的 Result 保持原样**（下一个调用可以放心复用）。`Unit` 收尾 + `Unref` callable——引用纪律与第 115 章对象一致。

### 第二个完整程序：值侧 callable 装箱

第二个程序来自 `examples/runtime/value_callable`——callable 进 Value 世界：

```embed path="extlibs/xruntime/examples/runtime/value_callable/main.c" title="extlibs/xruntime/examples/runtime/value_callable/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/value_callable/main.c -lws2_32 -liphlpapi
（输出 callable 装入 Value 后的调用自检结果）
```

**刚才发生了什么。** ① `runtime_value_callable` 是独立裁剪层：callable 强引用装进 `xvalue`（第 31 章动态值的运行时扩展）——脚本层的"函数是一等值"由此落地：Value 里的 callable 可以存进 dict、传给函数、从函数返回。② 基础 `runtime_call` **不强制启用** Value 装箱——不需要一等的场景零装箱成本（第 114 章裁剪哲学的逐层贯彻）。③ 配套示例族：`value_future`（Future 装箱）、`value_object`（对象装箱）、`value_type`（类型值装箱）——**运行时对象↔动态值**的完整桥面。

## 契约

- **签名借用**：签名/类型/参数/返回数组声明方持有、生命覆盖 callable；命名参数参与签名身份且唯一；空名仅位置。
- **帧借用**：Self/参数/关键字数组/上下文全部借用——调用期间有效。
- **校验全量**：完整性/空值/重复关键字/必需/重复传参/未知关键字/标志组合——验证过的帧才进入口。
- **参数读取**：FrameParameter 按签名下标选位置/关键字（首选）；可选未提供=NULL 不设错；关键字不存在=NULL 不设错。
- **结果语义**：前四内联；Set/Push 增引用、Take 族转移清源；Clear 保容量；只替换或追加——无稀疏写入。
- **callable 不可变**：创建后签名/入口/环境不变；引用计数原子；环境恰一次析构。
- **Invoke 原子**：六步序列；失败释放部分结果+包装原因链、调用方结果无损；成功一次替换。
- **入口纪律**：可并发可重入；环境并发安全归入口实现。
- **ABI 边界**：仅一种类型明确入口；原始函数经适配器接入——无通用调用无 UB。
- **装箱独立**：Value callable 是独立裁剪——基础层零装箱成本。

## 避坑

### 坑 1：签名数组是栈上的临时

症状：callable 调用时读到垃圾签名——创建时的参数/返回数组已随栈帧消亡。

原因：签名是**借用**——生命周期必须覆盖 callable 全程（callable 不复制它）。签名放静态存储或与 callable 同生命的堆。

```c bad
xrtcallable* make(void) {
	xrtfunctionsig Sig = { ...栈上... };
	return xrtCallableCreate(&Sig, entry, NULL, NULL);  /* 返回即悬空 */
}
```

```c good
static const xrttype* const kArgs[] = { ... };       /* 静态 */
static const xrtfunctionsig kSig = { ... };
xrtcallable* C = xrtCallableCreate(&kSig, entry, NULL, NULL);
```

### 坑 2：失败调用后直接用结果

症状：失败路径读 Result——拿到的是**上一次成功调用的旧值**（原子性保护了它，但它不是本次结果）。

原因：六步序列保证"失败不破坏原结果"——原结果可能是空（首次）也可能是旧值；失败后 Result 的语义是"未变"，必须走错误分支而不是读结果。

```c bad
if ( !xrtCallableInvoke(C, &Frame, &Result) ) {
	use(xrtCallResultGet(&Result, 0));   /* 旧值/空——不是本次的 */
}
```

```c good
if ( !xrtCallableInvoke(C, &Frame, &Result) ) {
	report(xrtErrorMessage(xrtGetError()));   /* 错误分支：只看错误 */
} else {
	use(xrtCallResultGet(&Result, 0));         /* 成功才有本次结果 */
}
```

### 坑 3：稀疏写入结果（Set 下标 2 但只有 0）

症状：被拒绝——结果只允许替换现有或追加末尾。

原因：稀疏写入的语义空洞（中间下标是什么？null？）是旧版设计坑——新契约"返回数量始终明确"，要占位就显式 Push 空。

```c bad
xrtCallResultSet(&Result, 2u, Value);   /* 越界稀疏：拒绝 */
```

```c good
/* 依次追加——数量与顺序都明确 */
xrtCallResultPush(&Result, V0);
xrtCallResultPush(&Result, V1);
xrtCallResultPush(&Result, V2);
```

## 练习

### 基础：加法 callable 两种参数轨

同一入口：位置参数调用 + 关键字参数调用（名称数组传参）——FrameParameter 双轨取参。验收标准：两轨解出同参；未知关键字被 Validate 拒绝。

### 进阶：可选参数与默认值

三参签名（一必需两可选）：入口对未提供参数取默认——FrameParameter 的 NULL 语义。验收标准：三种调用形态（全给/给一个/只给必需）行为正确；重复传参（位置+关键字同名）被拒。

### 挑战：脚本函数表

用 dict（第 32 章）存 name→callable 的注册表；实现 `call(name, 帧字符串)` 的迷你解释器：查表→帧→Invoke→结果打印。加一个 C 原函数（如 `max(int64,int64)`）经适配器注册。验收标准：三函数注册可调；未知名清晰报错；并发调用同一 callable 零错（入口无状态）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 三件套 | 签名（借用声明）/帧（借用组装）/callable（不可变+原子引用） |
| 命名参数 | 名称+值借用数组——非字典；命名参与签名身份且唯一；空名仅位置 |
| 校验 | Validate 全量：重复关键字/必需/未知/varargs 组合 |
| 参数读取 | FrameParameter 按签名下标（首选）；可选未给=NULL 不设错 |
| 结果 | 前四内联；Set/Push 引用、Take 转移、Clear 保容量；只替换/追加 |
| Invoke 六步 | 复制帧→验证→临时结果→失败清理→数量检查→一次替换 |
| 原子性 | 失败不破坏调用方原结果；错误包 xrt.call 原因链 |
| 入口 | 可并发可重入；环境并发归实现 |
| ABI 边界 | 仅一种类型明确入口；原始函数经适配器——无 UB 无平台成本 |
| 装箱 | Value callable 独立裁剪——一等函数可选 |
