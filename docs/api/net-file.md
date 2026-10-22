# 原生异步文件 I/O

`net_file` 把普通文件定位读写提交到 Network Engine 的完成端口。它用于文件服务器、
代理缓存和自定义存储管线，不创建 Future、不占用 TaskPool，也不复制调用方载荷。

## 生命周期

- 使用 `xrtNetFileOpen` 打开文件；函数自动附加 `XFILE_ASYNC`。
- 同一文件在首次提交后固定由同一个 Worker 使用。
- 文件、缓冲和 `xnetcompletion` 必须保持到操作产生唯一终态事件。
- 全部操作终结后使用 `xrtClose` 关闭文件。
- `xrtNetFileCancel` 只请求取消；资源仍在原操作终态回调后释放。
- 成功受理借用 I/O 后，该轮端口保持不透明，Engine 的闭合拥有图认证必须拒绝；
  不能把任意调用方 Completion／缓冲猜成受管理引用。拒绝的提交不改变此状态，
  原生端口退休后恢复认证。该约束不影响普通 C I/O 的执行和显式停止。
- 文件 EOF 是成功的零字节读取：`Result == XNET_RESULT_OK`、`Bytes == 0`、
  `SystemCode == 0`。Windows 的真实 EOF 完成包必须先取走，才能释放 OVERLAPPED
  操作存储；不能把文件 EOF 误当作 TCP 关闭或传输错误。

Windows 使用 IOCP `ReadFile`/`WriteFile`，Linux 使用 io_uring `READV`/`WRITEV`。
其他后端通过能力位明确报告不支持，不会退化为阻塞 Worker。

这些入口仍是显式借用的 C 接口，不因上述修正而成为拥有型 Future。语言接入
需要独立的原生拥有记录、取消／完成唯一终态及 checked 文件退休，不能仅在
Completion 外套一个 Promise 并借用调用方内存。

### 拥有型 `net_file_future`

独立模块 `net_file_future` 复用上述原生校验／提交 primitive，但使用自己的真实
文件、操作、Future producer、CancelWatch 与 Engine runtime 注册槽。它不走
借用入口再清除端口封口标记，也不创建任务池或辅助线程。只使用低层 `net_file`
不会拉入 Future、通用 Buffer 或这套拥有记录。

| 入口 | 合同 |
|---|---|
| `xrtNetFileCreate(engine, worker, path, options)` | 查询实际后端 FILE_IO 后预备控制存储／拥有目录，再打开文件；不支持时不创建／截断文件 |
| `xrtNetFileRef`／`Destroy` | 原生强引用；Destroy 仅释放引用，不等待、不隐式关闭 |
| `xrtNetFileReadFuture(file, offset, size, cancel)` | 定位读取，结果为 Future 唯一拥有的 `xbuffer*`；空文件及 EOF 为成功空缓冲 |
| `xrtNetFileWriteFuture(file, offset, input, cancel)` | 复制输入快照，再异步定位写入；Future 值为 `(uintptr_t)` 实际字节数 |
| `xrtNetFileWriteRefPolicyV1(...)` | 稳定的不可变输入拥有合同，成功受理消费一份 Owner 引用，失败不消费；无需再次复制载荷 |
| `xrtNetFileCloseFuture(file)` | 拒绝新提交、请求取消、等真实终态及物理关闭；每次取得同一预分配 Future 的一个观察引用 |

构造的文件打开是调用线程上的同步准备操作，不能标注 `nonblocking`，网络回调
应提前准备文件。定位读写才使用真正的完成式后端。没有隐藏的后端替换或线程池
文件执行；路径仍为原生文件路径，不将 VFS 的同步后端伪装成完成式 I/O。

读结果通过 `xrtFutureValue` 借用，保留 Future 到最后一次访问；C 调用者可显式
使用 `xrtBufferTake` 移交分配（这会改变共享 Future 中的 Buffer，必须自己同步
其他观察者）。语言桥应仅在自己的私有原生结果上移交一次，再缓存拥有的语言
bytes 值，不能让每次 await 都从同一 Buffer 取走数据。

