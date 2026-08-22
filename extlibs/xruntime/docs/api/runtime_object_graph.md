# 运行时对象图

`runtime_object_graph` 在引用计数对象之上提供可裁剪的强引用环收集能力，面向宿主
对象、callable 环境、容器和插件对象图。它保留引用计数在确定性析构、C 扩展所有权和
弱引用方面的优势，只在调用方选择的安全点处理单纯引用计数无法回收的环。

该模块属于 `xruntime` 扩展，不进入 XRT 核心发布面。

## 启用与成本

启用 `XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH` 会依赖 `XRUNTIME_FEATURE_RUNTIME_OBJECT`：

```c
#include <xrt/runtime_object_graph.h>
```

该功能关闭时，`xrtobject` 不包含图指针、链表指针或收集状态，也不会链接收集器代码。
功能打开时，每个对象最多属于一个图；图只借用对象，不增加常驻强引用。

## 类型追踪

可参与收集的类型通过 `xrtinstanceops.Trace` 精确枚举负载直接拥有的强对象引用：

```c
typedef struct node {
	xrtobject* Next;
} node;

static bool nodeTrace(
	const void* Value,
	const xrttype* Type,
	xrtobjectvisitor Visit,
	ptr Context
)
{
	const node* Node = (const node*)Value;
	(void)Type;

	return (Node->Next == NULL) || Visit(Node->Next, Context);
}
```

该回调安装到 `xrttype::InstanceOps`，而不是处理对象引用槽位的 `Ops`。

每个实际强引用槽位必须访问一次。两个字段拥有同一个对象时访问两次；弱引用、借用指针
和空槽位不能访问。`Drop` 必须释放 `Trace` 报告的全部强引用。违反该契约会造成收集拒绝
或对象泄漏，因此语言字段、容器元素和闭包捕获都应复用同一份类型元数据来生成追踪与销毁。

## 图成员

`xrtObjectGraphCreate` 创建空图，`xrtObjectGraphDestroy` 摘除全部成员后销毁图，不销毁
仍存活的对象。图必须比任何仍可能调用它的线程存活更久。

`xrtObjectGraphTrack` 幂等加入活动对象；对象已属于另一图时返回 `XERR_EXISTS`。
`xrtObjectGraphUntrack` 摘除成员，不属于该图时返回 `false` 且不设置错误。对象通过普通
最后强引用终结时会自动摘除。`xrtObjectGraphContains` 和 `xrtObjectGraphCount` 是加锁的瞬时查询。

同一个图上的成员操作可并发。调用方必须持有被传对象的有效强引用，并保证同一对象不会
同时在不同图上执行成员操作。`Destroy` 与其他图操作互斥。

## 收集

`xrtObjectGraphCollect` 使用引用计数自动识别外部根并收集不可达环。算法先取得对象快照，
再以地址哈希表执行两遍追踪：第一遍统计图内入边，第二遍从根传播可达性，时间复杂度为
`O(N + E)`，临时空间为 `O(N)`。

```c
xrtobjectgraphresult Result;

if ( !xrtObjectGraphCollect(Graph, &Result) ) {
	return false;
}
printf("collected=%zu\n", Result.CollectedCount);
```

对象强引用数大于图内入边数时，该对象是外部根。未跟踪对象持有的引用也自然表现为外部
引用。`xrtObjectGraphCollectRoots` 还接受 `xrtobjectrootproc`，用于补充语言栈、生成器、
挂起任务或宿主状态中的借用根；根枚举器可以访问未跟踪对象，收集器只标记属于当前图的对象。
根回调只声明本轮收集期间的可达性，不会增加持久强引用；调用方必须保证枚举出的借用根在
收集结束前始终存活。

外部 `xvalue` 所有权图需要特别处理：Value 外壳引用和容器 COW backing 引用不会逐项增加
内部运行时对象的强引用，因此不能仅靠自动根推断。启用 `runtime_value_roots` 后，单值使用
`xrtObjectGraphCollectValueRoot`，栈槽、全局槽和挂起帧批量使用
`xrtObjectGraphCollectValueRoots`。这两个入口复用 `xrtValueTraceRuntimeObjects`，并把失败
包装为 `xrt.object-graph -> xrt.runtime-value` 原因链。

