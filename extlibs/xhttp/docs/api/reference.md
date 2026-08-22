# xhttp 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xhttp/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `1036` 个函数、`901` 个常量或宏、
`442` 个公共类型。

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

### 类型 (11)

- `xcookieattribute`
- `xcookieattributenext`
- `xcookielimits`
- `xcookienext`
- `xcookiepair`
- `xcookiepriority`
- `xcookiesamesite`
- `xrt`
- `xsetcookie`
- `xstrview`
- `xtime`

## `extlibs/xhttp/include/xrt/cookie_jar.h`

[查看带契约注释的公共头](../../include/xrt/cookie_jar.h)

### 函数 (20)

- `xrtCookieJarApply`
- `xrtCookieJarBuild`
- `xrtCookieJarBuildUrl`
- `xrtCookieJarClear`
- `xrtCookieJarConfigInit`
- `xrtCookieJarCount`
- `xrtCookieJarCreate`
- `xrtCookieJarPurge`
- `xrtCookieJarRelease`
- `xrtCookieJarRetain`
- `xrtCookieJarSet`
- `xrtCookieJarSnapshot`
- `xrtCookieJarStore`
- `xrtCookieJarStoreHeaders`
- `xrtCookieJarStoreUrl`
- `xrtCookieJarWrite`
- `xrtCookieJarWriteUrl`
- `xrtCookieSnapshotAt`
- `xrtCookieSnapshotCount`
- `xrtCookieSnapshotDestroy`

### 常量与宏 (4)

- `XHTTP_FEATURE_COOKIE_JAR`
- `XHTTP_FEATURE_COOKIE_JAR_HEADERS`
- `XHTTP_FEATURE_HTTP_HEADERS`
- `XHTTP_FEATURE_URL`

### 类型 (11)

- `xcookieinfo`
- `xcookiejar`
- `xcookiejarconfig`
- `xcookiepublicsuffixfn`
- `xcookiereject`
- `xcookierequestcontext`
- `xcookiesnapshot`
- `xcookiestorecontext`
- `xcookiestorereport`
- `xcookiestorestatus`
- `xhttpheaders`

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

### 常量与宏 (11)

- `XHTTP_BODY_UNKNOWN`
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

### 函数 (64)

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
- `xrtHttpDigestChallengeChoose`
- `xrtHttpDigestChallengeRead`
- `xrtHttpDigestChallengeWrite`
- `xrtHttpDigestClientAuth`
- `xrtHttpDigestEqual`
- `xrtHttpDigestExchangeAuth`
- `xrtHttpDigestExchangeProof`
- `xrtHttpDigestExchangeRelease`
- `xrtHttpDigestExchangeRetain`
- `xrtHttpDigestHash`
- `xrtHttpDigestInfoRead`
- `xrtHttpDigestInfoVerify`
- `xrtHttpDigestInfoWrite`
- `xrtHttpDigestNonceCreate`
- `xrtHttpDigestNonceVerify`
- `xrtHttpDigestNonceWrite`
- `xrtHttpDigestPolicyInit`
- `xrtHttpDigestProofVerify`
- `xrtHttpDigestQopName`
- `xrtHttpDigestQopParse`
- `xrtHttpDigestReplayCheck`
- `xrtHttpDigestReplayCheckKey`
- `xrtHttpDigestReplayClear`
- `xrtHttpDigestReplayConfigInit`
- `xrtHttpDigestReplayCreate`
- `xrtHttpDigestReplayDestroy`
- `xrtHttpDigestReplayKey`
- `xrtHttpDigestReplayPurge`
- `xrtHttpDigestReplayStats`
- `xrtHttpDigestRequest`
- `xrtHttpDigestRspAuth`
- `xrtHttpDigestSecret`
- `xrtHttpDigestSessionAccept`
- `xrtHttpDigestSessionAuthorize`
- `xrtHttpDigestSessionCreate`
- `xrtHttpDigestSessionRelease`
- `xrtHttpDigestSessionRetain`
- `xrtHttpDigestSessionUpdate`
- `xrtHttpDigestSize`
- `xrtHttpDigestUserHash`
- `xrtHttpDigestVerify`
- `xrtHttpFieldChallengeNext`

### 常量与宏 (101)