零长操作仍检查文件状态、权限与 int64 偏移边界；有效零长不提交 OS I/O。
预取消不执行 I/O；已提交取消只请求系统取消，资源保持到唯一真实完成包，
成功完成可以赢过取消。失败不交付半成品读取；部分写入不会回滚文件变化。
Close 的终态在原生文件关闭及 Engine 运行占用释放后发布，随后可以立即 Stop。
物理关闭错误保留为同一个失败 Future，不 abort；排队拒绝可通过再次 Close 重试。

`xrtNetFileOwnership`、两组精确 adapter／quiescence getter 及 runtime、producer、
payload、cancel policy getter 提供封闭原生 family。Count 使用真实强引用数；
File 对 Promise 与 CloseFuture 的两个槽即使指向同一控制块也分别报告。OS
完成队列只借用实际操作记录，File 的实际在途注册槽拥有操作，不新增影子目录。
未知输入 policy 在派发其 Ops 前拒绝。被上层规划器证明不可达、且已取得 claim 的
File 可在 Prepare 中授权真实原生关闭：排队命令借用已有 runtime 注册槽；物理
关闭取走该槽后，将同一引用转交回调尾部释放，不另建强引用根。未完成操作仍
保持到真实 OS 完成，不用合成取消或就绪 Future 代替。上层执行资源须先结算文件
关闭，再退休 Engine；普通 C 调用者只丢最后一个观察引用仍不等于显式关闭。

当前 `tests/network/test_net_file_future.c` 覆盖真实读写／NUL／零长／EOF、输入
快照与保留拥有引用、取消、并发提交／关闭、观察者提前丢弃、真实引用图、
清理回调中的独立 freeze、不可达准备授权、首次后端分配失败以及 128 个构造／
读写准备分配预算。独立 XLIR 规划器另验证真实不可达图，而非用手动 claim 测试
代替图证明。
调试用例检查完整资源退出后的精确活动内存与错误释放账本。测试不产生应用
网络流量，也不进行网络安全专项；未将这些证据计作 xlang 语言／发布产品验收。

`tests/network/net_file_boundary_cases.h` 由已登记的 `test_net_file.c` 执行：
真实普通文件的读写、文件末端与末端之后读取、拒绝提交、拥有图封口和端口
退休后恢复。打开自身新建的测试文件，不访问应用网络；调试构建另验证实际
分配失败不污染拥有状态及精确内存账本。

## API

### `xrtNetFileOpen`

打开可提交到完成端口的文件；自动附加 `XFILE_ASYNC`，不改变调用方选项对象。

```c
xfile xrtNetFileOpen(
	cstr sPath,
	const xfileoptions* pOptions
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 文件路径 |
| `pOptions` | 输入 | 允许空 | 空 = `xrtFileOptionsInit` 默认值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 异步文件句柄 | — |
| 空 | 打开失败 | `xrt.file` 域错误 |

#### 错误

- `xrt.file` 域错误 — 路径非法、权限不足或共享冲突（同 `xrtFileOpen`）

#### 范例

[file_tour](../../examples/network/file_tour/main.c) · 打开

```c
		File = xrtNetFileOpen(sFile, &Options);
