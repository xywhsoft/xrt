# HTTP 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[http.md](http.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `118` 个函数、`132` 个常量或宏、
`41` 个公共类型。

## `include/xrt/http.h`

[查看带契约注释的公共头](../../include/xrt/http.h)

### 函数 (58)

- `xrtHttpAuthorityPort`
- `xrtHttpAuthorityValid`
- `xrtHttpContentLengthParse`
- `xrtHttpDirectiveCount`
- `xrtHttpDirectiveFind`
- `xrtHttpDirectiveNext`
- `xrtHttpFieldBlockCount`
- `xrtHttpFieldBlockWrite`
- `xrtHttpFieldCount`
- `xrtHttpFieldFind`
- `xrtHttpFieldGet`
- `xrtHttpFieldGetUnique`
- `xrtHttpFieldNameEqual`
- `xrtHttpFieldNext`
- `xrtHttpFieldParse`
- `xrtHttpFieldTokenCount`
- `xrtHttpFieldTokenCursorInit`
- `xrtHttpFieldTokenFind`
- `xrtHttpFieldTokenNext`
- `xrtHttpFieldValueValid`
- `xrtHttpFieldWrite`
- `xrtHttpHostEqual`
- `xrtHttpHostParse`
- `xrtHttpHostValid`
- `xrtHttpIpv4Valid`
- `xrtHttpIpv6Valid`
- `xrtHttpMethodEqual`
- `xrtHttpMethodIdempotent`
- `xrtHttpMethodSafe`
- `xrtHttpOwsTrim`
- `xrtHttpParamBuild`
- `xrtHttpParamCount`
- `xrtHttpParamFind`
- `xrtHttpParamHostValid`
- `xrtHttpParamNext`
- `xrtHttpParamTokenEqual`
- `xrtHttpParamTokenValid`
- `xrtHttpParamValueCursorInit`
- `xrtHttpParamValueNext`
- `xrtHttpParamValueWrite`
- `xrtHttpParamWrite`
- `xrtHttpQualityParse`
- `xrtHttpQuotedBuild`
- `xrtHttpQuotedRead`
- `xrtHttpQuotedValid`
- `xrtHttpQuotedWrite`
- `xrtHttpResponseContentAllowed`
- `xrtHttpStatusText`
- `xrtHttpTargetAuthority`
- `xrtHttpTargetParse`
- `xrtHttpTokenEqual`
- `xrtHttpTokenListBuild`
- `xrtHttpTokenListCount`
- `xrtHttpTokenListHas`
- `xrtHttpTokenListWrite`
- `xrtHttpTokenNext`
- `xrtHttpTokenValid`
- `xrtHttpWeightedTokenNext`

### 常量与宏 (81)