- `XHTTP_AUTH_NONE`
- `XHTTP_AUTH_PARAMS`
- `XHTTP_AUTH_TOKEN68`
- `XHTTP_BEARER_HAS_ERROR`
- `XHTTP_BEARER_HAS_ERROR_DESCRIPTION`
- `XHTTP_BEARER_HAS_ERROR_URI`
- `XHTTP_BEARER_HAS_REALM`
- `XHTTP_BEARER_HAS_SCOPE`
- `XHTTP_DIGEST_ALGORITHMS_ALL`
- `XHTTP_DIGEST_ALGORITHMS_MD5`
- `XHTTP_DIGEST_ALGORITHMS_MD5_SESSION`
- `XHTTP_DIGEST_ALGORITHMS_SHA2`
- `XHTTP_DIGEST_ALGORITHMS_SHA256`
- `XHTTP_DIGEST_ALGORITHMS_SHA256_SESSION`
- `XHTTP_DIGEST_ALGORITHMS_SHA512_256`
- `XHTTP_DIGEST_ALGORITHMS_SHA512_256_SESSION`
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
- `XHTTP_DIGEST_CHOOSE_ACCEPTED`
- `XHTTP_DIGEST_CHOOSE_ERROR`
- `XHTTP_DIGEST_CHOOSE_REJECTED`
- `XHTTP_DIGEST_CLIENT_`
- `XHTTP_DIGEST_CLIENT_USERNAME_EXTENDED`
- `XHTTP_DIGEST_INFO_ERROR`
- `XHTTP_DIGEST_INFO_HAS_NEXT_NONCE`
- `XHTTP_DIGEST_INFO_HAS_RESPONSE`
- `XHTTP_DIGEST_INFO_INVALID`
- `XHTTP_DIGEST_INFO_VALID`
- `XHTTP_DIGEST_MAX_SIZE`
- `XHTTP_DIGEST_MAX_TEXT_SIZE`
- `XHTTP_DIGEST_NONCE_ERROR`
- `XHTTP_DIGEST_NONCE_INVALID`
- `XHTTP_DIGEST_NONCE_KEY_MIN`
- `XHTTP_DIGEST_NONCE_SALT_SIZE`
- `XHTTP_DIGEST_NONCE_STALE`
- `XHTTP_DIGEST_NONCE_TEXT_SIZE`
- `XHTTP_DIGEST_NONCE_VALID`
- `XHTTP_DIGEST_POLICY_PLAIN_USERNAME`
- `XHTTP_DIGEST_POLICY_PREFER_AUTH_INT`
- `XHTTP_DIGEST_POLICY_REQUIRE_USERHASH`
- `XHTTP_DIGEST_POLICY_REQUIRE_UTF8`
- `XHTTP_DIGEST_QOPS_ALL`
- `XHTTP_DIGEST_QOPS_AUTH`
- `XHTTP_DIGEST_QOPS_AUTH_INT`
- `XHTTP_DIGEST_QOP_AUTH`
- `XHTTP_DIGEST_QOP_AUTH_INT`
- `XHTTP_DIGEST_QOP_NONE`
- `XHTTP_DIGEST_REPLAY_ACCEPTED`
- `XHTTP_DIGEST_REPLAY_ERROR`
- `XHTTP_DIGEST_REPLAY_EXPIRED`
- `XHTTP_DIGEST_REPLAY_FULL`
- `XHTTP_DIGEST_REPLAY_KEY_SIZE`
- `XHTTP_DIGEST_REPLAY_REPLAY`
- `XHTTP_DIGEST_SESSION_ERROR`
- `XHTTP_DIGEST_SESSION_INVALID`
- `XHTTP_DIGEST_SESSION_SUPERSEDED`
- `XHTTP_DIGEST_SESSION_UPDATED`
- `XHTTP_DIGEST_SESSION_VALID`
- `XHTTP_DIGEST_VERIFY_ERROR`
- `XHTTP_DIGEST_VERIFY_INVALID`
- `XHTTP_DIGEST_VERIFY_REQUIRE_USERHASH`
- `XHTTP_DIGEST_VERIFY_STALE`
- `XHTTP_DIGEST_VERIFY_VALID`
- `XHTTP_FEATURE_HTTP_AUTH`
- `XHTTP_FEATURE_HTTP_AUTH_BASIC`
- `XHTTP_FEATURE_HTTP_AUTH_BEARER`
- `XHTTP_FEATURE_HTTP_AUTH_BEARER_CHALLENGE`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_INFO`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2`
- `XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY`
- `XHTTP_FEATURE_HTTP_EXT_VALUE`
- `XHTTP_FEATURE_URL_PARAM`

### 类型 (30)

- `xhttpauth`
- `xhttpauthcursor`
- `xhttpauthkind`
- `xhttpbasicauth`
- `xhttpbasicchallenge`
- `xhttpbearerchallenge`
- `xhttpdigestalgorithm`
- `xhttpdigestauth`
- `xhttpdigestchallenge`
- `xhttpdigestchoice`
- `xhttpdigestchoosecheck`
- `xhttpdigestclientauth`
- `xhttpdigestexchange`
- `xhttpdigestinfo`
- `xhttpdigestinfocheck`
- `xhttpdigestinfoverification`
- `xhttpdigestnoncecheck`
- `xhttpdigestpolicy`
- `xhttpdigestproof`
- `xhttpdigestqop`
- `xhttpdigestreplay`
- `xhttpdigestreplaycheck`
- `xhttpdigestreplayconfig`
- `xhttpdigestreplaykey`
- `xhttpdigestreplaystats`
- `xhttpdigestsession`
- `xhttpdigestsessioncheck`
- `xhttpdigestsessionconfig`
- `xhttpdigestverification`
- `xhttpdigestverifycheck`

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

### 常量与宏 (12)

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

## `extlibs/xhttp/include/xrt/http_body_file.h`

[查看带契约注释的公共头](../../include/xrt/http_body_file.h)

### 函数 (4)

- `xrtHttpBodyFileAdopt`
- `xrtHttpBodyFileConfigInit`
- `xrtHttpBodyFileFuture`
- `xrtHttpBodyFileRangeFuture`

### 常量与宏 (9)

- `XHTTP_BODY_FILE_ERROR_ADOPT`
- `XHTTP_BODY_FILE_ERROR_CREATE`
- `XHTTP_BODY_FILE_ERROR_OPEN`
- `XHTTP_BODY_FILE_ERROR_RANGE`
- `XHTTP_BODY_FILE_ERROR_READ`
- `XHTTP_BODY_FILE_ERROR_SIZE`
- `XHTTP_BODY_FILE_ERROR_SUBMIT`
- `XHTTP_BODY_FILE_READ_DEFAULT`
- `XHTTP_FEATURE_HTTP_BODY_FILE`

### 类型 (4)

- `xasyncfile`
- `xhttpbodyfileconfig`
- `xhttpbodyfileerror`
- `xtaskpool`

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

