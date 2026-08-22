# WebSocket 连接组

`websocket_group` 是 WebSocket Connection 之上的可选并发容器。它负责唯一成员、引用生命周期、硬容量、封闭状态和稳定快照，不负责握手、路由、消息编码或隐式关闭。

## 模块

- 裁剪宏：`XRT_MODULE_WEBSOCKET_GROUP`
- 特性宏：`XRT_FEATURE_WEBSOCKET_GROUP`
- 依赖：`websocket_connection`、`set`、`mutex`
- 头文件：`<xrt/websocket_group.h>`；`<xrt.h>` 也会包含它
- 批量裁剪宏：`XRT_MODULE_WEBSOCKET_GROUP_FUTURE`
- 批量特性宏：`XRT_FEATURE_WEBSOCKET_GROUP_FUTURE`
- 批量依赖：`websocket_group`、`websocket_connection_ref_future`、`future`

## 生命周期

`xrtWsGroupCreate(Limit)` 创建连接组。`Limit == 0` 表示没有显式成员上限。组本身通过 `xrtWsGroupRef` 和 `xrtWsGroupDestroy` 共享。

`xrtWsGroupAdd` 成功后持有一个 Connection 引用；重复加入同一指针保持成功，但不会形成第二个成员或引用。`xrtWsGroupRemove`、`xrtWsGroupClear` 和最终 `xrtWsGroupDestroy` 归还这些引用。它们都不会关闭或中止 Connection。`Clear` 在组锁内用空集合原子替换成员集，随后在锁外归还旧成员引用，因此大量成员析构、最后引用释放或分配器工作不会长期占用组锁。

`xrtWsGroupHas` 查询指定 Connection 是否属于组，`xrtWsGroupCount` 返回当前成员数，`xrtWsGroupLimit` 返回创建时的硬上限；三者都是加锁取得的瞬时快照。`xrtWsGroupSealed` 返回永久封闭状态。查询不会增加 Connection 引用，也不能替代需要稳定遍历的 Snapshot。

成员达到非零 `Limit` 时，新成员返回 `false`，错误为 `XERR_AGAIN / XWS_GROUP_ERROR_CAPACITY`。这表示移除成员后可以重试，不是协议错误。

## 封闭

`xrtWsGroupSeal` 是永久且幂等的：封闭后不能加入新成员，但重复加入已有成员仍成功，移除、清空和创建快照仍可使用。服务器停止接收新会话后应先封闭连接组，再决定对现有会话执行优雅关闭或立即中止。

`xwsgrouperror` 的稳定错误码为：`XWS_GROUP_ERROR_ARGUMENT` 表示对象或参数范围无效，`XWS_GROUP_ERROR_MEMORY` 表示分配失败，`XWS_GROUP_ERROR_STATE` 表示引用或封闭状态不允许操作，`XWS_GROUP_ERROR_CAPACITY` 表示成员硬上限已满，`XWS_GROUP_ERROR_RANGE` 表示快照或操作索引越界。

## 快照

`xrtWsGroupSnapshotCreate` 按成员加入顺序复制当前成员，并为每个成员增加引用。连续快照存储在组锁外分配；重新取得锁后若成员增长超过容量便释放并重试，容量足够时才在线性化点保序增持成员，因此大块分配不会阻塞 `Add/Remove/Clear`。快照不持有组；即使原组随后销毁，快照成员仍保持有效。

使用 `xrtWsGroupSnapshotCount` 和 `xrtWsGroupSnapshotGet` 借用成员，最后调用 `xrtWsGroupSnapshotDestroy`。越界访问返回 `NULL` 和 `XERR_RANGE / XWS_GROUP_ERROR_RANGE`。

Group、Snapshot 和批量 Operation 都是 opaque 对象。全部公开入口会在解引用前验证固定头或按成员数计算出的完整连续存储，地址回绕以 `XERR_ARGUMENT / XWS_GROUP_ERROR_ARGUMENT` 拒绝。`Destroy(NULL)` 保持无操作；其它需要对象的空参数按参数错误处理。

## 基本用法

```c
xwsgroup* pGroup = xrtWsGroupCreate(10000);

/* Upgrade Open 回调。 */
xrtWsGroupAdd(pGroup, pConnection);

/* Connection Close 回调。 */
xrtWsGroupRemove(pGroup, pConnection);

/* 管理线程读取稳定成员集。 */
xwsgroupsnapshot* pSnapshot = xrtWsGroupSnapshotCreate(pGroup);
for ( size_t i = 0; i < xrtWsGroupSnapshotCount(pSnapshot); i++ ) {
	xwsconn* pConnection = xrtWsGroupSnapshotGet(pSnapshot, i);
	(void)pConnection;
}
xrtWsGroupSnapshotDestroy(pSnapshot);

xrtWsGroupDestroy(pGroup);
```

