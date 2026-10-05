# xjwt public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xjwt/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `24` 个函数、`23` 个常量或宏、
`4` 个公共类型。

## `extlibs/xjwt/include/xjwt/api.h`

[查看带契约注释的公共头](../../include/xjwt/api.h)

### 函数 (24)

- `xjwtAlgName`
- `xjwtAlgParse`
- `xjwtCheckInit`
- `xjwtClaimString`
- `xjwtClaimsValid`
- `xjwtConfigInit`
- `xjwtDecode`
- `xjwtDecodeHeader`
- `xjwtEs256`
- `xjwtHs256`
- `xjwtHs384`
- `xjwtHs512`
- `xjwtJwksFree`
- `xjwtJwksParse`
- `xjwtKeyFree`
- `xjwtKeyParse`
- `xjwtLastError`
- `xjwtRs256`
- `xjwtRs384`
- `xjwtRs512`
- `xjwtSign`
- `xjwtVerify`
- `xjwtVerifyJwks`
- `xjwtVerifyKey`

### 常量与宏 (23)

- `XJWT_ALG_ES256`
- `XJWT_ALG_HS256`
- `XJWT_ALG_HS384`
- `XJWT_ALG_HS512`
- `XJWT_ALG_INVALID`
- `XJWT_ALG_NONE`
- `XJWT_ALG_RS256`
- `XJWT_ALG_RS384`
- `XJWT_ALG_RS512`
- `XJWT_ERROR_ALG_MISMATCH`
- `XJWT_ERROR_ARGUMENT`
- `XJWT_ERROR_AUDIENCE`
- `XJWT_ERROR_EXPIRED`
- `XJWT_ERROR_ISSUER`
- `XJWT_ERROR_KEY_NOT_FOUND`
- `XJWT_ERROR_MALFORMED`
- `XJWT_ERROR_NOT_YET`
- `XJWT_ERROR_PARSE`
- `XJWT_ERROR_SIGNATURE`
- `XJWT_FEATURE_XJWT`
- `XJWT_VERSION_MAJOR`
- `XJWT_VERSION_MINOR`
- `XJWT_VERSION_PATCH`

### 类型 (4)

- `xjwtcheck`
- `xjwtconfig`
- `xjwtjwks`
- `xjwtkey`
