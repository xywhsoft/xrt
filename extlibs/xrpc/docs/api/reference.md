# xrpc public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xrpc/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `72` 个函数、`150` 个常量或宏、
`71` 个公共类型。

## `extlibs/xrpc/include/xrpc/call.h`

[查看带契约注释的公共头](../../include/xrpc/call.h)

### 函数 (10)

- `xrpcBatchCallCreate`
- `xrpcBatchCallFinish`
- `xrpcCallCancellationRequested`
- `xrpcCallConfigInit`
- `xrpcCallCreate`
- `xrpcCallDeadline`
- `xrpcCallDestroy`
- `xrpcCallFinish`
- `xrpcCallMarkSent`
- `xrpcCallPoll`

### 常量与宏 (24)

- `XRPC_CALL_ALLOCATION`
- `XRPC_CALL_ALREADY_DONE`
- `XRPC_CALL_ARGUMENT`
- `XRPC_CALL_BATCH_DONE`
- `XRPC_CALL_CANCELLED`
- `XRPC_CALL_CLOSED`
- `XRPC_CALL_FINISHED`
- `XRPC_CALL_FINISH_ERROR`
- `XRPC_CALL_HTTP_ERROR`
- `XRPC_CALL_LIMIT`
- `XRPC_CALL_MEMORY`
- `XRPC_CALL_NOTIFY_DONE`
- `XRPC_CALL_OK`
- `XRPC_CALL_PENDING`
- `XRPC_CALL_PROTOCOL`
- `XRPC_CALL_REMOTE_ERROR`
- `XRPC_CALL_STATE`
- `XRPC_CALL_TIMEOUT`
- `XRPC_CALL_TRANSPORT`
- `XRPC_FEATURE_XRPC_CALL`
- `XRPC_FEATURE_XRPC_RESPONSE`
- `XRPC_MAY_HAVE_EXECUTED`
- `XRPC_NOT_SENT`
- `XRPC_REPLIED`

### 类型 (9)

- `xrpcbatchcallresult`
- `xrpccall`
- `xrpccallconfig`
- `xrpccallerror`
- `xrpccallfinish`
- `xrpccallresult`
- `xrpccallstatus`
- `xrpcdelivery`
- `xrpcresponses`

## `extlibs/xrpc/include/xrpc/decode.h`

[查看带契约注释的公共头](../../include/xrpc/decode.h)

### 函数 (2)

- `xrpcDecode`
- `xrpcDecodeConfigInit`

### 常量与宏 (18)

- `XRPC_DECODE_DONE`
- `XRPC_DECODE_DUPLEX`
- `XRPC_DECODE_LOCAL_ERROR`
- `XRPC_DECODE_PARSE_ERROR`
- `XRPC_DECODE_REQUESTS`
- `XRPC_DECODE_RESPONSES`
- `XRPC_FEATURE_XRPC_DECODE`
- `XRPC_INVALID_AMBIGUOUS`
- `XRPC_INVALID_DUPLICATE`
- `XRPC_INVALID_EMPTY_BATCH`
- `XRPC_INVALID_ERROR`
- `XRPC_INVALID_ID`
- `XRPC_INVALID_METHOD`
- `XRPC_INVALID_NONE`
- `XRPC_INVALID_OBJECT`
- `XRPC_INVALID_PARAMS`
- `XRPC_INVALID_RESPONSE`
- `XRPC_INVALID_VERSION`

### 类型 (8)

- `xrpcdecodeconfig`
- `xrpcdecodemode`
- `xrpcdecodestatus`
- `xrpcdocument`
- `xrpcinvalidreason`
- `xrpcmessage`
- `xrpcmessagekind`
- `xrpcvalue`

## `extlibs/xrpc/include/xrpc/dispatch.h`

[查看带契约注释的公共头](../../include/xrpc/dispatch.h)

### 函数 (5)

- `xrpcDispatch`
- `xrpcDispatchConfigInit`
- `xrpcReplyAbort`
- `xrpcReplyError`
- `xrpcReplyResult`

