# Core API

`core` 是 XRT 唯一不可裁剪的公共底座，提供版本、固定宽度类型、借用视图和
引用计数。它不需要显式初始化，也不依赖线程、容器、文件或网络模块。

```c
#include <xrt/core.h>
```

聚合入口 `<xrt.h>` 会自动包含核心、[内存](memory.md)和[错误](error.md)
三个头文件。

## 版本与公共宏

| 名称 | 含义 |
| --- | --- |
| `XRT_VERSION_MAJOR` | 主版本号 |
| `XRT_VERSION_MINOR` | 次版本号 |
| `XRT_VERSION_PATCH` | 修订版本号 |
| `XRT_VERSION_TEXT` | 零结尾版本字符串 |
| `XRT_NPOS` | `size_t` 查找接口共用的未找到值 |
| `XRT_API` | 静态库、导出库和导入库共用的符号规则 |
| `XRT_EXTERN_C_BEGIN` / `XRT_EXTERN_C_END` | C++ 调用时使用 C 链接 |

`xrtVersion()` 返回 `XRT_VERSION_TEXT`。结果是静态借用字符串，不得释放。

## 基础类型

`int8`、`uint8`、`int16`、`uint16`、`int32`、`uint32`、`int64` 和 `uint64`
固定宽度。`ptr` 是可写无类型指针，`str` / `cstr` 是可写和只读 UTF-8
零结尾字符串，`bytes` / `cbytes` 是可写和只读字节指针。

`xtime` 是有符号 64 位 Unix Epoch 微秒，可直接用作 FFI 和跨模块时间标量。
单调时钟不使用该类型，避免把持续时间误当成绝对日期。

## 资源边界

`xrtresourcelimits` 为解析器、压缩器、归档器和其他可能放大输入的模块提供统一边界：

| 字段 | 含义 |
| --- | --- |
| `iMaxInputBytes` | 最多消费的输入字节数 |
| `iMaxOutputBytes` | 最多生成的输出字节数 |
| `iMaxItemBytes` | 单个条目、字段或文件的最大字节数 |
| `iMaxEntries` | 最大条目数 |
| `iMaxNodes` | 最大语法树或对象节点数 |
| `iMaxDepth` | 最大嵌套深度 |
| `iMaxCompressionRatio` | 最大输出输入比；输入为零时由具体模块拒绝非空输出 |
| `iFlags` | 显式允许符号链接、硬链接、设备文件或外部实体 |

`xrtResourceLimitsInit()` 会先清零整个结构，再写入适合处理不受信任输入的保守默认值。
调用方可在初始化后调整字段；某个限制字段为零表示不限制该项目。`iSize` 和 `iVersion`
必须保持初始化后的值，扩展库据此拒绝不完整或未知的结构。

```c
xrtresourcelimits Limits;

xrtResourceLimitsInit(&Limits);
Limits.iMaxInputBytes = 16u * 1024u * 1024u;
Limits.iMaxDepth = 32u;
```

允许危险资源必须显式设置 `XRT_RESOURCE_ALLOW_SYMLINKS`、
`XRT_RESOURCE_ALLOW_HARDLINKS`、`XRT_RESOURCE_ALLOW_DEVICE_FILES` 或
`XRT_RESOURCE_ALLOW_EXTERNAL_ENTITIES`。具体模块只解释与自身相关的标志。

## 借用视图

```c
typedef struct xbytesview {
	cbytes Data;
	size_t Size;
} xbytesview;

typedef struct xstrview {
	cstr Data;
	size_t Size;
} xstrview;
```

两个 View 都只借用内存，不拥有数据，也不要求末尾补零。`Size == 0` 时
`Data == NULL` 合法。`XRT_BYTES_INIT` / `XRT_STR_INIT` 用于静态聚合初始化，
`XRT_BYTES_LITERAL` / `XRT_STR_LITERAL` 用于赋值和函数实参。

这四个宏按 `sizeof(array) - 1` 计长，只能接受编译期数组或字符串字面量，
不能用于指针：

```c
static const xstrview Protocols[] = {
	XRT_STR_INIT("h2"),
	XRT_STR_INIT("http/1.1")
};

xstrview Method = XRT_STR_LITERAL("GET");
```

## 引用计数

```c
int32 xrtRefRetain(volatile int32* pCount);
int32 xrtRefRelease(volatile int32* pCount);
```

对象把初始计数设为一。`Retain` 原子加一并返回新值，`Release` 原子减一并
返回新值；返回零的线程负责析构对象。空指针、非正计数、复活已经归零的对象、
释放非正计数及递增 `INT32_MAX` 都返回 `-1`，且不修改计数。

这两个函数只保护计数，不发布对象字段，也不替代对象自己的并发契约。对象在
交给其他线程前仍必须通过锁、原子发布或 XRT 已声明的线程安全 API 建立
happens-before。

完整范例位于 `examples/core/reference/main.c`。

## 生命周期

XRT 2.0 的核心按需初始化内部进程资产，不再要求每个程序配对调用
`xrtInit()` / `xrtUnit()`。有状态模块都由自己的创建和销毁函数管理对象，
进程级分配器和共享缓存保持到进程结束。

旧版公开可变 `xCore` 把应用路径、错误回调、分配器、近似比较策略、线程状态
和内存池混入一个结构，导致模块边界与裁剪失效。新版将这些能力归入各自模块，
不保留第二套全局兼容入口。

## 旧版资产决策

新版保留旧 `xrt.h` / `base.h` 中简短类型名称、微秒 `xtime`、原子引用及
无需复杂对象即可使用的风格，并补齐固定宽度、只读指针和显式借用 View。
旧 `test_base.h` 的有效引用边界由核心单元测试和并发测试承接；可变全局状态、
隐式字符串所有权和初始化引用计数被明确退役。
