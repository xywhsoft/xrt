# xws API 参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xws/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `133` 个函数、`72` 个常量或宏、
`35` 个公共类型。

## `extlibs/xws/include/xrt/websocket_group.h`

[查看带契约注释的公共头](../../include/xrt/websocket_group.h)

### 函数 (39)

- `xrtWsGroupAdd`
- `xrtWsGroupBinaryAsync`
- `xrtWsGroupBinaryRefAsync`
- `xrtWsGroupClear`
- `xrtWsGroupCloseAsync`
- `xrtWsGroupCount`
- `xrtWsGroupCreate`
- `xrtWsGroupDestroy`
- `xrtWsGroupHas`
- `xrtWsGroupLimit`
- `xrtWsGroupOpAccepted`
- `xrtWsGroupOpCancel`
- `xrtWsGroupOpCount`
- `xrtWsGroupOpDestroy`
- `xrtWsGroupOpDoneCount`
- `xrtWsGroupOpFutureRef`
- `xrtWsGroupOpItemFutureRef`
- `xrtWsGroupOpRef`
- `xrtWsGroupOpRejected`
- `xrtWsGroupOpResult`
- `xrtWsGroupOpWait`
- `xrtWsGroupOpWaitFor`
- `xrtWsGroupOpWaitUntil`
- `xrtWsGroupOpWaitUntilCancel`
- `xrtWsGroupPingAsync`
- `xrtWsGroupPongAsync`
- `xrtWsGroupRef`
- `xrtWsGroupRemove`
- `xrtWsGroupSeal`
- `xrtWsGroupSealed`
- `xrtWsGroupSendAsync`
- `xrtWsGroupSendRefAsync`
- `xrtWsGroupSnapshotCount`
- `xrtWsGroupSnapshotCreate`
- `xrtWsGroupSnapshotDestroy`
- `xrtWsGroupSnapshotGet`
- `xrtWsGroupTextAsync`
- `xrtWsGroupTextRefAsync`
- `xrtWsGroupWaitAsync`

### 常量与宏 (16)

- `XWS_FEATURE_WEBSOCKET_CONNECTION`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE`
- `XWS_FEATURE_WEBSOCKET_GROUP`
- `XWS_FEATURE_WEBSOCKET_GROUP_FUTURE`
- `XWS_GROUP_ERROR_ARGUMENT`
- `XWS_GROUP_ERROR_CAPACITY`
- `XWS_GROUP_ERROR_MEMORY`
- `XWS_GROUP_ERROR_RANGE`
- `XWS_GROUP_ERROR_STATE`
- `XWS_GROUP_OP_CANCELLED`
- `XWS_GROUP_OP_CLOSED`
- `XWS_GROUP_OP_FAILED`
- `XWS_GROUP_OP_PENDING`
- `XWS_GROUP_OP_REJECTED`
- `XWS_GROUP_OP_RESOLVED`

### 类型 (9)

- `xwsconn`
- `xwsconnwait`
- `xwsgroup`
- `xwsgrouperror`
- `xwsgroupop`
- `xwsgroupopresult`
- `xwsgroupopstate`
- `xwsgroupsnapshot`
- `xwsopcode`

## `extlibs/xws/include/xrt/websocket_http.h`

[查看带契约注释的公共头](../../include/xrt/websocket_http.h)

### 函数 (14)

- `xrtWsClientCheck`
- `xrtWsClientConfigInit`
- `xrtWsClientRequestClone`
- `xrtWsClientRequestCreate`
- `xrtWsConnect`
- `xrtWsConnectRequest`
- `xrtWsRequestCreate`
- `xrtWsServerCheck`
- `xrtWsServerConfigInit`
- `xrtWsServerConfigValid`
- `xrtWsServerReject`
- `xrtWsServerReply`
- `xrtWsUpgrade`
- `xrtWsUpgradeAccept`

### 常量与宏 (11)

- `XWS_ACCEPT_CAPACITY`
- `XWS_DEFLATE_MAX_SIZE`
- `XWS_FEATURE_WEBSOCKET_CLIENT`
- `XWS_FEATURE_WEBSOCKET_CLIENT_DEFLATE`
- `XWS_FEATURE_WEBSOCKET_CLIENT_HTTPS`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_TLS`
- `XWS_FEATURE_WEBSOCKET_SERVER`
- `XWS_FEATURE_WEBSOCKET_SERVER_DEFLATE`
- `XWS_FEATURE_WEBSOCKET_SERVER_TLS`
- `XWS_KEY_CAPACITY`

