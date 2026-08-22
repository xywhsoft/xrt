# 内存统计 API

## 裁剪与开销

定义 `XRT_FEATURE_MEMORY_STATS` 才会编译统计状态和堆内埋点。未定义时所有埋点由空宏消除。编译功能后，运行时默认关闭；关闭时只执行一次原子开关读取。

开启后，高频计数按当前线程哈希分散到 16 个槽，尺寸类使用独立短锁。`xrtMemStatsGet`、`xrtMemStatsReset` 和 `xrtMemStatsEnable` 会锁住全部槽，形成一致且可解释的统计边界。

## 统计口径

`MallocCalls`、`CallocCalls`、`ReallocCalls`、`MemDupCalls` 和 `FreeCalls` 是公开 API 请求数，对应字节字段保存请求大小。溢出的 `calloc` 请求以 `SIZE_MAX` 作为饱和值。

`BlockAllocCalls` 与 `BlockFreeCalls` 记录全局堆的逻辑块流量。一次跨尺寸类 `realloc` 可能产生一个新块和一次旧块释放；同尺寸类原地调整不会产生新块。

`PooledAllocCalls`、`DirectAllocCalls` 和尺寸类数组反映全局堆选择的逻辑通道。`DirectAlloc` 表示超过池化上限、直接拥有独立原始块的用户分配。

`BackingAllocCalls`、`BackingReallocCalls` 和 `BackingFreeCalls` 才表示 XRT 对进程级底层分配器的实际请求；字节字段包含块头、对齐余量、尺寸类 span、临时 arena 和其他内部原始块。它们与逻辑用户字节不能混为一谈，也不承诺每次逻辑池化分配都会触发 backing 请求。

启用内存调试时，大块释放会先进入隔离队列，因此 `BlockFreeCalls` 和公开 `FreeCalls` 可以先于 `BackingFreeCalls` 增长。隔离队列淘汰或 `xrtMemDebugReset` 真正归还原始块时，才记录 backing free。尺寸类缓存和 arena 保留也会使一段采样区间内的 backing 分配与释放数量不相等。

## 类型

### `xmemstats`

快照包含运行时开关、尺寸类元数据、公开 API 请求、临时内存请求、逻辑块流量、池化/直通通道、实际 backing 分配器请求，以及每个尺寸类的调用数和请求字节数。数组有效长度由 `ClassCount` 给出。

## 函数

### `xrtMemStatsEnable`

开启或关闭运行时统计。切换与所有已经进入统计临界区的调用形成线性化顺序。

### `xrtMemStatsEnabled`

原子读取当前运行时统计开关。

### `xrtMemStatsReset`

清空所有计数但保留开关状态。返回后开始的操作不会被清除。

### `xrtMemStatsGet`

获取字段相互一致的快照。输出参数为 `NULL` 时设置 `XERR_ARGUMENT`。

## 旧版资产决策

旧版 `xrtMemTelemetry` 的公开请求计数、临时内存计数、尺寸类直方图以及启用后重置语义全部保留，并由独立模块、并发槽和单头文件测试承接。

旧版 `pooled candidate` 与 `fallback` 只描述请求尺寸，容易被误读为底层分配器调用。新版把它细化为逻辑堆的 `PooledAlloc`、`DirectAlloc`，并另外记录真实的 `BackingAlloc`、`BackingRealloc` 与 `BackingFree`。这样既能分析用户请求，也能观察 span、arena 和调试快照产生的实际底层成本。

旧版 `test_memtelemetry_baseline.h` 只串行调用其他模块测试并打印结果，没有断言、稳定输入或性能阈值，因此不再作为正确性门禁保留。统计 API 仍可由各模块基准直接采样；后续性能门禁由对应模块提供固定负载、阈值和独立结果，避免统计模块反向耦合协程、网络、HTTP 与 WebSocket。

## 范例

完整范例位于 `examples/memory/stats/main.c`，并由 `python tools/build.py --suite memory_stats` 自动编译运行。
