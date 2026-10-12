# xhttp-client 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xhttp-client/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `87` 个函数、`147` 个常量或宏、
`83` 个公共类型。

## `extlibs/xhttp-client/include/xhttp-client/export.h`

[查看带契约注释的公共头](../../include/xhttp-client/export.h)

### 常量与宏 (2)

- `XHTTP_CLIENT_BUILD_SHARED`
- `XHTTP_CLIENT_USE_SHARED`

## `extlibs/xhttp-client/include/xrt/http_client_easy.h`

[查看带契约注释的公共头](../../include/xrt/http_client_easy.h)

### 函数 (9)

- `xrtHttpClientGet`
- `xrtHttpClientGetAsync`
- `xrtHttpClientGetSync`
- `xrtHttpClientPost`
- `xrtHttpClientPostAsync`
- `xrtHttpClientPostSync`
- `xrtHttpClientSendBytes`
- `xrtHttpClientSendBytesAsync`
- `xrtHttpClientSendBytesSync`

### 常量与宏 (4)

- `XHTTP_FEATURE_HTTP_CLIENT`
- `XHTTP_FEATURE_HTTP_CLIENT_EASY`
- `XHTTP_FEATURE_HTTP_CLIENT_EASY_FUTURE`
- `XHTTP_FEATURE_HTTP_CLIENT_FUTURE`

### 类型 (8)

- `xbytesview`
- `xfuture`
- `xhttpcall`
- `xhttpcalloptions`
- `xhttpcallproc`
- `xhttpclient`
- `xhttpresult`
- `xstrview`

## `extlibs/xhttp-client/include/xrt/http_client_future.h`

[查看带契约注释的公共头](../../include/xrt/http_client_future.h)

### 函数 (15)

