# 原生异步文件 I/O

`net_file` 把普通文件定位读写提交到 Network Engine 的完成端口。它用于文件服务器、
代理缓存和自定义存储管线，不创建 Future、不占用 TaskPool，也不复制调用方载荷。

## 生命周期

- 使用 `xrtNetFileOpen` 打开文件；函数自动附加 `XFILE_ASYNC`。
- 同一文件在首次提交后固定由同一个 Worker 使用。
- 文件、缓冲和 `xnetcompletion` 必须保持到操作产生唯一终态事件。
- 全部操作终结后使用 `xrtClose` 关闭文件。
- `xrtNetFileCancel` 只请求取消；资源仍在原操作终态回调后释放。

Windows 使用 IOCP `ReadFile`/`WriteFile`，Linux 使用 io_uring `READV`/`WRITEV`。
其他后端通过能力位明确报告不支持，不会退化为阻塞 Worker。

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

[file_tour](../../examples/network/file_tour/main.c) · 请求取消

```c
				if ( !xrtNetFileCancel(Task.pWorker,
						iCancelId) ||
					!exampleSpin(&CancelIo,
						xrtDeadlineAfter(
							3000000ull)) ) {
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
