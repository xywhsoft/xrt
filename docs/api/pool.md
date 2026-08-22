# 内存池 API

## 分层模型

内存池由浅入深分为三层：

| 层 | 类型 | 适用场景 |
|---|---|---|
| 单页 | `xpoolpage` | 精确控制一个 1 到 256 槽固定对象页，或搭建更高层分配器 |
| 固定池 | `xpool` | 同一种结构或节点的长期高频分配，按约 64 KiB 目标自适应页容量并自动跨页增长 |
| 变长池 | `xmempool` | 同一生命周期域内的多尺寸内存，小块池化，大块独立登记 |

旧版 `MemUnit` 的 256 槽默认布局、空闲槽复用、满页边界和显式标记回收被保留；重复实现多页固定块管理的 `BSMM` 与 `FSMemPool` 合并为 `xpool`。多页固定池不再把每种对象都机械放大为 256 个槽：小对象仍保持 256 槽吞吐，大对象按目标页字节数降低槽数。`xmempool` 继续采用 16 字节尺寸类和默认 1024 字节分界，但删除了未参与当前分配路径的二叉树、LUT 和对象前 4 字节头。

三个层次都可独立裁剪。`XRT_FEATURE_POOL` 依赖 `XRT_FEATURE_POOL_PAGE`，`XRT_FEATURE_MEMORY_POOL` 依赖前两层。

## 共同契约

### 所有权与安全

池只接受自己当前持有的活动指针。释放外部指针、槽内部指针、另一个池的指针或已经释放的指针会返回 `false`，不会读取用户指针前方内存。释放后地址可以立即复用，旧指针随即失效。

`Init` / `Unit` 用于栈上或嵌入结构，`Create` / `Destroy` 用于堆上池对象。同一结构再次 `Init` 前必须先 `Unit`；直接重复初始化会丢失原资源。`Unit` 和 `Destroy` 不调用用户对象析构器；需要逐对象清理时，应先使用 `Visit` 或在上层保存生命周期信息。

公共结构用于栈上初始化和诊断，调用方不得直接修改字段。池对象和其返回的地址在 `Unit` / `Destroy` 后全部失效。

### 对齐

默认对齐是 16 字节。显式对齐必须是非零二次幂。对象大小会向对齐值取整，但 `xpoolpageinfo.ItemSize` 和 `xpoolinfo.ItemSize` 始终保留用户对象大小。

### 线程

三层池都不包含锁，同一时间只能由一个线程或执行上下文操作。不同池可以并行使用。需要跨线程共享时，应由上层同步或使用并发模块提供的共享包装，不在热路径中隐式加锁。

旧版 `XRT_OBJMODE_LOCAL` 会检查线程归属，`XRT_OBJMODE_SHARED` 会在每次池操作中隐式加锁。这项能力没有被遗漏，而是从通用池热路径中移除：线程归属检查不能覆盖协程迁移，共享模式又让所有调用永久承担同步分支和锁开销。新版本将“无锁线程内池”和“显式同步共享包装”分层，调用成本和并发边界都更清晰。

`tests/containers/test_container_external_sync.c` 让四个线程通过同一外部 Mutex 组合使用固定池和四类基础容器，并验证分配/释放计数相等、活动对象归零。该测试直接承接旧 Phase 2 的 shared allocator/container 场景。

### 标记回收

池只提供显式标记与扫描，不查找根，也不遍历对象图：

1. 调用方从自己的根集合出发，对全部可达块调用 `Mark`。
2. `Sweep` 释放未标记块，并清除幸存块的标记。
3. 下一轮必须重新标记。

`FreeMarked` 是独立的选择性批量释放操作，只释放已标记块。旧版含义不清的 `GC(pool, boolean)` 不再存在。

### 错误

池错误使用稳定域 `xrt.pool`：

| 代码 | 常量 | 含义 |
|---|---|---|
| 1 | `XPOOL_ERROR_INVALID_POINTER` | 指针不是该池的精确槽起点或活动大块 |
| 2 | `XPOOL_ERROR_NOT_ALLOCATED` | 槽已经空闲 |
| 3 | `XPOOL_ERROR_PAGE_FULL` | 单页配置的槽全部使用，错误种类为 `XERR_AGAIN` |
| 4 | `XPOOL_ERROR_INVALID_ALIGNMENT` | 对齐不是有效二次幂 |
| 5 | `XPOOL_ERROR_INVALID_SIZE` | 固定槽大小为零 |
| 6 | `XPOOL_ERROR_INDEX_OUT_OF_RANGE` | 单页槽索引超出已建立范围 |
| 7 | `XPOOL_ERROR_VISIT_ACTIVE` | 活动访问器期间尝试改变分配集合或嵌套访问 |
| 8 | `XPOOL_ERROR_INVALID_CAPACITY` | 显式页槽数不在 1 到 256 范围内 |

