# Executor

`Executor` 是独立可裁剪的高吞吐 detached 工作执行器。它不创建 `Future`，不传递返回值，
也不提供运行中任务的强制取消；需要结果、错误传播或协作取消时应使用 `TaskPool`。

## 性能契约

- 创建时按 `Threads * QueueLimit` 预分配全部作业槽。
- 提交、批量提交、窃取和完成不申请通用堆内存。
- 每个 Worker 独立维护双端队列，本地从新端执行，空闲 Worker 从旧端窃取。
- `QueueLimit` 是每个 Worker 的硬上限；满载立即返回 `XERR_AGAIN`。
- 批量提交在一个 Worker 队列中全成或全败，成功后才整体转移数据析构责任。

## 所有权

`xrtExecutorSubmit` 和 `xrtExecutorSubmitBatch` 成功后接管每项 `Destroy`。工作执行完成，或
`xrtExecutorCancel` 丢弃尚未开始的工作时，析构恰好执行一次。提交失败时执行器不会调用析构。

## 生命周期

- `xrtExecutorClose`：停止受理，让已受理工作自然排空。
- `xrtExecutorCancel`：停止受理并丢弃排队工作，运行中的工作继续返回。
- `xrtExecutorWait*`：只能在关闭后、非本执行器 Worker 上调用。
- `xrtExecutorDestroy`：执行关闭、排空、线程回收和内存释放。

## 示例

```c
xexecutor* pExecutor = xrtExecutorCreate(NULL);

xrtExecutorSubmit(pExecutor, run, pData, destroy, pContext);
xrtExecutorClose(pExecutor);
xrtExecutorWait(pExecutor);
xrtExecutorDestroy(pExecutor);
```
