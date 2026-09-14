---
num: 115
slug: xruntime-object
title: xruntime（二）：对象与对象图
volume: 卷十一 其他扩展库
type: practice
lead: 引用计数堆对象与弱引用、可裁剪的环收集（两遍标记 O(N+E)）、安全点纪律与失败原子性——宿主对象的完整生命周期。
api: xruntime-runtime_object, xruntime-runtime_object_graph
---

## 导读

类型系统（第 114 章）描述事实；对象层让事实**活起来**：`runtime_object` 提供引用计数堆对象（`xrtobject` 不透明控制块+一次分配承载头/对齐/负载）与**弱引用**（`xrtweak`——可提升的观察引用）；`runtime_object_graph` 在其上提供**可裁剪的强引用环收集**——引用计数处理不了循环引用（A 持 B、B 持 A，计数永不归零），对象图在**调用方选择的安全点**用两遍标记（O(N+E)）收集不可达环。设计立场："保留引用计数在确定性析构、C 扩展所有权和弱引用方面的优势，只在调用方选择的安全点处理单纯引用计数无法回收的环"——不是全代 GC，是**精确补丁**。`Trace` 契约（类型元数据驱动的强引用枚举）把追踪与销毁统一在同一份事实（第 114 章 InstanceOps 的兑现）。

## 引入

宿主语言的值的生命周期两头都不归 C 管：脚本变量消失的时机由解释器决定、对象间的循环由语言语义允许（`a.next = b; b.prev = a`）。引用计数处理前者（变量消失即 Unref——确定性析构），处理不了后者（环内计数恒正）。三种历史解法：全代 GC（stop-the-world、延迟析构——C 扩展的所有权噩梦）；手动 `weak_table`（每个语言运行时重写一遍）；**精确环收集**（只在安全点、只处理环、其余交给引用计数）——xruntime 选第三条：图只借用对象不加常驻引用、功能关闭时对象布局**零图字段**（裁剪的真实性——不用图一分钱都不付）。

安全点纪律是核心：收集必须由上层运行时在**对象图静止**时安排（任务/Future/生成器状态稳定后）——XRT 不用一把全局锁假装任意负载可安全并发读取；`Trace`/根枚举/`Drop` 不得启动新收集。这是第 60 章结构化并发的对象图版：**暂停的世界里做一致的观察**。

## 概念

### 对象布局与强引用

```diagram flow
- 创建：xrtObjectCreate(&Type)——一次分配（头+对齐填充+负载）
  ——负载地址满足 InstanceAlign（含高于堆默认的 32/64B 对齐）
  ——分配区先清零、再 TypeInitInstance；Init 失败立即回收且不调 Drop
- 强引用：Create/Lock 返回；Ref 增（调用期间必须已持有效强引用——不复活悬空）
  ——Unref 减（最后引用当线程一次 Drop、随后不可提升）
- 负载访问：ObjectData/ConstData/Size 借用——仅持有强引用期间有效
```

`CreateSized` 支持不小于 InstanceSize 的真实负载（可变尾随部分由类型自行解释/销毁）。**Init/Drop 责任**：失败的 Init 自行释放部分资源并设错误（系统不调 Drop——Drop 只对完整初始化的对象执行）。

### 对象值与弱引用

**对象值**（强引用槽作为普通值保存）：声明 COPYABLE+RELOCATABLE 并 `Ops = xrtObjectValueOps()`——标准对象值操作支持空值初始化/失败原子复制/移动/释放/地址比较散列/强引用追踪——**字段、参数、容器元素里的 `xrtobject*` 槽位**由它统一处理（第 117 章 typed 容器存对象的基础）。**弱引用**（`xrtweak`）：栈/结构/容器中的小型值——首次使用前清零、Init（从可选存活对象）/Copy（替换目标、失败保留原值、自复制空操作）/Expired（目标是否已终结）/Lock（**唯一提升方式**——提升成功返回新强引用、已终结返回空——"检查后使用"的竞态由 Lock 的原子性消灭）。

### 对象图：追踪契约

可收集类型经 `InstanceOps.Trace` 精确枚举负载直接拥有的强引用：

```c
static bool nodeTrace(const void* Value, const xrttype* Type,
		xrtobjectvisitor Visit, ptr Context) {
	const node* Node = (const node*)Value;
	return (Node->Next == NULL) || Visit(Node->Next, Context);
}
```

三条铁律：**每个实际强引用槽位访问一次**（两字段持同一对象访问两次——计数语义）；**弱引用/借用指针/空槽不访问**；**Drop 必须释放 Trace 报告的全部强引用**——违反则收集拒绝或泄漏。语言字段、容器元素、闭包捕获都应**复用同一份类型元数据**生成追踪与销毁（单一事实——第 114 章哲学的 GC 兑现）。

