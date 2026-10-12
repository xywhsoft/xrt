# xhttp-server 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xhttp-server/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `141` 个函数、`161` 个常量或宏、
`89` 个公共类型。

## `extlibs/xhttp-server/include/xhttp-server/export.h`

[查看带契约注释的公共头](../../include/xhttp-server/export.h)

### 常量与宏 (2)

- `XHTTP_SERVER_BUILD_SHARED`
- `XHTTP_SERVER_USE_SHARED`

## `extlibs/xhttp-server/include/xrt/http_body_file.h`

[查看带契约注释的公共头](../../include/xrt/http_body_file.h)

### 函数 (4)

- `xrtHttpBodyFileAdopt`
- `xrtHttpBodyFileConfigInit`
- `xrtHttpBodyFileFuture`
- `xrtHttpBodyFileRangeFuture`

### 常量与宏 (10)

- `XHTTP_BODY_FILE_ERROR_ADOPT`
- `XHTTP_BODY_FILE_ERROR_CREATE`
- `XHTTP_BODY_FILE_ERROR_OPEN`
- `XHTTP_BODY_FILE_ERROR_RANGE`
- `XHTTP_BODY_FILE_ERROR_READ`
- `XHTTP_BODY_FILE_ERROR_SIZE`
- `XHTTP_BODY_FILE_ERROR_SUBMIT`
- `XHTTP_BODY_FILE_READ_DEFAULT`
- `XHTTP_FEATURE_HTTP_BODY_ASYNC`
- `XHTTP_FEATURE_HTTP_BODY_FILE`

### 类型 (6)

- `xasyncfile`
- `xfuture`
- `xhttpbody`
- `xhttpbodyfileconfig`
- `xhttpbodyfileerror`
- `xtaskpool`

## `extlibs/xhttp-server/include/xrt/http_compress.h`

[查看带契约注释的公共头](../../include/xrt/http_compress.h)

### 函数 (3)

- `xrtHttpReplyCompress`
- `xrtHttpReplyCompressConfigInit`
- `xrtHttpServerReplyCompress`

### 常量与宏 (26)

- `XHTTP_FEATURE_HTTP_BODY_DEFLATE`
- `XHTTP_FEATURE_HTTP_CACHE`
- `XHTTP_FEATURE_HTTP_ETAG`
- `XHTTP_FEATURE_HTTP_REPLY_COMPRESS`
- `XHTTP_FEATURE_HTTP_SERVER_COMPRESS`
- `XHTTP_FEATURE_HTTP_SERVER_REPLY`
- `XHTTP_FEATURE_HTTP_SERVER_REQUEST`
- `XHTTP_FEATURE_HTTP_VARY`
- `XHTTP_FEATURE_MIME`
- `XHTTP_REPLY_COMPRESS_ALLOW_ABSENT`
- `XHTTP_REPLY_COMPRESS_ALLOW_ANY_TYPE`
- `XHTTP_REPLY_COMPRESS_ALLOW_UNKNOWN_LENGTH`
- `XHTTP_REPLY_COMPRESS_APPLIED`
- `XHTTP_REPLY_COMPRESS_EAGER_DEFAULT`
- `XHTTP_REPLY_COMPRESS_ERROR`
- `XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT`
- `XHTTP_REPLY_COMPRESS_ERROR_CONFIG`
- `XHTTP_REPLY_COMPRESS_ERROR_HEADER`
- `XHTTP_REPLY_COMPRESS_ERROR_RESPONSE`
- `XHTTP_REPLY_COMPRESS_IDENTITY`
- `XHTTP_REPLY_COMPRESS_IGNORE_NO_TRANSFORM`
- `XHTTP_REPLY_COMPRESS_KEEP_LARGER`
- `XHTTP_REPLY_COMPRESS_MIN_DEFAULT`
- `XHTTP_REPLY_COMPRESS_NONE`
- `XHTTP_REPLY_COMPRESS_NOT_ACCEPTABLE`
- `XHTTP_REPLY_COMPRESS_SKIP`

### 类型 (10)

