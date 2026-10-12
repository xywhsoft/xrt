# xhttp-policy 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xhttp-policy/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `134` 个函数、`263` 个常量或宏、
`111` 个公共类型。

## `extlibs/xhttp-policy/include/xhttp-policy/export.h`

[查看带契约注释的公共头](../../include/xhttp-policy/export.h)

### 常量与宏 (2)

- `XHTTP_POLICY_BUILD_SHARED`
- `XHTTP_POLICY_USE_SHARED`

## `extlibs/xhttp-policy/include/xrt/cookie_jar.h`

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

### 常量与宏 (5)

- `XHTTP_FEATURE_COOKIE_JAR`
- `XHTTP_FEATURE_COOKIE_JAR_HEADERS`
- `XHTTP_FEATURE_HTTP_HEADERS`
- `XHTTP_FEATURE_SET_COOKIE`
- `XHTTP_FEATURE_URL`

### 类型 (16)

- `xcookieinfo`
- `xcookiejar`
- `xcookiejarconfig`
- `xcookiepriority`
- `xcookiepublicsuffixfn`
- `xcookiereject`
- `xcookierequestcontext`
- `xcookiesamesite`
- `xcookiesnapshot`
- `xcookiestorecontext`
- `xcookiestorereport`
- `xcookiestorestatus`
- `xhttpheaders`
- `xsetcookie`
- `xstrview`
- `xtime`

## `extlibs/xhttp-policy/include/xrt/http_auth_policy.h`

[查看带契约注释的公共头](../../include/xrt/http_auth_policy.h)

### 函数 (34)

- `xrtHttpDigestChallengeChoose`
- `xrtHttpDigestClientAuth`
- `xrtHttpDigestEqual`
- `xrtHttpDigestExchangeAuth`
- `xrtHttpDigestExchangeProof`
- `xrtHttpDigestExchangeRelease`
- `xrtHttpDigestExchangeRetain`
- `xrtHttpDigestHash`
- `xrtHttpDigestInfoVerify`
- `xrtHttpDigestNonceCreate`
- `xrtHttpDigestNonceVerify`
- `xrtHttpDigestNonceWrite`
- `xrtHttpDigestPolicyInit`
- `xrtHttpDigestProofVerify`
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
- `xrtHttpDigestUserHash`
- `xrtHttpDigestVerify`

### 常量与宏 (57)

- `XHTTP_DIGEST_ALGORITHMS_ALL`
- `XHTTP_DIGEST_ALGORITHMS_MD5`
- `XHTTP_DIGEST_ALGORITHMS_MD5_SESSION`
- `XHTTP_DIGEST_ALGORITHMS_SHA2`
- `XHTTP_DIGEST_ALGORITHMS_SHA256`
- `XHTTP_DIGEST_ALGORITHMS_SHA256_SESSION`
- `XHTTP_DIGEST_ALGORITHMS_SHA512_256`
- `XHTTP_DIGEST_ALGORITHMS_SHA512_256_SESSION`
- `XHTTP_DIGEST_CHOOSE_ACCEPTED`
- `XHTTP_DIGEST_CHOOSE_ERROR`
- `XHTTP_DIGEST_CHOOSE_REJECTED`
- `XHTTP_DIGEST_CLIENT_USERNAME_EXTENDED`
- `XHTTP_DIGEST_INFO_ERROR`
- `XHTTP_DIGEST_INFO_INVALID`
- `XHTTP_DIGEST_INFO_VALID`
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

### 类型 (25)

- `xbytesview`
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

## `extlibs/xhttp-policy/include/xrt/http_cache_policy.h`

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

### 类型 (15)

- `xhttpcacheage`
- `xhttpcachecontrol`
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

## `extlibs/xhttp-policy/include/xrt/http_cache_range.h`

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

### 类型 (18)

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
- `xhttpfield`
- `xhttpnext`

## `extlibs/xhttp-policy/include/xrt/http_cache_store.h`

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

## `extlibs/xhttp-policy/include/xrt/http_cache_validate.h`

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

## `extlibs/xhttp-policy/include/xrt/http_client_policy.h`

[查看带契约注释的公共头](../../include/xrt/http_client_policy.h)

### 函数 (6)

- `xrtHttp1RequestPrepareDigest`
- `xrtHttp1RequestPrepareProxyDigest`
- `xrtHttpResponseDigestChallengeChoose`
- `xrtHttpResponseDigestSessionAccept`
- `xrtHttpResponseProxyDigestChallengeChoose`
- `xrtHttpResponseProxyDigestSessionAccept`

### 常量与宏 (6)

- `XHTTP_FEATURE_HTTP_CLIENT_PREPARE`
- `XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_INFO`
- `XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION`

### 类型 (4)

- `xhttp1requestoptions`
- `xhttp1requestplan`
- `xhttprequest`
- `xhttpresponse`

## `extlibs/xhttp-policy/include/xrt/http_redirect_policy.h`

[查看带契约注释的公共头](../../include/xrt/http_redirect_policy.h)

### 函数 (3)

- `xrtHttpRedirectConfigInit`
- `xrtHttpRedirectRequest`
- `xrtHttpRedirectStatus`

### 常量与宏 (15)

- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST`
- `XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS`
- `XHTTP_FEATURE_HTTP_REDIRECT_POLICY`
- `XHTTP_REDIRECT_ALLOW_DOWNGRADE`
- `XHTTP_REDIRECT_DEFAULT`
- `XHTTP_REDIRECT_DOWNGRADE`
- `XHTTP_REDIRECT_ERROR`
- `XHTTP_REDIRECT_FOLLOW`
- `XHTTP_REDIRECT_FORWARD_CREDENTIALS`
- `XHTTP_REDIRECT_INVALID`
- `XHTTP_REDIRECT_MANUAL`
- `XHTTP_REDIRECT_MAX_DEFAULT`
- `XHTTP_REDIRECT_OK`
- `XHTTP_REDIRECT_POST_TO_GET`
- `XHTTP_REDIRECT_REPLAY`

### 类型 (3)

- `xhttpredirectconfig`
- `xhttpredirecterror`
- `xhttpredirectmode`

## `extlibs/xhttp-policy/include/xrt/http_retry_policy.h`

[查看带契约注释的公共头](../../include/xrt/http_retry_policy.h)

### 函数 (2)

- `xrtHttpRetryBackoff`
- `xrtHttpRetryStatusDefault`

### 常量与宏 (1)

- `XHTTP_FEATURE_HTTP_RETRY_POLICY`
