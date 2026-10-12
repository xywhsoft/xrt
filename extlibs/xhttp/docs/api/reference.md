# xhttp 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xhttp/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `719` 个函数、`390` 个常量或宏、
`226` 个公共类型。

## `extlibs/xhttp/include/xhttp/export.h`

[查看带契约注释的公共头](../../include/xhttp/export.h)

### 常量与宏 (2)

- `XHTTP_BUILD_SHARED`
- `XHTTP_USE_SHARED`

## `extlibs/xhttp/include/xrt/cookie.h`

[查看带契约注释的公共头](../../include/xrt/cookie.h)

### 函数 (12)

- `xrtCookieBuild`
- `xrtCookieDateParse`
- `xrtCookieFind`
- `xrtCookieNext`
- `xrtCookieParse`
- `xrtCookieValidate`
- `xrtCookieWrite`
- `xrtSetCookieAttributeNext`
- `xrtSetCookieBuild`
- `xrtSetCookieParse`
- `xrtSetCookieValidate`
- `xrtSetCookieWrite`

### 常量与宏 (2)

- `XHTTP_FEATURE_COOKIE`
- `XHTTP_FEATURE_SET_COOKIE`

### 类型 (10)

- `xcookieattribute`
- `xcookieattributenext`
- `xcookielimits`
- `xcookienext`
- `xcookiepair`
- `xcookiepriority`
- `xcookiesamesite`
- `xsetcookie`
- `xstrview`
- `xtime`

## `extlibs/xhttp/include/xrt/form.h`

[查看带契约注释的公共头](../../include/xrt/form.h)

### 函数 (8)

- `xrtFormBuild`
- `xrtFormDecode`
- `xrtFormDecodeNew`
- `xrtFormEncode`
- `xrtFormEncodeNew`
- `xrtFormFind`
- `xrtFormParse`
- `xrtFormWrite`

### 常量与宏 (2)

- `XHTTP_FEATURE_FORM_URLENCODED`
- `XHTTP_FEATURE_QUERY`

### 类型 (4)

- `xbytesview`
- `xformfield`
- `xformfind`
- `xformlimits`

## `extlibs/xhttp/include/xrt/form_data.h`

[查看带契约注释的公共头](../../include/xrt/form_data.h)

### 函数 (24)

- `xrtFormDataAppendBody`
- `xrtFormDataAppendBytes`
- `xrtFormDataAppendText`
- `xrtFormDataAt`
- `xrtFormDataBody`
- `xrtFormDataBodyRandom`
- `xrtFormDataClear`
- `xrtFormDataClone`
- `xrtFormDataConfigInit`
- `xrtFormDataCount`
- `xrtFormDataCountName`
- `xrtFormDataCreate`
- `xrtFormDataDestroy`
- `xrtFormDataFind`
- `xrtFormDataGet`
- `xrtFormDataHas`
- `xrtFormDataMetadata`
- `xrtFormDataParse`
- `xrtFormDataParseContentType`
- `xrtFormDataRemove`
- `xrtFormDataReserve`
- `xrtFormDataSetBody`
- `xrtFormDataSetBytes`
- `xrtFormDataSetText`

### 常量与宏 (10)

- `XHTTP_FEATURE_FORM_DATA`
- `XHTTP_FEATURE_FORM_DATA_MULTIPART`
- `XHTTP_FEATURE_FORM_DATA_PARSE`
- `XHTTP_FEATURE_FORM_DATA_RANDOM`
- `XHTTP_FEATURE_HTTP_BODY`
- `XHTTP_FEATURE_HTTP_BODY_COMPOSE`
- `XHTTP_FEATURE_MIME`
- `XHTTP_FEATURE_MULTIPART`
- `XHTTP_FEATURE_MULTIPART_RANDOM`
- `XHTTP_FEATURE_MULTIPART_WRITE`

### 类型 (10)

- `xformdata`
- `xformdataconfig`
- `xformdataerror`
- `xformdatapart`
- `xformdatapartflags`
- `xhttpbody`
- `xhttpnext`
- `xmultipartboundary`
- `xmultiparterrorinfo`
- `xmultipartlimits`

## `extlibs/xhttp/include/xrt/http_accept.h`

[查看带契约注释的公共头](../../include/xrt/http_accept.h)

### 函数 (8)

- `xrtHttpAcceptCursorInit`
- `xrtHttpAcceptMatch`
- `xrtHttpAcceptNext`
- `xrtHttpAcceptQuality`
- `xrtHttpAcceptSelect`
- `xrtHttpMediaRangeMatch`
- `xrtHttpMediaRangeNext`
- `xrtHttpMediaRangeParamNext`

### 常量与宏 (4)

- `XHTTP_FEATURE_HTTP_ACCEPT`
- `XHTTP_MEDIA_RANGE_ANY`
- `XHTTP_MEDIA_RANGE_EXACT`
- `XHTTP_MEDIA_RANGE_TYPE`

### 类型 (7)

- `xhttpacceptcursor`
- `xhttpacceptmatch`
- `xhttpfield`
- `xhttpmediarange`
- `xhttpmediarangespecificity`
- `xhttpparam`
- `xmediatype`

## `extlibs/xhttp/include/xrt/http_auth.h`

[查看带契约注释的公共头](../../include/xrt/http_auth.h)

### 函数 (30)

- `xrtHttpAuthBuild`
- `xrtHttpAuthCursorInit`
- `xrtHttpAuthParamNext`
- `xrtHttpAuthParse`
- `xrtHttpAuthToken68Valid`
- `xrtHttpAuthWrite`
- `xrtHttpBasicChallengeRead`
- `xrtHttpBasicChallengeWrite`
- `xrtHttpBasicRead`
- `xrtHttpBasicWrite`
- `xrtHttpBearerChallengeRead`
- `xrtHttpBearerChallengeWrite`
- `xrtHttpBearerRead`
- `xrtHttpBearerTokenValid`
- `xrtHttpBearerWrite`
- `xrtHttpChallengeNext`
- `xrtHttpDigestAlgorithmName`
- `xrtHttpDigestAlgorithmParse`
- `xrtHttpDigestAlgorithmSession`
- `xrtHttpDigestAlgorithmSupported`
- `xrtHttpDigestAuthRead`
- `xrtHttpDigestAuthWrite`
- `xrtHttpDigestChallengeRead`
- `xrtHttpDigestChallengeWrite`
- `xrtHttpDigestInfoRead`
- `xrtHttpDigestInfoWrite`
- `xrtHttpDigestQopName`
- `xrtHttpDigestQopParse`
- `xrtHttpDigestSize`
- `xrtHttpFieldChallengeNext`

### 常量与宏 (48)

- `XHTTP_AUTH_NONE`
- `XHTTP_AUTH_PARAMS`
- `XHTTP_AUTH_TOKEN68`
- `XHTTP_BEARER_HAS_ERROR`
- `XHTTP_BEARER_HAS_ERROR_DESCRIPTION`
- `XHTTP_BEARER_HAS_ERROR_URI`
- `XHTTP_BEARER_HAS_REALM`
- `XHTTP_BEARER_HAS_SCOPE`
- `XHTTP_DIGEST_ALGORITHM_MD5`
- `XHTTP_DIGEST_ALGORITHM_MD5_SESSION`
- `XHTTP_DIGEST_ALGORITHM_SHA256`
- `XHTTP_DIGEST_ALGORITHM_SHA256_SESSION`
- `XHTTP_DIGEST_ALGORITHM_SHA512_256`
- `XHTTP_DIGEST_ALGORITHM_SHA512_256_SESSION`
- `XHTTP_DIGEST_ALGORITHM_UNKNOWN`
- `XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT`
- `XHTTP_DIGEST_AUTH_HAS_OPAQUE`
- `XHTTP_DIGEST_AUTH_HAS_USERHASH`
- `XHTTP_DIGEST_AUTH_USERHASH`
- `XHTTP_DIGEST_AUTH_USERNAME_EXTENDED`
- `XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT`
- `XHTTP_DIGEST_CHALLENGE_HAS_DOMAIN`
- `XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE`
- `XHTTP_DIGEST_CHALLENGE_HAS_STALE`
- `XHTTP_DIGEST_CHALLENGE_HAS_USERHASH`
- `XHTTP_DIGEST_CHALLENGE_QOP_AUTH`
- `XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT`
- `XHTTP_DIGEST_CHALLENGE_STALE`
- `XHTTP_DIGEST_CHALLENGE_USERHASH`
- `XHTTP_DIGEST_CHALLENGE_UTF8`
- `XHTTP_DIGEST_INFO_HAS_NEXT_NONCE`
- `XHTTP_DIGEST_INFO_HAS_RESPONSE`
- `XHTTP_DIGEST_MAX_SIZE`
- `XHTTP_DIGEST_MAX_TEXT_SIZE`
- `XHTTP_DIGEST_QOP_AUTH`
- `XHTTP_DIGEST_QOP_AUTH_INT`
- `XHTTP_DIGEST_QOP_NONE`
- `XHTTP_FEATURE_HTTP_AUTH`
- `XHTTP_FEATURE_HTTP_AUTH_BASIC`
- `XHTTP_FEATURE_HTTP_AUTH_BEARER`
- `XHTTP_FEATURE_HTTP_AUTH_BEARER_CHALLENGE`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_INFO`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE`
- `XHTTP_FEATURE_HTTP_EXT_VALUE`
- `XHTTP_FEATURE_URL_PARAM`

