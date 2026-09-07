---
num: 116
slug: xruntime-typed
title: xruntime（四）：typed 容器
volume: 卷十一 其他扩展库
type: practice
lead: 类型描述驱动的 array/stack/list/tree/set/dict 六族、统一所有权与失败原子、描述指针相等与复用 XRT 快速路径——宿主值的容器面，卷十一收官。
api: xruntime-typed_array, xruntime-typed_dict, xruntime-runtime_type
---

## 导读

xruntime 四章收官。typed 容器族（`xtypedarray`/`xtypedstack`/`xtypedlist`/`xtypedtree`/`xtypedset`/`xtypeddict`）在六种底层数据结构之上共享**同一套运行时值契约**：借用不可变的类型描述、描述驱动的值操作族管理值生命周期（复制/移动/比较/散列/销毁全部经描述表——第 113 章类型事实的容器兑现）、统一所有权动词（Set/Add/Push 复制、Take/Pop 移动、SetTake 失败原子转移、Clone 深复制）、统一失败原子性（多步操作先工作存储后提交、失败不破坏可见状态）。**工程要点**：扩展直接**复用 XRT 容器的内部快速路径**（不复制底层实现——typed 层是"类型驱动的壳"，int64 数组的性能就是第 14 章数组）；涉及两容器的操作要求**描述指针相同**（同 ID/名称不够——生命 ABI 与私有上下文不可互换）。卷十一十八章至此收官。

## 引入

为什么需要 typed 容器？第 14-23 章的 C 容器（`xarray(int64)` 等）是**编译期类型**——快、静态，但类型写死在编译时刻。宿主语言场景（脚本数组、JSON 装回内存、插件交换数据）需要**运行时类型**的容器："这个数组的元素类型是那个注册表里名叫 `app.Counter` 的描述"——同一份容器代码、运行时按描述分派值操作。朴素做法是 `void*` 数组+回调散落——每个操作每处转型；typed 层把分派集中到描述表：**容器代码一份、类型行为全在 xrttype**。

与第 31 章 `xvalue` 动态值的关系：xvalue 是"单个动态值"（自描述标签联合）；typed 容器是"同型元素的集合"（容器级描述、元素零标签开销——int64 数组就是连续 int64 不是 Value 数组）。两者互补：Value 适合异构树（JSON），typed 适合同构集合（脚本数组/对象字段表）。

## 概念

### 共同契约：六族一面

| 动词 | 语义 |
| --- | --- |
| `Set`/`Add`/`Push`/`Insert` | 复制来源值——容器拥有新值、来源归调用方 |
| `SetTake` | 失败原子移动——成功后来源恢复空值 |
| `Take`/`Pop` | 移动到已初始化输出——容器不再拥有 |
| `Remove`/`Clear`/`Unit` | 对每个被拥有值执行一次 Drop |
| `Clone`/`Merge`/集合代数 | 深复制——不共享隐藏所有权 |

**地址合法性**：接受完整类型值地址的 API 允许"外部值或本容器准确活动槽"——指向容器结构/元数据/备用容量/填充/槽中间的地址**读取或修改前拒绝**；移动的输出与来源必须在容器内存之外（防御性校验的容器版）。

### 六族的特有面

- **array**（连续存储）：定长/可变长、索引访问、Concat/Equals 族。
- **stack**（LIFO）：Push/Pop/Peek。
- **list**（稀疏整数键）：插入/删除按序迭代——脚本列表的形态。
- **tree**（有序泛型键）：比较驱动的有序映射——键类型需比较操作。
- **set**（唯一值）：散列或有序去重——键类型需散列/比较。
- **dict**（文本键）：字符串键字典——第 114 章动态字段节点（`xrtdynamicfields`）的载荷就是它。

### 描述指针相等：为什么 ID 相同不够

契约原文级："涉及两个类型容器的操作要求元素类型描述**指针相同**。相同 ID、名称、大小或操作表**不足以证明**两个描述的生命周期 ABI 及私有上下文可互换。"——两个模块各自静态声明了"相同"的类型？指针不同就不互操作：**XRT 内建类型和宿主注册类型应使用进程期规范描述**（`xrtTypeInt64()` 返回的同一指针）。这条规则消灭"描述内容相同但声明处不同"的隐患（私有上下文/生命期的隐式分歧）。

### 复用而非复制

扩展**直接复用 XRT 容器的内部快速路径**——`xtypedarray` 的连续存储就是第 14 章 array 的内部结构，typed 层加的是"经描述表分派值操作"的入口。收益双向：性能（int64 push 的快速路径不因 typed 壳变慢）与维护（容器算法一份——第 22 章池化的思想在"实现复用"上的又一形态）。

