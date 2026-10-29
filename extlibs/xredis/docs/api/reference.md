# xredis public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xredis/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `58` 个函数、`123` 个常量或宏、
`44` 个公共类型。

## `extlibs/xredis/include/xredis/client.h`

[查看带契约注释的公共头](../../include/xredis/client.h)

### 函数 (13)

- `xredisClientCall`
- `xredisClientCallAsync`
- `xredisClientClose`
- `xredisClientClosed`
- `xredisClientConfigInit`
- `xredisClientConnect`
- `xredisClientConnectInit`
- `xredisClientInfo`
- `xredisClientInitConfigInit`
- `xredisClientReady`
- `xredisClientRelease`
- `xredisClientRetain`
- `xredisClientTakePush`

### 常量与宏 (52)

- `XREDIS_ADMISSION_ACCEPTED`
- `XREDIS_ADMISSION_CLOSED`
- `XREDIS_ADMISSION_ERROR`
- `XREDIS_ADMISSION_OVERLOADED`
- `XREDIS_CLIENT_ARGUMENT`
- `XREDIS_CLIENT_CLOSED`
- `XREDIS_CLIENT_CLOSING`
- `XREDIS_CLIENT_COMMAND_MODE`
- `XREDIS_CLIENT_CONNECTING`
- `XREDIS_CLIENT_INITIALIZATION_FAILED`
- `XREDIS_CLIENT_OPEN`
- `XREDIS_CLIENT_STATE`
- `XREDIS_FEATURE_XREDIS_CLIENT`
- `XREDIS_FEATURE_XREDIS_COMMAND`
- `XREDIS_FEATURE_XREDIS_REPLY`
- `XREDIS_INIT_AUTH`
- `XREDIS_INIT_AUTHENTICATE`
- `XREDIS_INIT_AUTO`
- `XREDIS_INIT_DATABASE`
- `XREDIS_INIT_DONE`
- `XREDIS_INIT_HELLO`
- `XREDIS_INIT_NAME`
- `XREDIS_INIT_NONE`
- `XREDIS_INIT_RESP2`
- `XREDIS_INIT_RESP3`
- `XREDIS_INIT_SELECT`
- `XREDIS_INIT_SETNAME`
- `XREDIS_INIT_USERNAME`
- `XREDIS_MAY_HAVE_EXECUTED`
- `XREDIS_NOT_SENT`
- `XREDIS_PROTOCOL_UNKNOWN`
- `XREDIS_READY_AUTH_FAILED`
- `XREDIS_READY_CLOSED`
- `XREDIS_READY_DISCONNECTED`
- `XREDIS_READY_LIMIT`
- `XREDIS_READY_MEMORY`
- `XREDIS_READY_OK`
- `XREDIS_READY_PROTOCOL`
- `XREDIS_READY_SERVER_ERROR`
- `XREDIS_READY_TIMEOUT`
- `XREDIS_READY_UNSUPPORTED`
- `XREDIS_REPLIED`
- `XREDIS_REQUEST_CANCELLED`
- `XREDIS_REQUEST_CLOSED`
- `XREDIS_REQUEST_DISCONNECTED`
- `XREDIS_REQUEST_INITIALIZATION`
- `XREDIS_REQUEST_LIMIT`
- `XREDIS_REQUEST_MEMORY`
- `XREDIS_REQUEST_OK`
- `XREDIS_REQUEST_PROTOCOL`
- `XREDIS_REQUEST_SERVER_ERROR`
- `XREDIS_REQUEST_TIMEOUT`

### 类型 (17)

- `xredisadmission`
- `xredisclient`
- `xredisclientconfig`
- `xredisclienterror`
- `xredisclientinfo`
- `xredisclientinitconfig`
- `xredisclientstate`
- `xredisdelivery`
- `xredisinitmode`
- `xredisinitstep`
- `xredisprotocol`
- `xredisreadyresult`
- `xredisreadystatus`
- `xredisreply`
- `xredisreplyreaderconfig`
- `xredisrequestresult`
- `xredisrequeststatus`