### 类型 (11)

- `xhttpauth`
- `xhttpauthcursor`
- `xhttpauthkind`
- `xhttpbasicauth`
- `xhttpbasicchallenge`
- `xhttpbearerchallenge`
- `xhttpdigestalgorithm`
- `xhttpdigestauth`
- `xhttpdigestchallenge`
- `xhttpdigestinfo`
- `xhttpdigestqop`

## `extlibs/xhttp/include/xrt/http_body.h`

[查看带契约注释的公共头](../../include/xrt/http_body.h)

### 函数 (20)

- `xrtHttpBodyBorrow`
- `xrtHttpBodyChunkRelease`
- `xrtHttpBodyCopy`
- `xrtHttpBodyCreate`
- `xrtHttpBodyDestroy`
- `xrtHttpBodyEmpty`
- `xrtHttpBodyFlags`
- `xrtHttpBodyLength`
- `xrtHttpBodyNext`
- `xrtHttpBodyOpen`
- `xrtHttpBodyRead`
- `xrtHttpBodyReaderBytes`
- `xrtHttpBodyReaderDestroy`
- `xrtHttpBodyReaderError`
- `xrtHttpBodyReaderWait`
- `xrtHttpBodyRef`
- `xrtHttpBodyReference`
- `xrtHttpBodyReplayable`
- `xrtHttpBodyTake`
- `xrtHttpBodyView`

### 常量与宏 (13)

- `XHTTP_BODY_AGAIN`
- `XHTTP_BODY_DATA`
- `XHTTP_BODY_EOF`
- `XHTTP_BODY_ERROR`
- `XHTTP_BODY_ERROR_CONTRACT`
- `XHTTP_BODY_ERROR_LENGTH`
- `XHTTP_BODY_ERROR_REOPEN`
- `XHTTP_BODY_ERROR_SOURCE`
- `XHTTP_BODY_NONE`
- `XHTTP_BODY_REPLAYABLE`
- `XHTTP_BODY_UNKNOWN`
- `XHTTP_FEATURE_HTTP_BODY_ASYNC`
- `XHTTP_FEATURE_HTTP_BODY_TRANSFORM`

### 类型 (15)

- `xerror`
- `xfuture`
- `xhttpbodychunk`
- `xhttpbodycloseproc`
- `xhttpbodydestroyproc`
- `xhttpbodyerror`
- `xhttpbodyflag`
- `xhttpbodynextproc`
- `xhttpbodyopenproc`
- `xhttpbodyops`
- `xhttpbodyreader`
- `xhttpbodyreaderops`
- `xhttpbodyreleaseproc`
- `xhttpbodystatus`
- `xhttpbodywaitproc`

## `extlibs/xhttp/include/xrt/http_body_compose.h`

[查看带契约注释的公共头](../../include/xrt/http_body_compose.h)

### 函数 (3)

- `xrtHttpBodyCompose`
- `xrtHttpBodyPieceBody`
- `xrtHttpBodyPieceBytes`

### 常量与宏 (2)

- `XHTTP_BODY_PIECE_BODY`
- `XHTTP_BODY_PIECE_BYTES`

### 类型 (2)

- `xhttpbodypiece`
- `xhttpbodypiecekind`

## `extlibs/xhttp/include/xrt/http_body_decode.h`

[查看带契约注释的公共头](../../include/xrt/http_body_decode.h)

### 函数 (3)

- `xrtHttpBodyDecode`
- `xrtHttpBodyDecodeConfigInit`
- `xrtHttpBodyDecodeFields`

### 常量与宏 (6)

- `XHTTP_BODY_DECODE_APPLIED`
- `XHTTP_BODY_DECODE_ERROR`
- `XHTTP_BODY_DECODE_UNCHANGED`
- `XHTTP_BODY_DECODE_UNSUPPORTED`
- `XHTTP_FEATURE_HTTP_BODY_DECODE`
- `XHTTP_FEATURE_HTTP_BODY_INFLATE`

### 类型 (3)

- `xhttpbodydecodeconfig`
- `xhttpbodydecoderesult`
- `xhttpbodyinflateconfig`

## `extlibs/xhttp/include/xrt/http_body_deflate.h`

[查看带契约注释的公共头](../../include/xrt/http_body_deflate.h)

### 函数 (2)

- `xrtHttpBodyDeflate`
- `xrtHttpBodyDeflateConfigInit`

### 常量与宏 (3)

- `XHTTP_BODY_DEFLATE_QUEUE_DEFAULT`
- `XHTTP_BODY_DEFLATE_READ_DEFAULT`
- `XHTTP_FEATURE_HTTP_BODY_DEFLATE`

### 类型 (2)

- `xdeflateconfig`
- `xhttpbodydeflateconfig`

## `extlibs/xhttp/include/xrt/http_body_inflate.h`

[查看带契约注释的公共头](../../include/xrt/http_body_inflate.h)

### 函数 (2)

- `xrtHttpBodyInflate`
- `xrtHttpBodyInflateConfigInit`

### 常量与宏 (3)

- `XHTTP_BODY_INFLATE_OUTPUT_DEFAULT`
- `XHTTP_BODY_INFLATE_QUEUE_DEFAULT`
- `XHTTP_BODY_INFLATE_READ_DEFAULT`

### 类型 (1)

- `xinflateconfig`

## `extlibs/xhttp/include/xrt/http_body_stream.h`

[查看带契约注释的公共头](../../include/xrt/http_body_stream.h)

### 函数 (12)

- `xrtHttpBodyStreamClose`
- `xrtHttpBodyStreamConfigInit`
- `xrtHttpBodyStreamConfigValid`
- `xrtHttpBodyStreamCreate`
- `xrtHttpBodyStreamDestroy`
- `xrtHttpBodyStreamFail`
- `xrtHttpBodyStreamInfo`
- `xrtHttpBodyStreamRef`
- `xrtHttpBodyStreamWaitWritable`
- `xrtHttpBodyStreamWrite`
- `xrtHttpBodyStreamWriteRef`
- `xrtHttpBodyStreamWriteTake`

### 常量与宏 (13)

- `XHTTP_BODY_STREAM_AGAIN`
- `XHTTP_BODY_STREAM_BYTES_DEFAULT`
- `XHTTP_BODY_STREAM_CHUNKS_DEFAULT`
- `XHTTP_BODY_STREAM_CLOSED`
- `XHTTP_BODY_STREAM_ERROR`
- `XHTTP_BODY_STREAM_ERROR_ARGUMENT`
- `XHTTP_BODY_STREAM_ERROR_CONFIG`
- `XHTTP_BODY_STREAM_ERROR_FAILED`
- `XHTTP_BODY_STREAM_ERROR_INTERNAL`
- `XHTTP_BODY_STREAM_ERROR_LIMIT`
- `XHTTP_BODY_STREAM_ERROR_STATE`
- `XHTTP_BODY_STREAM_OK`
- `XHTTP_FEATURE_HTTP_BODY_STREAM`

### 类型 (5)

- `xhttpbodystream`
- `xhttpbodystreamconfig`
- `xhttpbodystreamerror`
- `xhttpbodystreaminfo`
- `xhttpbodystreamresult`

## `extlibs/xhttp/include/xrt/http_bridge.h`

[查看带契约注释的公共头](../../include/xrt/http_bridge.h)

### 函数 (43)

- `xrtHttpBridge1ExchangeConfigValid`
- `xrtHttpBridge1RequestPrepareField`
- `xrtHttpBridge1ServerConfigValid`
- `xrtHttpBridge1ServerResponseOwnRefs`
- `xrtHttpBridge1ServerResponsePrepareSource`
- `xrtHttpBridgeBodyStreamBuild`
- `xrtHttpBridgeByteRangeNextValue`
- `xrtHttpBridgeCacheTimeAdd`
- `xrtHttpBridgeCacheTimeSeconds`
- `xrtHttpBridgeContentRangeParseValue`
- `xrtHttpBridgeDigestHexValue`
- `xrtHttpBridgeDigestHexViewValid`
- `xrtHttpBridgeDigestNonceCountWrite`
- `xrtHttpBridgeETagParseValue`
- `xrtHttpBridgeETagValid`
- `xrtHttpBridgeOriginTupleSame`
- `xrtHttpBridgeOriginValueValid`
- `xrtHttpBridgePreconditionInputRead`
- `xrtHttpBridgeRangeMultipartLength`
- `xrtHttpBridgeRangeParseValue`
- `xrtHttpBridgeReplyOutputValid`
- `xrtHttpBridgeRequestClearTrailers`
- `xrtHttpBridgeRequestOutputValid`
- `xrtHttpBridgeResponseAddHeader`
- `xrtHttpBridgeResponseAddTrailer`
- `xrtHttpBridgeResponseAddWireBody`
- `xrtHttpBridgeResponseBufferDeliveredBody`
- `xrtHttpBridgeResponseCreate`
- `xrtHttpBridgeResponseDeliverBody`
- `xrtHttpBridgeResponseOutputValid`
- `xrtHttpBridgeResponseSetBodyBytes`
- `xrtHttpBridgeResponseSetBuffered`
- `xrtHttpBridgeResponseSetDecoded`
- `xrtHttpBridgeResponseSetError`
- `xrtHttpBridgeResponseSetFlags`
- `xrtHttpBridgeResponseSetRedirects`
- `xrtHttpBridgeResponseSetUrl`
- `xrtHttpBridgeResponseWrapError`
- `xrtHttpBridgeServerRequestOutputValid`
- `xrtHttpBridgeServerWireBody`
- `xrtHttpBridgeServerWireChunks`
- `xrtHttpBridgeUrlParseValue`
- `xrtHttpBridgeUrlPortValue`