### 收集：两遍标记与根

```diagram flow
- 快照：取得图内对象集合（TrackedCount）
- 第一遍：Trace 全体——统计图内入边（EdgeCount）
- 根识别：强引用数>图内入边数=外部根；Roots 回调补充借用根（语言栈/生成器/宿主状态）
- 第二遍：从根传播可达性——O(N+E)、临时空间 O(N)
- 终结：全部不可达候选同时取得终结权→逐个 Drop→摘除——弱引用自终结起不可提升
```

**Value 外壳的特别处理**：xvalue 引用与容器 COW backing 不逐项增加内部对象强引用——自动根推断不够；`runtime_value_roots` 提供 `CollectValueRoot`/`CollectValueRoots`（单值/批量栈槽全局槽挂起帧）——复用 `xrtValueTraceRuntimeObjects`、失败包装原因链。**动态字段节点**（`xrtdynamicfields`）：对象图中的独立字典节点——宿主只追踪字段对象、字段的 typed dict 载荷再经 `xrtTypeValue()` 追踪值引用——分层避免收集器对 xvalue 字典递归猜测。

### 失败原子性与错误域

**全部前置检查在任何析构前完成**（分配/追踪/根枚举/入边一致性/快照计数检查）——任一失败图成员/强弱引用/负载/输出保持不变；**只有全部候选同时取得终结权才摘除**——不存在"收了一半"的状态。错误域 `xrt.object-graph` 五码（ARGUMENT/TRACK/TRACE/STATE/ROOTS）；线程错误的隔离与恢复有精确规则（成功恢复调用方原错误、失败只保留本次错误）。

## 示例

### 第一个完整程序：自环对象的收集

下面的程序来自 `examples/runtime/object_graph`——环引用的建立与回收：

```embed path="extlibs/xruntime/examples/runtime/object_graph/main.c" title="extlibs/xruntime/examples/runtime/object_graph/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/object_graph/main.c -lws2_32 -liphlpapi
（输出 tracked/edges/collected 的收集结果自检行）
```

**刚才发生了什么。** ① `nodeDrop` 释放 `Next` 引用、`nodeTrace` 枚举 `Next`——**Drop 与 Trace 的对称**（铁律第三条：Drop 释放 Trace 报告的全部）。② 建立自环：`Node->Next = xrtObjectRef(Node)` 再 Unref 外部引用——此刻只剩环内自持（引用计数恒正、无外部引用）。③ `xrtObjectGraphCollect` 一call：快照→两遍标记→识别"无外部根"→终结——`CollectedCount` 报告回收数。**这就是引用计数做不到、对象图一句话完成的事**；而收集之外的一切（创建/访问/普通析构）仍是纯引用计数的确定性世界。

### 第二个完整程序：对象与弱引用生命周期

第二个程序来自 `examples/runtime/object`——引用计数的常规面：

```embed path="extlibs/xruntime/examples/runtime/object/main.c" title="extlibs/xruntime/examples/runtime/object/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/object/main.c -lws2_32 -liphlpapi
（输出 counter 值与弱引用 expired 状态的自检行）
```

**刚才发生了什么。** ① `xrtObjectCreate`+`xrtObjectData` 写负载——一次分配、借用访问（强引用期间）。② 计数器递减逻辑走 `InstanceOps`（Init/Drop 由类型提供——payload 零散资源在 Drop 清）。③ `xrtWeakInit`/Expired 的观察者形态：对象终结后 `expired=true`——弱引用是"不延长生命的观察"（缓存/观察者模式的标准件）；要复活就得 `WeakLock` 提升强引用（对象活着才有新强引用）。配套示例族：`value_weak`/`value_roots`（Value 侧的弱引用与根）、`value_trace`（值追踪）——图生态的 Value 面。

## 契约

- **对象布局**：一次分配承载头/对齐/负载；负载满足 InstanceAlign（含 32/64B 超常规）；Init 失败即回收不调 Drop。
- **强引用纪律**：Ref 前提是已持有效引用（不复活悬空）；Unref 最后一次当线程 Drop；负载访问仅强引用期间。
- **对象值**：COPYABLE+RELOCATABLE+ObjectValueOps——字段/元素/参数槽的统一操作；地址比较散列仅进程内。
- **弱引用**：首次清零；Lock 是唯一原子提升；终结起不可提升。
- **图裁剪**：功能关=零图字段零收集器代码；开=对象至多属一图、图只借不增引用。
- **Trace 铁律**：每强引用槽恰好一次；弱/借/空不访；Drop 释放 Trace 全部——违反=拒绝或泄漏。
- **成员操作**：Track 幂等（他图归属 EXISTS）；普通终结自动摘除；同图成员操作可并发。
- **安全点**：静止时收集；Trace/根/Drop 不得再入收集；不与并发修改同批对象。
- **失败原子**：全部检查先于析构；候选同时取得终结权；无半收集状态。
- **复杂度**：O(N+E) 两遍标记、O(N) 临时空间；结果四字段（Tracked/Edge/Root/Collected）。
- **Value 根**：外壳/COW 引用需 CollectValueRoot(s) 显式补充；动态字段分层追踪。