## `extlibs/xredis/include/xredis/command.h`

[查看带契约注释的公共头](../../include/xredis/command.h)

### 函数 (2)

- `xredisCommandSize`
- `xredisCommandWrite`

### 常量与宏 (4)

- `XREDIS_COMMAND_ARGUMENT`
- `XREDIS_COMMAND_CAPACITY`
- `XREDIS_COMMAND_OVERFLOW`
- `XREDIS_COMMAND_OVERLAP`

### 类型 (1)

- `xrediscommanderror`

## `extlibs/xredis/include/xredis/dial.h`

[查看带契约注释的公共头](../../include/xredis/dial.h)

### 函数 (3)

- `xredisClientDial`
- `xredisClientDialAddresses`
- `xredisClientDialConfigInit`

### 常量与宏 (3)

- `XREDIS_DIAL_ADDRESSES_MAX`
- `XREDIS_DIAL_HOST_MAX`
- `XREDIS_FEATURE_XREDIS_DIAL`

### 类型 (1)

- `xredisclientdialconfig`

## `extlibs/xredis/include/xredis/export.h`

[查看带契约注释的公共头](../../include/xredis/export.h)

### 常量与宏 (2)

- `XREDIS_BUILD_SHARED`
- `XREDIS_USE_SHARED`

## `extlibs/xredis/include/xredis/reader.h`

[查看带契约注释的公共头](../../include/xredis/reader.h)

### 函数 (7)

- `xredisReaderConfigInit`
- `xredisReaderCreate`
- `xredisReaderDestroy`
- `xredisReaderFeed`
- `xredisReaderFinish`
- `xredisReaderInfo`
- `xredisReaderReset`

### 常量与宏 (40)

- `XREDIS_FEATURE_XREDIS_READER`
- `XREDIS_LENGTH_UNKNOWN`
- `XREDIS_READER_ARGUMENT`
- `XREDIS_READER_CALLBACK`
- `XREDIS_READER_ERROR`
- `XREDIS_READER_LEGACY_NAN`
- `XREDIS_READER_LIMIT`
- `XREDIS_READER_MORE`
- `XREDIS_READER_OK`
- `XREDIS_READER_PAUSED`
- `XREDIS_READER_PROTOCOL`
- `XREDIS_READER_READY`
- `XREDIS_READER_STATE`
- `XREDIS_READER_TRUNCATED`
- `XREDIS_READER_YIELD`
- `XREDIS_READ_BEGIN`
- `XREDIS_READ_DATA`
- `XREDIS_READ_END`
- `XREDIS_READ_FAIL`
- `XREDIS_READ_NEXT`
- `XREDIS_READ_PAUSE`
- `XREDIS_REPLY_ARRAY`
- `XREDIS_REPLY_ATTRIBUTE`
- `XREDIS_REPLY_BIG_NUMBER`
- `XREDIS_REPLY_BOOLEAN`
- `XREDIS_REPLY_BULK_ERROR`
- `XREDIS_REPLY_BULK_STRING`
- `XREDIS_REPLY_DOUBLE`
- `XREDIS_REPLY_INTEGER`
- `XREDIS_REPLY_MAP`
- `XREDIS_REPLY_NULL`
- `XREDIS_REPLY_NULL_VALUE`
- `XREDIS_REPLY_PUSH`
- `XREDIS_REPLY_SET`
- `XREDIS_REPLY_SIMPLE_ERROR`
- `XREDIS_REPLY_SIMPLE_STRING`
- `XREDIS_REPLY_STREAMED`
- `XREDIS_REPLY_VERBATIM`
- `XREDIS_RESP2`
- `XREDIS_RESP3`

### 类型 (10)

- `xredisreadaction`
- `xredisreader`
- `xredisreaderconfig`
- `xredisreadererror`
- `xredisreaderinfo`
- `xredisreadevent`
- `xredisreadkind`
- `xredisreadproc`
- `xredisreadstatus`
- `xredisreplytype`

## `extlibs/xredis/include/xredis/reply.h`