### 常量与宏 (16)

- `XHTTP_FEATURE_HTTP_CACHE_TIME`
- `XHTTP_FEATURE_HTTP_CLIENT_PREPARE`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE`
- `XHTTP_FEATURE_HTTP_ETAG`
- `XHTTP_FEATURE_HTTP_EXCHANGE`
- `XHTTP_FEATURE_HTTP_ORIGIN`
- `XHTTP_FEATURE_HTTP_PRECONDITION`
- `XHTTP_FEATURE_HTTP_RANGE`
- `XHTTP_FEATURE_HTTP_RANGE_MULTIPART`
- `XHTTP_FEATURE_HTTP_SERVER_EXCHANGE`
- `XHTTP_FEATURE_HTTP_SERVER_REPLY`
- `XHTTP_FEATURE_HTTP_SERVER_REQUEST`
- `XHTTP_FEATURE_HTTP_SERVER_RESPONSE`
- `XHTTP_FEATURE_URL`

### 类型 (23)

- `xerrkind`
- `xhttp1exchangeconfig`
- `xhttp1requestoptions`
- `xhttp1requestplan`
- `xhttp1serverconfig`
- `xhttp1serverresponse`
- `xhttpbodyfillproc`
- `xhttpbyterange`
- `xhttpcontentrange`
- `xhttpetag`
- `xhttpheadersconfig`
- `xhttporigin`
- `xhttprangespec`
- `xhttpreply`
- `xhttprepresentation`
- `xhttprequest`
- `xhttprequestfieldproc`
- `xhttpresponse`
- `xhttpresponseerror`
- `xhttpresponsesource`
- `xhttpserverrequest`
- `xhttpversion`
- `xurl`

## `extlibs/xhttp/include/xrt/http_cache.h`

[查看带契约注释的公共头](../../include/xrt/http_cache.h)

### 函数 (10)

- `xrtHttpCacheControlAdd`
- `xrtHttpCacheControlInit`
- `xrtHttpCacheControlParse`
- `xrtHttpCacheControlValid`
- `xrtHttpCacheCursorInit`
- `xrtHttpCacheDeltaParse`
- `xrtHttpCacheDeltaRead`
- `xrtHttpCacheDirectiveName`
- `xrtHttpCacheDirectiveParse`
- `xrtHttpCacheNext`

### 常量与宏 (25)

- `XHTTP_CACHE_CONFLICT`
- `XHTTP_CACHE_DELTA_MAX`
- `XHTTP_CACHE_DUPLICATE`
- `XHTTP_CACHE_EXTENSION`
- `XHTTP_CACHE_FLAG_NONE`
- `XHTTP_CACHE_INVALID`
- `XHTTP_CACHE_MAX_AGE`
- `XHTTP_CACHE_MAX_STALE`
- `XHTTP_CACHE_MAX_STALE_ANY`
- `XHTTP_CACHE_MIN_FRESH`
- `XHTTP_CACHE_MUST_REVALIDATE`
- `XHTTP_CACHE_MUST_UNDERSTAND`
- `XHTTP_CACHE_NO_CACHE`
- `XHTTP_CACHE_NO_CACHE_FIELDS`
- `XHTTP_CACHE_NO_STORE`
- `XHTTP_CACHE_NO_TRANSFORM`
- `XHTTP_CACHE_ONLY_IF_CACHED`
- `XHTTP_CACHE_PRESENT`
- `XHTTP_CACHE_PRIVATE`
- `XHTTP_CACHE_PRIVATE_FIELDS`
- `XHTTP_CACHE_PROXY_REVALIDATE`
- `XHTTP_CACHE_PUBLIC`
- `XHTTP_CACHE_S_MAXAGE`
- `XHTTP_CACHE_UNKNOWN`
- `XHTTP_FEATURE_HTTP_CACHE`

### 类型 (5)

- `xhttpcachecontrol`
- `xhttpcachecursor`
- `xhttpcachedirective`
- `xhttpcacheflag`
- `xhttpcacheitem`

## `extlibs/xhttp/include/xrt/http_cache_status.h`

[查看带契约注释的公共头](../../include/xrt/http_cache_status.h)

### 函数 (6)

- `xrtHttpCacheStatusCursorInit`
- `xrtHttpCacheStatusFieldCursorInit`
- `xrtHttpCacheStatusFieldNext`
- `xrtHttpCacheStatusNext`
- `xrtHttpCacheStatusValid`
- `xrtHttpCacheStatusWrite`

### 常量与宏 (14)

- `XHTTP_CACHE_STATUS_HAS_COLLAPSED`
- `XHTTP_CACHE_STATUS_HAS_DETAIL`
- `XHTTP_CACHE_STATUS_HAS_FORWARD`
- `XHTTP_CACHE_STATUS_HAS_FORWARD_STATUS`
- `XHTTP_CACHE_STATUS_HAS_HIT`
- `XHTTP_CACHE_STATUS_HAS_KEY`
- `XHTTP_CACHE_STATUS_HAS_STORED`
- `XHTTP_CACHE_STATUS_HAS_TTL`
- `XHTTP_CACHE_STATUS_ISSUE_FORWARD_REQUIRED`
- `XHTTP_CACHE_STATUS_ISSUE_HIT_AND_FORWARD`
- `XHTTP_FEATURE_HTTP_CACHE_STATUS`
- `XHTTP_FEATURE_HTTP_CACHE_STATUS_WRITE`
- `XHTTP_FEATURE_HTTP_STRUCTURED`
- `XHTTP_FEATURE_HTTP_STRUCTURED_WRITE`

### 类型 (8)

- `xhttpcachestatus`
- `xhttpcachestatuscursor`
- `xhttpcachestatusfieldcursor`
- `xhttpcachestatusflag`
- `xhttpcachestatusissue`
- `xhttpstructuredbare`
- `xhttpstructuredfieldcursor`
- `xhttpstructureditemvalue`

## `extlibs/xhttp/include/xrt/http_cache_time.h`

[查看带契约注释的公共头](../../include/xrt/http_cache_time.h)

### 函数 (8)

- `xrtHttpCacheAgeValid`
- `xrtHttpCacheCurrentAge`
- `xrtHttpCacheFresh`
- `xrtHttpCacheFreshness`
- `xrtHttpCacheFreshnessValid`
- `xrtHttpCacheTimeInit`
- `xrtHttpCacheTimeParse`
- `xrtHttpCacheTimeValid`

### 常量与宏 (20)

- `XHTTP_CACHE_CALC_ERROR`
- `XHTTP_CACHE_CALC_INVALID`
- `XHTTP_CACHE_CALC_NONE`
- `XHTTP_CACHE_CALC_READY`
- `XHTTP_CACHE_FRESHNESS_EXPIRES`
- `XHTTP_CACHE_FRESHNESS_EXTENSION`
- `XHTTP_CACHE_FRESHNESS_HEURISTIC`
- `XHTTP_CACHE_FRESHNESS_MAX_AGE`
- `XHTTP_CACHE_FRESHNESS_NONE`
- `XHTTP_CACHE_FRESHNESS_S_MAXAGE`
- `XHTTP_CACHE_TIME_AGE`
- `XHTTP_CACHE_TIME_AGE_EXTRA`
- `XHTTP_CACHE_TIME_AGE_INVALID`
- `XHTTP_CACHE_TIME_DATE`
- `XHTTP_CACHE_TIME_DATE_DUPLICATE`
- `XHTTP_CACHE_TIME_DATE_INVALID`
- `XHTTP_CACHE_TIME_EXPIRES`
- `XHTTP_CACHE_TIME_EXPIRES_DUPLICATE`
- `XHTTP_CACHE_TIME_EXPIRES_INVALID`
- `XHTTP_CACHE_TIME_NONE`

### 类型 (6)

- `xhttpcacheage`
- `xhttpcachecalc`
- `xhttpcachefreshness`
- `xhttpcachefreshnesssource`
- `xhttpcachetime`
- `xhttpcachetimeflag`

## `extlibs/xhttp/include/xrt/http_client.h`

[查看带契约注释的公共头](../../include/xrt/http_client.h)

### 函数 (95)

- `xrtHttp1RequestAcceptTrailers`
- `xrtHttp1RequestOptionsInit`
- `xrtHttp1RequestPlanBody`
- `xrtHttp1RequestPlanBodyLength`
- `xrtHttp1RequestPlanBodyMode`
- `xrtHttp1RequestPlanClose`
- `xrtHttp1RequestPlanDestroy`
- `xrtHttp1RequestPlanEnd`
- `xrtHttp1RequestPlanExpectContinue`
- `xrtHttp1RequestPlanHead`
- `xrtHttp1RequestPlanHost`
- `xrtHttp1RequestPlanMethod`
- `xrtHttp1RequestPlanPort`
- `xrtHttp1RequestPlanSecure`
- `xrtHttp1RequestPlanTarget`
- `xrtHttp1RequestPlanUrl`
- `xrtHttp1RequestPrepare`
- `xrtHttpRequestAddHeader`
- `xrtHttpRequestAddTrailer`
- `xrtHttpRequestBody`
- `xrtHttpRequestClearAuth`
- `xrtHttpRequestClearProxyAuth`
- `xrtHttpRequestClone`
- `xrtHttpRequestCreate`
- `xrtHttpRequestCreateWithHeaders`
- `xrtHttpRequestDestroy`
- `xrtHttpRequestEditTrailers`
- `xrtHttpRequestHeader`
- `xrtHttpRequestHeaderAt`
- `xrtHttpRequestHeaderCount`
- `xrtHttpRequestHeaderData`
- `xrtHttpRequestHeaders`
- `xrtHttpRequestMethod`
- `xrtHttpRequestRemoveHeader`
- `xrtHttpRequestRemoveTrailer`
- `xrtHttpRequestSetAuth`
- `xrtHttpRequestSetBasicAuth`
- `xrtHttpRequestSetBearerAuth`
- `xrtHttpRequestSetBody`
- `xrtHttpRequestSetBytes`
- `xrtHttpRequestSetDigestAuth`
- `xrtHttpRequestSetForm`
- `xrtHttpRequestSetFormData`
- `xrtHttpRequestSetFormDataRandom`
- `xrtHttpRequestSetHeader`
- `xrtHttpRequestSetMethod`
- `xrtHttpRequestSetProxyAuth`
- `xrtHttpRequestSetProxyBasicAuth`
- `xrtHttpRequestSetProxyBearerAuth`
- `xrtHttpRequestSetProxyDigestAuth`
- `xrtHttpRequestSetQueryParams`
- `xrtHttpRequestSetTrailer`
- `xrtHttpRequestSetUrl`
- `xrtHttpRequestTrailer`
- `xrtHttpRequestTrailerAt`
- `xrtHttpRequestTrailerCount`
- `xrtHttpRequestTrailerData`
- `xrtHttpRequestTrailers`
- `xrtHttpRequestUrl`
- `xrtHttpRequestUrlText`
- `xrtHttpResponseBasicChallengeNext`
- `xrtHttpResponseBearerChallengeNext`
- `xrtHttpResponseBody`
- `xrtHttpResponseBodyBytes`
- `xrtHttpResponseBodyText`
- `xrtHttpResponseChallengeNext`
- `xrtHttpResponseContentType`
- `xrtHttpResponseDestroy`
- `xrtHttpResponseDigestChallengeNext`
- `xrtHttpResponseDigestInfo`
- `xrtHttpResponseFlags`
- `xrtHttpResponseHeader`
- `xrtHttpResponseHeaderAt`
- `xrtHttpResponseHeaderCount`
- `xrtHttpResponseHeaderData`
- `xrtHttpResponseHeaders`
- `xrtHttpResponseOriginalEncoding`
- `xrtHttpResponseProxyBasicChallengeNext`
- `xrtHttpResponseProxyBearerChallengeNext`
- `xrtHttpResponseProxyChallengeNext`
- `xrtHttpResponseProxyDigestChallengeNext`
- `xrtHttpResponseProxyDigestInfo`
- `xrtHttpResponseReason`
- `xrtHttpResponseRedirects`
- `xrtHttpResponseSetCookieNext`
- `xrtHttpResponseStatus`
- `xrtHttpResponseSuccess`
- `xrtHttpResponseTrailer`
- `xrtHttpResponseTrailerAt`
- `xrtHttpResponseTrailerCount`
- `xrtHttpResponseTrailerData`
- `xrtHttpResponseTrailers`
- `xrtHttpResponseUrl`
- `xrtHttpResponseVersion`
- `xrtHttpResponseWireBodyBytes`

### 常量与宏 (49)

- `XHTTP_FEATURE_HTTP_CLIENT_CONTENT_TYPE`
- `XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_BASIC`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_BEARER`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_DIGEST`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA_RANDOM`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_QUERY`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TE`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_BASIC`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_BEARER`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_INFO`
- `XHTTP_FEATURE_HTTP_CLIENT_SET_COOKIE`
- `XHTTP_FEATURE_HTTP_HEADERS`
- `XHTTP_FEATURE_QUERY_PARAMS`
- `XHTTP_REQUEST_BODY_CHUNKED`
- `XHTTP_REQUEST_BODY_FIXED`
- `XHTTP_REQUEST_BODY_NONE`
- `XHTTP_REQUEST_ERROR_CONNECTION`
- `XHTTP_REQUEST_ERROR_CONTENT_LENGTH`
- `XHTTP_REQUEST_ERROR_EXPECT`
- `XHTTP_REQUEST_ERROR_FORM`
- `XHTTP_REQUEST_ERROR_FORM_DATA`
- `XHTTP_REQUEST_ERROR_HOST`
- `XHTTP_REQUEST_ERROR_HOST_HEADER`
- `XHTTP_REQUEST_ERROR_METHOD`
- `XHTTP_REQUEST_ERROR_QUERY`
- `XHTTP_REQUEST_ERROR_SCHEME`
- `XHTTP_REQUEST_ERROR_TARGET`
- `XHTTP_REQUEST_ERROR_TE`
- `XHTTP_REQUEST_ERROR_TRACE_BODY`
- `XHTTP_REQUEST_ERROR_TRAILER`
- `XHTTP_REQUEST_ERROR_TRANSFER_ENCODING`
- `XHTTP_REQUEST_ERROR_URL`
- `XHTTP_REQUEST_ERROR_USERINFO`
- `XHTTP_RESPONSE_DECOMPRESSED`
- `XHTTP_RESPONSE_ERROR_ARGUMENT`
- `XHTTP_RESPONSE_ERROR_AUTH`
- `XHTTP_RESPONSE_ERROR_CONTENT_TYPE`
- `XHTTP_RESPONSE_ERROR_HEADER`
- `XHTTP_RESPONSE_ERROR_INDEX`
- `XHTTP_RESPONSE_ERROR_SET_COOKIE`
- `XHTTP_RESPONSE_NONE`
- `XHTTP_RESPONSE_STREAMED`
- `XHTTP_RESPONSE_UPGRADED`

### 类型 (6)

- `xhttp1targetform`
- `xhttpheaders`
- `xhttprequestbodymode`
- `xhttprequesterror`
- `xhttpresponseflag`
- `xqueryparams`

## `extlibs/xhttp/include/xrt/http_content_disposition.h`

[查看带契约注释的公共头](../../include/xrt/http_content_disposition.h)

### 函数 (6)

- `xrtHttpContentDispositionBuild`
- `xrtHttpContentDispositionFileNameBuild`
- `xrtHttpContentDispositionFileNameWrite`
- `xrtHttpContentDispositionParam`
- `xrtHttpContentDispositionParse`
- `xrtHttpContentDispositionWrite`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_CONTENT_DISPOSITION`

