# xacme

`xacme` 是构建在 xrt 核心之上的 ACME（RFC 8555）客户端扩展库。
它把账户、订单、dns-01 挑战与证书获取做成可组合的 C API：宿主传入
CA directory、DNS provider 与凭据、存储位置，换取**证书链与配对私钥**。
它不是工具：没有配置文件、没有定时器、没有 reload 钩子——一切由
宿主经参数与回调组合。单头形态为 `single/extlibs/xacme.h`。

单头实现与声明分别为仓库根目录的 `single/extlibs/xacme.h` 与
`single/extlibs/xacme_decl.h`，均只包含 xacme 自身代码。使用前须按顺序提供
XRT 的所需模块，再包含 xacme；实现宏为 `XACME_IMPLEMENTATION`。
依赖选择与实现组合见 [构建说明](../../docs/BUILD.md#扩展单头与依赖顺序)。

## 最短路径（一站式申领）

```c
#include <xrt/acme_obtain.h>

xacmeaccountconfig Account;
xacmeobtainconfig Obtain;
xacmednsprovider Dns = { 0 };

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
if(xrtAcmeDnsCf(&Cf, NULL, &Dns))
{
	xacmeissuegrant Grant;
	bool bRenewed;
	if(xrtAcmeObtain(&Obtain, Domains, 1u, &Dns, &Grant, &bRenewed))
	{
		/* Grant.sFullchainPem + Grant.sKeyPem 可直接喂 TLS 服务器 */
		xrtAcmeGrantUnit(&Grant);
	}
	/* Dns 留给宿主清理；完成条件与退出示例见下方。 */
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
python tools/test_acme_http_faults.py
python tools/test_acme_http_interop.py    # 独立 HTTP/TLS 边界与分配失败回归
python tools/test_acme_provider_wire.py   # 也已纳入上项默认入口
python -m unittest tools.test_acme_mock -v
python tools/measure_acme_coverage.py     # 模块套件 + 模拟 CA + 全部独立探针
```

覆盖率工具额外依赖 Python `cryptography`，默认重建全部 16 个自有 `.c` 文件，
执行全量模拟 CA 场景且不允许跳过，并合并独立探针的 gcov JSON 原始计数。
部分探针只编译传输或 provider 文件；工具检查每份探针的实际编译范围、
相同文件的控制流身份，以及最终分母包含全部 16 个文件，避免重复或漏计。
测量前先核验 Core 与 ACME 单头生成结果，避免合并来自不同运行时版本的计数。
源码、Core、生成头文件、测试及相关构建脚本在运行期间变化会使测量失败。
报告保存在 `out/acme/coverage/{win32,linux}/coverage.json`，含精确计数、
未触达位置、各探针新增触达量和输入 SHA256；编译产物与计数保留供复核，
证书和存储夹具使用临时目录。默认防倒退门槛为行 70%/分支结果 55%，
这些门槛不表示已经通过生产验收。内部头文件中的 static 实现使用
`python tools/measure_extension_header_coverage.py --product xacme` 单独统计；
同一命令追加 `--report-only` 可核对主报告及原始对象计数的输入绑定。
结果为 `out/acme/coverage/{win32,linux}/header-functions.json`，分母不混入
自有 C 报告。
`--module-only` 仅诊断模块基线，不运行模拟 CA 与独立探针。
WSL 应使用 `--temp-root /tmp`，保证 POSIX 私钥权限测试位于原生文件系统。

HTTP 故障脚本用本地服务端验证响应体截断、超时后的资源清理、GET 断流重试、
已发送 POST 的结果未知/不重放，以及 8 MiB 慢读 POST 的整次发送截止时间；
独立互操作脚本使用 OpenSSL 临时签发的证书与 Python TLS 服务端，覆盖
100/101 个响应字段与 trailer、4 MiB 正文边界、原始 NUL 拒绝、正常 EOF
与截断的区分，以及 8 MiB POST 部分上传后的超时/断流。正文携带约 8 MiB
chunk 扩展时要求接收内存有界，完整复制响应头后才复用存储解析 trailer。
五个响应字段分别注入分配失败，禁止把缺少字段的响应作为成功结果交付；
每项独立场景在销毁传输后检查活动分配归零和原始错误保留。
请求侧验证 94 个附加字段与自动生成字段共计 100 个时全部发出，超限、
空参数以及覆盖传输层管理的字段会在发请求前拒绝。
这项脚本无需真实账号，依赖 Python、GCC 和 OpenSSL 命令；Linux 可加
`--sanitize`，可用 `--case eof` 等选择分组。HTTP/TLS 共 40 项（非法 URL 的
79 样本与合法端口/容量向量各计一组，IPv6 不可用时少一项）；默认还运行
HTTP 生命周期及客户端/provider 拥有权回归，见下方清理契约。
`--case ownership` 还执行内建 provider 的 135 项精确错误断言：空指针、
成功清理后的缓存回调、退休超时/错误及不完整传输均在任何记录操作前拒绝；
检查已拥有记录与未跟踪 Remove，确认无 HTTP、签名、nonce 或记录修改。
另用公开聚合头执行 40 项缓存回调断言，覆盖私有/借用引擎及资源归零。
调用方每次预置不同错误，避免旧诊断导致测试误通过。
公共 API 范例登记在 `examples/obtain/main.c`。它使用手动 DNS 回调和
`xrtAcmeObtain`，执行账户持久化、证书/配对私钥落盘和续签缓存复用。
每条 `DNS_ADD` / `DNS_REMOVE` 输出属主与 TXT 值，完成对应操作后输入
`yes`；未确认的修改保留未知诊断和待核查计数，Add 不自动重放。
范例接受 directory URL、CA PEM、传播 resolver、store 目录和域名五个
参数；不带参数仅显示用法。构建清单包含该范例，CI 通过
`python tools/test_acme_examples.py` 在本地 TLS CA/UDP DNS 上实际执行，
独立核验 SAN、证书/私钥配对、账户持久化、缓存不重复下单及 TXT 清理。
可加 `--single` 验证单头交付，POSIX 可再加 `--sanitize`。
手动回调供单次命令使用，服务端集成应提供符合拥有权与并发契约的 provider。
独立 grant 验证器使用 `cryptography>=42` 逐段核对链签名、显式信任锚、
有效期、CA/签名用途和路径长度；18 项对抗证书测试由
`python -m unittest tools.test_acme_grant_verifier -v` 执行。该离线验证器
不替代一般 PKIX 策略引擎。

默认还运行 `--case provider-bodies` 的 18 项序列化回归：Cloudflare 创建、
腾讯云域名/记录查询、创建/删除和华为云创建记录在普通及 253 字节域名/200 字节 TXT
输入下，非零缓冲尾部不进入 JSON 请求；补 NUL 的真实分配失败不发请求，
不提交新记录或丢弃待删记录，并保留内存错误。此项控制 HTTP 响应来验证
实际 provider 实现的请求构造和清理，提供商 HTTPS 互操作仍须单独验收。
默认 `--case provider-wire` 再运行五家 provider 的 716 项本地 TLS 场景
（Ali 124 / AWS 77 / Cloudflare 134 / Tencent 199 / Huawei 182），保留实际
provider、网络引擎和 HTTP 实现，系统根证书加载替换为临时测试 CA；
故障用例另注入真实分配失败及确定的发送前失败，正常请求仍使用真实传输。
provider 服务同时监听 IPv4/IPv6 回环，避免 `localhost` 地址族回退影响
每次请求；系统禁用 IPv6 时保留 IPv4 入口，场景与请求校验不变。
独立 Python 服务端按
[ACS3 规范](https://www.alibabacloud.com/help/en/sdk/product-overview/v3-request-structure-and-signature)
与 [AWS SigV4 规范](https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_sigv-create-signed-request.html)、
[TC3 规范](https://intl.cloud.tencent.com/zh/document/api/1209/56016)及
[Huawei 签名规范](https://support.huaweicloud.com/intl/en-us/devg-apisign/api-sign-algorithm-002.html)
重新计算真实请求签名、正文哈希、日期、查询排序和 Ali nonce 唯一性。
场景覆盖跨 zone/同名多值、未登记的 Remove 不发删除请求、失败后保留句柄并重试、
权限拒绝和畸形响应，以及请求已到达服务端后断开连接的未知结果。
Route53 服务端独立校验 DELETE/CREATE 批次、原 TTL 和其他 TXT 值，
其中 14 项区域回归分别保存父/子/属主区域的数据，核对完整探测顺序、
动态区域变化、区域查询失败恢复及原区域清理 ID；其他 AWS 场景也逐项
核对完整属主到父区域的查询顺序；没有托管区时覆盖调用方先前的无关错误。
Cloudflare/华为云另有 20 项区域回归，覆盖父/子区域、子区域新增和撤销、
属主本身为区域、区域 ID 替换、权限拒绝/畸形/歧义响应，以及无区域时
覆盖旧错误；独立服务保存每个记录的原区域 ID，核对完整查询与清理顺序。
三家已拥有记录的 71 项 Add 回归覆盖同名同值重复、大小写、十次调用、
满八槽及八线程同值添加；分别核对原区域/记录 ID、只读请求次数、无新增
写入、身份变化/状态不可用/断流/拒绝/解析 OOM 后保留句柄和恢复。
Cloudflare/华为云的严格缺失可释放原槽并重新发现区域；腾讯云索引延迟
下的缺失候选不释放原 ID，随后可只读重新核对。腾讯云另核验原 DomainId
和域名，区域 ID 替换或改名不能移动原拥有权。
三项多记录部分清理场景保留真实请求与 ACK 校验，在互斥锁内通过内部
创建入口构造历史双 ID 同名同值状态；只绕过新公开 Add 的重复保护，
没有替换读取、身份核验或删除实现。新公开 Add 不再生成这种重复状态。
仅对已拥有值的删除执行结果对账：响应丢失后先读取，确认已删除则不重发。
新增的写入响应丢失或畸形时无法证明值的所有权，必须报告结果未知，
不凭随后看到同值而认领，也不重放新增。额外场景验证既有值及 XML 转义
同值不删除、冲突时另一写者创建同值、重复 Add 不占新槽、跨实例借用、
八槽容量/释放复用、未知状态隔离，以及两个未知错误包装的真实分配失败。
Ali 场景把实际 provider 接入流程重试函数，同时核对 Add 回调与云 API
请求均只发生一次；覆盖创建已提交后的五类异常应答、传输与诊断分配失败、
真实 JSON 解析 OOM、未知/已确认共用八槽、未知状态隔离及八线程添加清理。
Ali 的 500/503/599 与两家新增提交前后 503 均作为未知结果，不重放；
AWS 已确认拥有值的删除在提交前后 503 场景中仍按读取结果安全对账。
删除应答解析 OOM 保留内存错误和目标句柄；既有记录消失及未知删除的
本地对账按各家严格响应语义验证，实云缺失语义和一致性边界仍须独立验收。
Ali 区域发现另覆盖 25 类错误响应及七项发现/缓存场景：错误不能触发父区域
回退或创建，清空错误后同一实例仍可恢复；完整分页、更具体区域、大小写
缓存键、四项缓存替换及 219 字节深层属主均走真实签名和 TLS 请求。
模块测试逐一注入发现 JSON 解析的 26 个分配失败位置，核对原始内存错误
和活动分配归零；该数量对应当前测试向量，解析实现变化时会重新扫描。
这些本地场景使用固定虚构凭据，不表示真实云账号互操作已完成。
腾讯云的创建和删除对完整、无矛盾的 `OperationDenied` 及其点分子类
报告权限错误；类似 `OperationDeniedSpoof` 的前缀不作为明确拒绝，
仍保留未知结果。五项独立 TLS 场景核对错误种类、槽位保留或释放、
只读对账次数，以及调用方恢复后仍按原记录 ID 清理。
URL 回归核对实际请求目标与唯一 Host 字段：无路径查询使用 `/?...`，片段
按 [RFC 3986 的片段语法](https://www.rfc-editor.org/rfc/rfc3986#section-3.5)
校验后不进入请求；空片段、合法百分号编码及长片段允许，片段不占请求
目标容量。支持大小写等价的 scheme、IPv6 方括号和 DNS 末尾根点。
DNS SNI 去掉末尾根点；IPv4/IPv6 字面量只做证书身份验证，不发送 SNI，
证书缺少 IP 身份时仍拒绝。端点拒绝 userinfo、畸形/越界端口、旧式数值
地址、未编码的控制字符、反斜杠和畸形百分号编码。原有纯整数或十六进制
IPv4 写法及点分十六进制/混合八进制写法需改用四段十进制，依据
[RFC 3986 的旧式数值地址说明](https://www.rfc-editor.org/rfc/rfc3986#section-7.4)；
带非数值 DNS 标签的名称（例如 `0xdead.example`）仍允许。
IPv6 scope/IPvFuture 不受支持。
合法向量还验证端口 1/65535、前导零十进制端口、保留编码内容与 1023 字节
请求目标边界，并穷举非 NUL 片段字节的 ASCII 语法。
实现依据 [URI 组件与 scheme 规则](https://www.rfc-editor.org/rfc/rfc3986#section-3)
及 [SNI DNS/IP 名称规则](https://www.rfc-editor.org/rfc/rfc6066#section-3)。
双平台 CI 均已接入，最新本地验证与托管结果状态见
`extlibs/production-readiness.md`。

同一客户端的调用必须由宿主串行化；五家内建 provider 的 Add/Remove 内部加锁，
自定义 provider 的调用须由宿主串行化。所有 provider 的 Unit 都须等待在途调用结束。
HTTP 清理释放 resolver
和验证器，并在配置预算内等待私有引擎退休；超时或退休错误时保留拥有者。
实例一旦开始清理就不能再请求，即使引擎暂时仍运行；借用引擎不会被停止。
公开客户端可用 `xrtAcmeClientCleanup(client)` 判断结果：`true` 后调用
`xrtAcmeClientDestroy(client)` 释放外壳，`false` 时保留句柄并稍后重试。
直接调用 Destroy 也会在清理失败时保留外壳。清理保留调用前已有诊断，因此
必须用 Cleanup 的布尔结果判断完成，不能只看线程错误是否为空。
内建 DNS provider 的 ProviderUnit 完成后 `pContext == NULL`；未清空时保留
provider 并继续重试 Unit。失败构造与一站式申领的临时客户端单独留出至少
30 秒的回滚预算，避免极短请求超时导致异步退休资源被丢弃。若仍未完成，
未交付对象的拥有权转移至跨实例待清理队列，保留原始操作错误，并立即擦除
拥有者中的密钥、凭据及业务字段。`xrtAcmeCleanupPending(timeout, &pending)`
负责重试；入列与非阻塞 BUSY 轮询均不分配内存，也不启动后台线程。
`timeout == 0` 处理一批对象后立即返回，非零为等待预算，退休 ERROR 会提前
结束。`true` 表示队列及其他清理调用已取出的对象均释放，`false` 时根据
`pending` 保留库和相关运行环境再重试。已有线程错误会保留，判断依据是
返回值和计数。存在未完成对象时，新私有引擎构造先非阻塞清理，仍未完成则
以 `XERR_STATE` 拒绝，防止持续失败无限增加线程；借用引擎的构造仍可使用。

宿主退出或卸载前先停止新调用、等待在途操作结束，再清理已交付实例及队列。
下面以 Cloudflare provider 为例；传入对象须由宿主串行化：

```c
/* false 时保留 Client/Dns 及已加载的库，稍后再次调用。 */
bool AcmeCfShutdown(struct xacmeclient** Client, xacmednsprovider* Dns)
{
	if(*Client != NULL)
	{
		if(!xrtAcmeClientCleanup(*Client)) return false;
		xrtAcmeClientDestroy(*Client);
		*Client = NULL;
	}
	xrtAcmeDnsCfProviderUnit(Dns);
	if(Dns->pContext != NULL) return false;
	return xrtAcmeCleanupPending(UINT64_C(5000000), NULL);
}
```

`--case lifecycle` 使用真实引擎 Pin 固定 BUSY、注入退休 ERROR，并用真实
TCP 监听器确认清理后的请求没有拨号；同时验证原始错误、幂等清理、借用
边界、无效 CA 的 1 微秒失败初始化和逐分配点 OOM。`--case ownership`
检查五家 provider 与客户端 Unit/Cleanup/Destroy 在超时、错误、借用三种
条件下的 24 种组合、五种公开构造失败，以及各 provider 和失败客户端的
六轮构造 OOM 扫描；另检查六类失败工厂的 ERROR/BUSY 延后归属、清理前禁止
新增私有引擎、借用引擎不受影响、密钥擦除、零分配入列，以及并发取出计数、
入列/回链、四线程生产与清理、ERROR/超时后的完整尾链保留。一站式流程使用
受控构造/存储/签发依赖验证真实 Done 路径的成功和失败清理；真实 CA 流程由
下述 mock 脚本验证。每项结束后活动分配归零，无非法释放或重复释放。
`--case dns-lifecycle` 以独立 Python UDP 服务端验证 TXT、NXDOMAIN、畸形应答、
丢包超时及过期事务 ID，并注入发送/接收故障。查询结束明确 Close/Abort 后才
Destroy，借用引擎须恢复到零活动 UDP 对象并继续运行；私有 DNS 上下文的
清理也保留 BUSY/ERROR 拥有者、支持重试，启动失败不丢弃引擎。这些回归已
纳入默认脚本及双平台 CI。
mock CA 全链路脚本需要 Python `cryptography`。协议向量由独立
openssl/python 结果固化；Pebble/challtestsrv
集成与 LE staging、各云厂商真实 API 联测由环境变量门控
（`XACME_PEBBLE_URL` / `XACME_LIVE` / `XACME_ALI_KEY` / `XACME_CF_TOKEN` /
`XACME_TENCENT_ID` / `XACME_AWS_KEY` / `XACME_HUAWEI_AK` 等）。

阿里云实际 DNS-01 签发测试 `test_flow_live` 必须显式设置 `XACME_LIVE=1`，
并通过环境变量提供 `XACME_ALI_KEY`、`XACME_ALI_SECRET`、`XACME_LIVE_DOMAIN`
（你授权创建、删除挑战 TXT 的域名）和 `XACME_LIVE_STORE`（新的独立测试目录）。
可选 `XACME_LIVE_CONTACT` 为账户联系邮箱；`XACME_LIVE_DIRECTORY` 默认是
[Let's Encrypt staging](https://letsencrypt.org/docs/staging-environment/)。使用公开
Client/Store API 验证签发、账户密钥保存、授权读取及缓存命中；必须观察到本次 TXT
创建和成功删除。测试会保存生成的账户与证书私钥，测试目录应置于受限且不受版本
控制的路径。staging 证书用于测试，不能作为正式网站证书。

仅测试 DNS provider 的 `test_dnsali` 也要求 `XACME_LIVE=1`，并显式提供
`XACME_ALI_FQDN` 与唯一的 `XACME_ALI_VALUE`。没有启用标记时只运行离线解析、
签名和 OOM 回归；传播失败后仍尝试删除本实例拥有的精确 TXT。其他 provider 的
门控变量见各测试源码。不得将真实凭据写入示例、测试数据、日志或仓库配置。

写请求已经开始发送但没有完整响应时，错误域 `xrt.acme.http` 的
`XACME_HTTP_ERROR_UNCERTAIN` 表示服务端执行结果未知。库不会自动重发这类
写入；调用方应先查询服务端状态，再决定是否重新执行操作。ACME 的只读
POST-as-GET 由客户端换新 nonce 和 JWS 后限次重试。

账户密钥轮换按 RFC 8555 使用内层新钥、外层旧账户钥签名。发出换钥
请求后若响应丢失，客户端会以新钥查询现有账户绑定；确认成功才切换
内存密钥。调用方应在发起轮换前保留新钥 PEM，以便进程重启后用同一
新钥再次调用轮换接口完成对账。若指定的 store 写入失败，接口返回
失败，但内存仍保持已生效的新钥，避免继续用失效旧钥请求。

## CA 中立

任何 RFC 8555 directory 均可（`xacmeaccountconfig.sDirectoryUrl`），
内置 LE / LE staging / ZeroSSL / Google 预设常量；
需要 EAB 的 CA 经 `xacmeeab`（Kid + base64url HMAC，RFC 8555 §7.3.4）
在开户时自动绑定，联系方式走 `sContactEmail`。
切换 CA = 换参数，不换构建。

`XACME_DIRECTORY_BUYPASS` 和 `XACME_DIRECTORY_BUYPASS_TEST` 保留为历史
URL 常量，供已有调用兼容使用。Buypass 已于 2025-10-15 停止 TLS/SSL
新申请及续签，并于 2025-10-31 停止签发；这两个预设不能作为新部署的
签发或续签服务。服务状态于 2026-10-04 根据
[Buypass 官方停服公告](https://www.buypass.com/products/tls-ssl-certificates/discontinues-issuance-of-tls-ssl-certificates)
核对。公共符号索引保留这些常量，不能据此推定提供商服务仍在运行。

## DNS provider

- 内建五家：
  - `dns_ali`（阿里云 alidns，ACS3-HMAC-SHA256）
  - `dns_cf`（Cloudflare v4，Bearer API Token）
  - `dns_tencent`（DNSPod API 3.0，TC3-HMAC-SHA256）
  - `dns_aws`（Route53，SigV4 + XML 记录集读取与原子替换；保留同名 TXT 和原 TTL）
  - `dns_huawei`（华为云 DNS v2，SDK-HMAC-SHA256）
- 自定义：宿主直接填写 `xacmednsprovider` 的 id、Add、Remove
  （可选 Propagate）字段，零注册零全局状态。

阿里云添加只保存成功响应中的 RecordId，移除时只删除这些已确认句柄。
区域发现显式请求 `PageNumber=1&PageSize=1`，校验分页、RequestId 和记录列表；
非空列表的记录区域须与请求区域一致，列表本身不建立记录所有权。
仅 [公共错误表](https://www.alibabacloud.com/help/en/dns/api-alidns-2015-01-09-errorcodes)
中的 `InvalidDomainName.NoExist` 或 `DomainNotFound` 配合 400/404 才继续向父区域探测。
权限拒绝、限流、服务故障、普通 404 和畸形响应均停止。根据
[发现接口](https://www.alibabacloud.com/help/en/dns/api-alidns-2015-01-09-describedomainrecords)
的分页结构，一条有效记录即可确认区域存在，不要求读取完整记录列表。
四项缓存只匹配大小写规范化后的完整探测起点，填满后轮换，不能用已缓存
父区域跳过尚未探测的子区域。
含错误 `Code` 的创建响应不建立记录所有权。DNS 属主按 ASCII 小写登记和
匹配，TXT 摘要保持逐字节比较。重复 Add 和 Remove 都先按本实例保存的 ID
调用 [DescribeDomainRecordInfo](https://www.alibabacloud.com/help/en/dns/api-alidns-2015-01-09-describedomainrecordinfo)，
核对 RecordId、属主、TXT 值、TXT 类型和默认线路。重复 Add 在记录启用时
复用句柄；记录禁用、被修改、查询拒绝或响应畸形时失败并保留句柄。
本实例已确认的 ID 明确消失后，重复 Add 可以重新创建；同名同值的其他 ID
不会被认领。Remove 可以删除已禁用的本实例记录。删除成功响应必须返回
目标 RecordId；查询或删除仅在 400/404 返回 `DomainRecordNotBelongToUser`、
有效非空 RequestId 且无 RecordId 字段时确认消失，清除句柄。通用 404、
权限拒绝、服务故障、矛盾字段和畸形响应均保留句柄。删除已提交但应答丢失
时，后续 Remove 通过读取确认消失，避免再次发送删除。
创建响应复用本实例其他记录的 ID 时，原记录的句柄保持不变；新记录保留
未知状态，禁止通过同名同值认领或清理。Remove 按调用者匹配的登记槽校验
记录，不能因其他槽使用相同 ID 而改为删除另一条记录。

`DescribeDomainRecords` 与 `DescribeDomainRecordInfo` 是只读 RPC，传输断流
允许至多三次调用，每次使用新的 nonce 和签名。HTTP 层因 POST 标记的未知
结果在只读 RPC 中恢复为原始传输错误，流程层仍按自己的上限处理重试；
内存失败立即停止。添加、删除不使用该只读重试规则。每次 RPC 在签名前
清除上一请求的 HTTP 未知标记，先前未知写入仍由登记槽保留，签名阶段失败
不会把尚未发送的新记录登记为未知写入。

RAM 策略现在需要 `alidns:DescribeDomainRecordInfo`，以及原有的
`alidns:DescribeDomainRecords`、`alidns:AddDomainRecord`、`alidns:DeleteDomainRecord`。
按记录 ID 读取和删除是两个请求，接口没有条件删除；宿主须协调外部写入者
在这两个请求之间的修改。上述校验不提供跨进程事务保证。
新增响应丢失、5xx、畸形或缺少安全 RecordId 时返回
`xrt.acme.dns/XACME_DNS_ERROR_UNCERTAIN`；解析或错误包装分配失败保留
原始内存错误。实例内固定槽保留未知状态，后续同名同值 Add/Remove 均
失败且不发请求；需外部核对该次操作及服务端状态，再重建实例。
已确认和未知记录共用八槽，满载时拒绝新的记录；已确认记录的重复 Add 仍
可核对并复用句柄，已确认槽删除后可复用。
同实例 Add/Remove 加锁串行保护 HTTP 状态、zone 缓存和记录登记；宿主仍
须先停止新调用，再执行 ProviderUnit 和 Pending 清理。
签发流程不重放这类添加，也不按同名同值认领或自动清理其他签发者的 TXT。

Cloudflare 与华为云的 zone/record ID 只接受可直接作为单个 URL 路径段的
ASCII 字符；空值、点段、路径分隔符、查询/片段分隔符及控制字符均会拒绝。
zone 列表响应缺失或畸形时停止探测，不会把错误响应当作“该 zone 不存在”
而继续选择父 zone。
Cloudflare 的 `success` 必须为布尔 `true` 才接受 zone 列表和新建记录响应。
Cloudflare、腾讯云和华为云在创建请求发送前保留固定登记槽。HTTP 5xx、
响应丢失、畸形或无法核验的创建应答返回 `XACME_DNS_ERROR_UNCERTAIN`；
JSON 解析或错误包装的内存失败保留 `XERR_MEMORY`，登记槽仍保持未知状态。
同名同值的后续 Add/Remove 不发请求；属主按 ASCII 小写比较，TXT 值逐字节
比较，大小写别名与并发调用不能绕过保护。未知和已确认记录共用八槽。
必须由宿主核对原操作与服务端记录，再决定何时重建实例；不能仅凭查询到
同值而认领。确定的发送前失败及经过验证的明确拒绝会释放预留槽。
Cloudflare 创建应答还需空 `errors` 数组和匹配的 TXT name/content；华为云
需匹配 name/type/zone_id 及唯一 TXT 值。腾讯云需正整数 RecordId、有效
RequestId 且无 Error；TC3 签名中的 `x-tc-action` 值转成小写，传输头保留
原动作名。复用其他登记记录 ID 的应答不会覆盖原拥有者。
本地 TLS 回归覆盖这些契约、容量、未知状态隔离及诊断/解析分配失败；
Cloudflare Remove 先在登记的 zone/record ID 路径执行
[记录详情查询](https://developers.cloudflare.com/api/resources/dns/subresources/records/methods/get/)，
核对返回的 ID、TXT 类型、属主和值。身份不符、权限拒绝或畸形读取保留句柄，
不发 DELETE。HTTP 404 只有完整失败信封和记录不存在码 `81044` 才算缺失；
不能把代理、区域或鉴权的 404 当作记录已删除。
[删除应答](https://developers.cloudflare.com/api/resources/dns/subresources/records/methods/delete/)
要求 HTTP 200 和匹配的 result.id；可选 success/errors 一旦存在必须一致。
删除结果未知时只读取原 ID 对账，缺失才释放句柄；仍存在、身份变化或读取
失败则保留。内存失败保留原诊断与句柄，后续 Remove 可重新对账；一次
Remove 不重放删除。多条记录清理只释放已确认成功的槽，失败及后续槽保留。
GET 与 DELETE 之间没有 API 级原子条件，宿主须协调同一记录的外部写者。
华为云 Remove 在原 zone/recordset ID 路径执行
[记录详情查询](https://support.huaweicloud.com/intl/zh-cn/api-dns/ShowRecordSet.html)，
核对 ID、带尾点的属主、TXT 类型、zone_id、唯一 TXT 值及布尔 default=false。
创建/更新/冻结/禁用中的记录返回 `XERR_AGAIN`，不发 DELETE；身份不符、
读取失败或内存失败保留句柄和诊断。只有 HTTP 404 与完整错误体中的
[DNS.0313](https://support.huaweicloud.com/intl/zh-cn/api-dns/ErrorCode.html)
证明记录消失，区域不存在、通用 404 或矛盾记录字段均不能释放句柄。
[删除接口](https://support.huaweicloud.com/intl/zh-cn/api-dns/DeleteRecordSet.html)
的 HTTP 202/PENDING_DELETE 仅表示异步受理。最多四次读取原 ID，间隔
0.5、1、2 秒；确认消失才清除登记。轮询耗尽返回 `XERR_AGAIN`，句柄及
删除中状态保留，同名同值 Add 不发请求。后续 Remove 继续只读对账，
已核验受理的删除不重复发送；未知结果只有再次核对原记录仍稳定存在时
才允许重试原 ID。DELETE 应答可省略 records，若存在则须匹配唯一 TXT 值。
所需权限包含 `dns:recordset:get` 与 `dns:recordset:delete`；宿主须协调
GET 与 DELETE 之间同一记录的外部写者。Unit 仅释放本地资源。
腾讯云每次新建记录均从完整 TXT 属主逐级执行
[DescribeDomain](https://cloud.tencent.com/document/api/1427/56173)，核对域名与
正整数 DomainId，固定在对应登记槽；只允许准确的域名缺失码向父域回退。
新 Add 重新查询更具体域名，既有记录的查询/删除始终使用原 DomainId。
属主本身为托管域名时，按照 [CreateRecord](https://cloud.tencent.com/document/api/1427/56180)
的 `@` 主机记录编码创建和核验；宿主须具备相应套餐、托管及委派配置，参见
[子域名托管说明](https://cloud.tencent.com/document/product/302/79793)。本地服务只验证
该 API 契约，不证明具体账户可以托管任意子域名。20 项区域回归逐一核对完整
探测顺序、父子区域新增/撤销/ID 替换、大小写、253 字节属主、准确缺失码回退、
查询错误不写入/不向父域回退、无区域覆盖旧错误及顶点记录身份/解析 OOM 后恢复。
Remove 执行 [DescribeRecord](https://cloud.tencent.com/document/api/1427/56168)，
核对原 ID、DomainId、相对属主、TXT 值、默认线路名称/ID 及整数 Enabled。
身份变化、权限拒绝、协议错误和内存失败保留原句柄，不发 DELETE。
[DeleteRecord](https://cloud.tencent.com/document/api/1427/56176) 的 ACK 要求
HTTP 200、非空 RequestId、无 Error 或矛盾结果字段；可选 RecordId 若存在
须与原 ID 相同。一次 Remove 只发送一次删除；结果未知时只读对账，后续
Remove 只有再核对原身份后才可重试原 ID，未知状态阻止同名同值 Add。
`InvalidParameter.RecordIdInvalid` 只表示记录编号错误，不能单独当作缺失。
对账另核对当前域名 ID 和
[无过滤记录库存](https://cloud.tencent.com/document/api/1427/56166)，要求
TotalCount/ListCount/数组长度一致、最多 3000 条、ID 唯一及有效字段；不使用
分页、过滤、错误信封或畸形库存证明缺失。官方说明新记录有索引延迟并建议
30 秒后重查；创建 ACK 后不足 30 秒或单调时钟回退时保留句柄并返回
`XERR_AGAIN`。即使经过多天或重复查询、完整库存仍未出现原 ID，也不
释放句柄，不允许同名同值 Add。库存只用于诊断，不是缺失证明。
重新读取原 ID 且身份匹配后可恢复清理；删除已提交但 ACK 丢失且原 ID
无法读取时也保留待核查状态，不自动重放或认定成功。宿主须核对云端
原操作后决定何时重建实例。四项延迟超过一天的真实 TLS 场景覆盖空与
非空索引、重复观察和未知删除后的恢复。实云互操作仍须单独验收。
只读 RPC 仅对传输断流限次重新签名重试，协议/NUL/内存错误不重试。
权限需覆盖 DescribeDomain、DescribeRecord、DescribeRecordList、CreateRecord
及 DeleteRecord；宿主须协调读取与 DELETE 之间同一记录的外部写者。
Cloudflare/华为云新记录从完整 TXT 属主逐级查询当前凭据可见的最近托管区域，每次重新
探测，不用父区域缓存跳过子区域；区域新增、撤销或 ID 替换会影响后续
新记录，已有记录的清理仍绑定创建时的区域和记录 ID。查询错误立即停止，
只有有效空列表才向父区域查询；没有托管区域明确返回
`XERR_PROTOCOL / XACME_DNS_ERROR_ZONE`，覆盖先前无关错误。
两家的区域列表只接受 HTTP 200；201/202/206 即使携带有效列表也不能据此
新建记录。401/403 返回 `XERR_PERMISSION`，429/5xx 返回 `XERR_AGAIN`，
其他状态和畸形结构返回 `XERR_PROTOCOL`，均不向父区域回退或发送创建。
JSON 解析 OOM 保留原错误对象；已有无关内存错误不影响本次协议/状态分类。
34 项区域输入回归覆盖错误分类、过期诊断、NUL/尾随正文及空/命中列表的
实际 JSON 分配失败：CF 8/16、Huawei 7/15 个位置逐个注入，在真实 TLS
调用中核对首因和无创建，并在网络引擎启动前独立核对解析/错误资源归零。
只读区域故障未自动重试，宿主可按错误分类自行限次重试。

Cloudflare/腾讯云/华为云已拥有的同名同值再次 Add 先读取原 ID 核对身份，
成功不创建新记录、不占新槽，满八槽也可重复调用。未登记的云端同值不
自动认领；创建未知或删除待确认仍阻止 Add。Cloudflare/华为云只有原 ID
被严格证明不存在才释放并重新发现区域；身份变化或读取失败保留句柄。
腾讯云使用原 Domain/DomainId/RecordId，不凭列表缺失重建；只读核对失败
没有发送删除，不把该失败标成未知删除，后续可再次读取原 ID。
腾讯云禁用记录与华为云 DISABLE/FREEZE/ILLEGAL/POLICE/ERROR 状态返回
`XERR_STATE`；华为云创建/更新中返回 `XERR_AGAIN`，读到 PENDING_DELETE
记录受理状态并阻止后续 Add，继续 Remove 对账。不可用记录仍可按原身份
清理。重复 Add 的成功只确认 API 记录状态，传播确认仍走独立 DNS 探测。
华为云 zone 查询显式使用 `search_mode=equal`；其 API 的默认模糊匹配配合
`limit=1` 可能使目标 zone 未出现在结果中。
两家 provider 均拒绝多个同名 zone 或已填满的查询页，避免在存在未读取候选时
任意选择首个 ID；需要消歧的账户布局仍须由宿主另行处理。
区域查询参数依据 [Cloudflare List Zones](https://developers.cloudflare.com/api/resources/zones/methods/list/)
与 [Huawei 公有区域查询](https://support.huaweicloud.com/intl/en-us/api-dns/dns_api_62003.html)。
宿主仍须配置实际 DNS 委派并赋予区域查询权限；本地 TLS 夹具不证明实云
账户可见性和权威 DNS 已配置正确。

Route 53 内建 provider 只改写普通 TXT 记录集。遇到加权、地理位置等
路由策略记录集会报错，不会丢弃策略字段；单集合最多读取 400 个 TXT 值、
4 MiB 的 XML 响应。变更批次中所有 TXT 值的字符数合计受 AWS 的 32,000
字符配额约束，超限会在发送前报错。zone 发现只接受唯一公有托管区；
同名多个公有托管区或分页仍可能含同名区时会报错，避免选错区。
新属主/值从完整 TXT 属主逐级探测，不用父区域缓存跳过子区域；这也支持
在 `_acme-challenge` 属主本身建立托管区。查询失败立即停止，不向父区域
发起记录写入。已拥有值的重复 Add 和 Remove 使用登记时的区域 ID，
区域布局变化不能移动清理句柄；未认领的同值再次 Add 会重新探测。
宿主须按 [Route53 子区域委派步骤](https://docs.aws.amazon.com/Route53/latest/DeveloperGuide/CreatingNewSubdomain.html)
配置权威 NS；自动选区只核对该账号的托管区列表，不验证公网 NS 委派。
变更响应必须包含完整 `ChangeInfo`（Id、PENDING/INSYNC、SubmittedAt）；
空或畸形的 2xx 应答不能单凭 HTTP 状态认定成功，5xx 不能证明写入未执行。
写入错误必须是完整、无歧义的 REST-XML `ErrorResponse/Error/Code`；
消息、注释和错误码前缀均不能冒充 `InvalidChangeBatch`。解析接受 UTF-8、
实体、CDATA、标准默认命名空间或一致绑定的前缀；DTD、重复字段、非法字符、
尾随数据及未支持的信封形式进入未知保护状态。明确拒绝的 HTTP 400
`InvalidChangeBatch`、`Throttling`、`PriorRequestNotComplete` 最多尝试四次，
间隔 500ms、1s、2s，每次重新读取 TXT，保留期间出现的外部值及原 TTL。
这遵循 [Route53 对前次请求未完成的退避建议](https://docs.aws.amazon.com/Route53/latest/APIReference/API_ChangeResourceRecordSets.html)。
限流耗尽返回 `XERR_AGAIN/XACME_DNS_ERROR_NETWORK`，权限错误返回
`XERR_PERMISSION/XACME_DNS_ERROR_CREDENTIAL`；明确拒绝的新值不登记拥有权。
新增返回
`xrt.acme.dns/XACME_DNS_ERROR_UNCERTAIN` 并保留未知条目；该实例后续同名
同值 Add/Remove 均拒绝且不发请求，需核对服务端状态后重建实例。HTTP
内部的写入未知标记不依赖错误对象分配；包装分配失败时交付内存诊断，
仍保留未知条目。
流程也不会重放内存失败的 Add。发送前的确定失败不登记新记录。
已有同值的 Add 是成功空操作，Remove 不删该值；本实例已确认新增的值
重复 Add 保留原拥有权、只占一个槽。相同 zone/name/value 的跨实例或
跨进程使用，宿主须协调完整 Add→验证→Remove 生命周期及外部写者；
根据 Route53 的 [ResourceRecord 字段模型](https://docs.aws.amazon.com/Route53/latest/APIReference/API_ResourceRecord.html)，
同值读取无法证明逐值所有者或独立签发的租约。
DNS 属主按照 [Route53 域名规则](https://docs.aws.amazon.com/Route53/latest/DeveloperGuide/DomainNameFormat.html)
以 ASCII 小写用于登记、查找和签名请求；TXT 值始终逐字节比较。大小写
等价的属主不能绕过未知写入保护，满载时的重复 Add 和并发调用也复用同一槽。
真实 AWS 凭据测试通过
`XACME_AWS_KEY`、`XACME_AWS_SECRET` 和 `XACME_AWS_FQDN` 显式启用。

TXT 铺设后的传播确认由签发流程统一负责：provider 带
`XACME_DNS_CAP_PROPAGATE` 时委托其自证，否则对默认公共 resolver 组
（223.5.5.5 / 119.29.29.29 / 8.8.8.8，可经客户端配置覆盖）轮询 TXT，
任一可见即触发挑战；普通不可达或确认超时不阻断签发（CA 只查权威侧）。
传播探测的分配失败保留原 `XERR_MEMORY`，立即停止其他 resolver、等待
和挑战触发，并清理已铺设的 TXT；资源耗尽不能作为普通网络滞后忽略。

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

客户端保留 JSON/PEM/CSR/签名的内存失败和底层传输错误；按
`xrtErrorKind` 分类，不要假定所有错误都来自 `xrt.acme.flow`。
CA 响应的字符串须完整且不含 NUL；账户状态须为 `valid`，轮询遇到
无效 JSON 或内存不足立即失败。`badNonce` 只按问题对象的完整 `type`
识别，并使用该响应的有效 `Replay-Nonce`，最多发送三次 POST。
吊销只有 HTTP 200 或 HTTP 400 的准确 `alreadyRevoked` 类型视为成功。
签发在复制域名或发送订单前检查 1-511 字节及无 NUL 的限制；最多
16 个不同域名。失败时不交付部分链/密钥，TXT 清理保留签发的原始错误。

`acme_flow_fault_tests` 用公开客户端 API、真实 JSON/PEM/JWS/CSR 代码
验证错误传播、边界输入和恢复，HTTP 边界由测试控制；实际 HTTP/TLS、
nonce/JWS/EAB/DNS 和证书配对由 `python tools/test_acme_mock.py` 独立验证。

下载只接受 `application/pem-certificate-chain`，正文只能包含证书 PEM 与
空白；每张 DER 最多 256 KiB，一条链最多 16 张。客户端核对叶证书的
公钥、当前有效期和本次订单的完整 DNS SAN 集合，并验证所提供链中的
发行者、CA 标志、可选签名 KeyUsage 与各相邻证书的签名。证书钥不能
复用当前账户钥。解析或分配失败不交付部分产物；不支持的签名算法保留
`UNSUPPORTED` 诊断。部署仍需按自己的根信任、用途及完整 PKIX 策略验证
产物；传输所信任的 CA 不等同于证书的部署信任根。依据
[RFC 8555 §7.4.2、§11.1 与 §11.4](https://www.rfc-editor.org/rfc/rfc8555.html)。

## 独立 ACME 服务验收

`tools/prepare_acme_pebble.py` 获取固定版本的上游
[Pebble](https://github.com/letsencrypt/pebble) 并使用随附 vendor 依赖编译。
需要 Git、可启动 Go 1.26.0 工具链的 Go 安装，以及 `cryptography>=42`。
Windows 和 Linux 都可以从仓库根目录运行：

```text
python tools/prepare_acme_pebble.py --output out/pebble-runtime
python tools/build.py --manifest extlibs/xacme/config/modules.json --suite xacme_tests --test test_flow --no-run --no-single --no-examples --jobs 4
```

Linux 随后运行：

```text
python tools/test_acme_pebble.py --runtime out/pebble-runtime --binary out/gcc/native-linux/xacme_tests/test_flow --output out/pebble-acceptance
```

Windows 将 `--binary` 改为
`out/gcc/native-windows/xacme_tests/test_flow.exe`。输出目录必须是新目录。
这三组验收覆盖真实 DNS-01 查询、授权复用、签发、存储与续签判定、账户
密钥轮换、停用和吊销；备用链用例要求手工签发链连接到备用根，默认存储
路径的链连接到主根，不能以同时信任两根代替选择验证。固定 90 天证书
profile 用于核对 30 天阈值下的复用，短期证书应按相同阈值重新签发。
测试服务只监听回环地址，退出时关闭；CA 信任显式传入客户端，不改系统
信任库。源码、可执行文件、日志和产物哈希保存在结果中。

客户端保留并按顺序合并重复 `Link` 字段，从各链接自身的 `rel` 参数选择
备用链，接受带引号的关系列表；不支持的 `anchor` 链接会跳过。备用下载
或校验失败继续交付主链。备用链须包含相同 DER 叶证书；相对引用按照
[RFC 3986 §5.2](https://www.rfc-editor.org/rfc/rfc3986.html#section-5.2)
以主证书下载 URL 解析，查询和百分号转义保持原样，片段不参与 HTTP
请求或 JWS。mock CA 用同一叶证书的交叉签发链，分别验证绝对、相对、
点路径和网络路径链接，且备用用例仅信任备用根。Pebble 是测试 CA，
本验收仍需补充实际 CA、云 DNS
凭据及部署环境验证。