[查看带契约注释的公共头](../../include/xredis/reply.h)

### 函数 (12)

- `xredisReplyDouble`
- `xredisReplyGet`
- `xredisReplyInfo`
- `xredisReplyReaderConfigInit`
- `xredisReplyReaderCreate`
- `xredisReplyReaderDestroy`
- `xredisReplyReaderFeed`
- `xredisReplyReaderFinish`
- `xredisReplyReaderInfo`
- `xredisReplyReaderReset`
- `xredisReplyRelease`
- `xredisReplyRetain`

### 常量与宏 (12)

- `XREDIS_REPLY_ARGUMENT`
- `XREDIS_REPLY_ERROR`
- `XREDIS_REPLY_LIMIT`
- `XREDIS_REPLY_MEMORY`
- `XREDIS_REPLY_MORE`
- `XREDIS_REPLY_NONE`
- `XREDIS_REPLY_OK`
- `XREDIS_REPLY_PROTOCOL`
- `XREDIS_REPLY_READY`
- `XREDIS_REPLY_STATE`
- `XREDIS_REPLY_TRUNCATED`
- `XREDIS_REPLY_YIELD`

### 类型 (6)

- `xredisreplyerror`
- `xredisreplyinfo`
- `xredisreplyreader`
- `xredisreplyreaderinfo`
- `xredisreplystatus`
- `xredisreplyvalue`

## `extlibs/xredis/include/xredis/tls.h`

[查看带契约注释的公共头](../../include/xredis/tls.h)

### 函数 (4)

- `xredisClientTlsConfigInit`
- `xredisClientTlsConnect`
- `xredisClientTlsDial`
- `xredisClientTlsDialAddresses`

### 常量与宏 (1)

- `XREDIS_FEATURE_XREDIS_TLS`

### 类型 (1)

- `xredisclienttlsconfig`

## `extlibs/xredis/include/xredis/tls_auth.h`

[查看带契约注释的公共头](../../include/xredis/tls_auth.h)

### 函数 (3)

- `xredisClientTlsAuthConnectV1`
- `xredisClientTlsAuthDialAddressesV1`
- `xredisClientTlsAuthDialV1`

### 常量与宏 (1)

- `XREDIS_FEATURE_XREDIS_TLS_AUTH`

## `extlibs/xredis/include/xredis/transaction.h`

[查看带契约注释的公共头](../../include/xredis/transaction.h)

### 函数 (3)

- `xredisClientTransaction`
- `xredisClientTransactionAsync`
- `xredisClientTransactionConfigInit`

### 常量与宏 (7)

- `XREDIS_FEATURE_XREDIS_TRANSACTION`
- `XREDIS_TRANSACTION_ABORTED`
- `XREDIS_TRANSACTION_COMMANDS_DEFAULT`
- `XREDIS_TRANSACTION_COMMANDS_MAX`
- `XREDIS_TRANSACTION_EXECUTED`
- `XREDIS_TRANSACTION_MAY_HAVE_EXECUTED`
- `XREDIS_TRANSACTION_NOT_EXECUTED`

### 类型 (4)

- `xredistransactioncommand`
- `xredistransactionconfig`
- `xredistransactionexecution`
- `xredistransactionresult`

## `extlibs/xredis/include/xredis/watch.h`

[查看带契约注释的公共头](../../include/xredis/watch.h)

### 函数 (11)

- `xredisClientWatch`
- `xredisClientWatchAsync`
- `xredisWatchAbort`
- `xredisWatchCall`
- `xredisWatchCallAsync`
- `xredisWatchClosed`
- `xredisWatchCommit`
- `xredisWatchCommitAsync`
- `xredisWatchConfigInit`
- `xredisWatchRelease`
- `xredisWatchRetain`

### 常量与宏 (1)

- `XREDIS_FEATURE_XREDIS_WATCH`

### 类型 (4)

- `xrediswatch`
- `xrediswatchclosedresult`
- `xrediswatchconfig`
- `xrediswatchresult`
