# TLS

`tls` 是 XRT 内建 TLS 体系的协议基础层。当前这一层公开稳定的版本、状态、Alert 和记录编解码契约；它不直接持有 socket，也不建立第二套异步模型。

## 裁剪

启用宏：

```c
#define XRT_FEATURE_TLS
```

直接依赖核心模块中的结构化错误体系；核心模块不可裁剪，因此不需要额外启用宏。

记录保护层继续细分：

| 宏 | 功能 | 直接依赖 |
| --- | --- | --- |
| `XRT_FEATURE_TLS_SESSION` | 无 socket 的公共会话生命周期、惰性有界队列、所有权输入与 Span 输出 | `tls_context`、`tls_record`、`tls_messages`、`tls_schedule`、`net_buffer`、`crypto_core` |
| `XRT_FEATURE_TLS_CLIENT` | 独立客户端配置、真实后端能力过滤和初始 ClientHello | `tls_session`、`tls_hello_write`、`tls_handshake_reader`、`tls_schedule`、`tls_key_exchange`、`random_secure` |
| `XRT_FEATURE_TLS_SERVER` | 独立服务端配置、SNI/ALPN、身份选择、TLS 1.3 证书航班和 READY 状态 | `tls_session`、`tls_hello_write`、`tls_messages_write`、`tls_schedule`、`tls_key_exchange`、`tls_identity`、`random_secure` |
| `XRT_FEATURE_TLS_VERIFY` | 不可变信任快照、默认对端验证、自定义信任决策和 TLS 1.3 握手验签 | `tls_negotiate`、`x509_store`、`x509_identity`、`x509_verify`、`crypto_core` |
| `XRT_FEATURE_TLS_CLIENT_VERIFY` | 客户端 Certificate 与 CertificateVerify 状态 | `tls_client`、`tls_verify` 及所选 X.509 密码后端 |
| `XRT_FEATURE_TLS_RESUME` | 不可变 TLS 1.3 ticket/PSK 恢复资产、有效期与票据年龄 | `tls`、`time`、`crypto_core` |
| `XRT_FEATURE_TLS_CLIENT_RESUME` | 客户端恢复主密钥、票据 PSK 派生、ClientHello binder 和有界显式交接 | `tls_client_verify`、`tls_resume`、`tls_psk_write`、`crypto_sha256` |
| `XRT_FEATURE_TLS_SERVER_RESUME` | 服务端外部票据查找、严格 binder 校验、票据签发和恢复对象交接 | `tls_server`、`tls_resume`、`tls_psk`、`tls_psk_write` |
| `XRT_FEATURE_TLS_STREAM` | TCP 与 TLS 会话组合流、双层背压、阶段超时和认证关闭 | `net_tcp`、`tls_client`、`tls_server` |
| `XRT_FEATURE_TLS_STREAM_FUTURE` | TLS Stream 的发送、接收和条件等待 Future | `tls_stream`、`future` |
| `XRT_FEATURE_TLS_STREAM_DIAL` | 主机解析、TCP 地址竞速和 TLS 握手组成的受管客户端拨号 | `tls_stream`、`net_tcp_dial` |
| `XRT_FEATURE_TLS_STREAM_DIAL_FUTURE` | 受管 TLS Dial 的 Future 结果与协作取消 | `tls_stream_dial`、`future` |
| `XRT_FEATURE_TLS_IDENTITY` | 不可变证书链与外部签名器核心 | `tls_negotiate`、`x509_parse`、`crypto_core` |
| `XRT_FEATURE_TLS_RECORD` | 单向密钥、序列号、nonce、TLS 1.2/1.3 记录保护骨架 | `tls`、`crypto_core` |
| `XRT_FEATURE_TLS_RECORD_AES` | AES-128-GCM、AES-256-GCM 记录后端 | `tls_record`、`crypto_aes_gcm` |
| `XRT_FEATURE_TLS_RECORD_CHACHA` | ChaCha20-Poly1305 记录后端 | `tls_record`、`crypto_chacha20_poly1305` |
| `XRT_FEATURE_TLS_HANDSHAKE` | 握手消息与扩展 framing | `tls` |
| `XRT_FEATURE_TLS_HANDSHAKE_READER` | 跨记录握手消息的有界自适应重组 | `tls_handshake` |
| `XRT_FEATURE_TLS_HELLO` | 扩展游标、SNI、ALPN、版本、组、签名、key_share 与 Hello 严格解析 | `tls_handshake` |
| `XRT_FEATURE_TLS_NEGOTIATE` | 无状态版本、套件、签名与 key_share 选择 | `tls_hello` |
| `XRT_FEATURE_TLS_POLICY` | 客户端与服务端共享的有序协议偏好及完整校验 | `tls_negotiate`、`tls_key_exchange` |
| `XRT_FEATURE_TLS_CONTEXT` | 深拷贝策略、硬限制与公平性预算的共享只读快照 | `tls_policy` |
| `XRT_FEATURE_TLS_KEY_EXCHANGE` | 命名组元数据、后端探测与调用方缓冲密钥交换骨架 | `tls` |
| `XRT_FEATURE_TLS_KEY_EXCHANGE_X25519` | X25519 临时密钥与共享秘密后端 | `tls_key_exchange`、`crypto_x25519_keypair` |
| `XRT_FEATURE_TLS_KEY_EXCHANGE_X448` | X448 临时密钥与共享秘密后端 | `tls_key_exchange`、`crypto_x448_keypair` |
| `XRT_FEATURE_TLS_KEY_EXCHANGE_P256` | secp256r1 临时密钥与共享秘密后端 | `tls_key_exchange`、`crypto_p256_keypair` |
| `XRT_FEATURE_TLS_KEY_EXCHANGE_P384` | secp384r1 临时密钥与共享秘密后端 | `tls_key_exchange`、`crypto_p384_keypair` |
| `XRT_FEATURE_TLS_HELLO_WRITE` | 增量扩展 writer 与 ClientHello/ServerHello 正文编码 | `tls_hello` |
| `XRT_FEATURE_TLS_MESSAGES` | Certificate、EncryptedExtensions、CertificateVerify、Finished、KeyUpdate 与票据严格解析 | `tls_hello` |
| `XRT_FEATURE_TLS_MESSAGES_WRITE` | 握手语义消息的调用方缓冲编码 | `tls_messages` |
| `XRT_FEATURE_TLS_AUTH_MESSAGES` | CertificateRequest、TLS 1.2 ECDHE、OCSP 状态与压缩证书 framing | `tls_messages` |
| `XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE` | 认证消息与证书颁发者向量的调用方缓冲编码 | `tls_auth_messages` |
| `XRT_FEATURE_TLS_SCHEDULE` | transcript、TLS 1.2 PRF、TLS 1.3 密钥调度骨架 | `tls`、`crypto_core` |
| `XRT_FEATURE_TLS_SCHEDULE_SHA256` | SHA-256 transcript、HMAC 与 HKDF 后端 | `tls_schedule`、`crypto_hkdf_sha256` |
| `XRT_FEATURE_TLS_SCHEDULE_SHA384` | SHA-384 transcript、HMAC 与 HKDF 后端 | `tls_schedule`、`crypto_hkdf_sha512` |

只需要通用容器、文件或明文网络时可以完全裁掉 TLS。会话、握手、恢复、TCP 适配和主机名拨号都使用独立细粒度宏，不会被基础记录解析强制带入。

## 协议范围

XRT 只协商 TLS 1.2 和 TLS 1.3，默认优先 TLS 1.3，不提供 SSLv2、SSLv3、TLS 1.0 或 TLS 1.1 兼容路径。记录头中的 `legacy_version` 是线路兼容字段，不等于最终协商版本。

当前密码范围只保留 AEAD 与前向保密路径：TLS 1.3 支持 AES-128-GCM、AES-256-GCM 和 ChaCha20-Poly1305；TLS 1.2 支持 ECDHE-ECDSA/ECDHE-RSA 配合上述 AEAD。静态 RSA、CBC、RC4、3DES 和 TLS 压缩不进入新体系。`xrtTlsCipherName()` 返回可用于日志的标准套件名。

`xrtTlsCipherInfo()` 返回进程期只读的 `xtlscipherinfo`，把每个套件唯一的版本、摘要、AEAD、TLS 1.2 认证类型、密钥、静态 IV、显式 nonce 和认证标签长度集中表达。未知套件返回 `NULL` 且不设置错误，适合策略探测；调用方不能释放或修改返回对象。TLS 1.3 的 `Authentication` 固定为 `XTLS_CIPHER_AUTH_INDEPENDENT`，因为证书或 PSK 认证不再由密码套件决定。

```c
const xtlscipherinfo* Info = xrtTlsCipherInfo(
	XTLS_AES_128_GCM_SHA256
);

if ( (Info != NULL) && (Info->Version == XTLS_VERSION_13) ) {
	/* Info->KeySize 为 16，Info->IvSize 为 12。 */
}
```

`xtlshash` 和 `xtlsaead` 描述协议选择，不要求启用具体密码后端；`xtlscipherauth` 只表达 TLS 1.2 套件认证约束。记录保护、密钥调度和协商层共享这份元数据，避免各自维护一套容易漂移的套件表。

基础限制：

| 常量 | 含义 |
| --- | --- |
| `XTLS_RECORD_HEADER_SIZE` | 固定 5 字节记录头 |
| `XTLS_RECORD_PLAINTEXT_MAX` | 16384 字节明文上限 |
| `XTLS12_RECORD_CIPHERTEXT_MAX` | 18432 字节 TLS 1.2 密文上限 |
| `XTLS13_RECORD_CIPHERTEXT_MAX` | 16640 字节 TLS 1.3 密文上限 |
| `XTLS13_INNER_PLAINTEXT_MAX` | 16385 字节内容、类型与零填充总上限 |
| `XTLS_AES_GCM_RECORD_LIMIT` | 单组 AES-GCM 流量密钥最多处理 `2^24` 条记录 |

## 结果与错误

`xtlsresult` 把正常控制流与错误分开：

- `XTLS_OK`：操作完成。
- `XTLS_AGAIN`：输入尚不完整，继续喂入数据即可，不设置线程错误。
- `XTLS_CLOSED`：收到或完成协议级干净关闭。
- `XTLS_ERROR`：协议或调用失败，使用 `xrtGetError()` 读取 `xrt.tls` 结构化错误。

`xtlserror` 进一步区分版本、记录类型、记录长度、Alert、状态、密码、证书、校验和恢复阶段。调用方不需要解析错误字符串。

## 记录解析

```c
xtlsrecord Record;
size_t Required;
xtlsresult Result = xrtTlsRecordParse(Input, &Record, &Required);
```

解析器适合任意分片方式：

- 输入不足 5 字节时，返回 `XTLS_AGAIN`，`Required` 为 5。
- 已有完整头但负载不足时，返回 `XTLS_AGAIN`，`Required` 为整条记录长度。
- 只有 `XTLS_OK` 会写入 `Record`。
- `Record.Payload` 借用输入内存，不分配、不复制。
- 输入中包含多条记录时只返回第一条，`EncodedSize` 给出应消费的字节数。

未知内容类型、非法兼容版本和超过 TLS 1.2 密文硬上限的长度会立即返回协议错误，不会等待攻击者声明的超大负载。

## 记录编码

```c
uint8 Buffer[64];

bool OK = xrtTlsRecordEncode(
	XTLS_RECORD_APPLICATION_DATA,
	UINT16_C(0x0303),
	XRT_BYTES_LITERAL("hello"),
	Buffer,
	sizeof(Buffer)
);
```

编码器允许输入与输出重叠，适合在已有明文前原地腾出记录头。容量不足时输出保持不变。`xrtTlsRecordSize()` 可提前计算总长度。

记录编码器是协议工具，不会绕过后续会话层的加密、序列号和状态检查；应用数据应优先通过 TLS 会话写入。

## 握手与扩展 Framing

`tls_handshake` 公开握手消息和单个扩展的零分配线路原语。它只处理 framing，不在这一层判断某种消息或扩展能否出现在当前握手状态：

```c
xtlshandshake Message;
size_t Required;
xtlsresult Result = xrtTlsHandshakeParse(Input, &Message, &Required);
```

