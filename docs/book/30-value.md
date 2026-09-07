---
num: 30
slug: value
title: xvalue 动态值系统
volume: 卷四 文本与结构化数据
type: practice
lead: 一个类型装下所有数据——精确读取不做隐式转换、引用计数管生命周期、值树组合成配置与消息。
api: value
---

## 导读

从本章起进入结构化数据。`xvalue` 是 XRT 的动态类型系统：一个不透明指针可以装下 null/bool/int/float/string/bytes/time/数组/对象/集合/映射——JSON 能表达的它全都能表达，XSON 的扩展类型（第 32 章）也直接住在这里。三个设计决定它的使用体验：**精确读取**（GetInt 只接受 int，类型不符即失败——防类型混乱的第一道闸）、**引用计数生命周期**（第 3 章原语的完全体）、**不可变共享**（值树可以被多处引用而不复制）。它是第 31/32/34 章的解析产物、模板的数据源——本卷后半的地基。

## 引入

配置系统的经典难题：同一份配置文件，超时是数字、开关是布尔、标签是列表、嵌套服务是对象——C 的静态类型装不下"运行时才知道形状"的数据。手写 `void*` 加类型标签是每个人都在重复发明的轮子，而且坑密度极高：类型判断靠记忆、释放责任靠约定、复制语义靠祈祷。

`xvalue` 把这个轮子标准化：一个类型枚举（`xvaluetype`）加一族构造/读取/判定函数，配引用计数生命周期。它与动态语言的值对象（Python 的 object、JS 的 value）在概念上同构，但把"类型严格"放在第一位——**读什么类型就给什么类型的出参**，没有"数字字符串自动转换"的宽容。这种严格在配置解析场景恰好是刚需：`"timeout": "30"`（字符串）与 `"timeout": 30`（数字）是两个不同的错误信号，宽容会把前者悄悄变成后者，bug 从此深埋。

## 概念

### 类型族与构造

| 类型 | 构造 | 精确读取 |
| --- | --- | --- |
| null / bool | `xrtValueNull` / `Bool` | `GetBool` |
| int64 / double | `xrtValueInt` / `Float` | `GetInt` / `GetFloat` |
| string / bytes | `xrtValueString` / `Bytes` | `GetString`（视图）/ `GetBytes` |
| time / pointer | `xrtValueTime` / `Pointer` | `GetTime` / `GetPointer` |
| array / object | `ArrayNew` / `ObjectNew` + Set 族 | 按下标 / 按名 |
| set / intmap | `SetNew` / `IntMapNew` | 集合与整数键映射 |

Null 是**进程级单例**——每次返回同一指针，可以安全地 `==` 比较。字符串读取返回**视图**（借用，第 3 章约定），不是新分配的拷贝。数值有一个专门的宽松通道：`xrtValueScalarEqual` 让 int 2 与 float 2.0 相等（跨类型数值语义），且 `ValueHash` 与之一致（int 2 与 float 2.0 同哈希）——这对"值树做缓存键"的场景是必要的一致性设计。

### 精确读取的哲学

`GetInt` 收到 float 值时**失败**，不悄悄截断；要数值宽容先显式判类型再取。这条哲学与第 26 章严格解析同宗：**类型混乱在读取口暴露，不在消费端炸雷**。配合 `xrtValueType`（查类型）与 `Is`/`IsNumber`/`IsContainer`（判定族），读取代码的形状永远是"先判后取、类型不符走错误路径"。

### 生命周期：引用计数的完全体

```diagram flow
- 构造：Int/String/ObjectNew 返回计数为 1 的拥有式值
- 共享：Retain 增引用——值树存进多个结构不复制
- 释放：Release 减引用——归零者析构（含整棵子树）
- 容器挂值：Set 族把子值挂进容器——容器持引用，挂完可安全 Release 自己那份
```

