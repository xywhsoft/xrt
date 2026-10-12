# xws-group API 参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xws-group/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `38` 个函数、`18` 个常量或宏、
`9` 个公共类型。

## `extlibs/xws-group/include/xrt/websocket_group.h`

[查看带契约注释的公共头](../../include/xrt/websocket_group.h)

### 函数 (38)

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
- `xrtWsGroupOpWaitForCancel`
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
- `XWS_GROUP_ERROR_ARGUMENT`
- `XWS_GROUP_ERROR_CAPACITY`
- `XWS_GROUP_ERROR_MEMORY`
- `XWS_GROUP_ERROR_RANGE`
- `XWS_GROUP_ERROR_STATE`
- `XWS_GROUP_FEATURE_WEBSOCKET_GROUP`
- `XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE`
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

## `extlibs/xws-group/include/xws-group/export.h`

[查看带契约注释的公共头](../../include/xws-group/export.h)

### 常量与宏 (2)

- `XWS_GROUP_BUILD_SHARED`
- `XWS_GROUP_USE_SHARED`