- 握手头固定 4 字节，正文长度是 24 位；扩展头固定 4 字节，负载长度是 16 位。
- 输入不足返回 `XTLS_AGAIN` 和继续解析所需的精确总长度，不设置错误，也不修改输出。
- 成功结果借用输入，只消费开头第一条消息或第一个扩展，便于上层连续处理聚合记录。
- 未知握手类型和未知扩展类型会保留线路数值。后续状态机负责按版本、角色和阶段决定忽略、拒绝或交给扩展处理器。
- 编码允许正文或负载与输出精确重叠，容量不足和不可编码长度不会修改输出。
- `xrtTlsHandshakeSize()` 与 `xrtTlsExtensionSize()` 在分配或预留缓冲前执行 24 位、16 位线路上限检查。

通用 framing 不分配声明长度对应的内存。会话层必须另外配置实际握手消息上限，避免对端用接近 16 MiB 的合法 24 位长度占用过多连接内存。ClientHello、ServerHello、SNI、ALPN、supported_versions、key_share 等语义解析建立在这些原语之上，并负责重复扩展和上下文约束。

## 握手消息 Reader

`tls_handshake_reader` 解决握手消息跨 TLS record、跨 socket 读取分片的问题，同时不把每个连接变成固定大缓冲：

```c
xtlshandshakereader Reader;
xrtTlsHandshakeReaderInit(&Reader, NULL);

size_t Consumed;
xtlshandshake Message;
xtlsresult Result = xrtTlsHandshakeReaderRead(
	&Reader, Input, &Consumed, &Message
);
```

- 完整消息已经位于单个 `Input` 时直接返回借用输入的视图，不分配也不复制。
- 1 到 3 字节的分片头只写入 reader 内联的 4 字节 Header；确认正文需要跨输入后才分配重组区。
- 重组区按实际收到的字节以约 1.5 倍增长，不会在看到 24 位声明长度时立即分配整条消息。
- 默认完整编码消息上限是 1 MiB，可通过 `xtlshandshakereaderconfig.Limit` 在 4 字节到线路最大值之间调整。超限头零消费、零分配并保持 reader 不变。
- 默认只跨消息保留不超过 16 KiB 的容量；证书链等大消息在下一次 `Read()` 或 `Reset()` 时释放，不会永久放大每连接常驻内存。
- 一次只发布一条消息，`Consumed` 不跨过下一条聚合消息。跨分片结果借用 reader，生命周期到下一次 `Read()`、`Reset()` 或 `Unit()`；单片结果借用本次 Input。
- reader 自己的可移动分配区不能作为下一段 Input，API 会在 `realloc` 前显式拒绝别名。

`xrtTlsHandshakeReaderRequired()` 返回当前完整消息长度；只有部分头或空闲时返回 4。正常分片返回 `XTLS_AGAIN` 且不设置错误，容量不足、配置错误和超限消息通过结构化 TLS 错误表达。

## Hello 与核心扩展

`tls_hello` 在 framing 之上提供三层能力：

1. `xtlsextensioncursor`、`xrtTlsExtensionsRead()` 和 `xrtTlsExtensionsFind()` 遍历完整扩展向量，保留未知类型，并严格拒绝任何重复扩展。
2. `xrtTlsServerNames()`、`xrtTlsProtocols()`、`xrtTlsClientVersions()`、`xrtTlsGroups()`、`xrtTlsSignatures()` 和 key_share API 解析独立扩展数据。游标和结果都借用输入，不把 SNI、ALPN 或公钥复制到固定数组。
3. `xrtTlsClientHelloParse()` 与 `xrtTlsServerHelloParse()` 一次验证完整正文并发布易用视图。声明长度必须恰好消费正文，已知扩展采用精确长度规则，HelloRetryRequest 会由固定 random 自动识别。

```c
xtlsclienthello Hello;

if ( !xrtTlsClientHelloParse(Handshake.Body, &Hello) ) {
	return false;
}

xtlsextension Sni;
if ( xrtTlsExtensionsFind(
	Hello.Extensions, XTLS_EXTENSION_SERVER_NAME, &Sni
) == XTLS_ITEM_VALUE ) {
	xbytesview Host;
	if ( xrtTlsHostName(Sni.Data, &Host) == XTLS_ITEM_VALUE ) {
		/* Host 借用 ClientHello 输入。 */
	}
}
```

核心语义边界：

- 扩展、SNI 名称类型、supported_versions、supported_groups、signature_algorithms 和客户端 key_share 都检查重复项。
- ALPN `ProtocolName` 是非空不透明字节，不强加 ASCII、逗号分隔或末尾零字符限制；服务端选择必须恰好包含一个协议。
- `xrtTlsProtocolFind()` 区分找到、未找到和畸形列表；`xrtTlsProtocolSelect()` 按服务端偏好列表顺序选择双方第一个交集，不内置 HTTP 协议优先级。
- ClientHello 密码套件向量必须非空且为偶数字节，压缩列表必须包含 null compression；只要声明支持 TLS 1.3，兼容压缩字段就必须严格为单个零。
- TLS 1.3 ClientHello 的 `pre_shared_key` 必须是最后一个扩展。
- ServerHello key_share 必须恰好包含一个非空密钥；HelloRetryRequest key_share 必须恰好包含一个两字节命名组，不能用宽松的“大于等于长度”规则接受尾随数据。
- Hello 解析器只负责线路结构和已知核心扩展的局部语义。密码套件选择、版本协商、扩展出现位置、SNI 证书选择和 ALPN 策略属于后续状态机。

扩展唯一性检测不分配 8K 类型位图。游标只保存 32 字节桶状态；发生桶碰撞时最多回看同桶的 16 位类型，因此正常路径为线性扫描，最坏回看次数也受 16 位类型空间约束。

## 无状态协商

`tls_negotiate` 只选择协议参数，不生成随机数、密钥或签名，也不修改会话状态。所有偏好数组都由调用方提供并按本地顺序解释，因此服务端、客户端、硬件能力探测和应用策略可以共享同一套选择器，不受库内硬编码优先级限制。

```c
static const xtlsversion Versions[] = {
	XTLS_VERSION_13, XTLS_VERSION_12
};
static const xtlscipher Ciphers[] = {
	XTLS_CHACHA20_POLY1305_SHA256,
	XTLS_AES_128_GCM_SHA256,
	XTLS_AES_256_GCM_SHA384
};

xtlsversion Version;
xtlscipher Cipher;

xrtTlsVersionSelect(&OfferedVersions, Versions, 2, &Version);
xrtTlsCipherSelect(
	Version, &Hello.CipherSuites, XTLS_IDENTITY_RSA,
	Ciphers, 3, &Cipher
);
```

API 分层如下：

- `xrtTlsIdsSelect()` 按本地偏好求任意 16 位标识列表的交集，未知和私有线路值不会被截断。`xrtTlsKeyShareFind()` 在完整客户端 key_share 负载中查找指定组。
- `xrtTlsVersionSelect()` 选择已解析的 `supported_versions`；`xrtTlsClientVersionSelect()` 进一步处理扩展缺失语义，此时只能选择 TLS 1.2，绝不会根据 `legacy_version` 猜测 TLS 1.3。
- `xrtTlsCipherSelect()` 使用 `xrtTlsCipherInfo()` 和 `xtlsidentitytype` 过滤版本与 TLS 1.2 认证约束。TLS 1.3 套件与证书或 PSK 身份独立；TLS 1.2 ECDHE_RSA 只接受 RSA 身份，ECDHE_ECDSA 按 RFC 8422 接受 ECDSA 或 EdDSA 身份。
- `xrtTlsSignatureInfo()` 返回方案要求的身份类别、摘要长度和协议版本范围。元数据是进程期只读对象，未知线路值返回 `NULL` 且不设置错误；会话策略、身份选择和签名实现不再分别维护方案表。
- `xrtTlsSignatureSelect()` 区分版本规则。TLS 1.3 RSA 握手签名只选择 PSS，ECDSA 方案必须匹配身份曲线；TLS 1.2 的 ECDSA 线路值仍是哈希/签名对，不把名称中的曲线错误地强加到证书密钥。
- `xrtTlsKeyShareSelect()` 先验证 key_share 是 `supported_groups` 的同序子序列。`XTLS_KEY_SHARE_PREFER_GROUP` 严格保留本地组优先级，首选组没有 share 时发布 `Retry=true`；`XTLS_KEY_SHARE_PREFER_READY` 优先选择已经携带 share 的共同组以避免额外往返，没有可用 share 时才请求首选共同组。

所有选择器使用 `XTLS_ITEM_VALUE`、`XTLS_ITEM_DONE` 和 `XTLS_ITEM_ERROR`：无交集是正常 `DONE`，不设置错误且不修改输出；畸形对端输入或非法本地配置返回结构化 `xrt.tls` 协商错误。结果中的 key-share 公钥借用 ClientHello 输入，生命周期不超过原输入。

`xtlsidentitytype` 只描述握手签名公钥类别，不等同于后续持有证书链、私钥和策略的身份对象。这一层故意不判断密码后端是否编入、RSA-PSS 密钥参数和模数长度、证书链是否满足对端 `signature_algorithms_cert`、组公钥是否在曲线上，也不决定 PSK、SNI 或 ALPN。会话配置根据实际启用的密码后端和密钥能力构造偏好数组，身份选择发生在 SNI 之后，密钥交换层再验证组专用公钥并计算共享秘密；自定义组或签名仍可使用通用标识 API 和原始扩展视图实现。

## TLS 策略

`tls_policy` 把原先散落在客户端、服务端和身份分支中的硬编码优先级收敛成一份借用式配置。默认初始化不分配内存，数组指向进程期只读常量；调用方可以逐项替换为生命周期覆盖上下文创建过程的自有数组：

```c
static const xtlsversion Versions[] = { XTLS_VERSION_13 };
static const xtlscipher Ciphers[] = {
	XTLS_CHACHA20_POLY1305_SHA256,
	XTLS_AES_128_GCM_SHA256
};

xtlspolicy Policy;
xrtTlsPolicyInit(&Policy);
Policy.Versions = Versions;
Policy.VersionCount = 1;
Policy.Ciphers = Ciphers;
Policy.CipherCount = 2;
Policy.KeySharePolicy = XTLS_KEY_SHARE_PREFER_GROUP;

if ( !xrtTlsPolicyValid(&Policy) ) {
	return false;
}
```

策略校验不修改输入且不分配内存。版本和套件必须非空；所有列表都必须保持指针/数量一致、元素唯一且属于内建会话能力。每个套件必须对应一个启用版本，每个启用版本也必须至少保留一个套件。组和签名列表可以为空，为恢复会话、PSK 或后续外部认证路径保留扩展空间；非空签名必须能用于至少一个启用版本，因此 TLS 1.2 专用 PKCS#1 方案不会混入纯 TLS 1.3 策略。

策略只描述协议偏好，不承诺当前裁剪构建已经包含对应密码执行后端。后续客户端或服务端会话创建会结合 `xrtTlsGroupAvailable()`、记录 AEAD、身份私钥和验证后端生成实际执行路径；原始策略快照保持不变，便于同一配置服务不同机器和硬件能力。

## 共享上下文

`tls_context` 把调用方策略转换为可跨线程共享的只读快照。上下文使用一次紧凑分配保存对象和四个有序数组；创建返回后不再借用 `xtlspolicy`、数组或 `xtlscontextconfig`。连接和会话只保留上下文引用，证书或 ticket key 轮换通过创建新上下文并替换上层引用完成，不在活动对象中原地修改策略。

```c
xtlscontextconfig Config;
xtlscontext* Context;

xrtTlsContextConfigInit(&Config);
Config.Policy = &Policy;
Config.Limits.PlainLimit = 512u * 1024u;

Context = xrtTlsContextCreate(&Config);
if ( Context == NULL ) {
	return false;
}

/* 每个会话持有自己的引用。 */
xrtTlsContextRetain(Context);
xrtTlsContextRelease(Context);
xrtTlsContextRelease(Context);
```

`xtlslimits` 不会触发预分配。`FeedLimit`、`SendLimit` 和 `PlainLimit` 只是后续自适应 `xnetbuf` 队列的硬上限；空闲 context 或 session 不常驻 8 KiB 缓冲。三个队列至少容纳一条最大合法记录，避免对端发送标准大小记录后永久停滞。`HandshakeLimit` 限制单条重组消息，默认 1 MiB；`RecordBudget` 和 `HandshakeBudget` 限制一次非阻塞驱动处理的工作量，防止高吞吐连接独占 Worker。