### 类型 (10)

- `xwsclientconfig`
- `xwsclienthandshake`
- `xwsconnconfig`
- `xwsconnectproc`
- `xwsconnevents`
- `xwsdeflate`
- `xwsdeflateacceptproc`
- `xwsserverconfig`
- `xwsserverhandshake`
- `xwsupgradeproc`

## `extlibs/xws/include/xrt/websocket_http_future.h`

[查看带契约注释的公共头](../../include/xrt/websocket_http_future.h)

### 函数 (11)

- `xrtWsConnectAsync`
- `xrtWsConnectRequestAsync`
- `xrtWsConnectRequestSync`
- `xrtWsConnectSync`
- `xrtWsOpenResultConnection`
- `xrtWsOpenResultDestroy`
- `xrtWsOpenResultRef`
- `xrtWsOpenResultResponse`
- `xrtWsOpenResultTakeConnection`
- `xrtWsOpenResultTakeResponse`
- `xrtWsUpgradeAsync`

### 常量与宏 (5)

- `XWS_FEATURE_WEBSOCKET_CLIENT_FUTURE`
- `XWS_FEATURE_WEBSOCKET_HTTP_FUTURE`
- `XWS_FEATURE_WEBSOCKET_SERVER_FUTURE`
- `XWS_OPEN_RESULT_ERROR_ARGUMENT`
- `XWS_OPEN_RESULT_ERROR_STATE`

### 类型 (2)

- `xwsopenresult`
- `xwsopenresulterror`

## `extlibs/xws/include/xrt/websocket_runtime.h`

[查看带契约注释的公共头](../../include/xrt/websocket_runtime.h)

### 函数 (67)

- `xrtWsConnAbort`
- `xrtWsConnAsyncBytes`
- `xrtWsConnAsyncCount`
- `xrtWsConnAttach`
- `xrtWsConnAttachTls`
- `xrtWsConnBegin`
- `xrtWsConnBeginBinary`
- `xrtWsConnBeginBinaryCompressed`
- `xrtWsConnBeginCompressed`
- `xrtWsConnBeginText`
- `xrtWsConnBeginTextCompressed`
- `xrtWsConnBinary`
- `xrtWsConnBinaryAsync`
- `xrtWsConnBinaryCompressed`
- `xrtWsConnBinaryCompressedAsync`
- `xrtWsConnBinaryRef`
- `xrtWsConnBinaryRefAsync`
- `xrtWsConnBinaryTake`
- `xrtWsConnClose`
- `xrtWsConnCloseAsync`
- `xrtWsConnCloseInfo`
- `xrtWsConnConfigInit`
- `xrtWsConnConfigValid`
- `xrtWsConnDeflate`
- `xrtWsConnDestroy`
- `xrtWsConnError`
- `xrtWsConnPause`
- `xrtWsConnPaused`
- `xrtWsConnPending`
- `xrtWsConnPing`
- `xrtWsConnPingAsync`
- `xrtWsConnPong`
- `xrtWsConnPongAsync`
- `xrtWsConnProtocol`
- `xrtWsConnRef`
- `xrtWsConnResume`
- `xrtWsConnRole`
- `xrtWsConnSend`
- `xrtWsConnSendAsync`
- `xrtWsConnSendCompressed`
- `xrtWsConnSendCompressedAsync`
- `xrtWsConnSendRef`
- `xrtWsConnSendRefAsync`
- `xrtWsConnSendTake`
- `xrtWsConnState`
- `xrtWsConnTcp`
- `xrtWsConnTcpRef`
- `xrtWsConnText`
- `xrtWsConnTextAsync`
- `xrtWsConnTextCompressed`
- `xrtWsConnTextCompressedAsync`
- `xrtWsConnTextRef`
- `xrtWsConnTextRefAsync`
- `xrtWsConnTextTake`
- `xrtWsConnTls`
- `xrtWsConnTlsRef`
- `xrtWsConnWaitAsync`
- `xrtWsConnWorker`
- `xrtWsConnWritable`
- `xrtWsWriterDestroy`
- `xrtWsWriterFinish`
- `xrtWsWriterFinishRef`
- `xrtWsWriterFinishTake`
- `xrtWsWriterIsFinished`
- `xrtWsWriterWrite`
- `xrtWsWriterWriteRef`
- `xrtWsWriterWriteTake`