参数错误使用 `XERR_ARGUMENT`，大小计算溢出使用 `XERR_RANGE`，底层分配失败使用 `XERR_MEMORY`。`Owns`、`Size`、`GetInfo`、`Get` 和合法的空遍历是查询接口，不因“未找到”设置错误。

## 常量

### `XRT_POOL_PAGE_CAPACITY`

单页支持的最大槽数，也是快捷单页入口的默认槽数，值为 256。

### `XRT_POOL_PAGE_BYTES_DEFAULT`

多页固定池的默认目标页字节数，值为 65536。自动布局使用
`clamp(65536 / stride, 1, 256)` 选择每页槽数，其中 `stride` 是对齐后的实际槽步长。
因此小对象保持 256 槽，大对象不会仅因进入固定池就预留 256 倍对象大小的首批内存。
当单个槽已经超过 64 KiB 时，一页只包含一个槽。

### `XRT_POOL_ALIGNMENT_DEFAULT`

默认对象对齐，值为 16。

### `XRT_MEMPOOL_CLASS_STEP`

变长池的小块尺寸类步长，值为 16。

### `XRT_MEMPOOL_CUTOFF_DEFAULT`

变长池默认小块分界，值为 1024。

## 单页池

### `xpoolpage`

`Allocation` 和 `Memory` 分别是底层分配地址与对齐后的槽区；`ItemSize`、`Stride`、`Alignment`、`MemorySize` 和 `Capacity` 描述布局。`Used` 与 `Marked` 是外置位图，因而不占用户对象空间。`FreeList`、`LiveCount`、`NextIndex` 和 `FreeCount` 管理分配状态。四个链指针、`Parent` 和 `Flags` 供上层固定池维护页关系，不得由调用方修改。

### `xpoolpageinfo`

`ItemSize` 是用户对象大小，`Stride` 是实际槽步长，`Alignment` 是对齐；`LiveCount`、`FreeCount` 和 `Capacity` 分别表示活动槽、可直接复用槽和总槽数。

### `xrtPoolPageInit` / `xrtPoolPageInitAligned`

初始化调用方提供的 256 槽页结构。对象大小必须非零。失败后结构保持可安全 `Unit` 的零状态。

### `xrtPoolPageInitLayout`

使用显式对象大小、对齐和 1 到 256 的槽数初始化页。适合大对象、低延迟分配器或需要精确控制单页占用的上层模块。

### `xrtPoolPageCreate` / `xrtPoolPageCreateAligned`

创建 256 槽页结构和槽区。失败返回 `NULL`。

### `xrtPoolPageCreateLayout`

创建使用显式对齐和槽数的页。失败返回 `NULL`。

### `xrtPoolPageUnit` / `xrtPoolPageDestroy`

`Unit` 只释放页持有的槽区，`Destroy` 还释放由 `Create` 返回的页结构。

### `xrtPoolPageAlloc` / `xrtPoolPageCalloc`

分配一个槽。`Calloc` 清零 `ItemSize` 字节。页满时返回 `NULL` 和 `XERR_AGAIN`，不会自动扩页。

### `xrtPoolPageFree` / `xrtPoolPageFreeAt`

分别按精确对象指针或槽索引释放活动槽。成功返回 `true`。

### `xrtPoolPageGet` / `xrtPoolPageIndex`

`Get` 按索引返回活动对象，空闲或越界返回 `NULL`。`Index` 把活动对象转换为槽索引；输出参数不能为 `NULL`。

### `xrtPoolPageOwns`

仅当指针是该页当前活动槽的起始地址时返回 `true`。

### `xrtPoolPageMark` / `xrtPoolPageSweep` / `xrtPoolPageFreeMarked`

提供单页显式标记、释放未标记槽和释放已标记槽。两个释放函数返回释放数量。

### `xrtPoolPageReset`

一次释放全部活动槽并返回数量；页恢复到新建后的分配状态，槽内容不清零。

### `xrtPoolPageGetInfo`

复制页状态。输出参数为空时不执行操作；页无效时输出零结构。

## 固定对象池

### `xpool`