上下文当前保存协议策略，不把 Worker 专属 `xnetbufpool` 放入共享对象，也不把“协议已知”误当成“当前构建后端可执行”。客户端或服务端会话创建时再将上下文策略与已编入的组、AEAD、身份和验证能力求交集；无法得到完整执行路径时创建失败，而不会静默改写调用方优先级。

## 命名组与密钥交换

`tls_key_exchange` 把协议组信息与密码后端分开。`xrtTlsGroupInfo()` 对 X25519、X448、secp256r1 和 secp384r1 始终返回进程期只读元数据；`xrtTlsGroupAvailable()` 才反映当前裁剪配置。未知组查询返回 `NULL` 或 `false` 且不设置错误，便于策略层探测；对未知组或已裁剪后端执行生成、派生时返回 `XTLS_ERROR_KEY_EXCHANGE`。

```c
const xtlsgroupinfo* Info = xrtTlsGroupInfo(XTLS_GROUP_X25519);

if ( (Info != NULL) && xrtTlsGroupAvailable(Info->Group) ) {
	uint8 Private[32];
	uint8 Public[32];

	if ( !xrtTlsKeyShareGenerate(
		Info->Group, Private, sizeof(Private), Public, sizeof(Public)
	) ) {
		return false;
	}
}
```

- `PrivateSize`、`PublicSize` 和 `SharedSize` 是精确有效长度；输出容量可以更大，函数只写对应有效区间。
- `xrtTlsKeyShareGenerate()` 要求私钥与公钥有效输出区间不重叠。它先验证组、指针、容量和重叠，再调用随机源；任何失败都不修改两段输出。
- `xrtTlsKeyShareDerive()` 要求本地私钥和对端公钥视图恰好等于元数据长度。输出可以覆盖本地私钥或对端公钥，因为四个后端都在发布结果前完成输入消费；容量不足或密码校验失败时输出不变。
- X25519 与 X448 在派生阶段以常量时间聚合结果并拒绝全零共享秘密；P-256 与 P-384 验证私钥范围、未压缩点格式、坐标范围和曲线成员关系。
- 密码后端错误作为 `xrt.tls` 密钥交换错误的 `Cause` 保留，因此上层宿主和 C 调用方既能按 TLS 阶段处理，也能继续读取具体 `xrt.crypto` 原因。
- API 不分配堆内存，也不保存每连接状态。会话层只需在组选择完成后按元数据申请一组精确缓冲，不再为每个对象常驻四套私钥、公钥或最大共享秘密。

这层只执行传统 ECDHE/XDH。未来新增命名组时，协议解析和协商仍可通过 `uint16` 保留未知线路值；增加密码实现只需新增独立后端宏，不需要修改 Hello parser 的通用逃生路径。

## Hello Writer

`tls_hello_write` 把“高级构建器”和“直接线路控制”放在同一条分层路径上。`xtlswriter` 只引用调用方缓冲，不分配、不扩容，也不包含固定 1KB 或 8KB 数组：

```c
uint8 ExtensionBuffer[512];
xtlswriter Writer;

xrtTlsWriterInit(&Writer, ExtensionBuffer, sizeof(ExtensionBuffer));
xrtTlsWriterHostName(&Writer, XRT_BYTES_LITERAL("example.com"));

xbytesview Protocols[] = {
	XRT_BYTES_LITERAL("h2"),
	XRT_BYTES_LITERAL("http/1.1")
};
xrtTlsWriterProtocols(&Writer, Protocols, 2);

uint16 Versions[] = { XTLS_VERSION_13, XTLS_VERSION_12 };
xrtTlsWriterClientVersions(&Writer, Versions, 2);
```

常见路径可直接使用 `xrtTlsWriterHostName()`、`xrtTlsWriterProtocols()`、`xrtTlsWriterIds()`、版本和 key_share helper。少见或私有扩展使用 `xrtTlsWriterExtension()` 追加原始负载，不必修改协议层。每次追加先验证当前向量、重复类型、线路上限、容量和输入重叠；只有完整扩展写入成功才推进 `Writer.Size`。

`xrtTlsClientHelloSize()` / `xrtTlsServerHelloSize()` 先验证视图和核心扩展语义，再返回精确正文长度；对应 `Encode()` 在容量不足、Retry random 与标记不一致、TLS 1.3 压缩字段错误或输入输出重叠时保持输出不变。正文写好后可直接交给 `xrtTlsHandshakeEncode()` framing，或由后续会话层写入 transcript 和记录层。

重叠规则是显式的：原始 `xrtTlsWriterExtension()` 使用 `memmove`，允许负载来自 writer 缓冲；SNI、ALPN、标识数组、key_share 和完整 Hello 编码拒绝输入与本次目标区域重叠，避免多字段写入覆盖尚未读取的数据。空扩展向量编码时省略可选的两字节扩展总长字段。

大 ClientHello 只受调用方容量和 TLS 线路上限约束。回归包含超过旧版 1024 字节栈缓冲的 255 项 ALPN 列表，并由严格解析器完成 round-trip 验证。

## 握手语义消息

`tls_messages` 把握手 framing 与会话状态机之间反复出现的字段切分收敛为一层公共协议 API。它不解析 X.509、不执行签名，也不决定当前状态是否允许某种消息；它只保证正文符合 TLS 1.2 或 TLS 1.3 的精确线路结构。

证书链采用消息视图与游标两级 API：

```c
xtlscertificatemessage Certificates;
xtlscertificatecursor Cursor;
xtlscertificateentry Entry;

if ( !xrtTlsCertificateParse(Version, Handshake.Body, &Certificates) ||
	!xrtTlsCertificateEntries(&Certificates, &Cursor) ) {
	return false;
}

while ( xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_VALUE ) {
	/* Entry.Data 是 DER；TLS 1.3 的 Entry.Extensions 保留条目扩展。 */
}
```

- TLS 1.2 与 TLS 1.3 的请求上下文、列表和条目格式分别解析，不使用“至少还有这些字节”的宽松规则接受尾随数据。
- 证书列表可以为空，由握手角色和认证策略决定该场景是否合法；非空条目的 DER 数据必须至少一字节。
- 游标不保存固定证书指针数组，因此没有 4、8、16 项链上限。单张证书使用 24 位长度，70KB 以上证书不会被错误压缩成 16 位。
- TLS 1.3 每个证书条目的扩展向量都会验证 framing 与类型唯一性，并保留未知扩展供状态机、OCSP 和 SCT 层处理。
- 所有视图借用握手正文；解析和遍历不分配、不复制证书。

其他消息使用短路径 API：

| API | 契约 |
| --- | --- |
| `xrtTlsEncryptedExtensionsParse()` | 16 位扩展总长必须精确，拒绝重复扩展，并校验 ALPN 单选、空确认扩展和常用限制字段 |
| `xrtTlsCertificateVerifyParse()` | 签名方案保留未知线路值，签名非空且 16 位长度必须恰好消费正文 |
| `xrtTlsFinishedParse()` | 调用方传入协商后的验证数据长度；TLS 1.2 通常为 12，TLS 1.3 为当前 transcript 摘要长度 |
| `xrtTlsKeyUpdateParse()` | 正文必须恰好一字节且只能是 `not_requested` 或 `requested` |
| `xrtTlsSessionTicketParse()` | 区分 TLS 1.2 与 TLS 1.3 格式；TLS 1.3 票据必须非空、寿命不超过七天，early_data 扩展必须恰好四字节 |

未知扩展继续保留，协议状态机再验证它是否曾由对端提供、能否出现在当前消息和是否受本地策略允许。这样协议工具既不会提前封死未来扩展，也不会把重复类型、畸形长度或已知字段的局部错误拖到密码阶段。

## 语义消息编码

`tls_messages_write` 为上述消息提供精确 `Size()` 与失败原子的 `Encode()`：

```c
xtlscertificateentry Chain[2] = { 0 };
Chain[0].Data = LeafDer;
Chain[1].Data = IssuerDer;

size_t BodySize = xrtTlsCertificateSize(
	XTLS_VERSION_13, (xbytesview) { NULL, 0 }, Chain, 2
);
if ( (BodySize == 0) || !xrtTlsCertificateEncode(
	XTLS_VERSION_13, (xbytesview) { NULL, 0 }, Chain, 2,
	Body, BodyCapacity
) ) {
	return false;
}
```

- Certificate 与 NewSessionTicket 先验证全部字段、扩展、线路上限、容量和输入重叠，再写第一个字节；失败时输出不变。
- Certificate 接受调用方证书条目数组，不分配完整消息，不只发送叶证书，也不内置固定链数量。
- EncryptedExtensions、CertificateVerify 与 Finished 的单一负载使用 `memmove`，允许原位向前或向后腾出前缀。
- 多字段消息拒绝字段与目标区域重叠；描述结构本身在写入前做局部快照，避免前缀覆盖后续元数据。
- `Size()` 同时充当可编码性验证，不会静默丢弃 TLS 1.2 不存在的请求上下文、条目扩展、nonce 或 `age_add`。

语义编码只产生握手正文。调用方可以继续交给 `xrtTlsHandshakeEncode()` 生成四字节握手头；会话层则会在同一路径中追加 transcript 并交给记录保护层。原始扩展向量始终是逃生口，高级扩展 writer 只是可选构建器。

## 认证消息

`tls_auth_messages` 继续拆出认证状态机需要、但不应在客户端和服务端反复手写的线路对象：

| 对象 | 公开契约 |
| --- | --- |
| CertificateRequest | 分别解析 TLS 1.2 与 TLS 1.3 格式；证书类型、签名方案、证书签名方案、请求上下文和颁发者名称都保留为借用视图 |
| TLS 1.2 ECDHE ServerKeyExchange | 发布命名组、公钥、完整 `ServerECDHParams` 和签名；`Parameters` 可直接拼接两个 random 后参与验签 |
| TLS 1.2 ECDHE ClientKeyExchange | 严格解析单个一字节长度 ECPoint |
| CertificateStatus | 当前只接受具有已知正文形状的 OCSP 类型，并发布不透明响应 |
| CompressedCertificate | 发布算法、声明的解压长度和压缩负载，不在 framing 层绑定 zlib、Brotli 或 Zstandard 后端 |

CertificateRequest 的颁发者名称使用公共两级 API：`xrtTlsAuthorities()` 验证完整 16 位总长，`xrtTlsAuthoritiesRead()` 零拷贝遍历任意数量的非空 DER `DistinguishedName`。TLS 1.2 允许空列表；TLS 1.3 一旦携带 `certificate_authorities` 扩展就必须至少有一个名称。TLS 1.3 的 `signature_algorithms` 是必选扩展，`signature_algorithms_cert` 缺失时由后续状态机按协议回退，不在解析结果中伪造一份列表。

```c
xtls12serverkeyexchange Exchange;

if ( !xrtTls12ServerKeyExchangeParse(Handshake.Body, &Exchange) ) {
	return false;
}

/* 验签输入是 client_random || server_random || Exchange.Parameters。 */
```

语法层不根据当前内建密码后端限制 `Group`、签名方案或压缩算法。未知线路值继续发布，协商状态机再检查它是否由本端提供、是否与证书和密码套件匹配，以及公钥长度是否符合所选组。这样新增命名组或外置压缩后端不需要修改基础 parser。OCSP `CertificateStatus` 是例外：未知状态类型没有标准化的可跳过正文形状，因此 parser 明确拒绝。

压缩证书 parser 只接受非空负载和 24 位非零声明长度。后续解压层必须在分配前检查每连接配置上限，要求实际输出恰好等于 `UncompressedSize`，再把输出交给普通 `xrtTlsCertificateParse()`；不能信任对端声明直接分配接近 16 MiB 的缓冲。

`tls_auth_messages_write` 为每个对象提供精确 `Size()` 与失败原子的 `Encode()`。CertificateRequest 和 ServerKeyExchange 是多字段消息，字段不得与目标写入区重叠；ClientKeyExchange、CertificateStatus 和 CompressedCertificate 只有一个不透明负载，使用 `memmove` 支持原位腾出线路前缀。`xrtTlsAuthoritiesSize()` / `Encode()` 接受名称视图数组，不限制条目数量，只受 16 位向量总长限制。