### 常量与宏 (27)

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
- `XHTTP_PARAM_HAS_VALUE`
- `XHTTP_PARAM_QUOTED`

### 类型 (5)

- `xhttpcachecontrol`
- `xhttpcachecursor`
- `xhttpcachedirective`
- `xhttpcacheflag`
- `xhttpcacheitem`

## `extlibs/xhttp/include/xrt/http_cache_policy.h`

[查看带契约注释的公共头](../../include/xrt/http_cache_policy.h)

### 函数 (6)

- `xrtHttpCacheMethodDefault`
- `xrtHttpCacheStatusHeuristic`
- `xrtHttpCacheStoreInputInit`
- `xrtHttpCacheStorePlan`
- `xrtHttpCacheUseInputInit`
- `xrtHttpCacheUsePlan`

### 常量与宏 (72)

- `XHTTP_CACHE_REASON_AUTHORIZATION`
- `XHTTP_CACHE_REASON_CANDIDATE_MISS`
- `XHTTP_CACHE_REASON_DISCONNECTED`
- `XHTTP_CACHE_REASON_EXTENSION`
- `XHTTP_CACHE_REASON_HEADERS_INCOMPLETE`
- `XHTTP_CACHE_REASON_METHOD_NOT_CACHEABLE`
- `XHTTP_CACHE_REASON_METHOD_UNKNOWN`
- `XHTTP_CACHE_REASON_NONE`
- `XHTTP_CACHE_REASON_NO_CLOCK`
- `XHTTP_CACHE_REASON_NO_PERMISSION`
- `XHTTP_CACHE_REASON_ONLY_IF_CACHED`
- `XHTTP_CACHE_REASON_PARTIAL_UNSUPPORTED`
- `XHTTP_CACHE_REASON_POST_REQUIREMENTS`
- `XHTTP_CACHE_REASON_REPRESENTATION_UNUSABLE`
- `XHTTP_CACHE_REASON_REQUEST_NO_STORE`
- `XHTTP_CACHE_REASON_REQUEST_REVALIDATE`
- `XHTTP_CACHE_REASON_RESPONSE_INCOMPLETE`
- `XHTTP_CACHE_REASON_RESPONSE_NO_STORE`
- `XHTTP_CACHE_REASON_RESPONSE_REVALIDATE`
- `XHTTP_CACHE_REASON_SHARED_PRIVATE`
- `XHTTP_CACHE_REASON_STALE`
- `XHTTP_CACHE_REASON_STATUS_NOT_FINAL`
- `XHTTP_CACHE_REASON_STATUS_NOT_UNDERSTOOD`
- `XHTTP_CACHE_STORE_ACTION_NONE`
- `XHTTP_CACHE_STORE_AS_200`
- `XHTTP_CACHE_STORE_AUTHORIZATION`
- `XHTTP_CACHE_STORE_CONTENT_LOCATION_MATCH`
- `XHTTP_CACHE_STORE_ERROR`
- `XHTTP_CACHE_STORE_EXTENSION`
- `XHTTP_CACHE_STORE_EXTENSION_FRESHNESS`
- `XHTTP_CACHE_STORE_EXTENSION_OVERRIDE`
- `XHTTP_CACHE_STORE_HEADERS_COMPLETE`
- `XHTTP_CACHE_STORE_IGNORE_NO_STORE`
- `XHTTP_CACHE_STORE_KEEP`
- `XHTTP_CACHE_STORE_MARK_INCOMPLETE`
- `XHTTP_CACHE_STORE_METHOD_CACHEABLE`
- `XHTTP_CACHE_STORE_METHOD_UNDERSTOOD`
- `XHTTP_CACHE_STORE_NONE`
- `XHTTP_CACHE_STORE_RANGE_SUPPORTED`
- `XHTTP_CACHE_STORE_REMOVE_CONNECTION`
- `XHTTP_CACHE_STORE_REMOVE_NO_CACHE`
- `XHTTP_CACHE_STORE_REMOVE_PRIVATE`
- `XHTTP_CACHE_STORE_REMOVE_PROXY`
- `XHTTP_CACHE_STORE_RESPONSE_COMPLETE`
- `XHTTP_CACHE_STORE_SEPARATE_TRAILERS`
- `XHTTP_CACHE_STORE_SHARED`
- `XHTTP_CACHE_STORE_SKIP`
- `XHTTP_CACHE_STORE_STATUS_HEURISTIC`
- `XHTTP_CACHE_STORE_STATUS_UNDERSTOOD`
- `XHTTP_CACHE_USE_ACTION_NONE`
- `XHTTP_CACHE_USE_AUTHORIZATION`
- `XHTTP_CACHE_USE_CANDIDATE_MATCH`
- `XHTTP_CACHE_USE_CLOCK`
- `XHTTP_CACHE_USE_DISCONNECTED`
- `XHTTP_CACHE_USE_ERROR`
- `XHTTP_CACHE_USE_EVICT`
- `XHTTP_CACHE_USE_EXTENSION`
- `XHTTP_CACHE_USE_FORWARD`
- `XHTTP_CACHE_USE_GATEWAY_TIMEOUT`
- `XHTTP_CACHE_USE_NONE`
- `XHTTP_CACHE_USE_REMOVE_NO_CACHE`
- `XHTTP_CACHE_USE_REPRESENTATION`
- `XHTTP_CACHE_USE_SET_AGE`
- `XHTTP_CACHE_USE_SHARED`
- `XHTTP_CACHE_USE_STALE`
- `XHTTP_CACHE_USE_STALE_ALLOWED`
- `XHTTP_CACHE_USE_STATUS_UNDERSTOOD`
- `XHTTP_CACHE_USE_STORED`
- `XHTTP_CACHE_USE_VALIDATE`
- `XHTTP_CACHE_USE_VALIDATED`
- `XHTTP_FEATURE_HTTP_CACHE_POLICY`
- `XHTTP_FEATURE_HTTP_CACHE_TIME`