`Pages` 是全部页链，`Available` 是可分配页链，`Index` 是按槽区地址排序的安全查找索引。`ItemSize`、`Alignment`、`PageCapacity`、`PageCount`、`EmptyPages`、`LiveCount`、`PeakCount`、`AllocCount`、`FreeCount` 和 `RetainEmpty` 提供布局、统计与保留策略。`IndexCapacity` 和 `Flags` 是内部状态。

### `xpoolinfo`

包含对象大小、步长、对齐、每页槽数、页数、空页数、实时/峰值对象数、总容量和累计分配/释放次数。理论容量无法用 `size_t` 表示时，`Capacity` 饱和为 `SIZE_MAX`。

### `xpoolvisitor`

签名为 `bool visitor(ptr object, size_t index, ptr userData)`。`index` 是本次遍历从零开始的连续序号，不是可持久化句柄。返回 `false` 提前停止。

### `xrtPoolInit` / `xrtPoolInitAligned`

初始化固定对象池，初始不分配页，默认保留一个空页。对象大小必须非零。每页槽数按 `XRT_POOL_PAGE_BYTES_DEFAULT` 自动选择。

### `xrtPoolInitLayout`

使用显式对象大小、对齐和每页槽数初始化固定池。槽数必须在 1 到 256 范围内。显式布局可在较少页与较低单次页分配延迟之间作出领域相关的选择。

### `xrtPoolCreate` / `xrtPoolCreateAligned`

创建堆上固定对象池，并使用自动页容量。

### `xrtPoolCreateLayout`

创建使用显式对齐和每页槽数的堆上固定对象池。

### `xrtPoolUnit` / `xrtPoolDestroy`

释放全部页和索引；不会逐个调用对象清理逻辑。

### `xrtPoolAlloc` / `xrtPoolCalloc`

从可用页分配对象，必要时自动创建新页。`Calloc` 清零对象的 `ItemSize` 字节。

### `xrtPoolFree` / `xrtPoolOwns`

安全释放对象或查询活动所有权。释放导致空页数超过 `RetainEmpty` 时，会立即回收多余页。

### `xrtPoolMark` / `xrtPoolSweep` / `xrtPoolFreeMarked`

对整个固定池执行显式标记和两种明确的释放模式，返回释放对象数。扫描后按当前保留策略裁剪空页。

### `xrtPoolReset`

释放全部活动对象，保留不超过 `RetainEmpty` 个空页，返回对象数。

### `xrtPoolTrim` / `xrtPoolSetRetain`

`Trim` 立即把空页裁剪到指定数量并返回释放页数，但不修改自动保留策略。`SetRetain` 修改自动策略并立即裁剪。

### `xrtPoolGet`

复制固定池诊断状态，不分配内存。

### `xrtPoolVisit`

访问当前活动对象并返回已调用回调的数量。遍历期间可以查询和调用 `xrtPoolMark`。分配、释放、扫描、重置、裁剪、改变保留策略、释放池和嵌套访问都会以 `XPOOL_ERROR_VISIT_ACTIVE` 拒绝。保护状态会同步到全部页，因此经公共结构取得页后直接调用页级修改接口也不能让迭代器持有悬空页。

## 变长内存池

### `xmempool`

`Buckets` 是 16 字节尺寸类数组，`Pages` 是所有小块页的有序查找索引，`Large` 是独立大块哈希登记表。`Cutoff`、`ClassCount`、`PageCount`、`LargeCount`、`LiveCount`、`PeakCount`、`LiveBytes`、`PeakBytes`、`AllocCount` 和 `FreeCount` 是配置与统计。其余容量、删除计数和 `Flags` 是内部状态。

### `xmempoolinfo`

包含分界、尺寸类步长/数量、页数、小块/大块数量、实时/峰值块数、实时/峰值可用字节及累计操作次数。小块字节按尺寸类可用大小统计，不是原请求大小。

### `xmempoolvisitor`

签名为 `bool visitor(ptr memory, size_t size, size_t alignment, ptr userData)`。`size` 是 `xrtMemPoolSize` 可查询的安全可用大小，返回 `false` 提前停止。

### `xrtMemPoolInit` / `xrtMemPoolCreate`

建立变长池。`cutoff == 0` 使用 1024；其他值是池化小块的最大请求大小。尺寸类数量为 `ceil(cutoff / 16)`，初始化时只建立尺寸类状态，不分配槽页。

### `xrtMemPoolUnit` / `xrtMemPoolDestroy`

释放全部小块页、独立大块、登记表和尺寸类。不会调用块内对象析构器。

### `xrtMemPoolAlloc`