### 常量与宏 (7)

- `XRPC_DISPATCH_ABORTED`
- `XRPC_DISPATCH_ARGUMENT`
- `XRPC_DISPATCH_LIMIT`
- `XRPC_DISPATCH_SINK`
- `XRPC_DISPATCH_STATE`
- `XRPC_FEATURE_XRPC_DISPATCH`
- `XRPC_FEATURE_XRPC_ENCODE`

### 类型 (8)

- `xrpcdispatchcall`
- `xrpcdispatchconfig`
- `xrpcdispatchemit`
- `xrpcdispatcherror`
- `xrpcdispatchinfo`
- `xrpcencodeconfig`
- `xrpcoutgoing`
- `xrpcreply`

## `extlibs/xrpc/include/xrpc/encode.h`

[查看带契约注释的公共头](../../include/xrpc/encode.h)

### 函数 (3)

- `xrpcEncodeConfigInit`
- `xrpcEncodeSize`
- `xrpcEncodeWrite`

## `extlibs/xrpc/include/xrpc/export.h`

[查看带契约注释的公共头](../../include/xrpc/export.h)

### 常量与宏 (2)

- `XRPC_BUILD_SHARED`
- `XRPC_USE_SHARED`

## `extlibs/xrpc/include/xrpc/http_client.h`

[查看带契约注释的公共头](../../include/xrpc/http_client.h)

### 函数 (10)

- `xrpcHttpBatchAsync`
- `xrpcHttpCallAsync`
- `xrpcHttpClose`
- `xrpcHttpClosed`
- `xrpcHttpConfigInit`
- `xrpcHttpConnect`
- `xrpcHttpInfo`
- `xrpcHttpNotifyAsync`
- `xrpcHttpRelease`
- `xrpcHttpRetain`

### 常量与宏 (16)

- `XRPC_ADMISSION_ACCEPTED`
- `XRPC_ADMISSION_CLOSED`
- `XRPC_ADMISSION_ERROR`
- `XRPC_ADMISSION_OVERLOADED`
- `XRPC_FEATURE_XRPC_HTTP_CLIENT`
- `XRPC_FEATURE_XRPC_ID`
- `XRPC_FEATURE_XRPC_RESULT_QUOTA`
- `XRPC_HTTP_ALLOCATION`
- `XRPC_HTTP_ARGUMENT`
- `XRPC_HTTP_CLOSED`
- `XRPC_HTTP_CLOSING`
- `XRPC_HTTP_CONNECTING`
- `XRPC_HTTP_ID_EXHAUSTED`
- `XRPC_HTTP_OPEN`
- `XRPC_HTTP_RESPONSE`
- `XRPC_HTTP_STATE`

### 类型 (8)

- `xrpcadmission`
- `xrpchttpclient`
- `xrpchttpconfig`
- `xrpchttperror`
- `xrpchttpinfo`
- `xrpchttpitem`
- `xrpchttpstate`
- `xrpcresponseconfig`

## `extlibs/xrpc/include/xrpc/http_server.h`

[查看带契约注释的公共头](../../include/xrpc/http_server.h)

### 函数 (8)

- `xrpcHttpServerClose`
- `xrpcHttpServerConfigInit`
- `xrpcHttpServerCreate`
- `xrpcHttpServerDestroy`
- `xrpcHttpServerInfo`
- `xrpcHttpServerLocal`
- `xrpcHttpServerStart`
- `xrpcHttpServerWaitFor`

### 常量与宏 (11)

- `XRPC_FEATURE_XRPC_HTTP_SERVER`
- `XRPC_FEATURE_XRPC_SERVER`
- `XRPC_HTTP_SERVER_ARGUMENT`
- `XRPC_HTTP_SERVER_CLOSED`
- `XRPC_HTTP_SERVER_CLOSING`
- `XRPC_HTTP_SERVER_CREATED`
- `XRPC_HTTP_SERVER_IO`
- `XRPC_HTTP_SERVER_MEMORY`
- `XRPC_HTTP_SERVER_RESOURCE`
- `XRPC_HTTP_SERVER_RUNNING`
- `XRPC_HTTP_SERVER_STATE`