## 批量 Future

`websocket_group_future` 是独立裁剪层。它在调用开始时取得稳定快照，然后为每个成员调用既有 Connection Future API；基础 `websocket_group` 因此仍不依赖 Future，也不改变成员关系。

- `xrtWsGroupSendAsync`、`xrtWsGroupTextAsync`、`xrtWsGroupBinaryAsync`：完整消息广播。非空负载只复制一次，再用共享 Ref 分发；明文服务端可以继续零复制，客户端掩码和 TLS 仍按各自传输要求复制。
- `xrtWsGroupSendRefAsync`、`xrtWsGroupTextRefAsync`、`xrtWsGroupBinaryRefAsync`：共享调用方 Ref。Ref 固定结构允许未对齐，实现先复制局部快照；结构和完整负载不能覆盖 Group 私有存储。只有成功返回 `xwsgroupop` 才接管来源；即使组为空也会接管并释放一次。地址、操作码、UTF-8、分配或提交前置检查失败并返回 `NULL` 时，来源始终归调用方且不会调用 `Release`。
- `xrtWsGroupPingAsync`、`xrtWsGroupPongAsync`、`xrtWsGroupCloseAsync`：对稳定快照提交控制操作。
- `xrtWsGroupWaitAsync`：批量等待 `WRITE`、`DRAIN` 或 `CLOSE`。

批量提交不是全有或全无事务。某些 Connection 可能已经接纳 Future，另一些可能因队列硬上限、关闭或 OOM 同步拒绝。`xwsgroupop` 必须保留这种事实：`xrtWsGroupOpCount` 是稳定快照数，`xrtWsGroupOpAccepted` 和 `xrtWsGroupOpRejected` 描述提交结果，`xrtWsGroupOpDoneCount` 在操作锁内由拒绝数、接纳数和剩余数 O(1) 计算并发快照；`xrtWsGroupOpResult` 按快照顺序返回每个成员的 `REJECTED/PENDING/RESOLVED/FAILED/CANCELLED/CLOSED` 状态。Result 输出允许未对齐，实现通过局部结构一次发布；输出必须是完整范围且不能覆盖 Operation 自身。同步拒绝错误由操作对象持有，不依赖调用线程之后的错误槽。

`xrtWsGroupOpRef` 增加批量操作引用，`xrtWsGroupOpDestroy` 释放引用。`xrtWsGroupOpFutureRef` 返回完成 Future，`xrtWsGroupOpItemFutureRef` 返回指定成员的 Future 强引用。正常情况下，完成 Future 在全部已接纳成员进入任意终态后成功完成；它不把某个成员失败伪装成整个批量操作失败，调用方应检查逐项结果。对完成 Future 调用 `xrtFutureCancel` 会立即取消聚合等待并向全部尚未结束的成员传播协作取消请求；成员仍在各自 Worker 上决定最终状态。需要等待成员真正终止时，应保留逐成员 Future，或调用 `xrtWsGroupOpCancel` 后继续观察逐项状态。

`xwsgroupopstate` 的逐成员状态为 `XWS_GROUP_OP_REJECTED`、`XWS_GROUP_OP_PENDING`、`XWS_GROUP_OP_RESOLVED`、`XWS_GROUP_OP_FAILED`、`XWS_GROUP_OP_CANCELLED` 和 `XWS_GROUP_OP_CLOSED`。同步拒绝没有 Future；其余状态与对应成员 Future 的生命周期一致。

`xrtWsGroupOpWait`、`xrtWsGroupOpWaitFor`、`xrtWsGroupOpWaitUntil` 和 `xrtWsGroupOpWaitUntilCancel` 是完成 Future 的同步便利层。不得从这些 Connection 所属的网络 Worker 上执行阻塞等待。

## 批量示例

```c
xwsgroupop* pOperation = xrtWsGroupTextAsync(
	pGroup,
	XRT_STR_LITERAL("service-ready")
);

if ( pOperation != NULL ) {
	(void)xrtWsGroupOpWait(pOperation);
	for ( size_t i = 0; i < xrtWsGroupOpCount(pOperation); i++ ) {
		xwsgroupopresult Result;

		if ( xrtWsGroupOpResult(pOperation, i, &Result) ) {
			/* 按应用策略处理同步拒绝和异步终态。 */
		}
	}
	xrtWsGroupOpDestroy(pOperation);
}
```

HTTP Router 自动挂接仍不属于连接组：Router 的生命周期和事件转发需要独立适配器，不能通过替换部分回调破坏用户已有的 Message、Ping、Error 或 Close 处理。