默认按 16 字节对齐。零大小与 `xrtMalloc(0)` 一致，仍返回至少一个可用字节。请求不大于 `cutoff` 时进入 `ceil(size / 16)` 尺寸类，否则作为独立大块登记。

### `xrtMemPoolCalloc`

分配 `count * size` 字节并清零。乘法溢出失败；总大小为零时仍分配并清零至少一个字节。

### `xrtMemPoolAllocAligned`

接受二次幂对齐。对齐不大于 16 且大小不超过分界时使用尺寸类，更大对齐使用独立大块。零大小仍有效。

### `xrtMemPoolRealloc`

`memory == NULL` 等同 `Alloc`；非空块的新大小为零时释放并返回 `NULL`。同一小块尺寸类内或独立大块缩小时保持地址；增长时分配新块并复制 `min(oldUsableSize, newSize)` 字节。失败时原块保持有效。

小块不保存原请求大小，因此 `oldUsableSize` 和 `xrtMemPoolSize` 返回尺寸类大小。独立大块缩小只改变逻辑可用大小，不保证立即缩减底层保留内存。

### `xrtMemPoolFree` / `xrtMemPoolOwns` / `xrtMemPoolSize`

`Free` 安全释放活动块。`Owns` 查询活动所有权。`Size` 对小块返回尺寸类可用大小，对大块返回逻辑大小，未找到返回零。

### `xrtMemPoolMark` / `xrtMemPoolSweep` / `xrtMemPoolFreeMarked`

标记和扫描同时覆盖尺寸类小块与独立大块。`Sweep` 清除幸存标记；两个释放函数返回总块数。

### `xrtMemPoolReset`

释放全部活动块，释放所有独立大块，并让每个曾使用的尺寸类最多保留一个空页。登记表和尺寸类配置继续复用；登记表槽会恢复为空状态，不会留下墓碑并在下一次分配时触发无意义扩容。

### `xrtMemPoolTrim`

把每个尺寸类的空页裁剪到指定数量，返回释放页总数。该操作不申请内存，也不缩小大块登记表，适合内存压力路径。

### `xrtMemPoolGet`

复制变长池诊断状态，不分配内存。

### `xrtMemPoolVisit`

依次访问尺寸类活动块和独立大块，返回已调用回调的数量。遍历顺序不是稳定排序。遍历期间可以查询和标记块；会改变分配集合的操作、释放池和嵌套访问都会以 `XPOOL_ERROR_VISIT_ACTIVE` 拒绝。保护状态同时覆盖内部尺寸类与小块页，不能通过公开诊断字段绕过。

## 旧版资产复用

这三层实现逐项审计了旧版 `memunit.h`、`bsmm.h`、`mempool_fs.h`、`mempool.h` 及其测试、范例和中英文文档。

保留并加强的资产：

- `MemUnit` 的 256 槽默认容量、释放槽复用、满页失败和“释放标记/释放未标记”两种回收语义。
- `FSMemPool` 的跨页增长、满页与可用页迁移、空页缓存策略。
- `MemPool` 的小块分级、可配置分界和大块回退路径。
- 旧测试中的尺寸边界、256 槽边界、跨页增长、复用与 GC 场景，已改为无需人工查看输出的自动断言。

新增的自适应页容量解决了旧实现的大对象放大问题：显式单页仍可选择旧版 256 槽布局，通用多页池则不会为一个 8 KiB 对象立即申请约 2 MiB，也不会为一个 1 MiB 对象立即申请约 256 MiB。上层 AVLTree 等稳定地址容器直接复用这一能力，无需重复实现大对象旁路。

明确淘汰的实现：

- 用户内存前方的 4 字节管理头。新实现用外置位图、页地址索引和大块哈希登记，避免读取未知指针前方内存，并完整支持显式对齐。
- `BSMM` 和 `FSMemPool` 两套重叠的多页管理器。其有效能力统一进入 `xpool`，不保留重复 API 和实现。
- 旧变长池未参与最终热路径的树字段与重复 LUT 状态。新尺寸类可直接通过除法定位。
- 打印内部字段、依赖人工判断结果的旧测试形式。有效场景保留，测试方式升级为自动失败。

## 范例与回归

完整范例位于：

- `examples/memory/pool_page/main.c`
- `examples/memory/pool/main.c`
- `examples/memory/memory_pool/main.c`

三个裁剪层可分别执行：

```text
python tools/build.py --suite pool_page --compiler gcc --arch native
python tools/build.py --suite pool --compiler gcc --arch native
python tools/build.py --suite memory_pool --compiler gcc --arch native
```

构建脚本同时运行普通实现、范例和对应单头文件测试。
