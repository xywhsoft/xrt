# xws-client API 参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xws-client/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `17` 个函数、`12` 个常量或宏、
`9` 个公共类型。

## `extlibs/xws-client/include/xrt/websocket_client.h`

[查看带契约注释的公共头](../../include/xrt/websocket_client.h)

### 函数 (7)

- `xrtWsClientCheck`
- `xrtWsClientConfigInit`
- `xrtWsClientRequestClone`
- `xrtWsClientRequestCreate`
- `xrtWsConnect`
- `xrtWsConnectRequest`
- `xrtWsRequestCreate`

### 常量与宏 (7)

- `XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT`
- `XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE`
- `XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_HTTPS`
- `XWS_FEATURE_WEBSOCKET_CONNECTION`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE`
- `XWS_FEATURE_WEBSOCKET_CONNECTION_TLS`
- `XWS_KEY_CAPACITY`

### 类型 (7)

- `xwsclientconfig`
- `xwsclienthandshake`
- `xwsconn`
- `xwsconnconfig`
- `xwsconnectproc`
- `xwsconnevents`
- `xwsdeflate`

## `extlibs/xws-client/include/xrt/websocket_client_future.h`

[查看带契约注释的公共头](../../include/xrt/websocket_client_future.h)

### 函数 (10)

- `xrtWsClientResultConnection`
- `xrtWsClientResultDestroy`
- `xrtWsClientResultRef`
- `xrtWsClientResultResponse`
- `xrtWsClientResultTakeConnection`
- `xrtWsClientResultTakeResponse`
- `xrtWsConnectAsync`
- `xrtWsConnectRequestAsync`
- `xrtWsConnectRequestSync`
- `xrtWsConnectSync`

### 常量与宏 (3)

- `XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE`
- `XWS_CLIENT_RESULT_ERROR_ARGUMENT`
- `XWS_CLIENT_RESULT_ERROR_STATE`

### 类型 (2)

- `xwsclientresult`
- `xwsclientresulterror`

## `extlibs/xws-client/include/xws-client/export.h`

[查看带契约注释的公共头](../../include/xws-client/export.h)

### 常量与宏 (2)

- `XWS_CLIENT_BUILD_SHARED`
- `XWS_CLIENT_USE_SHARED`
