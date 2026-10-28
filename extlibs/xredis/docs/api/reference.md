# xredis public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xredis/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `21` 个函数、`60` 个常量或宏、
`20` 个公共类型。

## `extlibs/xredis/include/xredis/command.h`

[查看带契约注释的公共头](../../include/xredis/command.h)

### 函数 (2)

- `xredisCommandSize`
- `xredisCommandWrite`

### 常量与宏 (5)

- `XREDIS_COMMAND_ARGUMENT`
- `XREDIS_COMMAND_CAPACITY`
- `XREDIS_COMMAND_OVERFLOW`
- `XREDIS_COMMAND_OVERLAP`
- `XREDIS_FEATURE_XREDIS_COMMAND`

### 类型 (1)

- `xrediscommanderror`

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

### 类型 (11)

- `xredisprotocol`
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

### 常量与宏 (13)

- `XREDIS_FEATURE_XREDIS_REPLY`
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

### 类型 (8)

- `xredisreply`
- `xredisreplyerror`
- `xredisreplyinfo`
- `xredisreplyreader`
- `xredisreplyreaderconfig`
- `xredisreplyreaderinfo`
- `xredisreplystatus`
- `xredisreplyvalue`