### 类型 (14)

- `xhttpcacheage`
- `xhttpcachefreshness`
- `xhttpcachepolicyreason`
- `xhttpcachestoreaction`
- `xhttpcachestoredecision`
- `xhttpcachestoreflag`
- `xhttpcachestoreinput`
- `xhttpcachestoreplan`
- `xhttpcachetime`
- `xhttpcacheuseaction`
- `xhttpcacheusedecision`
- `xhttpcacheuseflag`
- `xhttpcacheuseinput`
- `xhttpcacheuseplan`

## `extlibs/xhttp/include/xrt/http_cache_range.h`

[查看带契约注释的公共头](../../include/xrt/http_cache_range.h)

### 函数 (9)

- `xrtHttpCacheCombinePlan`
- `xrtHttpCacheCoverageCovers`
- `xrtHttpCacheCoverageValid`
- `xrtHttpCacheFragmentComplete`
- `xrtHttpCacheFragmentInputInit`
- `xrtHttpCacheFragmentPlan`
- `xrtHttpCacheFragmentValid`
- `xrtHttpCacheMissingCursorInit`
- `xrtHttpCacheMissingNext`

### 常量与宏 (45)

- `XHTTP_CACHE_COMBINE_ACTION_NONE`
- `XHTTP_CACHE_COMBINE_APPLY`
- `XHTTP_CACHE_COMBINE_AS_200`
- `XHTTP_CACHE_COMBINE_CONFLICT`
- `XHTTP_CACHE_COMBINE_ERROR`
- `XHTTP_CACHE_COMBINE_MARK_INCOMPLETE`
- `XHTTP_CACHE_COMBINE_REMOVE_CONTENT_LENGTH`
- `XHTTP_CACHE_COMBINE_REMOVE_CONTENT_RANGE`
- `XHTTP_CACHE_COMBINE_REPLACE`
- `XHTTP_CACHE_COMBINE_SEPARATE`
- `XHTTP_CACHE_COMBINE_SET_CONTENT_LENGTH`
- `XHTTP_CACHE_COMBINE_UPDATE_INCOMING_FIELDS`
- `XHTTP_CACHE_COMBINE_USE_INCOMING_FIELDS`
- `XHTTP_CACHE_COVERAGE_ERROR`
- `XHTTP_CACHE_COVERAGE_HIT`
- `XHTTP_CACHE_COVERAGE_MISS`
- `XHTTP_CACHE_FRAGMENT_ACTION_NONE`
- `XHTTP_CACHE_FRAGMENT_AS_200`
- `XHTTP_CACHE_FRAGMENT_BODY_COMPLETE`
- `XHTTP_CACHE_FRAGMENT_ERROR`
- `XHTTP_CACHE_FRAGMENT_HAS_LENGTH`
- `XHTTP_CACHE_FRAGMENT_HAS_RANGE`
- `XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE`
- `XHTTP_CACHE_FRAGMENT_INPUT_NONE`
- `XHTTP_CACHE_FRAGMENT_MARK_INCOMPLETE`
- `XHTTP_CACHE_FRAGMENT_MULTIPART_PART`
- `XHTTP_CACHE_FRAGMENT_NONE`
- `XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH`
- `XHTTP_CACHE_FRAGMENT_REASON_CONTENT_LENGTH`
- `XHTTP_CACHE_FRAGMENT_REASON_CONTENT_RANGE`
- `XHTTP_CACHE_FRAGMENT_REASON_EMPTY`
- `XHTTP_CACHE_FRAGMENT_REASON_HEADERS`
- `XHTTP_CACHE_FRAGMENT_REASON_METHOD`
- `XHTTP_CACHE_FRAGMENT_REASON_NONE`
- `XHTTP_CACHE_FRAGMENT_REASON_STATUS`
- `XHTTP_CACHE_FRAGMENT_REASON_TRANSFORMED`
- `XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_LENGTH`
- `XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_RANGE`
- `XHTTP_CACHE_FRAGMENT_SET_CONTENT_LENGTH`
- `XHTTP_CACHE_FRAGMENT_SKIP`
- `XHTTP_CACHE_FRAGMENT_STORE`
- `XHTTP_CACHE_FRAGMENT_TRANSFORMED`
- `XHTTP_FEATURE_HTTP_CACHE_RANGE`
- `XHTTP_FEATURE_HTTP_CACHE_VALIDATE`
- `XHTTP_FEATURE_HTTP_RANGE`

### 类型 (16)

- `xhttpbyterange`
- `xhttpcachecombineaction`
- `xhttpcachecombinedecision`
- `xhttpcachecombineplan`
- `xhttpcachecoverage`
- `xhttpcachecoverageresult`
- `xhttpcacheentry`
- `xhttpcachefragment`
- `xhttpcachefragmentaction`
- `xhttpcachefragmentdecision`
- `xhttpcachefragmentflag`
- `xhttpcachefragmentinput`
- `xhttpcachefragmentinputflag`
- `xhttpcachefragmentplan`
- `xhttpcachefragmentreason`
- `xhttpcachemissingcursor`

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

## `extlibs/xhttp/include/xrt/http_cache_store.h`

[查看带契约注释的公共头](../../include/xrt/http_cache_store.h)

### 函数 (41)

