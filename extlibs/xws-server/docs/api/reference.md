# xws-server API 参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xws-server/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `14` 个函数、`24` 个常量或宏、
`17` 个公共类型。

## `extlibs/xws-server/include/xrt/websocket_server.h`

[查看带契约注释的公共头](../../include/xrt/websocket_server.h)

### 函数 (7)

- `xrtWsServerCheck`
- `xrtWsServerConfigInit`
- `xrtWsServerConfigValid`
- `xrtWsServerReject`
- `xrtWsServerReply`
- `xrtWsUpgrade`
- `xrtWsUpgradeAccept`

### 常量与宏 (8)

- `XWS_ACCEPT_CAPACITY`
- `XWS_DEFLATE_MAX_SIZE`
- `XWS_FEATURE_WEBSOCKET_CONNECTION`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_TLS`
- `XWS_SERVER_FEATURE_WEBSOCKET_SERVER`
- `XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE`
- `XWS_SERVER_FEATURE_WEBSOCKET_SERVER_TLS`

### 类型 (8)

- `xwsconn`
- `xwsconnconfig`
- `xwsconnevents`
- `xwsdeflate`
- `xwsdeflateacceptproc`
- `xwsserverconfig`
- `xwsserverhandshake`
- `xwsupgradeproc`

## `extlibs/xws-server/include/xrt/websocket_server_future.h`

[查看带契约注释的公共头](../../include/xrt/websocket_server_future.h)

### 函数 (5)

- `xrtWsServerResultConnection`
- `xrtWsServerResultDestroy`
- `xrtWsServerResultRef`
- `xrtWsServerResultTakeConnection`
- `xrtWsUpgradeAsync`

### 常量与宏 (3)

- `XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE`
- `XWS_SERVER_RESULT_ERROR_ARGUMENT`
- `XWS_SERVER_RESULT_ERROR_STATE`

### 类型 (2)

- `xwsserverresult`
- `xwsserverresulterror`

## `extlibs/xws-server/include/xrt/websocket_server_router.h`

[查看带契约注释的公共头](../../include/xrt/websocket_server_router.h)

### 函数 (2)

- `xrtWsServerRoute`
- `xrtWsServerRouteConfigInit`

### 常量与宏 (11)

- `XWS_SERVER_FEATURE_WEBSOCKET_SERVER_ROUTER`
- `XWS_SERVER_ORIGIN_ANY`
- `XWS_SERVER_ORIGIN_SAME_HOST`
- `XWS_SERVER_ORIGIN_SAME_HOST_OR_ABSENT`
- `XWS_SERVER_ROUTER_ERROR_ARGUMENT`
- `XWS_SERVER_ROUTER_ERROR_AUTHORIZATION`
- `XWS_SERVER_ROUTER_ERROR_CONFIG`
- `XWS_SERVER_ROUTER_ERROR_MEMORY`
- `XWS_SERVER_ROUTER_ERROR_RESPONSE`
- `XWS_SERVER_ROUTER_ERROR_ROUTE`
- `XWS_SERVER_ROUTER_ERROR_STATE`

### 类型 (7)

- `xwsserveroriginpolicy`
- `xwsserverrouteauthorizeproc`
- `xwsserverrouteconfig`
- `xwsserverrouteerrorproc`
- `xwsserverrouteopenproc`
- `xwsserverroutererror`
- `xwsserverrouterreleaseproc`

## `extlibs/xws-server/include/xws-server/export.h`

[查看带契约注释的公共头](../../include/xws-server/export.h)

### 常量与宏 (2)

- `XWS_SERVER_BUILD_SHARED`
- `XWS_SERVER_USE_SHARED`