- `xdeflatestrategy`
- `xhttpacceptencoding`
- `xhttpcoding`
- `xhttpreply`
- `xhttpreplycompressconfig`
- `xhttpreplycompresserror`
- `xhttpreplycompressflag`
- `xhttpreplycompressstatus`
- `xhttpserverrequest`
- `xstrview`

## `extlibs/xhttp-server/include/xrt/http_route.h`

[查看带契约注释的公共头](../../include/xrt/http_route.h)

### 函数 (3)

- `xrtHttpRouteMatch`
- `xrtHttpRouteParam`
- `xrtHttpRouteValidate`

### 常量与宏 (5)

- `XHTTP_FEATURE_HTTP_ROUTE`
- `XHTTP_ROUTE_ERROR`
- `XHTTP_ROUTE_MATCH`
- `XHTTP_ROUTE_MISS`
- `XHTTP_ROUTE_MORE`

### 类型 (2)

- `xhttprouteparam`
- `xhttproutestatus`

## `extlibs/xhttp-server/include/xrt/http_router.h`

[查看带契约注释的公共头](../../include/xrt/http_router.h)

### 函数 (11)

- `xrtHttpRouterAdd`
- `xrtHttpRouterBytes`
- `xrtHttpRouterConfigInit`
- `xrtHttpRouterCount`
- `xrtHttpRouterCreate`
- `xrtHttpRouterDestroy`
- `xrtHttpRouterFreeze`
- `xrtHttpRouterFrozen`
- `xrtHttpRouterMatch`
- `xrtHttpRouterMethods`
- `xrtHttpRouterNodes`

### 常量与宏 (8)

- `XHTTP_FEATURE_HTTP_ROUTER`
- `XHTTP_ROUTER_ANY_METHOD`
- `XHTTP_ROUTER_ERROR`
- `XHTTP_ROUTER_HEAD_FALLBACK`
- `XHTTP_ROUTER_MATCH`
- `XHTTP_ROUTER_METHOD_NOT_ALLOWED`
- `XHTTP_ROUTER_MORE`
- `XHTTP_ROUTER_NOT_FOUND`

### 类型 (4)

- `xhttprouter`
- `xhttprouterconfig`
- `xhttproutermatch`
- `xhttprouterstatus`

## `extlibs/xhttp-server/include/xrt/http_server_file.h`

[查看带契约注释的公共头](../../include/xrt/http_server_file.h)

### 函数 (4)

- `xrtHttpConnFile`
- `xrtHttpConnFileRange`
- `xrtHttpReplyFileFuture`
- `xrtHttpReplyFileRangeFuture`

### 常量与宏 (3)

- `XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC`
- `XHTTP_FEATURE_HTTP_SERVER_FILE`
- `XHTTP_FEATURE_HTTP_SERVER_FUTURE`

### 类型 (1)

- `xhttpconn`

## `extlibs/xhttp-server/include/xrt/http_server_future.h`

[查看带契约注释的公共头](../../include/xrt/http_server_future.h)

### 函数 (2)