- `xrtHttpCacheClear`
- `xrtHttpCacheConfigInit`
- `xrtHttpCacheCreate`
- `xrtHttpCacheGet`
- `xrtHttpCacheInsert`
- `xrtHttpCacheKeyInit`
- `xrtHttpCacheOpen`
- `xrtHttpCachePut`
- `xrtHttpCacheRecordBodyBytes`
- `xrtHttpCacheRecordCharge`
- `xrtHttpCacheRecordCreate`
- `xrtHttpCacheRecordEntry`
- `xrtHttpCacheRecordField`
- `xrtHttpCacheRecordFieldAt`
- `xrtHttpCacheRecordFieldCount`
- `xrtHttpCacheRecordFlags`
- `xrtHttpCacheRecordInputInit`
- `xrtHttpCacheRecordKey`
- `xrtHttpCacheRecordLength`
- `xrtHttpCacheRecordMatches`
- `xrtHttpCacheRecordPartAt`
- `xrtHttpCacheRecordPartCount`
- `xrtHttpCacheRecordReason`
- `xrtHttpCacheRecordRelease`
- `xrtHttpCacheRecordRequestClock`
- `xrtHttpCacheRecordResponseClock`
- `xrtHttpCacheRecordResponseTime`
- `xrtHttpCacheRecordRetain`
- `xrtHttpCacheRecordStatus`
- `xrtHttpCacheRecordTrailerAt`
- `xrtHttpCacheRecordTrailerCount`
- `xrtHttpCacheRecordVaryAt`
- `xrtHttpCacheRecordVaryCount`
- `xrtHttpCacheRecordVersion`
- `xrtHttpCacheRelease`
- `xrtHttpCacheRemove`
- `xrtHttpCacheRemoveRecord`
- `xrtHttpCacheRemoveURI`
- `xrtHttpCacheReplace`
- `xrtHttpCacheRetain`
- `xrtHttpCacheStats`

### 常量与宏 (19)

- `XHTTP_CACHE_BYTES_DEFAULT`
- `XHTTP_CACHE_CHANGE_APPLIED`
- `XHTTP_CACHE_CHANGE_CONFLICT`
- `XHTTP_CACHE_CHANGE_ERROR`
- `XHTTP_CACHE_ENTRIES_DEFAULT`
- `XHTTP_CACHE_ENTRY_BYTES_DEFAULT`
- `XHTTP_CACHE_LOOKUP_ERROR`
- `XHTTP_CACHE_LOOKUP_HIT`
- `XHTTP_CACHE_LOOKUP_MISS`
- `XHTTP_CACHE_PUT_CONFLICT`
- `XHTTP_CACHE_PUT_ERROR`
- `XHTTP_CACHE_PUT_REJECTED`
- `XHTTP_CACHE_PUT_REPLACED`
- `XHTTP_CACHE_PUT_STORED`
- `XHTTP_CACHE_RECORD_COMPLETE`
- `XHTTP_CACHE_RECORD_HAS_LENGTH`
- `XHTTP_CACHE_RECORD_NONE`
- `XHTTP_FEATURE_HTTP_CACHE_STORE`
- `XHTTP_FEATURE_HTTP_VARY`

### 类型 (13)

- `xhttpcache`
- `xhttpcachechange`
- `xhttpcacheconfig`
- `xhttpcachekey`
- `xhttpcachelookup`
- `xhttpcacheops`
- `xhttpcachepart`
- `xhttpcacheput`
- `xhttpcacherecord`
- `xhttpcacherecordflag`
- `xhttpcacherecordinput`
- `xhttpcachestats`
- `xhttpversion`

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

### 类型 (3)

- `xhttpcachecalc`
- `xhttpcachefreshnesssource`
- `xhttpcachetimeflag`

## `extlibs/xhttp/include/xrt/http_cache_validate.h`

[查看带契约注释的公共头](../../include/xrt/http_cache_validate.h)

### 函数 (13)

- `xrtHttpCache304Select`
- `xrtHttpCacheEntryValid`
- `xrtHttpCacheFieldStore`
- `xrtHttpCacheFieldUpdate`
- `xrtHttpCacheHeadPlan`
- `xrtHttpCacheIfRangePlan`
- `xrtHttpCacheInvalidationCursorInit`
- `xrtHttpCacheInvalidationNext`
- `xrtHttpCacheInvalidationWrite`
- `xrtHttpCachePreconditionsEvaluate`
- `xrtHttpCacheValidateETagsWrite`
- `xrtHttpCacheValidatePlan`
- `xrtHttpCacheValidateResult`

### 常量与宏 (41)