成功结果字段：

| 字段 | 含义 |
| --- | --- |
| `TrackedCount` | 收集开始时的快照对象数量。 |
| `EdgeCount` | 快照对象之间被追踪到的强引用边数量。 |
| `RootCount` | 自动根和显式根去重后的图内根数量。 |
| `CollectedCount` | 本次实际终结并摘除的对象数量。 |

结果指针可为空。失败时调用方提供的结果结构保持原值。

## 安全点

收集必须由上层运行时安排在对象图静止的安全点：

- 不得并发修改跟踪成员、强引用字段或相关强引用计数。
- `Trace`、根枚举器和 `Drop` 不得启动新的图收集。
- 普通对象的独立引用与弱引用操作仍是线程安全的，但不能与收集同一批对象并发。

图锁只保护成员链表，不会在用户 `Trace`、根枚举器或 `Drop` 回调期间持有。XRT 不用一把
全局锁假装任意对象负载可安全并发读取；宿主调度器应在任务、Future 和生成器状态稳定后
进入安全点，再调用收集。

## 失败原子性

临时内存分配、全部类型追踪、根枚举、入边一致性和快照引用计数检查都在任何负载析构前
完成。任一阶段失败时，图成员、强弱引用状态、对象负载和结果输出保持不变。只有全部候选
同时取得终结权后，收集器才摘除对象并执行一次 `Drop`；弱引用从终结开始即不可提升。

OOM 保留 `XERR_MEMORY`。类型追踪失败保留 `xrt.type` 及其下层原因；根枚举失败保留宿主
错误。错误最终包装到稳定域 `xrt.object-graph`。

收集器会隔离调用前的线程错误。成功时恢复调用方原有错误并丢弃 `Trace` 或根枚举器在成功路径
留下的临时错误；失败时丢弃调用前旧错误，只保留本次收集产生的错误和原因链。回调返回
`false` 时应尽量设置具体错误，未设置时收集器会生成稳定的对象图错误。

## 错误

`xobjectgrapherror` 定义对象图模块的稳定错误代码，错误域为 `xrt.object-graph`：

| 代码 | 常量 | 用途 |
| --- | --- | --- |
| 1 | `XOBJECT_GRAPH_ERROR_ARGUMENT` | 空图、空对象或空根。 |
| 2 | `XOBJECT_GRAPH_ERROR_TRACK` | 对象归属或存活状态错误。 |
| 3 | `XOBJECT_GRAPH_ERROR_TRACE` | 追踪失败、空强引用或入边不一致。 |
| 4 | `XOBJECT_GRAPH_ERROR_STATE` | 安全点内图、状态或引用计数发生变化。 |
| 5 | `XOBJECT_GRAPH_ERROR_ROOTS` | 宿主根枚举失败。 |

## 动态字段节点

`xrtdynamicfields` 是对象图中的独立字典节点。宿主对象只需追踪它持有的字段对象，字段对象的 `xtypeddict<Value>` 载荷再通过 `xrtTypeValue()` 追踪值图中的运行时对象引用。该分层避免收集器对普通 `xvalue` 字典递归猜测所有权，也避免同一字段表在多个路径中重复计边。

动态字段写入会隔离 Value 容器 backing；自引用字段可以直接形成 `DynamicFields -> DynamicFields` 边，并由安全点收集正常回收。具体所有权和 API 见 `docs/api/runtime_dynamic_field.md`。

## 历史资产

本模块保留旧版 `lib/type.h`、`lib/value.h` 与 `test/test_value_weak.c` 已验证的引用计数、
弱引用、环收集、容器和闭包追踪思路，同时修复旧实现的全局无锁注册表、线性目标查找、
只追踪动态字段和直接读取公开动态值引用计数等问题。旧版环、自环、外部根、弱引用和失败
边界已扩展为常规、契约、OOM、并发、示例和单头测试。

通用 C 宿主、容器、callable、Future 和异步状态的统一对象模型见
`docs/design/runtime_object_model.md`。