- `xrtHttpConnRespondFuture`
- `xrtHttpServerWaitAsync`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_SERVER`

### 类型 (1)

- `xhttpserver`

## `extlibs/xhttp-server/include/xrt/http_server_middleware.h`

[查看带契约注释的公共头](../../include/xrt/http_server_middleware.h)

### 函数 (4)

- `xrtHttpServerMiddlewareCount`
- `xrtHttpServerNext`
- `xrtHttpServerUse`
- `xrtHttpServerUseOwned`

### 常量与宏 (8)

- `XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE`
- `XHTTP_FEATURE_HTTP_SERVER_ROUTER`
- `XHTTP_SERVER_MIDDLEWARE_ERROR_ARGUMENT`
- `XHTTP_SERVER_MIDDLEWARE_ERROR_CALLBACK`
- `XHTTP_SERVER_MIDDLEWARE_ERROR_LIMIT`
- `XHTTP_SERVER_MIDDLEWARE_ERROR_MEMORY`
- `XHTTP_SERVER_MIDDLEWARE_ERROR_NEXT`
- `XHTTP_SERVER_MIDDLEWARE_ERROR_STATE`

### 类型 (5)

- `xhttpservermiddlewareerror`
- `xhttpservermiddlewareproc`
- `xhttpservernext`
- `xhttpserverrouter`
- `xhttpserverrouterreleaseproc`

## `extlibs/xhttp-server/include/xrt/http_server_mux.h`

[查看带契约注释的公共头](../../include/xrt/http_server_mux.h)

### 函数 (10)

- `xrtHttpServerMuxConfigInit`
- `xrtHttpServerMuxCreate`
- `xrtHttpServerMuxDefault`
- `xrtHttpServerMuxDestroy`
- `xrtHttpServerMuxHost`
- `xrtHttpServerMuxMatch`
- `xrtHttpServerMuxRef`
- `xrtHttpServerMuxRemove`
- `xrtHttpServerMuxStart`
- `xrtHttpServerMuxStats`

### 常量与宏 (14)

- `XHTTP_FEATURE_HTTP_SERVER_MUX`
- `XHTTP_SERVER_MUX_DEFAULT`
- `XHTTP_SERVER_MUX_ERROR`
- `XHTTP_SERVER_MUX_ERROR_ARGUMENT`
- `XHTTP_SERVER_MUX_ERROR_CONTEXT`
- `XHTTP_SERVER_MUX_ERROR_HOST`
- `XHTTP_SERVER_MUX_ERROR_INTERNAL`
- `XHTTP_SERVER_MUX_ERROR_LIMIT`
- `XHTTP_SERVER_MUX_ERROR_LOCK`
- `XHTTP_SERVER_MUX_ERROR_MEMORY`
- `XHTTP_SERVER_MUX_ERROR_START`
- `XHTTP_SERVER_MUX_ERROR_STATE`
- `XHTTP_SERVER_MUX_HOST`
- `XHTTP_SERVER_MUX_NOT_FOUND`

### 类型 (8)

- `xhttpserverconfig`
- `xhttpserverevents`
- `xhttpservermux`
- `xhttpservermuxconfig`
- `xhttpservermuxerror`
- `xhttpservermuxstats`
- `xhttpservermuxstatus`
- `xnetengine`

## `extlibs/xhttp-server/include/xrt/http_server_mux_tls.h`

[查看带契约注释的公共头](../../include/xrt/http_server_mux_tls.h)

### 函数 (1)

- `xrtHttpServerMuxStartTls`

### 常量与宏 (2)

- `XHTTP_FEATURE_HTTP_SERVER_MUX_TLS`
- `XHTTP_FEATURE_HTTP_SERVER_TLS`

### 类型 (1)

- `xhttpservertlsconfig`

## `extlibs/xhttp-server/include/xrt/http_server_raw.h`

[查看带契约注释的公共头](../../include/xrt/http_server_raw.h)

### 函数 (5)

- `xrtHttpConnRespondRaw`
- `xrtHttpConnRespondRawBody`
- `xrtHttpConnRespondRawRef`
- `xrtHttpConnRespondRawRefs`
- `xrtHttpConnRespondRawTake`

### 常量与宏 (3)

- `XHTTP_FEATURE_HTTP_SERVER_RAW`
- `XHTTP_SERVER_RAW_KEEP_ALIVE`
- `XHTTP_SERVER_RAW_NONE`

### 类型 (4)

- `xbytesview`
- `xhttpserverrawflag`
- `xnetref`
- `xnetresult`

## `extlibs/xhttp-server/include/xrt/http_server_router.h`

[查看带契约注释的公共头](../../include/xrt/http_server_router.h)

### 函数 (17)

- `xrtHttpServerAny`
- `xrtHttpServerDelete`
- `xrtHttpServerGet`
- `xrtHttpServerPatch`
- `xrtHttpServerPost`
- `xrtHttpServerPut`
- `xrtHttpServerRoute`
- `xrtHttpServerRouteEvents`
- `xrtHttpServerRouteEventsInit`
- `xrtHttpServerRouterCount`
- `xrtHttpServerRouterCreate`
- `xrtHttpServerRouterDestroy`
- `xrtHttpServerRouterDispatch`
- `xrtHttpServerRouterFreeze`
- `xrtHttpServerRouterFrozen`
- `xrtHttpServerRouterRef`
- `xrtHttpServerRouterStart`

### 常量与宏 (8)

- `XHTTP_SERVER_ROUTER_ERROR_ARGUMENT`
- `XHTTP_SERVER_ROUTER_ERROR_INTERNAL`
- `XHTTP_SERVER_ROUTER_ERROR_LIMIT`
- `XHTTP_SERVER_ROUTER_ERROR_MEMORY`
- `XHTTP_SERVER_ROUTER_ERROR_RESPONSE`
- `XHTTP_SERVER_ROUTER_ERROR_START`
- `XHTTP_SERVER_ROUTER_ERROR_STATE`
- `XHTTP_SERVER_ROUTER_ERROR_TARGET`

### 类型 (6)

- `xhttpserverbodypolicy`
- `xhttpserverroutebodyproc`
- `xhttpserverrouteevents`
- `xhttpserverrouteheadersproc`
- `xhttpserverrouteproc`
- `xhttpserverroutererror`

## `extlibs/xhttp-server/include/xrt/http_server_router_tls.h`

[查看带契约注释的公共头](../../include/xrt/http_server_router_tls.h)

### 函数 (1)

- `xrtHttpServerRouterStartTls`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_SERVER_ROUTER_TLS`

