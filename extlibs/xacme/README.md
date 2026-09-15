# xacme

`xacme` 是构建在 xrt 核心之上的 ACME（RFC 8555）客户端扩展库。
它把账户、订单、dns-01 挑战与证书获取做成可组合的 C API：宿主传入
CA directory、DNS provider 与凭据、存储位置，换取**证书链与配对私钥**。
它不是工具：没有配置文件、没有定时器、没有 reload 钩子——一切由
宿主经参数与回调组合。单头形态为 `single/xacme.h`。

## 最短路径（一站式申领）

```c
#include <xrt/acme_obtain.h>

xacmeaccountconfig Account;
xacmeobtainconfig Obtain;
xacmednsprovider Dns;

xrtAcmeAccountConfigInit(&Account);
Account.sDirectoryUrl = XACME_DIRECTORY_LE;   /* 或 ZeroSSL/Google + EAB */
Account.sContactEmail = "ops@example.com";

xrtAcmeObtainConfigInit(&Obtain);
Obtain.pAccount  = &Account;
Obtain.sStoreRoot = "/var/lib/myapp/acme";    /* 账户与证书落盘根 */

/* provider 按需选择一家：Ali / CF / Tencent / AWS / Huawei */
xacmednscfconfig Cf;
xrtAcmeDnsCfConfigInit(&Cf);
Cf.sApiToken = "...";
xrtAcmeDnsCf(&Cf, NULL, &Dns);

xacmeissuegrant Grant;
bool bRenewed;
if(xrtAcmeObtain(&Obtain, Domains, 1u, &Dns, &Grant, &bRenewed))
{
	/* Grant.sFullchainPem + Grant.sKeyPem 可直接喂 TLS 服务器 */
	xrtAcmeGrantUnit(&Grant);
}
```

阈值内重复调用直接复用本地证书（`bRenewed=false`），过期自动重签；
账户密钥按 CA 隔离持久化复用（`accounts/<ca16>/account.pem`）。
需要更细的控制（多 CA 并存、自定义传播确认 resolver、吊销）时用
`xrt/acme_client.h` 的客户端对象。

## 模块选择

宏必须在包含任何 xacme 头之前定义；
`include/xacme/features.h` 由 `tools/generate_extension_features.py`
按清单生成（与其他扩展同款语义）。

```c
/* 默认：全量（全部 provider 与流程） */
#define XACME_IMPLEMENTATION
#include "xacme.h"

/* 点名单个模块（自动带入其依赖闭包） */
#define XACME_MODULE_ACME_OBTAIN
#define XACME_IMPLEMENTATION
#include "xacme.h"
```

## 构建与测试

```text
python tools/build.py --manifest extlibs/xacme/config/modules.json --suite xacme_tests
```

19 个测试常跑（协议向量为 openssl/python 独立固化）；Pebble/challtestsrv
集成与 LE staging、各云厂商真实 API 联测由环境变量门控
（`XACME_PEBBLE_URL` / `XACME_LIVE` / `XACME_ALI_KEY` / `XACME_CF_TOKEN` /
`XACME_TENCENT_ID` / `XACME_AWS_KEY` / `XACME_HUAWEI_AK` 等）。

## CA 中立

任何 RFC 8555 directory 均可（`xacmeaccountconfig.sDirectoryUrl`），
内置 LE / LE staging / ZeroSSL / Google / Buypass 预设常量；
需要 EAB 的 CA 经 `xacmeeab`（Kid + base64url HMAC，RFC 8555 §7.3.4）
在开户时自动绑定，联系方式走 `sContactEmail`。
切换 CA = 换参数，不换构建。

## DNS provider

- 内建五家：
  - `dns_ali`（阿里云 alidns，ACS3-HMAC-SHA256）
  - `dns_cf`（Cloudflare v4，Bearer API Token）
  - `dns_tencent`（DNSPod API 3.0，TC3-HMAC-SHA256）
  - `dns_aws`（Route53，SigV4 + XML UPSERT/DELETE）
  - `dns_huawei`（华为云 DNS v2，SDK-HMAC-SHA256）
- 自定义：宿主直接填写 `xacmednsprovider` 的 id、Add、Remove
  （可选 Propagate）字段，零注册零全局状态。

TXT 铺设后的传播确认由签发流程统一负责：provider 带
`XACME_DNS_CAP_PROPAGATE` 时委托其自证，否则对默认公共 resolver 组
（223.5.5.5 / 119.29.29.29 / 8.8.8.8，可经客户端配置覆盖）轮询 TXT，
任一可见即触发挑战；确认超时不阻断签发（CA 只查权威侧）。

## 产物与存储布局

签发产物 `xacmeissuegrant` 恒为「链 + 配对私钥」一体交付
（私钥栈副本即刻擦除）；存储布局：

```text
<root>/accounts/<ca16>/account.pem   账户密钥（按 CA 隔离）
<root>/certs/<domain>/key.pem        证书私钥（敏感，宿主管目录权限）
<root>/certs/<domain>/fullchain.pem  证书链
<root>/certs/<domain>/meta.txt       directory=<CA directory URL>
```

`xrtAcmeStoreListDomains` 枚举已管域名供续签守护遍历；
`xrtAcmeClientRevoke`（RFC 8555 §7.6）提供吊销路径。
