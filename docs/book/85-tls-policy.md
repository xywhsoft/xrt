---
num: 85
slug: tls-policy
title: 验证策略：协议白名单与信任决策
volume: 卷八 安全
type: practice
lead: 策略对象定义"允许说什么话"，验证器定义"允许相信谁"——两份配置一处定义、处处引用的安全基线。
api: tls, tls_verify, x509
---

## 导读

TLS 的全部安全决策浓缩在两个配置对象里。**策略**（`xtlspolicy`）：允许哪些版本、套件、组、签名方案——协议白名单，第 84 章的协商以它为基线；默认值即现役安全基线（2 版本/9 套件/4 组/14 方案，无弱算法），收紧只改数组。**验证器**（`xtlsverifier`）：信任决策——默认路径走"信任库建链 + 用途检查 + 名称匹配"（第 80/81 章的原语组合），自定义回调把决策权完全交给应用（证书固定、内嵌私有 CA、审计日志），还可叠加策略回调组合 CRL/OCSP。两者都不可变、引用计数、可被多个会话并发共享——"一处定义、处处引用"。第 82 章的客户端配置在这里补完全貌。

## 引入

三个真实需求。其一：合规审计要求你的服务"只允许 TLS 1.3 与特定两三个套件"——你要一处改完全部连接生效，而不是在每个客户端/服务端配置里散落硬编码。其二：你的客户端只连自家后端，公共 CA 的信任面太大——想固定到具体某张证书（pinning），第 81 章的信任库做不到"单证书即锚+指纹比对+审计日志"。其三：实验环境想临时加一条"吊销检查"（第 81 章的 CRL 查询），又不想重写建链逻辑。

三个需求对应本章两层：策略对象管协议面（需求一），验证器的三层扩展点管信任面（需求二、三）。关键设计是**决策与执行分离**：验证器只管"信任决策"，协议层的签名验证、Finished 校验仍由 XRT 完成且**任何回调不能绕过**——你接管的是"信不信"，不是"协议对不对"。

## 概念

### 策略对象：协议偏好的单一事实源

`xtlspolicy` 是借用式配置：默认初始化指向进程期只读常量（零分配），收紧就换自有数组（生命周期覆盖上下文创建过程）：

```diagram flow
- 默认基线：xrtTlsPolicyInit → 只读常量（2 版本/9 套件/4 组/14 方案，全部现役）
- 收紧：替换 Versions/Ciphers/Groups/Signatures 数组指针与计数
- 自洽校验：xrtTlsPolicyValid —— 空集/版本套件失配/1.2 方案混入 1.3 即拒绝
- 引用：Context/客户端/服务端配置共享同一份策略快照
```

校验规则（启动期拦截配置错误）：版本与套件必须非空；列表指针/数量一致、元素唯一、属于内建能力；每个套件对应一个启用版本、每个版本至少一个套件；非空签名列表必须能用于至少一个启用版本——1.2 专用 PKCS#1 方案进不了纯 1.3 策略。两个边界：策略只描述**偏好**，不承诺当前裁剪构建含对应后端（会话创建时结合 `xrtTlsGroupAvailable` 等生成实际执行路径）；`KeySharePolicy` 可覆盖本地 key-share 选择模式（如 `XTLS_KEY_SHARE_PREFER_GROUP`）——ClientHello 带哪个组的公钥由它定。

### 验证器：信任决策的三层

`xrtTlsVerifierCreate(配置)` 深复制可选信任库——创建后来源 store 可释放或修改；没有自定义回调时**必须**提供至少一个信任锚，空 store 创建期失败（与第 82 章"不验证不是选项"同源）。三层决策结构：