### 失败原子性（共同）

分配/值初始化/复制/移动失败时：公开可见的元素、键、顺序、所有权**保持不变**——多步操作（Clone/Merge/集合代数）先在独立工作存储完成再一次提交；失败清理只销毁本次成功构造的值并保留底层错误为原因。OOM 最终类别恒为 `XERR_MEMORY`。这与第 31 章 Value 操作、第 18 章容器的原子性纪律一脉——typed 层不因为"动态"就放松。

### typed 容器在 xruntime 四章中的位置

```diagram flow
- 类型系统（第 113 章）：xrttype 描述——值操作/比较/散列的事实源
- 对象与图（第 114 章）：引用计数对象——容器可作负载获得对象身份
- 动态调用（第 115 章）：callable 值——可作容器元素流通
- typed 容器（本章）：描述驱动的六族容器——描述指针相等才互操作
```

## 示例

### 第一个完整程序：typed 数组全操作

下面的程序来自 `examples/runtime/typed_array`——Init/Push/Clone/Concat/Equals/IndexOf 的闭环：

```embed path="extlibs/xruntime/examples/runtime/typed_array/main.c" title="extlibs/xruntime/examples/runtime/typed_array/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/typed_array/main.c -lws2_32 -liphlpapi
（输出 count/joined/index 的数组操作自检行）
```

**刚才发生了什么。** ① `xrtTypedArrayInit(&Values, xrtTypeInt64())`——**内建 int64 的规范描述**驱动：后续 Push 复制 int64（描述的 Ops 说了算）、Equals 按 int64 比较——容器代码不含任何 int64 特定逻辑。② 三元素 Push 后 `Clone` 深复制、`Concat` 拼接（Values+Copy=6 元素）、`Equals(Values, Copy)` 判等——所有权动词按共同契约运作（Clone 深复制不共享）。③ 打印 count=3、joined=6、index=查到的下标——**运行时类型的数组行为与 C 数组完全同构**，差异只在"类型来自描述"。换成 `xrtTypeString()` 或自定义类型——同一组 API（描述驱动的一切）。

### 第二个完整程序：对象字段字典

第二个程序来自 `examples/runtime/typed_dict`——动态字段的实体：

```embed path="extlibs/xruntime/examples/runtime/typed_dict/main.c" title="extlibs/xruntime/examples/runtime/typed_dict/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/typed_dict/main.c -lws2_32 -liphlpapi
（输出文本键字典操作的自检行）
```

**刚才发生了什么。** ① 文本键 + 描述驱动值类型的字典——Set/Take/Remove 按共同契约；键是字符串（内建 STRING 描述）、值任意（这里演示的类型）。② 这个形态正是第 114 章 `xrtdynamicfields` 的载荷——**宿主对象的动态字段表 = 对象图中的字典节点**：对象图追踪字典节点、字典的值经 `xrtTypeValue()` 追踪运行时对象引用——三章（114/113/116）在此合流。③ 配套示例族覆盖六族全体（`typed_stack/list/tree/set`）加队列三形态（`typed_queue_spsc/mpsc/mpmc`——第 21 章拓扑专用队列的 typed 版）与 `typed_value_containers`（Value 装箱容器）——**每种底层结构一个样本**是六族契约的验收面。

## 契约

- **描述借用**：六族借用不可变 xrttype；注册与容器生命期间稳定（第 113 章纪律）。
- **指针相等**：跨容器操作要求描述指针相同——ID/名称/大小/操作表相同不足；内建与宿主类型用进程期规范描述。
- **值操作分派**：全部经类型描述的值操作族（描述表驱动）——容器零类型特定代码。
- **所有权动词**：Set 族复制 / SetTake 失败原子转移 / Take 族移动 / Remove 族逐值 Drop / Clone 族深复制。
- **地址合法**：值地址限外部或本容器准确活动槽；结构/元数据/容量/填充/槽中间拒绝；移动输出来源在容器外。
- **失败原子**：可见状态失败不变；多步先工作存储后提交；OOM 恒 XERR_MEMORY、底层错误保留为原因。
- **实现复用**：直接复用 XRT 容器内部快速路径——typed 壳零性能税、算法一份。
- **普通 C 容器边界**：C 容器不带引用计数与对象身份——弱引用/环回收经 runtime_object 把容器作负载组合。
- **六族特有**：array 连续/stack LIFO/list 稀疏序/tree 比较序/set 唯一/dict 文本键——共同契约之上各按结构特化。