- `xrtHttpClientDoAsync`
- `xrtHttpClientDoSync`
- `xrtHttpClientWaitAsync`
- `xrtHttpResultBuffered`
- `xrtHttpResultDestroy`
- `xrtHttpResultInfo`
- `xrtHttpResultRedirects`
- `xrtHttpResultRef`
- `xrtHttpResultResponse`
- `xrtHttpResultTakeResponse`
- `xrtHttpResultTakeTcp`
- `xrtHttpResultTakeTls`
- `xrtHttpResultTcp`
- `xrtHttpResultTls`
- `xrtHttpResultUpgraded`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_CLIENT_HTTPS`

### 类型 (5)

- `xhttpcallinfo`
- `xhttprequest`
- `xhttpresponse`
- `xnetstream`
- `xtlsstream`

## `extlibs/xhttp-client/include/xrt/http_client_runtime.h`

[查看带契约注释的公共头](../../include/xrt/http_client_runtime.h)

### 函数 (39)

- `xrtHttpCallCancel`
- `xrtHttpCallDestroy`
- `xrtHttpCallError`
- `xrtHttpCallInfo`
- `xrtHttpCallOptionsInit`
- `xrtHttpCallPause`
- `xrtHttpCallPaused`
- `xrtHttpCallRef`
- `xrtHttpCallRequestClone`
- `xrtHttpCallResume`
- `xrtHttpCallState`
- `xrtHttpCallWorker`
- `xrtHttpClientAbort`
- `xrtHttpClientCache`
- `xrtHttpClientCacheConfigInit`
- `xrtHttpClientCacheOptionsInit`
- `xrtHttpClientCloseIdle`
- `xrtHttpClientConfigInit`
- `xrtHttpClientCookieJar`
- `xrtHttpClientCreate`
- `xrtHttpClientCreateTls`
- `xrtHttpClientCreateWithResolver`
- `xrtHttpClientDestroy`
- `xrtHttpClientDo`
- `xrtHttpClientDrain`
- `xrtHttpClientEngine`
- `xrtHttpClientPoolConfigInit`
- `xrtHttpClientProxy`
- `xrtHttpClientRef`
- `xrtHttpClientResumeClear`
- `xrtHttpClientResumeStats`
- `xrtHttpClientState`
- `xrtHttpClientStats`
- `xrtHttpCookieOptionsInit`
- `xrtHttpDecompressConfigInit`
- `xrtHttpProxyOptionsInit`
- `xrtHttpResumeConfigInit`
- `xrtHttpRetryConfigInit`
- `xrtHttpRetryOptionsInit`

### 常量与宏 (108)

- `XHTTP_CALL_CANCELLED`
- `XHTTP_CALL_DIALING`
- `XHTTP_CALL_EXCHANGING`
- `XHTTP_CALL_FAILED`
- `XHTTP_CALL_HANDSHAKING`
- `XHTTP_CALL_PHASE_CACHE`
- `XHTTP_CALL_PHASE_CONNECT`
- `XHTTP_CALL_PHASE_POOL`
- `XHTTP_CALL_PHASE_PROXY`
- `XHTTP_CALL_PHASE_QUEUED`
- `XHTTP_CALL_PHASE_REQUEST`
- `XHTTP_CALL_PHASE_RESPONSE_BODY`
- `XHTTP_CALL_PHASE_RESPONSE_HEADERS`
- `XHTTP_CALL_PHASE_RETRY`
- `XHTTP_CALL_PHASE_TLS`
- `XHTTP_CALL_QUEUED`
- `XHTTP_CALL_SUCCEEDED`
- `XHTTP_CLIENT_ABORTING`
- `XHTTP_CLIENT_CACHE_BODY_DEFAULT`
- `XHTTP_CLIENT_CACHE_BYPASS`
- `XHTTP_CLIENT_CACHE_DEFAULT`
- `XHTTP_CLIENT_CACHE_DISABLED`
- `XHTTP_CLIENT_CACHE_HEURISTIC_MAX_DEFAULT`
- `XHTTP_CLIENT_CACHE_HEURISTIC_PERCENT_DEFAULT`
- `XHTTP_CLIENT_CACHE_HIT`
- `XHTTP_CLIENT_CACHE_MAX_RANGES_DEFAULT`
- `XHTTP_CLIENT_CACHE_MISS`
- `XHTTP_CLIENT_CACHE_NONE`
- `XHTTP_CLIENT_CACHE_ONLY`
- `XHTTP_CLIENT_CACHE_ONLY_MISS`
- `XHTTP_CLIENT_CACHE_RELOAD`
- `XHTTP_CLIENT_CACHE_REVALIDATED`
- `XHTTP_CLIENT_CACHE_STALE`
- `XHTTP_CLIENT_CACHE_UPDATED`
- `XHTTP_CLIENT_CLOSED`
- `XHTTP_CLIENT_DRAINING`
- `XHTTP_CLIENT_ERROR_ARGUMENT`
- `XHTTP_CLIENT_ERROR_CACHE`
- `XHTTP_CLIENT_ERROR_CALLBACK`
- `XHTTP_CLIENT_ERROR_CANCELLED`
- `XHTTP_CLIENT_ERROR_CONFIG`
- `XHTTP_CLIENT_ERROR_COOKIE`
- `XHTTP_CLIENT_ERROR_DECOMPRESSION`
- `XHTTP_CLIENT_ERROR_DIAL`
- `XHTTP_CLIENT_ERROR_INTERNAL`
- `XHTTP_CLIENT_ERROR_NONE`
- `XHTTP_CLIENT_ERROR_POOL`
- `XHTTP_CLIENT_ERROR_PROTOCOL`
- `XHTTP_CLIENT_ERROR_PROXY`
- `XHTTP_CLIENT_ERROR_REDIRECT`
- `XHTTP_CLIENT_ERROR_REDIRECT_DOWNGRADE`
- `XHTTP_CLIENT_ERROR_REDIRECT_LIMIT`
- `XHTTP_CLIENT_ERROR_REDIRECT_REPLAY`
- `XHTTP_CLIENT_ERROR_REQUEST`
- `XHTTP_CLIENT_ERROR_RESPONSE`
- `XHTTP_CLIENT_ERROR_RETRY`
- `XHTTP_CLIENT_ERROR_STATE`
- `XHTTP_CLIENT_ERROR_TIMEOUT_IDLE`
- `XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL`
- `XHTTP_CLIENT_ERROR_TLS`
- `XHTTP_CLIENT_ERROR_TRANSPORT`
- `XHTTP_CLIENT_IDLE_TIMEOUT_DEFAULT`
- `XHTTP_CLIENT_RUNNING`
- `XHTTP_CLIENT_TIMEOUT_DEFAULT`
- `XHTTP_CLIENT_TIMEOUT_NONE`
- `XHTTP_COOKIE_DISABLED`
- `XHTTP_COOKIE_SAME_SITE`
- `XHTTP_COOKIE_TOP_LEVEL`
- `XHTTP_DECOMPRESS_AUTO`
- `XHTTP_DECOMPRESS_BODY_DEFAULT`
- `XHTTP_DECOMPRESS_CODINGS_DEFAULT`
- `XHTTP_DECOMPRESS_CODINGS_MAX`
- `XHTTP_DECOMPRESS_DEFAULT`
- `XHTTP_DECOMPRESS_RAW`
- `XHTTP_FEATURE_COOKIE_JAR_HEADERS`
- `XHTTP_FEATURE_HTTP_CACHE_RANGE`
- `XHTTP_FEATURE_HTTP_CACHE_STORE`
- `XHTTP_FEATURE_HTTP_CLIENT_CACHE`
- `XHTTP_FEATURE_HTTP_CLIENT_COOKIES`
- `XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS`
- `XHTTP_FEATURE_HTTP_CLIENT_POOL`
- `XHTTP_FEATURE_HTTP_CLIENT_PROXY`
- `XHTTP_FEATURE_HTTP_CLIENT_REDIRECT`
- `XHTTP_FEATURE_HTTP_CLIENT_RESUME`
- `XHTTP_FEATURE_HTTP_CLIENT_RETRY`
- `XHTTP_FEATURE_HTTP_CLIENT_STREAM`
- `XHTTP_FEATURE_HTTP_CLIENT_TLS`
- `XHTTP_FEATURE_HTTP_RANGE_MULTIPART`
- `XHTTP_FEATURE_HTTP_REDIRECT_POLICY`
- `XHTTP_FEATURE_HTTP_RETRY`
- `XHTTP_FEATURE_HTTP_RETRY_POLICY`
- `XHTTP_FEATURE_MULTIPART`
- `XHTTP_PROXY_DEFAULT`
- `XHTTP_PROXY_DIRECT`
- `XHTTP_PROXY_EXPLICIT`
- `XHTTP_RESUME_ENTRIES_DEFAULT`
- `XHTTP_RESUME_ORIGIN_DEFAULT`
- `XHTTP_RETRY_BASE_DEFAULT`
- `XHTTP_RETRY_DEFAULT`
- `XHTTP_RETRY_DELAY_MAX_DEFAULT`
- `XHTTP_RETRY_DISABLED`
- `XHTTP_RETRY_ENABLED`
- `XHTTP_RETRY_JITTER`
- `XHTTP_RETRY_MAX_DEFAULT`
- `XHTTP_RETRY_RESPECT_AFTER`
- `XHTTP_RETRY_STATUS`
- `XHTTP_RETRY_TRANSPORT`
- `XHTTP_RETRY_UNSAFE`

### 类型 (46)

- `xcancel`
- `xcookiejar`
- `xerror`
- `xhttp1callconfig`
- `xhttp1exchangeconfig`
- `xhttp1requestoptions`
- `xhttpcache`
- `xhttpcallbodyproc`
- `xhttpcallevents`
- `xhttpcallheadersproc`
- `xhttpcallinformationalproc`
- `xhttpcallphase`
- `xhttpcallresult`
- `xhttpcallstate`
- `xhttpclientcacheconfig`
- `xhttpclientcachemode`
- `xhttpclientcacheoptions`
- `xhttpclientcacheoutcome`
- `xhttpclientconfig`
- `xhttpclienterror`
- `xhttpclientpoolconfig`
- `xhttpclientstate`
- `xhttpclientstats`
- `xhttpcookieoptions`
- `xhttpdecompressconfig`
- `xhttpdecompressmode`
- `xhttpproxymode`
- `xhttpproxyoptions`
- `xhttpredirectconfig`
- `xhttpredirectmode`
- `xhttpresumeconfig`
- `xhttpresumestats`
- `xhttpretryconfig`
- `xhttpretrymode`
- `xhttpretryoptions`
- `xnetdialconfig`
- `xnetengine`
- `xnetproxy`
- `xnetresolver`
- `xnetresolverconfig`
- `xnetresult`
- `xnetworker`
- `xtime`
- `xtlscontext`
- `xtlsstreamconfig`
- `xtlsverifier`

## `extlibs/xhttp-client/include/xrt/http_client_stream.h`

[查看带契约注释的公共头](../../include/xrt/http_client_stream.h)

### 函数 (12)

- `xrtHttp1CallCancel`
- `xrtHttp1CallConfigInit`
- `xrtHttp1CallDestroy`
- `xrtHttp1CallError`
- `xrtHttp1CallEventsInit`
- `xrtHttp1CallPause`
- `xrtHttp1CallPaused`
- `xrtHttp1CallRef`
- `xrtHttp1CallResume`
- `xrtHttp1CallState`
- `xrtHttp1CallTcp`
- `xrtHttp1CallTls`

### 常量与宏 (3)

- `XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC`
- `XHTTP_FEATURE_HTTP_EXCHANGE`
- `XHTTP_FEATURE_HTTP_EXCHANGE_ASYNC`

### 类型 (9)

- `xhttp1call`
- `xhttp1callerror`
- `xhttp1callevents`
- `xhttp1callproc`
- `xhttp1callresult`
- `xhttp1callstate`
- `xhttp1exchange`
- `xhttp1progress`
- `xhttp1progressproc`

## `extlibs/xhttp-client/include/xrt/http_sse_client.h`

[查看带契约注释的公共头](../../include/xrt/http_sse_client.h)

### 函数 (12)

- `xrtHttpSseClientClose`
- `xrtHttpSseClientConfigInit`
- `xrtHttpSseClientDestroy`
- `xrtHttpSseClientError`
- `xrtHttpSseClientInfo`
- `xrtHttpSseClientPause`
- `xrtHttpSseClientPaused`
- `xrtHttpSseClientRef`
- `xrtHttpSseClientResume`
- `xrtHttpSseClientState`
- `xrtHttpSseConnect`
- `xrtHttpSseConnectRequest`

### 常量与宏 (29)

- `XHTTP_FEATURE_HTTP_SSE_CLIENT`
- `XHTTP_FEATURE_HTTP_SSE_HTTP`
- `XHTTP_FEATURE_HTTP_SSE_PARSER`
- `XHTTP_SSE_CLIENT_CLOSED`
- `XHTTP_SSE_CLIENT_CONNECTING`
- `XHTTP_SSE_CLIENT_ERROR_ARGUMENT`
- `XHTTP_SSE_CLIENT_ERROR_CALLBACK`
- `XHTTP_SSE_CLIENT_ERROR_CANCELLED`
- `XHTTP_SSE_CLIENT_ERROR_CONFIG`
- `XHTTP_SSE_CLIENT_ERROR_HTTP`
- `XHTTP_SSE_CLIENT_ERROR_INTERNAL`
- `XHTTP_SSE_CLIENT_ERROR_PARSE`
- `XHTTP_SSE_CLIENT_ERROR_RECONNECT`
- `XHTTP_SSE_CLIENT_ERROR_REQUEST`
- `XHTTP_SSE_CLIENT_ERROR_RESPONSE`
- `XHTTP_SSE_CLIENT_ERROR_STATE`
- `XHTTP_SSE_CLIENT_OPEN`
- `XHTTP_SSE_CLOSE_CALLBACK`
- `XHTTP_SSE_CLOSE_CANCELLED`
- `XHTTP_SSE_CLOSE_HTTP`
- `XHTTP_SSE_CLOSE_INTERNAL`
- `XHTTP_SSE_CLOSE_PARSE`
- `XHTTP_SSE_CLOSE_RECONNECT_LIMIT`
- `XHTTP_SSE_CLOSE_REJECTED`
- `XHTTP_SSE_CLOSE_STOP`
- `XHTTP_SSE_CLOSE_USER`
- `XHTTP_SSE_RECONNECT_MAX_DEFAULT`
- `XHTTP_SSE_RETRY_MAX_DEFAULT`
- `XHTTP_SSE_RETRY_MIN_DEFAULT`

### 类型 (15)

- `xhttpsseclient`
- `xhttpsseclientconfig`
- `xhttpsseclienterror`
- `xhttpsseclientevents`
- `xhttpsseclientinfo`
- `xhttpsseclientstate`
- `xhttpssecloseproc`
- `xhttpsseclosereason`
- `xhttpssecommentproc`
- `xhttpssemessage`
- `xhttpssemessageproc`
- `xhttpsseopenproc`
- `xhttpsseparserconfig`
- `xhttpsseretryingproc`
- `xhttpsseretryproc`