- `XHTTP_AUTHORITY_HAS_PORT`
- `XHTTP_AUTHORITY_IP_LITERAL`
- `XHTTP_AUTHORITY_PORT_EMPTY`
- `XHTTP_AUTHORITY_PORT_VALUE`
- `XHTTP_NEXT_END`
- `XHTTP_NEXT_ERROR`
- `XHTTP_NEXT_ITEM`
- `XHTTP_PARAM_HAS_VALUE`
- `XHTTP_PARAM_NONE`
- `XHTTP_PARAM_QUOTED`
- `XHTTP_QUALITY_MAX`
- `XHTTP_STATUS_ACCEPTED`
- `XHTTP_STATUS_ALREADY_REPORTED`
- `XHTTP_STATUS_BAD_GATEWAY`
- `XHTTP_STATUS_BAD_REQUEST`
- `XHTTP_STATUS_CONFLICT`
- `XHTTP_STATUS_CONTENT_TOO_LARGE`
- `XHTTP_STATUS_CONTINUE`
- `XHTTP_STATUS_CREATED`
- `XHTTP_STATUS_EARLY_HINTS`
- `XHTTP_STATUS_EXPECTATION_FAILED`
- `XHTTP_STATUS_FAILED_DEPENDENCY`
- `XHTTP_STATUS_FORBIDDEN`
- `XHTTP_STATUS_FOUND`
- `XHTTP_STATUS_GATEWAY_TIMEOUT`
- `XHTTP_STATUS_GONE`
- `XHTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED`
- `XHTTP_STATUS_IM_USED`
- `XHTTP_STATUS_INSUFFICIENT_STORAGE`
- `XHTTP_STATUS_INTERNAL_SERVER_ERROR`
- `XHTTP_STATUS_LENGTH_REQUIRED`
- `XHTTP_STATUS_LOCKED`
- `XHTTP_STATUS_LOOP_DETECTED`
- `XHTTP_STATUS_METHOD_NOT_ALLOWED`
- `XHTTP_STATUS_MISDIRECTED_REQUEST`
- `XHTTP_STATUS_MOVED_PERMANENTLY`
- `XHTTP_STATUS_MULTIPLE_CHOICES`
- `XHTTP_STATUS_MULTI_STATUS`
- `XHTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED`
- `XHTTP_STATUS_NON_AUTHORITATIVE_INFORMATION`
- `XHTTP_STATUS_NOT_ACCEPTABLE`
- `XHTTP_STATUS_NOT_EXTENDED`
- `XHTTP_STATUS_NOT_FOUND`
- `XHTTP_STATUS_NOT_IMPLEMENTED`
- `XHTTP_STATUS_NOT_MODIFIED`
- `XHTTP_STATUS_NO_CONTENT`
- `XHTTP_STATUS_OK`
- `XHTTP_STATUS_PARTIAL_CONTENT`
- `XHTTP_STATUS_PAYMENT_REQUIRED`
- `XHTTP_STATUS_PERMANENT_REDIRECT`
- `XHTTP_STATUS_PRECONDITION_FAILED`
- `XHTTP_STATUS_PRECONDITION_REQUIRED`
- `XHTTP_STATUS_PROCESSING`
- `XHTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED`
- `XHTTP_STATUS_RANGE_NOT_SATISFIABLE`
- `XHTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE`
- `XHTTP_STATUS_REQUEST_TIMEOUT`
- `XHTTP_STATUS_RESET_CONTENT`
- `XHTTP_STATUS_SEE_OTHER`
- `XHTTP_STATUS_SERVICE_UNAVAILABLE`
- `XHTTP_STATUS_SWITCHING_PROTOCOLS`
- `XHTTP_STATUS_TEMPORARY_REDIRECT`
- `XHTTP_STATUS_TOO_EARLY`
- `XHTTP_STATUS_TOO_MANY_REQUESTS`
- `XHTTP_STATUS_UNAUTHORIZED`
- `XHTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS`
- `XHTTP_STATUS_UNPROCESSABLE_CONTENT`
- `XHTTP_STATUS_UNSUPPORTED_MEDIA_TYPE`
- `XHTTP_STATUS_UPGRADE_REQUIRED`
- `XHTTP_STATUS_URI_TOO_LONG`
- `XHTTP_STATUS_USE_PROXY`
- `XHTTP_STATUS_VARIANT_ALSO_NEGOTIATES`
- `XHTTP_TARGET_ABSOLUTE`
- `XHTTP_TARGET_ASTERISK`
- `XHTTP_TARGET_AUTHORITY`
- `XHTTP_TARGET_HAS_AUTHORITY`
- `XHTTP_TARGET_HAS_QUERY`
- `XHTTP_TARGET_HAS_SCHEME`
- `XHTTP_TARGET_ORIGIN`
- `XHTTP_VERSION_1_0`
- `XHTTP_VERSION_1_1`

### 类型 (12)

- `xhttpauthority`
- `xhttpfield`
- `xhttpfieldtokencursor`
- `xhttpnext`
- `xhttpparam`
- `xhttpparamflags`
- `xhttpparamvaluecursor`
- `xhttpstatus`
- `xhttptarget`
- `xhttptargetform`
- `xhttpversion`
- `xhttpweightedtoken`

## `include/xrt/http_connection.h`

