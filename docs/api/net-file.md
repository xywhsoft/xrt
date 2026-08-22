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
