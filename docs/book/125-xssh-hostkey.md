---
num: 125
slug: xssh-hostkey
title: SSH（三）：主机密钥与 known_hosts
volume: 卷十一 其他扩展库
type: practice
lead: 无分配的主机密钥格式层、Ed25519 验签、指纹计算、known_hosts 三信任态与哈希主机名——SSH 信任判断的全部工具。
api: xssh-ssh_hostkey, xssh-ssh_known_host_db, xssh-ssh_fingerprint
---

## 导读

第 122 章的 VERIFY_HOST_KEY 事件把信任判断交给了应用——本章是判断的全部工具。**格式层**（`ssh_hostkey`）：通用公钥/签名的编解码（算法名 + 原始参数视图——新增算法不改公共前缀解析器）与 `ssh-ed25519` 固定格式；**验证层**（`ssh_hostkey_ed25519`）：签名验证的密码实现；**指纹**（`ssh_fingerprint`）：主机密钥的 SHA-256 指纹——"人比对密钥"的形态；**信任库**（`ssh_known_host`/`_db`/`_hash`）：known_hosts 格式的解析/匹配/写入、多信任态（MATCH/NEW/CHANGED/REVOKED/CERT——变更即改密警报）与哈希主机名（隐私形态）。known_hosts 是 SSH 信任模型的核心文件——"第一次记下、以后比对、变了就喊"的 TOFU 完整实现。

## 引入

第一次连接新服务器，客户端问"我要记下这个主机密钥，信任吗？"——这就是 TOFU（Trust On First Use）。此后每次连接把对端密钥与记录比对：一致放行；没有记录（新机器）提示；**不一致**——要么服务器换了密钥（重装/轮换），要么**有人在你和服务器之间**（中间人）。第三种情况是 known_hosts 存在的全部理由——它是唯一能挡"网络位置劫持"的客户端防线（证书体系的散件版）。`MATCH` 放行、`NEW`（无记录）问询、`CHANGED`（有记录但密钥不同）**大声警告**——比布尔多出来的"变更"态就是安全的价值所在。

工具层的设计与第 79 章 X.509 视图同款：格式层无分配（参数保持原始编码——算法演进不动公共解析器）、验证层独立（密码实现可裁剪）、信任层是纯文件协议（不碰网络不碰连接）。

## 概念

### 通用格式：前缀解析与算法扩展

`xrtSshPublicKeyRead` 读算法名（string）+ 其后字段作为**原始 `Parameters` 视图**——公共前缀解析器不知道各算法的参数结构，新增 RSA/ECDSA/安全密钥算法时**不需要修改它**。`xrtSshSignatureRead` 严格读 `string algorithm || string signature`、拒绝尾随数据；`SignatureWrite` 直接写调用方缓冲。Ed25519 固定格式（`ssh_hostkey_ed25519`）：公钥=32 字节 key（string 包装）、签名=64 字节 r||s（string 包装）——读写+验证三件套。

### 指纹：人类比对的形态

主机密钥几十字节、不可读——指纹（`ssh_fingerprint`）把它压成 `SHA256:` 前缀十六进制形态的短串（第 74 章摘要的协议应用）。两个用途：TOFU 首次记录时的"人眼确认"（对照带外渠道——控制台输出/管理员核对）；日志与告警里的密钥标识（"密钥变更事件"记录的是指纹不是全文）。

### known_hosts：格式与三态

```diagram flow
- 文件行：[|@]marker [|]hostnames keytype base64-key [comment]
  （@标记 revoked；哈希主机名 |1|salt|hash 形态）
- 查询：Check(主机名列表, 端口, 密钥视图) → 信任态
  MATCH=密钥与某行一致 / NEW=主机无记录（TOFU 入口）/
  CHANGED=主机有记录但密钥不同（改密或 MITM——告警！）
- 记录：新主机的行写入；revoked 行优先于任何匹配
```

`ssh_known_host` 是单行/内存形态的解析与匹配；`ssh_known_host_db` 是文件级（加载/查询/追加）；`ssh_known_host_hash` 是哈希主机名（`|1|salt|hash`——防known_hosts 文件本身泄漏内网主机名；匹配时按 salt 重算）。**端口语义**：非标准端口的主机名形态 `[host]:port`——同一机器不同端口是不同身份。

### 信任决策的组装