[查看带契约注释的公共头](../../include/xrt/http_connection.h)

### 函数 (5)

- `xrtHttpConnectionCount`
- `xrtHttpConnectionCursorInit`
- `xrtHttpConnectionFind`
- `xrtHttpConnectionNext`
- `xrtHttpConnectionPersistence`

### 常量与宏 (6)

- `XHTTP_CONNECTION_ALLOW_HTTP10_KEEP_ALIVE`
- `XHTTP_CONNECTION_CLOSE`
- `XHTTP_CONNECTION_ERROR`
- `XHTTP_CONNECTION_PERSIST`
- `XHTTP_CONNECTION_PROXY`
- `XHTTP_CONNECTION_RESPONSE`

### 类型 (2)

- `xhttpconnectionflag`
- `xhttpconnectionstatus`

## `include/xrt/http_decode.h`

[查看带契约注释的公共头](../../include/xrt/http_decode.h)

### 函数 (9)

- `xrtHttpDecodeConfigInit`
- `xrtHttpDecodeCreate`
- `xrtHttpDecodeDestroy`
- `xrtHttpDecodeDone`
- `xrtHttpDecodeInputSize`
- `xrtHttpDecodeMode`
- `xrtHttpDecodeOutputSize`
- `xrtHttpDecodeReset`
- `xrtHttpDecodeWrite`

### 常量与宏 (12)

- `XHTTP_DECODE_ALLOW_RAW`
- `XHTTP_DECODE_CONTENT`
- `XHTTP_DECODE_ERROR_ARGUMENT`
- `XHTTP_DECODE_ERROR_CONFIG`
- `XHTTP_DECODE_ERROR_CONTENT_ENCODING`
- `XHTTP_DECODE_ERROR_LIMIT`
- `XHTTP_DECODE_ERROR_OUTPUT`
- `XHTTP_DECODE_ERROR_STATE`
- `XHTTP_DECODE_ERROR_UNSUPPORTED`
- `XHTTP_DECODE_IDENTITY`
- `XHTTP_DECODE_OUTPUT_UNLIMITED`
- `XHTTP_DECODE_RAW`

### 类型 (6)

- `xhttpdecode`
- `xhttpdecodeconfig`
- `xhttpdecodeerror`
- `xhttpdecodeflag`
- `xhttpdecodemode`
- `xhttpdecodeoutputproc`

## `include/xrt/http_encoding.h`

[查看带契约注释的公共头](../../include/xrt/http_encoding.h)

### 函数 (12)

- `xrtHttpAcceptEncodingAdd`
- `xrtHttpAcceptEncodingInit`
- `xrtHttpAcceptEncodingParse`
- `xrtHttpAcceptEncodingQuality`
- `xrtHttpAcceptEncodingSelect`
- `xrtHttpAcceptEncodingValid`
- `xrtHttpCodingName`
- `xrtHttpCodingParse`
- `xrtHttpContentEncodingCursorInit`
- `xrtHttpContentEncodingNext`
- `xrtHttpContentEncodingPlan`
- `xrtHttpContentEncodingWrite`

### 常量与宏 (17)

- `XHTTP_ACCEPT_ENCODING_DEFLATE`
- `XHTTP_ACCEPT_ENCODING_GZIP`
- `XHTTP_ACCEPT_ENCODING_IDENTITY`
- `XHTTP_ACCEPT_ENCODING_NONE`
- `XHTTP_ACCEPT_ENCODING_PRESENT`
- `XHTTP_ACCEPT_ENCODING_WILDCARD`
- `XHTTP_CODING_DEFLATE`
- `XHTTP_CODING_GZIP`
- `XHTTP_CODING_IDENTITY`
- `XHTTP_CODING_NONE`
- `XHTTP_CONTENT_CODINGS_DEFAULT`
- `XHTTP_CONTENT_CODINGS_MAX`
- `XHTTP_CONTENT_ENCODING_IDENTITY`
- `XHTTP_CONTENT_ENCODING_LEGACY`
- `XHTTP_CONTENT_ENCODING_NONE`
- `XHTTP_CONTENT_ENCODING_PRESENT`
- `XHTTP_CONTENT_ENCODING_UNKNOWN`