这是第 3 章 `xrtRefRetain`/`xrtRefRelease` 原语的完全体应用：构造返回计数 1；`xrtValueRetain` 共享、`xrtValueRelease` 归还；容器挂子值时**容器取得自己的引用**——你构造后立刻 Release 自己那份，子值的生命周期从此由容器管。整棵树释放时递归归还——根 Release 归零，全树析构。

### 值树操作

配置场景的两大件：`xrtValueObjectMerge`（对象覆盖合并——默认配置 + 用户覆盖的标准姿势，策略含 REPLACE/保留/报错冲突）与 `xrtValueSetUnion/Merge`（集合并集——权限合并的标准姿势）。`xrtValueClone` 做浅克隆（共享元素）——深浅克隆的选择在 ownership 范例里有对照实验。

## 示例

### 完整程序：全类型速览与精确读取

来自仓库范例 `examples/value/basic/main.c`：

```embed path="examples/value/basic/main.c" title="examples/value/basic/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/value/basic/main.c -lws2_32 -liphlpapi
xrt value API v2
time type: time
```

**刚才发生了什么。** ① 构造族逐一建值：Int、Float、String、Bytes、Time……每个构造返回计数 1 的拥有式值。② 精确读取族逐个取回：`GetInt` 读 int 值成功、读 float 值**失败**——"读什么类型给什么类型"的闸门在逐个类型上验证。③ 判定族与 `ValueTypeName`（类型枚举转小写名）配套——调试输出与错误消息里报的是 `"time"` 这样的稳定名字。④ `ScalarEqual` 验证 2 与 2.0 跨类型相等、`ValueHash` 验证两者同哈希——宽松数值语义与哈希一致性成对出现，值树做键的前提。

### 完整程序：配置合并与集合并集

来自 `examples/value/collections/main.c`：

```embed path="examples/value/collections/main.c" title="examples/value/collections/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/value/collections/main.c -lws2_32 -liphlpapi
options=1 permissions=2
```

**刚才发生了什么。** ① 对象合并 `Merge(默认, 用户, REPLACE)`：用户配置的 timeout 5 覆盖默认的 30——合并后 count 仍是 1，覆盖不增键。② 集合并集两条路：`Union` 产新集合与 `Merge` 就地并入，产物用 `SetEqual` 判等——两条路殊途同归。③ `IsDisjoint` 判断两集合不相交——权限检查的"有无交集"一步到位。④ 这两个操作合起来就是"默认配置 + 用户覆盖 + 权限合并"的配置层骨架，第 34 章模板与第 31 章 JSON 读出的值树都直接用它们。

## 契约

- **精确读取**：类型不符即失败，无隐式转换；数值宽容走显式的 `ScalarEqual`。
- **引用计数**：构造=1、`Ref` 共享、`Release` 归还、归零析构整树；容器挂值取得自己的引用。
- **Null 单例**：进程级同一指针，可 `==` 比较。
- **字符串借用**：`GetString` 返回视图，时效跟着值树走。
- **哈希一致**：`ValueHash` 与 `ScalarEqual` 配套——值树可做缓存键。
- **合并策略**：REPLACE 覆盖 / 保留既有 / 报错冲突——显式选择。

### 从示例到工程：值树的三种宿主

值树在工程里的三种典型宿主，决定引用计数的配平方式。**配置树**：启动时解析（第 31 章 JSON）一次构造、全程只读、退出时根 Release 一次——最简单的配平（只有构造与根释放，中间无 Ref/Release）。**消息传递**：值树作为消息跨线程/跨模块传递——发送方 Retain 后交给接收方、双方各自 Release；或者用 SetTake 移交（第 18 章队列场景的标准姿势，一次移交零多余引用）。**缓存共享**：同一棵值树被多个使用者引用（默认配置共享给所有请求）——构造方 Retain 后放入全局、使用者各自 Retain/Release；COW（写时复制）语义让浅共享的 Clone 在首次编辑时才真正复制。三种宿主之外再加一条总纪律：**配平写在设计文档里，不靠运行时祈祷**——第 6 章的统计是配平正确性的最终裁判。

### 与容器的分工