## `extlibs/xhttp-server/include/xrt/http_server_runtime.h`

[查看带契约注释的公共头](../../include/xrt/http_server_runtime.h)

### 函数 (38)

- `xrtHttpConnAbort`
- `xrtHttpConnClose`
- `xrtHttpConnDestroy`
- `xrtHttpConnEndpoint`
- `xrtHttpConnError`
- `xrtHttpConnInform`
- `xrtHttpConnLocal`
- `xrtHttpConnPauseRequestBody`
- `xrtHttpConnRef`
- `xrtHttpConnRemote`
- `xrtHttpConnReply`
- `xrtHttpConnReplyBody`
- `xrtHttpConnRequest`
- `xrtHttpConnRequestBodyPaused`
- `xrtHttpConnRespond`
- `xrtHttpConnResumeRequestBody`
- `xrtHttpConnSecure`
- `xrtHttpConnServer`
- `xrtHttpConnSetRequestBodyLimit`
- `xrtHttpConnState`
- `xrtHttpConnStats`
- `xrtHttpConnTcp`
- `xrtHttpConnWorker`
- `xrtHttpServerAbort`
- `xrtHttpServerConfigInit`
- `xrtHttpServerDestroy`
- `xrtHttpServerDrain`
- `xrtHttpServerEndpointCount`
- `xrtHttpServerError`
- `xrtHttpServerEventsInit`
- `xrtHttpServerListenerCount`
- `xrtHttpServerLocal`
- `xrtHttpServerNetwork`
- `xrtHttpServerRef`
- `xrtHttpServerSecure`
- `xrtHttpServerStart`
- `xrtHttpServerState`
- `xrtHttpServerStats`

### 常量与宏 (31)