### 类型 (7)

- `xrpchttpserver`
- `xrpchttpserverconfig`
- `xrpchttpservererror`
- `xrpchttpserverinfo`
- `xrpchttpserverstate`
- `xrpcserverconfig`
- `xrpcserverinfo`

## `extlibs/xrpc/include/xrpc/id.h`

[查看带契约注释的公共头](../../include/xrpc/id.h)

### 函数 (3)

- `xrpcIdConfigInit`
- `xrpcIdKeySize`
- `xrpcIdKeyWrite`

### 常量与宏 (4)

- `XRPC_ID_ARGUMENT`
- `XRPC_ID_CAPACITY`
- `XRPC_ID_LIMIT`
- `XRPC_ID_VALUE`

### 类型 (2)

- `xrpcidconfig`
- `xrpciderror`

## `extlibs/xrpc/include/xrpc/methods.h`

[查看带契约注释的公共头](../../include/xrpc/methods.h)

### 函数 (7)

- `xrpcMethodsCall`
- `xrpcMethodsConfigInit`
- `xrpcMethodsCreate`
- `xrpcMethodsFind`
- `xrpcMethodsInfo`
- `xrpcMethodsRelease`
- `xrpcMethodsRetain`

### 常量与宏 (9)

- `XRPC_FEATURE_XRPC_METHODS`
- `XRPC_METHODS_ARGUMENT`
- `XRPC_METHODS_DUPLICATE`
- `XRPC_METHODS_ERROR`
- `XRPC_METHODS_FOUND`
- `XRPC_METHODS_LIMIT`
- `XRPC_METHODS_MEMORY`
- `XRPC_METHODS_NOT_FOUND`
- `XRPC_METHODS_VALUE`

### 类型 (6)

- `xrpcmethod`
- `xrpcmethods`
- `xrpcmethodsconfig`
- `xrpcmethodserror`
- `xrpcmethodsinfo`
- `xrpcmethodsstatus`

## `extlibs/xrpc/include/xrpc/pending.h`

[查看带契约注释的公共头](../../include/xrpc/pending.h)

### 函数 (10)

- `xrpcPendingConfigInit`
- `xrpcPendingCreate`
- `xrpcPendingDestroy`
- `xrpcPendingExpire`
- `xrpcPendingInfo`
- `xrpcPendingInsert`
- `xrpcPendingTakeBatch`
- `xrpcPendingTakeFirst`
- `xrpcPendingTakeId`
- `xrpcPendingTakeToken`

### 常量与宏 (12)

- `XRPC_FEATURE_XRPC_PENDING`
- `XRPC_PENDING_ARGUMENT`
- `XRPC_PENDING_DUPLICATE`
- `XRPC_PENDING_ERROR`
- `XRPC_PENDING_FULL`
- `XRPC_PENDING_LIMIT`
- `XRPC_PENDING_MATCHED`
- `XRPC_PENDING_MEMORY`
- `XRPC_PENDING_MISSING`
- `XRPC_PENDING_OK`
- `XRPC_PENDING_REPEATED`
- `XRPC_PENDING_UNKNOWN`

### 类型 (9)

- `xrpcpending`
- `xrpcpendingcall`
- `xrpcpendingconfig`
- `xrpcpendingerror`
- `xrpcpendinginfo`
- `xrpcpendingresolution`
- `xrpcpendingresolutionstatus`
- `xrpcpendingstatus`
- `xrpcpendingtoken`

## `extlibs/xrpc/include/xrpc/receive.h`

[查看带契约注释的公共头](../../include/xrpc/receive.h)

### 函数 (1)

- `xrpcReceive`

### 常量与宏 (3)

