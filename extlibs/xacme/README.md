# xacme

`xacme` 是构建在 xrt 核心之上的 ACME（RFC 8555）客户端扩展库。
它把账户、订单、dns-01 挑战与证书获取做成可组合的 C API：宿主传入
CA directory、DNS provider 与凭据、存储位置，换取证书链与有效期。
它不是工具：没有配置文件、没有定时器、没有 reload 钩子——一切由
宿主经参数与回调组合。单头形态为 `single/xacme.h`。

## 模块选择（三态）

宏必须在包含任何 xacme 头之前定义；实现细节见 `include/xacme/features.h`。

```c
/* 默认：全部内建 DNS provider */
#define XACME_IMPLEMENTATION
#include "xacme.h"

/* 全量但剔除个别 */
#define XACME_NO_DNS_AWS
#define XACME_IMPLEMENTATION
#include "xacme.h"

/* 白名单：点名任意 provider 即退出默认全量 */
#define XACME_MODULE_DNS_ALI
#define XACME_MODULE_DNS_CF
#define XACME_IMPLEMENTATION
#include "xacme.h"
```

## 构建与测试

```text
python tools/build.py --manifest extlibs/xacme/config/modules.json --suite xacme --no-single
```

套件：`acme_dns`（接口层契约）、`acme_core`（账户配置）、`dns_ali`
（provider 构造器）、`xacme`（组合根与注册表）。

## CA 中立

任何 RFC 8555 directory 均可（`xacmeaccountconfig.sDirectoryUrl`），
内置 LE / ZeroSSL / Google / Buypass 预设常量；EAB 走 `xacmeeab`。
切换 CA = 换参数，不换构建。

## DNS provider

- 内建：`dns_ali`（阿里云 alidns，V3 签名 ACS3-HMAC-SHA256）。
  规划中：`dns_cf`、`dns_tencent`（TC3）、`dns_aws`、`dns_huawei`。
- 自定义：宿主直接填写 `xacmednsprovider` 的 id、Add、Remove
  （可选 Propagate）字段，零注册零全局状态。

## 路线图

1. JOSE/JWK（ES256：JWK、thumbprint、JWS 保护头）
2. CSR（PKCS#10）与密钥序列化（SEC1/PKCS#8 → PEM 写侧）
3. 内部 HTTPS 传输（xrt 核心 http1 + TLS 拨号；代理经 xnetproxyconfig）
4. ACME 流程（directory/账户/订单/挑战轮询/finalize/下载；Pebble 回归）
5. dns_txt 传播探测（迷你 UDP TXT 查权威 NS）
6. dns_ali V3 传输真实联测（test.xxrpa.com）
7. store 文件实现（xrtFileWriteAtomic）与续签辅助