认证消息 API 不执行签名、验签、密钥交换、OCSP ASN.1 解析或解压。这些能力由会话、X.509、crypto 和可选压缩后端逐层组合，避免协议 framing 与算法实现形成不可裁剪的强耦合。

## 记录保护内部契约

记录保护层是握手与会话的内部底座，不额外公开一套让应用直接管理流量密钥的 API。它具备以下已压实边界：

- 每个 `record-key` 只管理一个发送或接收方向，序列号不会跨方向共享。
- TLS 1.3 使用静态 IV 异或 64 位序列号，外层类型固定为 `application_data`，打开后去除内层类型与零填充。
- TLS 1.2 AES-GCM 使用 4 字节静态盐和 8 字节显式序列号；ChaCha20-Poly1305 不在线路上发送显式 nonce。
- 只有认证成功才递增接收序列号；认证失败不会写出明文，也不会改变类型结果。
- AES-GCM 在序列号达到 `2^24` 前由会话层执行 TLS 1.3 KeyUpdate，TLS 1.2 则关闭连接；ChaCha20-Poly1305 在序列号耗尽前更新或关闭。
- TLS 1.3 单条记录可选零填充；TLS 1.2 AEAD 记录不接受这一参数。
- 明文与密文缓冲允许精确原位处理；TLS 1.2 AES 打开时明文起点是显式 nonce 后的密文起点。

记录后端不分配堆内存。会话层后续使用网络自适应缓冲承接密文和明文，不为每个连接预留固定 8K 记录数组。

## Transcript 与密钥调度

调度层是握手状态机的内部密码底座，不公开应用直接传入 traffic secret 的 API。它把摘要算法选择与握手解析分开，并保证以下契约：

- 一个 `xtlstranscript` 只保存协商密码套件实际使用的 SHA-256 或 SHA-384 状态，不同时计算两种摘要。
- transcript 支持任意分块追加和不结束状态的摘要快照；失败不会把未初始化或错误长度结果当成有效摘要。
- HelloRetryRequest 使用 RFC 8446 规定的 synthetic `message_hash` 重建 transcript，替换过程成功前不修改原状态。
- TLS 1.3 提供 HKDF-Extract、HKDF-Expand-Label、Derive-Secret 和 Finished 内部原语；secret、transcript hash 与输出长度都按所选摘要严格检查。
- `HkdfLabel` 支持协议允许的 249 字节调用方 label 和 255 字节 context，完整编码最大 514 字节，不沿用旧实现的 256 字节固定缓冲限制。
- TLS 1.2 PRF 流式处理 `label || seed`，不复制到固定数组；大于旧版 256 字节限制的 seed 仍可正常派生。
- SHA-256 和 SHA-384 是独立裁剪后端。只启用公共调度骨架时，所有算法请求以结构化 unsupported 错误拒绝。
- 调度原语不分配堆内存；摘要、HMAC 中间状态和临时 secret 在退出前清零。

TLS 1.3 Expand-Label 输出同时受 16 位线路长度和 HKDF 的 255 个摘要块限制。TLS 1.2 PRF 单次内部派生上限为 65535 字节；握手状态机仍应只请求协议实际需要的几十到数百字节。

## Alert

`xrtTlsAlertParse()` 和 `xrtTlsAlertEncode()` 处理严格的两字节 Alert 负载。`xrtTlsAlertName()` 返回稳定英文名称，便于日志和结构化诊断。

未知 Alert 描述仍会保留其线路数值，由会话状态机按照协议决定是否终止；非法 Alert 级别和错误长度会被拒绝。

## 所有权与线程

- 基础记录和 Alert API 不分配内存。
- 所有输入和解析结果都是借用视图。
- 纯编解码函数没有共享可变状态，可以并发调用。
- 当前线程已有错误不会因 `XTLS_AGAIN` 被清除或覆盖。

## 参考标准