### 类型 (2)

- `xcontentdisposition`
- `xcontentdispositionflags`

## `extlibs/xhttp/include/xrt/http_digest.h`

[查看带契约注释的公共头](../../include/xrt/http_digest.h)

### 函数 (13)

- `xrtHttpDigestCursorInit`
- `xrtHttpDigestFieldNext`
- `xrtHttpDigestNext`
- `xrtHttpDigestPreferenceFieldNext`
- `xrtHttpDigestPreferenceNext`
- `xrtHttpDigestPreferenceValid`
- `xrtHttpDigestPreferenceWrite`
- `xrtHttpDigestRead`
- `xrtHttpDigestSha256Write`
- `xrtHttpDigestSha2Verify`
- `xrtHttpDigestSha512Write`
- `xrtHttpDigestValid`
- `xrtHttpDigestWrite`

### 常量与宏 (9)

- `XHTTP_DIGEST_CONTENT`
- `XHTTP_DIGEST_MATCH_ERROR`
- `XHTTP_DIGEST_MATCH_MISMATCH`
- `XHTTP_DIGEST_MATCH_OK`
- `XHTTP_DIGEST_MATCH_UNSUPPORTED`
- `XHTTP_DIGEST_REPRESENTATION`
- `XHTTP_FEATURE_HTTP_DIGEST`
- `XHTTP_FEATURE_HTTP_DIGEST_SHA2`
- `XHTTP_FEATURE_HTTP_DIGEST_WRITE`

### 类型 (6)

- `xhttpdigest`
- `xhttpdigestcursor`
- `xhttpdigestmatch`
- `xhttpdigestpreference`
- `xhttpdigesttarget`
- `xhttpstructuredmapcursor`

