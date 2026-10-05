# xoauth2 public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xoauth2/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `27` 个函数、`15` 个常量或宏、
`5` 个公共类型。

## `extlibs/xoauth2/include/xoauth2/api.h`

[查看带契约注释的公共头](../../include/xoauth2/api.h)

### 函数 (27)

- `xoauth2BeginLogin`
- `xoauth2ClientUnit`
- `xoauth2CompleteLogin`
- `xoauth2ConfigInit`
- `xoauth2GetUserInfo`
- `xoauth2GetWechatUserInfo`
- `xoauth2HttpGet`
- `xoauth2HttpXrt`
- `xoauth2HttpXrtCleanup`
- `xoauth2HttpXrtCleanupPending`
- `xoauth2HttpXrtCreate`
- `xoauth2HttpXrtDestroy`
- `xoauth2HttpXrtInit`
- `xoauth2HttpXrtUnit`
- `xoauth2LastError`
- `xoauth2NonceConsume`
- `xoauth2PkceGenerate`
- `xoauth2Refresh`
- `xoauth2StateGenerate`
- `xoauth2TokenExpiring`
- `xoauth2TokenFree`
- `xoauth2UrlEncode`
- `xoauth2UseCustom`
- `xoauth2UseGithub`
- `xoauth2UseGoogle`
- `xoauth2UseMicrosoft`
- `xoauth2UseWechat`

### 常量与宏 (15)

- `XOAUTH2_AUTH_AUTO`
- `XOAUTH2_AUTH_BASIC`
- `XOAUTH2_AUTH_BODY`
- `XOAUTH2_ERROR_ARGUMENT`
- `XOAUTH2_ERROR_NETWORK`
- `XOAUTH2_ERROR_NONCE_MISMATCH`
- `XOAUTH2_ERROR_REFRESH`
- `XOAUTH2_ERROR_STATE_MISMATCH`
- `XOAUTH2_ERROR_TOKEN_DENIED`
- `XOAUTH2_ERROR_TOKEN_ENDPOINT`
- `XOAUTH2_ERROR_TOKEN_RESPONSE`
- `XOAUTH2_FEATURE_XOAUTH2`
- `XOAUTH2_VERSION_MAJOR`
- `XOAUTH2_VERSION_MINOR`
- `XOAUTH2_VERSION_PATCH`

### 类型 (5)

- `xoauth2client`
- `xoauth2config`
- `xoauth2httpproc`
- `xoauth2httpxrt`
- `xoauth2token`