- `XRPC_FEATURE_XRPC_RECEIVE`
- `XRPC_RECEIVE_ARGUMENT`
- `XRPC_RECEIVE_CAPACITY`

### 类型 (3)

- `xrpcreceiveerror`
- `xrpcreceiveinfo`
- `xrpcresponsestatus`

## `extlibs/xrpc/include/xrpc/response.h`

[查看带契约注释的公共头](../../include/xrpc/response.h)

### 函数 (5)

- `xrpcResponseConfigInit`
- `xrpcResponsesInfo`
- `xrpcResponsesRead`
- `xrpcResponsesRelease`
- `xrpcResponsesRetain`

### 常量与宏 (8)

- `XRPC_RESPONSE_ARGUMENT`
- `XRPC_RESPONSE_DONE`
- `XRPC_RESPONSE_ENVELOPE`
- `XRPC_RESPONSE_INVALID`
- `XRPC_RESPONSE_LIMIT`
- `XRPC_RESPONSE_LOCAL_ERROR`
- `XRPC_RESPONSE_MEMORY`
- `XRPC_RESPONSE_PARSE_ERROR`

### 类型 (2)

- `xrpcresponseerror`
- `xrpcresponseinfo`

## `extlibs/xrpc/include/xrpc/server.h`

[查看带契约注释的公共头](../../include/xrpc/server.h)

### 函数 (8)

- `xrpcServerCancel`
- `xrpcServerClose`
- `xrpcServerConfigInit`
- `xrpcServerCreate`
- `xrpcServerDestroy`
- `xrpcServerInfo`
- `xrpcServerSubmit`
- `xrpcServerWaitFor`

### 常量与宏 (14)

- `XRPC_SERVER_ACCEPTED`
- `XRPC_SERVER_ADMISSION_ERROR`
- `XRPC_SERVER_ALLOCATION`
- `XRPC_SERVER_ARGUMENT`
- `XRPC_SERVER_CANCELLED`
- `XRPC_SERVER_CLOSED`
- `XRPC_SERVER_DONE`
- `XRPC_SERVER_LIMIT`
- `XRPC_SERVER_LOCAL_ERROR`
- `XRPC_SERVER_MEMORY`
- `XRPC_SERVER_OVERLOADED`
- `XRPC_SERVER_RESOURCE`
- `XRPC_SERVER_STATE`
- `XRPC_SERVER_TIMEOUT`

### 类型 (7)

- `xrpcserver`
- `xrpcserveradmission`
- `xrpcservercall`
- `xrpcservercontext`
- `xrpcservererror`
- `xrpcserverresult`
- `xrpcserverstatus`

## `extlibs/xrpc/include/xrpc/types.h`

[查看带契约注释的公共头](../../include/xrpc/types.h)

### 常量与宏 (22)

- `XRPC_CODEC_ARGUMENT`
- `XRPC_CODEC_CAPACITY`
- `XRPC_CODEC_LIMIT`
- `XRPC_CODEC_VALUE`
- `XRPC_ERROR_INTERNAL`
- `XRPC_ERROR_INVALID_PARAMS`
- `XRPC_ERROR_INVALID_REQUEST`
- `XRPC_ERROR_METHOD_NOT_FOUND`
- `XRPC_ERROR_PARSE`
- `XRPC_MESSAGE_CALL`
- `XRPC_MESSAGE_ERROR`
- `XRPC_MESSAGE_INVALID`
- `XRPC_MESSAGE_NOTIFICATION`
- `XRPC_MESSAGE_RESULT`
- `XRPC_METHOD_RESERVED`
- `XRPC_VALUE_ARRAY`
- `XRPC_VALUE_BOOLEAN`
- `XRPC_VALUE_MISSING`
- `XRPC_VALUE_NULL`
- `XRPC_VALUE_NUMBER`
- `XRPC_VALUE_OBJECT`
- `XRPC_VALUE_STRING`

### 类型 (2)

- `xrpccodecerror`
- `xrpcvaluekind`