把本章工具接到第 122 章事件上：`HostKeyAccept` 前查库——MATCH 放行；NEW 按策略（交互问询/带外核对指纹后记录/配置白名单）；CONFLICT 拒绝并告警。指纹在 NEW/CHANGED 两态都打出来——前者供核对、后者供报告。这个组装在第 127 章客户端运行时里有标准形态。

## 示例

### 第一个完整程序：Ed25519 公钥的写出与读回

下面的程序来自 `examples/hostkey`——格式层的往返：

```embed path="extlibs/xssh/examples/hostkey/main.c" title="extlibs/xssh/examples/hostkey/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/hostkey/main.c -lws2_32 -liphlpapi
（输出 ed25519 公钥尺寸与 blob 尺寸的自检结果）
```

**刚才发生了什么。** ① `xrtSshEd25519PublicKeyWrite` 把算法名（`ssh-ed25519`）+32 字节密钥编码成标准 blob——`Writer.Size` 即 blob 尺寸。② `xrtSshEd25519PublicKeyRead` 读回——`PublicKey.Size`（32）与 blob 尺寸的关系就是"算法名 string + key string"的包装开销。③ 往返一致=格式自洽——这个 blob 正是 ECDH_REPLY 里 `K_S` 的形态与 known_hosts 里 base64 的内容：**同一份编码贯穿握手、验证、存储三处**。配套 `hostkey_ed25519`（验证三件套）、`key_text`（文本形态转换）、`private_key`/`private_key_pem`（私钥装载——OpenSSH/PEM 格式）。

### 第二个完整程序：known_hosts 三态查询

第二个程序来自 `examples/known_host_db`——信任判断的核心：

```embed path="extlibs/xssh/examples/known_host_db/main.c" title="extlibs/xssh/examples/known_host_db/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/known_host_db/main.c -lws2_32 -liphlpapi
（输出 trust=MATCH 与记录行号的自检结果）
```

**刚才发生了什么。** ① 查询入参：主机名列表（一行可记多个主机）、端口、密钥视图——`Check.Trust` 输出三态、`Check.Entry.LineNumber` 指向命中行（告警/删除的定位信息）。② 本例命中 MATCH（trust=1）+ 行号——放行路径的形态；CONFLICT/UNKNOWN 在测试向量里覆盖（`known_host`/`known_host_hash` 两示例分别验单行匹配与哈希主机名匹配）。③ 多态的调用方分支就是第 122 章事件处理的实体：MATCH→Accept、NEW→策略、CHANGED→Fail+告警（附指纹与行号）。

### 与 TLS 证书体系的对照（设计观察）

第 80/81 章的 X.509 与本章的 known_hosts 解决同一个问题——"这个公钥属于谁"——但形态几乎相反。**证书**：层级信任（根→中间→叶）、第三方担保、验证是链构建（PathBuild）+策略（Verify 层分离）；**known_hosts**：扁平信任（每台主机一条记录）、自我担保（TOFU 或带外）、验证是单点比对（Check 三态）。代价与收益各有：证书体系部署重（CA 运营/吊销/更新）但扩展性好（一台根信百万站点）；known_hosts 零部署但每台客户端各自维护。工程选型不互斥——SSH 也有证书形态（`XSSH_KNOWN_HOST_TRUST_CERT_AUTHORITY` 信任态就是为证书权威记录预留的出口），TLS 内网也有 pinning（第 85 章）。理解两套形态的读者能按场景选对工具：开放互联网 HTTPS 用证书、内部自动化 SSH 用 known_hosts、高敏感两者叠加指纹钉扎。

## 契约

- **格式层无分配**：参数原始编码视图；公共前缀解析器不随算法演进修改；读写借用/直写。
- **签名严格**：`algorithm || signature` 双 string；尾随拒绝；Ed25519 固定 32/64 字节。
- **指纹**：SHA-256 形态；用于人核对与日志标识——不是验证的替代（比对仍按密钥）。
- **信任态语义**：MATCH/NEW/CHANGED/REVOKED/CERT；CHANGED 是安全事件（改密轮换或 MITM——默认拒绝+告警）。
- **revoked 优先**：@revoked 行命中即拒绝——即使另有匹配行。
- **哈希主机名**：`|1|salt|hash` 形态；匹配按 salt 重算；隐私场景用（文件泄漏不暴露主机名）。
- **端口身份**：`[host]:port` 形态——非标端口独立身份。
- **行定位**：查询结果带行号——告警/删除/审计的定位。
- **文件协议层**：known_hosts 层不碰网络/连接/密钥生成——纯文件与匹配。

