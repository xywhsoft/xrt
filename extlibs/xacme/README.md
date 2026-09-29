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
  - `dns_aws`（Route53，SigV4 + XML 记录集读取与原子替换；保留同名 TXT 和原 TTL）
  - `dns_huawei`（华为云 DNS v2，SDK-HMAC-SHA256）
- 自定义：宿主直接填写 `xacmednsprovider` 的 id、Add、Remove
  （可选 Propagate）字段，零注册零全局状态。

Route 53 内建 provider 只改写普通 TXT 记录集。遇到加权、地理位置等
路由策略记录集会报错，不会丢弃策略字段；单集合最多读取 400 个 TXT 值、
4 MiB 的 XML 响应。变更批次中所有 TXT 值的字符数合计受 AWS 的 32,000
字符配额约束，超限会在发送前报错。zone 发现只接受唯一公有托管区；
同名多个公有托管区或分页仍可能含同名区时会报错，避免选错区。
真实 AWS 凭据测试通过
`XACME_AWS_KEY`、`XACME_AWS_SECRET` 和 `XACME_AWS_FQDN` 显式启用。

TXT 铺设后的传播确认由签发流程统一负责：provider 带
`XACME_DNS_CAP_PROPAGATE` 时委托其自证，否则对默认公共 resolver 组
（223.5.5.5 / 119.29.29.29 / 8.8.8.8，可经客户端配置覆盖）轮询 TXT，
任一可见即触发挑战；确认超时不阻断签发（CA 只查权威侧）。

## 产物与存储布局

签发产物 `xacmeissuegrant` 恒为「链 + 配对私钥」一体交付
（私钥栈副本即刻擦除）；存储布局：

```text
<root>/accounts/<ca16>/account.pem   账户密钥（按 CA 隔离）
<root>/certs/<domain>/current        当前版本目录名
<root>/certs/<domain>/current.lock   跨进程写者锁，运行期间勿删除
<root>/certs/<domain>/.grant-<16hex>/key.pem       证书私钥
<root>/certs/<domain>/.grant-<16hex>/fullchain.pem 证书链
<root>/certs/<domain>/.grant-<16hex>/meta.txt      CA directory URL
```

`*.example.com` 的物理目录为 `%2A.example.com`，以便在 Windows 和 POSIX
使用同一布局；枚举 API 仍返回 `*.example.com`。POSIX 上早期直接使用
`*.example.com` 的版本目录继续可读；新签发会写入 `%2A.example.com`，
之后读取优先使用新目录。离线回收工具也识别这一映射；新旧目录并存时，
可在维护窗口以 `--legacy-wildcard` 显式选择旧目录回收。

`SaveGrant` 完整写入一个版本后才原子切换 `current`。读取链与私钥应调用
`LoadGrant`，或先读取一次 `current` 并固定同一目录。旧版直属文件仅在
`current` 缺失时作为兼容数据读取，续签不会继续更新它们；宿主应限制
root 的 ACL，并在确认没有读者使用旧版本后清理历史目录。
POSIX 上发布前会同步版本目录及祖先目录，发布后同步 `current` 所在目录。
如果最后一次目录同步失败，`SaveGrant` 会返回失败，但新版本可能已经可见；
宿主应调用 `LoadGrant` 核对当前状态。磁盘与文件系统断电恢复仍需部署环境验证。
账户私钥的原子替换也执行目录同步；其发布后同步失败同样需要重新读取核对。
Windows 的 `current` 用同目录临时文件和重命名替换；写者通过 `current.lock`
串行发布，并发争用造成的短暂
读取失败会限次重试。重命名已尝试而返回失败时保留版本，宿主应重新读取
当前授权；Windows 的断电持久性仍取决于系统及目标卷。

历史版本只在停止该 store 的全部读者、续签写者和直接文件消费者后回收。
从仓库根目录运行 `python tools/prune_acme_store.py --root <root> --domain example.com`
可预览；确认维护窗口后加 `--apply` 执行。默认保留当前版本和
一个最近的完整旧版，`--keep-previous` 可调整旧版保留数。工具只清理严格命名
的未引用版本目录及库自身已知的临时文件，遇到链接、未知文件或无效 `current`
会拒绝操作；它不会安全擦除磁盘上的私钥残留，也不会删除 `current.lock`。

`xrtAcmeStoreListDomains` 枚举已管域名供续签守护遍历；
`xrtAcmeClientRevoke`（RFC 8555 §7.6）提供吊销路径。