- **默认路径**（`xrtTlsPeerVerify`）：显式时间下验证路径（第 81 章建链）+ 角色化用途检查（`digitalSignature` KeyUsage、`serverAuth`/`clientAuth` EKU）+ 服务端角色按 RFC 9525 匹配请求名称（第 80 章）。线路链按叶到根；锚来自 store、不要求出现在对端链中。
- **Verify 回调**（完全接管）：`XTLS_VERIFY_ACCEPT` 接管信任 / `REJECT` 拒绝 / `DEFAULT` 回退不可变 store / `ERROR` 报结构化错误。回调拿到的是**已通过密码学校验的对端结构**（签名、Finished 由协议层保证）——你只决定信不信。接受结果仍不能跳过 CertificateVerify 与 Finished——"接管信任"不等于"接管协议"。
- **Policy 回调**（叠加检查）：默认检查全部成功后触发，读取 `xtlsverifiedpeer`（`Path` 叶到根不含锚、全部视图仅回调期间借用）——组合 CRL（第 81 章）、OCSP、CT、证书固定、企业规则，**不必重复建链**。返回 false 可设错误，包装为 `XTLS_ERROR_VERIFY` 的 cause。

时间源可注入（`xtlsverifytimeproc`）：路径与附加策略共用同一确定时间——测试可复现、缓存可判定；省略用 `xrtNow()`。

### 边界：TLS 核心不偷偷联网

TLS 核心不会隐式加载系统根、下载 CRL、发起 OCSP 或提交 CT 查询——数据加载与联网属于独立组合层。基础客户端不会在验证期间突然阻塞在文件或系统 API 上（第 77 章"系统层失效"的预防设计）。要 CRL 就在 Policy 回调里自己调第 81 章的 API——软失败还是硬失败是你的策略决定，验证器保留明确结果供你决策。

### 装配形态：策略+验证器+会话

完整的客户端装配（第 82 章示例的分解视图）：

```diagram flow
- 策略：xtlspolicy 收紧到目标基线 → PolicyValid 校验 → 挂上共享上下文配置
- 信任：StoreSystem（或显式 Add）→ VerifierCreate 深复制 → Store 可释放
- 扩展：Verify/Policy/Time 回调按需注入（pinning、CRL、确定时间）
- 会话：ClientConfig{Verifier, ServerName, Protocols} → 会话持引用 → 自己的引用可 Release
```

## 示例

### 第一个完整程序：策略基线与收紧校验

下面的程序来自 `examples/tls/policy/main.c`，打印默认基线并覆盖 key-share 模式：

```embed path="examples/tls/policy/main.c" title="examples/tls/policy/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/policy/main.c -lws2_32 -liphlpapi
versions=2 ciphers=9 groups=4 signatures=14
```

**刚才发生了什么。** ① 默认基线**输出即证据**：2 个版本（1.2/1.3）、9 个套件、4 个命名组、14 个签名方案——无弱算法，这就是"不给配置也安全"的底线。② `KeySharePolicy = XTLS_KEY_SHARE_PREFER_GROUP` 覆盖本地 key-share 选择——ClientHello 优先带偏好组的公钥，减少 HRR 往返（第 84 章）。③ `PolicyValid` 是配置的守门人——把 Versions 换成 1.3-only 而 Ciphers 没动（含 1.2 专用套件）时它会拒绝；收紧示例（只要 1.3 + ChaCha20/AES-GCM 两套件）就是把数组与计数一起换、再过一次校验。策略快照被上下文引用后不再被修改——同一份配置服务不同硬件能力（后端编入在会话创建时决定）。

### 第二个完整程序：自定义验证器接管信任

第二个程序来自 `examples/tls/verify/main.c`，用回调把证书决策权交给应用：

```embed path="examples/tls/verify/main.c" title="examples/tls/verify/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/verify/main.c -lws2_32 -liphlpapi
TLS verifier is ready
```