```


### `xrtNetFileRead`

从绝对偏移读取到调用方缓冲，返回非零操作标识；只能在所属 Worker 执行。

```c
uint64 xrtNetFileRead(
	xnetworker* pWorker,
	xfile File,
	uint64 iOffset,
	void* pData,
	size_t iSize,
	xnetcompletion* pCompletion
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空、当前线程 | 目标 Worker |
| `File` | 输入 | `xrtNetFileOpen` 产物 | 异步文件句柄 |
| `iOffset` | 输入 | — | 绝对读取偏移 |
| `pData` | 输入/输出 | 非空 | 调用方缓冲，保持到终态 |
| `iSize` | 输入 | > 0 | 读取字节数 |
| `pCompletion` | 输入 | 非空且 `Proc` 非空 | 终态回调，保持到唯一终态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非零 | 操作标识，与终态事件 `Id` 对应 | — |
| `0` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — Worker、文件、Completion（含 Proc）、缓冲为空或长度为 0
- `XERR_STATE` — 不在所属 Worker 线程，或文件未附加异步/缺少对应访问标志
- `xrt.net` / `XNET_ERROR_PORT_SUBMIT`（`XERR_RANGE`） — 偏移 + 长度超出完成事件可表达范围
- `XERR_UNSUPPORTED` — Worker 端口后端无原生文件 I/O 能力（如 SELECT 后端）

#### 范例

[file_tour](../../examples/network/file_tour/main.c) · 定位读取

```c
	pTask->iId = xrtNetFileRead(pTask->pWorker, pTask->File,
		pTask->iOffset, (void*)pTask->pData, pTask->iSize,
		&pTask->pIo->Completion);
```


### `xrtNetFileWrite`

把调用方缓冲写入绝对偏移，返回非零操作标识；热路径不复制载荷。

```c
uint64 xrtNetFileWrite(
	xnetworker* pWorker,
	xfile File,
	uint64 iOffset,
	const void* pData,
	size_t iSize,
	xnetcompletion* pCompletion
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空、当前线程 | 目标 Worker |
| `File` | 输入 | 含写入访问 | 异步文件句柄 |
| `iOffset` | 输入 | — | 绝对写入偏移 |
| `pData` | 输入 | 非空 | 载荷缓冲，保持到终态 |
| `iSize` | 输入 | > 0 | 写入字节数 |
| `pCompletion` | 输入 | 非空且 `Proc` 非空 | 终态回调，保持到唯一终态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非零 | 操作标识，与终态事件 `Id` 对应 | — |
| `0` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — Worker、文件、Completion（含 Proc）、缓冲为空或长度为 0
- `XERR_STATE` — 不在所属 Worker 线程，或文件未附加异步/缺少对应访问标志
- `xrt.net` / `XNET_ERROR_PORT_SUBMIT`（`XERR_RANGE`） — 偏移 + 长度超出完成事件可表达范围
- `XERR_UNSUPPORTED` — Worker 端口后端无原生文件 I/O 能力（如 SELECT 后端）

#### 范例

[file_tour](../../examples/network/file_tour/main.c) · 定位写入

```c
	pTask->iId = xrtNetFileWrite(pTask->pWorker, pTask->File,
		pTask->iOffset, pTask->pData, pTask->iSize,
		&pTask->pIo->Completion);
```


### `xrtNetFileCancel`

在所属 Worker 请求取消操作；原操作仍通过 Completion 产生唯一终态。

```c
bool xrtNetFileCancel(
	xnetworker* pWorker,
	uint64 Id
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空、当前线程 | 目标 Worker |
| `Id` | 输入 | 非零 | 要取消的操作标识 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 取消请求已受理 | — |
| `false` | 参数非法或不在所属线程 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — Worker 为空或 `Id` 为 0
- `XERR_STATE` — 不在所属 Worker 线程

#### 范例

参见已注册的 [examples/network/file_tour/main.c](../../examples/network/file_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtNetFileCancel(Task.pWorker,
						iCancelId)
```
## 示例

```c
static void onFile(xnetworker* pWorker,
	const xnetportevent* pEvent, ptr pData)
{
	/* pEvent->Type、Result、Bytes 和 Id 描述唯一终态。 */
}

static void start(xnetworker* pWorker, ptr pData)
{
	xfile File = (xfile)pData;
	static char Buffer[4096];
	static xnetcompletion Completion;

	xrtNetCompletionInit(&Completion, onFile, NULL);
	(void)xrtNetFileRead(pWorker, File, 0,
		Buffer, sizeof(Buffer), &Completion);
}
```

## 模块契约：错误

失败经 `xrtGetError()` 报告：

| 域/种类 | 触发场景 |
|---|---|
| `XERR_ARGUMENT` / `XERR_STATE` / `XERR_RANGE` | 参数、Worker 归属与偏移范围 |
| `XERR_UNSUPPORTED` | Worker 端口后端无原生文件 I/O 能力 |
| `xrt.net` / `XNET_ERROR_PORT_SUBMIT` | 提交失败（`RANGE`/`UNSUPPORTED` 等 kind） |
| `xrt.file` 域错误 | 打开失败（同 `xrtFileOpen`） |