## `extlibs/xhttp/include/xrt/http_exchange.h`

[查看带契约注释的公共头](../../include/xrt/http_exchange.h)

### 函数 (20)

- `xrtHttp1ExchangeConfigInit`
- `xrtHttp1ExchangeContinue`
- `xrtHttp1ExchangeCreate`
- `xrtHttp1ExchangeDestroy`
- `xrtHttp1ExchangeError`
- `xrtHttp1ExchangeFeed`
- `xrtHttp1ExchangeInformationalCount`
- `xrtHttp1ExchangeOutput`
- `xrtHttp1ExchangeOutputConsume`
- `xrtHttp1ExchangePause`
- `xrtHttp1ExchangePaused`
- `xrtHttp1ExchangeRemainder`
- `xrtHttp1ExchangeRequestComplete`
- `xrtHttp1ExchangeRequestWireBytes`
- `xrtHttp1ExchangeResponse`
- `xrtHttp1ExchangeResponseComplete`
- `xrtHttp1ExchangeResume`
- `xrtHttp1ExchangeReusable`
- `xrtHttp1ExchangeTakeResponse`
- `xrtHttp1ExchangeUpgraded`

### 类型 (10)

- `xhttp1bodylimits`
- `xhttp1bodyproc`
- `xhttp1exchange`
- `xhttp1exchangeerror`
- `xhttp1exchangeevents`
- `xhttp1feedstatus`
- `xhttp1headersproc`
- `xhttp1informationalproc`
- `xhttp1limits`
- `xhttp1outputstatus`

## `extlibs/xhttp/include/xrt/http_exchange_async.h`

[查看带契约注释的公共头](../../include/xrt/http_exchange_async.h)

### 函数 (1)

- `xrtHttp1ExchangeOutputWait`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_EXCHANGE_ASYNC`

## `extlibs/xhttp/include/xrt/http_ext_value.h`

[查看带契约注释的公共头](../../include/xrt/http_ext_value.h)

### 函数 (4)

- `xrtHttpExtValueBuild`
- `xrtHttpExtValueParse`
- `xrtHttpExtValueRead`
- `xrtHttpExtValueWrite`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_LANGUAGE_CORE`

### 类型 (1)

- `xhttpextvalue`

## `extlibs/xhttp/include/xrt/http_forward.h`

[查看带契约注释的公共头](../../include/xrt/http_forward.h)

### 函数 (5)

- `xrtHttpHopField`
- `xrtHttpHopFieldKnown`
- `xrtHttpMaxForwardsParse`
- `xrtHttpMaxForwardsUpdate`
- `xrtHttpMaxForwardsWrite`

### 常量与宏 (4)

- `XHTTP_FEATURE_HTTP_FORWARD`
- `XHTTP_FORWARD_ERROR`
- `XHTTP_FORWARD_FINAL`
- `XHTTP_FORWARD_NEXT`

### 类型 (1)

- `xhttpforwardstatus`

## `extlibs/xhttp/include/xrt/http_forwarded.h`

[查看带契约注释的公共头](../../include/xrt/http_forwarded.h)

### 函数 (15)

- `xrtHttpForwardedBuild`
- `xrtHttpForwardedCount`
- `xrtHttpForwardedCursorInit`
- `xrtHttpForwardedElementParse`
- `xrtHttpForwardedElementWrite`
- `xrtHttpForwardedFieldCount`
- `xrtHttpForwardedFieldCursorInit`
- `xrtHttpForwardedFieldNext`
- `xrtHttpForwardedHostValid`
- `xrtHttpForwardedNext`
- `xrtHttpForwardedNodeValid`
- `xrtHttpForwardedPairNext`
- `xrtHttpForwardedProtoValid`
- `xrtHttpForwardedValid`
- `xrtHttpForwardedWrite`

### 常量与宏 (6)

- `XHTTP_FEATURE_HTTP_FORWARDED`
- `XHTTP_FEATURE_HTTP_FORWARDED_WRITE`
- `XHTTP_FORWARDED_HAS_BY`
- `XHTTP_FORWARDED_HAS_FOR`
- `XHTTP_FORWARDED_HAS_HOST`
- `XHTTP_FORWARDED_HAS_PROTO`

### 类型 (6)

- `xhttpforwarded`
- `xhttpforwardedcursor`
- `xhttpforwardedfieldcursor`
- `xhttpforwardedflags`
- `xhttpforwardedpairvalue`
- `xhttpforwardedvalue`

## `extlibs/xhttp/include/xrt/http_headers.h`

[查看带契约注释的公共头](../../include/xrt/http_headers.h)

### 函数 (26)

- `xrtHttpHeadersAdd`
- `xrtHttpHeadersAddBlock`
- `xrtHttpHeadersAt`
- `xrtHttpHeadersBuild`
- `xrtHttpHeadersBytes`
- `xrtHttpHeadersClear`
- `xrtHttpHeadersClone`
- `xrtHttpHeadersCompact`
- `xrtHttpHeadersConfigInit`
- `xrtHttpHeadersConfigValid`
- `xrtHttpHeadersCount`
- `xrtHttpHeadersCountName`
- `xrtHttpHeadersCreate`
- `xrtHttpHeadersData`
- `xrtHttpHeadersDestroy`
- `xrtHttpHeadersGet`
- `xrtHttpHeadersGetAll`
- `xrtHttpHeadersGetNth`
- `xrtHttpHeadersGetUnique`
- `xrtHttpHeadersHas`
- `xrtHttpHeadersParse`
- `xrtHttpHeadersRemove`
- `xrtHttpHeadersReserve`
- `xrtHttpHeadersSet`
- `xrtHttpHeadersSwap`
- `xrtHttpHeadersWrite`

## `extlibs/xhttp/include/xrt/http_language.h`

[查看带契约注释的公共头](../../include/xrt/http_language.h)

### 函数 (10)