第 30 章的值容器（array/object/set/intmap）与卷三的同名容器（第 14/18/19 章）是什么关系？**两套平行实现，服务两种世界**。卷三容器是静态类型的世界：元素类型编译期定、无类型标签、极致性能——热路径数据结构用它。值容器是动态类型的世界：每个值带类型标签、支持异构混装、可序列化——配置与协议数据用它。选择口诀：**数据形状编译期已知 → 卷三容器；运行时才知（来自 JSON/网络/脚本）→ 值树**。两套之间可桥接（值树的 intmap 与 xintmap 互转），但不要混用语义——把值树当高性能容器用，或反过来给静态容器加类型标签，都是在重造对方。

## 避坑

### 坑 1：挂进容器后忘记 Release 自己那份

症状：内存泄漏——值树释放后统计里仍有活跃值；泄漏量与"挂进容器的子值数"成正比。

原因：容器挂值时**增加**引用（容器的那份）；你构造时拿到的那份引用仍在手里，不 Release 就永远多一份。

```c bad
xvalue* pTimeout = xrtValueInt(30);
xrtValueObjectSet(pConfig, XRT_STR_LITERAL("timeout"), pTimeout);
/* 少了 Release：pTimeout 的引用停在 2，容器释放后仍剩 1——泄漏 */
```

```c good
xvalue* pTimeout = xrtValueInt(30);
xrtValueObjectSet(pConfig, XRT_STR_LITERAL("timeout"), pTimeout);
xrtValueRelease(pTimeout);   /* 容器已持引用——自己那份归还 */
```

### 坑 2：靠类型宽容读取

症状：配置里数字写成字符串（`"30"`），程序"能跑"但行为随上游格式漂移；重构时雷爆。

原因：先 `IsNumber` 判断再取的纪律被"先试试 GetInt"的心态取代——宽容掩盖了上游数据形状错误。

```c bad
int64 iTimeout;
if ( xrtValueGetInt(pValue, &iTimeout) ) {
	/* "30"（字符串）在这里失败——但更糟的是有人会加个 fallback 再试字符串 */
}
```

```c good
if ( xrtValueType(pValue) == XVALUE_INT ) {
	int64 iTimeout;
	xrtValueGetInt(pValue, &iTimeout);
} else {
	ReportTypeMismatch("timeout", "int");   /* 形状错误在读取口报告 */
}
```

## 练习

### 基础：类型速览

构造全部标量类型各一个，逐一打印 `ValueTypeName` 与 `ValueType`；对 int 值调用 GetFloat 验证失败——严格性的手感。

### 进阶：配置合并器

构造"默认配置 + 用户配置"两棵对象树（各 3~4 个键、含嵌套对象），REPLACE 合并后输出全部键值；再换报错冲突策略，构造一个键冲突的输入观察失败。提示：嵌套对象的合并语义也要想清楚（整棵替换还是递归合并——按你的业务定并写进注释）。

### 挑战：值树缓存

把"配置名 → 值树"存进第 18 章的映射，值树用 `ValueHash` 做指纹、`ScalarEqual` 做等价；同一配置两次解析指纹必同、改一个字段指纹必变。验收标准：指纹稳定可做测试断言；缓存命中与未命中的值树 `ScalarEqual` 判等；全部 Release 后第 6 章统计验证零泄漏。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 类型族 | 标量 + string/bytes/time/pointer + array/object/set/intmap |
| 精确读取 | 类型不符即失败；数值宽容走 `ScalarEqual`；判型用 `ValueType` |
| 生命周期 | 构造=1 / `Ref` 共享 / `Release` 归还；容器挂值自持引用 |
| Null 单例 | 同一指针可 `==`；区分"没有"与"值为 null"时用 `XVALUE_NULL` 判型 |
| 哈希 | `ValueHash` 与 `ScalarEqual` 配套——值树可做缓存键 |
| 配置两件套 | `ObjectMerge`（REPLACE/保留/冲突）+ `SetUnion/Merge` |
| 字符串 | `GetString` 返回借用视图；要副本自己 Dup |