## 避坑

### 坑 1：CONFLICT 当 UNKNOWN 处理

症状：密钥变更时客户端问"要记录新密钥吗？"——用户习惯性回车，中间人攻击完成接管。

原因：UNKNOWN（首次）与 CONFLICT（已记但不同）是**完全不同**的安全等级：前者是正常流程，后者默认必须拒绝——告警人工排查（服务器确实换密钥？带外确认后再清旧记录）。

```c bad
if ( Check.Trust != XSSH_KNOWN_HOST_TRUST_MATCH ) {
	prompt_and_save(Host, Key);   /* 冲突也当首记：接管漏洞 */
}
```

```c good
switch ( Check.Trust ) {
case XSSH_KNOWN_HOST_TRUST_MATCH:
	accept(); break;
case XSSH_KNOWN_HOST_TRUST_NEW:
	if ( confirm_tofu(fingerprint) ) { save_and_accept(); }
	break;
default:  /* CONFLICT */
	alert_key_changed(Host, fingerprint, Check.Entry.LineNumber);
	reject();   /* 默认拒绝——人工确认后才能清旧记录 */
}
```

### 坑 2：比对用指纹代替密钥

症状：按指纹前 8 字节匹配就放行——碰撞面被拉大，且指纹编码形态（base64/hex/带不带前缀）不一致时误判。

原因：指纹是**展示形态**；程序化比对按密钥字节（Check 的密钥视图参数）——精确匹配。指纹只进人机界面与日志。

```c bad
if ( strncmp(fp_text, LineFp, 8) == 0 ) { accept(); }  /* 前缀撞+编码差 */
```

```c good
/* 库内按密钥比对 */
xrtSshKnownHostDbCheck(Db, Hosts, Port, KeyView, &Check);
if ( Check.Trust == XSSH_KNOWN_HOST_TRUST_MATCH ) { accept(); }
```

### 坑 3：known_hosts 明文记遍内网主机名

症状：合规审计不过——笔记本失窃即泄漏全部内网拓扑。

原因：标准 known_hosts 明文主机名；哈希形态（`known_host_hash`）专为此设计——匹配端无感（按 salt 重算），文件端不可读。

```c bad
save_line("buildserver.internal.corp,10.1.2.3 ssh-ed25519 AAAA...");
```

```c good
/* 写入用哈希形态——文件泄漏只见盐与哈希 */
save_hashed_line(Host, Key);   /* |1|salt|hash ssh-ed25519 AAAA... */
```

## 练习

### 基础：三态矩阵

构造 known_hosts 三行（匹配/他钥/无记录的主机），对同一密钥+另一密钥查询——覆盖 MATCH/NEW/CHANGED 全组合。验收标准：六组查询结果与信任态语义一致；CHANGED 的行号指向既有记录。

### 进阶：TOFU 交互客户端片段

实现 `verify_host(主机, 密钥, 库)`：查库→分态（MATCH 静默放行/NEW 打指纹问询/CHANGED 告警拒绝）——接第 122 章事件。验收标准：三路径行为正确；指纹输出含算法前缀（SHA256: 形态）。

### 挑战：密钥轮换流程

模拟服务器换密钥：旧库 CHANGED → 告警 → 模拟带外确认 → 删旧行（按行号）→ 记新行 → 重连 MATCH。全程哈希主机名形态。验收标准：轮换后放行；未确认前始终拒绝；文件全程无明文主机名。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 格式层 | 算法名+原始参数视图；公共解析器不随算法演进；无分配 |
| Ed25519 | 公钥 32B/签名 64B（string 包装）；读写验证三件套 |
| 指纹 | SHA-256 展示形态；人核对与日志用——程序比对按密钥 |
| 信任态 | MATCH 放行 / NEW 问询（TOFU）/ CHANGED 告警拒绝 / REVOKED 即拒 |
| revoked | @行命中即拒——优先于匹配 |
| 哈希主机名 | \|1\|salt\|hash；匹配按盐重算；防文件泄漏拓扑 |
| 端口身份 | [host]:port——非标端口独立身份 |
| 行定位 | 结果带 LineNumber——告警/删除/审计定位 |
| 与 118 章接口 | VERIFY_HOST_KEY 事件 + 本章查库 = 完整信任判断 |
