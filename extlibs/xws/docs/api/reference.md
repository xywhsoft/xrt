# xws API 参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xws/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `68` 个函数、`40` 个常量或宏、
`15` 个公共类型。

## `extlibs/xws/include/xrt/websocket_runtime.h`

[查看带契约注释的公共头](../../include/xrt/websocket_runtime.h)

### 函数 (68)

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
- `xrtWsConnValid`
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

### 常量与宏 (38)

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
- `XWS_FEATURE_WEBSOCKET_CONNECTION`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_REF`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_TLS`
- `XWS_FEATURE_WEBSOCKET_WRITER`
- `XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE`
- `XWS_FEATURE_WEBSOCKET_WRITER_REF`

### 类型 (15)

- `xwsconn`
- `xwsconnclose`
- `xwsconncloseflag`
- `xwsconnconfig`
- `xwsconnerror`
- `xwsconnevents`
- `xwsconnstate`
- `xwsconnwait`
- `xwsdeflate`
- `xwsdeflaterconfig`
- `xwsinflaterconfig`
- `xwsmessageinfo`
- `xwsopcode`
- `xwsrole`
- `xwswriter`

## `extlibs/xws/include/xws/export.h`

[查看带契约注释的公共头](../../include/xws/export.h)

### 常量与宏 (2)

- `XWS_BUILD_SHARED`
- `XWS_USE_SHARED`