- `XHTTP_CACHE_ENTRY_NONE`
- `XHTTP_CACHE_ENTRY_PARTIAL`
- `XHTTP_CACHE_ENTRY_RANGE_COVERED`
- `XHTTP_CACHE_FIELD_STORE_ERROR`
- `XHTTP_CACHE_FIELD_STORE_KEEP`
- `XHTTP_CACHE_FIELD_STORE_SKIP`
- `XHTTP_CACHE_FIELD_UPDATE_ERROR`
- `XHTTP_CACHE_FIELD_UPDATE_REPLACE`
- `XHTTP_CACHE_FIELD_UPDATE_SKIP`
- `XHTTP_CACHE_HEAD_ERROR`
- `XHTTP_CACHE_HEAD_IGNORE`
- `XHTTP_CACHE_HEAD_STALE`
- `XHTTP_CACHE_HEAD_UPDATE`
- `XHTTP_CACHE_IF_RANGE_DATE`
- `XHTTP_CACHE_IF_RANGE_ERROR`
- `XHTTP_CACHE_IF_RANGE_ETAG`
- `XHTTP_CACHE_IF_RANGE_NONE`
- `XHTTP_CACHE_INVALIDATE_CONTENT_LOCATION`
- `XHTTP_CACHE_INVALIDATE_LOCATION`
- `XHTTP_CACHE_INVALIDATE_TARGET`
- `XHTTP_CACHE_UPDATE_FIELD_DEPENDENT`
- `XHTTP_CACHE_UPDATE_FIELD_NONE`
- `XHTTP_CACHE_UPDATE_FIELD_PROCESSED`
- `XHTTP_CACHE_UPDATE_MATCH_ERROR`
- `XHTTP_CACHE_UPDATE_MATCH_NONE`
- `XHTTP_CACHE_UPDATE_MATCH_SINGLE`
- `XHTTP_CACHE_UPDATE_MATCH_STRONG`
- `XHTTP_CACHE_UPDATE_MATCH_WEAK`
- `XHTTP_CACHE_VALIDATE_ACTION_NONE`
- `XHTTP_CACHE_VALIDATE_CONDITIONAL`
- `XHTTP_CACHE_VALIDATE_ERROR`
- `XHTTP_CACHE_VALIDATE_IF_MODIFIED_SINCE`
- `XHTTP_CACHE_VALIDATE_IF_NONE_MATCH`
- `XHTTP_CACHE_VALIDATE_NONE`
- `XHTTP_CACHE_VALIDATE_RESULT_ERROR`
- `XHTTP_CACHE_VALIDATE_RESULT_FULL`
- `XHTTP_CACHE_VALIDATE_RESULT_NOT_MODIFIED`
- `XHTTP_CACHE_VALIDATE_RESULT_SERVER_FAILURE`
- `XHTTP_FEATURE_HTTP_FORWARD`
- `XHTTP_FEATURE_HTTP_ORIGIN`
- `XHTTP_FEATURE_HTTP_PRECONDITION`

### 类型 (17)

- `xhttpcacheentryflag`
- `xhttpcachefieldstore`
- `xhttpcachefieldupdate`
- `xhttpcacheheaddecision`
- `xhttpcacheifrange`
- `xhttpcacheifrangekind`
- `xhttpcacheinvalidatecursor`
- `xhttpcacheinvalidateitem`
- `xhttpcacheinvalidatekind`
- `xhttpcacheupdatefieldflag`
- `xhttpcacheupdatematch`
- `xhttpcachevalidateaction`
- `xhttpcachevalidatedecision`
- `xhttpcachevalidateplan`
- `xhttpcachevalidateresult`
- `xhttpetag`
- `xhttpprecondition`

## `extlibs/xhttp/include/xrt/http_client.h`

[查看带契约注释的公共头](../../include/xrt/http_client.h)

### 函数 (101)

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
- `xrtHttp1RequestPrepareDigest`
- `xrtHttp1RequestPrepareProxyDigest`
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
- `xrtHttpResponseDigestChallengeChoose`
- `xrtHttpResponseDigestChallengeNext`
- `xrtHttpResponseDigestInfo`
- `xrtHttpResponseDigestSessionAccept`
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
- `xrtHttpResponseProxyDigestChallengeChoose`
- `xrtHttpResponseProxyDigestChallengeNext`
- `xrtHttpResponseProxyDigestInfo`
- `xrtHttpResponseProxyDigestSessionAccept`
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

### 常量与宏 (55)

