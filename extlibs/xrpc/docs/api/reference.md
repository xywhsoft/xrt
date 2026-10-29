# xrpc public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xrpc/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `9` 个函数、`48` 个常量或宏、
`18` 个公共类型。

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

### 函数 (4)

- `xrpcDispatch`
- `xrpcDispatchConfigInit`
- `xrpcReplyError`
- `xrpcReplyResult`

### 常量与宏 (6)

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