**刚才发生了什么。** ① `VerifyConfig.Verify = exampleTlsVerify` 注入回调——此形态**不需要 store**（决策全在回调里）。② 回调拿到 `xtlspeer`（对端信息：证书链视图、协商参数），返回决策枚举——本例是最小策略"有证书即收"（仅演示接口，不是可抄的生产策略）。③ 验证器可被多个客户端共享——创建一次、注入每个 `ClientConfig.Verifier`、自己的引用随手 `Release`。生产形态举例：证书固定（比对叶证书 SHA-256 与预置指纹，不匹配 REJECT）、内嵌私有 CA（store 只装自家根 + DEFAULT 回退）、审计日志（Policy 回调记录每次验证的对端与路径再放行）——三层决策结构里各有归属。

## 契约

- **策略校验**：非空版本与套件；指针/数量一致、元素唯一、属内建能力；套件↔版本双向覆盖；签名列表可用于至少一个启用版本；校验不修改输入、零分配。
- **策略边界**：描述偏好不承诺后端编入；会话创建结合 `GroupAvailable`/AEAD/身份/验证后端生成实际路径；原始策略快照不变。
- **KeySharePolicy**：本地 key-share 选择模式（如 `XTLS_KEY_SHARE_PREFER_GROUP`）——影响 ClientHello 携带的组，减少 HRR。
- **验证器强制**：无 Verify 回调时必须至少一个信任锚；空 store 创建期失败；深复制后来源 store 可释放/修改。
- **三层决策**：默认路径（建链+用途+名称）/ Verify 回调（完全接管，四态决策）/ Policy 回调（默认全过后叠加，视图仅回调期间借用）。
- **协议不可绕过**：接受决策不能跳过 CertificateVerify 与 Finished——签名与完整性由协议层保证。
- **时间注入**：`xtlsverifytimeproc` 为路径与策略提供同一确定时间；省略 `xrtNow()`。
- **并发**：验证器不可变、引用计数；Verify/Policy/Time 回调可并发调用——不得依赖线程局部可变共享。
- **无隐式联网**：不加载系统根、不下载 CRL、不发 OCSP、不查 CT——组合层与策略回调的事。
- **错误包装**：回调 false 且设错误 → `XTLS_ERROR_VERIFY` + cause；未设错误 → 明确 `XERR_PERMISSION` 拒绝。

## 避坑

### 坑 1：把策略当能力声明（裁剪没编也"配上了"）

症状：策略配了 ChaCha20 套件，固件构建没编 ChaCha20 后端——运行时协商不到，连接失败或降级到别的套件，与配置预期不符。

原因：策略是偏好不是承诺——校验只查"属于内建会话能力"，不查当前构建的后端编入。实际执行路径在会话创建时结合 `GroupAvailable` 等生成。

```c bad
/* 假设配了就能用 */
Policy.Ciphers = ChaChaFirst;
Policy.CipherCount = 1;
/* 构建 --feature 无 CHACHA20：目标环境全部协商失败 */
```

```c good
/* 部署前核对能力：组用 GroupAvailable，套件按构建清单 */
if ( !xrtTlsGroupAvailable(XTLS_GROUP_X25519) ) {
	/* 启动期报告能力缺口，而不是线上连接失败 */
}
/* CI 里对目标构建跑协商冒烟测试，策略与能力一起验 */
```

### 坑 2：Verify 回调里重做密码学检查

症状：回调里自己验签名、比对 Finished——代码臃肿还可能验错；或反过来以为"接管验证"后协议校验被跳过，放松了警惕。

原因：没理解"决策与执行分离"：回调拿到的是**已通过密码学校验**的对端结构（签名、Finished 由协议层保证且不可绕过）；你的职责是"信不信"（信任链、固定、审计），不是"重验协议"。

```c bad
static xtlsverifydecision myVerify(const xtlspeer* pPeer, ptr pCtx) {
	/* 重做签名验证：重复、易错、且协议层已保证 */
	if ( !self_verify_signature(pPeer) ) { return XTLS_VERIFY_REJECT; }
	return XTLS_VERIFY_ACCEPT;
}
```