- `XHTTP_FEATURE_HTTP_CLIENT_CONTENT_TYPE`
- `XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS`
- `XHTTP_FEATURE_HTTP_CLIENT_PREPARE`
- `XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_BASIC`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_BEARER`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_DIGEST`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA_RANDOM`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_QUERY`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TE`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_BASIC`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_BEARER`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_INFO`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION`
- `XHTTP_FEATURE_HTTP_CLIENT_SET_COOKIE`
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

### 类型 (12)

- `xhttp1requestoptions`
- `xhttp1requestplan`
- `xhttp1targetform`
- `xhttpheadersconfig`
- `xhttprequest`
- `xhttprequestbodymode`
- `xhttprequesterror`
- `xhttpresponse`
- `xhttpresponseerror`
- `xhttpresponseflag`
- `xqueryparams`
- `xurl`

## `extlibs/xhttp/include/xrt/http_client_easy.h`

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

### 类型 (5)

- `xhttpcall`
- `xhttpcalloptions`
- `xhttpcallproc`
- `xhttpclient`
- `xhttpresult`

## `extlibs/xhttp/include/xrt/http_client_future.h`

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

### 类型 (3)

- `xhttpcallinfo`
- `xnetstream`
- `xtlsstream`

## `extlibs/xhttp/include/xrt/http_client_runtime.h`

[查看带契约注释的公共头](../../include/xrt/http_client_runtime.h)

### 函数 (40)

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
- `xrtHttpRedirectConfigInit`
- `xrtHttpResumeConfigInit`
- `xrtHttpRetryConfigInit`
- `xrtHttpRetryOptionsInit`

### 常量与宏 (109)

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
- `XHTTP_FEATURE_HTTP_CLIENT_CACHE`
- `XHTTP_FEATURE_HTTP_CLIENT_COOKIES`
- `XHTTP_FEATURE_HTTP_CLIENT_POOL`
- `XHTTP_FEATURE_HTTP_CLIENT_PROXY`
- `XHTTP_FEATURE_HTTP_CLIENT_REDIRECT`
- `XHTTP_FEATURE_HTTP_CLIENT_RESUME`
- `XHTTP_FEATURE_HTTP_CLIENT_RETRY`
- `XHTTP_FEATURE_HTTP_CLIENT_STREAM`
- `XHTTP_FEATURE_HTTP_CLIENT_TLS`
- `XHTTP_FEATURE_HTTP_RANGE_MULTIPART`
- `XHTTP_FEATURE_HTTP_RETRY`
- `XHTTP_PROXY_DEFAULT`
- `XHTTP_PROXY_DIRECT`
- `XHTTP_PROXY_EXPLICIT`
- `XHTTP_REDIRECT_ALLOW_DOWNGRADE`
- `XHTTP_REDIRECT_DEFAULT`
- `XHTTP_REDIRECT_ERROR`
- `XHTTP_REDIRECT_FOLLOW`
- `XHTTP_REDIRECT_FORWARD_CREDENTIALS`
- `XHTTP_REDIRECT_MANUAL`
- `XHTTP_REDIRECT_MAX_DEFAULT`
- `XHTTP_REDIRECT_POST_TO_GET`
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

### 类型 (41)

- `xcancel`
- `xhttp1callconfig`
- `xhttp1exchangeconfig`
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
- `xtlscontext`
- `xtlsstreamconfig`
- `xtlsverifier`

## `extlibs/xhttp/include/xrt/http_client_stream.h`

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

## `extlibs/xhttp/include/xrt/http_compress.h`

[查看带契约注释的公共头](../../include/xrt/http_compress.h)

### 函数 (3)

- `xrtHttpReplyCompress`
- `xrtHttpReplyCompressConfigInit`
- `xrtHttpServerReplyCompress`

### 常量与宏 (22)

- `XHTTP_FEATURE_HTTP_ETAG`
- `XHTTP_FEATURE_HTTP_REPLY_COMPRESS`
- `XHTTP_FEATURE_HTTP_SERVER_COMPRESS`
- `XHTTP_FEATURE_HTTP_SERVER_REPLY`
- `XHTTP_FEATURE_HTTP_SERVER_REQUEST`
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

### 类型 (9)

- `xdeflatestrategy`
- `xhttpacceptencoding`
- `xhttpcoding`
- `xhttpreply`
- `xhttpreplycompressconfig`
- `xhttpreplycompresserror`
- `xhttpreplycompressflag`
- `xhttpreplycompressstatus`
- `xhttpserverrequest`

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

### 类型 (9)

- `xhttp1bodylimits`
- `xhttp1bodyproc`
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

## `extlibs/xhttp/include/xrt/http_ext_value.h`

[查看带契约注释的公共头](../../include/xrt/http_ext_value.h)

### 函数 (4)

- `xrtHttpExtValueBuild`
- `xrtHttpExtValueParse`
- `xrtHttpExtValueRead`
- `xrtHttpExtValueWrite`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_LANGUAGE_CORE`

### 类型 (2)

- `xhttp`
- `xhttpextvalue`

## `extlibs/xhttp/include/xrt/http_forward.h`

[查看带契约注释的公共头](../../include/xrt/http_forward.h)

### 函数 (5)

- `xrtHttpHopField`
- `xrtHttpHopFieldKnown`
- `xrtHttpMaxForwardsParse`
- `xrtHttpMaxForwardsUpdate`
- `xrtHttpMaxForwardsWrite`

### 常量与宏 (3)

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

### 类型 (7)

- `xhttplink`
- `xhttplinkcursor`
- `xhttplinkfieldcursor`
- `xhttplinkflags`
- `xhttplinkparamvalue`
- `xhttplinkvalue`
- `xhttpparamflags`

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

### 类型 (2)

- `xhttporigin`
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

### 函数 (7)

- `xrtHttpRetryAfterBuild`
- `xrtHttpRetryAfterDelay`
- `xrtHttpRetryAfterFields`
- `xrtHttpRetryAfterParse`
- `xrtHttpRetryAfterWrite`
- `xrtHttpRetryBackoff`
- `xrtHttpRetryStatusDefault`

### 常量与宏 (3)

- `XHTTP_RETRY_AFTER_DATE`
- `XHTTP_RETRY_AFTER_DELAY`
- `XHTTP_RETRY_AFTER_NONE`

### 类型 (2)

- `xhttpretryafter`
- `xhttpretryafterkind`

## `extlibs/xhttp/include/xrt/http_route.h`

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

## `extlibs/xhttp/include/xrt/http_router.h`

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

### 类型 (7)

- `xhttpcontentrange`
- `xhttpetagitem`
- `xhttpetagkind`
- `xhttprangeresult`
- `xhttprangespec`
- `xhttprangespecform`
- `xhttprepresentation`

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

### 常量与宏 (6)

- `XHTTP_FEATURE_HTTP_SERVER_EXCHANGE`
- `XHTTP_FEATURE_HTTP_SERVER_RESPONSE`
- `XHTTP_SERVER_BODY_BUFFER`
- `XHTTP_SERVER_BODY_DISCARD`
- `XHTTP_SERVER_BODY_REJECT`
- `XHTTP_SERVER_BODY_STREAM`

### 类型 (12)

- `xhttp1serverconfig`
- `xhttp1servererror`
- `xhttp1serverevents`
- `xhttp1serverexchange`
- `xhttp1serverfeedstatus`
- `xhttp1serveroutputstatus`
- `xhttp1serverresponse`
- `xhttp1serverresponseerror`
- `xhttpserverbodypolicy`
- `xhttpserverbodyproc`
- `xhttpservercompleteproc`
- `xhttpserverheadersproc`

## `extlibs/xhttp/include/xrt/http_server_file.h`

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

## `extlibs/xhttp/include/xrt/http_server_future.h`

[查看带契约注释的公共头](../../include/xrt/http_server_future.h)