- `XHTTP_CONN_BODY`
- `XHTTP_CONN_CLOSED`
- `XHTTP_CONN_CLOSING`
- `XHTTP_CONN_INFORMATION`
- `XHTTP_CONN_REQUEST`
- `XHTTP_CONN_RESPONSE`
- `XHTTP_CONN_UPGRADED`
- `XHTTP_CONN_WAITING`
- `XHTTP_FEATURE_HTTP_SERVER_EXCHANGE`
- `XHTTP_FEATURE_HTTP_SERVER_RESPONSE`
- `XHTTP_FEATURE_HTTP_SERVER_RESPONSE_ASYNC`
- `XHTTP_SERVER_ABORTING`
- `XHTTP_SERVER_CLOSED`
- `XHTTP_SERVER_DRAINING`
- `XHTTP_SERVER_ERROR_ARGUMENT`
- `XHTTP_SERVER_ERROR_CALLBACK`
- `XHTTP_SERVER_ERROR_CONFIG`
- `XHTTP_SERVER_ERROR_CONNECTION`
- `XHTTP_SERVER_ERROR_INTERNAL`
- `XHTTP_SERVER_ERROR_LISTEN`
- `XHTTP_SERVER_ERROR_PROTOCOL`
- `XHTTP_SERVER_ERROR_RESPONSE`
- `XHTTP_SERVER_ERROR_STATE`
- `XHTTP_SERVER_ERROR_TIMEOUT_BODY`
- `XHTTP_SERVER_ERROR_TIMEOUT_HEADER`
- `XHTTP_SERVER_ERROR_TIMEOUT_IDLE`
- `XHTTP_SERVER_ERROR_TIMEOUT_REQUEST`
- `XHTTP_SERVER_ERROR_TIMEOUT_WRITE`
- `XHTTP_SERVER_ERROR_TLS`
- `XHTTP_SERVER_ERROR_UPGRADE`
- `XHTTP_SERVER_RUNNING`

### 类型 (12)

- `xerror`
- `xhttp1serverconfig`
- `xhttpconnstate`
- `xhttpconnstats`
- `xhttpservererror`
- `xhttpserverstate`
- `xhttpserverstats`
- `xnetaddr`
- `xnetserver`
- `xnetserverconfig`
- `xnetstream`
- `xnetworker`

## `extlibs/xhttp-server/include/xrt/http_server_static.h`

[查看带契约注释的公共头](../../include/xrt/http_server_static.h)

### 函数 (6)

- `xrtHttpConnStatic`
- `xrtHttpReplyFromStatic`
- `xrtHttpReplyStatic`
- `xrtHttpReplyStaticFuture`
- `xrtHttpStaticReplyConfigInit`
- `xrtHttpStaticServeConfigInit`

### 常量与宏 (16)

- `XHTTP_FEATURE_HTTP_RANGE_MULTIPART`
- `XHTTP_FEATURE_HTTP_SERVER_STATIC`
- `XHTTP_FEATURE_HTTP_STATIC_FILE`
- `XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY`
- `XHTTP_FEATURE_HTTP_STATIC_PATH`
- `XHTTP_FEATURE_HTTP_STATIC_PLAN`
- `XHTTP_FEATURE_HTTP_STATIC_RESPONSE`
- `XHTTP_FEATURE_MIME_TYPES`
- `XHTTP_SERVER_STATIC_ERROR_BODY`
- `XHTTP_SERVER_STATIC_ERROR_BOUNDARY`
- `XHTTP_SERVER_STATIC_ERROR_CONFIG`
- `XHTTP_SERVER_STATIC_ERROR_PATH`
- `XHTTP_SERVER_STATIC_ERROR_PLAN`
- `XHTTP_SERVER_STATIC_ERROR_REPLY`
- `XHTTP_SERVER_STATIC_ERROR_RESPONSE`
- `XHTTP_SERVER_STATIC_ERROR_TARGET`

### 类型 (8)

- `xhttpserverstaticerror`
- `xhttpstaticfile`
- `xhttpstaticpathconfig`
- `xhttpstaticplanconfig`
- `xhttpstaticreplyconfig`
- `xhttpstaticresponse`
- `xhttpstaticserveconfig`
- `xroot`

## `extlibs/xhttp-server/include/xrt/http_server_tls.h`

[查看带契约注释的公共头](../../include/xrt/http_server_tls.h)

### 函数 (3)

- `xrtHttpConnTls`
- `xrtHttpServerStartTls`
- `xrtHttpServerTlsConfigInit`

### 类型 (3)

- `xtlsserverconfig`
- `xtlsstream`
- `xtlsstreamconfig`

## `extlibs/xhttp-server/include/xrt/http_server_upgrade.h`

[查看带契约注释的公共头](../../include/xrt/http_server_upgrade.h)

### 函数 (4)