## 避坑

### 坑 1：Trace 漏报强引用

症状：收集拒绝（入边不一致）或对象泄漏（该收没收到）——Drop 释放了 Trace 没报告的引用。

原因：Trace 的"每槽恰好一次"是收集正确性的根基——漏报让入边统计错（该是根的不成根）、多报让可达对象被误判。语言字段的 Trace 生成必须与字段布局同源。

```c bad
static bool trace(const void* V, ..., xrtobjectvisitor Visit, ptr Ctx) {
	const node* N = V;
	return Visit(N->Next, Ctx);
	/* 还有 N->Prev 强引用没报——入边缺一：统计错乱 */
}
```

```c good
static bool trace(const void* V, ..., xrtobjectvisitor Visit, ptr Ctx) {
	const node* N = V;
	if ( (N->Next != NULL) && !Visit(N->Next, Ctx) ) { return false; }
	return (N->Prev == NULL) || Visit(N->Prev, Ctx);  /* 全部强引用 */
}
```

### 坑 2：Expired 检查后直接用裸指针

症状：检查时活着、用时已终结——TOCTOU 竞态崩溃。

原因：Expired 是瞬时观察；唯一安全的"检查+使用"是 `WeakLock` 原子提升——成功即持有新强引用（期间对象必活）。

```c bad
if ( !xrtWeakExpired(&Weak) ) {
	use(xrtObjectData(Weak...));   /* 检查与使用之间可能终结 */
}
```

```c good
xrtobject* Strong = xrtWeakLock(&Weak);
if ( Strong != NULL ) {
	use(xrtObjectData(Strong));   /* 持强引用：期间必活 */
	xrtObjectUnref(Strong);
}
```

### 坑 3：非安全点触发收集

症状：收集过程读到并发修改的引用字段——STATE 错误或更糟的错收。

原因：契约明示"静止时收集"——Trace/字段写入/引用计数变动都属修改。宿主调度器要在任务/Future/生成器稳定后进入安全点（第 60 章暂停语义）。

```c bad
/* 任一线程还在改对象字段时 */
xrtObjectGraphCollect(Graph, &Result);   /* STATE 错误/错收风险 */
```

```c good
/* 宿主进入安全点：暂停任务/生成器、状态稳定后 */
host_suspend_all();
xrtObjectGraphCollect(Graph, &Result);
host_resume_all();
```

## 练习

### 基础：强弱引用计数实验

对象 + 两个强引用 + 一个弱引用：逐步 Unref 观察 Expired 翻转点；Lock 提升前后对比。验收标准：终结点恰在最后强引用；弱引用不延长生命；Lock 失败后 Weak 可复用（Init 新目标）。

### 进阶：双向环收集

A↔B 双向环 + 外部引用 C→A：验证有外部根时不收集（C 释放后再收集成功）。验收标准：两次收集的 Root/Collected 计数与手推一致。

### 挑战：宿主安全点集成

模拟宿主调度器：N 个"任务"持对象引用——暂停全部（安全点）→收集→恢复；任务中挂起的"生成器"持引用走 Roots 回调补充。验收标准：任务持引用的对象不被收；释放后收；根回调的借用根期间存活（收集结束前）；全程 STATE 零错误。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 定位 | 引用计数之上的环收集补丁——非全代 GC；裁剪真实（关=零成本） |
| 对象布局 | 一次分配；负载满足 InstanceAlign；Init 失败不调 Drop |
| 强引用 | Ref 需已持有效；最后 Unref 当线程 Drop；访问限强引用期 |
| 对象值 | ObjectValueOps——字段/元素槽统一；地址比较仅进程内 |
| 弱引用 | Lock 唯一原子提升；Expired 仅观察；终结起不可提升 |
| Trace 铁律 | 每强引用槽恰一次；弱/借/空不访；Drop=Trace 全集 |
| 收集算法 | 快照→入边统计→根识别（自动+回调）→可达传播 O(N+E) |
| 安全点 | 静止收集；回调不再入；宿主调度器负责安排 |
| 失败原子 | 检查先于析构；候选同时终结；错误域 xrt.object-graph 五码 |
| Value 根 | 外壳/COW 需显式 ValueRoot(s)；动态字段分层追踪 |
