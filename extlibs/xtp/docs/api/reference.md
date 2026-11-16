# xtp public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xtp/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `0` 个函数、`79` 个常量或宏、
`48` 个公共类型。

## `extlibs/xtp/include/xtp/codec.h`

[查看带契约注释的公共头](../../include/xtp/codec.h)

### 常量与宏 (2)

- `XTP_API`
- `XTP_FEATURE_XTP_CODEC`

### 类型 (8)

- `xtpcodecconfig`
- `xtpframeview`
- `xtppacket`
- `xtpparamentry`
- `xtpparamiter`
- `xtpparamresult`
- `xtpparamsview`
- `xtpresult`

## `extlibs/xtp/include/xtp/reader.h`

[查看带契约注释的公共头](../../include/xtp/reader.h)

### 常量与宏 (6)

- `XTP_FEATURE_XTP_READER`
- `XTP_FLOW_ACCEPT`
- `XTP_FLOW_PAUSE`
- `XTP_FLOW_STOP`
- `XTP_INPUT_RECORD`
- `XTP_INPUT_STREAM`

### 类型 (8)

- `xtpflow`
- `xtpinputmode`
- `xtpreader`
- `xtpreaderconfig`
- `xtpreaderevents`
- `xtpreaderinfo`
- `xtpreadprogress`
- `xtpreallocproc`

## `extlibs/xtp/include/xtp/session.h`

[查看带契约注释的公共头](../../include/xtp/session.h)

### 常量与宏 (17)

- `XTP_ACTION_ACCEPT`
- `XTP_ACTION_BUSY`
- `XTP_ACTION_PAUSE`
- `XTP_ACTION_REJECT`
- `XTP_FEATURE_XTP_SESSION`
- `XTP_FINISH_ACK`
- `XTP_FINISH_CLOSED`
- `XTP_FINISH_LOCAL_ERROR`
- `XTP_FINISH_RESPONSE`
- `XTP_FINISH_SENT`
- `XTP_FINISH_TIMEOUT`
- `XTP_KNOW_ACCEPTED`
- `XTP_KNOW_NOT_SENT`
- `XTP_KNOW_REJECTED`
- `XTP_KNOW_REPLIED`
- `XTP_KNOW_UNKNOWN`
- `XTP_STATUS_BUSY`

### 类型 (22)

- `xtpaction`
- `xtpbinding`
- `xtpcommand`
- `xtpcompletion`
- `xtpconfig`
- `xtpdecision`
- `xtpevents`
- `xtpfault`
- `xtpfinish`
- `xtpinfo`
- `xtpknowledge`
- `xtplane`
- `xtpmessage`
- `xtpmessagekind`
- `xtpmessageview`
- `xtpoperationtoken`
- `xtpoutput`
- `xtpoutputticket`
- `xtprequesttoken`
- `xtpresponse`
- `xtpsession`
- `xtpsubmitoptions`

## `extlibs/xtp/include/xtp/transport.h`

[查看带契约注释的公共头](../../include/xtp/transport.h)

### 常量与宏 (1)

- `XTP_FEATURE_XTP_TRANSPORT`

### 类型 (3)

- `xtpwritebudget`
- `xtpwriteproc`
- `xtpwriteprogress`

## `extlibs/xtp/include/xtp/types.h`

[查看带契约注释的公共头](../../include/xtp/types.h)

### 常量与宏 (51)

- `XTP_ACK`
- `XTP_AGAIN`
- `XTP_BATCH_MAX`
- `XTP_CLOSED`
- `XTP_COMMAND_ID`
- `XTP_COMMAND_NAME`
- `XTP_COMMAND_NONE`
- `XTP_DROPPED`
- `XTP_END`
- `XTP_E_ARGUMENT`
- `XTP_E_CAPACITY`
- `XTP_E_DEADLINE`
- `XTP_E_LENGTH`
- `XTP_E_LIMIT`
- `XTP_E_MAGIC`
- `XTP_E_MEMORY`
- `XTP_E_STATE`
- `XTP_E_TRUNCATED`
- `XTP_E_TYPE`
- `XTP_E_VALUE`
- `XTP_E_VERSION`
- `XTP_FLAG_PARAM_ARRAY`
- `XTP_FLAG_REPLAYABLE`
- `XTP_FLAG_RESULT_ACK`
- `XTP_FLAG_ROUTE_ID`
- `XTP_HEADER_SIZE`
- `XTP_MESSAGE`
- `XTP_MORE`
- `XTP_NOTIFY`
- `XTP_OK`
- `XTP_PARAMS_ARRAY`
- `XTP_PARAMS_MAX`
- `XTP_PARAMS_TABLE`
- `XTP_PARAM_ARGUMENT`
- `XTP_PARAM_FOUND`
- `XTP_PARAM_MISSING`
- `XTP_PARAM_WRONG_MODE`
- `XTP_PAUSED`
- `XTP_PUSH`
- `XTP_REQUEST`
- `XTP_RESPONSE`
- `XTP_STATUS_BAD_ARGUMENT`
- `XTP_STATUS_DEADLINE_EXCEEDED`
- `XTP_STATUS_INTERNAL`
- `XTP_STATUS_NOT_FOUND`
- `XTP_STATUS_OK`
- `XTP_STATUS_REJECTED`
- `XTP_STATUS_RESULT_TOO_LARGE`
- `XTP_STATUS_STATE_LOST`
- `XTP_STATUS_UNSUPPORTED`
- `XTP_WIRE_VERSION`

### 类型 (5)

- `xtpcommandkind`
- `xtpfield`
- `xtpparamkind`
- `xtpparams`
- `xtpstatus`

## `extlibs/xtp/include/xtp/ws.h`

[查看带契约注释的公共头](../../include/xtp/ws.h)

### 常量与宏 (1)

- `XTP_FEATURE_XTP_WS`

### 类型 (1)

- `xtpwsinput`

## `extlibs/xtp/include/xtp/xrudp.h`

[查看带契约注释的公共头](../../include/xtp/xrudp.h)

### 常量与宏 (1)

- `XTP_FEATURE_XTP_XRUDP`

### 类型 (1)

- `xtpxrudpwriter`