### 函数 (2)

- `xrtHttpConnRespondFuture`
- `xrtHttpServerWaitAsync`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_SERVER`

### 类型 (1)

- `xhttpserver`

## `extlibs/xhttp/include/xrt/http_server_middleware.h`

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

## `extlibs/xhttp/include/xrt/http_server_mux.h`

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

### 类型 (7)

- `xhttpserverconfig`
- `xhttpserverevents`
- `xhttpservermux`
- `xhttpservermuxconfig`
- `xhttpservermuxerror`
- `xhttpservermuxstats`
- `xhttpservermuxstatus`

## `extlibs/xhttp/include/xrt/http_server_mux_tls.h`

[查看带契约注释的公共头](../../include/xrt/http_server_mux_tls.h)

### 函数 (1)

- `xrtHttpServerMuxStartTls`

### 常量与宏 (2)

- `XHTTP_FEATURE_HTTP_SERVER_MUX_TLS`
- `XHTTP_FEATURE_HTTP_SERVER_TLS`

### 类型 (1)

- `xhttpservertlsconfig`

## `extlibs/xhttp/include/xrt/http_server_raw.h`

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

### 类型 (2)

- `xhttpserverrawflag`
- `xnetref`

## `extlibs/xhttp/include/xrt/http_server_response_async.h`

[查看带契约注释的公共头](../../include/xrt/http_server_response_async.h)

### 函数 (1)

- `xrtHttp1ServerResponseWait`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_SERVER_RESPONSE_ASYNC`

## `extlibs/xhttp/include/xrt/http_server_router.h`

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

### 类型 (5)

- `xhttpserverroutebodyproc`
- `xhttpserverrouteevents`
- `xhttpserverrouteheadersproc`
- `xhttpserverrouteproc`
- `xhttpserverroutererror`

## `extlibs/xhttp/include/xrt/http_server_router_tls.h`

[查看带契约注释的公共头](../../include/xrt/http_server_router_tls.h)

### 函数 (1)

- `xrtHttpServerRouterStartTls`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_SERVER_ROUTER_TLS`

## `extlibs/xhttp/include/xrt/http_server_runtime.h`

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

### 常量与宏 (28)

- `XHTTP_CONN_BODY`
- `XHTTP_CONN_CLOSED`
- `XHTTP_CONN_CLOSING`
- `XHTTP_CONN_INFORMATION`
- `XHTTP_CONN_REQUEST`
- `XHTTP_CONN_RESPONSE`
- `XHTTP_CONN_UPGRADED`
- `XHTTP_CONN_WAITING`
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

### 类型 (8)

- `xhttpconnstate`
- `xhttpconnstats`
- `xhttpservererror`
- `xhttpserverstate`
- `xhttpserverstats`
- `xnetaddr`
- `xnetserver`
- `xnetserverconfig`

## `extlibs/xhttp/include/xrt/http_server_static.h`

[查看带契约注释的公共头](../../include/xrt/http_server_static.h)

### 函数 (6)

- `xrtHttpConnStatic`
- `xrtHttpReplyFromStatic`
- `xrtHttpReplyStatic`
- `xrtHttpReplyStaticFuture`
- `xrtHttpStaticReplyConfigInit`
- `xrtHttpStaticServeConfigInit`

### 常量与宏 (15)

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

## `extlibs/xhttp/include/xrt/http_server_tls.h`

[查看带契约注释的公共头](../../include/xrt/http_server_tls.h)

### 函数 (3)

- `xrtHttpConnTls`
- `xrtHttpServerStartTls`
- `xrtHttpServerTlsConfigInit`

### 类型 (1)

- `xtlsserverconfig`

## `extlibs/xhttp/include/xrt/http_server_upgrade.h`

[查看带契约注释的公共头](../../include/xrt/http_server_upgrade.h)

### 函数 (4)

- `xrtHttpConnUpgrade`
- `xrtHttpConnUpgradeRaw`
- `xrtHttpConnUpgradeResponse`
- `xrtHttpUpgradeAbort`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_SERVER_UPGRADE`

### 类型 (2)

- `xhttpupgrade`
- `xhttpupgradeproc`

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

## `extlibs/xhttp/include/xrt/http_sse_client.h`

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

### 常量与宏 (27)

- `XHTTP_FEATURE_HTTP_SSE_CLIENT`
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

### 类型 (13)

- `xhttpsseclient`
- `xhttpsseclientconfig`
- `xhttpsseclienterror`
- `xhttpsseclientevents`
- `xhttpsseclientinfo`
- `xhttpsseclientstate`
- `xhttpssecloseproc`
- `xhttpsseclosereason`
- `xhttpssecommentproc`
- `xhttpssemessageproc`
- `xhttpsseopenproc`
- `xhttpsseretryingproc`
- `xhttpsseretryproc`

## `extlibs/xhttp/include/xrt/http_sse_server.h`

[查看带契约注释的公共头](../../include/xrt/http_sse_server.h)

### 函数 (4)

- `xrtHttpSseReplyCreate`
- `xrtHttpSseSend`
- `xrtHttpSseSendComment`
- `xrtHttpSseSendEvent`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_SSE_SERVER`

## `extlibs/xhttp/include/xrt/http_static.h`

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

### 常量与宏 (17)

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

### 类型 (8)

- `xfileinfo`
- `xhttpstaticfileerror`
- `xhttpstaticmultipartbodyerror`
- `xhttpstaticpath`
- `xhttpstaticpathflag`
- `xhttpstaticpathstatus`
- `xhttpstaticplan`
- `xhttpstaticresponseconfig`

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

### 常量与宏 (6)

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