```c good
static xtlsverifydecision myVerify(const xtlspeer* pPeer, ptr pCtx) {
	/* 只做信任决策：例——证书固定 */
	if ( !pin_matches(pPeer /* 叶证书摘要比对 */ ) ) {
		return XTLS_VERIFY_REJECT;
	}
	return XTLS_VERIFY_ACCEPT;   /* 协议校验由 XRT 保证 */
}
```

### 坑 3：Policy 回调里持有借用视图不还

症状：回调返回后访问 `xtlsverifiedpeer.Path` 的视图——崩溃或读到垃圾。

原因：Policy 回调的全部视图（路径、锚、证书）**只在回调期间借用**——回调返回即失效。要长期保存必须深复制（DER 字节复制）。

```c bad
static bool myPolicy(const xtlsverifiedpeer* pPeer, ptr pCtx) {
	g_SavedCert = pPeer->Path[0];   /* 存下借用视图：返回即悬空 */
	return true;
}
```

```c good
static bool myPolicy(const xtlsverifiedpeer* pPeer, ptr pCtx) {
	audit_log(pPeer);   /* 回调内完成读取与记录 */
	/* 要长期保存：在此复制 DER 字节，由自己管理生命周期 */
	return true;
}
```

## 练习

### 基础：基线快照与收紧演练

跑通 policy 示例记录默认基线；然后收紧到"仅 TLS 1.3 + 两套件"（数组替换 + `PolicyValid`），打印新计数。验收标准：收紧后 Valid 通过；把一个 1.2 专用签名方案塞进 1.3-only 策略，Valid 明确拒绝——错误消息能指出问题列表。

### 进阶：证书固定验证器

实现 pinning 验证器：`Verify` 回调比对叶证书的 SHA-256 摘要与预置指纹数组（第 74 章流式摘要 + `ConstTimeEqual`），命中 ACCEPT、未命中 REJECT。用自签证书走第 82 章 dial 的本地变体验证。验收标准：正确证书通过、换一张证书立即拒绝；指纹比对用常量时间（解释为什么）。

### 挑战：带 CRL 的三层组合

组装"系统信任 + Policy 回调 CRL 检查"的验证器：默认路径建链（StoreSystem），Policy 回调里对叶证书跑第 81 章的 `CrlValidate + CrlCheck`，结论 REVOKED 拒绝、DONE 走软失败（记录日志放行）、GOOD 放行。用测试 CA 生成已吊销证书验证。验收标准：三种吊销结论各自走对路径；CRL 只在回调触发时才被读取（验证"无隐式联网"——不连任何主机也能完成 GOOD/REVOKED 判定）；时间源注入固定时间后结果可复现。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 策略对象 | 版本/套件/组/签名四列表 + KeySharePolicy；默认即安全基线（2/9/4/14） |
| 策略校验 | PolicyValid：非空、唯一、套件↔版本双向覆盖；1.2 方案进不了纯 1.3 |
| 策略边界 | 偏好≠能力；后端编入在会话创建时决定；快照不变可跨机共享 |
| 验证器创建 | 深复制 store（随后可释放）；无回调必须有锚；空 store 拒绝 |
| 三层决策 | 默认路径（链+用途+名称）/ Verify 回调（四态接管）/ Policy 回调（叠加 CRL/OCSP/固定） |
| 决策执行分离 | 回调只管信不信；CertificateVerify/Finished 由协议层保证不可绕过 |
| 四态决策 | ACCEPT 接管 / REJECT 拒绝 / DEFAULT 回退 store / ERROR 带结构化错误 |
| 时间注入 | timeproc 统一确定时间（测试/缓存可复现）；省略 xrtNow() |
| 借用边界 | Policy 回调视图仅回调期间有效；长期保存须深复制 |
| 无隐式联网 | 不自动装系统根/CRL/OCSP/CT——获取与软失败策略归你 |