- `xrtHttpAcceptLanguageLookup`
- `xrtHttpAcceptLanguageMatch`
- `xrtHttpAcceptLanguageNext`
- `xrtHttpAcceptLanguageQuality`
- `xrtHttpAcceptLanguageSelect`
- `xrtHttpLanguageBasicMatch`
- `xrtHttpLanguageCursorInit`
- `xrtHttpLanguageRangeNext`
- `xrtHttpLanguageRangeValid`
- `xrtHttpLanguageTagValid`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_LANGUAGE`

### 类型 (3)

- `xhttplanguagecursor`
- `xhttplanguagematch`
- `xhttplanguagerange`

## `extlibs/xhttp/include/xrt/http_link.h`

[查看带契约注释的公共头](../../include/xrt/http_link.h)

### 函数 (17)

- `xrtHttpLinkAnchorBuild`
- `xrtHttpLinkAnchorWrite`
- `xrtHttpLinkBuild`
- `xrtHttpLinkCursorInit`
- `xrtHttpLinkElementParse`
- `xrtHttpLinkElementWrite`
- `xrtHttpLinkFieldCursorInit`
- `xrtHttpLinkFieldNext`
- `xrtHttpLinkNext`
- `xrtHttpLinkParam`
- `xrtHttpLinkRelationFind`
- `xrtHttpLinkTargetResolve`
- `xrtHttpLinkTargetResolveBuild`
- `xrtHttpLinkTitleBuild`
- `xrtHttpLinkTitleWrite`
- `xrtHttpLinkValid`
- `xrtHttpLinkWrite`

### 常量与宏 (9)

- `XHTTP_FEATURE_HTTP_LINK`
- `XHTTP_FEATURE_HTTP_LINK_WRITE`
- `XHTTP_LINK_HAS_ANCHOR`
- `XHTTP_LINK_HAS_MEDIA`
- `XHTTP_LINK_HAS_REL`
- `XHTTP_LINK_HAS_REV`
- `XHTTP_LINK_HAS_TITLE`
- `XHTTP_LINK_HAS_TITLE_EXT`
- `XHTTP_LINK_HAS_TYPE`

### 类型 (6)

- `xhttplink`
- `xhttplinkcursor`
- `xhttplinkfieldcursor`
- `xhttplinkflags`
- `xhttplinkparamvalue`
- `xhttplinkvalue`

## `extlibs/xhttp/include/xrt/http_origin.h`

[查看带契约注释的公共头](../../include/xrt/http_origin.h)

### 函数 (11)

- `xrtHttpOriginBuild`
- `xrtHttpOriginCursorInit`
- `xrtHttpOriginFields`
- `xrtHttpOriginFromUrl`
- `xrtHttpOriginListWrite`
- `xrtHttpOriginNext`
- `xrtHttpOriginNull`
- `xrtHttpOriginParse`
- `xrtHttpOriginSame`
- `xrtHttpOriginValid`
- `xrtHttpOriginWrite`

### 常量与宏 (2)

- `XHTTP_FEATURE_HTTP_ORIGIN_WRITE`
- `XHTTP_ORIGIN_NULL`

### 类型 (1)

- `xhttporigincursor`

## `extlibs/xhttp/include/xrt/http_priority.h`

[查看带契约注释的公共头](../../include/xrt/http_priority.h)

### 函数 (5)

- `xrtHttpPriorityInit`
- `xrtHttpPriorityOverlay`
- `xrtHttpPriorityParse`
- `xrtHttpPriorityValueParse`
- `xrtHttpPriorityWrite`

### 常量与宏 (6)

- `XHTTP_FEATURE_HTTP_PRIORITY`
- `XHTTP_FEATURE_HTTP_PRIORITY_WRITE`
- `XHTTP_PRIORITY_HAS_INCREMENTAL`
- `XHTTP_PRIORITY_HAS_URGENCY`
- `XHTTP_PRIORITY_URGENCY_DEFAULT`
- `XHTTP_PRIORITY_URGENCY_MAX`

### 类型 (2)

- `xhttppriority`
- `xhttppriorityflag`

## `extlibs/xhttp/include/xrt/http_proxy_status.h`

[查看带契约注释的公共头](../../include/xrt/http_proxy_status.h)

### 函数 (13)

- `xrtHttpProxyAliasCursorInit`
- `xrtHttpProxyAliasNext`
- `xrtHttpProxyAliasRead`
- `xrtHttpProxyAliasWrite`
- `xrtHttpProxyAliasesBuild`
- `xrtHttpProxyAliasesValid`
- `xrtHttpProxyAliasesWrite`
- `xrtHttpProxyStatusCursorInit`
- `xrtHttpProxyStatusFieldCursorInit`
- `xrtHttpProxyStatusFieldNext`
- `xrtHttpProxyStatusNext`
- `xrtHttpProxyStatusValid`
- `xrtHttpProxyStatusWrite`

### 常量与宏 (11)

- `XHTTP_FEATURE_HTTP_PROXY_ALIAS`
- `XHTTP_FEATURE_HTTP_PROXY_ALIAS_WRITE`
- `XHTTP_FEATURE_HTTP_PROXY_STATUS`
- `XHTTP_FEATURE_HTTP_PROXY_STATUS_WRITE`
- `XHTTP_PROXY_ALPN_MAX`
- `XHTTP_PROXY_STATUS_HAS_DETAILS`
- `XHTTP_PROXY_STATUS_HAS_ERROR`
- `XHTTP_PROXY_STATUS_HAS_NEXT_HOP`
- `XHTTP_PROXY_STATUS_HAS_NEXT_HOP_ALIASES`
- `XHTTP_PROXY_STATUS_HAS_NEXT_PROTOCOL`
- `XHTTP_PROXY_STATUS_HAS_RECEIVED_STATUS`

### 类型 (5)

- `xhttpproxyaliascursor`
- `xhttpproxystatus`
- `xhttpproxystatuscursor`
- `xhttpproxystatusfieldcursor`
- `xhttpproxystatusflag`

## `extlibs/xhttp/include/xrt/http_retry.h`

[查看带契约注释的公共头](../../include/xrt/http_retry.h)

### 函数 (5)

- `xrtHttpRetryAfterBuild`
- `xrtHttpRetryAfterDelay`
- `xrtHttpRetryAfterFields`
- `xrtHttpRetryAfterParse`
- `xrtHttpRetryAfterWrite`

### 常量与宏 (4)

- `XHTTP_FEATURE_HTTP_RETRY`
- `XHTTP_RETRY_AFTER_DATE`
- `XHTTP_RETRY_AFTER_DELAY`
- `XHTTP_RETRY_AFTER_NONE`

### 类型 (2)

- `xhttpretryafter`
- `xhttpretryafterkind`

## `extlibs/xhttp/include/xrt/http_semantics.h`

[查看带契约注释的公共头](../../include/xrt/http_semantics.h)

### 函数 (24)

- `xrtHttpByteRangeCount`
- `xrtHttpByteRangeNext`
- `xrtHttpByteRangeResolve`
- `xrtHttpByteRangesResolve`
- `xrtHttpContentRangeBuild`
- `xrtHttpContentRangeParse`
- `xrtHttpContentRangeWrite`
- `xrtHttpETagBuild`
- `xrtHttpETagListStrongHas`
- `xrtHttpETagListWeakHas`
- `xrtHttpETagNext`
- `xrtHttpETagParse`
- `xrtHttpETagStrongEqual`
- `xrtHttpETagWeakEqual`
- `xrtHttpETagWrite`
- `xrtHttpIfRangeMatch`
- `xrtHttpPreconditionsEvaluate`
- `xrtHttpRangeBuild`
- `xrtHttpRangeMultipartCloseWrite`
- `xrtHttpRangeMultipartEndWrite`
- `xrtHttpRangeMultipartHeadWrite`
- `xrtHttpRangeMultipartLength`
- `xrtHttpRangeParse`
- `xrtHttpRangeWrite`

### 常量与宏 (13)

- `XHTTP_ETAG_ANY`
- `XHTTP_ETAG_VALUE`
- `XHTTP_PRECONDITION_ERROR`
- `XHTTP_PRECONDITION_FAILED`
- `XHTTP_PRECONDITION_NOT_MODIFIED`
- `XHTTP_PRECONDITION_PROCEED`
- `XHTTP_RANGE_EMPTY`
- `XHTTP_RANGE_ERROR`
- `XHTTP_RANGE_SATISFIED`
- `XHTTP_RANGE_SPEC_CLOSED`
- `XHTTP_RANGE_SPEC_OPEN`
- `XHTTP_RANGE_SPEC_SUFFIX`
- `XHTTP_RANGE_UNSATISFIED`

### 类型 (5)

- `xhttpetagitem`
- `xhttpetagkind`
- `xhttpprecondition`
- `xhttprangeresult`
- `xhttprangespecform`

## `extlibs/xhttp/include/xrt/http_server.h`

[查看带契约注释的公共头](../../include/xrt/http_server.h)

### 函数 (75)

- `xrtHttpReplyAddBasicChallenge`
- `xrtHttpReplyAddBearerChallenge`
- `xrtHttpReplyAddChallenge`
- `xrtHttpReplyAddDigestChallenge`
- `xrtHttpReplyAddHeader`
- `xrtHttpReplyAddProxyBasicChallenge`
- `xrtHttpReplyAddProxyBearerChallenge`
- `xrtHttpReplyAddProxyChallenge`
- `xrtHttpReplyAddProxyDigestChallenge`
- `xrtHttpReplyAddTrailer`
- `xrtHttpReplyBody`
- `xrtHttpReplyClone`
- `xrtHttpReplyConfigInit`
- `xrtHttpReplyCreate`
- `xrtHttpReplyCreateWithConfig`
- `xrtHttpReplyDestroy`
- `xrtHttpReplyEditHeaders`
- `xrtHttpReplyEditTrailers`
- `xrtHttpReplyHeader`
- `xrtHttpReplyHeaderAt`
- `xrtHttpReplyHeaderCount`
- `xrtHttpReplyHeaderData`
- `xrtHttpReplyHeaders`
- `xrtHttpReplyReason`
- `xrtHttpReplyRemoveHeader`
- `xrtHttpReplyRemoveTrailer`
- `xrtHttpReplySetBody`
- `xrtHttpReplySetBytes`
- `xrtHttpReplySetDigestInfo`
- `xrtHttpReplySetHeader`
- `xrtHttpReplySetProxyDigestInfo`
- `xrtHttpReplySetReason`
- `xrtHttpReplySetStatus`
- `xrtHttpReplySetTrailer`
- `xrtHttpReplyStatus`
- `xrtHttpReplyTrailer`
- `xrtHttpReplyTrailerAt`
- `xrtHttpReplyTrailerCount`
- `xrtHttpReplyTrailerData`
- `xrtHttpReplyTrailers`
- `xrtHttpServerRequestAcceptsTrailers`
- `xrtHttpServerRequestAuth`
- `xrtHttpServerRequestAuthority`
- `xrtHttpServerRequestBasicAuth`
- `xrtHttpServerRequestBearerAuth`
- `xrtHttpServerRequestBody`
- `xrtHttpServerRequestBodyBytes`
- `xrtHttpServerRequestBodyMode`
- `xrtHttpServerRequestContentLength`
- `xrtHttpServerRequestContentType`
- `xrtHttpServerRequestCookie`
- `xrtHttpServerRequestCookies`
- `xrtHttpServerRequestDestroy`
- `xrtHttpServerRequestDigestAuth`
- `xrtHttpServerRequestFlags`
- `xrtHttpServerRequestForm`
- `xrtHttpServerRequestFormData`
- `xrtHttpServerRequestHeader`
- `xrtHttpServerRequestHeaderAt`
- `xrtHttpServerRequestHeaderCount`
- `xrtHttpServerRequestHeaderData`
- `xrtHttpServerRequestMethod`
- `xrtHttpServerRequestParseTarget`
- `xrtHttpServerRequestProxyAuth`
- `xrtHttpServerRequestProxyBasicAuth`
- `xrtHttpServerRequestProxyBearerAuth`
- `xrtHttpServerRequestProxyDigestAuth`
- `xrtHttpServerRequestQueryParams`
- `xrtHttpServerRequestRef`
- `xrtHttpServerRequestTarget`
- `xrtHttpServerRequestTrailer`
- `xrtHttpServerRequestTrailerAt`
- `xrtHttpServerRequestTrailerCount`
- `xrtHttpServerRequestTrailerData`
- `xrtHttpServerRequestVersion`

### 常量与宏 (31)

- `XHTTP_FEATURE_HTTP_SERVER_CONTENT_TYPE`
- `XHTTP_FEATURE_HTTP_SERVER_COOKIE`
- `XHTTP_FEATURE_HTTP_SERVER_FORM`
- `XHTTP_FEATURE_HTTP_SERVER_FORM_DATA`
- `XHTTP_FEATURE_HTTP_SERVER_QUERY`
- `XHTTP_FEATURE_HTTP_SERVER_REPLY_AUTH`
- `XHTTP_FEATURE_HTTP_SERVER_REPLY_AUTH_BASIC`
- `XHTTP_FEATURE_HTTP_SERVER_REPLY_AUTH_BEARER`
- `XHTTP_FEATURE_HTTP_SERVER_REPLY_AUTH_DIGEST`
- `XHTTP_FEATURE_HTTP_SERVER_REPLY_AUTH_DIGEST_INFO`
- `XHTTP_FEATURE_HTTP_SERVER_REQUEST_AUTH`
- `XHTTP_FEATURE_HTTP_SERVER_REQUEST_AUTH_BASIC`
- `XHTTP_FEATURE_HTTP_SERVER_REQUEST_AUTH_BEARER`
- `XHTTP_FEATURE_HTTP_SERVER_REQUEST_AUTH_DIGEST`
- `XHTTP_SERVER_REQUEST_ACCEPTS_TRAILERS`
- `XHTTP_SERVER_REQUEST_COMPLETE`
- `XHTTP_SERVER_REQUEST_DISCARDED`
- `XHTTP_SERVER_REQUEST_ERROR_ARGUMENT`
- `XHTTP_SERVER_REQUEST_ERROR_AUTH`
- `XHTTP_SERVER_REQUEST_ERROR_BODY`
- `XHTTP_SERVER_REQUEST_ERROR_CONTENT_TYPE`
- `XHTTP_SERVER_REQUEST_ERROR_FORM`
- `XHTTP_SERVER_REQUEST_ERROR_HEADER`
- `XHTTP_SERVER_REQUEST_ERROR_QUERY`
- `XHTTP_SERVER_REQUEST_ERROR_STATE`
- `XHTTP_SERVER_REQUEST_ERROR_TARGET`
- `XHTTP_SERVER_REQUEST_EXPECT_CONTINUE`
- `XHTTP_SERVER_REQUEST_KEEP_ALIVE`
- `XHTTP_SERVER_REQUEST_NONE`
- `XHTTP_SERVER_REQUEST_STREAMED`
- `XHTTP_SERVER_REQUEST_UPGRADE`

### 类型 (7)

- `xhttp1bodymode`
- `xhttpauthority`
- `xhttpreplyconfig`
- `xhttpserverrequesterror`
- `xhttpserverrequestflag`
- `xhttptarget`
- `xqueryparamsconfig`

## `extlibs/xhttp/include/xrt/http_server_exchange.h`

[查看带契约注释的公共头](../../include/xrt/http_server_exchange.h)

### 函数 (25)

- `xrtHttp1ServerConfigInit`
- `xrtHttp1ServerExchangeComplete`
- `xrtHttp1ServerExchangeCreate`
- `xrtHttp1ServerExchangeDestroy`
- `xrtHttp1ServerExchangeError`
- `xrtHttp1ServerExchangeFeed`
- `xrtHttp1ServerExchangeNext`
- `xrtHttp1ServerExchangePause`
- `xrtHttp1ServerExchangePaused`
- `xrtHttp1ServerExchangeRequest`
- `xrtHttp1ServerExchangeResume`
- `xrtHttp1ServerExchangeSetBodyLimit`
- `xrtHttp1ServerExchangeWireBytes`
- `xrtHttp1ServerResponseClose`
- `xrtHttp1ServerResponseComplete`
- `xrtHttp1ServerResponseCreate`
- `xrtHttp1ServerResponseDestroy`
- `xrtHttp1ServerResponseError`
- `xrtHttp1ServerResponseInform`
- `xrtHttp1ServerResponseInformational`
- `xrtHttp1ServerResponseOutput`
- `xrtHttp1ServerResponseOutputConsume`
- `xrtHttp1ServerResponsePrepare`
- `xrtHttp1ServerResponseTunnel`
- `xrtHttp1ServerResponseWireBytes`

### 常量与宏 (4)

- `XHTTP_SERVER_BODY_BUFFER`
- `XHTTP_SERVER_BODY_DISCARD`
- `XHTTP_SERVER_BODY_REJECT`
- `XHTTP_SERVER_BODY_STREAM`

### 类型 (10)

- `xhttp1servererror`
- `xhttp1serverevents`
- `xhttp1serverexchange`
- `xhttp1serverfeedstatus`
- `xhttp1serveroutputstatus`
- `xhttp1serverresponseerror`
- `xhttpserverbodypolicy`
- `xhttpserverbodyproc`
- `xhttpservercompleteproc`
- `xhttpserverheadersproc`

## `extlibs/xhttp/include/xrt/http_server_response_async.h`

[查看带契约注释的公共头](../../include/xrt/http_server_response_async.h)

### 函数 (1)

- `xrtHttp1ServerResponseWait`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_SERVER_RESPONSE_ASYNC`