### 类型 (7)

- `xhttpacceptencoding`
- `xhttpacceptencodingflag`
- `xhttpcoding`
- `xhttpcontentencodingcursor`
- `xhttpcontentencodingflag`
- `xhttpcontentencodingitem`
- `xhttpcontentencodingplan`

## `include/xrt/http_expect.h`

[查看带契约注释的公共头](../../include/xrt/http_expect.h)

### 函数 (8)

- `xrtHttpExpectCount`
- `xrtHttpExpectCursorInit`
- `xrtHttpExpectFieldCursorInit`
- `xrtHttpExpectFieldNext`
- `xrtHttpExpectFields`
- `xrtHttpExpectNext`
- `xrtHttpExpectValid`
- `xrtHttpExpectationParse`

### 常量与宏 (8)

- `XHTTP_EXPECT_BARE`
- `XHTTP_EXPECT_CONTINUE`
- `XHTTP_EXPECT_ERROR`
- `XHTTP_EXPECT_HAS_PARAMETERS`
- `XHTTP_EXPECT_HAS_VALUE`
- `XHTTP_EXPECT_NONE`
- `XHTTP_EXPECT_UNSUPPORTED`
- `XHTTP_EXPECT_VALUE_QUOTED`

### 类型 (5)

- `xhttpexpectation`
- `xhttpexpectcursor`
- `xhttpexpectfieldcursor`
- `xhttpexpectflag`
- `xhttpexpectresult`

## `include/xrt/http_te.h`

[查看带契约注释的公共头](../../include/xrt/http_te.h)

### 函数 (10)

- `xrtHttpTeAcceptsTrailers`
- `xrtHttpTeCodingParse`
- `xrtHttpTeCount`
- `xrtHttpTeCursorInit`
- `xrtHttpTeFieldCursorInit`
- `xrtHttpTeFieldNext`
- `xrtHttpTeNext`
- `xrtHttpTeParse`
- `xrtHttpTeQuality`
- `xrtHttpTeValid`

### 常量与宏 (8)

- `XHTTP_TE_ACCEPTS_TRAILERS`
- `XHTTP_TE_CODING_HAS_PARAMETERS`
- `XHTTP_TE_CODING_HAS_WEIGHT`
- `XHTTP_TE_CODING_NONE`
- `XHTTP_TE_CODING_TRAILERS`
- `XHTTP_TE_HAS_TRANSFER_CODINGS`
- `XHTTP_TE_NONE`
- `XHTTP_TE_PRESENT`

### 类型 (6)

- `xhttptecoding`
- `xhttptecodingflag`
- `xhttptecursor`
- `xhttptefieldcursor`
- `xhttpteflag`
- `xhttpteinfo`

## `include/xrt/http_trailer.h`

[查看带契约注释的公共头](../../include/xrt/http_trailer.h)

### 函数 (6)

- `xrtHttpTrailerCount`
- `xrtHttpTrailerFind`
- `xrtHttpTrailerNameValid`
- `xrtHttpTrailerNamesBuild`
- `xrtHttpTrailerNamesWrite`
- `xrtHttpTrailerSectionValid`

## `include/xrt/http_upgrade.h`

[查看带契约注释的公共头](../../include/xrt/http_upgrade.h)

### 函数 (10)

- `xrtHttpUpgradeBuild`
- `xrtHttpUpgradeCount`
- `xrtHttpUpgradeCursorInit`
- `xrtHttpUpgradeElementWrite`
- `xrtHttpUpgradeFieldCursorInit`
- `xrtHttpUpgradeFieldNext`
- `xrtHttpUpgradeNext`
- `xrtHttpUpgradeParse`
- `xrtHttpUpgradeValid`
- `xrtHttpUpgradeWrite`

### 类型 (3)

- `xhttpupgradecursor`
- `xhttpupgradefieldcursor`
- `xhttpupgradeitem`