- [RFC 8446: TLS 1.3](https://www.rfc-editor.org/rfc/rfc8446.html)
- [RFC 5246: TLS 1.2](https://www.rfc-editor.org/rfc/rfc5246.html)
- [RFC 6066: TLS 扩展与 SNI](https://www.rfc-editor.org/rfc/rfc6066.html)
- [RFC 7301: ALPN](https://www.rfc-editor.org/rfc/rfc7301.html)
- [RFC 8422: TLS 1.2 椭圆曲线密码](https://www.rfc-editor.org/rfc/rfc8422.html)
- [RFC 8879: TLS 1.3 证书压缩](https://www.rfc-editor.org/rfc/rfc8879.html)
- [RFC 9325: TLS 安全部署建议](https://www.rfc-editor.org/rfc/rfc9325.html)

## 范例与测试

- `examples/tls/messages/main.c`
- `examples/tls/auth_messages/main.c`
- `examples/tls/key_exchange/main.c`
- `examples/tls/context/main.c`
- `examples/tls/record/main.c`
- `tests/tls/test_tls.c`
- `tests/tls/test_tls_mutation.c`
- `tests/tls/test_tls_record.c`
- `tests/tls/test_tls_record_aes.c`
- `tests/tls/test_tls_record_chacha.c`
- `tests/tls/test_tls_handshake.c`
- `tests/tls/test_tls_handshake_mutation.c`
- `tests/tls/test_tls_handshake_reader.c`
- `tests/tls/test_tls_handshake_reader_limits.c`
- `tests/tls/test_tls_hello.c`
- `tests/tls/test_tls_hello_negative.c`
- `tests/tls/test_tls_hello_mutation.c`
- `tests/tls/test_tls_key_exchange.c`
- `tests/tls/test_tls_key_exchange_negative.c`
- `tests/tls/test_tls_context.c`
- `tests/tls/test_tls_context_negative.c`
- `tests/tls/test_tls_context_oom.c`
- `tests/tls/test_tls_hello_write.c`
- `tests/tls/test_tls_hello_write_limits.c`
- `tests/tls/test_tls_messages.c`
- `tests/tls/test_tls_messages_negative.c`
- `tests/tls/test_tls_messages_mutation.c`
- `tests/tls/test_tls_messages_write.c`
- `tests/tls/test_tls_messages_write_limits.c`
- `tests/tls/test_tls_auth_messages.c`
- `tests/tls/test_tls_auth_messages_negative.c`
- `tests/tls/test_tls_auth_messages_mutation.c`
- `tests/tls/test_tls_auth_messages_write.c`
- `tests/tls/test_tls_auth_messages_write_limits.c`
- `tests/tls/test_tls_schedule.c`
- `tests/tls/test_tls_schedule_sha256.c`
- `tests/tls/test_tls_schedule_sha384.c`
- `tests/single/test_single_tls.c`
- `tests/single/test_single_tls_auth_messages.c`
- `tests/single/test_single_tls_auth_messages_write.c`
- `tests/single/test_single_tls_key_exchange.c`
- `tests/single/test_single_tls_context.c`
- `tests/tls/test_tls_session.c`
- `tests/tls/test_tls_session_limits.c`
- `tests/tls/test_tls_session_negative.c`
- `tests/tls/test_tls_session_oom.c`
- `tests/tls/test_tls_session_record.c`
- `tests/tls/test_tls_session_record_oom.c`
- `tests/single/test_single_tls_session.c`
- `tests/single/test_single_tls_session_record.c`

## 公共会话底座

`tls_session` 位于纯协议层与客户端/服务端状态机之间，公共声明单独放在 `<xrt/tls_session.h>`。这个头文件明确组合 `<xrt/tls.h>` 与 `<xrt/net.h>`，而纯协议头 `<xrt/tls.h>` 不反向依赖网络。会话本身不调用 socket；后续客户端、服务端和 TCP 适配器共享同一对象与队列，不再维护第二套 TLS 实现。

会话创建入口分别属于后续 `tls_client` 和 `tls_server`。公共底座不发布 `bool is_server` 式构造器，避免角色配置混用；创建后的 `xtlssession` 由一个线程或 Worker 独占驱动，并持有一份 `xtlscontext` 引用。

三个持久队列都是惰性 `xnetbuf`：

- `Feed` 保存尚未处理的输入密文，提供复制、借用、接管、自定义释放和完整 `xnetbuf` 块链接管五种所有权入口。
- `Send` 保存等待底层传输发送的密文，通过 `Front` / `Spans` 借用视图并由完成路径精确消费。
- `Plain` 保存等待应用读取的明文，既支持 Span 零复制解析，也支持 `xrtTlsSessionRead()` 复制式便捷读取。

收到完整受保护记录时，会话按该记录的精确长度惰性取得一块临时 `Scratch`。认证成功后，应用数据块直接移动到 `Plain`，不再复制；认证失败、OOM 或会话销毁都会先擦除临时明文。`Scratch` 不是每连接常驻的第四个固定缓冲。

创建空会话不会分配任何队列块。`FeedLimit`、`SendLimit` 和 `PlainLimit` 是追加前检查的硬上限；达到上限返回 `XTLS_AGAIN`，队列、引用所有权、记录序号和当前线程错误都保持不变。`xrtTlsSessionFeedBuffer()` 在通过上限检查后零复制移动源 `xnetbuf` 的全部块，成功后源缓冲为空并可复用；`XTLS_AGAIN`、活动写预留或其他失败都保持源缓冲不变。会话与缓冲必须由同一线程或 Worker 驱动，借用块的原始存活约束不会因移动而改变。所有 `Consume` API 拒绝超过待处理字节数的完成通知，不会用静默截断掩盖适配器错误。会话销毁时，其拥有的明文块会在归还 Worker 缓冲池前安全擦除；借用或引用的外部输入永不被写回。

`xrtTlsSessionWrite()` 是应用明文写入入口。它按 TLS 记录上限拆分输入，并在 `SendLimit` 只容纳部分记录时返回 `XTLS_OK` 和实际短写长度；完全没有进展时才返回 `XTLS_AGAIN`。每条记录要么完整进入 `Send` 并消耗一个写序号，要么队列和序号均不改变。底层传输通过 `xrtTlsSessionSendFront()` 或 `xrtTlsSessionSendSpans()` 发送，再以 `xrtTlsSessionSendConsume()` 报告精确完成量。

`xrtTlsSessionClose()` 只排队一次 `close_notify`。收到对端 `close_notify` 时会话自动排队一次回应，等待本地密文排空后进入 `CLOSED`；重复调用不会产生重复 Alert。`xrtTlsSessionPeerAlert()` 可查询最后收到的 Alert。底层传输遇到 EOF 必须调用 `xrtTlsSessionEof()`：只有此前收到认证的 `close_notify` 才是正常关闭，否则报告 `XTLS_ERROR_TRUNCATED`，避免把截断攻击误认为正常 EOF。

客户端或服务端发现协议失败时，会话会按根错误映射并尽力排队一次 fatal Alert：握手密钥尚未建立时使用明文记录，建立后使用当前写 epoch 保护。Alert 排队成功与否不改变失败结果；OOM、发送硬上限或已经结束的传输都不能覆盖最初的结构化错误。会话随后进入 `FAILED`，调用方仍可通过 `SendFront` / `SendSpans` 排空已经生成的 fatal Alert，再关闭底层传输。

`xrtTlsSessionProtocol()` 借用返回状态机已经严格确认的 ALPN 选择，视图有效到会话销毁。尚未协商或对端没有选择 ALPN 时返回 `false`，不设置错误，也不修改输出；空会话或空输出参数才是 `XTLS_ERROR_ARGUMENT`。应用可以在会话进入 `READY` 后查询，协议适配器也可以在内部握手阶段据此选择 HTTP/1.1、HTTP/2 或自定义上层协议。

`xtlswait` 是等待原因位集合：`INPUT`、`OUTPUT`、`APPLICATION`、`IDENTITY` 和 `VERIFY` 可以组合。公开状态只包含 `NEW`、`HANDSHAKE`、`READY`、`CLOSING`、`CLOSED` 与 `FAILED`，内部握手步骤不进入 ABI。`XTLS_AGAIN` 和 `XTLS_CLOSED` 是正常控制结果，不创建、清除或覆盖 `xerror`。

## TLS 会话恢复对象

`<xrt/tls_resume.h>` 提供独立可裁剪的 `xtlsresume`，用于在 TLS 状态机、调用方缓存和后续连接之间传递恢复资产。对象创建时深拷贝 ticket、PSK、SNI、ALPN 和可选对端身份，一次精确分配后保持不可变；引用计数允许跨线程共享，最后一个引用释放前会清除整块内存。协议客户端不拥有全局缓存，应用可以直接保存对象，也可以在后续组合显式缓存策略。

```c
xtlsresumeconfig Config;
xtlsresumeinfo Info;
xtlsresume* Resume;

xrtTlsResumeConfigInit(&Config);
Config.Cipher = XTLS_AES_128_GCM_SHA256;
Config.Ticket = Ticket;
Config.Secret = Psk;
Config.ServerName = XRT_STR_LITERAL("example.com");
Config.Protocol = XRT_BYTES_LITERAL("h2");
Config.Lifetime = 3600;
Resume = xrtTlsResumeCreate(&Config);

if ( Resume != NULL ) {
	(void)xrtTlsResumeInfo(Resume, &Info);
	xrtTlsResumeRelease(Resume);
}
```

当前对象契约只接受 TLS 1.3：ticket 必须为 1 到 65535 字节，PSK 长度必须等于密码套件摘要长度，寿命必须位于 1 到 604800 秒。ALPN 最多 255 字节，SNI 拒绝内嵌空字节；可选 `PeerIdentity` 是不参与线路编码的调用方身份域，可保存证书或信任策略摘要。`xrtTlsResumeInfo()` 发布的全部视图只在对象引用存活期间有效，其中 `Secret` 是敏感只读视图，调用方不得修改或无保护地记录。

有效期使用 `[IssuedAt, ExpiresAt)` 半开区间；墙钟回退到签发时刻以前时对象安全失效，不会延长票据寿命。`xrtTlsResumeTicketAge()` 先验证有效期，再按毫秒向下取整并与 `AgeAdd` 执行协议规定的 32 位模加，失败时不修改输出。对象层只表达材料和时间，不决定票据替换、容量、并发或淘汰策略。

对象生命周期示例位于 `examples/tls/resume/main.c`，下一连接 ClientHello 示例位于 `examples/tls/client_resume/main.c`；深拷贝、引用、时间边界、非法字段、OOM 和单头门禁位于 `tests/tls/test_tls_resume*.c` 与 `tests/single/test_single_tls_resume.c`。对象释放路径直接复用已经独立验证的 `xrtSecureZero()` 清零原语。

恢复对象可以直接交给下一条客户端连接。`Resume` 在创建期间由配置借用；创建成功后会话持有独立引用，因此调用方可以立即释放自己的引用：

```c
xtlsclientconfig ClientConfig;

xrtTlsClientConfigInit(&ClientConfig);
ClientConfig.Resume = Resume;
Session = xrtTlsClientCreate(&ClientConfig, Pool);
xrtTlsResumeRelease(Resume);
```

省略 `ServerName` 和 ALPN 列表时，客户端从恢复对象精确继承两者。显式 SNI 必须完全匹配票据绑定；显式 ALPN 列表必须包含票据协议，票据未绑定 ALPN 时则不能为恢复连接额外提供协议。过期、尚未生效、套件被当前策略禁用或路由域不匹配的对象都在会话分配前拒绝。



## TLS 客户端

`<xrt/tls_client.h>` 提供独立客户端构造入口，不把客户端字段塞进服务端配置，也不调用 socket。`xtlsclientconfig` 在创建期间借用共享上下文、SNI、ALPN 数组、可选 verifier 和可选恢复对象；创建成功后，会话持有这些共享对象的引用，并在同一块角色尾部内存中保存名称、协议名、临时密钥、扩展工作区和初始 ClientHello 的独立快照。

```c
static const xstrview Protocols[] = {
	{ "h2", sizeof("h2") - 1u },
	{ "http/1.1", sizeof("http/1.1") - 1u }
};
xtlsclientconfig Config;
xtlssession* Session;

xrtTlsClientConfigInit(&Config);
Config.ServerName = XRT_STR_LITERAL("example.com");
Config.Protocols = Protocols;
Config.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
Session = xrtTlsClientCreate(&Config, Pool);
```

空配置使用默认共享策略、无 SNI 和无 ALPN。构造器只发布当前裁剪构建可以完整执行的协议参数：密码套件必须同时具备记录 AEAD 和对应摘要调度后端，组必须具备密钥交换后端，签名方案必须适用于发布的版本。当前角色状态只接入 TLS 1.3，因此即使上下文同时允许 TLS 1.2，ClientHello 也不会虚假发布 TLS 1.2；纯 TLS 1.2 策略会以 `XTLS_ERROR_VERSION` 明确失败。

创建成功会立即生成密码安全随机数、兼容 session ID 和首选可用组的一份 key share，并排队一条完整明文 ClientHello 记录。SNI、ALPN、版本、组、签名和 key_share 均使用公共 Hello writer 编码，不保留固定 1024 字节构建缓冲；发送队列仍受上下文 `SendLimit` 约束。此时会话进入 `XTLS_STATE_HANDSHAKE`，等待原因同时包含输入和输出，传输层可直接发送 `xrtTlsSessionSendFront()` 或 `xrtTlsSessionSendSpans()` 返回的密文。

`xrtTlsClientDrive()` 在上下文记录与握手预算内消费已经 Feed 的线路记录。普通 TLS 1.3 ServerHello 必须精确回显兼容 session ID，只能选择实际写入 ClientHello 的套件和 key share，并同时携带唯一的 `supported_versions` 与 `key_share`。ServerHello 可以跨任意数量的明文记录重组；兼容 CCS 只接受单字节 `1`，Alert、意外明文应用数据、尾随握手消息和未 offer 的选择都不会穿过状态机。

有效 ServerHello 先在临时状态中完成 transcript、ECDHE、handshake secret、双向 traffic secret 和记录 key/IV 派生，再消费记录并一次替换收发 epoch。任一步失败都会进入 `XTLS_STATE_FAILED`，不会发布半组密钥；成功后立即擦除客户端临时私钥，接收方向使用服务端流量秘密，发送方向使用客户端流量秘密。`XTLS_AGAIN` 只表示需要更多输入或后续阶段尚未推进，不覆盖线程错误。

握手密钥切换后，EncryptedExtensions 必须位于 TLS 1.3 受保护记录中。状态机允许 SNI 确认、`supported_groups` 和 ALPN，拒绝 `early_data`、未知扩展、未请求的 SNI/ALPN 和不在客户端真实线路 offer 中的协议。消息支持跨加密记录重组；一条记录内的后续握手消息不会被提前消费，而是以稳定记录偏移留给下一状态。只有整条 EE 通过解析、出现位置和 offer 校验后，transcript 与 ALPN 才会一起提交。

Certificate 使用严格公共 parser，拒绝非空请求上下文和客户端未请求的每个 CertificateEntry 扩展。状态机先精确测量完整线路链，再用一次分配保存证书视图与独立 DER；证书视图数组按目标 ABI 的 `xx509cert` 对齐，不能假设父对象尾地址天然满足 64 位字段对齐。链长度没有固定张数上限，只受握手消息和整数上限约束。信任验证和 transcript 更新都成功后才发布对端快照。CertificateVerify 必须使用 ClientHello 真实提供的方案，并以消息加入 transcript 之前的摘要验证 RFC 8446 上下文签名；自定义信任回调不能绕过此步骤。

启用客户端验证后，`xrtTlsClientCertificateCount()` 返回当前已经认证的链长度，`xrtTlsClientCertificate()` 按索引借用稳定到会话销毁的 `xx509cert`；原始 DER 位于 `Certificate.Raw`，不需要重复的 DER 指针和长度 API。握手尚未收到证书或本次使用 PSK 恢复时数量为零，因为恢复线路没有重传证书；按索引查询此时返回状态错误，不能把票据中的身份摘要伪装成证书。

CertificateVerify 通过后，客户端严格按服务端握手流量秘密校验 Finished。校验使用加入服务端 Finished 之前的 transcript；成功后先把服务端 Finished 纳入临时 transcript，再派生主秘密和双向应用流量秘密，并以仍在生效的客户端握手写 epoch 排队客户端 Finished。只有该记录完整进入有界 Send 队列后，状态机才一次提交 transcript、应用收发 epoch 和 `XTLS_STATE_READY`；输出空间不足返回 `XTLS_AGAIN`，不会消费服务端 Finished 或提前切换密钥。

进入 READY 后，同一个 `xrtTlsClientDrive()` 继续处理应用记录、Alert 与后握手消息。应用明文以 Scratch 块零拷贝移动到 Plain 队列；达到 `PlainLimit` 时返回 `XTLS_AGAIN` 并发布 `XTLS_WAIT_APPLICATION`，挂起记录不会重复解密或再次递增接收序列号。收到 `close_notify` 后客户端自动排队一次响应，忽略对端随后发送的应用数据，并在本地响应排空后进入 `CLOSED`。

TLS 1.3 KeyUpdate 必须独占一条记录。客户端收到 `update_not_requested` 时只替换读取 secret/key；收到 `update_requested` 时先用旧写 epoch 排队 `not_requested` 应答，再一次提交新收发 epoch。强制应答遇到 `SendLimit` 会保持活动 secret、写序列号和挂起记录不变，待输出排空后重试。应用也可以主动轮换写方向：

```c
xtlsresult Result = xrtTlsClientKeyUpdate(
	Session,
	XTLS_KEY_UPDATE_REQUESTED
);
```

主动消息同样使用旧写 epoch，只有完整记录入队后才替换客户端写 secret/key；`XTLS_AGAIN` 不会让连接进入半更新状态。线路队列保证已经排队的旧 epoch 数据、KeyUpdate 和随后新 epoch 数据保持调用顺序。

启用 `XRT_FEATURE_TLS_CLIENT_RESUME` 后，客户端从包含客户端 Finished 的最终 transcript 派生 resumption master secret，再按每张 `NewSessionTicket` 的 nonce 派生独立 PSK。发布对象深拷贝 ticket、实际 SNI、实际协商 ALPN 和已验证叶证书 SHA-256 身份；`MaxEarlyData` 只保存服务端限制，不代表当前客户端支持发送 0-RTT。

`ResumeLimit` 默认为 4，可设为 0 禁用保存，最大为 64。队列槽位包含在客户端角色的单块尾部内存中，不为节点单独分配；满队列淘汰最旧票据并保留最新票据。`xrtTlsClientResumeCount()` 查询数量，`xrtTlsClientResumeDropped()` 统计显式禁用、容量淘汰和恢复对象 OOM，`xrtTlsClientTakeResume()` 按接收顺序把会话持有的引用转移给调用方。空队列返回 `NULL` 且不改变错误状态；对象缓存、目标选择和跨连接并发仍由调用方拥有。已进入 `READY` 的连接不会因为可选票据缓存 OOM 失败，该票据只计入丢弃统计且不会泄漏临时线程错误。

`NewSessionTicket` 支持跨记录重组和同记录多消息，畸形寿命、长度、空票据或已知扩展会使客户端失败。下一条 ClientHello 最多提供一张外部票据，同时发送 `psk_dhe_ke`、正常 X25519 key share 和位于扩展列表末尾的 `pre_shared_key`。binder 从最终编码的 ClientHello 截断 transcript、票据 PSK 和套件摘要独立计算；当前不发送 0-RTT。

服务端可以不选择 PSK，此时客户端安全回退到完整证书握手。服务端选择时只接受 identity `0`，套件摘要必须与票据一致且仍必须提供有效 key share；接受后客户端跳过 Certificate/CertificateVerify，直接校验服务端 Finished，再按正常路径发送客户端 Finished 并切换应用 epoch。恢复连接的 ALPN 必须与票据绑定完全一致，后续新票据继承原来已经认证的 `PeerIdentity`。`xrtTlsClientResumed()` 在服务端 ServerHello 接受 identity `0` 后返回 `true`；它报告协商选择，不把后续握手成功状态混入同一个查询。

当前门禁已经覆盖证书认证和 PSK+DHE 恢复型 TLS 1.3 客户端从 ClientHello 到持续 READY 的主路径：独立复算 binder、双方 Finished、应用流量秘密、resumption master 与 ticket PSK，验证恢复回退、错误 identity、缺失 key share、ALPN 绑定、双向应用数据、认证关闭、KeyUpdate 两种请求、主动更新、旧/新 epoch 顺序、票据分片/聚合/有界淘汰/深拷贝路由域、应用及输出背压、非法消息、目标 OOM、GCC、TinyCC x86 和单头文件。客户端同时支持带 extended master secret 的 TLS 1.2 ECDHE 证书完整握手、ALPN、双向应用数据与认证关闭；TLS 1.2 不提供会话恢复、重新协商或 KeyUpdate。没有 verifier 的客户端仍可生成首航，但收到证书认证航班时会以 `XTLS_ERROR_VERIFY` 安全失败。HRR、CertificateRequest、客户端证书、0-RTT 和自动密钥轮换策略仍属于后续工作。对应门禁位于 `tests/tls/test_tls_client.c`、`tests/tls/test_tls_client_server_hello.c`、`tests/tls/test_tls_server12.c`、`tests/tls/test_tls_client_oom.c`、`tests/single/test_single_tls_client.c` 和 `tests/single/test_single_tls_client_server_hello.c`。



## TLS 服务端

`<xrt/tls_server.h>` 提供独立服务端入口，不把服务端身份、SNI 路由或票据缓存塞进客户端配置，也不直接持有 socket。`xtlsserverconfig` 在创建期间借用共享上下文和静态身份；成功后会话持有引用并深复制 ALPN 列表。至少必须提供静态 `Identity` 或同步 `Select`，从而保证未知 SNI 或恢复回退仍有明确认证路径。

```c
static const xstrview Protocols[] = {
	XRT_STR_LITERAL("h2"),
	XRT_STR_LITERAL("http/1.1")
};
xtlsserverconfig Config;
xtlssession* Session;

xrtTlsServerConfigInit(&Config);
Config.Identity = Identity;
Config.Protocols = Protocols;
Config.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
Config.RequireProtocol = true;
Session = xrtTlsServerCreate(&Config, Pool);
```

会话创建时不生成输出，只进入 `HANDSHAKE` 并等待 ClientHello。传输层把收到的密文交给 `xrtTlsSessionFeed*()`，调用 `xrtTlsServerDrive()`，再从公共 `Send` 队列发送服务端航班；应用数据、关闭、等待原因和所有权接口与客户端共享。TLS 1.3 完整证书航班依次生成 ServerHello、EncryptedExtensions、Certificate、CertificateVerify 和 Finished，验证客户端 Finished 后原子切换到应用 epoch 与 `READY`。TLS 1.2 路径要求 extended master secret，执行 ECDHE 证书完整握手并在 ChangeCipherSpec 边界原子切换记录 epoch；它支持 SNI、ALPN、双向应用数据和认证关闭，不提供会话恢复、重新协商或 KeyUpdate。

服务端首航的扩展表、证书条目、签名输入和临时编码流来自会话内的惰性临时 Arena。
空闲连接和仅创建未握手的会话不分配 Arena 块；首航完成后立即安全清零并释放全部块，
不会为每个连接保留固定握手缓冲。TLS 1.2 与 TLS 1.3 复用同一个 Arena 和清理路径，
错误、OOM 与销毁也执行相同的安全释放；最终需要排队发送的记录仍由公共 `Send` 队列
独立拥有，不借用已经清理的临时内存。

`Select` 在 ClientHello 完成严格解析、SNI/ALPN 提取之后，任何服务端输出生成之前执行。请求中的 SNI 和完整 ALPN 扩展负载只在回调期间借用；`xtlsserverchoice` 初始包含静态身份和按服务端偏好计算的协议下标，回调可以替换身份或协议。返回身份按共享对象处理，选择结果在回调返回后由会话持有。`XTLS_SERVER_PROTOCOL_NONE` 表示不协商 ALPN；`RequireProtocol` 为 `true` 时没有共同协议会明确失败。回调上下文只借用到首航完成，不能递归驱动同一会话。

`xrtTlsServerName()` 返回服务端从 ClientHello 深复制的 SNI，视图稳定到会话销毁；`xrtTlsSessionProtocol()` 返回最终 ALPN。进入 READY 后，`xrtTlsServerKeyUpdate()` 与收到的 KeyUpdate 都遵循“旧 epoch 完整排队，新 epoch 一次提交”的顺序，发送背压和分配失败不会发布半更新状态。

启用 `XRT_FEATURE_TLS_SERVER_RESUME` 后，`Resume` 回调接收 SNI、完整 ALPN 负载、不透明 ticket 和客户端计算的混淆年龄。回调返回借用的不可变 `xtlsresume`；会话在回调返回后立即增加引用，再校验版本、套件、SNI、ALPN、有效期和年龄容差。未找到票据或路由绑定不匹配会安全回退到完整证书握手；已经匹配同一票据元数据但 binder 错误属于认证失败，必须发送 fatal Alert，不能降级绕过认证。当前恢复始终保留 ECDHE key share，只支持 PSK+DHE，不支持纯 PSK 或 0-RTT。

READY 服务端使用 `xrtTlsServerTicket()` 把调用方给出的非空不透明 ticket 和寿命编码为 NewSessionTicket，并把对应服务端恢复对象的所有权交给调用方；`xrtTlsServerTicketNew()` 使用 32 字节密码安全随机 ticket 和 86400 秒默认寿命。XRT 不维护进程全局票据缓存，调用方可以按租户、容量、过期和持久化需求选择 Map、分片缓存或外部存储。发送硬上限会在随机数、派生和分配之前预检；返回 `XTLS_AGAIN` 时输出对象为 `NULL`，写序号、traffic secret 和恢复状态均不变，排空输出后可原样重试。

服务端门禁覆盖随机小分片 TLS 1.2/1.3 证书握手、SNI/ALPN 动态选择、双向应用数据、认证关闭、TLS 1.3 主动和被动 KeyUpdate、明文与受保护 fatal Alert、票据签发、第二连接 PSK+DHE 恢复、未知或过期票据回退、SNI/ALPN/年龄绑定、坏 binder、畸形 PSK 扩展、发送背压、定向 OOM 和单头文件。完整示例位于 `examples/tls/server/main.c`，测试位于 `tests/tls/test_tls_server*.c` 与 `tests/single/test_single_tls_server.c`。当前没有 HRR、客户端证书认证、0-RTT、TLS 1.2 会话恢复、重新协商或异步身份选择；TCP 组合入口由后文独立的 `tls_stream` 裁剪单元提供。

## TLS 对端验证

`<xrt/tls_verify.h>` 把信任决策从客户端状态机和 X.509 原语之间独立出来。`xrtTlsVerifierCreate()` 深复制可选 `xx509store`，因此创建后可以释放或修改来源 store；验证器本体不可变、引用计数共享，并要求自定义回调可以并发执行。没有回调时必须提供至少一个 trust anchor，空 store 会在创建期失败。

`xrtTlsVerifierRetain()` 增加共享引用，`xrtTlsVerifierRelease()` 释放引用并在最后一次释放时销毁快照。`xrtTlsVerifierVerify()` 使用配置的时间源、信任决策和附加策略完成一次对端验证；同一个验证器可以被多个 TLS 会话并发调用。

默认 `xrtTlsPeerVerify()` 使用调用方显式给出的时间验证路径，并按角色检查 `digitalSignature` KeyUsage 和 `serverAuth` / `clientAuth` EKU；服务端角色还按 RFC 9525 匹配请求名称。线路链按叶到根排列，trust anchor 来自 store 且不要求出现在对端链中。调用方可以传入 `xtlsverifypolicyproc`，在这些默认检查全部成功后读取 `xtlsverifiedpeer`：`Path` 按叶到根排列但不包含独立 `Anchor`，全部视图只在回调期间借用。策略可以直接组合 CRL、OCSP、CT、证书固定或企业规则，不必重复建链和身份验证。

TLS 核心不会隐式加载系统根、下载 CRL、发起 OCSP 请求或提交 CT 查询；数据加载和联网属于独立可裁剪组合层，基础客户端不会在验证期间偷偷阻塞文件或系统 API。同步策略返回 `false` 时可以设置结构化错误，验证器会把它包装为 `XTLS_ERROR_VERIFY` 的 cause；没有设置错误时得到明确的 `XERR_PERMISSION` 拒绝。

```c
xtlsverifierconfig VerifyConfig;
xtlsclientconfig ClientConfig;
xtlsverifier* Verifier;

xrtTlsVerifierConfigInit(&VerifyConfig);
VerifyConfig.Store = TrustStore;
Verifier = xrtTlsVerifierCreate(&VerifyConfig);

xrtTlsClientConfigInit(&ClientConfig);
ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
ClientConfig.Verifier = Verifier;
Session = xrtTlsClientCreate(&ClientConfig, Pool);
xrtTlsVerifierRelease(Verifier);
```

`xtlsverifydecision` 是自定义 `xtlsverifyproc` 的完整结果域：`XTLS_VERIFY_ACCEPT` 接管信任，`XTLS_VERIFY_REJECT` 明确拒绝，`XTLS_VERIFY_DEFAULT` 回退到不可变 store，`XTLS_VERIFY_ERROR` 报告本次回调产生的结构化错误。回退时没有 store 会明确失败；错误结果必须在本次回调内设置错误，验证器隔离回调前后的错误槽并把本次错误保留为 cause。接受结果不会再调用只适用于默认路径的 `Policy`，但仍不能跳过 CertificateVerify 和 Finished。

`xtlsverifytimeproc` 为路径和附加策略提供同一个确定时间；省略时使用 `xrtNow()`。`Verify`、`Policy`、`Time` 可以并发调用，不能依赖线程局部的可变共享状态。配置的 `Context` 只在 `xrtTlsVerifierCreate()` 成功后转移所有权；最后一个引用释放时，`xtlsverifyreleaseproc` 恰好调用一次。

`xrtTls13CertificateVerifySignature()` 是公开的协议验签原语。它构造标准的 64 个空格、角色上下文、零分隔符和 transcript hash，严格区分 `rsae` / `pss` 密钥、P-256 / P-384 方案和 Ed25519，并拒绝 TLS 1.3 禁止的 PKCS#1 方案。它不执行证书路径验证，调用方若直接使用原语必须先完成信任决策。

完整示例位于 `examples/tls/verify/main.c`；基础契约、默认真实 RSA 路径、CRL 路径策略、OOM、客户端端到端航班、TinyCC x86 和单头门禁分别位于 `tests/tls/test_tls_verify*.c`、`tests/tls/test_tls_client_server_hello.c` 与 `tests/single/test_single_tls_verify.c`。`tests/tls/test_tls_verify_policy_rsa.c` 同时展示无 CRL、空 CRL、吊销和过期 CRL 的完整组合方式。

## TLS 身份

`<xrt/tls_identity.h>` 提供与网络和角色状态机解耦的共享身份对象。身份创建时深复制完整 DER 证书链，第一张证书必须是叶证书；`xrtTlsIdentityCertificate()`、`xrtTlsIdentityPublicKey()` 返回的借用视图一直有效到最后一个身份引用释放。身份创建后不可变，可由多个服务端配置和 Worker 并发共享。

| 构造器 | 私钥输入 | 验证 |
| --- | --- | --- |
| `xrtTlsIdentityRsa()` | PKCS#1 或未加密 PKCS#8 DER | 模数、指数、全部 CRT 参数、叶 SPKI 和 RSA-PSS 限制 |
| `xrtTlsIdentityP256()` | 32 字节标量、SEC1 或未加密 PKCS#8 DER | 标量范围、曲线 OID、SEC1 可选公钥和叶 P-256 SPKI |
| `xrtTlsIdentityP384()` | 48 字节标量、SEC1 或未加密 PKCS#8 DER | 标量范围、曲线 OID、SEC1 可选公钥和叶 P-384 SPKI |
| `xrtTlsIdentityEd25519()` | 32 字节种子、单/双层 DER OCTET 或 RFC 8410 PKCS#8 | 算法参数缺省、派生公钥和叶 Ed25519 SPKI |

构造器不借用私钥输入。内置身份与证书链保存在一块紧凑分配中，释放前安全清零；RSA 保留完整 CRT 视图，Ed25519 保留一次展开的签名密钥，避免每次握手重复派生。PEM、文件和系统证书库不属于身份核心依赖，调用方可先用对应层取得 DER，后续再由独立便捷加载模块组合。

`xrtTlsIdentityCanSign()` 同时检查 TLS 版本、证书身份类型、标准签名方案和后端限制。TLS 1.3 不会接受 RSA-PKCS#1 CertificateVerify，ECDSA 方案必须与 P-256/P-384 证书曲线一致；TLS 1.2 的 ECDSA 线路值仍按摘要/签名对解释，因此两条已实现曲线都可按对端选择使用 SHA-256、SHA-384 或 SHA-512。`rsaEncryption` 与 `RSASSA-PSS` 证书分别只进入 `rsa_pss_rsae_*` 和 `rsa_pss_pss_*` 路径。受限 RSA-PSS 密钥还必须满足证书与私钥两侧的摘要、MGF1、最小盐长和 trailer 约束；没有任何共同 TLS 方案的身份在构造时直接拒绝。

`xrtTlsIdentitySign()` 接收完整 TLS 待签内容。空输出查询精确签名长度，容量不足时不会调用签名器；内置后端通过临时结果或底层原子发布契约保证失败不发布部分签名。RSA-PSS 使用密码安全随机盐，ECDSA 按线路摘要使用 RFC 6979 确定性 low-S 签名，Ed25519 使用纯 Ed25519 模式。

```c
size_t iSignatureSize = 0;

if ( !xrtTlsIdentitySign(
	Identity, XTLS_VERSION_13,
	XTLS_SIGNATURE_ED25519, Content,
	NULL, 0, &iSignatureSize
) ) {
	return false;
}
```

`xrtTlsIdentityCreate()` 是真实扩展接口，用于 HSM、系统密钥库或远程签名器。配置中的证书链仍由 XRT 深复制；签名上下文只在创建成功后转移所有权，最后释放时调用 `Release`。`Supports` 与 `Sign` 必须允许并发调用；`Supports` 是不设置错误的能力谓词，`Sign` 失败时必须保持输出和长度不变。没有硬件或系统密钥需求时应使用内置强类型构造器。

身份层错误使用 `xrt.tls` 的 identity 错误码，并把 DER、X.509 或 crypto 失败保留为 `Cause`。因此上层宿主可以按 TLS 阶段映射，C 调用方也能继续读取底层格式或密码原因。

裁剪宏相互独立：

- `XRT_FEATURE_TLS_IDENTITY`：共享对象、证书快照、引用和外部签名器。
- `XRT_FEATURE_TLS_IDENTITY_RSA`：RSA PKCS#1/PKCS#8、PKCS#1 与 PSS 签名。
- `XRT_FEATURE_TLS_IDENTITY_EC`：共享 SEC1/PKCS#8 EC 格式解析，不单独发布构造器。
- `XRT_FEATURE_TLS_IDENTITY_P256`、`XRT_FEATURE_TLS_IDENTITY_P384`：各自曲线身份。
- `XRT_FEATURE_TLS_IDENTITY_ED25519`：Ed25519 身份。

完整示例位于 `examples/tls/identity/main.c`；模块化、负向、OOM、组合和单头门禁位于 `tests/tls/test_tls_identity*.c` 与 `tests/single/test_single_tls_identity*.c`。

## TLS-over-TCP 组合流

`<xrt/tls_stream.h>` 以 `XRT_FEATURE_TLS_STREAM` 独立裁剪，把公开 TCP Stream 与公开 TLS 客户端/服务端会话组合成事件驱动明文字节流。它不复制握手、验证、身份或记录状态机，也不让 TLS 原语反向依赖 socket；高级用户仍可直接使用传输无关会话层。

客户端连接数字地址使用 `xrtTlsStreamConnect()`。已经通过代理、自定义拨号或
明文协议协商得到 TCP Stream 时，在其所属 Worker 上调用
`xrtTlsStreamClient()`；它成功后接管 TCP 引用，后续只从 TLS Stream 读写。
需要自行创建角色 Session 的底层用户可以调用 `xrtTlsStreamAttach()`，成功时
同时转移 Session 与 TCP 引用。服务端在 TCP `Accept` 回调内调用
`xrtTlsStreamAccept()`，并把其布尔结果直接作为 Accept 结果返回：

```c
static bool acceptTls(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	server* pServer = (server*)pData;

	(void)pListener;
	return xrtTlsStreamAccept(
		pTransport,
		&pServer->Tls,
		&pServer->Stream,
		&pServer->Events,
		pServer,
		NULL
	);
}
```

`xrtTlsStreamAccept()` 成功后接管 TCP Accept 交付的调用方引用；失败时保持原
Accept 失败回收规则。`xrtTlsStreamAttach()` 和 `xrtTlsStreamClient()` 只接受
已经发布 Open、尚未读结束、写结束、Close 或 Abort 的双向 Stream，必须在该
Stream 的 Worker 上调用。调用方必须在升级点停止直接收发和改变 TCP 状态。
失败时输出清空，不接管输入引用，不改变 Session 的缓冲池归属，也不替换 TCP
事件或用户数据；修正配置后可用同一 Session 与 Transport 重试。它们会立即
处理 TCP 缓冲中已有的密文，因此代理响应后的尾随 TLS 记录不会丢失。成功时
`ppStream` 返回独立调用方引用。
`xrtTlsStreamConnect()` 成功同样返回调用方引用。`xrtTlsStreamDestroy()` 只释放
引用，不隐式关闭；运行时引用会保持对象活到唯一 `Close` 回调结束。

完成握手后，可以在所属 Worker 上调用
`xrtTlsStreamSetEvents(pStream, pEvents, pData)` 原子替换事件表和用户数据。
它用于 HTTPS Upgrade、WebSocket 和应用协议协商后的处理器接管；不会再次发布
`Open`，也不会重放切换前已经留在明文缓冲中的数据。接管层必须在切换点显式
取得并处理协议余量。原始 TCP 仍由 TLS 组合流独占，不能借此绕过 TLS 接管。

`xrtTlsStreamData()` 以 acquire 语义返回当前用户数据的借用指针快照；事件切换
在所属 Worker 上保持顺序，但查询可以来自任意线程。快照不延长目标生命周期，
指针所指对象的存活期与并发访问仍由调用方管理。`xrtTlsStreamRef()` 在引用计数
耗尽时返回空并设置状态错误，不会让已经释放或饱和的对象重新进入生命周期。

需要主机名时启用 `XRT_FEATURE_TLS_STREAM_DIAL` 并调用 `xrtTlsDial()`。该可选层
直接复用 `xrtNetDial()` 的 Resolver、双栈候选竞速、回退、取消和统计，不复制
另一套 DNS 或 TCP 连接器。数字地址使用 `xrtTlsStreamConnect()`；已有 TCP
Stream 使用 `xrtTlsStreamClient()` 或 `xrtTlsStreamAttach()`；服务端 Accept
使用 `xrtTlsStreamAccept()`；完全自定义传输仍可直接使用会话层。

```c
xtlsdialconfig Config;
xtlsdial* pDial;

xrtTlsDialConfigInit(&Config);
Config.Timeout = 15000000u;
pDial = xrtTlsDial(
	Engine,
	Resolver,
	"example.com",
	443,
	&Tls,
	&Config,
	&Events,
	App,
	dialDone,
	App
);
```

`Config.Transport` 控制 DNS、地址族、Happy Eyeballs、候选数和 TCP 阶段超时；`Config.Stream` 控制 TLS 握手、认证关闭和缓冲硬上限；`Config.Timeout` 是从提交开始覆盖 DNS、TCP 与 TLS 的全过程硬上限，零值表示只使用各阶段超时。`ServerNameFromHost` 默认开启，仅在 `Tls.ServerName` 为空时用 `sHost` 填充 SNI 与证书名称；显式名称始终优先，关闭该选项则不做自动填充。

通过配置校验后，受管 TLS Dial 会先取得一份 Engine 初始化租约，再创建客户端会话、组合 Stream、全过程 Timer 和底层 TCP Dial。只有这些活动对象已经接管 Engine 生命周期后，临时租约才会释放；任一创建步骤失败则在返回前完整回滚并保留原始 cause。并发 `xrtNetEngineDestroy()` 不能跨过这段尚未返回对象的初始化窗口。

成功时，最终 Stream 的用户 `Open` 先执行，随后 `xtlsdialproc` 以 `XNET_RESULT_OK` 交付同一 Stream 的调用方引用。握手完成前失败只发布一次 Dial 完成回调；半初始化 TLS Stream 在回调前释放，也不额外调用用户 Stream `Close`。`xrtTlsDialCancel()` 可以在解析、TCP 连接或 TLS 握手阶段竞争取消；返回真表示取消已经赢得唯一终态门，之后不能再发布成功。安全 Stream 已赢得发布权或 Dial 已终止时返回假。终态由 `xrtTlsDialState()` 与 `xrtTlsDialError()` 固定保存；底层 TCP Dial 已经终结 Engine 活动占用，但其只读快照保留到 TLS Dial 销毁，因此 `xrtTlsDialTransportStats()` 在 TLS 阶段和终态继续提供候选、尝试、失败和获胜地址统计。

启用 `XRT_FEATURE_TLS_STREAM_DIAL_FUTURE` 后，可以用同一受管拨号状态机直接取得
Future：

```c
xfuture* pDial = xrtTlsDialAsync(
	Engine,
	Resolver,
	"example.com",
	443,
	&Tls,
	&Config,
	&Events,
	App
);
```

成功 Future 持有一个已经 `OPEN` 的 `xtlsstream` 引用，用户 `Open` 回调仍先于
`XFUTURE_RESOLVED` 发布。`xrtFutureValue()` 返回借用指针；需要在销毁 Future 后
继续使用时，先调用 `xrtTlsStreamRef()` 保留独立引用。Future 值析构只释放它所
持有的引用，不隐式关闭其他调用方引用。

`xrtFutureCancel()` 把协作取消传递到当前 DNS、TCP 或 TLS 阶段，只有底层完成
清理后 Future 才进入 `XFUTURE_CANCELLED`。连接失败与全过程超时进入
`XFUTURE_FAILED`，`xrtFutureError()` 保留 TLS 包装错误和底层原因链。需要观察
候选统计或在完成前查询阶段时继续使用回调式 `xrtTlsDial()`；Future 入口是常见
连接路径的轻量适配器，不复制 Dial 状态机。

`Open` 只在 TCP 已连接、TLS 已到 `READY` 且 SNI/ALPN/证书验证全部完成后发布。客户端与服务端事件都在底层 TCP 所属 Worker 上串行执行。`Send`、`SendBound`、`Buffer`、`Read`、`Consume` 和 `Session` 必须在该 Worker 上调用；`Close`、`Abort`、`State`、`Available`、`Pending`、`Transport`、`Data` 和终态 `Error` 支持并发调用或快照读取。

```c
size_t iWritten = 0;
xtlsresult Result = xrtTlsStreamSend(
	Stream,
	Data,
	Size,
	&iWritten
);

size_t WireSize;
bool Sized = xrtTlsStreamSendBound(Stream, Size, &WireSize);
```

`Send` 允许成功短写。`XTLS_OK` 且 `iWritten < Size` 表示该前缀已原子受理，剩余数据由调用方保留；`XTLS_AGAIN` 保证 `iWritten == 0`。`xrtTlsStreamSendVec()` 先校验全部 Span 与总长度，再直接逐片生成记录，不分配一块拼接副本；`iWritten` 是跨 Span 的连续受理前缀，输入无效或总长度溢出时保持为零且不会修改会话。两种发送入口使用相同的背压契约。

`xrtTlsStreamSendBound()` 在所属 Worker 上返回一次 `Send` 对指定明文产生的精确密文线路长度，包含每条记录的头、显式 nonce、TLS 1.3 内层类型和认证标签。它不修改会话、序列号或发送队列；零长度返回零，算术溢出或输出与 Stream/Session 重叠时失败且不修改输出。协议适配层可用它在接受明文前把自身队列预算换算成真实 TLS 线路成本。

背压会登记一个边沿，TLS 发送队列和 TCP 队列重新具备容量后发布 `Writable`。应用必须在 `Writable` 中从未受理偏移继续发送，不能重发已经计入 `iWritten` 的前缀。每次至少受理一个字节后会登记 `Drain`；只有 TLS 密文队列与 TCP 用户态发送预算同时归零才发布该事件。`Writable` 与 `Drain` 均不会重入尚未返回的 `xrtTlsStreamSend()` 或 `xrtTlsStreamSendVec()`，因此调用方可以在返回后再统一提交 `iWritten`。

`xrtTlsStreamPending()` 返回 TLS Session 尚未转移的密文与底层 TCP 用户态发送
队列的饱和相加快照。它不包含已被操作系统接受的内核缓冲字节，也不代表对端
已经读取；成功短写后允许立即为零。该查询可从任意线程用于统计和限流，
`Drain` 回调发布时它必须为零。

适配器要求 TCP `WriteLimit >= TLS SendLimit`，从而把一批完整 TLS 密文块链全有或全无地转移给 TCP。常规路径不复制密文；TCP ReadBuffer 可以整体移动进 TLS Feed。当一次 TCP 输入大于 Feed 剩余容量时，只复制可容纳的前部 Span，消费后继续，避免以固定 8 KiB 缓冲或无界增长掩盖压力。成功短写与同步 TCP 低水位回调之间有重入门；同步产生的 `Writable`/`Drain` 会转为同一 Worker 的内部命令，外层发送返回后才允许进入应用。

`Read` 回调借用 `const xnetbuf*`。应用可以用 `xrtTlsStreamRead()` 复制并消费，也可以检查 `xrtTlsStreamBuffer()` 后以 `xrtTlsStreamConsume()` 精确确认已处理字节。默认通知采用受控边沿语义：当前明文没有全部消费前暂停新的 TCP 接收。增量协议解析器在保留不完整前缀时可调用 `xrtTlsStreamReadMore()`；TLS 会在 `PlainLimit` 内继续解密，只在明文增长后再次发布 `Read`，并要求限制中仍能容纳一条最大明文 record，避免有空间但无法取得下一条完整记录的永久停滞。请求待完成期间不能替换事件接收者。`xrtTlsStreamPullup()` 只按需连续化精确前缀，不消费明文。借用不能保存到下一次回调；消费到零后恢复普通残留密文处理和底层读取。

### TLS Stream Future

启用 `XRT_FEATURE_TLS_STREAM_FUTURE` 后，同一个 TLS Stream 可以从任意线程使用
Future 入口，不需要为同步等待或协程再建立一套连接对象：

```c
xfuture* pSend = xrtTlsStreamSendAsync(Stream, Data, Size);
xfuture* pDrain = xrtTlsStreamWaitAsync(
	Stream,
	XTLS_STREAM_WAIT_DRAIN
);
xfuture* pReceive = xrtTlsStreamRecvAsync(Stream, 64u * 1024u);
```

`xrtTlsStreamSendAsync()` 和 `xrtTlsStreamSendVecAsync()` 在提交期间复制完整输入，
调用返回后不再借用原数据。多个发送保持严格 FIFO；成功 Future 表示全部明文
已经被 TLS 会话受理，不表示密文已经离开 TCP 用户态队列，更不表示对端已经
读取。需要本地排空时另行等待 `XTLS_STREAM_WAIT_DRAIN`。

`xrtTlsStreamClose()` 与异步发送接纳共享一个线性化门。关闭调用之前已经成功
返回 Future 的发送会先保持 FIFO 完整受理，随后才生成 `close_notify`；关闭门
生效后的新发送立即以 `XERR_STATE/XTLS_ERROR_STATE` 拒绝。构造期间的 OOM
会完整归还预算并继续被延迟的关闭，不会让连接永久停在 OPEN。零字节发送是
合法的 FIFO 节点，在轮到它时成功完成且不产生 TLS 应用记录。

发送取消只在首个字节被 TLS 会话受理前有效。尚未开始的节点确认
`XFUTURE_CANCELLED` 并完整归还预算；已经发生成功短写的节点忽略后续取消请求，
继续按原顺序发送剩余后缀，最终成功或报告真实连接终态。这个规则避免取消把
一个应用消息静默截成线路前缀。

`xrtTlsStreamRecvAsync()` 是 pull 模式入口。它在所属 Worker 上先为结果分配独立
存储，再消费当前可用明文；成功值是由 Future 持有的 `xnetbytes`。读取内容统一
调用 `xrtNetBytesView()`；通过 `xrtNetBytesRef()` 增加引用后，结果可以越过 Future
生命周期继续使用。`iMaxBytes == 0` 读取当前全部明文。结果分配失败不会
消费任何字节，恢复内存后可以重试。一个 Stream 不能同时安装 `Read` 回调并
登记 READ/Recv Future；双向切换都以 `XERR_STATE/XTLS_ERROR_STATE` 拒绝，防止
两条路径竞争消费同一明文。

已经通过 TLS 记录认证并解密的明文不会因为随后发生 TCP 截断、协议失败或本地 Abort
而被丢弃。终态 Stream 的 `RecvAsync` 先返回这些缓冲字节；缓冲耗尽后，下一次接收
才返回稳定的失败或关闭结果。Stream 的 `FAILED` 状态和 `xrtTlsStreamError()` 在读取
期间保持不变，因此截断敏感的上层协议仍能明确拒绝不完整消息。

`xrtTlsStreamWaitAsync()` 提供六个水平条件：

| 条件 | 完成点 |
| --- | --- |
| `OPEN` | TCP、TLS 握手和认证全部完成 |
| `READ` | 至少一个明文字节可消费 |
| `WRITE` | 异步发送 FIFO 为空且当前可受理明文 |
| `DRAIN` | 异步发送 FIFO、TLS 密文和 TCP 用户态发送队列全部为空 |
| `END` | 收到并认证对端 `close_notify`，且此前明文已经全部交付 |
| `CLOSE` | TLS Stream 到达最终传输终态 |

`xtlsstreamconfig` 的 `AsyncBytesLimit`、`AsyncCountLimit` 是独立硬边界，
`AsyncBatch` 限制一次 Worker 轮转最多完成的节点数。三个值都必须非零。
超出字节边界返回 `XERR_RANGE/XTLS_ERROR_LIMIT`，并发操作数饱和返回
`XERR_AGAIN/XTLS_ERROR_LIMIT`；失败提交不会残留节点或预算。
`xrtTlsStreamAsyncBytes()` 与 `xrtTlsStreamAsyncCount()` 提供无锁并发快照。

所有 Promise 终态都由所属 Worker 确认；已经终止并失去 Worker 的对象允许在
调用线程立即返回固定结果。认证关闭使 END/CLOSE 成功，普通对端 EOF 或 TLS
协议错误使挂起操作失败并保留 `xrtTlsStreamError()` 根因，本地主动 Abort 使
挂起操作进入 `XFUTURE_CANCELLED`。通用 `xrtFutureWait*()` 和
`xrtFutureAwait*()` 可以直接消费这些 Future，不增加 TLS 专用协程 API。
已认证应用明文始终先于 END 和接收侧 `XFUTURE_CLOSED` 交付；即使最后一个
应用记录与 `close_notify` 同批到达，也不会出现先观察 EOF、后出现残留明文的
窗口。

固定大小等待、接收元数据和总分配不超过 1 KiB 的发送节点共享所属 Worker 的
`NodeCacheBytes` 预算；较大发送保持一次普通堆分配，不把载荷塞入小节点缓存。活动
缓存节点持有临时 Engine 租约，节点归还后才释放。TLS Stream 发布最终 Close 后，
新建 Future 使用独立堆且不访问底层 TCP Worker，因此调用方保留的终态 TLS Stream
可以晚于 Engine 销毁，并继续取得 Close、EOF 或固定失败结果。

`xrtTlsStreamClose()` 排队一次 `close_notify`，排空 TCP，并等待对端经过认证的 `close_notify`。收到对端通知时先发布一次 `End`，双向认证关闭和 TCP 终态都完成后才发布 `CLOSED/XNET_RESULT_OK`。握手超时默认 10 秒，认证关闭超时默认 5 秒；配置值为零显式禁用对应 Timer。对端直接 EOF 映射为 `XTLS_ERROR_TRUNCATED`，关闭等待到期映射为 `XERR_TIMEOUT/XTLS_ERROR_CLOSED`。`Abort` 不生成 Alert 并立即放弃 TCP；若 TLS 已经失败但仍在发送 fatal Alert 或等待传输收尾，`Abort` 仍会加速关闭，同时保留原来的失败结果和根因。只有尚无失败根因的主动 Abort 才发布取消结果。

第一个 TLS、验证、内存、Timer 或传输根因保存在对象中，不会被后续关闭错误覆盖。失败 `Close` 回调中的 `pError` 和 `xrtTlsStreamError()` 都借用该稳定原因；底层 TCP 失败以 TLS 组合错误包装并保留原 Cause。正常 `CLOSED` 的 `xrtTlsStreamError()` 始终为空。

`xrtTlsStreamSession()` 是 Worker 内高级查询入口，可读取 ALPN、恢复票据等会话资产。`xrtTlsStreamTransport()` 借用原始 TCP Stream，只用于地址、统计和标准库尚未覆盖的只读/安全选项；应用不得关闭、收发、切换阻塞模式、替换事件或直接消费其缓冲。确实需要自定义传输行为时应回到公开会话层，而不是破坏组合对象状态机。

启用 `XRT_FEATURE_TLS_CLIENT_RESUME` 时，`xtlsstreamevents.Ticket` 在客户端
恢复队列新增 ticket 后于所属 Worker 发布。回调只表示“现在有票据可取”，
不转移 ticket，也不延迟 `Open`、HTTP 完成或连接关闭；处理器应通过
`xrtTlsStreamSession()` 取得会话，再循环调用 `xrtTlsClientTakeResume()`，
直到队列为空。队列已满并用新 ticket 替换最旧项时，长度虽然不变，仍会发布
新的边沿。切换 `xrtTlsStreamSetEvents()` 后，后续 ticket 只通知新的
处理器；已经在队列中的票据不会重放事件，接管层应在切换时主动排空一次。

完整 Echo 服务示例位于 `examples/tls/stream/main.c`，Future 用法位于
`examples/tls/stream_future/main.c`，使用系统信任库的主机名客户端位于
`examples/tls/dial/main.c`，Future Dial 位于
`examples/tls/dial_future/main.c`。select、IOCP 与 io_uring 共用同一份 TLS
Stream 和 TLS Dial 测试主体；后端文件只选择端口实现，不复制握手、背压、超时
或关闭断言。生命周期、Attach 失败原子性、失败后 Abort 根因保持、截断 EOF、
握手超时、认证关闭超时、单段和向量发送、向量失败原子校验、成功短写、
`Writable`/`Drain` 非重入、小 FeedLimit 大 TCP Read、延迟明文消费、组合对象
OOM、Timer 调度拒绝回滚、会话恢复、非法参数和单头文件真实传输门禁位于
`tests/tls/test_tls_stream*.c` 与 `tests/single/test_single_tls_stream*.c`。
Future 门禁另外覆盖 callback/pull 排他、并发硬预算、锁外错误构造、构造与结果
OOM、构造预留回滚、零字节发送、开始前取消、成功短写后的取消、关闭门之前
4 MiB 发送完整交付、关闭后的发送拒绝、同批应用记录先于 END、认证关闭、Abort、
异常关闭后的已认证明文、Worker 节点缓存复用、Engine 销毁后的终态 Future、
GCC/TinyCC、Select/IOCP 和通用协程恢复。主机名、验证名称、IPv6 到 IPv4 回退、
TCP 耗尽、解析期取消、全过程
超时、Timer 拒绝恢复、传输统计和 Open-before-Done 顺序由
`tests/tls/test_tls_stream_dial*.c` 压实。Engine Timer 扩容 OOM 的独立边界由
`tests/network/test_net_engine_oom.c` 压实。Future Dial 另外验证成功 Stream 在
Future 发布前已经 OPEN、保留引用后销毁 Future 不关闭连接、DNS/TCP/TLS 各阶段
协作取消、全过程超时、结构化原因链、Select/IOCP/io_uring 后端包装与通用协程
Await；公共桥接器的监听安装 OOM 由
`tests/concurrency/test_future_bridge_oom.c` 确定性覆盖。