## `extlibs/xhttp/include/xrt/http_sse.h`

[查看带契约注释的公共头](../../include/xrt/http_sse.h)

### 函数 (24)

- `xrtHttpSseCommentBuild`
- `xrtHttpSseCommentSize`
- `xrtHttpSseCommentWrite`
- `xrtHttpSseContentTypeValid`
- `xrtHttpSseEventBuild`
- `xrtHttpSseEventSize`
- `xrtHttpSseEventValid`
- `xrtHttpSseEventWrite`
- `xrtHttpSseLastEventIdValid`
- `xrtHttpSseParserConfigInit`
- `xrtHttpSseParserConfigValid`
- `xrtHttpSseParserCreate`
- `xrtHttpSseParserDestroy`
- `xrtHttpSseParserInit`
- `xrtHttpSseParserLastEventId`
- `xrtHttpSseParserRead`
- `xrtHttpSseParserReconnect`
- `xrtHttpSseParserReset`
- `xrtHttpSseParserRetry`
- `xrtHttpSseParserTrim`
- `xrtHttpSseParserUnit`
- `xrtHttpSseRequestHeaders`
- `xrtHttpSseResponseCheck`
- `xrtHttpSseResponseHeaders`

### 常量与宏 (29)

- `XHTTP_FEATURE_HTTP_SSE`
- `XHTTP_FEATURE_HTTP_SSE_HTTP`
- `XHTTP_FEATURE_HTTP_SSE_PARSER`
- `XHTTP_SSE_ERROR_ALLOCATION`
- `XHTTP_SSE_ERROR_ARGUMENT`
- `XHTTP_SSE_ERROR_DATA_TOO_LARGE`
- `XHTTP_SSE_ERROR_ID_TOO_LARGE`
- `XHTTP_SSE_ERROR_LINE_TOO_LARGE`
- `XHTTP_SSE_ERROR_STATE`
- `XHTTP_SSE_ERROR_TYPE_TOO_LARGE`
- `XHTTP_SSE_ERROR_UTF8`
- `XHTTP_SSE_EVENT_DATA`
- `XHTTP_SSE_EVENT_ID`
- `XHTTP_SSE_EVENT_NONE`
- `XHTTP_SSE_EVENT_RETRY`
- `XHTTP_SSE_EVENT_TYPE`
- `XHTTP_SSE_ITEM_COMMENT`
- `XHTTP_SSE_ITEM_EVENT`
- `XHTTP_SSE_ITEM_RETRY`
- `XHTTP_SSE_MEDIA_TYPE`
- `XHTTP_SSE_PARSE_DONE`
- `XHTTP_SSE_PARSE_ERROR`
- `XHTTP_SSE_PARSE_ITEM`
- `XHTTP_SSE_PARSE_MORE`
- `XHTTP_SSE_RESPONSE_ERROR`
- `XHTTP_SSE_RESPONSE_OPEN`
- `XHTTP_SSE_RESPONSE_REJECT`
- `XHTTP_SSE_RESPONSE_STOP`
- `XHTTP_SSE_RETRY_DEFAULT`

### 类型 (13)

- `xbuffer`
- `xhttpsseerror`
- `xhttpsseerrorinfo`
- `xhttpsseevent`
- `xhttpsseeventflag`
- `xhttpsseitem`
- `xhttpsseitemkind`
- `xhttpssemessage`
- `xhttpsseparser`
- `xhttpsseparserconfig`
- `xhttpsseparsestatus`
- `xhttpsseresponse`
- `xutfpolicy`

## `extlibs/xhttp/include/xrt/http_structured.h`

[查看带契约注释的公共头](../../include/xrt/http_structured.h)

### 函数 (34)

