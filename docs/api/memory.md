# 内存 API

## 内存范围

`xrtMemRangeValid()` 验证半开范围 `[Data, Data + Size)` 的起点和末地址计算；空范围允许空指针。`xrtMemRangesOverlap()` 使用地址差判断两个非空范围是否重叠，不计算可能回绕的末地址。调用重叠判断前，调用方应先分别验证两个范围。

这两个函数是头文件内联的零分配基础能力，适合协议解析器、视图类型和扩展库统一执行边界检查。

## 设计契约

所有 XRT 动态内存最终经过同一个进程级底层分配器。默认实现使用 C 运行库分配器；嵌入方可以在第一次 XRT 分配之前替换它。首次原始分配后分配器冻结，避免同一地址由不同分配器交叉释放。安装、读取和首次分配使用同一短临界区，并发发生时只有一个顺序获胜，不会观察到撕裂配置。

`xrtMalloc(0)` 和 `xrtCalloc(0, n)` 仍返回一块可释放的非空内存。`xrtRealloc(p, 0)` 释放 `p` 并返回 `NULL`。所有返回地址至少按 16 字节对齐；该保证不依赖平台 C 分配器在 32 位环境下的最低对齐值。

小于等于 1024 字节的分配沿用旧版 16 字节尺寸类与 span 复用，并由每个原生
线程惰性缓存少量空闲块。缓存按批从中央链补给，超过上限时批量归还；线程退出
时通过 FLS 或 pthread key 析构完整归还。外部 C 线程无需先附加到 XRT。TLS
资源或缓存元数据申请失败只会退回中央链，不会让原本可完成的用户分配失败。
显式内存池的用户区始终从底层全局分配起点之后开始，因此首个池对象也不会与
可直接传给 `xrtFree` 的地址发生别名。

## 类型

### `xallocator`

```c
typedef struct xallocator {
	ptr Context;
	xallocproc Alloc;
	xreallocproc Realloc;
	xfreeproc Free;
} xallocator;
```

三个回调必须成套提供。回调负责原始内存，不得再次调用 XRT 分配 API。`Context` 原样传给每个回调，生命周期必须覆盖进程使用 XRT 的完整时期。回调可能由多个线程并发执行。

## 函数

### `xrtSetAllocator`

在首次分配前安装底层分配器。成功返回 `true`；参数不完整产生 `XERR_ARGUMENT`，分配器已经冻结产生 `XERR_STATE`。

### `xrtGetAllocator`

将当前分配器复制到调用方结构。输出参数不能为 `NULL`。

### `xrtMalloc`

分配 `iSize` 字节。失败返回 `NULL` 并设置 `XERR_MEMORY`。

### `xrtCalloc`

分配 `iCount * iSize` 字节并清零。乘法溢出返回 `NULL` 并设置 `XERR_RANGE`。

### `xrtRealloc`

调整由 XRT 分配的内存。成功时保留前 `min(oldSize, newSize)` 字节；失败时原内存仍由调用方持有。

### `xrtFree`

释放 XRT 全局堆内存，允许传入 `NULL`。非调试构建与 C 的 `free` 一样，传入非 XRT 地址或显式内存池对象属于调用方错误；内存调试构建先查询全局堆所有权，安全报告 `XERR_ARGUMENT`，并且不会改变原对象。显式池对象必须交回创建它的池。

### `xrtMemDup`

复制任意二进制数据。`iSize` 非零时 `pData` 不能为 `NULL`。

### `xrtSecureZero`

以不可被普通死存储优化删除的逐字节写入清除敏感数据。空区间允许空指针；
非空区间的空指针设置 `XERR_ARGUMENT`。

### 调试位置函数

启用 `XRT_FEATURE_MEMORY_DEBUG` 后，公开分配宏自动转到 `xrtMallocAt`、
`xrtCallocAt`、`xrtReallocAt`、`xrtFreeAt` 和 `xrtMemDupAt`，记录调用文件
与行号。普通代码继续使用短名称，不应直接拼装调试块头。

## 范例

```c
unsigned char source[] = { 1, 2, 3, 4 };
unsigned char* copy = (unsigned char*)xrtMemDup(source, sizeof(source));

if ( copy == NULL ) {
	return 1;
}
xrtFree(copy);
```

完整范例位于 `examples/core/memory/main.c`。

## 旧版资产决策

新版保留旧 `base.h` / `memglobal.h` 已经验证的零大小处理、16 字节尺寸类、
同类原位 `realloc`、跨类数据保留、小块 span 复用、每线程缓存及批量中央链
交换。旧公开 `xCore` 分配器函数指针、线程对象内嵌缓存布局和必须显式
`xrtUnit()` 才能清理的状态被替换为不可见进程级堆与独立线程缓存；分配器
配置使用独立 API，模块不能直接改写全局函数指针。

旧 `test_base.h` 与 `test_memglobal_core.h` 的有效边界由 `test_memory`、
`test_heap`、`test_heap_threads` 和 OOM 回归承接，并新增并发分配器冻结、
32 位对齐、溢出原子失败、未附加原生线程、线程退出缓存归还和调试归属诊断。
