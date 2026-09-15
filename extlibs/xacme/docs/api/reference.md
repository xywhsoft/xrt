# xacme 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xacme/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `40` 个函数、`42` 个常量或宏、
`23` 个公共类型。

## `extlibs/xacme/include/xrt/acme.h`

[查看带契约注释的公共头](../../include/xrt/acme.h)

### 函数 (2)

- `xrtAcmeAccountConfigInit`
- `xrtAcmeGrantUnit`

### 常量与宏 (21)

- `XACME_DIRECTORY_`
- `XACME_DIRECTORY_BUYPASS`
- `XACME_DIRECTORY_BUYPASS_TEST`
- `XACME_DIRECTORY_GOOGLE`
- `XACME_DIRECTORY_LE`
- `XACME_DIRECTORY_LE_STAGING`
- `XACME_DIRECTORY_ZEROSSL`
- `XACME_ERROR_ACCOUNT`
- `XACME_ERROR_ARGUMENT`
- `XACME_ERROR_CHALLENGE`
- `XACME_ERROR_CONFIG`
- `XACME_ERROR_DIRECTORY`
- `XACME_ERROR_DNS`
- `XACME_ERROR_FINALIZE`
- `XACME_ERROR_NETWORK`
- `XACME_ERROR_ORDER`
- `XACME_ERROR_PROTOCOL`
- `XACME_ERROR_RATE_LIMIT`
- `XACME_ERROR_STORE`
- `XACME_FEATURE_ACME_CORE`
- `XACME_FEATURE_ACME_DNS`

### 类型 (6)

- `xacme`
- `xacmeaccountconfig`
- `xacmeeab`
- `xacmeerror`
- `xacmeissuegrant`
- `xrt`

## `extlibs/xacme/include/xrt/acme_client.h`

[查看带契约注释的公共头](../../include/xrt/acme_client.h)

### 函数 (9)

- `xrtAcmeClientAccountPem`
- `xrtAcmeClientConfigInit`
- `xrtAcmeClientCreate`
- `xrtAcmeClientDestroy`
- `xrtAcmeClientIssue`
- `xrtAcmeClientIssueEx`
- `xrtAcmeClientIssueStored`
- `xrtAcmeClientRevoke`
- `xrtAcmeClientRollover`

### 常量与宏 (6)

- `XACME_FEATURE_ACME_CSR`
- `XACME_FEATURE_ACME_FLOW`
- `XACME_FEATURE_ACME_HTTP`
- `XACME_FEATURE_ACME_JOSE`
- `XACME_FEATURE_ACME_STORE`
- `XACME_FEATURE_DNS_ALI`

### 类型 (5)

- `xacmeclient`
- `xacmeclientconfig`
- `xacmednsprovider`
- `xnetengine`
- `xstrview`

## `extlibs/xacme/include/xrt/acme_dns.h`

[查看带契约注释的公共头](../../include/xrt/acme_dns.h)

### 函数 (3)

- `xrtAcmeDnsProviderCount`
- `xrtAcmeDnsProviderId`
- `xrtAcmeDnsProviderValidate`

### 常量与宏 (6)

- `XACME_DNS_CAP_PROPAGATE`
- `XACME_DNS_ERROR_ARGUMENT`
- `XACME_DNS_ERROR_CREDENTIAL`
- `XACME_DNS_ERROR_NETWORK`
- `XACME_DNS_ERROR_PROTOCOL`
- `XACME_DNS_ERROR_ZONE`

### 类型 (4)

- `xacmednsaddproc`
- `xacmednserror`
- `xacmednspropagateproc`
- `xacmednsremoveproc`

## `extlibs/xacme/include/xrt/acme_dns_ali.h`

[查看带契约注释的公共头](../../include/xrt/acme_dns_ali.h)

### 函数 (3)

- `xrtAcmeDnsAli`
- `xrtAcmeDnsAliConfigInit`
- `xrtAcmeDnsAliProviderUnit`

### 类型 (1)

- `xacmednaliconfig`

## `extlibs/xacme/include/xrt/acme_dns_aws.h`

[查看带契约注释的公共头](../../include/xrt/acme_dns_aws.h)

### 函数 (3)

- `xrtAcmeDnsAws`
- `xrtAcmeDnsAwsConfigInit`
- `xrtAcmeDnsAwsProviderUnit`

### 常量与宏 (1)

- `XACME_FEATURE_DNS_AWS`

### 类型 (1)

- `xacmednsawsconfig`

## `extlibs/xacme/include/xrt/acme_dns_cf.h`

[查看带契约注释的公共头](../../include/xrt/acme_dns_cf.h)

### 函数 (3)

- `xrtAcmeDnsCf`
- `xrtAcmeDnsCfConfigInit`
- `xrtAcmeDnsCfProviderUnit`

### 常量与宏 (1)

- `XACME_FEATURE_DNS_CF`

### 类型 (1)

- `xacmednscfconfig`

## `extlibs/xacme/include/xrt/acme_dns_huawei.h`

[查看带契约注释的公共头](../../include/xrt/acme_dns_huawei.h)

### 函数 (3)

- `xrtAcmeDnsHuawei`
- `xrtAcmeDnsHuaweiConfigInit`
- `xrtAcmeDnsHuaweiProviderUnit`

### 常量与宏 (1)

- `XACME_FEATURE_DNS_HUAWEI`

### 类型 (1)

- `xacmednshuaaweiconfig`

## `extlibs/xacme/include/xrt/acme_dns_tencent.h`

[查看带契约注释的公共头](../../include/xrt/acme_dns_tencent.h)

### 函数 (3)

- `xrtAcmeDnsTencent`
- `xrtAcmeDnsTencentConfigInit`
- `xrtAcmeDnsTencentProviderUnit`

### 常量与宏 (1)

- `XACME_FEATURE_DNS_TENCENT`

### 类型 (1)

- `xacmednstencentconfig`

## `extlibs/xacme/include/xrt/acme_obtain.h`

[查看带契约注释的公共头](../../include/xrt/acme_obtain.h)

### 函数 (2)

- `xrtAcmeObtain`
- `xrtAcmeObtainConfigInit`

### 常量与宏 (1)

- `XACME_FEATURE_ACME_OBTAIN`

### 类型 (1)

- `xacmeobtainconfig`

## `extlibs/xacme/include/xrt/acme_store.h`

[查看带契约注释的公共头](../../include/xrt/acme_store.h)

### 函数 (9)

- `xrtAcmeStoreListDomains`
- `xrtAcmeStoreLoadAccount`
- `xrtAcmeStoreLoadCert`
- `xrtAcmeStoreLoadCertCa`
- `xrtAcmeStoreLoadGrant`
- `xrtAcmeStoreNeedRenew`
- `xrtAcmeStoreSaveAccount`
- `xrtAcmeStoreSaveCert`
- `xrtAcmeStoreSaveGrant`

### 常量与宏 (4)

- `XACME_STORE_ERROR_ARGUMENT`
- `XACME_STORE_ERROR_IO`
- `XACME_STORE_ERROR_NOT_FOUND`
- `XACME_STORE_ERROR_PARSE`

### 类型 (2)

- `x509`
- `xacmestoreerror`