- `xrtHttpStructuredBareNext`
- `xrtHttpStructuredBareWrite`
- `xrtHttpStructuredBytesDecode`
- `xrtHttpStructuredDictionaryAt`
- `xrtHttpStructuredDictionaryCount`
- `xrtHttpStructuredDictionaryFieldAt`
- `xrtHttpStructuredDictionaryFieldCount`
- `xrtHttpStructuredDictionaryFieldFind`
- `xrtHttpStructuredDictionaryFieldNext`
- `xrtHttpStructuredDictionaryFind`
- `xrtHttpStructuredDictionaryMapFieldNext`
- `xrtHttpStructuredDictionaryMapNext`
- `xrtHttpStructuredDictionaryNext`
- `xrtHttpStructuredDictionaryValid`
- `xrtHttpStructuredDictionaryWrite`
- `xrtHttpStructuredDisplayDecode`
- `xrtHttpStructuredFieldCursorInit`
- `xrtHttpStructuredInnerNext`
- `xrtHttpStructuredInnerValid`
- `xrtHttpStructuredItemField`
- `xrtHttpStructuredItemParse`
- `xrtHttpStructuredItemWrite`
- `xrtHttpStructuredKeyValid`
- `xrtHttpStructuredListFieldNext`
- `xrtHttpStructuredListNext`
- `xrtHttpStructuredListValid`
- `xrtHttpStructuredListWrite`
- `xrtHttpStructuredMapCursorInit`
- `xrtHttpStructuredParameterAt`
- `xrtHttpStructuredParameterCount`
- `xrtHttpStructuredParameterFind`
- `xrtHttpStructuredParameterNext`
- `xrtHttpStructuredStringDecode`
- `xrtHttpStructuredTokenValid`

### 常量与宏 (10)

- `XHTTP_STRUCTURED_BOOLEAN`
- `XHTTP_STRUCTURED_BYTES`
- `XHTTP_STRUCTURED_DATE`
- `XHTTP_STRUCTURED_DECIMAL`
- `XHTTP_STRUCTURED_DISPLAY`
- `XHTTP_STRUCTURED_INTEGER`
- `XHTTP_STRUCTURED_MEMBER_INNER_LIST`
- `XHTTP_STRUCTURED_MEMBER_ITEM`
- `XHTTP_STRUCTURED_STRING`
- `XHTTP_STRUCTURED_TOKEN`

### 类型 (10)

- `xhttpstructureddictionaryentry`
- `xhttpstructureddictionarymember`
- `xhttpstructureditem`
- `xhttpstructuredmember`
- `xhttpstructuredmemberkind`
- `xhttpstructuredmembervalue`
- `xhttpstructuredparameter`
- `xhttpstructuredparameterentry`
- `xhttpstructuredtype`
- `xhttpstructuredvalue`

## `extlibs/xhttp/include/xrt/http_vary.h`

[查看带契约注释的公共头](../../include/xrt/http_vary.h)

### 函数 (5)

- `xrtHttpVaryCursorInit`
- `xrtHttpVaryFind`
- `xrtHttpVaryNext`
- `xrtHttpVaryPlan`
- `xrtHttpVaryWrite`

### 常量与宏 (7)

- `XHTTP_FEATURE_HTTP_VARY`
- `XHTTP_VARY_EMPTY`
- `XHTTP_VARY_MIXED`
- `XHTTP_VARY_NAMES`
- `XHTTP_VARY_NONE`
- `XHTTP_VARY_PRESENT`
- `XHTTP_VARY_WILDCARD`

### 类型 (4)

- `xhttpvarycursor`
- `xhttpvaryflag`
- `xhttpvaryitem`
- `xhttpvaryplan`

## `extlibs/xhttp/include/xrt/http_via.h`

[查看带契约注释的公共头](../../include/xrt/http_via.h)

### 函数 (10)

- `xrtHttpViaBuild`
- `xrtHttpViaCommentDecode`
- `xrtHttpViaCursorInit`
- `xrtHttpViaElementParse`
- `xrtHttpViaElementWrite`
- `xrtHttpViaFieldCursorInit`
- `xrtHttpViaFieldNext`
- `xrtHttpViaNext`
- `xrtHttpViaValid`
- `xrtHttpViaWrite`

### 常量与宏 (5)

- `XHTTP_FEATURE_HTTP_VIA`
- `XHTTP_FEATURE_HTTP_VIA_WRITE`
- `XHTTP_VIA_HAS_COMMENT`
- `XHTTP_VIA_HAS_PORT`
- `XHTTP_VIA_HAS_PROTOCOL_NAME`

### 类型 (5)

- `xhttpvia`
- `xhttpviacursor`
- `xhttpviafieldcursor`
- `xhttpviaflag`
- `xhttpviavalue`

## `extlibs/xhttp/include/xrt/mime.h`

[查看带契约注释的公共头](../../include/xrt/mime.h)

### 函数 (11)

- `xrtHttpContentTypeCompressible`
- `xrtHttpMediaTypeBuild`
- `xrtHttpMediaTypeCompressible`
- `xrtHttpMediaTypeEqual`
- `xrtHttpMediaTypeParam`
- `xrtHttpMediaTypeParse`
- `xrtHttpMediaTypeSuffix`
- `xrtHttpMediaTypeWrite`
- `xrtMime`
- `xrtMimeByExt`
- `xrtMimeByPath`

### 常量与宏 (1)

- `XHTTP_FEATURE_MIME_TYPES`

## `extlibs/xhttp/include/xrt/multipart.h`

[查看带契约注释的公共头](../../include/xrt/multipart.h)

### 函数 (24)

- `xrtMultipartBoundaryFromContentType`
- `xrtMultipartBoundaryParse`
- `xrtMultipartBoundaryRandom`
- `xrtMultipartBoundaryView`
- `xrtMultipartCloseWrite`
- `xrtMultipartContentTypeWrite`
- `xrtMultipartFieldWrite`
- `xrtMultipartFileWrite`
- `xrtMultipartFormHeadWrite`
- `xrtMultipartFormPartValid`
- `xrtMultipartLimitsInit`
- `xrtMultipartNext`
- `xrtMultipartParse`
- `xrtMultipartPartEndWrite`
- `xrtMultipartPartFileNameWrite`
- `xrtMultipartPartHeadWrite`
- `xrtMultipartPartNameWrite`
- `xrtMultipartPartWrite`
- `xrtMultipartReaderDone`
- `xrtMultipartReaderInit`
- `xrtMultipartReaderRead`
- `xrtMultipartReaderReset`
- `xrtMultipartValidate`
- `xrtMultipartValidateCount`

### 常量与宏 (1)

- `XHTTP_FEATURE_MULTIPART_STREAM`

### 类型 (6)

- `xmultiparterror`
- `xmultipartpart`
- `xmultipartpartflags`
- `xmultipartreader`
- `xmultipartreadstatus`
- `xmultipartvalidateflags`

## `extlibs/xhttp/include/xrt/query.h`

[查看带契约注释的公共头](../../include/xrt/query.h)

### 函数 (8)

- `xrtQueryBuild`
- `xrtQueryCount`
- `xrtQueryFind`
- `xrtQueryNext`
- `xrtQueryRawBuild`
- `xrtQueryRawWrite`
- `xrtQueryValidate`
- `xrtQueryWrite`

### 常量与宏 (1)

- `XHTTP_FEATURE_QUERY_CODEC`

### 类型 (3)

- `xquerylimits`
- `xquerynext`
- `xquerypair`

## `extlibs/xhttp/include/xrt/query_params.h`

[查看带契约注释的公共头](../../include/xrt/query_params.h)

### 函数 (24)

- `xrtQueryParamsAppend`
- `xrtQueryParamsAppendPair`
- `xrtQueryParamsAt`
- `xrtQueryParamsBuild`
- `xrtQueryParamsBytes`
- `xrtQueryParamsClear`
- `xrtQueryParamsClone`
- `xrtQueryParamsCompact`
- `xrtQueryParamsConfigInit`
- `xrtQueryParamsCount`
- `xrtQueryParamsCountName`
- `xrtQueryParamsCreate`
- `xrtQueryParamsDestroy`
- `xrtQueryParamsFind`
- `xrtQueryParamsGet`
- `xrtQueryParamsHas`
- `xrtQueryParamsParse`
- `xrtQueryParamsParseAppend`
- `xrtQueryParamsRemove`
- `xrtQueryParamsReserve`
- `xrtQueryParamsSet`
- `xrtQueryParamsSetPair`
- `xrtQueryParamsSort`
- `xrtQueryParamsWrite`

## `extlibs/xhttp/include/xrt/url.h`

[查看带契约注释的公共头](../../include/xrt/url.h)

### 函数 (17)

- `xrtUrlAuthorityParse`
- `xrtUrlAuthorityWrite`
- `xrtUrlBuild`
- `xrtUrlDefaultPort`
- `xrtUrlHostWrite`
- `xrtUrlParamValid`
- `xrtUrlParse`
- `xrtUrlPathNormalize`
- `xrtUrlPathNormalizeBuild`
- `xrtUrlPort`
- `xrtUrlPortIsDefault`
- `xrtUrlResolve`
- `xrtUrlResolveBuild`
- `xrtUrlSchemeIs`
- `xrtUrlSecure`
- `xrtUrlTargetWrite`
- `xrtUrlWrite`