## 避坑

### 坑 1：两个"相同类型"的描述指针不同就互操作

症状：Concat/比较两个元素类型"明明一样"的容器被拒——各自模块静态声明了同名类型。

原因：指针相等是硬规则——内容相同不证明生命 ABI 与私有上下文可互换。统一使用进程期规范描述（内建 `xrtTypeInt64()`/注册表的进程期描述）。

```c bad
/* 模块 A、B 各自 static 声明的 Counter 描述 */
xrtTypedArrayConcat(&A_Array, &B_Array);   /* 描述指针不同：拒绝 */
```

```c good
/* 全进程一个描述来源：注册表取进程期描述 */
const xrttype* T = host_registry_find("app.Counter");
xrtTypedArrayInit(&A, T); xrtTypedArrayInit(&B, T);
xrtTypedArrayConcat(&A, &B);   /* 同指针：可互操作 */
```

### 坑 2：Clone 之后共享了"以为独立"的值

症状：克隆容器后改一个、另一个跟着变——值里藏着引用/句柄类元素。

原因：Clone 是**深复制**——但"深"到什么程度由元素类型的 Ops 定义：引用类型元素（对象值）的复制是**强引用增持**（第 114 章 ObjectValueOps 的 Copy=Ref）——对象本体仍是共享的（引用语义类型本该如此）。期望"连对象都分开"需要元素级自定义 Clone。

```c bad
pCopy = xrtTypedArrayClone(&Objs);   /* 对象数组克隆：引用+1 非新对象 */
mutate(pCopy);                        /* 本体变了：原容器也"变" */
```

```c good
/* 引用语义是特性：多容器共享同一对象（图追踪负责回收） */
/* 确要独立副本：元素类型提供深 Clone 的 Ops */
```

### 坑 3：typed 容器当对象图根用（漏追踪）

症状：容器持有的对象引用没被对象图追踪——环泄漏。

原因：普通 typed 容器**不带**引用计数与对象身份——图追踪的是 runtime_object（第 114 章）。正解：容器作为对象负载（容器本身是对象）、或用动态字段节点（dict 载荷经类型值操作追踪）。

```c bad
/* 裸 typed array 持对象引用，期望图自动发现 */
xrtTypedArrayPush(&Arr, &ObjRef);   /* 图不知道 Arr 的存在 */
```

```c good
/* 把容器做成对象（负载持容器）→ 对象入图 → InstanceOps.Trace 枚举容器元素
   或用 xrtdynamicfields（字段字典节点自带追踪） */
```

## 练习

### 基础：六族巡检

六种容器各跑最小闭环（Init→两种操作→Destroy）——同一元素类型（int64）。验收标准：所有权动词跨族行为一致；失败原子性（OOM 模拟）各族可见状态不变。

### 进阶：多态列表

元素类型 `xrtTypeValue()`（第 31 章动态值装箱）的 list——存 int/string/object 三种值并遍历打印（Kind 分派）。验收标准：三种值共存；深克隆后独立（值语义元素）；排序按 Value 比较。

### 挑战：对象字段引擎

`xrtdynamicfields` 风格的字段表：对象（114 章）+ typed dict 载荷 + set/get/枚举字段 + 对象图追踪（自引用字段的环收集）。验收标准：字段的强引用被 Trace 枚举；自引用环在安全点回收；字段类型混布（int/string/对象）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 定位 | 六种底层结构共享一套运行时值契约——类型驱动、复用 XRT 快速路径 |
| 描述纪律 | 借用不可变描述；跨容器操作指针相等（内容同不足） |
| 值分派 | 全经类型描述的值操作族——容器零类型特定代码 |
| 所有权 | Set 复制/SetTake 原子转移/Take 移动/Remove 逐 Drop/Clone 深复制 |
| 失败原子 | 可见状态不变；先工作存储后提交；OOM 恒 MEMORY 保原因 |
| 六族 | array 连续/stack LIFO/list 稀疏/tree 比较序/set 唯一/dict 文本键 |
| 与 xvalue | Value=单动态值（异构树）；typed=同构集合（元素零标签开销） |
| 对象身份 | C 容器无引用计数——对象语义经 runtime_object 组合 |
| 追踪闭环 | 动态字段=dict 载荷+Value 追踪——114/113/116 合流 |
| 队列三态 | spsc/mpsc/mpmc 的 typed 版——第 21 章拓扑选择延续 |