### 常量与宏 (33)

- `XWS_CONN_ASYNC_BATCH_DEFAULT`
- `XWS_CONN_ASYNC_BYTES_DEFAULT`
- `XWS_CONN_ASYNC_COUNT_DEFAULT`
- `XWS_CONN_CLOSED`
- `XWS_CONN_CLOSE_CLEAN`
- `XWS_CONN_CLOSE_RECEIVED`
- `XWS_CONN_CLOSE_REMOTE`
- `XWS_CONN_CLOSE_SENT`
- `XWS_CONN_CLOSE_TIMEOUT_DEFAULT`
- `XWS_CONN_CLOSING`
- `XWS_CONN_CONTROL_RESERVE_DEFAULT`
- `XWS_CONN_ERROR_ARGUMENT`
- `XWS_CONN_ERROR_CONFIG`
- `XWS_CONN_ERROR_FRAME`
- `XWS_CONN_ERROR_LIMIT`
- `XWS_CONN_ERROR_MEMORY`
- `XWS_CONN_ERROR_MESSAGE`
- `XWS_CONN_ERROR_RANDOM`
- `XWS_CONN_ERROR_SEND`
- `XWS_CONN_ERROR_STATE`
- `XWS_CONN_ERROR_TIMEOUT`
- `XWS_CONN_ERROR_TRANSPORT`
- `XWS_CONN_FRAME_LIMIT_DEFAULT`
- `XWS_CONN_MESSAGE_LIMIT_DEFAULT`
- `XWS_CONN_OPEN`
- `XWS_CONN_SEND_LIMIT_DEFAULT`
- `XWS_CONN_WAIT_CLOSE`
- `XWS_CONN_WAIT_DRAIN`
- `XWS_CONN_WAIT_WRITE`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_REF`
- `XWS_FEATURE_WEBSOCKET_WRITER`
- `XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE`
- `XWS_FEATURE_WEBSOCKET_WRITER_REF`

### 类型 (9)

- `xwsconnclose`
- `xwsconncloseflag`
- `xwsconnerror`
- `xwsconnstate`
- `xwsdeflaterconfig`
- `xwsinflaterconfig`
- `xwsmessageinfo`
- `xwsrole`
- `xwswriter`

## `extlibs/xws/include/xrt/websocket_server_router.h`

[查看带契约注释的公共头](../../include/xrt/websocket_server_router.h)

### 函数 (2)

- `xrtWsServerRoute`
- `xrtWsServerRouteConfigInit`

### 常量与宏 (7)

- `XWS_FEATURE_WEBSOCKET_SERVER_ROUTER`
- `XWS_SERVER_ROUTER_ERROR_ARGUMENT`
- `XWS_SERVER_ROUTER_ERROR_CONFIG`
- `XWS_SERVER_ROUTER_ERROR_MEMORY`
- `XWS_SERVER_ROUTER_ERROR_RESPONSE`
- `XWS_SERVER_ROUTER_ERROR_ROUTE`
- `XWS_SERVER_ROUTER_ERROR_STATE`

### 类型 (5)

- `xwsserverrouteconfig`
- `xwsserverrouteerrorproc`
- `xwsserverrouteopenproc`
- `xwsserverroutererror`
- `xwsserverrouterreleaseproc`