- `xrtHttpConnUpgrade`
- `xrtHttpConnUpgradeRaw`
- `xrtHttpConnUpgradeResponse`
- `xrtHttpUpgradeAbort`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_SERVER_UPGRADE`

### 类型 (3)

- `xhttp1serverresponse`
- `xhttpupgrade`
- `xhttpupgradeproc`

## `extlibs/xhttp-server/include/xrt/http_sse_server.h`

[查看带契约注释的公共头](../../include/xrt/http_sse_server.h)

### 函数 (4)

- `xrtHttpSseReplyCreate`
- `xrtHttpSseSend`
- `xrtHttpSseSendComment`
- `xrtHttpSseSendEvent`

### 常量与宏 (3)

- `XHTTP_FEATURE_HTTP_BODY_STREAM`
- `XHTTP_FEATURE_HTTP_SSE_HTTP`
- `XHTTP_FEATURE_HTTP_SSE_SERVER`

### 类型 (4)

- `xhttpbodystream`
- `xhttpbodystreamconfig`
- `xhttpbodystreamresult`
- `xhttpsseevent`

## `extlibs/xhttp-server/include/xrt/http_static.h`

[查看带契约注释的公共头](../../include/xrt/http_static.h)

### 函数 (21)

- `xrtHttpStaticFileDestroy`
- `xrtHttpStaticFileFuture`
- `xrtHttpStaticFileInfo`
- `xrtHttpStaticFileOpen`
- `xrtHttpStaticFileRef`
- `xrtHttpStaticFileRepresentation`
- `xrtHttpStaticFileSize`
- `xrtHttpStaticFileTakeBody`
- `xrtHttpStaticFileTakeBodyAll`
- `xrtHttpStaticFileTakeFile`
- `xrtHttpStaticFileTakeMultipartBody`
- `xrtHttpStaticMultipartBodyAdopt`
- `xrtHttpStaticPathConfigInit`
- `xrtHttpStaticPathFree`
- `xrtHttpStaticPathInit`
- `xrtHttpStaticPathMap`
- `xrtHttpStaticPathWrite`
- `xrtHttpStaticPlanBuild`
- `xrtHttpStaticPlanConfigInit`
- `xrtHttpStaticResponseBuild`
- `xrtHttpStaticResponseConfigInit`

### 常量与宏 (19)

- `XHTTP_FEATURE_HTTP_PRECONDITION`
- `XHTTP_FEATURE_HTTP_RANGE`
- `XHTTP_STATIC_FILE_ERROR_ADOPT`
- `XHTTP_STATIC_FILE_ERROR_BODY`
- `XHTTP_STATIC_FILE_ERROR_CONSUMED`
- `XHTTP_STATIC_FILE_ERROR_OPEN`
- `XHTTP_STATIC_FILE_ERROR_RANGE`
- `XHTTP_STATIC_FILE_ERROR_REFERENCE`
- `XHTTP_STATIC_FILE_ERROR_SIZE`
- `XHTTP_STATIC_FILE_ERROR_STAT`
- `XHTTP_STATIC_FILE_ERROR_SUBMIT`
- `XHTTP_STATIC_FILE_ERROR_TYPE`
- `XHTTP_STATIC_MULTIPART_BODY_ERROR_ADOPT`
- `XHTTP_STATIC_PATH_ALLOW_HIDDEN`
- `XHTTP_STATIC_PATH_ERROR`
- `XHTTP_STATIC_PATH_MATCH`
- `XHTTP_STATIC_PATH_NO_MATCH`
- `XHTTP_STATIC_PATH_PORTABLE`
- `XHTTP_STATIC_RESPONSE_MAX_FIELDS`

### 类型 (11)

- `xfileinfo`
- `xhttpbyterange`
- `xhttpfield`
- `xhttprepresentation`
- `xhttpstaticfileerror`
- `xhttpstaticmultipartbodyerror`
- `xhttpstaticpath`
- `xhttpstaticpathflag`
- `xhttpstaticpathstatus`
- `xhttpstaticplan`
- `xhttpstaticresponseconfig`
