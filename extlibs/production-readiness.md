# 七个扩展库的生产就绪工作单

范围：`xacme`、`xmail`、`xpop3`、`xsmtp`、`ximap`、`xjwt`、`xoauth2`。
本文件记录可复现的验收条件和当前缺口；测试通过不等同于已完成生产验收。

## 验收条件

1. 所有库的模块化、单头、裁剪和包消费构建在 Linux 与 Windows CI 通过，启用 `-Wall -Wextra -Werror`。
2. 不可信协议输入有大小上限、类型校验和错误后不可复用的会话状态；认证凭据和私钥在故障路径中不泄露。
3. 关键故障路径有可注入的分配、断流、超时、TLS 拒绝和服务器拒绝测试；覆盖率按库测量并以有意义的分支作为补测依据。
4. 每个客户端库至少有一个真正执行核心操作的离线范例，以及清楚说明真实服务配置的范例。
5. 外部互操作与提供商测试有独立、显式启用的执行方式；没有凭据时不把离线通过冒充线上验证。
6. 发布前核对生成单头、公开声明、包消费和文档，并记录不兼容变更与剩余限制。

## 当前进度（2026-10-05）

共享 TCP 的发送/关闭引用释放竞争已修复，并完成 Windows/Linux 全量复验：44 条覆盖率命令、63 条交付命令及两平台 Compose 输出检查通过，14 对主报告/私有头报告绑定本轮源码和原始计数。独立服务客户端也已重建，Dovecot/Postfix 36 场景、Hydra 33 场景、Pebble 9 场景通过，包含 Linux sanitizer 检查。最新就绪结论及精确计数见文末“共享 TCP 发送与关闭的引用释放竞争”；其余章节保留分阶段细项和原始证据。

最近线上验收仍是修复前版本的阿里云 DNS 与 QQ 邮箱记录：Windows/Linux 完成 test.xxrpa.com 的 Let’s Encrypt staging DNS-01 签发、存储、缓存及独立证书/权限检查，测试 TXT 恢复原基线；QQ 的 SMTP 接受提交、POP3 取信及 IMAP 查询均已实测，自动投递到收件箱未确认。在线记录没有重标为本轮执行。生产整体验收仍未完成，当前主线托管 CI、历史 Huawei/POLICE 退出计数/分配原因、仓库基线文档门禁及最终部署/OAuth 范围继续保留。

| 库 | 已验证或已修复 | 待完成的验收工作 |
| --- | --- | --- |
| `xacme` | Windows 与 Ubuntu/WSL GCC 严格构建、裁剪、单头及静态包消费；离线 mock CA 签发、吊销、确定性断流重试；POSIX 私钥从创建起为 0600，域名与路径检查，通配符域名使用跨平台 `%2A` 目录且兼容 POSIX 旧布局；DNS provider 销毁时清除上下文；`SaveGrant` 版本目录加单点发布，授权及账户密钥的 POSIX 目录项同步和提交前后 `fsync` 故障回归；Linux 文件写入与私钥部分写入故障回归确认旧授权、旧账户密钥和临时文件状态；Windows `current` 发布避开 `ReplaceFile` 中间态并用文件锁协调写者，跨平台短暂读错误限次重试；Linux/Windows 双写者与读者并发、双进程压力本地验证，CI 脚本已接入；离线旧版回收工具默认预览、双平台合成测试验证保留当前版、保留指定旧版和拒绝异常目录；`LoadGrant` 固定同一版本，轮换和 OOM 回归确认旧授权不被失败写入污染；五家 DNS provider 拒绝超长凭据或端点；腾讯云 TC3 时间戳/密钥派生与 AWS SigV4 密钥派生按官方公式修正并用固定向量回归；异步 HTTP 仅接受成功 Future 终态，整次发送截止时间经双平台慢读 POST 验证，失败清理改为 Abort；修复单头私有函数重名；既有 Windows/Linux ASan 独立 HTTP/TLS 38 场景验证字段/trailer 的 100 项边界、4 MiB 正文边界、NUL 拒绝、EOF、chunk 扩展内存上限、部分上传与五处响应字段 OOM；请求预留自动字段，超限及管理字段覆盖在连接前拒绝；引擎清理等待异步退休，独立场景销毁后活动分配归零；URL authority、查询/片段、IPv6 与 SNI 修复，79 个非法 URL 均在拨号前拒绝；片段 ASCII 语法穷举与混合数值地址回归通过，URI/TLS 与容量向量在 Linux ASan 各重复五次；双平台完整套件、显式单头、裁剪与静态/共享消费者通过；Linux/Windows 模拟 CA 九项通过；本轮 HTTP 初始化顺序、清理后请求检查及拥有权传递修复，新增公共 ClientCleanup；双平台独立生命周期、24 种上层拥有权组合及构造 OOM 通过，Linux ASan 重复五次、三种拥有权丢弃变体检出；未交付对象的零分配待清理队列及并发重试通过；真实 UDP 查询显式关闭，七类查询及内部 DNS 拥有权回归通过；本轮正常 mock 全链路与断流重试两项双平台通过；本批次 Cloudflare/腾讯云/华为云五处请求体补 NUL，18 项尾部填充/长输入/真实分配失败回归、Linux ASan 五次重复及双平台五种变体检出；Ali 创建/删除响应校验；Route53 既有同值不认领/不误删、重复 Add 保留所有权、八槽容量及未知结果阻止重放修复；Ali 创建缺失/异常 RecordId、响应丢失及解析/错误包装 OOM 保留未知状态与原始内存诊断；未知条目占用容量且禁止同名同值 Add/Remove，实例内 Add/Remove 串行化；Ali/AWS 创建 5xx 按未知结果处理；571 项实际 TLS 请求、独立签名/多记录/断流/异常响应双平台通过；Ali 区域发现仅准确缺失码允许父区域回退，分页/字段/记录校验、四项完整起点缓存替换、更具体区域与 219 字节属主通过；发现解析 26 个 OOM 位置保留首因且资源归零，六种区域发现隔离变体双平台检出，真实 flow 的 Add 回调及实际创建请求均核对只调用一次；Linux ASan/UBSan 571 项通过，既有创建八种变体与本批区域发现六种变体分别检出；HTTP/DNS 未知与内存错误的首次/后续 Add 六种流程组合通过；Ali 属主按 ASCII 大小写规范化、TXT 字节区分大小写，重复 Add 和删除按保存 ID 核对完整记录；记录消失与已提交删除的应答丢失安全对账，禁用及外部修改回归通过；记录解析 19/7 个 OOM 位置、八种所有权隔离变体双平台检出；Ali 跨响应重复 ID 按未知结果保留、清理按请求槽核对；两个只读 RPC 断流限次重签名恢复及签名前 HTTP 标记复位；AWS 大小写等价属主/字节区分 TXT、借用/未知/满载和同值并发回归；九种 ID/只读/身份变体双平台检出；AWS 从完整属主发现区域、新记录不复用父区域缓存、重复 Add/Remove 固定原区域 ID、无区域结果覆盖旧错误；14 项区域回归和七种变体双平台检出；AWS 写入错误信封完整校验、明确拒绝的冲突/限流有界退避、畸形错误阻止 flow 重放，31 项错误/重试场景及八种变体双平台通过；Cloudflare/腾讯云/华为云创建结果未知保留八槽与大小写保护、互斥串行及 ACK 身份/重复 ID 校验，TC3 动作名转小写修复，81 项创建场景和八种变体双平台通过；acme-index-chain-freeze-20261002 快照完整模块、九项 mock 和所有独立探针的 16 个自有 C 文件合并覆盖率为 Windows 行 86.96%/分支结果 71.03%、Linux 行 86.77%/分支结果 71.01%，防倒退门槛 70%/55%；单头、裁剪和静态/共享消费者双平台重验通过；Cloudflare 删除前身份读取、严格 ACK/81044 缺失验证、未知结果只读对账，62 项故障回归及八种变体双平台通过；华为云删除前完整身份/状态读取、202 异步受理有界轮询和未知结果对账，100 项删除回归及九种变体双平台通过；腾讯云冻结快照验证 DescribeDomain 固定 DomainId、重新发现更具体域名、删除前身份读取与严格 ACK/库存诊断，117 项新增 TLS 回归及 11 种变体双平台通过; 公开 Obtain/手动 DNS 范例已登记并接入常规/夜间 CI，双平台模块化及单头实际签发、账户/证书私钥持久化、缓存复用、TXT 清理和未确认 Add 不重放通过；Linux 单头 GCC/Clang ASan/UBSan 通过；腾讯云 OperationDenied 完整码及子类与相似前缀五项回归双平台通过，14 种删除/索引不确定性/错误分类变体双平台检出；腾讯云一天以上及连续缺失库存不释放记录、不允许重复 Add，四项新 TLS 回归双平台及 Linux ASan/UBSan 通过；独立证书链验收改为签名、显式信任锚、有效期、CA/用途/路径长度检查，18 项对抗测试及所有九项 mock 通过；Cloudflare/华为云移除父区域缓存捷径，从完整属主重新逐级发现区域，无区域返回稳定错误，20 项新增回归、五家 591 项 TLS 场景在 Windows/Linux GCC 与 Linux Clang ASan/UBSan 通过；三家重复已确认 Add 按原 ID/区域核验后复用，不新增记录或占槽，读取失败保留拥有权；71 项原实现红灯回归及五家 662 项 TLS 场景三平台通过，双平台模块、公开单头及实际 Obtain 范例通过 | URL 其余输入及 flow/provider 故障分支补测；内部头 static 实现的统一统计；部署文件系统上的断电恢复与进程异常终止、Windows 写入失败故障注入；离线回收在实际维护窗口的演练及在线读者协调；DNS 请求内固定缓冲区、删除响应与多记录清理的系统性审计；Ali 其余 OOM；AWS 只读响应分类与状态码退避；其余 provider 的身份与缓存审计；腾讯云实云缺失语义及其余响应分类；CF/华为云区域查询 OOM 首因、腾讯云完整属主区域发现及其余只读响应分类；provider 清理后调用与多记录故障路径；腾讯云已提交但应答丢失后的宿主核查/恢复与实云缺失语义；Core 持续变动后的当前工作区覆盖率及完整运行验收刷新；真实 CA/DNS 提供商受控互操作；托管 CI 实际结果 |
| `xmail` | Windows/Linux 严格构建、单头、范例测试与静态包消费；新增 SASL PLAIN/XOAUTH2/OAUTHBEARER 字节向量、TLS 验证拒绝和回复中途断流测试；MIME 对抗样本覆盖伪装边界、重复字段、非法 Base64、未闭合 multipart、NUL、逐字节截断及控制/高位字节变异，双平台完整套件通过，Linux ASan/UBSan 通过且已接入 CI；共享 TCP 写队列背压的超时、取消与恢复回环通过；TLS 读等待超时/取消、未开始发送的取消及握手中取消/超时/断流回环通过；STARTTLS 排队接管的超时/取消与原 TCP 恢复通过；关闭等待 Future 分配失败现立即中止并保留原始错误，双平台定向回归和 Linux ASan/UBSan 通过；已验输入的精确模块套件对象口径：Windows 78.18% / 64.70%、Linux 78.20% / 64.70%；十三项溯源回归及原计数复核双平台通过；Core 后续变化，报告按原输入归档 | 扩充真实恶意邮件与独立互操作样本；其余网络/TLS 超时、取消及中途写失败分支仍需补测；DEFLATE 需参考 IMAP 集成覆盖 |
| `xpop3` | Windows 与 Linux 严格构建、依赖裁剪、静态及共享包消费和 STLS 等运行时测试；认证回复保留内存错误；畸形 `STAT`、单项 `LIST`/`UIDL` 成功响应使会话失败，正常 `-ERR` 后仍可继续；本地故障服务器覆盖状态行中途断流、命令回复超时、发命令后取消及 USER 回复超时，核对原始错误、最后完整响应与失败终态；SASL challenge 响应后服务端断流也验证失败终态、上一条完整回复保留及禁止复用，Windows/Linux 定向回归通过；离线回环范例在双平台执行 `Open/Login/RETR/QUIT` 并核对邮件内容，可配置的真实 RETR/TLS 范例通过编译和无参数入口检查；STAT、逐行 RETR、流式 RETR 与字节收集在 TLS 收到不完整线路后的取消、超时、断流十二种组合通过，核对原始错误、输出不交付、最后完整回复和销毁前中止；已验输入的精确模块套件对象口径：Windows 76.52% / 60.00%、Linux 76.52% / 60.00%；十三项溯源回归及原计数复核双平台通过；Core 后续变化，报告按原输入归档 | 外部服务器互操作；连接、其余认证阶段和写失败分支继续补测，超时/取消还需覆盖更多阶段；真实服务范例需要用户自行提供测试账号和 CA |
| `xsmtp` | Windows 与 Linux 严格构建、单头和依赖裁剪测试、静态及共享包消费与 STARTTLS 等运行时测试；回复保存失败置会话失败；DATA 与 BDAT 同次 TLS 写入部分推进后的取消、超时、断流六种组合通过，确认原始错误、最近完整回复、拒绝复用与销毁前中止，Linux ASan/UBSan 通过且已接入 CI；离线回环范例实际执行 `xrtSmtpSubmit` 并核对 envelope、主题、正文和 QUIT，两平台各连续运行五次；可配置的真实 SMTP AUTH/提交范例通过编译和无参数入口检查；已验输入的精确模块套件对象口径：Windows 81.49% / 63.44%、Linux 81.49% / 63.44%；十三项溯源回归及原计数复核双平台通过；Core 后续变化，报告按原输入归档 | 旧 `examples/client` 范例仍偏展示用途；外部服务器互操作；其余提交与连接故障分支补测；真实服务范例需要用户提供测试账号和 CA；托管 CI 实际结果 |
| `ximap` | Windows 与 Linux 严格构建、单头和依赖裁剪测试、静态及共享包消费与 STARTTLS/COMPRESS 等运行时测试；greeting 保存失败保留原始错误；主动 `BYE` 使会话终止并保留诊断，正常 LOGOUT 仍读取最终 tagged 回复；APPEND 上传前后 `NO`/`BAD`、非法提前 `OK`、预取消与服务器断流均有回环回归；同一个 TLS APPEND 写入 Future 部分推进后的取消、超时及断流回归，在销毁客户端前确认连接中止，双平台重复运行与 Linux ASan/UBSan 通过且已接入 CI；离线回环范例执行 `Open/LOGIN/EXAMINE/LOGOUT` 并核对只读邮箱摘要，两平台各连续运行五次；可配置的真实登录/EXAMINE 范例通过编译和无参数入口检查；已验输入的精确模块套件对象口径：Windows 79.82% / 61.49%、Linux 79.82% / 61.44%；十三项溯源回归及原计数复核双平台通过；Core 后续变化，报告按原输入归档 | 外部服务器互操作；客户端连接及其余命令失败分支补测；真实服务范例需要用户提供测试账号和 CA；托管 CI 实际结果 |
| `xjwt` | ES256 改为 JWS 定宽签名并拒绝 DER；claims 注入分配失败不签发；签发改为顶层 COW 副本，成功、失败和重复签发均不修改调用方 claims，并以 `const` 参数明示契约；Base64url 解码拒绝非零末组补齐位，编码长度溢出提前拒绝；README 示例的重复释放与错误算法混用已修正；RFC 5958 `OneAsymmetricKey` version 1 接受匹配私钥的 RSA/EC 外层公钥并严格校验版本、标签、未用位与属性顺序；HS/RS/ES 签发逐分配点 OOM 回归验证失败关闭、输入不变与恢复后可验签；JWKS 解析逐分配点 OOM 使整组失败并保留内存错误；HS/RS/ES 三类验签入口、RSA/EC 公钥缓存及解码逐分配点 OOM 失败关闭，缓存和解码核对资源释放与空输出；空 kid 与缺失 kid 分别选钥，含 NUL 的算法、kid、JWK 类型和曲线不按前缀识别；Windows/Linux 334 项测试、Linux ASan/UBSan 与独立 OpenSSL 解析/签验通过，OpenSSL 回归已接入 CI；冻结快照 Linux 自有源码行覆盖率 93.30%、分支结果 74.07%（`xjwt_sig.c` 为 66.27%） | Core 持续变动后的当前工作区覆盖率刷新；PKCS#8 属性语义、加密私钥与其他密钥封装边界继续审计；其他独立互操作、输入尺寸边界及签名模块未触达分支补测；持续审查 API 源码兼容性与实际调用方 |
| `xoauth2` | 响应类型与 OOM 失败关闭、授权 URL 编码、Microsoft tenant 限制、微信专有流程、独立裁剪构建及离线范例；URL authority、IPv6、SNI 与单调截止时间修复经既有双平台回归；失败中止连接、清空输出；接收支持各 100 个响应字段和 trailer，回收已消费前缀，拒绝原始 NUL，并区分正常 EOF；自建引擎清理限时等待异步关闭，失败保留拥有权和诊断并允许重试，借用引擎保持运行；清理后的句柄拒绝请求，初始化先完成配置再启动引擎；Windows/Linux ASan 独立 HTTP/TLS 与生命周期共 51 场景通过，85 个非法 URL 在拨号前拒绝，混合大小写 scheme 与片段语法回归通过，含构造 OOM Windows 127/Linux 139 位置扫描，销毁后活动分配归零；关键故障与生命周期各重复五次，六种未交付拥有权/诊断/准入变体被检出；失败堆构造零分配入列，公共 Pending 接口并发重试且保留在途计数与首因；双平台 CI 已接入；Windows 主测试 264 项、最小构建与离线范例通过；冻结快照三探针统一覆盖率为 Windows 行 92.41%/分支结果 77.39%、Linux 行 92.25%/分支结果 77.20%，默认门禁 91%/75%；六个令牌字符串字段拒绝解码 NUL，48 项新增断言通过；当前 Linux 主测试 257 项及 ASan/UBSan 通过 | Core 持续变动后的当前工作区覆盖率刷新；认证请求构造、微信异常响应及其余 HTTP/TLS 输入/分配故障补测；令牌字段其余语法及敏感缓冲清理审计；提供商实时验证；真实宿主退出/卸载演练；托管 CI 实际结果 |

POP3 收到完整但畸形的状态行时先解析、后保存，保留 `LastReply` 中上一条有效回复并使会话进入失败终态；本地故障服务器已分别覆盖事务命令和 PASS 认证阶段。Windows 与 Linux 的 `xpop3_tests` 严格构建、运行及单头测试通过，Linux 定向认证用例的 ASan/UBSan 也通过；托管 CI 仍需实际验证。
共享 `mail_net` 发送层过去在 TCP 队列立即接受数据时可能忽略调用前已取消或到期的请求；现在每个明文/TLS 发送片段提交前均检查截止时间和取消。POP3 回环故障服务器验证预取消、过期截止时间时没有命令字节到达，Linux 定向 ASan/UBSan 通过；`xmail_tests`、`xpop3_tests`、`xsmtp_tests`、`ximap_tests` 在 Windows/Linux 严格构建、运行和单头测试均通过。四库的静态包消费在 Windows/Linux 通过，xmail 的 Windows 共享包消费通过。连接在发起 TCP 或隐式 TLS 拨号前也会拒绝预取消与过期请求；回环测试核对了错误码、空传输状态和解析器未被调用。TCP 写队列重试前再次检查截止时间与取消，防止唤醒后继续提交过期数据。新增回环背压用例把客户端 Worker 停顿并占满 4 字节写预算，强制下一次发送进入 `AGAIN`；等待写就绪时的超时与跨线程取消保留正确错误且不额外发送，解除背压后精确续写。Windows/Linux 各连续通过 20 次，Linux ASan/UBSan 连续通过 5 次。写就绪唤醒恰逢取消的竞争、连接过程中的取消/超时、TLS 中途取消/超时及写失败仍需补测。
新增 TLS 故障测试前重跑的 xmail 完整 Linux gcov 基线为库自身 4413/5681 行（77.68%）、3087/4804 分支结果（64.26%）。其中 `mail_net.c` 为 169/302 行（56.0%）、96/212 分支结果（45.3%），`mail_net_tls.c` 为 75/165 行（45.5%）、19/86 分支结果（22.1%）；逐文件数据提示 TLS 中途故障和连接失败路径的缺口，不能只依据整体覆盖率判断传输层充分性。
真实 TLS 回环新增在已建立连接上的读超时、跨线程读取消和未开始的发送取消：每次读失败后，下一条独立回复仍被完整读取；客户端 Worker 停顿期间取消发送，服务端没有收到被取消的分片，随后正常发送成功。Windows/Linux 定向测试各连续通过 10 次，Linux ASan/UBSan 连续通过 5 次。已经开始并部分提交的 TLS 发送在超时/取消时如何失败、已建立 TLS 连接的服务端异常关闭以及协议客户端对这些失败的终态处理仍须单独验证。
新增测试后的完整 Linux gcov 套件通过；xmail 自身行覆盖为 4423/5681（77.86%）、分支结果为 3095/4804（64.43%）。`mail_net_tls.c` 提升到 83/165 行（50.3%）、24/86 分支结果（27.9%）；`mail_net.c` 为 171/302 行（56.6%）、99/212 分支结果（46.7%）。TLS 剩余分支仍较多，上述测试只证明明确列出的中途故障路径。
另以只接受 TCP 且不回应的本地服务端验证隐式 TLS 握手阶段：测试确认服务端收到 ClientHello 后，再分别请求取消、等待截止时间和主动断开连接。三条路径均及时失败，保留结构化错误且不向调用方留下活动 TCP/TLS Stream；Windows/Linux 各连续通过 3 次，Linux ASan/UBSan 连续通过 3 次。新测试已由 `xmail_tests` 汇总套件收集，完整 Linux gcov 套件通过；本次新增场景复用已触达的邮件层分支，整体覆盖率仍为 77.86% 行、64.43% 分支结果。外部 TLS 服务器及协议客户端的端到端互操作仍未验证。
共享 STARTTLS 接管曾在向 TCP 所属 Worker 投递任务后无界等待，Worker 队列停顿时会忽略调用方的截止时间和取消。接管请求现在使用原子门控管理排队/已开始/已撤销状态：排队期间超时或取消可及时返回，Worker 恢复后跳过 TLS 接管且不读取已失效的借用配置；若接管已开始则等待交接完成并清理已接管引用。新增本地 TCP 回环测试停顿 Worker，分别验证超时、跨线程取消、恢复后队列任务排空及原连接继续明文收发；Windows 严格构建与连续 10 次运行、Linux 严格构建与 ASan/UBSan 连续 10 次运行通过。POP3 STLS、SMTP STARTTLS、IMAP STARTTLS 正常升级的 Windows/Linux 定向回归全部通过。Windows 完整 xmail 套件（含单头）与 Linux 完整 gcov 套件均通过；Linux xmail 自有源码覆盖率为 4461/5710 行（78.13%）、3117/4822 分支结果（64.64%），其中 `mail_net_tls.c` 为 120/194 行（61.9%）、44/104 分支结果（42.3%）。接管已开始时与取消恰好并发的窗口仍缺可控故障注入。
本轮 `xmail_tests` 在 Windows/Linux 的完整套件（含单头）通过；POP3 认证、SMTP 客户端、IMAP 客户端的真实连接回环测试及对应单头客户端测试在两平台通过。四库的生成单头只读一致性检查通过；这轮下游运行的是定向回归，并未重新执行三套完整协议测试或线上互操作。
共享明文与隐式 TLS 拨号新增受控 DNS 解析器停顿窗口：解析已开始后取消和截止时间到期都及时返回，失败传输不持有活动 Stream；明文 Listener 在解析器恢复后没有收到迟到 TCP 连接，TLS Listener 没有收到迟到的完整会话。测试检查的是 TLS 会话而非底层 TCP SYN。Windows/Linux 定向测试每项各连续运行 10 次；改用有界 `xrtNetEngineTryDestroy` 等待取消后的异步拨号对象退休，Windows TLS 定向再连续运行 30 次、Linux 20 次，Linux ASan/UBSan 连续 5 次。Windows 完整 `xmail_tests` 与 Linux 完整 gcov 套件通过。当前 xmail 自有源码行覆盖为 4460/5721（77.96%）、分支结果为 3116/4826（64.57%）；`mail_net.c` 为 172/313 行（55.0%）、101/216 分支结果（46.8%），`mail_net_tls.c` 为 119/194 行（61.3%）、43/104 分支结果（41.3%）。已开始且部分提交的 TLS 发送、连接中取消恰逢 DNS 完成，以及外部服务器互操作仍待验证。
SMTP 回环补测了完整 DATA 被最终 `554` 拒绝后保留回复与 READY 状态，以及 DATA 命令被 `554` 拒绝后发 RSET 且保留原始协议错误。按 [RFC 5321 第 3.8 节](https://www.rfc-editor.org/rfc/rfc5321#section-3.8)，`421` 表示服务关闭传输通道；客户端现在在公共回复入口保留 LastReply、返回 `XERR_CLOSED` 并进入失败终态，多行 EHLO `421` 也不会被解析为能力。MAIL 与 EHLO 两阶段的回归在 Windows/Linux 定向测试及 Linux ASan/UBSan 下通过；双平台完整 `xsmtp_tests`、单头、Windows 静态/共享及 Linux 静态包消费通过。SMTP 自有源码覆盖率更新为行 80.86%、分支结果 62.95%；连接超时、写队列背压、TLS 中途故障和真实服务器互操作仍未验收。
IMAP 按 [RFC 9051 第 7.1.5 节](https://www.rfc-editor.org/rfc/rfc9051#section-7.1.5) 区分服务器主动 `BYE` 与正常 LOGOUT：主动 `BYE` 仍交付底层事件、保存最近响应，但状态立即失败，顺序命令返回 `XERR_CLOSED`；LOGOUT 发出后禁止新命令，同时允许继续读取 `BYE` 和最终 tagged completion。三种本地服务器场景在 Windows/Linux 完整 `ximap_tests` 和 Linux 定向 ASan/UBSan 下通过，Windows 静态/共享及 Linux 静态包消费通过。
APPEND 按 [RFC 9051 第 7.6 节](https://www.rfc-editor.org/rfc/rfc9051#section-7.6) 的同步 literal continuation 规则处理提前 completion：`NO`/`BAD` 保留会话与最近回复并分别返回权限/协议错误；正文未上传就收到 tagged `OK` 使会话失败。完整上传后的 `NO`/`BAD` 也保持可恢复。预取消写入不得有 literal 字节到达服务器；服务器在 continuation 后异常断开时上传不能成功，会话进入失败终态。故障回归在双平台完整套件及 Linux 定向 ASan/UBSan 下通过，断流场景还在 Windows 连跑十次及 Linux sanitizer 连跑五次；覆盖率更新为行 79.77%、分支结果 61.32%。连接超时、TLS 中途故障、写队列背压及真实服务器互操作仍未验收。
邮件三协议客户端在不可恢复的传输或解析失败后，现在立即请求异常中止底层连接，阻止已经返回失败的 TLS 写入 Future 继续发送后续字节；共享清理函数保留原始错误。IMAP 主动 `BYE` 仍向低层调用方交付事件与最近响应，但同步中止连接；正常 LOGOUT 不受影响。POP3 取消已发命令的故障服务器在调用方显式 `Abort` 前确认对端关闭，IMAP 预取消 APPEND 确认没有 literal 到达。Windows/Linux 的 `xmail_tests`、`xpop3_tests`、`xsmtp_tests`、`ximap_tests` 完整严格构建与测试（含单头）通过，POP3 和 IMAP 的新增故障路径在 Linux ASan/UBSan 下通过，四库生成单头只读检查通过。包门禁还暴露 `src/network/file.c` 缺少 Worker 内部端口声明头，已补齐；Windows/Linux 的全仓静态和共享包消费者均通过，Windows 的 POP3、SMTP、IMAP 独立静态包消费者也通过。POP3 的 TLS 协议故障阶段仍需补测，SMTP 部分写入验收见下；独立服务器互操作证据见各协议记录。

IMAP 新增实际 TLS 回环的 APPEND 部分发送回归：第一轮 4 MiB literal 写入结束后，
在所属 Worker 上记录已发送与两级待发密文的合计；第二轮 4 MiB 的同一个 Future
必须使该合计增加，且异步操作仍占用完整预算、尚未退休，随后才触发取消、超时
或对端断流。服务器恢复接收后须在客户端 Destroy 之前看到异常关闭，不能接收
完整 literal 或后续命令；调用方保留原始错误、最近完整响应与未提交的逻辑剩余量，
失败会话拒绝继续写入。用临时对象移除失败终态的 Abort，Windows/Linux 均确认
回归失败，证明测试可检出缺失中止；未修改源文件进行该敏感性检查。最终夹具在
Windows 连跑五次、Linux ASan/UBSan 连跑五次通过，已加入完整套件与 Linux CI
sanitizer 步骤。私有客户端结构移入内部头，清单补齐所属模块，保持公开 API 和
布局不变。Windows/Linux 完整模块化、单头及裁剪通过，静态包消费者重新验证通过；
最新 IMAP 自有源码覆盖率
为行 2199/2755（79.82%）、分支结果 1447/2355（61.44%）。这只验收 IMAP 的
共享发送路径，不替代 POP3/SMTP 各自协议阶段的故障测试。

SMTP 随后复用 `tests/fixtures/mail_tls_partial.h` 的 Worker 快照与小缓冲区设置，
分别覆盖 DATA 编码输出和 BDAT 原始块输出在同一次 TLS Future 部分推进时的
取消、超时与对端断流。DATA 使用长度小于 1000 字节的 CRLF 行，BDAT 声明精确
总长度；服务器核对已收到的字节内容，并在客户端销毁前观察异常关闭，不能收全
正文或后续命令。失败后保留最近的 354/250 完整回复，DATA writer 终止、BDAT
逻辑剩余量不提交，后续写入返回状态错误。私有结构移入内部头，公开 API 和布局
不变。Windows/Linux 的完整模块化、单头和裁剪通过，Linux ASan/UBSan 的六种
组合在 Windows 与 Linux 各连续运行五次通过；临时对象移除失败终态 Abort 后，
两平台均分别从 DATA 和 BDAT 入口确认回归失败，且原始源文件未改动。两平台
静态包消费者重新验证通过。最新 SMTP
自有源码覆盖率为行 801/983（81.49%）、分支结果 524/826（63.44%）。新增场景
已接入完整套件与 Linux sanitizer CI，POP3 的 TLS 故障阶段及其他提交故障仍待补测。

POP3 新增真实 TLS 回环的 STAT、逐行 RETR、流式 RETR 与字节收集四个入口，
各覆盖取消、超时和对端断流。测试在 Worker 上核对密文已收到、明文已取走且
接收 Future 仍待定，失败返回后另核对协议缓冲保存了缺少 CRLF 的完整前缀。
调用方须保留原始错误和最近完整回复，不交付不完整状态或拥有型结果；流式
回调只收到完整、去除 dot transparency 的线路。取消和超时须使对端在客户端
销毁前观察异常关闭，失败会话不能再发命令。Windows/Linux 的十二种组合、
各五次重复和 Linux ASan/UBSan 通过；临时对象分别移除 STAT、逐行 RETR 入口
共用的立即中止后，两平台均确认测试失败，原始源码未修改。测试已纳入完整
套件和 Linux sanitizer CI。最新 POP3 自有源码为行 665/869（76.52%）、
分支结果 408/680（60.00%）；连接和其余认证、写失败阶段仍待补测。

共享关闭路径新增确定性 OOM 回归：先停顿所属 Worker，再让 TLS 关闭等待
Future 的首次分配失败。修复前没有请求 Abort，定向回归失败；修复后所有
TCP/TLS 关闭失败均在共享入口异常中止并保留首个错误。测试在恢复 Worker 前
检查 Abort 请求，并在销毁传输前确认对端失败，双平台各连续五次及 Linux
ASan/UBSan 通过，已接入完整套件和 Linux sanitizer CI。裁剪检查另发现
`mail_net` 的 Base64 依赖缺少公共头拒绝检查，现与清单对齐。最新 xmail
自有源码为行 4472/5719（78.20%）、分支结果 3121/4824（64.70%）；
`mail_net.c` 为 183/313 行（58.5%）、104/216 分支结果（48.1%），
`mail_net_tls.c` 为 120/192 行（62.5%）、45/102 分支结果（44.1%）。
这两处剩余分支和真实邮件服务互操作仍需独立验收。
关闭修复后，Windows/Linux 的四库完整模块化与单头回归均通过；最后补齐
Base64 依赖检查后，四库裁剪、静态和共享包消费者在双平台通过，公开 API
文档与发布成熟度检查通过，四份最终生成单头只读检查一致。共享夹具新增
接收观察字段后，SMTP/IMAP 的部分发送场景也重新通过 Linux ASan/UBSan。
本地双平台检查共用工作目录时遇到单头生成文件占用，最终单头复核按生成
目标串行执行；这项本地证据不代替托管 CI 的实际运行结果。

新增独立 Python TLS 服务端驱动三个真实邮件客户端范例：POP3 在隐式 TLS/STLS 下完成 USER/PASS、RETR 与 QUIT；SMTP 在隐式 TLS/STARTTLS 下完成 AUTH PLAIN、envelope、DATA 与 QUIT；IMAP 在隐式 TLS/STARTTLS 下完成 AUTH PLAIN、EXAMINE 与 LOGOUT。每种协议和 TLS 模式均以 DNS、IPv4、IPv6 端点成功交换：DNS 发送 SNI，IP 字面量不发送 SNI 且按 IP SAN 校验证书。另以只含 DNS 身份的受信证书连接 IPv4，六条路径均在 TLS 握手阶段拒绝，未进入认证或提交处理。认证后截断响应的六条路径也已验证：POP3 在 RETR 正文中途断开，SMTP 在收到完整 DATA 后不返回最终提交结果，IMAP 在 EXAMINE 的 tagged completion 前断开；客户端均返回失败，服务端确认已到达目标阶段。OpenSSL 临时 CA 与独立 Python `ssl` 服务端仅在测试期间存在；Windows/Linux 完整脚本 `python tools/test_mail_tls_interop.py` 本地各 30 项通过并接入 CI，IPv6 回环不可用时会明确跳过对应六项。这证明本地跨实现 TLS/协议互操作；真实邮件服务商及其扩展行为仍待验收。
JWT 的 RSA PEM 解析现在要求完整规范 DER：SPKI 和 PKCS#8 检查 `rsaEncryption` OID 与 NULL 参数，SPKI BIT STRING 未使用位必须为零，PKCS#1 公私钥检查版本、INTEGER 与完整消费。RS256/384/512 的 PEM 公私钥及 JWKS 公钥统一执行 2048 位最低模数要求，弱 JWKS 条目被跳过。私钥组件直接借用单份解码 DER，签名后和解析失败时清零，不再保留八份额外组件分配。Windows/Linux JWT 164 项回归、Linux ASan/UBSan、OIDC 组合及 JWT 范例通过；本次 JWT 自有源码行覆盖 89.18%、分支结果 69.97%。这不替代独立互操作、故障分配注入和其他密钥格式审计。RSA 最低长度依据 [RFC 7518 第 3.3 节](https://www.rfc-editor.org/rfc/rfc7518#section-3.3)。
ES256 的 PEM 解析现在要求完整规范 DER：SPKI 和 PKCS#8 的算法标识必须为 `id-ecPublicKey` 与 P-256 namedCurve，SEC1 版本必须为 1，可选曲线必须为 P-256，可选公钥必须是有效曲线点且与私钥标量一致。公钥 SPKI 仅接受有效的 65 字节未压缩 P-256 点，私钥标量在解析时检查，解码后的私钥 DER 与签名栈缓冲在退出时清零；EC PEM 正文限 4096 字节。JWKS 的 EC 坐标在解析时验证曲线点，无效条目被跳过且重用槽位前清除残留 `kid`。依据 [RFC 5480](https://www.rfc-editor.org/rfc/rfc5480#section-2.1.1) 与 [RFC 5915](https://www.rfc-editor.org/rfc/rfc5915#section-3)。Windows/Linux JWT 189 项回归、Linux ASan/UBSan、双平台最小模块构建、OIDC 组合与 JWT 范例通过；最新自有源码行覆盖 89.13%、分支结果 69.34%。PKCS#8 属性兼容性见下；其他密钥封装格式、分配故障注入和独立服务互操作仍未验收。
RSA/EC 的 PKCS#8 `PrivateKeyInfo` 按 [RFC 5208](https://www.rfc-editor.org/rfc/rfc5208#section-5) 接受可选 `[0] IMPLICIT Attributes`，校验 Attribute OID、非空值集合、DER 排序与唯一字段位置，并忽略属性语义。带属性的两种密钥完成库内签名及公钥验签；独立生成的属性 PEM 经 OpenSSL 解析、签名和验签均为 `Verified OK`。错误上下文标签、缺失 OID、空值集合和乱序属性的回归被拒绝。新增 [RFC 5958 第 2 节](https://www.rfc-editor.org/rfc/rfc5958#section-2) `OneAsymmetricKey` version 1：仅在存在 `[1] IMPLICIT` 外层公钥时接受，RSA 比较模数与指数，EC 比较从私钥导出的有效 P-256 点；version 0 带外层公钥、version 1 无公钥、错误标签、非零 BIT STRING 未用位和公私钥不匹配均被拒绝。Windows/Linux JWT 240 项、Linux ASan/UBSan 与独立 OpenSSL RSA/EC 解析及签验通过，interop 脚本已接入 Linux CI。新增 HS/RS/ES 签发逐分配点 OOM 回归，核对失败时无令牌、输入 claims 不变及首次无故障签发可验签。重测 JWT 自有源码行覆盖率为 92.02%（`xjwt_ext.c` 为 92.79%，`xjwt_main.c` 为 93.10%）、分支结果为 71.40%；加密私钥仍未支持，属性值的应用层语义不由 JWT 解析器解释。
JWKS 解析原先在 RSA n/e 或 EC x/y 解码分配失败时会把该密钥当成畸形条目跳过，甚至返回缺少有效密钥的非空集合。现在分配失败会释放已解析的所有密钥并使整组解析失败，保留 `XERR_MEMORY`；正常的无效条目仍可跳过，成功解析保留调用前已有的线程诊断。逐分配点回归覆盖 JSON 解析、集合分配和 RSA/EC 字段解码，确认不返回部分集合、错误类别不丢失，恢复分配后可完整解析。Windows JWT 主测试与 Linux ASan/UBSan 主测试各 245 项通过，独立 OpenSSL RSA/EC 互操作通过。重测 JWT 自有源码行覆盖率为 92.25%、分支结果为 71.73%（`xjwt_ext.c` 为 93.24%/68.31%）。README 的旧手写编译路径已改为仓库根目录的跨平台测试脚本。
JWT 验签入口原先会在复制 header `kid` 分配失败后继续验证，JWKS 路径可能退化为无 `kid` 选首钥；JSON/PEM 解码及签名错误包装也可能覆盖内存错误。现在 HS/RS/ES 的 PEM、缓存公钥与 JWKS 验签逐分配点回归确认无 claims 返回且保留 `XERR_MEMORY`。公钥缓存还会在 RSA 探测发生内存不足后尝试 EC 并返回成功，已改为立即失败；正常 EC 探测成功则移除内部 RSA 诊断，并保留调用前已有的错误。解码请求算法输出时，header 失败会释放 claims，算法和 kid 仅在全部成功后交付，失败时为 `INVALID`/`NULL`；claims/header 的对应 JSON 须为对象。缓存与解码逐分配点测试另核对所有临时资源释放。
`kid` 按 [RFC 7515 第 4.1.4 节](https://www.rfc-editor.org/rfc/rfc7515.html#section-4.1.4) 保留区分大小写的字符串语义；规范没有非空要求，显式空标识只匹配显式空 JWK 标识，不匹配无 `kid` 条目。当前 C 字符串接口明确拒绝含 NUL 的 token 标识，并跳过含 NUL 或非字符串标识的 JWK；算法名、JWK 类型与曲线也不按 NUL 前缀识别。有效签名的对抗令牌和混合 JWKS 确认坏条目被拒绝而其他有效密钥仍可验证。Windows/Linux JWT 334 项、Linux ASan/UBSan、双平台最小构建/中间件范例/OIDC 组合及 Linux 独立 OpenSSL RSA/EC 解析签验通过；完整认证覆盖率门禁通过，OAuth2 205 项仍通过且覆盖率保持 87.02%/68.02%。本次 JWT 自有源码行覆盖率 93.30%、分支结果 74.07%，其中 `xjwt_main.c` 为 94.93%/83.60%，`xjwt_ext.c` 为 93.74%/69.34%。独立外部服务及生产宿主调用方兼容性仍需验收。
OAuth2 便捷传输的 authority 原先用 `atol` 解析端口，可能把 `:80junk` 或 `:80@other` 当作端口 80；现在按 [RFC 3986 第 3.2 节](https://www.rfc-editor.org/rfc/rfc3986#section-3.2) 分离 authority、路径和查询，拒绝 userinfo、畸形端口、非法数字地址与未编码的控制字符，片段不进入 HTTP 请求目标。[RFC 6066 第 3 节](https://www.rfc-editor.org/rfc/rfc6066#section-3) 不允许 IP 字面量出现在 SNI；DNS 端点保留 SNI，IPv4/IPv6 端点只设置不带方括号的证书校验名，并关闭拨号器从主机自动补 SNI。慢速分片回环验证完整响应共用单调截止时间；失败时 Abort 丢弃待发送字节，随后释放额外 Stream 引用。Windows/Linux OAuth2 主套件、最小构建、OIDC 组合与三项模拟范例通过，Linux ASan/UBSan 205 项通过；JWT/OAuth2 sanitizer 已接入 CI。重测 OAuth2 自有源码行覆盖率 87.02%、分支结果 68.02%，其中 `xoauth2_http.c` 为 80.21%/62.36%。另以 OpenSSL 临时签发的证书和独立 Python TLS 服务端实测 DNS SNI、IPv4/IPv6 不发送 SNI、非受信 CA、错误 DNS 身份及缺少 IP 身份拒绝，原有六项证书与身份场景，加上握手中断和静默超时两项，已在 Windows/Linux 本地通过；Linux 运行 ASan/UBSan，双平台 CI 均已接入独立测试。故障场景确认收到 ClientHello，客户端按期失败，状态码与响应体清空；外部提供商互操作尚未验收。
OAuth2 新增独立响应边界回归复现了三处接收问题：描述符容量小于允许字段数，
chunked trailer 没有可用存储，以及已经消费的 chunk 扩展仍留在接收缓冲。
现在响应头与 trailer 各支持 100 字段并拒绝第 101 字段；正文计划建立后复用
字段数组保存 trailer，接收正文时只保留尚未消费的字节。2048 字节正文携带约
8 MiB chunk 扩展的合法样本在 Windows 上，其 XRT 跟踪分配峰值从修复前
18,370,653 字节降至 1,359,381 字节（开始交换前峰值为 1,217,992 字节）；
这是库的分配器指标，不是进程 RSS。该内存断言及 8 MiB POST 部分上传超时、
断流各连续运行五次通过。上传失败测试保留额外 TLS 引用，在 HTTP 对象销毁前
要求对端看到异常关闭、正文未全部发出且没有重放；仅在隔离副本中去掉 Abort
会使测试失败，确认 Destroy 的隐式清理不能掩盖缺失的主动中止。
正文接口是无长度的 C 字符串，原始 NUL 原先能使成功结果只露出有效前缀；
现在在交付前拒绝。恰好 1 MiB 可交付，超大定长声明和累计超大的 chunked
正文都返回 `XOAUTH2_ERROR_NETWORK`，并清空响应输出。

合法关闭定界正文原先也会因 Recv Future 的 `XFUTURE_CLOSED` 而失败。
现在 TLS 额外等待 `XTLS_STREAM_WAIT_END` 的成功终态，以确认认证
`close_notify`；TCP 核对公开统计的读端结束和终止错误。此区分仅用于接收
EOF，发送仍只接受 `XFUTURE_RESOLVED`。Windows 的 TLS 认证关闭、直接断流、
TCP 正常 FIN 与不足定长正文均通过；认证关闭后的不足定长或未结束 chunked
正文也被拒绝，正常关闭不能代替 HTTP 分帧完成。独立脚本默认共 25 个场景，
TLS 认证关闭、直接断流及两项 TCP EOF 场景另各连续运行五次通过。
完整 Windows 严格构建通过，已沿用双平台 CI 与 Linux sanitizer 入口。
接收加固阶段的认证主套件测得 Windows OAuth2 行 86.72%、分支结果 67.25%，
`xoauth2_http.c` 为 79.60%/60.81%；这个 gcov 套件不包含上述独立 HTTP/TLS
驱动，因此不能用其覆盖率判断新增 TLS 场景是否充分。本轮 Linux 重验尚未
运行：本机 C 盘空间不足，WSL 启动发生 I/O 错误。已将本任务日志和临时产物
移到工作区所在 D 盘；没有把既有 Linux 通过结果当作本轮验收结果。

随后给 OAuth2 独立探针的全部场景开启内存调试并检查清理结果。正常响应先
通过，但握手超时复现销毁覆盖原始错误并遗留 135 项、1,148,863 字节活动
分配。现在 `xoauth2HttpXrtCleanup` 用公开 TryDestroy 限时等待自建引擎的
异步 Close/Abort，只有 READY 才放弃拥有权；失败保留句柄供再次清理，
`Destroy` 不释放尚未退休的堆句柄。清理保留调用前的非空诊断，并用 bool
返回值明确区分成功和失败；Unit 沿用同一路径。借用引擎不会被停止或销毁。
清理后即使拥有引擎仍运行，也拒绝再次请求，避免已释放的 resolver/verifier
进入拨号流程。真实 Pin 验证超时、Unit、Destroy 三条拥有权路径；另注入
退休错误，释放阻塞后均可重试成功，重复 Cleanup 幂等且无活动分配。

无效 CA 配合 1 微秒时限又复现初始化失败遗留 110 项、1,137,088 字节。
初始化改为先完成 resolver/证书配置，再创建、启动私有 engine；启动失败
同步释放尚未投入使用的 engine。无效 CA 回归与构造从首个分配到成功之间
127 个位置的真实 OOM 注入均在 Windows 通过，每个位置都确认资源归零。
独立驱动默认共 32 场景（构造 OOM 扫描计一组，IPv6 不可用时少一项），
全部通过；七项生命周期/初始化场景、握手超时、上传超时/断流及 chunk
内存样本又各连续五次通过。隔离副本分别改为无条件释放忙碌堆句柄、退休
错误后丢弃引擎拥有权，两种变体都被运行时断言拒绝，生产源码哈希保持一致。
本轮完整认证九项测试/最小构建/离线范例通过；最后初始化顺序与已清理句柄
保护改动后，OAuth2 主测试 212 项及最小构建再次通过。最新主套件测得
OAuth2 行 86.54%、分支结果 67.03%，其中 HTTP 为 79.48%/60.57%；JWT
仍为 93.30%/74.07%。这项 gcov 统计仍不包含独立场景，不能据此否定或
宣称上述分配失败与 TLS 分支的覆盖程度。下文记录环境恢复后的 Linux 重验。
证据存于 `out/goal-logs/20260930/oauth2-retirement-faults-before.log`、
`oauth2-init-short-deadline-before.log`、`oauth2-cleanup-final-32-windows.log`、
`oauth2-cleanup-final-repeat-windows.log`、`oauth2-cleanup-final-mutations-windows.log`、
`oauth2-cleanup-final-auth-windows.log` 和 `oauth2-cleanup-final-coverage-windows.log`。

ACME HTTP 的独立接收回归确认了同类容量与缓存问题，并发现响应字段复制
失败仍返回成功。现在请求与响应字段数组均为 100 项：请求先预留自动生成
字段，不再截掉多余附加字段后越界写入；超限、空字段指针、空正文指针带
非零长度，以及覆盖传输层管理字段会在连接前拒绝。独立服务端验证带正文
POST 的 94 个附加字段全部发出，总数恰好 100；主 HTTP 测试另覆盖 13 类
非法参数，包括 `SIZE_MAX` 数量与大小写混合的管理字段名。
响应头和 trailer 各接受 100 字段、拒绝第 101 字段，先完整复制需要的响应头
再复用字段数组；正文保留 4 MiB 上限并拒绝原始 NUL。2048 字节正文携带
约 8 MiB chunk 扩展时，Windows 的 XRT 分配峰值从 18,370,976 字节降至
1,359,819 字节（开始交换前峰值为 1,217,952 字节）。TLS 认证 EOF、直接
断流、TCP FIN、不足定长与未结束 chunked 正文分别验证，不把关闭终态
当作发送成功。正文阶段直接沿用响应的同一个截止时间，不再构造分片时限。

Location、Replay-Nonce、Retry-After、Link、Content-Type 五处复制分别注入
真实调试分配失败，交换均失败、所有响应字段清零、原始内存错误保留。
零活动分配断言又暴露了清理竞争：`Close`/`Abort` 只投递命令，清理函数
随即调用引擎 Stop/Destroy；引擎仍有活动对象时拒绝退休，但旧实现丢弃
拥有权，留下约 1.15 MiB 的引擎资源。现在 ACME 通过公开 TryDestroy 等待
异步关闭退休，并保留原始诊断；达到清理截止时间或遇到错误时保留拥有权，
释放阻塞对象后可再次 Unit。28 项独立场景在 Windows 严格构建下通过，
每项销毁后都确认活动分配归零且没有重复释放或非法释放；五处 OOM
不能靠丢弃响应字段或忽略销毁错误过关。独立脚本共用 Python HTTP/TLS
线格式夹具，抽取后 OAuth2 原有 25 项在 Windows 重跑通过。ACME 的新增
内存边界、五处字段 OOM、认证 EOF、上传超时/断流和请求容量各额外重复
五次通过；隔离副本中去掉 Abort 或把字段 OOM 当成成功，两种变体都被
运行时断言拒绝，生产源码未受这些故障变体影响。Windows 完整 ACME 套件、
显式单头测试、依赖裁剪、静态及共享包消费者、原有六项 HTTP 故障脚本通过；
模拟 CA 的九项签发、吊销、严格 EAB/联系方式与密钥轮换故障场景全部通过，
包括换钥响应丢失后的对账与只读断流重试。生成单头一致性和产品成熟度检查通过。新增
入口已接入 Linux ASan/UBSan 与 Windows CI；上述接收加固阶段的 Linux
验证曾受 C 盘空间不足导致的 WSL 启动故障限制，恢复后的结果见下文。
OAuth2 的同类引擎清理问题已按上文修复。

ACME URL/SNI 回归用独立 Python 服务端核对实际请求目标、唯一 Host 字段
与 ClientHello 的 SNI。旧实现向 IPv4 字面量发送 SNI、无法解析 IPv6；无
路径查询被端口解析吞掉，路径内片段导致请求行无效，末尾 DNS 根点也原样
进入 SNI，大小写混合 scheme 被拒绝。40 个非法 URL 的服务端陷阱还观察
到九次实际连接，包括数字端口后缀被忽略、端口后的 userinfo 被误读、畸形
百分号及反斜杠进入请求目标。现在完整分离 authority、路径、查询与片段，
端口按有溢出检查的十进制逐字节解析，拒绝 userinfo、非法字符及不规范的
纯数值 IPv4；IP 字面量仅用于证书校验，DNS 校验/SNI 去末尾根点，关闭
拨号器自动补 SNI。规则依据 [RFC 3986 组件/scheme](https://www.rfc-editor.org/rfc/rfc3986#section-3)
与 [RFC 6066 SNI 名称](https://www.rfc-editor.org/rfc/rfc6066#section-3)。
合法向量验证端口 1/65535、前导零十进制端口、IPv4-mapped IPv6 校验名与
1023 字节请求目标/查询前缀容量；16 个非法 URL 也加入模块化 HTTP 主测试。

扩展后的独立驱动共 38 场景（40 个非法 URL 与合法边界向量各计一组，
IPv6 不可用时少一项），Windows 与 Linux ASan/UBSan 全部通过，非法
URL 均在拨号前拒绝。Linux 的 URI/TLS 身份、边界向量及 40 非法 URL 又
各连续五次通过；隔离副本中恢复 IP SNI 或宽松端口解析均被运行时断言
检出，生产源码哈希未改变。共享 Python TLS 夹具新增唯一 Host 字段核对，
OAuth2 六项身份场景在 Windows 重跑通过。

C 盘空间恢复到约 4 GiB 后，WSL 实际启动并重新执行验证。OAuth2 当前
32 场景在 Linux ASan/UBSan 通过，构造 OOM 扫描覆盖 139 个位置且每次
资源归零；JWT/OAuth2 主测试 334/205 项在 Linux ASan/UBSan 通过。
Linux 完整认证九项测试、最小构建与所有离线范例也通过，OIDC 组合 15 项通过。
ACME URL 改动前的接收/资源 28 场景也已重验通过，随后上述 38 场景覆盖
最终 URL 实现。完整 ACME 模块化套件、显式单头、依赖裁剪、静态及共享
消费者在 Windows 和 Linux 均通过；Linux 存储测试设置输出根到原生
`/tmp/xacme-url-20260930`，没有用 `/mnt/d` 的模式位冒充 POSIX 0600 验收。
Linux 包指定 x64 输出，保留 Windows native 产物，避免两平台写同一静态
库。Linux/Windows 模拟 CA 九项签发、吊销、严格 EAB/contact、密钥轮换
和断流恢复全部通过（668.829/673.002 秒）。
生成单头一致性与产品成熟度检查通过，双平台 CI 仍需实际托管运行。
证据目录 `out/goal-logs/20260930/` 包括 `acme-url-identity-before-windows.log`、
`acme-url-invalid-before-windows.log`、`acme-url-final-38-windows.log`、
`acme-url-final-38-linux-asan.log`、`acme-url-repeat-linux-asan.log`、
`acme-url-mutations-windows.log`、`acme-url-full-{windows,linux}.log`、
`acme-url-single-{windows,linux}.log`、`acme-url-static-linux-x64.log`、
`acme-url-shared-linux-x64.log`、`acme-url-mock-{linux,windows}.log` 和
`oauth2-cleanup-final-32-linux-asan.log`、`auth-cleanup-final-linux-asan.log`。
片段合法性及混合十六进制旧式地址已在下文 2026-10-01 URI 批次补齐；已清理上下文的
拒绝复用在后续生命周期批次补齐。当前传输不是一个覆盖全部 URI 形式的通用解析器。

本地 Windows GCC 与 Ubuntu/WSL GCC 验证不能代替新增 CI job 的实际运行结果。全仓 `check_api_reference_detail.py --all` 当前还在 `cancel.md`、`core.md`、`value.md` 等核心 API 文档报缺失项；扩展库局部构建通过不代表该全仓文档门禁通过。
认证库覆盖率由 `python tools/measure_auth_coverage.py` 对库自身 `.c` 的行数和
gcov `Taken at least once` 分支结果数分别加权计算，排除测试代码和 xrt 核心；
CI 暂以 `xjwt` 行 85%/分支 67%、`xoauth2` 行 80%/分支 60% 作为回归底线，
这不是最终的场景充分性指标。
邮件库覆盖率由 `python tools/measure_mail_coverage.py --product <库>` 在 Linux/GCC
下运行各自现有套件并统计库自身 `.c`，排除 XRT 核心与其他邮件库；分支结果指标
采用 gcov 的 `Taken at least once`。四库 CI 暂设置低于实测值的回归底线，
不代表故障路径已充分覆盖。`xmail` 自身套件不包含所有下游 POP3/SMTP/IMAP
调用，跨套件代码触达仍需单独分析。
在 IMAP 覆盖率套件中，共享的 `mail_net_deflate.c` 达到行 86.78%、分支结果
59.26%，说明它在 `xmail` 单独套件中的 12.4% 行覆盖并不代表实际集成路径未测。
`xacme` 的签发产物从直属文件切换为 `current` 指向的版本目录，这是磁盘布局变更。
旧布局仅在没有 `current` 时读取；直接消费磁盘文件的宿主需先读取一次 `current`
并固定同一目录，或改为调用 `xrtAcmeStoreLoadGrant`。提交依赖底层文件 flush/rename；
POSIX 目录项现已显式同步，但尚未在目标文件系统上做断电恢复验证，不能据当前
测试宣称持久性已完全验收。发布后的目录同步失败会返回 false，而新版本可能已
可见，调用方须读取当前授权来核对结果。旧版本和
崩溃遗留目录不会自动回收；`tools/prune_acme_store.py` 提供默认预览和显式
执行模式，只能在全部读者与写者停止后使用。它不会安全擦除磁盘块中的私钥。
`current.lock` 用于写者协调，运行期间不能删除；本地双进程压力已覆盖
Windows、Linux 原生目录和 WSL 的 Windows 挂载盘，但不能替代目标卷的
断电恢复和跨进程崩溃注入。CI 中新增双进程脚本，实际托管结果仍待运行。
通配符域名的磁盘目录从 POSIX 可用、Windows 不可用的 `*.example.com`
统一映射为 `%2A.example.com`；旧 POSIX 目录仅在新映射目录尚未提交时
回退读取。直接依赖磁盘路径的宿主需按此布局迁移。
DNS provider 删除句柄已改为完整保存 Cloudflare 的双 32 字符 ID、华为云
较长 ID 及腾讯云完整 zone；添加成功但响应缺少可用记录 ID 时会报失败，
记录表已满时在发请求前拒绝。AWS 保存完整 TXT 值，避免删除时回放截断值。
华为云创建 TXT 的请求体已修复为合法 JSON，并用离线解析断言固定域名尾点与
TXT 引号编码。
华为云 SDK-HMAC-SHA256 签名按[官方签名规则](https://support.huaweicloud.com/intl/en-us/devg-apisign/api-sign-algorithm-001.html)
将 canonical URI 与查询参数分离，签名 URI 补尾斜杠并按字典序排列查询参数；
Signature 直接对 StringToSign 使用 SK 做 HMAC。GET 查询与 POST 写请求的
固定时间向量由独立 Python 标准库计算，另覆盖参数顺序、输出容量和非法目标。
Windows/Linux 完整 ACME 套件和单头测试、Linux 定向 ASan/UBSan 以及单头
一致性检查已通过。这些是离线签名验证，真实华为云 DNS 互操作仍待隔离账号执行。
阿里云 ACS3 签名按[官方 V3 规则](https://www.alibabacloud.com/help/en/sdk/product-overview/v3-request-structure-and-signature)
补入必需的 `x-acs-signature-nonce`，将 query 按参数名排序并拒绝未编码分隔符、
重复键与超长输入；`Authorization` 使用规范的逗号分隔格式。每次传输尝试
生成新随机 nonce 和签名，避免响应丢失后的重试重用 nonce。固定时间与 nonce
的签名向量由独立 Python 标准库计算；真实阿里云账号互操作尚待受控验证。
阿里云已取消添加前按 FQDN/RR/TXT 同值预清理：这些字段不能证明记录属于
当前 provider，误删并发签发者的记录比留待人工核对更危险。`Remove` 只按
本实例从成功创建响应拿到的 RecordId 删除。添加请求发送后若响应丢失，
provider 和流程层都不重放；此时可能遗留无句柄的记录，需要在服务端核对。
流程保留 `XACME_HTTP_ERROR_UNCERTAIN` 根因；若多域名签发的后续 TXT 添加
失败，已成功添加的前序 TXT 仍会进入清理。离线模拟 CA 分别注入第一次和
第二次添加的未知结果，校验调用次数、前序清理及最终错误码。这些改动尚需
真实账号的添加/删除及异常恢复互操作验证。
阿里云创建响应的 RecordId 与删除路径共用未保留 ASCII 校验；
异常分隔符、控制字符、错误类型及容量不足会在进入记录表前拒绝，删除时
再校验一次。离线回归覆盖最大可保存 ID 和短缓冲区；这不能代替真实服务
返回 ID 格式的互操作验证。
阿里云、Cloudflare、腾讯云、华为云的创建记录已绑定 FQDN/TXT；`Remove`
只选择匹配记录，失败后保留未删句柄，后续调用可重试并复用已释放槽位。
离线注入测试覆盖同名不同值和部分删除失败；DNS-01 输入字符在请求前检查。
腾讯云还检查 `Response.Error`，避免 HTTP 2xx 内的业务失败被当作成功。
共享 zone 缓存会优先选择最长后缀，并避免重复占用槽位；父、子 zone
同时存在的回归样本已加入。
Route 53 已改为先列出同名 TXT 记录集，再用 `DELETE` 旧集合和 `CREATE`
新集合的单批原子请求叠加或移除目标值，保留其他值和原 TTL；记录集冲突会
重新读取并限次重试，响应丢失后也重新读取确认结果。离线测试覆盖集合保留、
空集合、末条删除、畸形结构和非简单路由集合的拒绝；AWS 实际互操作仍待验证。
zone 发现会跳过同名私有托管区；同名多个公有区及分页仍可能存在同名区时
安全报错，离线样本覆盖这两种歧义。尚未提供显式 zone ID 选择，相关真实
账号布局和分页请求仍需互操作验证。
该 provider 的共享 HTTP 与记录槽位由互斥锁串行化；其余 provider 同时调用的上下文同步、
阿里云结果未知后的无句柄记录对账，以及真实服务上的添加/删除互操作仍未验收。
ACME HTTP 响应体上限为 4 MiB，超过时按协议错误终止，防止不可信服务端
无限扩张接收缓冲。
共享 HTTP 交换在响应头已分配后若读取响应体失败，会释放所有部分响应字段并
清零输出。GET/HEAD 只在未收到响应字节时自动重试；其他方法只在发送前失败时
重试，发送后失败返回 `XACME_HTTP_ERROR_UNCERTAIN`，避免把“未见响应”误判为
“服务端未执行”。只读的 ACME POST-as-GET 在上层重新获取 nonce、重建 JWS 后
限次重试；阿里云自管重试仅允许查询在结果未知时继续，创建/删除立即返回，
流程层也不再重调结果未知的 DNS Add。
连续可重试失败保留首个根因，后续不可重试错误保留该次错误。接收头和响应体
共用一次总截止时间，空读取不再无限延长超时。

ACME 生命周期审计复现了无效 CA 加 1 微秒初始化超时留下 110 项、1,137,088
字节活动分配的情况（`acme-lifecycle-before-windows.log`）。现先构造 resolver
及验证器，再启动私有引擎，启动失败同步回收；`xacmeHttpUnit` 返回清理结果，
在任何子资源拆除前保留原始错误。清理超时或退休 ERROR 时保留私有引擎；
客户端及五家 provider 不再无条件清零或释放拥有者。新增公共
`xrtAcmeClientCleanup`：true 后可 Destroy，false 时保留句柄供重试；
ProviderUnit 以 `pContext == NULL` 表示完成。开始清理后请求立即以 ACME 参数
错误拒绝，实际监听器确认没有拨号；借用引擎保持运行。失败客户端构造与
一站式流程的临时对象单独留出至少 30 秒回滚预算；证书密钥解析失败时清除
密钥内存，重试清理不会重复释放证书密钥。

`tools/test_acme_http_interop.py` 的默认 CI 路径现包含生命周期与拥有权探针。
Windows/Linux ASan 的完整独立 HTTP/TLS 回归通过；新增 HTTP 退休、重试、
原始错误及借用场景、清理后拨号陷阱、24 种客户端/provider 拥有权组合、
五种公开构造失败与六轮构造 OOM 扫描通过，结束时活动分配归零且无非法或
重复释放。HTTP 构造 OOM Windows 扫描 126、Linux 138 位置；五家 provider
分别扫描 127/139 位置，失败客户端分别扫描 131/143 位置。provider 的
构造探针用同一个测试 CA 替代宿主系统信任库，避免证书数量影响扫描范围。
公开 Create 从全零外壳开始，缺失、空值或超长 directory 在失败清理时不会
读取未初始化的传输指针；这三项参数路径同样检查原始参数错误及零活动分配。
这一补充后的最终生命周期/拥有权回归见
`acme-lifecycle-last-validation-{windows,linux-asan}.log`；其余 HTTP/TLS
完整分组的日志与下方五次重复日志分别记录各自验证范围。
日志为 `out/goal-logs/20260930/acme-lifecycle-final-{windows,linux-asan}.log`；
Linux ASan 连续五次生命周期与拥有权验证见 `acme-lifecycle-repeat-linux-asan.log`。
三个独立副本中的拥有权丢弃变体均被实际断言检出，生产源码哈希不变，见
`acme-lifecycle-mutations-windows.log`。双平台完整 `xacme_tests`、显式单头、
裁剪及静态/共享包消费者通过，包消费同时验证新 Cleanup 符号。
Linux/Windows 模拟 CA 各九项通过，分别耗时 788.598/793.062 秒，见
`acme-lifecycle-mock-{linux,windows}.log`；覆盖签发、吊销、存储、EAB、
账户换钥及断流重试。同步验收时共享核心文件 API 有并行改动，已保留并
重新生成 ACME 单头、核对一致性及包消费。真实服务与托管 CI 仍待验证。

后续复现了失败工厂在持续退休 ERROR 或回滚预算耗尽时留下 111 项、1,141,632
字节活动分配，却没有调用者可达重试路径的情况，见
`out/goal-logs/20261001/acme-rollback-before-windows.log`。现以 HTTP 首字段的
内嵌链结转移未交付堆对象的拥有权，入列不分配内存，立即清除非 HTTP 的密钥、
凭据及业务字段；新增公共 `xrtAcmeCleanupPending` 以返回值和数量报告结果。
计数包含其他清理调用已取出的对象，只有全部释放才返回 true；已有错误保留，
非阻塞 BUSY 不制造错误，退休 ERROR 提前返回。未完成时暂停创建新私有引擎，
借用引擎仍可使用。已交付客户端/provider 继续由调用者持有，不进入该队列。
宿主须停止新调用、等待在途调用结束，清理已交付实例及队列后才退出或卸载。
随后补齐失败诊断链：HTTP 初始化保留底层失败为 cause，部分启动失败先保存
启动错误再执行同步回滚；包装分配失败时也保留原错误。五家失败工厂的探针
明确检查错误链中仍含启动首因，再核对延后清理前后的诊断指针不变。

本轮真实模拟 CA 回归又暴露第二次 Obtain 被待清理队列拒绝。根因是 DNS TXT
查询仅 Destroy UDP 引用，却未关闭套接字，UDP 长期占用引擎；过去临时客户端
销毁忽略退休失败，端到端成功掩盖了这一问题。现查询完成明确 Close，失败则
Abort，并保留原始诊断。内部 DNS Unit 也改为有界等待、BUSY/ERROR 保留拥有者，
启动失败保留引擎供重试；借用引擎不停止，已清空 DNS 上下文拒绝查询。

默认独立脚本新增六类失败工厂的 ERROR/BUSY 共 12 组合、零分配入列与轮询、
并发取出计数、并发入列/回链、四线程生产与清理、ERROR/超时后的尾链保留，
以及真实 Obtain Done 路径的成功/失败两类各 ERROR/BUSY 组合。后者仅控制其
构造、账户存储及签发依赖，实际清理和引擎仍使用生产实现；真实签发由 mock
补充验证。另含四类内部 DNS 拥有权场景，以及独立 UDP 服务端的七类 TXT、
NXDOMAIN、畸形应答、丢包超时、过期事务 ID、发送/接收故障回归。各查询结束
等待实际活动 UDP 对象归零，借用引擎继续运行；每项末尾检查活动分配为零，
无非法或重复释放。最终 Windows/Linux ASan 结果见
`out/goal-logs/20261001/acme-deferred-dns-{windows,linux-asan}.log`；此前完整
HTTP/TLS 各分组通过见 `acme-deferred-final-{windows,linux-asan}.log`，日志分别
保留所验证的源码阶段与范围。独立探针尚未纳入统一行/分支覆盖率统计。
最后诊断链补充后的生命周期、拥有权、六轮构造 OOM 及七类 UDP 回归见
`acme-deferred-last-validation-{windows,linux-asan}.log`。

隔离副本中的五种错误变体（漏算已取出对象、丢弃未处理尾链、失败工厂丢弃
拥有者、Obtain 使用仅保留调用者句柄的 Destroy、UDP 不关闭）均被实际断言
检出，生产源码哈希未改变，见 `acme-deferred-mutations-final-windows.log`。
修复后的双平台完整 `xacme_tests` 通过，见 `acme-deferred-full-final-*.log`。
本轮重跑模拟 CA 的正常全链路与断流重试两项，两平台均通过，耗时 Windows
184.949/Linux 183.673 秒，见 `acme-deferred-mock-final-{windows,linux}.log`；
同时在真实流程末尾确认已交付客户端、DNS 传输及待清理队列均释放。其余七项
模拟场景的最近完整验证仍为上文 2026-09-30 批次。

最后诊断链补充后，关键延后拥有权、四类 DNS 拥有权及七类实际 UDP 查询在
Linux ASan/UBSan 下连续五次通过，见 `acme-deferred-repeat-last-linux-asan.log`。
最终隔离副本再增加丢弃启动 cause 的变体，共六种均被实际断言检出且生产
源码哈希不变，见 `acme-deferred-mutations-last-windows.log`。最终完整模块化
套件为 `acme-deferred-full-last-{windows,linux}.log`；双平台显式单头及
静态/共享包消费见 `acme-deferred-single-last-*.log` 和
`acme-deferred-package-last-{static,shared}-*.log`，实际消费者调用了新的
Pending 入口并检查空队列计数。依赖裁剪见 `acme-deferred-trim-final-*.log`；
之后的诊断链补充未改动依赖。生成特性、单头与公开符号参考一致性、API 文档
覆盖及发布成熟度门禁通过。新增 Pending/DNS 回归已进入既有双平台 CI 默认
脚本；真实服务、宿主卸载演练、统一覆盖率及托管 CI 结果仍待验证。

ACME 与 OAuth2 的 HTTP 传输现只把 Future 的 `XFUTURE_RESOLVED` 视为异步 I/O
成功；失败、取消和关闭终态不再被当作发送完成，ACME 接收失败立即报告 I/O
错误。ACME 分块发送共用一次总截止时间，失败时 Abort 丢弃排队数据；8 MiB
慢读 POST 在 Windows/Linux 本地约 1 秒返回，服务端各只收到一次请求。
Windows/Linux 独立回环故障脚本验证响应体截断、停顿、GET 断流重试、普通 POST
和单次 POST 不重放，以及大请求发送超时；脚本已接入双平台 CI 及 nightly sanitizer。mock CA 的 TLS/JWS
用例验证 GET 和一次 POST-as-GET 断流后恢复。Windows/Linux 完整
`xacme_tests`、单头测试与静态包消费者验证，以及 Linux 定向 ASan/UBSan 回归通过；
托管 CI 结果待验证。对于已发送
但响应丢失的写操作，DNS 添加/删除仍需按具体服务对账；账户密钥轮换增加了
客户端自动对账，真实 CA 上的恢复语义仍未验收。
账户密钥轮换曾将 RFC 8555 的 JWS 内外层钥匙反转，而本地模拟 CA 也复现了
同一错误。现按[规范的内层新钥 JWK、外层旧账户 kid](https://datatracker.ietf.org/doc/html/rfc8555#section-7.3.5)
修正两端；轮换前及发送后响应丢失时，使用
[onlyReturnExisting](https://datatracker.ietf.org/doc/html/rfc8555#section-7.3.1) 查询新钥对应
的账户 URL，只有 URL 与当前 kid 一致且账户状态有效才切换内存密钥。模拟 CA
故障注入覆盖换钥已提交后断响应、处理前断开、重复调用、`badNonce` 和本地
store 重存失败；重存失败会报告失败，但内存保留已生效的新钥。真实 CA 的
密钥轮换互操作、跨进程崩溃时新钥 PEM 的外部持久化仍需受控验证。
ACME 对 POST 使用强制 nonce 防重放；原样重放已处理请求会触发 nonce 错误，
而换新 nonce 重发写操作可能再次执行。上述策略依据
[RFC 8555 §6.5](https://www.rfc-editor.org/rfc/rfc8555#section-6.5)。
Cloudflare 与华为云的请求 URL 原本分别使用 400 字节固定缓冲，但最长允许的
endpoint 与 zone 查询路径组合可能超过该容量；现在两者共用带返回长度检查的
HTTPS URL 构造，512 字节缓冲容纳当前已验证的上限，容量不足时不会发请求。
近上限与短缓冲回归在双平台完整 ACME 套件及更新后的单头中通过。阿里云和
Route 53 原有显式格式长度检查；腾讯云 URL 的当前输入上限小于其缓冲容量。
其余签名头、请求体和路径固定缓冲区仍需逐项审计。Cloudflare 与华为云的 zone/record ID 过去只受 JSON 类型和长度约束，随后直接拼入 URL 路径；现在共用安全路径段校验，拒绝空值、`.`/`..`、斜杠、问号、片段符、百分号、控制字符和嵌入 NUL。zone 响应解析区分找到、确实缺失和协议错误；匹配名称的非法 ID、畸形数组或无效名称会立即失败，不再继续探测父 zone；Cloudflare zone 与新建记录响应还须显式返回布尔 `success:true`。两家 provider 均拒绝多个同名 zone 及已填满的查询页，防止未读候选造成首项误选；华为云查询改为 `limit=2` 以发现第二个候选。华为云 [zone 列表 API](https://support.huaweicloud.com/intl/en-us/api-dns/dns_api_62003.html) 默认模糊搜索，原先 `limit=1` 可能漏掉精确 zone；现显式使用 `search_mode=equal`，请求长度由格式化检查，新增独立计算的签名向量及接近 DNS 长度上限的 URL 测试。离线 JSON 对抗样本与双平台 `test_dnscf`、`test_dnshuawei` 严格构建、Linux ASan/UBSan 通过，生成单头在 Windows/Linux 编译并运行；真实服务返回 ID 的互操作仍待受控账号验证。
签名审计发现腾讯云 TC3 的 `StringToSign` 曾使用 ISO 时间文本，而传输头使用 Unix 秒，且两次取时可能跨 UTC 日期；派生密钥时还把 `tc3_request` 的结尾 NUL 计入 HMAC。现由同一次 UTC 时间生成签名、凭据日期与请求头，固定 Authorization 向量和 UTC 跨日向量通过。AWS Route53 的 SigV4 密钥链也曾把 `aws4_request` 的结尾 NUL 计入 HMAC；现按 [AWS 官方密钥派生公式](https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_sigv-create-signed-request.html) 修正，并以独立计算的 32 字节密钥向量回归。腾讯云修正依据 [官方 TC3 规范](https://cloud.tencent.com/document/api/213/30654)。两家离线签名回归在 Windows/Linux 严格构建及 Linux ASan/UBSan 连续 5 次运行下通过；Windows/Linux 完整 `xacme_tests` 与各自单头测试通过。Linux 完整套件设置 `XACME_TEST_ROOT` 到 WSL 原生 `/tmp`，覆盖真实 POSIX 私钥模式位。真实 DNS provider 互操作仍待受控账号验证。
本轮完整套件、单头与静态包消费在 Windows 和 Linux 原生文件系统上通过；
WSL 的 `/mnt/d` 挂载盘不能用来验证私钥 `0600` 断言，因此 Linux 完整套件在
原生文件系统隔离工作树运行。托管 CI 的结果仍待实际执行。

2026-10-01 ACME URI 批次确认，忽略 `#` 后内容前未校验语法，会放过
非法百分号编码、控制字符、反斜杠、额外 `#` 和未编码的非 ASCII 字节。
点分 `0x`/`0X` 与十进制/八进制混合地址也被当作 DNS 身份；Linux 宿主
解析器可将其中九种转换为回环 IP。新增真实监听器样本在旧实现中观察到
Windows 23/Linux 32 次连接，见 `out/goal-logs/20261001/acme-uri-before-*.log`。
现按 [RFC 3986 §3.5](https://www.rfc-editor.org/rfc/rfc3986#section-3.5)
校验片段后再丢弃；空片段、合法编码和长片段允许，不占请求目标容量。
按 [§7.4 的旧式数值形式说明](https://www.rfc-editor.org/rfc/rfc3986#section-7.4)
识别全部数值分量，除规范四段十进制 IPv4 外拒绝旧式地址；
`0xdead.example`、`a.0x1.` 等含非数值标签的 DNS 名仍允许。

Windows/Linux ASan 的独立 URL/TLS 本轮 15 组通过（包括 IPv6 回环），
79 个非法 URL 均在拨号前拒绝，检查错误种类、空响应及销毁后的活动分配。
正向向量另穷举 255 个非 NUL 片段字节，验证 4096 字节合法片段、
1023 字节请求目标加片段的容量独立性、IP/DNS 校验名及不发送 IP SNI。
证据为 `acme-uri-fixed-{windows,linux-asan}.log`；Linux ASan/UBSan 连续五次
通过见 `acme-uri-repeat-linux-asan.log`。隔离副本中的“忽略非法片段”与
“把混合地址当作 DNS”两个变体均被实际错误/连接断言检出，生产源码哈希
未改变，见 `acme-uri-mutations-windows.log`。默认独立驱动扩为 40 个 HTTP/TLS
场景，仍包含 DNS 与拥有权回归；本轮只重跑上述 URI/TLS 分组，其他故障
与资源场景的最近验证保持前文批次范围，不能据此把所有 40 组视为本轮重跑。

双平台完整 `xacme_tests`、显式单头及裁剪通过，见 `acme-uri-full-*.log`、
`acme-uri-single-*.log` 和 `acme-uri-trim-*.log`。模拟 CA 正常链路重新通过，
包括签发/吊销、存储与两次 Obtain、账户轮换及退出清理，Windows 91.948/
Linux 91.103 秒，见 `acme-uri-mock-*.log`；其余模拟场景未在本批次重跑。
Windows native/Linux x64 的静态与共享包均由真实消费者链接运行通过，见
`acme-uri-package-{static,shared}-{windows,linux}.log`；两平台输出目录保持独立。
生成特性、单头及 API 参考一致性、文档覆盖与发布成熟度检查通过。
现有双平台 CI 默认脚本包含新增样本；独立场景的统一覆盖率与托管 CI
结果仍待验证。OAuth2 的对应输入边界已在后续批次补齐，见下文。

2026-10-01 OAuth2 URI 批次确认其独立解析器拒绝混合大小写的 scheme，
忽略非法片段，且纯整数、裸十六进制及点分混合数值地址被当作 DNS 名。
扩展监听器样本在旧实现中观察到 Windows 23/Linux 36 次实际连接，真实
TLS 测试也观察到混合大小写 HTTPS 被格式拒绝，见
`out/goal-logs/20261001/oauth2-uri-before-{windows,linux}.log` 与
`oauth2-uri-before-wire-windows.log`。现按
[RFC 3986 §3.1、§3.5 与 §7.4](https://www.rfc-editor.org/rfc/rfc3986)
修复三处边界；使用显式 ASCII 十六进制判断，不依赖区域设置。
含非数值 DNS 标签的名称仍保留 DNS 身份，合法片段不改变 HTTP 目标或
占用目标容量。两库非法 URL 的独立监听器驱动移至公共 Python 夹具，
没有增加 OAuth2 对 ACME 的产品依赖；ACME 同批 15 组 URI/TLS 在 Windows/
Linux ASan 重验通过，见 `oauth2-uri-shared-fixture-*.log`。

OAuth2 默认独立驱动扩为 41 个场景，双平台全部通过，含 IPv6 回环、
85 个非法 URL 的拨号前拒绝、7 组实际 TLS/请求目标回归及合法容量/字节
向量，原有 32 组接收、上传、OOM 与生命周期场景也全部重跑。每项保留
空失败响应及销毁后的分配归零检查；构造 OOM 扫描仍为 Windows 127/
Linux 139 个位置。证据为 `oauth2-uri-full-{windows,linux-asan}.log`。
Linux ASan/UBSan 下 URI/TLS、容量向量及 85 个非法 URL 连续五次通过，
见 `oauth2-uri-repeat-linux-asan.log`。隔离副本中的片段校验遗漏、混合地址
当作 DNS、scheme 大小写敏感三个变体均被运行时断言检出，生产源文件
哈希未改变，见 `oauth2-uri-mutations-windows.log`。

主测试 Windows 215/Linux 208 项、最小模块闭包、OIDC 组合 15 项及三项
离线范例在双平台通过；Linux 六类构建均使用 ASan/UBSan，见
`oauth2-uri-auth-{windows,linux-asan}.log`。主测试自身的覆盖率门禁重新运行，
Windows/Linux 的 OAuth2 自有源码加权行 86.94%、分支结果 69.17%；其中
HTTP 为 80.93%/66.14%，JWT 保持 93.30%/74.07%，见
`oauth2-uri-coverage-{windows,linux}.log`。该指标仍未合并独立网络场景，
不能据此断言生产级场景全部充分。既有双平台 CI 默认驱动已包含新增回归，
实际托管 CI 结果待验证。

失败构造补充审计又确认 OAuth2 `Create` 在 `Init` 失败后直接释放外壳，
若私有引擎未完成退休，正常调用者仅收到 NULL，无法再重试清理。故障探针
仅模拟“引擎已部分启动后返回失败”及退休 ERROR，使用真实 Core engine
与 Pin 维持 BUSY；两种组合都观察到失败工厂返回 NULL 时仍有活动引擎。
Windows 各留下 110 个活动分配、1,137,088 字节，Linux 各 122 个活动分配、
1,174,696 字节。探针最后利用其专有观察指针显式释放每个引擎并检查分配
归零，见 `oauth2-factory-ownership-before-{windows,linux}.log`。这是受控故障
路径证据，不代表正常构造或已有 OOM 扫描发生泄漏；它证明现有失败出口
不能覆盖全部退休状态。该问题已在下面的未交付拥有权批次修复；该探针保留为
修复前证据，不作为修复后的验收结果。

2026-10-01 OAuth2 未交付拥有权批次增加公共
`xoauth2HttpXrtCleanupPending(uint64_t, size_t*)`。失败的堆构造若仍持有
未退休私有引擎，原外壳零分配入列；宿主可重试清理，不再丢失唯一拥有者。
队列不启动后台线程，允许并发清理，计数包含其他清理调用已取出的对象；
退休 ERROR 或预算耗尽会保留已处理和未处理尾链。待清理对象尚未释放时，
新建私有引擎先非阻塞轮询，仍未完成则拒绝；借用引擎可继续构造。
已交付实例仍由调用者保留句柄并重试 Cleanup，不自动混入未交付队列。

初始化失败回滚至少有 30 秒预算，与请求超时分开，之后恢复原请求时限。
首个初始化/启动错误在回滚前保存并作为 cause 包装；包装本身 OOM 时保留
原始诊断指针。Pending 保留调用前非空诊断；无旧诊断时报告退休错误或
结构化 `XERR_TIMEOUT`，零预算 BUSY 不制造错误。公开头、README 和范例明确
宿主退出顺序：停止新调用、等待在途调用结束、清理全部已交付实例，再重试
Pending 至 true；false 时保留库及运行环境。实际宿主卸载演练仍未验收。

新增独立 C 探针的十组回归使用真实 Core engine、Pin、实际初始化与清理：
栈/堆构造各 ERROR/BUSY、部分启动失败后立即退休、包装分配失败保留首因、
已交付句柄清理失败、零分配交接和轮询、取出对象仍计数及并发入列/回链、
四个真实失败工厂与四个清理线程，以及 ERROR/超时后的尾链保留。每组末尾
核对活动分配和字节为零，无非法或重复释放。夹具受控注入“部分启动后失败”
和退休结果，并将等待缩短；它检查至少 30 秒的预算值，未实际等待完整 30 秒，
也不把该注入等同于操作系统真实启动故障。

默认独立驱动扩为 51 组，Windows/Linux ASan 全部通过，含原有 41 组
HTTP/TLS、85 个非法 URL 及 Windows 127/Linux 139 位置的构造 OOM 扫描，
见 `out/goal-logs/20261001/oauth2-pending-full-{windows,linux-asan}.log`。
主测试 Windows 216/Linux 209 项、最小闭包、OIDC 组合 15 项和三项离线
范例均通过，Linux 六类构建使用 ASan/UBSan，见
`oauth2-pending-auth-{windows,linux-asan}.log`。最后补齐显式 atomic/thread
依赖并使用结构化超时错误后，双平台最小闭包和 API 范例重验通过，见
`oauth2-pending-closure-*.log`；十组拥有权回归也重新通过，见
`oauth2-pending-last-ownership-*.log`，这两份日志对应最后源码阶段。

最终源码的十组拥有权回归在 Linux ASan/UBSan 下连续五次通过，见
`oauth2-pending-repeat-final-linux-asan.log`。隔离副本的失败工厂丢弃拥有者、
漏算已取出对象、丢弃未处理尾链、丢失启动 cause、关闭私有引擎准入限制、
包装 OOM 丢失原诊断六种变体均被实际运行断言检出，严格构建成功且生产
源码哈希未变，见 `oauth2-pending-mutations-final-windows.log`。
最终源码的主测试覆盖率门禁在 Windows/Linux 重跑通过，见
`oauth2-pending-coverage-final-{windows,linux}.log`：OAuth2 自有源码加权行
83.75%、分支结果 66.44%，HTTP 为 74.44%/60.80%，flow 为 86.92%/68.62%，
core 为 95.56%/75.20%。主测试未触达新增队列的多条路径，分母增大后整体
指标下降；十组独立拥有权探针虽已通过，仍未合并进此统计，不能用该指标
替代场景验收。JWT 源码本批次未改，其最近覆盖率仍为上文 93.30%/74.07%。
上述独立回归已进入既有双平台 CI 默认驱动；实际托管 CI、统一覆盖率和
宿主卸载结果仍待验证。

2026-10-01 OAuth2 统一覆盖率批次将主测试、独立 HTTP/TLS/生命周期和
未交付拥有权三个探针纳入默认统计。读取
[gcov JSON 的原始行和分支计数](https://gcc.gnu.org/onlinedocs/gcc/Invoking-Gcov.html)，
仅计算扩展自身 `.c` 文件；以函数位置、基本块和分支结果身份核对控制流后
取触达并集，不平均百分比或重复累计分母。所有探针使用 GCC `-O0`、原子
profile 更新和同一编译器对应的 gcov，平台输出分开，运行前清除本次二进制
的旧计数。源码、公开/内部头及 Core 单头在运行期间变更会使统计失败；
缺少执行计数、未知格式、编译器/控制流不一致或负数计数同样拒绝。

新增统计工具的八类回归在 Windows GCC 16.2/Linux GCC 15.2 通过，见
`out/goal-logs/20261001/coverage-union-tool-final-{windows,linux}.log`。
其中三个真实 C 探针分开触达短路表达式的六个分支结果，合并达到 6/6，
重复计入同一探针不增大分母；另验证旧格式有序分支、相对路径、源码变更、
缺少/重复来源、控制流及版本不一致、负数/非整数计数和重复标签的拒绝。
拒绝不污染已有统计；CLI 的 NaN、无穷和越界门槛不能绕过覆盖率检查。

默认门禁已从 OAuth2 主测试的 80% 行/60% 分支提高为合并后的 91%/75%。
`--main-only` 保留为诊断主测试基线，双平台 CI 使用默认合并统计，并上传
各库 `coverage.json`。报告保留源码/依赖 SHA256、编译参数、各探针新增触达量
和未覆盖位置；计数及 gcov JSON 留在对应平台目录供复核。本地 YAML 结构
检查通过，见 `auth-union-workflow-structure.log`；实际托管 CI 仍待运行。
本批次最终工具的双平台完整 `python tools/measure_auth_coverage.py`
门禁通过，见 `auth-union-coverage-final-{windows,linux}.log`；该阶段在随后
NUL 修复前运行，OAuth2 为 Windows 行 92.24%/分支结果 77.21%，Linux
92.24%/77.12%。JWT 四份自有源文件按实际计数得到 1128/1209 行（93.30%）、
800/1080 分支结果（74.07%），与先前主测试指标一致。初版 OAuth2 独立探针
单库合并见 `oauth2-union-coverage-first-*.log`，相对主测试的新增触达量为
Windows 107 行/113 分支、Linux 105 行/111 分支；
异步回收条件可能使不同运行间少量触达变化，最终报告保留各次实际范围。

合并报告的逐文件缺口进一步暴露令牌字符串的嵌入 NUL 问题。修复前，三个
独立 JSON 样本的 access_token/token_type/refresh_token 均成功返回，后续
`strlen` 只观察到前缀，见 `oauth2-nul-field-before-windows.log`。这既不能
用 C 字符串无损交付，也不符合 [RFC 6749 附录 A 的令牌语法](https://www.rfc-editor.org/rfc/rfc6749#appendix-A)。
现六个公开字符串字段都在 JSON 解码后、字段分配复制前检查 NUL；拒绝时
整份令牌不交付，释放已复制字段并报告 TOKEN_RESPONSE，不做前缀截断。

新回归覆盖六字段的首/中/尾三种位置共 18 个样本，检查失败分类和资源基线；
另验证普通/微信的登录与刷新四个公开入口组合，失败响应不恢复已消费 state。
正向样本确认字面量反斜杠-u 文本仍完整保留，检查不是对原始 JSON 的全局过滤。
回归在既有套件完成后运行，使用已稳定的 Core/诊断初始化状态作为资源基线；
修复前 20 个功能断言失败，见 `oauth2-nul-regression-before-warmed-windows.log`。
修复后 Windows 264/Linux 257 项主测试、最小闭包、OIDC 组合 15 项和三项
离线范例均通过，Linux 六类构建使用 ASan/UBSan，见
`oauth2-nul-auth-fixed-{windows,linux-asan}.log`；Linux 完整主测试连续五次
通过见 `oauth2-nul-repeat-linux-asan.log`。

NUL 修复后的最终源码重新执行双平台 OAuth2 合并门禁，含主测试和全部 51 组
独立场景，见 `oauth2-nul-union-coverage-{windows,linux}.log`：Windows 自有源码
1145/1239 行（92.41%）、808/1044 分支结果（77.39%），Linux 1143/1239 行
（92.25%）、806/1044 分支结果（77.20%）。core 为 95.56%/75.20%，flow 为
86.99%/69.05%，HTTP 为 Windows 94.55%/83.40%、Linux 94.17%/83.00%。
公共 API 注释与 README 已说明字段拒绝及覆盖率复现方式，原有双平台 CI
主测试和默认覆盖率步骤均包含新增回归；实际托管 CI 仍未验收。
最后核对四份 Windows/Linux JWT/OAuth2 报告的源码及依赖 SHA256 均与当前
工作树匹配，各探针新增触达量之和等于合并触达量，默认门槛通过，见
`auth-union-artifact-current-hashes.log`；最终空白检查通过。
flow 的认证请求、微信异常响应、其余输入/分配故障，HTTP 的接收/发送分配
故障仍须补测；统一统计提高了可见性，不能单凭整体百分比判定生产级完成。

2026-10-01 ACME 统一覆盖率批次新增 `tools/measure_acme_coverage.py`，将完整模块
套件、九项模拟 CA、独立 HTTP/TLS/DNS/生命周期及 provider 请求体探针合并。
全部 16 个自有 `.c` 文件始终作为分母；每个探针明确编译范围，逐文件核对
函数位置、基本块和分支结果身份后取触达并集，不能以部分编译缩小分母。
共享统计工具新增范围回归后共十类测试在 Windows GCC 16.2/Linux GCC 15.2
通过，见 `acme-coverage-tool-final-{windows,linux}.log`；JWT/OAuth2 现有原始
报告重新合并，逐文件计数、整体指标及输入哈希保持一致，见
`acme-coverage-helper-auth-replay-{windows,linux}.log`。

独立生命周期探针不含 registry，由模块套件覆盖该文件。Linux 首次合并还
检出模块对象与独立探针的 store 定义不同：未开启 GNU 扩展的模块对象没有
`O_DIRECTORY`/`O_CLOEXEC` 两条代码。现在模块构建显式使用 `-D_GNU_SOURCE=`，
既与测试文件的空值定义一致，也开启与独立探针相同的系统接口；不放松控制流
一致性检查，证据见 `acme-coverage-store-cfg-linux.log`。Windows 在 provider
修复前的首次完整统计为 3583/5000 行（71.66%）、2476/4355 分支结果（56.85%），
九项模拟 CA 无跳过通过；HTTP 为 93.18%/82.00%，flow 为 69.38%/48.74%。
见 `acme-union-coverage-fixed-windows.log` 与保存的
`acme-coverage-before-provider-fix-windows.json`，这些是修复前基线。

逐文件缺口审查又复现五处 provider 字节缓冲未补 NUL 后直接交给 `strlen`
的问题：Cloudflare 创建记录，腾讯云查询/创建/删除记录，以及华为云创建记录。
Cloudflare 的 94 字节 JSON 在合法的非零缓冲尾部下按 164 字节发送；五条路径
的普通和长输入都可复现，见 `acme-cf-body-before-windows.log`、
`acme-provider-bodies-before-windows.log`。这会把有效内容之外的缓冲字节计入
请求，腾讯云和华为云也会把这些字节计入签名。现全部五处在传给 C 字符串
接口前显式追加 NUL，并检查分配结果；签名和 HTTP 的长度不包含终止符。
华为云公开的字节缓冲构造助手保留原有长度契约，只在传输调用处补终止符。

新探针控制 HTTP 响应并填充缓冲尾部，调用实际 provider 的 Add/Remove。
五条路径分别验证普通输入、253 字节域名/200 字节 TXT，以及终止符 append
触发真实下一次分配失败，共 15 项；检查有效 JSON、交付长度、成功后的记录
清理、失败时不提交新记录/不丢弃待删记录、内存错误保留及活动分配归零。
Windows 与 Linux ASan/UBSan 的默认脚本分组通过，见
`acme-provider-body-driver-{windows,linux-asan}.log`；Windows 和 Linux ASan
各连续五次通过，见 `acme-provider-bodies-mutations-windows.log`、
`acme-provider-bodies-repeat-linux-asan.log`。在隔离副本中分别删除五处终止符，
严格构建均成功且各自目标的运行时断言失败，双平台检出全部五种变体，见
`acme-provider-bodies-mutations-{windows,linux}.log`；生产源码与探针哈希未变。
此项验证序列化和故障清理，不替代提供商 HTTPS 或实际账号互操作。

最终源码的双平台默认合并门禁通过，见
`acme-union-coverage-final-{windows,linux}.log`：Windows 为 3884/5005 行
（77.60%）、2620/4365 分支结果（60.02%），Linux 为 3888/5021 行（77.43%）、
2633/4385 分支结果（60.05%）。两平台九项模拟 CA 无跳过通过，全部独立
HTTP/TLS、DNS、生命周期及 15 项请求体场景通过；每平台三项 provider 的
请求体探针另新增触达 303 行、145 分支结果。模块基线仅约 45%/38%，
模拟 CA 对 flow 另增加 706 行、381 分支结果，说明分开场景统计会遗漏实际
业务触达。当前 flow 仍为 69.38%/48.74%，Ali 为 52.02%/48.11%，AWS 为
Windows 30.14%/19.83%、Linux 30.34%/19.83%；CF 为 85.62%/60.38%，
腾讯云为 86.43%/60.09%，华为云为 88.66%/64.34%。这些缺口须继续补测。

报告分别保留两个平台的源码/692 个编译依赖输入哈希、编译参数、每个
探针的新增触达量与未覆盖位置。仅扩展自有 `.c` 进入指标，内部头中的
static 实现尚待统一计入；Core、测试和范例不计入扩展分母。默认 70% 行/
55% 分支门槛用于防止倒退，不能作为生产验收完成的依据。新增独立双平台
CI 工作避免与原有完整构建串在同一个任务中，执行全部模块、mock 和探针，
上传各平台 `coverage.json`；本地重复键/YAML 结构检查通过，见
`acme-union-workflow-final-structure.log`，托管运行结果仍待验收。

最终生成单头的只读检查、显式单头运行、全部裁剪及静态/共享消费者在
Windows/Linux 通过，见 `acme-provider-delivery-{windows,linux}.log` 和
`acme-provider-single-final-check.log`。共享统计工具最终十类回归又在两平台
通过，见 `acme-coverage-tool-release-{windows,linux}.log`。
最后复核六份 Windows/Linux ACME/JWT/OAuth2 报告的自有源码及全部依赖哈希
均与当前工作树一致，逐文件计数之和及各探针新增触达量之和等于报告总计，
相应门禁通过，见 `acme-auth-union-artifacts-final-hashes.log`；全工作树空白
检查通过，见 `acme-provider-union-whitespace-final.log`。

2026-10-01 Ali/AWS 真实请求批次增加 `tools/test_acme_provider_wire.py` 与实际
provider/HTTP/TLS C 探针，只替换系统根证书加载为本地测试 CA。Python
服务端使用标准库重新计算 ACS3/SigV4 签名，核对正文哈希、日期、查询排序、
Ali nonce 唯一性和 TLS SNI。固定虚构凭据不会访问云账号；这些本地服务
契约场景不替代真实云服务互操作。

红态复现 Ali 含错误 Code 的创建响应仍保存 RecordId，以及删除对任意 404、
空/畸形 JSON、缺失/错误 ID、错误 Code 和含 NUL 的 ID 都错误释放句柄，
见 `acme-provider-wire-red-windows.log`。现创建拒绝任何带 Code 的响应，删除
须成功且返回安全的目标 ID 才清除句柄；确定失败与发送后断流都保留原 ID，
后续成功重试释放，同一已删除句柄再次 Remove 不发请求。通用 404 不足以
证明目标记录消失；已被外部删除的记录的安全对账仍待补齐。

Route53 空/缺少完整 ChangeInfo 的 HTTP 200 也曾被错误认定为变更成功，见
`acme-provider-ack-red-windows.log`。现校验响应结构、Id、PENDING/INSYNC 和
SubmittedAt；不明确的 2xx 应答先限次读取当前状态，确认已提交则不重发，
未提交才重建请求。传输失败只在错误域/码明确为 HTTP 结果未知时对账，
不因确定的本地错误重发写入。相关纯解析正常/异常向量纳入模块套件。

默认 HTTP 独立入口及覆盖率入口均增加 27 项实际 TLS provider 场景：跨区、
同名多值、未登记 Remove 不发删除、创建/删除异常、失去应答后不盲目重放，
以及 AWS 保留外部值/TTL 的 DELETE/CREATE、冲突重读、四次冲突上限、
权限拒绝、提交前断开、提交后断开和畸形应答后的读取对账。Windows 与
Linux ASan/UBSan 首轮全部通过，见 `acme-provider-wire-green-{windows,linux-asan}.log`。
默认脚本随后使用最终源码重新验证并进入完整覆盖率门禁。

最终源码 ASan/UBSan 的 27 项全部通过，隔离副本的六种变体均检出：撤销 Ali
错误 Code 检查、恢复 404 删除成功、忽略删除目标 ID、绕过 AWS 应答校验，
以及分别破坏两家的签名。见 `acme-wire-mutations-final-linux-asan.log` 和
`acme-wire-signature-mutation-recheck-linux-asan.log`；最后一项初次检查脚本
错误地只接受一次签名拒绝，Ali 实际探测父域而得到两次拒绝，调整检查次数
后补验通过。生产源码/探针未修改。最终单头、裁剪及静态/共享消费者已在
双平台通过，见 `acme-wire-delivery-{windows,linux}.log`；共享统计工具十类
测试在两平台重验通过，见 `acme-wire-gcov-helper-{windows,linux}.log`。

最终完整模块、全部独立探针及两平台九项无跳过模拟 CA 门禁通过，见
`acme-wire-union-final-{windows,linux}.log`。当前 Windows 为 4217/5040 行
（83.67%）、2861/4429 分支结果（64.60%），Linux 为 4219/5056 行（83.45%）、
2874/4449 分支结果（64.60%）。新 provider wire 探针分别新增触达 311/309
行、各 211 分支结果。Ali 当前为 85.03%/67.91%，AWS provider 为 Windows
90.30%/68.15%、Linux 90.24%/68.15%；AWS TXT/XML 助手为 92.83%/67.59%。
flow 仍为 69.38%/48.74%，这些指标都不代表生产验收完成。

报告仍以全部 16 个自有 C 文件为分母，增加部分范围的第四份独立探针，
合计 36 份计数输入；两平台各 694 个编译依赖哈希与当前文件匹配。
六份 ACME/JWT/OAuth2 报告的源码/依赖哈希、逐文件总计及新增触达之和全部
重验通过，见 `acme-wire-artifacts-final-hashes.log`。默认回归入口已接入原有
双平台 CI，完整覆盖率 job 的结构检查通过，见
`acme-wire-workflow-structure.log`；单头只读检查确认生成文件一致，见
`acme-wire-single-final-check.log`。托管 CI 及真实服务结果仍待验收。

另外定向复现了尚未修复的 Route53 所有权边界：本次 Add 前已存在
完全相同的 TXT 值时，Add 没有提交任何变更，却登记该值并在 Remove 时
删除原有值，见 `acme-aws-existing-value-windows.log`。此复现使用已生成的
Windows provider 探针，在本批合并读取探针计数之后执行；它未计入本批
27 项受控场景的覆盖率，但该探针保留的 gcda 包含后续复现。下一批优先
修复既有同值的所有权和跨实例清理契约，不能把当前 provider 标为生产完成。
Ali 的 zone 错误分类/发现响应校验、删除不存在记录的安全对账和 OOM 错误
保留，以及 AWS 错误 XML 分类/退避仍待继续压实。

2026-10-01 Route53 所有权与未知结果批次修复上节已复现的误删除：
Add 发现同值已存在时成功返回，但不登记拥有权，后续 Remove 不发删除；
冲突重读后发现其他写者已经添加同值，也不认领。本实例已有已确认值的
重复 Add 保留原拥有权、只占一个槽。八槽满载时拒绝新增且不发请求，
删除释放槽后可复用；容量回归核对 19 次读取和 18 次变更的完整服务状态。

响应丢失或畸形的新增不再通过读取当前值推断拥有权，也不重放写入；
先用实例内固定存储保留未知状态，再返回
`xrt.acme.dns/XACME_DNS_ERROR_UNCERTAIN`。该错误码追加于稳定枚举末尾，
既有数值及公开结构布局不变；同实例后续同名同值 Add/Remove 均不发请求。
需外部核对该次操作及服务端状态，再重建实例。已确认拥有权的 Remove
仍可限次读取对账，并保留集合中的外部值和原 TTL。

HTTP 私有未知标记独立于错误对象分配，且每次请求入口重置。探针分别在
HTTP 与 DNS 未知错误包装处注入真实分配失败，核对两次失败、原始内存
诊断、保留未知条目以及无额外请求；另一场景核对未知状态与其他值隔离。
flow 的 Add 重试和上层授权遍历同时停止 HTTP 未知、DNS 未知与内存错误，
保留原始诊断并清理此前已成功添加的 TXT。两项 mock 方法各运行三种错误
子场景，共六种首次/后续 Add 组合；初版上层只识别 HTTP 未知的问题被
四种子场景检出后修复，见 `acme-aws-ownership-flow-target-windows.log` 和
最终通过记录 `acme-aws-ownership-flow-target-final-windows.log`。

两个 provider 实例的同值借用/清理场景验证借用者不删原值，并在拥有者
清理前完成借用者生命周期。根据官方 [ResourceRecord 字段模型](https://docs.aws.amazon.com/Route53/latest/APIReference/API_ResourceRecord.html)，
同值读取无法证明独立签发的逐值所有权；此场景不证明自动协调独立签发。
宿主仍须对相同 zone/name/value 协调完整 Add→验证→Remove 生命周期，
包括其他进程及外部 DNS 写者。
公开 API 与 README 已明确此约束及未知结果的恢复要求。

默认实际 TLS provider 场景由 27 项扩充为 35 项（Ali 14/AWS 21）。
独立服务端验证请求签名、请求次数、最终多值集合与 TTL；使用本地 CA 和
固定虚构凭据，未执行真实云账号互操作。Linux ASan/UBSan 全部通过，
隔离副本的六种变体全部检出：认领既有值、认领未知新增、丢失未知条目、
丢失原拥有权、丢失原始未知标记和未重置请求状态；源码/探针哈希未变。
见 `acme-aws-ownership-mutations-release-linux-asan.log`。其中丢失原拥有权
由独立服务端的残留值检查检出，不能只凭客户端返回码认定场景成功。

最终双平台完整模块、全部独立探针和九项无跳过 mock CA 门禁通过，见
`acme-aws-ownership-union-release-{windows,linux}.log`。
Windows 为 4240/5059 行（83.81%）、2903/4463 分支结果（65.05%）；
Linux 为 4240/5075 行（83.55%）、2915/4483 分支结果（65.02%）。
AWS provider 分别为 Windows 91.32%/73.33%、
Linux 91.26%/73.33%；flow 分别为 Windows
69.48%/49.09%、Linux 69.48%/49.09%。
分母仍为全部 16 个自有 C 文件，36 份计数输入按语义位置合并，每个平台
694 个编译依赖哈希匹配，70%/55% 防倒退门禁通过。

双平台最终单头、裁剪及静态/共享消费者通过，见
`acme-aws-ownership-delivery-{windows,linux}.log`。共享统计工具十类测试
两平台通过，CI 结构检查通过，见 `acme-aws-ownership-gcov-{windows,linux}.log`
和 `acme-aws-ownership-workflow-structure.log`；公共符号参考已重新生成，新增
错误码进入索引且只读检查通过，见 `acme-aws-ownership-api-reference-check.log`。
生成单头只读检查及全工作树空白检查通过，见
`acme-aws-ownership-single-final-check.log` 和
`acme-aws-ownership-whitespace-final.log`。六份 ACME/JWT/OAuth2 报告的
源码/依赖哈希、逐文件总计及新增触达之和重验通过，见
`acme-aws-ownership-artifacts-final-hashes.log`；最终场景/交付证据核对见
`acme-aws-ownership-evidence-final.log`。内部 static 头实现尚未纳入自有 C
统计，真实云服务、真实宿主退出/卸载、部署文件系统故障和托管 CI 均仍待
验收，长期任务保持进行中。

等待最终统计时的静态复核确认 Ali 创建缺少可用 RecordId 时仍设置普通
协议错误，并覆盖解析失败的原始内存错误；flow 对该协议错误仍允许重试。
本批六种 flow 子场景使用受控 provider 注入，尚未验证此 Ali 响应路径。
下一批须以实际 Ali 响应接入 flow 重放回归，并压实错误保留和未知状态。

2026-10-01 Ali 创建未知状态、OOM 与 5xx 批次先以真实 Ali 响应接入实际
`xacmeFlowDnsAdd`：创建已提交但响应畸形或解析 OOM 时，旧实现各发出
三次 Add，见 `acme-ali-create-red-windows.log`。另以创建提交后返回 503
复现三次创建，见 `acme-ali-create-5xx-pending-linux.log`；这些红测记录
不代表修复后的状态。

现在在发出创建前校验输入及八槽容量，未知与已确认条目共享容量；发出后
使用无分配固定存储保存未知状态。丢失响应、异常或缺失安全 RecordId、
错误信封，以及创建 5xx 均不会被 flow 重放，同名同值的后续 Add/Remove
也不发请求。保留原始解析或诊断包装 MEMORY，HTTP 写入未知标记独立于
错误对象分配。其他值可正常添加/删除，释放已确认槽可复用，未知槽不可复用。
Ali 的公开未知结果改为 `XACME_DNS_ERROR_UNCERTAIN`，内存错误仍为 MEMORY；
旧调用方如按 HTTP 未知码分支，需按更新后的公开说明处理。公开结构和
错误枚举数值未改变。Ali 实例内 Add/Remove 增加互斥锁，模块与公开特性
依赖显式加入 mutex；Unit 前仍须由宿主停止新调用并等待在途操作。

AWS 创建的 5xx 同样保留未知状态，不能用服务错误推断写入未执行；已有
明确所有权的删除仍可读取对账。默认 provider-wire 共 57 项（Ali 32/AWS 25），
双平台核对独立 ACS3/SigV4 签名、实际服务状态与请求次数。真实 flow 故障
场景同时检查 Add 回调和创建 RPC 各一次，覆盖创建前/后 503、500/599、
响应丢失、RecordId 各种异常、实际 JSON 解析/错误包装分配失败、八槽满载
与复用、八线程并发以及非法输入零请求。Ali 删除响应解析 OOM 保留诊断和
已确认 RecordId，但删除已生效后安全退休该句柄的对账仍未完成。

Linux ASan/UBSan 的 57 项通过；八种隔离变体均检出：忘记未知条目、复用
未知槽、覆盖解析内存错误、忽略原始写入未知标记、移除串行化、重放内存
失败 Add、重放 Ali 服务错误及拒绝 AWS 服务错误却丢失未知状态。见
`acme-ali-create-mutations-final-linux-asan.log` 和
`acme-ali-aws-5xx-mutations-final-linux-asan.log`；生产源及探针哈希未变。
第一次变体脚本因移除锁后留下未使用变量编译失败，仅修复隔离脚本后重跑；
未把编译失败当作检出，也未修改生产代码绕过检查。

初次最终覆盖收集的 provider 探针范围清单只有五个 C 文件，但实际流程
探针嵌入十一份实现，严格检查因此拒绝报告。收集工具现从探针实际 C include
推导独立范围（HTTP 1、拥有权 15、请求体 4、provider 11），继续核对完整
自有范围、源哈希和控制流；未放宽 gcov 检查。失败测量不作为最终报告，
最终冻结输入重新完整测量。双平台完整模块、所有独立探针和九项无跳过
mock CA 通过，见 `acme-ali-create-union-release-{windows,linux}.log`。
Windows 为 4287/5107 行（83.94%）、2919/4473 分支结果（65.26%）；
Linux 为 4289/5123 行（83.72%）、2932/4493 分支结果（65.26%）。
Ali 为 Windows 86.87%/69.79%、
Linux 86.87%/69.79%；AWS 为 Windows
91.40%/74.29%、Linux 91.35%/74.29%。
分母仍是全部 16 个自有 C 文件、36 份计数输入和 694 个编译依赖哈希；
默认 70% 行/55% 分支结果是防倒退门槛，内部 static 头实现仍未计入。

双平台单头、裁剪及静态/共享消费者通过，见
`acme-ali-create-delivery-final-{windows,linux}.log`。特性、生成单头与公共
符号参考的只读一致性检查通过，见 `acme-ali-create-features-check.log`、
`acme-ali-create-single-check-final.log` 和
`acme-ali-create-api-reference-check-final.log`；六份 ACME/JWT/OAuth2 报告
的源码/编译依赖哈希、逐文件总计及新增触达之和核对通过，见
`acme-ali-create-artifacts-final-hashes.log`。场景、交付与故障变体证据核对
见 `acme-ali-create-evidence-final.log`，工作树空白检查见
`acme-ali-create-whitespace-final.log`。

等待最终统计时，以全新无覆盖率客户端复现 Ali zone 的四类未修复问题：
子 zone 的 403/Forbidden 仍回退到父 zone；父 zone 的畸形 JSON、Code 错误
信封与错误字段结构只因 HTTP 2xx 就被接受。每类均确认实际发出两次
Describe、一次 Add 和一次 Delete，见 `acme-ali-zone-pending-windows.log`；
该日志的 passed 表示复现了错误行为，不是生产验收成功。这些场景未纳入
本批 57 项统计，未执行最终覆盖率探针，生产源及探针哈希未变。下一批
优先修复并将四类红测改为失败关闭回归，再继续 Ali 删除对账。
官方 [删除接口](https://www.alibabacloud.com/help/en/dns/api-alidns-2015-01-09-deletedomainrecord)
的成功响应带 RecordId，[公共错误表](https://www.alibabacloud.com/help/en/dns/api-alidns-2015-01-09-errorcodes)
将 DomainRecordNotBelongToUser 定义为当前账号中无该记录；不能把普通
HTTP 404 或权限错误推断为已删除。需进一步核对准确错误及读取响应后
建立回归。本批使用本地 CA 和固定虚构凭据，真实云账号、真实宿主卸载、
部署文件系统故障及托管 CI 仍待验收，长期任务保持进行中。

2026-10-01 Ali 区域发现响应与缓存批次先用四项已编译的红测确认旧实现
在子区域权限拒绝及父区域畸形 JSON、错误信封、错误字段结构时仍发出一次
Add，见 `acme-ali-zone-red-windows.log`；这些失败不计入修复后的验收。

发现结果现明确区分找到、准确缺失和错误。仅 HTTP 400/404 的精确
`InvalidDomainName.NoExist` 或 `DomainNotFound` 继续向父区域探测；权限、
限流、5xx、普通 404、成功状态中的错误信封均停止，不缓存区域或发送创建。
成功页显式请求 PageNumber/PageSize 均为 1，校验非负整数 TotalCount、
非空安全 RequestId、DomainRecords.Record 数量，以及记录 DomainName 与
请求区域一致、RecordId 可用。重复 JSON 字段及 NUL 前缀不被接受。
依据官方 [发现接口](https://www.alibabacloud.com/help/en/dns/api-alidns-2015-01-09-describedomainrecords)
和 [错误表](https://www.alibabacloud.com/help/en/dns/api-alidns-2015-01-09-errorcodes)
建立响应向量；完整页可以确认区域存在，不把列表中的既有记录据为己有。
解析失败保留 MEMORY，逐分配扫描当前向量的 26 个位置，每处核对活动
分配、字节数与非法/重复释放计数不变；真实 TLS 解析 OOM 后同一实例恢复。

四项缓存以完整且按 ASCII 大小写规范化的探测起点匹配，不用父区域后缀
匹配遮蔽更具体区域；填满后轮换替换，并直接返回本次已验证的区域。
六区域及重复使用测试核对 14 次发现、8 次创建/删除，缓存始终最多四项。
长期缓存不自动感知云端区域变更，需要更新区域布局时重建 provider。
RR 缓冲扩为 256 字节，219 字节深层属主的 207 字节 RR 不再被原 200 字节
容量拒绝；公开结构与函数签名保持不变。缓存键回归不能替代记录拥有权
匹配的大小写语义验收；下列独立红测说明该保护尚有待修复的问题。

默认 provider-wire 从 57 扩为 89 项（Ali 64/AWS 25）：25 种发现失败响应
及七项发现/缓存场景。错误场景要求失败前零创建、零拥有权、零缓存，
随后同一实例成功添加/删除；独立服务端继续核对实际 ACS3/SigV4 签名和
最终状态。Windows 首次全量仅新增缓存容量场景触及旧 60 秒时限，夹具为
该多请求场景采用与既有容量/并发场景相同的 180 秒上限；随后定向与最终
完整测量通过，没有把时限失败当作生产源码缺陷。Linux ASan/UBSan 的
89 项通过，见 `acme-ali-zone-green-linux-asan-recovered.log`；Windows 严格
构建的全新无覆盖率客户端 89 项也全部通过，见
`acme-ali-zone-green-final-windows.log`。双平台各六种
隔离变体均被检出：仅按状态接受、错误继续父探测、覆盖原始 MEMORY、
借用父区域缓存、满四项即失败、RR 恢复 200 字节。见
`acme-ali-zone-mutations-windows.log` 和
`acme-ali-zone-mutations-linux-asan-recovered.log`；均实际编译并保持生产源
及测试哈希不变。初次测试夹具的声明位置和错误常量编译问题先修正，未将
编译失败当作变体检出。

本批曾因 C 盘仅约 13 MiB 导致 Ubuntu 文件系统应急只读和 OpenSSL EIO，
两份最初 Linux 日志在生成证书前失败，不能作为库失败或成功证据。外部
释放空间后文件系统恢复读写，重新运行 Linux 验证；未迁移或停止 Ubuntu，
其中其他项目进程保持运行。TLS/变体临时产物放 D 盘；最终 Linux 交付与
覆盖测试的私钥目录使用原生 POSIX `/dev/shm`，先核对 0600、文件/目录
fsync 与清理，避开仍波动的 C 盘空间。tmpfs 上的逻辑和故障注入通过不
替代实际部署文件系统的持久化、进程异常终止和断电演练。

最终覆盖收集期间另以全新无覆盖率隔离客户端，双平台复现 Ali 未知记录
的域名大小写绕过：初次创建已提交但应答畸形，随后把 API.EXAMPLE.COM
改成大写后，Remove 错误地返回成功，Add 又实际发出第二次创建。服务器
核对两次创建对应同一 DNS 属主和值，随后只删第二个已确认 ID，首个未知
ID 保留。见 `acme-ali-case-alias-pending-windows.log` 与
`acme-ali-case-alias-pending-linux-asan.log`。证明脚本要求出现此错误行为才
返回成功；这两份日志不是生产验收，不计入本批 89 项或覆盖率。生产源及
探针哈希保持不变，优先在下一批规范化记录身份并补齐大小写混用回归。

最终冻结源码、公共/内部头、测试和收集工具后重跑完整模块、所有独立探针
与九项无跳过 mock CA，见 `acme-ali-zone-union-release-{windows,linux}.log`。
Windows 为 4360/5179 行（84.19%）、3003/4569 分支结果（65.73%）；
Linux 为 4362/5195 行（83.97%）、3016/4589 分支结果（65.72%）。
Ali 逐文件为 Windows 89.00%/73.40%、Linux 89.00%/73.40%。
分母仍是全部 16 个自有 C 文件、36 份计数输入和 694 个编译依赖哈希；
70% 行/55% 分支结果是防倒退门槛，内部 static 头实现仍未计入。

双平台单头、裁剪和静态/共享消费者通过，见
`acme-ali-zone-delivery-final-{windows,linux}.log`。特性、生成单头和公共
符号参考的只读一致性通过，见 `acme-ali-zone-features-check.log`、
`acme-ali-zone-single-check-final.log` 和
`acme-ali-zone-api-reference-check-final.log`。十项 gcov 收集回归通过，见
`acme-ali-zone-gcov-collector-test.log` 和
`acme-ali-zone-gcov-collector-test-linux.log`。六份 ACME/JWT/OAuth2 报告的源码/
编译依赖哈希、逐文件总计及新增触达之和通过，见
`acme-ali-zone-artifacts-final-hashes.log`。精确场景集合、mock、OOM 与
故障变体证据核对见 `acme-ali-zone-evidence-final.log`，工作树空白检查见
`acme-ali-zone-whitespace-final.log`。本地 CA 与虚构凭据的验收不等同于
真实 CA/云账号、真实宿主卸载和托管 CI 验收，长期任务保持进行中。

末次哈希核对发现旧的四份 JWT/OAuth2 报告仅 Core 单头输入哈希与当前
文件不同，不能沿用为当前运行时证据；Core 单头的只读生成一致性检查
通过后，在双平台冻结输入重新完整测量认证主套件及 OAuth2 全部独立
HTTP/TLS/拥有权组，见 `acme-ali-zone-auth-refresh-{windows,linux}.log`。
当前 JWT 为 Windows 93.30%/74.07%、Linux 93.30%/74.07%；OAuth2 为 Windows 92.25%/77.30%、Linux 92.25%/77.20%。
Core 单头只读检查见 `acme-ali-zone-core-single-check.log`，原四份报告未
手动改写哈希来冒充重测。本批认证源码未修改，实测新增触达的微小变化
不能据此推断认证实现变更。

## 2026-10-01：Ali 记录身份规范化与安全删除对账

本批在原 89 项提供商回归上新增 48 项 Ali 场景，当前为 Ali 112 项、AWS
25 项，共 137 项。六项新域名大小写回归先在旧实现中全部失败，见
`acme-ali-alias-red-windows.log`：大小写混用能绕过未知状态保护、重复创建，
也能使 Remove 返回成功却没有删除已保存 ID 的记录；满载和并发重复 Add
同样没有正确复用记录。修复按 [RFC 4343](https://www.rfc-editor.org/rfc/rfc4343.html)
的 ASCII 域名比较规则，在 Add/Remove 校验后统一登记与查找，TXT 摘要仍
逐字节区分大小写。未知记录的域名大小写混用继续失败且不发请求，另一条
仅摘要大小写不同的记录可以独立添加和清理。

已确认记录的重复 Add 与实际 Remove 先按本实例保存的 RecordId 调用
[DescribeDomainRecordInfo](https://www.alibabacloud.com/help/en/dns/api-alidns-2015-01-09-describedomainrecordinfo)，
核对 ID、属主、TXT 值、TXT 类型、默认线路及启用状态。重复 Add 仅在记录
启用时复用；满八槽仍可核对已有记录，八个同名同值并发 Add 只创建一次。
已确认 ID 明确消失时可重新创建，但不认领同名同值的其他 ID。记录被外部
持续修改时保留句柄、失败关闭且不删除；记录禁用时 Add 失败，Remove 仍
能清理本实例记录。查询畸形、字段不匹配、权限、服务错误和真实解析 OOM
都保留已确认句柄，不把缓存状态直接作为成功依据。

Remove 在已确认 ID 消失时直接清除本地句柄；删除已提交但应答断流、畸形、
5xx 或解析 OOM 时，后续读取确认消失，不再重复发送删除。读取和删除之间
的并发删除也可安全对账。本实现仅把 400/404 配合精确
`DomainRecordNotBelongToUser`、有效非空 RequestId 且无 RecordId 字段判为
消失；普通 404、403/5xx 携带该码、成功状态携带错误码、缺失请求标识、NUL
和矛盾字段均失败。错误类别保留权限/网络/内存首因。读取和删除是两个
请求，接口没有条件删除，宿主仍须协调其间的外部修改，不能视为事务。

公开配置结构及函数签名未改变，但 RAM 策略须新增
`alidns:DescribeDomainRecordInfo`，配合原有 DescribeDomainRecords、
AddDomainRecord、DeleteDomainRecord。每次实际删除和重复 Add 增加按 ID
读取；README、公共头、生成单头及 API 符号参考已同步这一行为契约。

Windows 严格构建和 Linux ASan/UBSan 的八种隔离变体均检出：移除 Add 或
Remove 的域名规范化、直接接受缓存、忽略 TXT 修改、跳过删除前核对、
接受普通 404、接受禁用记录的重复 Add、拒绝明确消失的清理。最终变体
全部成功编译后由业务回归拒绝，编译错误不计为检出；见
`acme-ali-ownership-mutations-final-{windows,linux-asan}.log`。
完整记录与缺失记录响应的逐分配点扫描分别为 19 和 7 个位置，原始内存
诊断、失败输出和资源释放均核对；区域发现原有 26 个位置仍通过。最终
变体运行期间源码、测试、Core 单头和编译头依赖哈希不变。

最终冻结输入后的完整模块、全部独立 HTTP/TLS/拥有权探针与九项无跳过
mock CA 双平台通过，见 `acme-ali-ownership-union-{windows,linux}.log`。
Windows 为 4446/5267 行（84.41%）、3097/4681 分支结果（66.16%）；
Linux 为 4446/5283 行（84.16%）、3109/4701 分支结果（66.13%）。
Ali 逐文件为 Windows 521/579 行（89.98%）、438/582 分支结果（75.26%）、Linux 521/579 行（89.98%）、438/582 分支结果（75.26%）。
默认防倒退门槛仍为 70% 行/55% 分支结果，统计范围仍是全部 16 个自有 C
文件、36 份计数输入和 694 个编译依赖哈希；内部 static 头实现尚未计入。
最新 Linux ASan/UBSan 的完整 137 项通过，见
`acme-ali-ownership-linux-asan-final.log`。早期 sanitizer 运行中的容量
复用预期顺序已修正，最终重跑全部场景，没有把失败的早期日志当作验收。

双平台单头、裁剪、静态及共享消费者通过，见
`acme-ali-ownership-delivery-{windows,linux}.log`。十项 gcov 收集回归
双平台通过，生成 Core/ACME 单头与 API 参考的只读检查通过。六份
ACME/JWT/OAuth2 报告的当前源码/编译依赖哈希、逐文件总计与新增触达之和
核对通过，见 `acme-ali-ownership-artifacts-final-hashes.log`；精确场景集合、
mock、OOM、八种故障变体及交付证据见 `acme-ali-ownership-evidence-final.log`。
本轮 JWT/OAuth2 源码与其编译输入未变化，四份既有认证报告哈希仍有效，
没有修改其哈希或重写报告来冒充重测。工作树空白检查见
`acme-ali-ownership-whitespace-final.log`。

冻结期间另用全新无覆盖率隔离探针，双平台复现下一批的三项缺口。Ali
按 ID 只读请求应答丢失会因 POST 传输标记被视作写入未知结果，flow 在
一次 Add 回调后停止；已确认句柄完整，手动重试与按 ID 删除仍成功，见
`acme-ali-record-read-retry-pending-{windows,linux-asan}.log`。AWS 的属主
登记/查找仍按字节区分大小写，Remove 改域名大小写会错误返回成功且保留
句柄，未知记录也可经大写 flow Add 绕过身份查找并返回成功；见
`acme-aws-case-alias-pending-{windows,linux-asan}.log`。
AWS 独立服务端先核对原始签名，再按
[Route 53 的域名小写存储规则](https://docs.aws.amazon.com/Route53/latest/DeveloperGuide/DomainNameFormat.html)
处理名称，TXT 字节保持不变。这些证明要求错误行为出现才返回成功，
不属于 137 项验收或覆盖率；生产源与正式探针哈希不变。

Ali 另有跨创建响应的 ID 碰撞缺口：受控服务端实际创建 ID 12 的第二条 TXT，
却在有效 JSON 中返回已保存的 ID 11。现实现登记两个相同 ID，删除第二条
TXT 时按 ID 找到第一槽，核对并删除了第一条记录，真实第二条记录仍在云端
但不再被追踪。双平台见 `acme-ali-duplicate-id-pending-{windows,linux-asan}.log`。
这证明客户端未拒绝矛盾创建响应，不代表真实 Ali 接口发生过该异常；证明
要求错误行为出现才成功，不属于本批验收或覆盖率，输入哈希保持不变。
下一批先拒绝本实例已保存 ID 的跨响应碰撞，并保证清理核对使用请求的
记录身份，再修复 AWS 大小写匹配和 Ali 只读请求的失败/重试语义。

本地受控 TLS、虚构凭据与 tmpfs 私钥逻辑验证，不替代真实 CA/云账号、
部署文件系统持久化、真实宿主退出/卸载及托管 CI 验收；长期任务保持进行中。

## 2026-10-01：DNS 记录 ID 碰撞、只读恢复与 AWS 身份匹配

本批修复上一批隔离证明的三项缺口，并补充签名前 HTTP 状态复位。公开
配置结构及函数签名保持兼容，Ali 的四项 RAM 动作要求仍见 README。

Ali 创建响应中的 RecordId 如果与实例已登记 ID 碰撞，即使 JSON 有效，也
不能建立新记录归属。新请求的登记槽保留未知状态，原记录句柄和 TXT 不变。
已提交与未提交的服务端变体、最后一槽、错误包装 OOM、未知状态阻止同名
同值重放和已确认槽复用均验证。Remove 按调用者请求匹配的槽核对完整记录，
不能按 ID 找到另一槽后删除另一条 TXT；模拟旧重复 ID 登记时拒绝错误清理。

Ali 的 DescribeDomainRecords/DescribeDomainRecordInfo 是只读 RPC。请求在
传输层使用 POST 时，断流造成的 HTTP 写入未知包装恢复为原始传输错误，
清除本请求的写入标记；两个动作允许至多三次 RPC，每次新 nonce 和签名。
内存失败立即停止，创建和删除未被该规则自动重放。六项实际 TLS 回归覆盖
重复 Add/Remove 的单次断流、内部三次耗尽、流程恢复及流程上限、真实错误
包装 OOM；核对回调、实际 API 次数和句柄。流程上限样本总计九次失败读取，
后续手动清理的第十次读取成功，不能以错误清除掩盖重试次数。

每次 RPC 在 nonce/签名之前清除上一请求的 HTTP 未知标记。先前未知写入仍
由登记槽保存；受控 nonce 生成故障返回内存诊断时，不会把尚未发送的新记录
登记为未知。该项使用生成器故障注入，并非宣称随机数实现发生真实分配失败。
定向红测试先复现旧状态污染，修复后原未知记录继续受保护，新记录可正常添加清理。

AWS 在登记查找、zone 探测与签名请求之前按 ASCII 小写处理 DNS 属主；
TXT 摘要仍逐字节比较。七项回归核对已有值不认领、已确认值重复 Add、
大小写等价 Remove、未知状态保护、仅 TXT 大小写不同的独立值、满八槽
重复 Add 及八线程同名同值添加。独立服务端先核对原始 SigV4，再处理 DNS
名称语义；保留其他 TXT、TTL 与原有独立 ACS3/SigV4 检验。

正式 wire 集合现为 156 项（Ali 124/AWS 32）。最终冻结输入后的双平台
完整模块、全部独立 HTTP/TLS/拥有权探针及九项无跳过 mock CA 通过，日志为
`acme-id-read-alias-snapshot-union-{windows,linux}.log`；Linux ASan/UBSan 的
完整 156 项另行通过，见 `acme-id-read-alias-snapshot-wire-linux-asan.log`。
早期借用记录预期把未调用写接口写成显式零计数，已修正这一测试表示问题；
修正前运行不计入验收。Windows 新增红测试先拒绝旧业务行为，故障变体
独立编译成功后由回归检出，不把编译失败或 sanitizer 崩溃计为检出。

九种隔离变体双平台检出：允许重复 ID、按 ID 选择第一槽、排除记录只读
重试、保留只读未知错误、保留只读写入标记、保留上次 RPC 标记、移除 AWS
Add/Remove 属主规范化及错误折叠 TXT 摘要。见
`acme-id-read-alias-snapshot-mutations-final-{windows,linux-asan}.log`。运行期间源码、
编译头、生成单头和正式探针哈希不变；区域与完整/缺失记录解析逐分配点
扫描仍覆盖 26、19、7 个位置，原始内存错误和资源释放核对通过。

最新合并覆盖率：Windows 4478/5291 行（84.63%）、3138/4707 分支结果（66.67%）；
Linux 4480/5307 行（84.42%）、3151/4727 分支结果（66.66%）。
Ali 逐文件：Windows 行 91.12% / 分支结果 77.74%，Linux 行 91.12% / 分支结果 77.74%；
AWS 逐文件：Windows 行 91.56% / 分支结果 74.83%，Linux 行 91.51% / 分支结果 74.83%。
防倒退门槛仍是 70% 行/55% 分支结果，统计全部 16 个自有 C 文件、36 份
计数输入和 694 个依赖哈希，内部 static 头实现仍未计入。

本机 Windows 完整覆盖率收集已接近原 60 分钟 CI 上限，作业还需安装工具。
将 ACME coverage 作业窗口调整为 90 分钟，保持全场景与原通过门槛，不缩减
回归；两个矩阵平台仍使用相同入口。托管 CI 的实际执行结果仍待验收。

双平台单头、裁剪、静态及共享消费者通过，见
`acme-id-read-alias-snapshot-delivery-final-{windows,linux}.log`；十项 gcov 收集回归
双平台通过，Core/ACME 单头与 API 参考只读检查通过。六份 ACME/JWT/OAuth2
当前源码/依赖哈希、逐文件总计、各输入新增触达之和核对通过，见
`acme-id-read-alias-artifacts-final-hashes.log`；完整场景集合、mock、OOM、
九种变体、交付及初始红测试证据见 `acme-id-read-alias-evidence-final.log`。
最初的 Windows 全部业务测试虽通过，收集器最终检测到并行工作改变了
Future 头、实现及 Core 单头，拒绝生成报告。此前 Linux 与四份认证报告
也不再对应当前依赖；没有改写报告哈希来冒充重测。随后建立 6998 文件
隔离快照，逐文件核对复制前后哈希，并同步 ACME 内嵌 Core 的两个生成
单头。最终全量回归、变体与交付均在该快照运行；输出的六份报告与工作区
当前编译输入逐项匹配后原样复制。快照及公布检查分别见
`acme-id-read-alias-source-freeze.log` 与 `acme-id-read-alias-snapshot-publish.log`。

JWT/OAuth2 两个平台全部重新收集，日志为
`acme-id-read-alias-snapshot-auth-{windows,linux}.log`。JWT：Windows
行 93.30%/分支结果 74.07%，Linux 行 93.30%/分支结果 74.07%；OAuth2：
Windows 行 92.41%/分支结果 77.39%，Linux 行 92.25%/分支结果 77.20%。
Future 的合并、分配失败、所有权、投影观察及阶段回归在双平台分别通过
五个模块化和四个单头测试，见 `acme-id-read-alias-snapshot-core-{combine,single}-final-{windows,linux}.log`。
初始附加命令的选择器及共享输出目录错误已修正，失败运行不计作验收。
空白检查见 `acme-id-read-alias-whitespace-final.log`。

冻结期间使用全新无覆盖率隔离探针，双平台另复现 AWS 的父区域缓存缺口：
先添加 first.example.com 下的记录缓存 example.com 后，api.example.com
托管子区域存在，但后续添加跳过子区域查询，直接写入缓存父区域。实际
TLS 签名、三个区域查询、四次记录读取与四次变更及清理后的 TXT/TTL 均
独立核对；见 `acme-aws-cached-parent-pending-{windows,linux-asan}.log`。
该证明要求错误行为出现才成功，不属于 156 项验收或覆盖率，生产输入
哈希不变，下一批优先修复。CF/腾讯云/华为云的共享属主匹配和区域缓存
已完成源码审计，仍需各家实际请求及故障回归，不能据源码检查宣布验收。

本地受控 TLS、虚构凭据与 tmpfs 私钥逻辑验证，不替代真实 CA/云账号、
部署文件系统持久化、真实宿主退出/卸载及托管 CI；七库长期任务继续进行。

## 2026-10-01：AWS 区域发现、原区域清理与错误诊断

AWS 新记录从完整 TXT 属主开始逐级查找唯一公有托管区，移除后缀区域
缓存及多余的候选复制；父区域曾被发现不能证明子区域不存在。支持 TXT
属主本身作为托管区。查询失败、歧义或同名分页未读完时停止，不写入父区。
既有拥有值的重复 Add 和 Remove 固定使用登记时的区域 ID；新属主/值及
再次添加的借用值重新探测。不同区域中同名 TXT 的拥有权和清理句柄互不转移。
所有候选查找均成功却没有可用区域时，交付本次 DNS 区域错误，覆盖调用方
先前的无关错误。公开函数签名不变；README 说明真实 NS 委派由宿主配置。

正式 TLS 集合扩为 170 项（Ali 124 / AWS 46），双平台完整覆盖率运行及
Linux ASan/UBSan 全部通过。14 项新增区域场景覆盖父/子区、属主区域、
新出现的子区、原区重复与外部删除后的重建、借用值重新探测、权限拒绝、
畸形响应、歧义、分页、私有区、区域 ID 替换、没有区域与旧错误残留。
独立服务端分别保存各区域的记录，核对签名、完整查询顺序、每次读取与
变更的区域 ID，以及最终保留的原有 TXT 和 TTL。其余 AWS 场景也逐项
核对新记录的完整探测顺序与已拥有记录不重新选区。

七种隔离变体在 Windows 严格构建及 Linux ASan/UBSan 下独立编译并检出：
跳过属主区域、复用父区句柄、移动已拥有值、把查询错误当缺失、清理重新
选区、接受私有区、保留旧错误。编译失败与 sanitizer 崩溃不计作检出，
运行期间生产输入哈希不变。见 `acme-aws-zone-mutations-dual-{windows,linux-asan}.log`。
最初 13 项 Windows 红测试分别由业务断言或完整查询序列拒绝旧实现；
旧错误缺陷另外在首次冻结源码上以实际四次 TLS 查询复现，要求错误 777
残留才通过，属于缺陷证明，不计入正式验收。见
`acme-aws-zone-red-windows.log`、`acme-aws-zone-stale-error-red-linux.log`。

合并覆盖率：Windows 4481/5288 行（84.74%）、3146/4707 分支结果（66.84%）；
Linux 4481/5304 行（84.48%）、3158/4727 分支结果（66.81%）。
统计范围仍为全部 16 个自有 C 文件、36 份计数输入和 694 个依赖哈希；
70% 行 / 55% 分支结果门槛不变，内部 static 头实现仍待纳入。
覆盖率收集日志为 `acme-aws-zone-union-dual-{windows,linux}.log`，
完整 sanitizer 协议回归见 `acme-aws-zone-wire-dual-linux-asan.log`。

双平台单头、裁剪、静态及共享消费者通过，见
`acme-aws-zone-delivery-final-{windows,linux}.log`；生成单头与 API 参考只读
检查通过。交付验证与最终快照的编译输入一致，两个快照的差异仅是 README
和 provider Python 监听逻辑；逐文件核对通过，无需重复相同编译验证。
最终建立 6998 文件隔离快照，复制前后逐文件核对，编译输入
在整个收集期间不变。最初覆盖率和 Linux 交付运行因补充诊断修复而主动
停止；随后 provider 服务改为 IPv4/IPv6 双回环监听，原覆盖率运行也先确认
终止再重收，场景和门槛未缩减。Windows 独立探针同样七次 TLS RPC 在
IPv4 监听两次分别为 14.735、14.453 秒，IPv6 两次均为 0.156 秒，见
`acme-aws-loopback-family-timing-windows.log`。不把已终止运行算作最终验收，
没有改写报告哈希。最终两份 ACME 报告与当前
工作区输入逐项匹配后原样公布，四份 JWT/OAuth2 报告的输入仍匹配当前
源码。冻结、公布和完整场景/变体/交付检查分别见
`acme-aws-zone-source-freeze-dual.log`、`acme-aws-zone-publish-final.log`、
`acme-aws-zone-evidence-final.log`、`acme-aws-zone-artifacts-final.log`。

AWS 错误 XML 分类与退避、其余 provider 的真实请求/身份/缓存、内部头
覆盖率、部署文件系统故障和真实服务/托管 CI 仍待验收，七库长期任务继续。

## 2026-10-01：AWS 写入错误 XML 与有界退避

旧实现对 HTTP 400 正文全文搜索 `InvalidChangeBatch`。独立 TLS 服务复现
消息正文和错误码前缀引起四次变更请求，截断错误 XML 经 flow 放大为十二次写请求；
明确 `Throttling` 拒绝却不能恢复。红测试见 `acme-aws-error-red-windows.log`，
属于缺陷证明，不计入通过的验收运行。

新增零分配、4 MiB 有界的 REST-XML 错误解析，仅完整且唯一的
`ErrorResponse/Error/Code` 参与分类。支持标准默认命名空间或一致绑定的
根前缀、UTF-8、注释、实体、CDATA 和可选 Messages/RequestId。DTD、非法
字符/UTF-8、重复字段、错配命名空间、截断、尾随数据和不支持的信封均拒绝。
未知写入结果保留同名同值保护，flow 只调用一次 Add，后续 Add/Remove 不发
请求；合法拒绝的错误消息、注释或 Code 前缀不能冒充冲突。

仅完整错误信封中的 HTTP 400 `InvalidChangeBatch`、`Throttling` 和
`PriorRequestNotComplete` 进入最多四次尝试，500ms/1s/2s 退避后重新读取
TXT 并重建原子变更，符合
[Route53 对前次请求未完成的建议](https://docs.aws.amazon.com/Route53/latest/APIReference/API_ChangeResourceRecordSets.html)。
服务端在拒绝期间添加外部 TXT；每次后续 DELETE 必须精确匹配当前集合，
原 TTL 和外部值均保留。Add 拒绝耗尽不认领，Remove 拒绝耗尽保留拥有条目；
末次繁忙交付 AGAIN/NETWORK，权限错误交付 PERMISSION/CREDENTIAL，
成功恢复清除旧错误。已拥有值的删除遇到畸形 4xx，以再次读取核对已提交和
未提交两种结果；新值的 Add 不据此认领。公开接口和配置布局不变。

正式 wire 集合扩为 201 项（Ali 124 / AWS 77），新增 31 项写入错误与重试
场景。双平台完整模块/独立探针/九项 mock CA 的合并覆盖率及 Linux
ASan/UBSan 全部通过。错误解析有效/无效向量及每个截断前缀另以分配失败
注入验证零分配，Windows 与 Linux ASan/UBSan 通过。8 种隔离变体全部独立
编译并检出：恢复全文搜索、忽略重复 Code、忽略尾随数据、丢弃未知保护、
禁用繁忙恢复、移除退避、扩大尝试上限、覆盖繁忙诊断。编译失败、服务端
异常和 sanitizer 崩溃不计作检出；生产输入哈希不变。

合并覆盖率：Windows 4652/5461 行（85.19%）、3443/5055 分支结果（68.11%）；
Linux 4654/5477 行（84.97%）、3456/5075 分支结果（68.10%）。统计范围仍为全部 16 个自有 C 文件、36 份计数
输入和 694 个依赖哈希，70% 行 / 55% 分支结果门槛不变；内部 static 头
实现尚未计入。

双平台显式单头、裁剪、静态/共享消费者及生成单头/API 参考检查通过。
6998 文件快照 `aws-error-retry-freeze-20261001` 在整个最终收集期间不变；
两份新 ACME 报告逐项核对当前源码/依赖后原样公布，没有改写哈希；四份
JWT/OAuth2 报告的输入仍匹配。README 同步支持的错误信封、诊断和退避
合同。provider 本地 IPv6 socket 创建失败也回退到 IPv4 监听。

证据位于 `out/goal-logs/20261001/`：
`acme-aws-error-union-{windows,linux}.log`、
`acme-aws-error-wire-linux-asan.log`、
`acme-aws-error-mutations-{windows,linux-asan}.log`、
`acme-aws-error-vectors-final-{windows,linux-asan}.log`、
`acme-aws-error-delivery-{windows,linux}.log`、
`acme-aws-error-source-freeze.log`、`acme-aws-error-publish.log`、
`acme-aws-error-evidence.log`、`acme-aws-error-artifacts.log`。
首轮因编译警告停止的向量模块构建和修复前的失败日志不算最终验收。
额外向量探针最初引入完整 TLS，Windows GCC16 的 `-O1` 对 Core
`iExtensions` 发出 maybe-uninitialized 警告。源码核对显示计算尺寸的成功
分支赋值、错误函数始终返回 false，据此判断为跨函数分析警告；共享 Core
未修改。最终独立向量探针提取相同模块测试，仅编译解析和 buffer 依赖，
保持 `-O1 -Wall -Wextra -Werror` 及 Linux ASan/UBSan，双平台通过。

AWS 只读 HTTP 响应分类与状态码退避、其余 provider 的实际请求/身份/
缓存、内部头覆盖率、部署文件系统故障及真实服务/托管 CI 仍待验收；
七库长期任务保持进行中。

## 2026-10-01：三家 provider 创建保护与 TC3 规范化

实际 TLS 服务独立复现 Cloudflare 畸形创建应答与华为云 HTTP 500 后 flow
各发出三次创建请求，腾讯云请求因 TC3 动作名大小写错误被独立签名服务拒绝。
红测试 `acme-rest-create-red-windows.log` 只作为缺陷证明，不计入通过结果。
TC3 canonical 的 `x-tc-action` 值现转成小写，传输头仍使用原动作名；固定
向量从独立 Python HMAC/SHA-256 重算，实际服务端按收到的字节核验，依据
[腾讯云 TC3 规范](https://intl.cloud.tencent.com/zh/document/api/1209/56016)。
华为云服务端独立核对标准 URI 尾斜线、查询排序、日期、正文与 HMAC，依据
[Huawei SDK-HMAC 规范](https://support.huaweicloud.com/intl/en-us/devg-apisign/api-sign-algorithm-002.html)；
Cloudflare 的实际 Bearer、API 路径及 JSON 请求同样逐项核对。

三家在请求发送前保留固定记录槽，状态不依赖错误对象能否分配成功。
HTTP 5xx、应答丢失、畸形/矛盾或不可核验的创建响应保留未知状态，返回
`XACME_DNS_ERROR_UNCERTAIN`；解析及诊断分配失败保留 `XERR_MEMORY`。
flow 只调用一次 Add，同名同值后续 Add/Remove 不发任何请求。属主使用
ASCII 小写，TXT 逐字节比较；两线程的大小写别名不能绕过保护。未知和
已确认共用八槽，满载不发创建。发送前的确定失败、完整明确拒绝释放预留
槽；每次 RPC 清除上次 HTTP 未知标记，原未知状态仍保存在自己的槽中。

创建 ACK 要有可用句柄。Cloudflare 要求 success=true、空 errors 数组及
匹配的 TXT name/type/content；华为云要求 name/type/zone_id 与唯一 TXT
值匹配；腾讯云要求正整数 RecordId、非空合法 RequestId 且无 Error。
复用其他记录 ID 的 ACK 不覆盖原拥有者，新记录保持未知。未知记录不会
阻止另一条已确认记录的清理。响应按 HTTP 交付的字节长度解析。
三家 Add/Remove 在上下文内加互斥锁；模块清单和公开特性前提增加 mutex，
配置布局与公开函数签名不变。Unit 仍须由宿主等待在途调用结束。

新建 81 项实际 TLS 回归（Cloudflare 26 / Tencent 28 / Huawei 27），全套
为 282 项（另含 Ali 124 / AWS 77）。双平台完整模块/独立探针/九项 mock
CA 合并运行通过；Linux 全 282 项 ASan/UBSan 通过。服务端保存提交及记录
状态，核对创建和删除次数、正文/认证/签名和最终记录；内存探针检查清理后
活动分配归零，无非法释放或重复释放。原有 15 项尾部填充/长输入/补 NUL
分配失败回归同步使用完整响应与真实长度，双平台完整收集和 Linux sanitizer
验证通过。真实云账号、托管 CI 和部署环境不据此视作已验证。

8 种变体分别独立编译并在 Windows 与 Linux ASan/UBSan 检出：签名动作名
不转小写、畸形 ACK 后重放、覆盖已有 ID、复用未知槽、把终止字节计入正文、
覆盖内存错误、忽略 ACK 属主、保留上次 HTTP 未知标记。编译失败和 sanitizer
崩溃不计为检出；签名变体必须被独立服务端的签名断言拒绝，其余变体必须由
客户端业务断言检出，不能以服务端异常充数。初选 strlen 变体被底层 NUL
拒绝遮蔽，已替换；旧标记控制初选华为云，被前置 GET 复位遮蔽，改在腾讯云
缓存命中路径检出。保留原日志中七项有效控制，只补跑修改后的一项；初选无效
控制不计为通过，最终验证不修改生产输入。

合并覆盖率：Windows 4855/5646 行（85.99%）、3629/5269 分支结果（68.87%）；
Linux 4857/5662 行（85.78%）、3642/5289 分支结果（68.86%）。仍统计全部 16 个自有 C 文件、36 份计数输入和
694 个依赖哈希，70% 行 / 55% 分支结果门槛不变；内部 static 头实现尚未
纳入统计。6998 文件快照 `rest-create-freeze-20261001` 在最终测量期间不变；
两份新 ACME 报告逐项匹配当前源码和依赖后原样公布，四份 JWT/OAuth2
报告的输入仍匹配。双平台显式单头、裁剪、静态/共享消费者通过；Core/ACME
单头、API 参考与特性头只读检查通过，工作区空白检查通过。

证据位于 `out/goal-logs/20261001/`：
`acme-rest-create-union-{windows,linux}.log`、
`acme-rest-create-wire-linux-asan.log`、
`acme-rest-create-mutations-stage2-{windows,linux-asan}.log`（前七项有效控制）、
`acme-rest-create-mutations-last-{windows,linux-asan}.log`（修正后的腾讯云标记控制）、
`acme-rest-create-delivery-{windows,linux}.log`、
`acme-rest-create-source-freeze.log`、`acme-rest-create-publish.log`、
`acme-rest-create-evidence.log`、`acme-rest-create-artifacts.log`、
`acme-rest-create-final-inputs.log`。
早期编译修复、无效变体和 scratch 路径错误不计入最终验收。

范例核对发现 xacme 模块清单的 examples 全为空；当前只有 README 片段、
测试客户端和 mock CA 测试。`acme-rest-create-examples-windows.log` 的 suite
pass 没有编译任何范例，不能算范例验证。后续需登记并实际运行独立离线
ACME 范例，演示核心签发/保存与清理契约，并给出真实服务配置入口。

三家的删除前身份读取、404 与删除 ACK 校验、结果未知的删除对账、重复
已确认 Add 的核对、更具体区域缓存刷新和只读响应分类仍待加固。固定缓冲
与其他签名输入继续审计；AWS 只读状态退避、内部头覆盖率、部署文件系统
故障、真实服务及托管 CI 仍待验收；七库长期任务保持进行中。

## 2026-10-01：Cloudflare 删除身份核对与只读对账

独立 TLS 服务在旧代码下复现两处风险：Remove 未读取记录身份就发送 DELETE，
以及 HTTP 200 空正文虽未执行删除仍使本地句柄清空。红测试日志仅作为缺陷
证据。现在 Remove 按登记时的 zone/record ID 执行
[Cloudflare 记录详情查询](https://developers.cloudflare.com/api/resources/dns/subresources/records/methods/get/)，
必须核对 id、name、TXT 类型和 content，且 success=true、errors 为空数组。
身份变化、畸形响应或权限拒绝不会发删除，也不会清空原句柄。

只有 HTTP 404、完整且无矛盾的失败信封，以及全部错误码为整数 81044 才
证明原记录缺失；错误码与响应形状依据
[Cloudflare 官方仓库的实际响应记录](https://github.com/cloudflare/terraform-provider-cloudflare/issues/1621)
核对，不能把通用 404、鉴权错误、字符串错误码或带结果的矛盾应答当作缺失。
[DELETE 规范](https://developers.cloudflare.com/api/resources/dns/subresources/records/methods/delete/)
允许仅返回 result.id，因此接受 HTTP 200 的匹配 ID；可选 success/errors 若
存在必须是布尔 true/空数组。无 ID、错误 ID、空或畸形正文不能单凭 2xx 成功。

删除丢包、5xx 或不可核验 ACK 后，只读原 ID 对账：明确缺失才释放句柄；
仍存在、身份变化或读取失败均保留。一轮 Remove 不再次发送删除。明确权限
拒绝保留权限诊断；解析、诊断包装或对账分配失败保留 XERR_MEMORY 和原
句柄，后续 Remove 可重新读取。多条匹配记录先前成功的槽可清空，失败和
尚未执行的槽继续保留。内部删除回调传入匹配的属主/TXT 视图，公开 API 不变；
腾讯云/华为云本批只适配内部回调签名，其删除逻辑未据此视作已加固。

新增 62 项 Cloudflare 实际 TLS 删除场景，全套 344 项（Ali 124 / AWS 77 /
Cloudflare 88 / Tencent 28 / Huawei 27）。双平台完整模块、独立 HTTP/TLS、
正文/拥有权/DNS 探针及九项 mock CA 合并运行通过；Linux 全 344 项
ASan/UBSan 通过。新场景核对服务端最终记录、实际读取与删除次数、句柄保持、
大小写别名、原 ID 路径及资源归零；包括读取重试耗尽/恢复、缺失信封、
身份矛盾、ACK 元数据、提交/未提交丢包与错误、多记录部分失败，以及读取、
DELETE 解析、未知诊断和对账解析 OOM。原 15 项正文回归也包含真实身份
读取夹具，在完整收集与独立 Linux ASan/UBSan 下通过。

8 种隔离变体分别在 Windows 与 Linux ASan/UBSan 检出：跳过身份读取、
忽略记录 ID、忽略 TXT 值、信任通用 404、信任空 DELETE ACK、跳过未知
删除对账、覆盖读取内存诊断、清空失败记录槽。每个变体独立编译，必须由
客户端业务断言返回 rc=1，服务端无异常；编译失败或 sanitizer 崩溃不算检出。
所有变体只改临时副本，最终生产输入不变。

合并覆盖率：Windows 4922/5705 行（86.28%）、3713/5345 分支结果（69.47%）；
Linux 4922/5721 行（86.03%）、3725/5365 分支结果（69.43%）。统计仍为 16 个自有 C 文件、36 份计数输入、
694 个依赖哈希，70% 行/55% 分支结果门槛不变；内部 static 头尚未纳入。
6998 文件快照 cf-delete-freeze-20261001 的指纹为 dca9423da93e523bd8b752a76ef61a199bda881f80b3b5e6c9a926fb04ae0dab。
测量期间输入保持不变；两份 ACME 报告按当前源码和依赖逐项核对后原样发布，
四份 JWT/OAuth2 报告仍匹配。双平台单头、裁剪、静态/共享消费者通过；
Core/ACME 单头、API、特性头只读检查与工作区空白检查通过。

证据位于 out/goal-logs/20261001/：acme-cf-delete-union-{windows,linux}.log、
acme-cf-delete-wire-linux-asan.log、acme-cf-delete-mutations-{windows,linux-asan}.log、
acme-cf-delete-delivery-{windows,linux}.log、acme-cf-delete-source-freeze.log、
acme-cf-delete-publish.log、acme-cf-delete-evidence.log、acme-cf-delete-artifacts.log、
acme-cf-delete-final-inputs.log。早期定向测试中的服务夹具修复不计入最终验收。

GET/DELETE 不是原子操作，外部写者可在两次调用之间修改记录；宿主须协调
同一记录的写入生命周期。本批不是实云账号验收，亦未完成腾讯云/华为云删除
身份与结果对账、三家重复已确认 Add/更具体区域缓存、AWS 只读状态退避、
独立离线 ACME 范例、内部头覆盖率和部署/托管 CI。flow 的 DNS 清理重试
耗尽仍按尽力而为处理，后续核验其失败可观测性与宿主回收契约；七库长期
任务仍进行中。

## 2026-10-01：华为云异步删除身份核对与状态对账

旧实现的两项风险由独立 TLS 红测试复现：未读取原记录身份便发送 DELETE，
以及删除仍处于异步处理中就清空本地句柄。现在 Remove 先按保存的
zone/recordset ID 调用
[华为云 v2 记录详情查询](https://support.huaweicloud.com/intl/zh-cn/api-dns/ShowRecordSet.html)，
核对 id、带尾点的 name、TXT 类型、zone_id、唯一 TXT 值和布尔 default=false。
创建/更新/冻结/禁用中的记录返回 XERR_AGAIN，不发 DELETE；身份不符、
权限拒绝、畸形响应及读取内存失败保留原句柄与诊断。

[删除接口](https://support.huaweicloud.com/intl/zh-cn/api-dns/DeleteRecordSet.html)
的 HTTP 202/PENDING_DELETE 表示受理，不能直接作为清理完成。ACK 要求
匹配身份及 default=false；文档示例省略 records，因而允许省略，但若存在
须仍为唯一匹配 TXT 值。受理后最多读取四次，间隔 0.5、1、2 秒；只有
HTTP 404 和无矛盾的 DNS.0313/error_msg 完整错误体证明记录消失，依据
[官方错误码](https://support.huaweicloud.com/intl/zh-cn/api-dns/ErrorCode.html)。
区域不存在 DNS.0302、前缀相似错误码、通用 404 或带记录状态的矛盾错误
均不释放句柄。成功清理只清空对应槽，部分失败保留失败及后续记录。

轮询耗尽返回 XERR_AGAIN，保留句柄和删除中标记；同名同值 Add（含大小写
别名）拒绝且不发请求。后续 Remove 继续读取，已核验受理或读取到
PENDING_DELETE 的记录不重发 DELETE，即使暂时读到旧 ACTIVE 状态。
未知写入结果仅在后续 Remove 再次核对原记录稳定存在时，允许重试原 ID。
发送前失败还原预留标记；明确且无矛盾的拒绝保留权限/协议诊断；解析、
对账及未知错误包装 OOM 保留 XERR_MEMORY。创建 ACK 另拒绝孤立 error_msg。
所需权限包含 dns:recordset:get/delete；Unit 仅释放本地资源。

新增 100 项华为云删除场景及一项创建矛盾应答场景，全套 445 项（Ali 124 /
AWS 77 / Cloudflare 88 / Tencent 28 / Huawei 128）。服务端验证最终记录、
读/写次数、异步状态与原 ID；客户端核对句柄、大小写保护、诊断和资源归零。
覆盖稳定/处理中状态、默认记录、多 TXT 值、缺失分类、重复字段、NUL、
提交/未提交丢包、5xx、错误 HTTP 200/204、畸形/矛盾 ACK、多记录部分失败、
发送前分配失败及读取/ACK/对账/错误包装 OOM。双平台完整模块、独立
HTTP/TLS、正文/拥有权/DNS 探针及九项 mock CA 合并运行通过；Linux 全
445 项 ASan/UBSan 通过。15 项正文回归的华为云夹具按异步删除契约更新，
完整收集与独立 Linux ASan/UBSan 验证通过。

9 种隔离变体分别在 Windows 与 Linux ASan/UBSan 检出：跳过身份读取、
忽略记录 ID、忽略拥有值、把区域 404 当作缺失、受理时提前释放、重复已
受理删除、允许删除中的 Add、覆盖读取内存诊断、删除处理中记录。
每个变体独立编译并由客户端业务断言返回 rc=1，服务端无异常；编译失败
或 sanitizer 崩溃不算检出。生产输入在检测期间保持不变。

合并覆盖率：Windows 5026/5807 行（86.55%）、3855/5489 分支结果（70.23%）；
Linux 5026/5823 行（86.31%）、3867/5509 分支结果（70.19%）。仍统计 16 个自有 C 文件、36 份计数输入及
694 个依赖哈希；70% 行/55% 分支结果门槛不变，内部 static 头尚未纳入。
6998 文件快照 hw-delete-freeze-20261001 的指纹为 313f0f232b3e224a491aa488674c4830ef62eb25b8f5e7ff842cab80de7b9272。
冻结输入在测量及发布时与工作区逐项一致；两份 ACME 报告原样发布，
四份 JWT/OAuth2 报告仍匹配当前输入。发布后仅更新本文。双平台单头、
裁剪、静态/共享消费者通过；Core/ACME
单头、API、特性头和工作区空白只读检查通过。

证据在 out/goal-logs/20261001/：acme-hw-delete-union-{windows,linux}.log、
acme-hw-delete-wire-linux-asan.log、acme-hw-delete-mutations-{windows,linux-asan}.log、
acme-hw-delete-delivery-{windows,linux}.log、acme-hw-delete-source-freeze.log、
acme-hw-delete-bodies-frozen-linux-asan.log、
acme-hw-delete-publish.log、acme-hw-delete-evidence.log、acme-hw-delete-artifacts.log、
acme-hw-delete-final-inputs.log。早期定向日志不是冻结输入的最终验收证据。

GET 与 DELETE 没有原子条件，宿主须协调同一记录的外部写者。本批不代表
实云互操作完成；腾讯云删除身份/结果对账、三家区域缓存/重复已确认 Add、
AWS 只读响应分类与退避、独立离线 ACME 范例、内部头覆盖率、部署文件
系统异常恢复及托管 CI 仍待验收。flow 清理重试耗尽的诊断与宿主回收契约
仍需核验；七库长期任务继续。

## 2026-10-01：腾讯云固定域名 ID、删除身份核对与库存对账

独立 TLS 红测试复现旧版未读取原记录身份便删除，以及 HTTP 200 的空
Response 导致丢弃尚未删除记录的句柄。现在每次新 Add 从完整候选域名
逐级查询 [DescribeDomain](https://cloud.tencent.com/document/api/1427/56173)，
核对 Domain 与正整数 DomainId；仅准确的域名缺失码允许父域回退，权限、
畸形或矛盾响应不回退。去除父区域缓存，每次重新检查更具体域名；每个
登记槽固定自己的 DomainId，后续查询/删除始终使用原 ID。

Remove 使用 [DescribeRecord](https://cloud.tencent.com/document/api/1427/56168)
核对原 ID、DomainId、相对属主、TXT 值、默认 RecordLine/RecordLineId 及
整数 Enabled。身份变化、错误字段、权限/协议/内存失败保留句柄，不发
DELETE。[DeleteRecord](https://cloud.tencent.com/document/api/1427/56176) 的
ACK 仅接受 HTTP 200、非空 RequestId、无 Error 或矛盾结果；可选 RecordId
若存在须为原正整数。创建 ACK 也拒绝非 200 或矛盾结果字段。

删除结果未知，一次 Remove 只读对账，不重复 DELETE；仍存在或读取失败
保留原句柄和诊断，未知状态阻止同名同值 Add。后续 Remove 只有重新核对
原身份后才可重试原 ID。发送前失败还原状态；明确拒绝保留权限/协议诊断。
解析及诊断分配失败保留 XERR_MEMORY，后续对象访问不覆盖该首因。
只读 RPC 仅对传输断流限次重新签名重试，HTTP 协议/NUL/内存错误不重试；
此限制由预检中 NUL 被重试掩盖的问题修正并回归。

RecordIdInvalid 仅表示记录编号错误，不能单独证明记录消失。对账另核对
当前域名 ID 与 [无过滤记录库存](https://cloud.tencent.com/document/api/1427/56166)，
固定 Offset=0/Limit=3000/ErrorOnEmpty=no，要求 TotalCount、ListCount 与
数组长度一致且不超过 3000，所有 ID 为唯一正整数且字段有效。空库存可用，
分页、过滤、错误体、重复 ID、NUL 或畸形字段均不能证明缺失。库存仍含
原 ID 返回 XERR_AGAIN；过大的域名库存保留句柄，交由宿主核对。
官方说明新记录存在索引延迟并建议 30 秒后重查；创建 ACK 后不足 30 秒
或单调时钟回退不查询库存，也不释放句柄。该宽限来自重查建议，并非
服务端一致性的最大延迟保证；当前缺失判断依赖最终索引可见性，实云
缺失语义与更长索引滞后仍待受控互操作验收。

新增 100 项删除/库存场景、15 项域名发现及两项创建 ACK 场景，全套
562 项（Ali 124 / AWS 77 / Cloudflare 88 / Tencent 145 / Huawei 128）。
覆盖外部修改、默认线路、禁用记录、固定 ID、更具体域名、多记录部分失败、
0/3000/超限库存、计数字段、重复 ID、原始及 JSON 解码后的 NUL、边界时钟、
提交/未提交丢包、5xx、错误 ACK、只读断流、发送前及各阶段 OOM。
服务端核对签名、请求字段、原 ID、读写次数、最终记录与未拥有的 NS/A
记录；客户端核对句柄、大小写保护、错误分类及分配归零。双平台完整
模块、独立 HTTP/TLS/正文/拥有权/DNS 探针及九项 mock CA 合并通过；
Linux 全 562 项 ASan/UBSan 通过。正文回归扩至 18 项，独立 Linux
ASan/UBSan 验证通过。

11 种独立编译变体在 Windows 与 Linux ASan/UBSan 检出：跳过身份读取、
忽略 DomainId、忽略 TXT 值、仅凭编号错误释放、忽略索引宽限、接受空
RequestId、允许未知删除中的 Add、覆盖内存诊断、忽略库存计数、忽略
重复 ID、重试协议错误。均为客户端业务断言 rc=1，服务端无异常；编译
失败与 sanitizer 崩溃不算检出。生产输入在检测期间保持不变。

合并覆盖率：Windows 5231/6015 行（86.97%）、4135/5823 分支结果（71.01%）；Linux 5233/6031 行（86.77%）、4148/5843 分支结果（70.99%）。
仍统计 16 个自有 C 文件、36 份计数输入、694 个依赖哈希；70% 行/55%
分支结果门槛不变，内部 static 头尚未纳入。6998 文件快照
tc-delete-freeze-20261001 指纹为 16488daa1e841506f8fe6bafa93a9e4ff432c5d292f816e3db2a97a2b546ce12。所有冻结输入均保持快照哈希，xacme 自有 C 文件、公开/私有头及探针仍匹配工作区。
工作区 Core 日志实现和单头在冻结后继续变化，六份 ACME/JWT/OAuth2
报告仅作为快照证据原样归档于 out/acme/tc-delete-frozen-20261001/coverage
及 out/auth_extensions/tc-delete-frozen-20261001/coverage，不替换或改写
工作区的当前报告。其依赖哈希失配须待 Core 输入稳定后重新测量；当前
工作区覆盖率刷新仍是待验收项。双平台单头、裁剪和静态/共享消费者均在
该冻结快照上通过。Core 后续改动使工作区 ACME 单头过期，现已重新生成，
单头/API/特性头一致性检查通过；新生成版本的运行验收和覆盖率仍需刷新。

证据：out/goal-logs/20261001/acme-tc-delete-union-{windows,linux}.log、
acme-tc-delete-wire-linux-asan.log、acme-tc-delete-mutations-{windows,linux-asan}.log、
acme-tc-delete-delivery-{windows,linux}.log、acme-tc-delete-red-windows.log、
acme-tc-delete-source-freeze.log、acme-tc-delete-bodies-frozen-linux-asan.log、
acme-tc-delete-frozen-evidence.log、acme-tc-delete-frozen-publish.log、
acme-tc-delete-frozen-artifacts.log、acme-tc-delete-frozen-final-inputs.log。预检日志不作为冻结版本的最终验收证据。

查询与 DELETE 之间无原子条件，宿主须协调同一记录的外部写者；权限需
包含 DescribeDomain/DescribeRecord/DescribeRecordList/CreateRecord/DeleteRecord。
本批不代表实云互操作验收。OperationDenied 子类等响应分类尚需定向核验。
独立公共 API Obtain/手动 DNS 范例草稿已在双平台实际执行签发、账户落盘、
证书/私钥配对、缓存复用及 TXT 清理，未确认 TXT 的 Add 不重放；证据为
acme-obtain-example-draft4-{windows,linux}.log。草稿仍在 out/goal-temp，
尚未登记至构建清单或接入 CI，不能视作已交付范例。CF/华为云缓存、三家重复已确认 Add、其余
只读分类/OOM、实际运行的独立离线 ACME 范例、flow 清理诊断、内部头
覆盖率、文件系统异常恢复与托管 CI 继续作为七库长期任务的待验收项。

## 2026-10-02：公开 ACME 范例交付与腾讯云拒绝分类

公开 API 范例 `extlibs/xacme/examples/obtain/main.c` 已登记于 acme_obtain
与 xacme 产品构建清单。范例使用同步手动 DNS 回调，五个参数明确指定
目录、CA PEM、传播 resolver、store 和域名；无参数仅显示用法。
`tools/test_acme_examples.py` 实际驱动注册账户、DNS-01 签发、保存/加载
账户、证书及配对私钥、第二次 Obtain 缓存复用和 TXT 清理。独立服务端
核对账户/订单仅创建一次，cryptography 核对 SAN、叶证书/私钥及
给定 CA 的 issuer 名称匹配；独立签名链验证尚需加强。
POSIX 另核对账户和私钥权限 0600。EOF 未确认修改仅调用一次 Add，
返回结果未知及 pending_dns=1，不凭同值认领或重放。手动回调供单次
命令演示，服务宿主仍须实现可重入的拥有权 provider。

冻结版本的 Windows/Linux 模块化及单头范例均实际通过；Linux 单头
分别使用 GCC、Clang ASan/UBSan。常规 CI 双平台执行这两种形式，
夜间 CI 执行 Clang 单头 Sanitizer；已验证对应本地命令及 YAML 语法，
托管 runner 的实际结果仍待验收。范例执行是独立功能门禁，未计入
下述自有 C 文件覆盖率，内部头文件 static 实现也尚未统一统计。

腾讯云明确拒绝的完整错误信封现在识别 OperationDenied 及点分子类，
创建和删除均保留权限错误种类；相似前缀 OperationDeniedSpoof 仍按
未知结果处理。创建明确拒绝释放预留槽位，删除明确拒绝恢复原 pending
状态供调用方重试；未知删除只读对账且保留记录 ID。新增五项真实 TLS
场景核对槽位、诊断、请求次数和原 ID 清理。五家总计 567 项
（Ali 124 / AWS 77 / Cloudflare 88 / Tencent 150 / Huawei 128），
双平台完整集合和 Linux 全集合 ASan/UBSan 通过；18 项请求正文
Sanitizer 回归通过。13 种独立编译的删除/分类缺陷在 Windows 和
Linux Sanitizer 下均由客户端断言检出，服务端无错误，不以编译或内存
诊断失败充当检出证据。单头、裁剪、静态/共享消费者双平台通过。

| 平台 | ACME 行 | ACME 分支结果 | JWT 行/分支结果 | OAuth2 行/分支结果 |
| --- | --- | --- | --- | --- |
| Windows | 5231/6015（86.97%） | 4137/5823（71.05%） | 93.30% / 74.07% | 92.41% / 77.39% |
| Linux | 5233/6031（86.77%） | 4150/5843（71.03%） | 93.30% / 74.07% | 92.25% / 77.20% |

ACME 两份报告覆盖 16 个自有 C 文件、36 个合并来源，694 个依赖输入
均按原始 SHA256 校验；完整模块、九项 mock CA（无跳过）、所有独立
HTTP/TLS、DNS、拥有权和 provider 探针均运行。JWT/OAuth2 四份报告
也重新测量，包括 OAuth2 全部独立网络和生命周期组。六份报告原样
发布至 `out/acme/coverage/{win32,linux}/coverage.json` 和
`out/auth_extensions/coverage/{win32,linux}/{jwt,oauth2}/coverage.json`，
发布前冻结版本与工作区源码/依赖哈希逐项相同，未修改任何报告哈希。

本轮 source-freeze 位于 out/goal-temp/acme-example-freeze-20261002，
7000 个输入文件，指纹 94bdfbfd12ccadc50474a84235476242e864021f560b24b50a284d27b58fc3db。
所有快照文件在验证后仍匹配冻结哈希，验证完成后仅更新验收记录及
README 待验收表述；运行源码、生成物及报告依赖仍与快照一致。先前 tc-delete
归档保留为历史证据，本轮报告刷新了 Core 变动后的工作区状态。

证据位于 out/goal-logs/20261002/acme-example-{union-windows,union-linux,
auth-windows,auth-linux,wire-linux-asan,bodies-linux-asan,delivery-windows,
delivery-linux,modular-windows,modular-linux,single-windows,single-linux-asan,
single-linux-clang-asan,mutations-windows,mutations-linux-asan,source-freeze,
evidence,publish,artifacts,final-inputs}.log。预检不作为冻结版的最终证据。

本轮不代表七库生产级验收完成。腾讯云索引可见性的安全判定与实云
缺失语义、CF/华为云缓存和三家重复已确认 Add、其余响应分类/OOM、
证书链独立密码学验证、flow 清理诊断、内部头覆盖率、文件系统异常恢复、其他协议/认证扩展
的剩余故障分支和真实服务互操作仍按下一阶段推进。

## 2026-10-02：腾讯云长索引滞后与独立证书链验收

腾讯云 [DescribeRecordList](https://cloud.tencent.com/document/product/1427/56166)
说明新记录有索引延迟，建议 30 秒后重试，但没有承诺最大延迟。
旧实现把宽限后的完整库存缺失作为记录消失的证明；新实现不再据此释放
记录。RecordIdInvalid、空库存、连续完整库存都只提供诊断，保留原 ID，
返回 XERR_AGAIN 并阻止同名同值 Add。创建后不足 30 秒或时钟回退
仍不查询库存；宽限仅限制诊断查询，不充当一致性保证。
库存类型、计数、唯一 ID、原域名 ID 和字段校验保持失败关闭。

后续读取恢复且原记录身份完全匹配时，允许按原 ID 删除；成功清理需要
严格有效的 DeleteRecord ACK。若云端已提交删除而 ACK 丢失，后续缺失
仍无法自动证明最终结果，句柄与 pending 状态会保留，宿主须核查云操作
并协调恢复/重建。此可用性限制已写入公开头与 README；实云恢复契约和
受控互操作仍待验收。查询与删除之间仍须协调外部写者。

四项独立 TLS 场景让物理记录仍存在、索引在一天后缺失，并分别覆盖
非空库存、空库存、连续两次跨日缺失，以及未提交删除丢包后的缺失。
原实现四项均由客户端业务断言失败（rc=1、server=[]），修复后原句柄
和未知状态保持，禁止重复创建，读取恢复后仅清理原 ID。
五家完整集合现为 571 项（Ali 124 / AWS 77 / Cloudflare 88 /
Tencent 154 / Huawei 128）；Windows/Linux 与 Linux ASan/UBSan
完整集合通过。14 种独立编译缺陷在两平台均由客户端断言检出，包含
重新接受库存缺失与遗漏 pending 隔离；编译失败及内存诊断不算检出。

`tools/test_acme_mock.py` 的授权证书独立检查改为 cryptography 验签：
按叶证书起点验证有序链、精确 DER 信任锚、全部有效期、签发者 CA、
keyCertSign 与路径长度，支持省略根和中间证书链；私钥配对与 SAN
仍独立核对。18 项真实密钥/证书对抗测试覆盖同名错误密钥、伪根、断链、
重复、过期/未来证书、非 CA、用途及路径长度限制、备选信任根。
旧 oracle 有 11 项失败，新 oracle 两平台全部通过，九项 mock CA 无跳过。
这只是离线夹具验收，不是通用 PKIX 策略、撤销或名称约束验证器；
运行依赖 cryptography>=42，CI/夜间配置已加入该独立门禁。

| 平台 | ACME 行 | ACME 分支结果 |
| --- | --- | --- |
| Windows | 5230/6014（86.96%） | 4133/5819（71.03%） |
| Linux | 5232/6030（86.77%） | 4146/5839（71.01%） |

覆盖率仍为 16 个自有 C 文件、36 个合并来源，依赖增加至 695 个原始
SHA256 输入；完整模块、HTTP/TLS/拥有权/DNS/provider 探针、18 项
oracle 和九项 mock 全部执行。70%/55% 防倒退门槛不变，内部 static
头未纳入；公开范例为独立功能门禁，不计入这些 C 文件计数。
两平台单头、裁剪、静态/共享消费者以及模块化/单头公开范例通过；
Linux GCC/Clang 单头 ASan/UBSan 范例和 18 项正文回归通过。

两份 ACME 报告在本快照重新测量。JWT/OAuth2 的四份前轮报告与本
冻结快照的源码、Core 和测试依赖相同，原样复用，没有宣称重新测量。
最终发布校验发现工作区 Core 字符串头/实现、Core 单头及相关生成物
在运行期间变化，因此拒绝把六份报告发布为当前工作区证据。
报告原样归档于 out/acme/index-chain-frozen-20261002/coverage 和
out/auth_extensions/index-chain-frozen-20261002/coverage；源文件与
依赖哈希、原始计数、增量合并及门槛全部按冻结输入核对，未改写哈希。
工作区当前 ACME/JWT/OAuth2 覆盖率仍须在新 Core 输入稳定后重新测量。
本轮快照 out/goal-temp/acme-index-chain-freeze-20261002 包含
7001 个文件，指纹 d6d80ba6fb988c18e17a3938ef105034f3f126b8e54162acff616784d6dde2c8；快照全部输入保持原哈希，ACME
自有实现、公开/私有头与探针仍匹配工作区。Core 后续变化使工作区
ACME 单头过期，现已重新生成并通过一致性检查。新 Core 下公开单头
范例已在 Windows 及 Linux Clang ASan/UBSan 再次实际执行通过；这两项
定向验证不替代当前 Core 下完整套件、覆盖率及交付消费者的重新验收。

冻结证据位于 out/goal-logs/20261002/acme-index-chain-{union-windows,
union-linux,wire-linux-asan,bodies-linux-asan,delivery-windows,delivery-linux,
modular-windows,modular-linux,single-windows,single-linux-asan,
single-linux-clang-asan,mutations-windows,mutations-linux-asan,source-freeze,
auth-revalidated,frozen-evidence,frozen-archive,workspace-drift,final-inputs}.log。
新 Core 定向验证为 acme-index-chain-current-core-single-{windows,linux-asan}.log。
acme-index-chain-evidence.log 记录最终工作区漂移导致的发布拒绝，
不能当作当前工作区完整验收通过。
红灯证据为 acme-late-index-red3-windows.log 和
acme-grant-verifier-red-windows.log；预检不替代冻结验收。
前轮章节保留为历史快照；其索引缺失判断及 issuer 名称 oracle
已由本轮取代。七库整体生产验收仍未完成。

## 2026-10-02：覆盖率溯源门禁与七库双平台验收归档

用户批准后安装并注册 Ubuntu 26.04.1 LTS，补齐 GCC/gcov 15.2.0、
Clang 21.1.8、Python 3.14.4、cryptography 46.0.5 与 OpenSSL 3.5.5。
所有 Linux 结果来自恢复环境后的实际执行；Windows GCC 16.2.0 验证同步完成。
环境版本记录见 out/goal-logs/20261002/ubuntu-restored-environment.log。

认证覆盖率曾漏绑定主测试、密钥夹具及 OAuth2 独立探针/工具。
真实 GCC 编译执行后更改测试输入仍被旧工具接受，三项溯源检查原实现
全部失败；修复后两平台全部通过。JWT 报告现在绑定九项依赖，OAuth2
统一报告绑定十三项依赖，测试或运行时变化使测量失败且不保留旧报告。
CI 已接入 tools/test_auth_coverage_inputs.py，现有 85%/67% 与 91%/75%
防倒退门槛保持不变。

ACME 测量在编译前检查 Core 与 ACME 单头生成结果，并把生成物及相关
生成工具纳入输入哈希，依赖从 695 项增加到 701 项。共享工作区变化曾使
复制的 Core 与 ACME 单头不同步；实际失效快照已被检查拒绝。两个失败的
预检快照不作为本轮验收结果，未把源代码一致的哈希误当作生成一致性的证明。
验证用快照 auth-core-ready-freeze-20261002 同时通过 Core/ACME 生成检查，
7002 个文件，指纹 8e674a946db60538abc70ca3d0515e0fec91027fea54708ffe5325a0cbfcc5a2。
测量后全部冻结文件仍匹配原始哈希；发布另逐项检查当前工作区的实际测量依赖。

邮件四库替换文本百分比换算，使用精确 gcov JSON 和完整自有 C 文件集。
Windows/Linux 计数按 native-windows/native-linux 隔离，每份报告记录 867
项源码、Core、测试、夹具、清单与构建工具依赖，以及原始对象 gcno/gcda
哈希。--report-only 仅复核已有报告，拒绝未绑定旧计数、陈旧输入、变化或
丢失的计数、伪造计数/非有限值/重复 JSON 字段，不重写原报告或重绑哈希。
旧工具在五项真实 GCC 检查中全部失败，新工具十三项回归两平台全部通过。
默认门槛与既有 CI 精确一致；CI 新增 Windows 四产品覆盖率矩阵，两个
平台保留 JSON 报告和自有对象的原始 gcno/gcda。

| 库 | 原测量输入 Windows 行 / 分支结果 | 原测量输入 Linux 行 / 分支结果 |
| --- | --- | --- |
| `xacme` | 87.00% / 71.04% | 86.77% / 71.01% |
| `xmail` | 78.18% / 64.70% | 78.20% / 64.70% |
| `xpop3` | 76.52% / 60.00% | 76.52% / 60.00% |
| `xsmtp` | 81.49% / 63.44% | 81.49% / 63.44% |
| `ximap` | 79.82% / 61.49% | 79.82% / 61.44% |
| `xjwt` | 93.30% / 74.07% | 93.30% / 74.07% |
| `xoauth2` | 92.25% / 77.30% | 92.25% / 77.20% |

表项均只统计扩展自有 C 文件，排除 Core 和内部头中的 static 实现。
ACME 仍为 16 个 C 文件、36 个合并来源，完整模块、全部独立 HTTP/TLS、
DNS/拥有权/provider 探针与九项 mock CA 都执行；JWT 为主套件，OAuth2
为主套件及两个独立网络/拥有权来源的合并。邮件为明确标注的
module_suite_object_profiles：xmail 19、xpop3 4、xsmtp 4、ximap 9 个 C 文件。
独立 Python 邮件 TLS 探针尚未计入本轮覆盖率；没有宣称邮件已经统一合并。
精确分子、分母、剩余缺口、来源贡献和输入哈希见各库 JSON 报告。

本轮重新运行 Windows/Linux ACME 571 项线路用例、18 项证书链对抗检查、
全部九项 mock CA（无跳过），Linux 571 项线路及 18 项请求正文 ASan/UBSan。
公共 ACME 模块化/单头范例、Linux GCC/Clang 单头内存检查，以及双平台
单头、裁剪、静态/共享消费者通过。认证九种测试/范例、JWT 334 项、
OAuth2 Windows 264/Linux 257 项、OIDC 15 项与双平台 OpenSSL RFC 5958
互操作通过；Linux JWT/OAuth2 主套件及全部 OAuth2 独立组 ASan/UBSan 通过。
邮件完整模块套件、原报告复核及四库聚合公共单头两平台通过，生成物同步
并通过 features/单头/API 参考检查。上一阶段的十四种 provider 变体仍保留
为其冻结版本证据，本轮没有重复执行或宣称在新 Core 下重跑这些变体。

最终复核发现共享工作区 src/value/value_container.c 与 single/xrt.h 再次
变化：容器新增允许 UINT/CHAR 元素，Core 单头随后同步。当前发布门禁
实际拒绝了这两项漂移，邮件 --report-only 也实际拒绝了变化后的 Core。
六份 ACME/认证报告不发布为当前证据，八份邮件报告虽在工作区测量成功，
现在也只对应原测量输入；没有修改任何报告里的输入哈希。

十四份报告原样归档于 out/{acme,auth_extensions,mail}/input-bound-frozen-20261002/coverage。
邮件自有对象的原始 gcno/gcda 另存于邮件归档的 profiles 子目录，全部核对
原始 SHA256。867 项邮件原输入按报告哈希，从工作区未变文件和先前保留的
有效快照恢复至 out/goal-temp/mail-input-bound-recovery-20261002；这明确是
原测量输入恢复，不是事后建立的新测量快照。恢复指纹为
d08349948ce09f1bf073434fc2511b941cd96805888a713015675b99a770261e。
复核记录为 seven-input-bound-archive-verification.json 和 seven-current-evidence.json。
当前默认报告路径里的旧报告保留原输入哈希，不能视作新 Core 的完整验收；
七库当前版本仍须刷新验证，表中数字仅属于已保存的输入版本。
新 Core 下 ACME 与邮件四库生成头已同步，Core/扩展单头、features 和
API 参考检查通过。公开 ACME 单头签发/缓存/清理范例又在 Windows 与
Linux Clang ASan/UBSan 实际通过，相关输入在运行前后保持原哈希；证据为
seven-value-current-generated.log、seven-value-current-acme-single-{windows,linux-clang-asan}.log
及同名输入记录。这两项当前 Core 定向验证不替代完整套件和覆盖率刷新。

日志位于 out/goal-logs/20261002/auth-core-ready-{source-freeze,union-windows,
union-linux,auth-windows,auth-linux,auth-full-windows,auth-full-linux,
auth-sanitizer-linux,oauth2-sanitizer-linux,wire-linux-asan,bodies-linux-asan,
delivery-windows,delivery-linux,modular-windows,modular-linux,single-windows,
single-linux-asan,single-linux-clang-asan,jwt-openssl-windows,jwt-openssl-linux}.log，
以及 mail-current-{gcov-inputs,mail-inputs,measure,report-only}-{windows,linux}.log、
mail-current-single-{xmail,xpop3,xsmtp,ximap}-{windows,linux}.log 和 mail-current-generated.log。
红灯为 auth-coverage-inputs-red-windows.log、mail-coverage-inputs-red-windows.log；
生成拒绝证据为 auth-core-generation-gate-stale-windows.log。Linux 单头初次
WSL 通配符参数展开失败保留为 argv-preflight 日志，后续实际构建通过；
参数预检失败不作为库测试失败或变体检出证据。

本轮完成原输入双平台测量、证据归档与测量工具门禁，七库整体生产验收仍未完成。
CF/华为云缓存、三家重复已确认 Add、provider/flow 故障分支、内部头
统计、宿主恢复/卸载、部署文件系统故障、外部服务互操作与托管 CI 结果
继续作为明确的剩余验收项。本文早期章节保留历史结果，最新输入绑定数字以本节为准。

## 下一阶段的验收顺序

1. 本轮七库原输入的双平台精确测量与证据归档已完成；共享 Core 再次变化，继续刷新当前版本验收，保留原输入及计数，不能重绑旧哈希；同时推进已确认的 provider 缓存与重复 Add 问题；腾讯云保留未知缺失与独立签名链已完成离线回归；继续建立已提交删除应答丢失的宿主恢复契约及实云语义证据；完善 Cloudflare/华为云更具体 zone 缓存刷新、三家重复已确认 Add 的核对，以实际 TLS 回归验收；补全 AWS 只读 HTTP 响应分类与状态码退避、各 provider 分配失败及多记录清理；继续补 flow 的签发/账户/响应故障分支，并把内部 static 头实现纳入统计。
2. 依据 OAuth2 合并报告补齐认证请求构造、异常输入及分配失败分支；核对真实宿主的停止新调用、清理重试与卸载顺序，验证失败未交付对象的 Pending 契约。
3. 在实际维护窗口演练 ACME 旧版回收，并对 Windows 写入失败、并发进程异常终止和断电恢复做定向验证；继续审计文件系统与编译特性下的目录/私钥文件描述符约束。
4. 邮件四库精确 gcov JSON、原始计数/输入 SHA256 和双平台门禁已完成；继续把独立探针纳入统一统计，再以畸形 MIME、断流、超时、TLS 拒绝和服务器错误样本补足缺口；单独分析共享 `xmail` 认证与传输层的跨协议触达。
5. 在隔离测试账号下执行 CA/DNS、POP3/SMTP/IMAP 和 OAuth 提供商互操作；记录服务端版本、配置与结果，再跑 Linux/Windows CI 和共享包消费验证。

## 2026-10-02：Cloudflare/华为云完整属主与动态区域发现

两家 provider 原来按已缓存父区域直接查 ID，漏查同一父区域下更具体的
子区域；缓存子区域撤销后也不能恢复探测父区域。未缓存路径先去掉属主
首段，遗漏完整 `_acme-challenge` 属主本身为托管区的情况。区域全部有效
为空时，返回失败却留下 NONE 或先前无关错误。

新增独立 TLS 服务分别保存父区域、子区域和记录的创建区域，支持区域
新增、撤销及 ID 替换。原实现实际编译后，20 项新增场景全部被拒绝：
18 项为客户端契约断言失败（退出 1、服务端无错误），两项为独立服务
检出区域 ID 替换时跳过完整属主的查询顺序。没有把编译失败当作红灯。
原有 Huawei rest-zone-id 场景另作正向保护通过；初次测试路由误把这一
旧场景纳入新服务的日志仅保留排查，不计入本轮 20 项红灯。
原二进制、原编译输入哈希和改动前的实际 provider/公共助手源码另行保存。

修复移除父区域后缀缓存及只被该缓存使用的内部助手。每次新建记录从
完整属主逐级查询当前凭据可见的最近托管区域；只有有效空列表才继续
查父区域。鉴权、协议、歧义等查询错误立即停止，不发起父区域写入。
当前区域不再存在时，可基于新的有效空结果继续查询父区域；新记录使用
重新发现的区域 ID，原记录的 zone/record ID 保持原值。没有托管区域
明确设置 xrt.acme.dns/XACME_DNS_ERROR_ZONE，种类为 XERR_PROTOCOL，
覆盖先前无关错误。删除仍先核验登记时的完整身份，不重新选区域。

20 项回归覆盖父/子区域、子区域新增/撤销、完整属主为区域、区域 ID
替换、权限拒绝、畸形与歧义响应，以及无区域和旧错误覆盖。失败后的
已有记录仍可按原 ID 清理；未创建的另一属主/值 Remove 不发请求。
独立服务核对完整查询次序、每个记录的创建/删除区域和 ID、读取次数及
清理后无残留记录。原缓存测试随无用缓存实现移除，公开 provider 最小
契约和错误语义测试仍保留。

验收使用 rest-zone-ready-freeze-20261002 快照，7003 个文件，指纹
e27b8fa2824d062c369c145a115123c23db25a1be5dea4e934acdf597a308a24。
快照复制前后同时核对输入，并通过 Core/ACME 生成一致性检查。Windows
GCC、Linux GCC、Linux Clang ASan/UBSan 各先执行 20 项区域回归，再执行
五家全部 591 项独立 TLS 场景（Ali 124、AWS 77、CF 98、Tencent 154、
Huawei 138），全部通过。Clang 启用栈返回后访问、泄漏和未定义行为
检查；实际 provider/HTTP/网络引擎仍参与执行，未替换区域解析或删除实现。

Windows/Linux 完整模块套件的 27 个程序、聚合公开单头与实际 Obtain
单头范例通过。模块套件中五家实云入口和 TLS 调试入口因未配置环境显式
跳过；591 项独立 TLS 场景均实际运行，没有跳过。范例独立确认签发、
账户和配对私钥持久化、第二次缓存复用、一次 TXT 清理，以及未确认 TXT
留给宿主处理且不重放 Add。两平台使用独立对象目录和原生临时存储目录。

全部冻结文件在各组执行前后和最终复核时保持原哈希。当前工作区与快照
的运行时、测试、清单和工具输入一致；只有随后补写的 README 与本工作单
不同。逐项结果、日志与二进制 SHA256、原红灯分类见
out/goal-temp/rest-zone-evidence.json；日志为 rest-zone-red-windows.log、
rest-zone-full-{windows,linux,linux-clang-asan}.log、
rest-zone-modules-{windows,linux}.log 和 rest-zone-generated.log。
新场景随现有完整 provider-wire 入口进入常规及 sanitizer CI；本轮没有
重新运行此前的故障变体，也没有宣称托管 CI 或真实提供商账号已验收。

公开说明与生成头、声明、API 参考已同步。区域 API 可见性和真实 DNS
委派须由宿主配置，并在实云独立验收。本轮未重新测量覆盖率；上一节
十四份归档报告仍只属于其原输入，不作为本轮新实现的计数证据。
接下来继续三家已确认 Add 的重复调用、区域解析 OOM 原始错误、只读状态
分类及多记录清理；七库完整覆盖率和交付闭环随后在稳定输入下刷新。
长期目标保持 active，七库整体生产验收未完成。

## 2026-10-02：三家已确认 DNS Add 的拥有权复用

Cloudflare、腾讯云及华为云的公开 Add 原来会为已经确认拥有的同属主、
同值 TXT 再次创建记录；重复调用耗尽八槽容量，并可能产生重复资源。
新增 71 项独立 TLS 场景先以原实现实际编译运行，全部为客户端契约断言
失败（退出 1、服务端无错误）。原二进制、编译输入哈希、三家原源码、
公共助手、测试客户端和独立服务源码均保留；没有把编译失败作为红灯。

公开 Add 在原互斥锁内按规范化属主、逐字节 TXT 查找已经登记的非空 ID。
只有读取该原始 ID、核对完整身份与有效状态成功，才复用原槽并返回成功。
即使容量已满或新子区域出现，重复调用也不创建记录、不重新选择区域。
不同 TXT 的新建仍走当前区域发现；不把其他账号创建的同值记录认领为己有。
同名同值未知创建与尚未确认的删除仍阻止重放。

Cloudflare 核对原 zone/record ID、属主、TXT 类型和值。华为云同时检查
默认线路标志和状态：ACTIVE 可复用，PENDING_CREATE/PENDING_UPDATE 等
未完成状态返回 AGAIN，DISABLE/FREEZE/ILLEGAL/POLICE/ERROR 拒绝作为
有效 Add；PENDING_DELETE 保留原 ID 和删除待确认状态，后续 Add 不发请求。
Cloudflare/华为云只有严格确认原 ID 缺失，才能释放原槽并重新发现区域创建。
腾讯云先核对保存的 Domain/DomainId，再读取原 RecordId 并检查 Enabled；
区域替换、更名、禁用、完整身份不符均保留记录并返回错误。腾讯云记录
索引中的缺失仍不作为删除证据；Add 的只读失败不虚构删除待确认状态。
读取鉴权、断流、5xx、畸形正文、NUL、OOM 等故障均不触发新的创建请求。

71 项新增场景包括大小写等价属主、十次重复、八槽满载、八线程同时 Add、
子区域新增、新旧值共存、原记录消失，以及 ID/属主/类型/值/域名/状态错误。
独立服务从实际 TLS 请求核验签名、域名 ID、查询顺序、创建/读取/删除次数、
原记录所在区域和最终资源清理。真实 JSON 解析分配失败及一次/连续断流
仍由实际 provider、HTTP 和网络引擎处理，没有替换这些实现。

原有三项重复记录部分清理回归保留：它们专门通过实际内部 AddLocked
创建历史重复记录，再走实际公开删除与身份读取，核对权限失败时剩余
拥有权和重试。仅这些历史种子绕过新的公开复用入口；71 项新增场景全部
调用公开 Add。原腾讯云更具体区域测试使用不同 TXT 创建第二条记录，
继续核对原父区域记录与新子区域记录各自的清理，没有削弱删除断言。

验收快照 rest-owned-ready-freeze-20261002 包含 7003 个输入文件，指纹
c7a683e028922583a9b9313a12c18ee03ce24ecaa51fd0ad81e7053b8f4153a6。
Windows GCC、Linux GCC、Linux Clang ASan/UBSan 各先执行 71 项新增回归，
再执行全部 662 项独立 TLS 场景（Ali 124、AWS 77、CF 117、Tencent 179、
Huawei 165），每种配置实际执行 733 次、全部通过。Clang 启用泄漏、
栈返回后访问和未定义行为检查。初次绿灯中腾讯云本地 DomainId=0 场景
遇到独立服务分类错误，该日志不算完整通过；修正服务后单项确认，再用
当前冻结输入重编译并完整执行三种配置，上述结果均来自最终完整运行。

Windows/Linux 完整模块套件各完成 27 个测试程序和聚合公开单头运行。
模块中的 10 个外部配置入口显式跳过：五家 DNS 实云、实 DNS TXT、
Pebble flow、实云 flow、指定 HTTP 服务及 TLS 调试；其他本地检查通过。
662 项独立 TLS 场景没有跳过。实际公开 Obtain 单头范例在两平台独立验证
签发、账户及配对私钥持久化、缓存复用、一次 TXT 清理与未确认 Add 不重放。

快照全部输入在执行前后及最终复核时保持原哈希；复核时工作区的运行时、
测试、清单、生成文件和工具输入与快照一致，仅本工作单的后补记录不同。
逐项计数、验证时间、原红灯、日志与二进制 SHA256 见
out/goal-temp/rest-owned-evidence.json；日志为 rest-owned-red-windows.log、
rest-owned-full-{windows,linux,linux-clang-asan}.log、
rest-owned-modules-{windows,linux}.log 和 rest-owned-generated.log。
公开头、README、生成单头及 API 参考同步；新场景随完整 provider-wire
入口进入常规及 sanitizer CI。本轮没有重跑历史变体，也没有重新测量
覆盖率；此前十四份归档报告仍只属于各自原输入。托管 CI 和真实提供商
账号互操作尚未完成。接下来继续区域解析 OOM 首因、只读状态分类/退避、
腾讯云完整属主发现、provider 销毁后调用及 flow 清理诊断，再在稳定输入下
刷新七库覆盖率和完整交付验收。长期目标保持 active。


## 内建 DNS provider 回调生命周期修复（2026-10-02）

Ali、Cloudflare、Tencent、AWS、Huawei 的十个公开 Add/Remove 入口先检查
provider 与上下文，再取得现有实例锁并检查 HTTP 的 engine、resolver、verifier。
正常交付的传输具备三者；HTTP Unit 在退休 BUSY/ERROR 时也会撤销后两者，
因而已开始清理的实例不能继续记录操作。检查先于已拥有 Add 复用、槽位预留、
空记录 Remove、签名和 HTTP 调用。空 provider 返回 ARGUMENT/ARGUMENT；
已释放或清理未完成返回 STATE/STATE，错误域为 xrt.acme.dns，新稳定代码为 7，
既有 1..6 不变。公共错误契约、README、聚合头与 API 参考同步。

本轮未改变私有上下文布局、五家 ProviderUnit 实现或 HTTP 退休协议；仍保留
原始诊断、BUSY/ERROR 上下文与重试拥有权，完成后上下文置空，借用引擎继续
运行。最小 Validate 保持允许自定义空上下文。宿主必须停止新调用、等待已有
调用结束，并串行执行 Unit；入口校验不提供 Unit 与回调并发销毁的安全保证。

原公开聚合头探针的五家 × NULL/已释放 × Add/Remove 共 20 项均为真实
访问冲突（0xC0000005）；历史二进制、源码与结果哈希继续保留。本轮聚合头
40 项精确断言覆盖这 20 项及借用引擎的同一矩阵，使用实际公开构造和系统
信任库，不发送网络请求。模块化探针的 135 项精确断言另覆盖实际引擎 Pin
造成的 Unit 超时、注入退休 ERROR、重试/重复 Unit、借用边界及三种缺失
传输资源，检查已拥有记录和未跟踪 Remove。每次调用先设置不同错误哨兵；
拒绝路径的 HTTP、SHA/HMAC、nonce 计数均为零，记录/凭据/HTTP 状态逐字节
保持，最后活动分配、非法释放及重复释放归零。正常提供商请求仍由既有独立
TLS 服务和实际网络实现验证。

额外隔离对照用当前冻结测试及 Core 输入编译修改前的五家回调，实际 Unit
超时/退休 ERROR 的 10 个实例均拒绝通过状态契约断言；修复后的同样 10 个
对照均通过。过滤夹具首版在报告错误后继续调用旧的已释放回调，复现已知
访问冲突；已保留夹具/二进制并修正为在清理状态断言失败后清理并终止，
没有将该夹具错误或编译失败冒充状态契约的红灯。

最终快照 provider-lifecycle-final-freeze-20261002 有 7004 个输入文件，指纹为
4d7127c97f517b333d8b94952ca1c18c4e4492633f40a9d502127af31345377d。
Windows GCC、Linux GCC、Linux Clang ASan/UBSan 各通过 135 项模块化断言、
40 项公开聚合头断言和全部 696 项实际 provider TLS 场景（Ali 124 / AWS 77 /
Cloudflare 134 / Tencent 179 / Huawei 182）。Windows/Linux 各通过 27 个模块
程序、原公开单头测试和实际 Obtain 单头范例；10 个需要外部配置的入口仍
显式 SKIP，独立 696 项没有 SKIP。

Linux 首轮的 huawei/rest-owned-read-inactive-POLICE 已完成预期请求与删除、
stderr 为空，但客户端在后续退出检查返回 1。独立集中重复及验收重跑内的
20 次集中重复都通过；完整重跑沿用同一二进制，696 项全部通过。本轮保留
首次失败日志，未修改、放宽或自动重试该场景；尚不能确定偶发退出失败的
原因，作为长期任务的清理问题继续追查。

所有冻结输入在执行前后保持哈希。最后复核时当前工作区的运行时、测试、
清单、生成文件与相关工具仍匹配快照；工作单补记为单独文档差异。逐项案例、
日志和二进制 SHA256、10 红/10 绿对照、原 20 个访问冲突与首轮失败说明见
out/goal-temp/provider-lifecycle-evidence.json。
日志为 provider-lifecycle-full-windows.log、provider-lifecycle-full-linux-retry.log、
provider-lifecycle-full-linux-clang-asan.log、provider-lifecycle-modules-{windows,linux}.log；
首次 Linux 日志 provider-lifecycle-full-linux.log 保留为未通过的一次执行。

本轮没有重新测量覆盖率，历史十四份归档仍属于各自原输入。真实云账号、
托管 CI 和七库完整生产验收仍待完成。


## 2026-10-02：腾讯云完整 TXT 属主发现与退出诊断

腾讯云原实现从 TXT 属主的第一层父域开始探测，因而跳过属主自身的托管
域名；记录身份核验也仅接受相对主机记录。现在每次新建从完整规范化 TXT
属主查询 DescribeDomain，仅准确的域名缺失/无效码允许逐级回退。命中属主
自身时按 CreateRecord 的 SubDomain=@ 编码创建、重复 Add 核验与 Remove
核验；顶点匹配要求完整属主等于原域名，其他相对记录仍要求准确的标签边界。
已有记录始终携带创建时保存的 Domain/DomainId/RecordId，不跟随区域变化
移动清理身份。相对记录缓冲可容纳已验证的属主长度，取消原 200 字节局部
限制。公开契约、README、聚合头和 API 参考同步。

20 项独立真实 TLS 回归核对完整探测顺序、父子区域新增/撤销/ID 替换、
完整属主区域、大小写与 253 字节顶点属主；准确 DomainInvalid 回退、无域名
覆盖旧错误；权限、503、畸形/NUL、错误域名与缺失 RequestId 均停止发现和
写入，清空错误后同一实例恢复。顶点属主、DomainId、TXT 值不符及实际
JSON 分配失败保留原句柄，不发送删除，正确读取后可恢复清理。冻结旧实现
未通过全部 20 项：有实际客户端失败及独立服务端查询顺序断言失败；长属主
旧实现发出未预期的父候选，被独立服务拒绝。原实现、夹具、诊断及二进制
继续归档，没有把编译错误计作上述旧实现回归。

请求体探针仍保留 HTTP 应答替换和真实缓冲分配故障，但以实际 HTTP
初始化持有 engine/resolver/verifier，以符合公共回调的新生命周期契约。
额外的完整属主查询也逐项校验 JSON 长度、非零尾部、补 NUL 分配失败及
无请求/无登记/保留待删记录，故障位置仍分别对应发现、创建和删除。
最初初始化调用漏传 CA 参数，三平台夹具编译失败，保留日志且不计通过；
修正仅涉及该探针，后续完整验收来自新的最终冻结输入。

最终快照 tencent-zone-final-freeze-20261002 包含 7004 个输入文件，指纹
79fafed68d45c142ce2fa003da98d111b74e595f5c52970d6323a3817a93b177。
Windows GCC、Linux GCC、Linux Clang ASan/UBSan 每种配置先通过新增 20 项，
再通过全部 716 项 provider TLS 场景（Ali 124 / AWS 77 / Cloudflare 134 /
Tencent 199 / Huawei 182），并通过 135 项模块化生命周期、40 项公开聚合头
生命周期和 18 项请求体回归。全部实际执行，没有跳过，sanitizer 未报告
内存、泄漏或未定义行为错误。

Windows/Linux 模块套件各通过 27 个程序、原公开单头和实际 Obtain 单头范例。
这两套执行使用先前候选快照，所有实际构建输入与最终快照逐字节相同；
全快照唯一差异是未由模块清单或该范例使用的请求体探针 CA 参数修正。
未为此重复相同模块构建。十个依赖外部配置的入口仍明确 SKIP；真实云账号
与托管 CI 尚未验证。云端子域名托管受套餐和委派配置限制，本地 API 契约
验证不证明具体账户允许托管任意子域名。

provider wire 客户端在失败时报告 Unit 后残留上下文及错误、待清理数量、
内存计数与活动分配源位置，仍在原时点执行严格退出检查，不加入延时或
自动重试。带诊断的独立冻结版本对既有 Huawei/POLICE 场景重复 250 次，
全部通过；首次 Linux 退出失败和同一旧二进制重跑记录继续保留。尚未
得到其根因证据，没有根据线程退出时序猜测修改 Core 或认定问题已解决。

原红灯、所有失败尝试、三种最终配置、两套模块/范例结果、250 次集中诊断
复测及源码/日志/二进制 SHA256 见 out/goal-temp/tencent-zone-evidence.json。
日志为 tencent-zone-red-linux.log、tencent-zone-green-final-{windows,linux,
linux-clang-asan}.log、tencent-zone-modules-{windows,linux}.log、
provider-exit-diagnostics-linux.log 和 tencent-zone-generated.log。
复核时当前运行时、测试、清单、生成文件与工具匹配最终快照；仅本工作单
补记不同。没有重新测量覆盖率或改写此前十四份归档，长期目标保持 active。

## 2026-10-02：TLS 已认证关闭的延迟 END 与交付头刷新

刷新发现邮件四库的聚合实现头与声明头落后于当前 Core，重新生成了八个
交付文件。该基线冻结为 seven-runtime-refresh-freeze-20261002，7004 个
输入文件，指纹 938c19c8c3811ab481fbee9bf84abe670871a43fa57582a28592b9d0a97a71b4。
Core、ACME 和邮件四库的生成一致性通过；Windows 五套产品的单头、裁剪、
静态与共享消费者，以及实际 ACME Obtain 单头范例通过。

基线得到十三份有效覆盖率报告，邮件四库双平台均通过原始输入、对象
profile 和计数复核；ACME 双平台完整测量也通过。第十四份 Windows OAuth2
报告没有发布：实际独立 TLS close-body 场景读取失败。原始失败及部分
profile 保留，不用局部重跑补成完整通过。一次临时工具验收的模块导入
命令不符合现有测试脚本的直接执行方式，修正验收命令后双平台工具自测
通过；原命令失败日志也保留，库源码不受该命令修正影响。

失败二进制最初单独重复 50 次均通过，不能据此认定无问题。进一步诊断
实际复现了 TLS 状态已为 CLOSED、明文已交付，但 END Future 为 CLOSED
而非 RESOLVED 的状态组合。Core 的 CLOSED 由双向 close_notify 和发送
密文排空确认；密文排空同步重入最终关闭可能先于 End 事件记账。
HTTP 适配器正确要求认证 EOF，但原 Future 判断过度依赖事件记账。

src/tls/stream_future.c 现在对已认证 CLOSED 且无剩余明文的 END 等待
返回成功，不改变 FAILED、取消或未认证断流的拒绝路径。确定性回归使用
真实 TLS 连接和完整关闭，在 Engine 已销毁后仅清除 End 事件记账，
既不伪造认证状态，也不与 Worker 竞争。原运行时稳定失败（exit 1）；
修复后 Windows/Linux 模块化及单头、Windows IOCP 均通过。新增单头
生命周期测试已注册到 tls_stream_future_tests 清单。

修复后的 Core、ACME 与邮件四库聚合头再次同步，冻结为
tls-late-end-fixed-freeze-20261002，7005 个输入文件，指纹
77cb6d2991d94afb19e41667babaefc392327634ac62f7ef93f65d531c499a41。
Windows 17 个、Linux 16 个 TLS/Future 模块和单头程序通过；同一诊断
构建方式在 Windows/Linux 各重复 300 次真实 TLS 正常关闭，1/13/16384
字节分片各 100 次，全部通过。Windows ACME 六项 EOF 场景通过，覆盖
认证关闭、异常 TLS EOF、认证但不足长度/未完 chunk，以及普通 TCP EOF。
Linux Clang ASan/UBSan 的 OAuth2、ACME 全部独立互操作通过，包含 716
项 provider TLS 场景（Ali 124 / AWS 77 / CF 134 / Tencent 199 / Huawei 182），
sanitizer 未报告内存、泄漏或未定义行为错误。

当前修复版本的认证库双平台完整测量均通过，报告范围仍为自有 C 文件：

| 库/平台 | 覆盖行 | 覆盖分支结果 | 测量范围 |
| --- | --- | --- | --- |
| JWT Windows/Linux | 1128/1209，93.30% | 800/1080，74.07% | 主测试 |
| OAuth2 Windows | 1143/1239，92.25% | 807/1044，77.30% | 主测试及独立 HTTP/TLS、生命周期、未交付对象拥有权 |
| OAuth2 Linux | 1143/1239，92.25% | 806/1044，77.20% | 同上 |

十三份基线报告保留其原运行时绑定，不能改写哈希作为当前版本的报告；
当前版本仅新增上述四份认证库报告。3818 个原始 gcno/gcda 文件的哈希、
两组冻结输入、日志、确定性红灯、600 次复测及完整验收元数据见
out/goal-temp/seven-refresh-late-end-evidence.json。复核时当前输入与修复
快照相同，仅本工作单的补记不同。历史 Huawei/POLICE Linux 退出检查
失败继续独立跟踪，没有证据证明它与本次 TLS END 故障同源。

七库生产验收仍未完成：当前运行时的 ACME/邮件覆盖率与完整交付需刷新；
邮件独立网络探针和私有静态头函数尚未纳入覆盖范围；剩余 provider、
输入/OOM、宿主关闭/恢复、存储故障路径、真实服务和托管 CI 验收继续推进。

## 2026-10-02：邮件独立 TLS 覆盖合并与私有头函数测量

邮件覆盖率现在默认完整执行模块测试及真实 POP3/SMTP/IMAP 范例的独立 TLS、STARTTLS 探针。xmail 测量三协议，其他库只测对应协议；覆盖 DNS、IPv4、可用时的 IPv6、错误 IP 身份及截断响应。每个平台实际执行 60 项（xmail 30，其余各 10）；执行记录及二进制哈希缺一不可，失败不发布报告。各产品、平台及探针有独立目录和计数器；完整产品特性保持与模块套件一致。仅当源码与 CFG 相同才合并，同一行/分支分母不随重复执行增加。

| 库 | Windows 行 | Windows 分支结果 | Linux 行 | Linux 分支结果 |
| --- | --- | --- | --- | --- |
| xacme | 5344/6121，87.31% | 4274/5945，71.89% | 5344/6137，87.08% | 4286/5965，71.85% |
| xmail | 4522/5715，79.13% | 3149/4824，65.28% | 4526/5719，79.14% | 3149/4824，65.28% |
| xpop3 | 669/869，76.99% | 410/680，60.29% | 669/869，76.99% | 410/680，60.29% |
| xsmtp | 806/983，81.99% | 528/826，63.92% | 806/983，81.99% | 528/826，63.92% |
| ximap | 2209/2755，80.18% | 1455/2355，61.78% | 2210/2755，80.22% | 1455/2355，61.78% |
| xjwt | 1128/1209，93.30% | 800/1080，74.07% | 1128/1209，93.30% | 800/1080，74.07% |
| xoauth2 | 1143/1239，92.25% | 807/1044，77.30% | 1143/1239，92.25% | 806/1044，77.20% |

这些数字的范围仍为各库自有 C 文件；CI 下限是回归门禁，不是生产充分性证明。ACME 的测量完整执行模块、独立 HTTP/TLS/拥有权/五家 DNS provider 716 场景和九项 mock CA；每平台十项需线上环境的模块测试明确 SKIP。认证库沿用已完整执行、原始输入及 profile 未变化的报告，未重写绑定。

私有头函数单独使用原始报告已绑定的产品对象 profile。按函数核对源码、范围、基本块和分支标识，允许不同编译单元只发射一部分函数，拒绝 CFG 不同、计数畸形或未发射的库存函数。同一静态函数在多个对象中只计一次；不计入主 C 文件指标，也不额外计入范围外的依赖库副本。当前清点的是普通 static C 函数定义；未来属性或宏生成定义须先扩展采集支持。

- Windows xmail：10 个函数，50/53 行，50/76 分支结果。
- Linux xmail：10 个函数，50/53 行，50/76 分支结果。
- POP3/SMTP/IMAP 的自有私有头当前为声明及桥接，没有对应 static 实现，标记不适用；共享辅助函数归属 xmail。

双平台各通过 gcov 合并 10 项、邮件原始输入/实际对象 19 项、认证来源 3 项及新增头函数 13 项校验（各 45 项）。其中头函数测试用真实 GCC 对象证明不同发射子集与正反分支可合并且分母不重复，并拒绝过期源码、遗漏函数、不同 CFG/编译器、伪造计数及旧原始报告。report-only 复算保留原报告字节，不重新构建或替换旧输入哈希。

Linux/Windows CI 已接入完整测量及两份报告的原始计数复核；工件保留报告、gcno/gcda、实际范例二进制及执行记录。Windows 覆盖率环境补齐 OpenSSL，YAML 结构本地复核通过；尚未把配置修改认定为托管 CI 已运行成功。

本阶段基础输入冻结为 mail-independent-coverage-freeze-20261002，7005 个文件，指纹 e6a0a98eccfc65aca51f93b58d1c72cba40852a0cd461ed1d44bb1a0a40134f2；新增的三个头函数采集/验证工具另有独立输入哈希。十四份报告、八份头函数报告、全部执行元数据与日志及 7798 个原始 gcno/gcda 哈希见 out/goal-temp/mail-independent-coverage-evidence.json。表中数值描述冻结的修复运行时；最终核对发现以下工作区输入在测量期间出现后续变化，不能据此认定当前工作区已验收：

- `include/xrt/pattern.h`
- `src/internal/xrt_pattern.h`
- `src/text/pattern_builder.c`
- `single/xrt.h`
- `single/xrt_decl.h`

七库生产验收仍未完成：当前运行时的完整单头/裁剪/静态共享交付须继续刷新，邮件压缩及剩余私有头分支、ACME/JWT 私有头函数度量、provider 输入/OOM、宿主关闭/恢复和存储故障路径仍须审计，真实服务和托管 CI 仍待验证。此前 Huawei/POLICE Linux 退出检查失败未确定根因；本轮全量通过不撤销该记录。

## 2026-10-02：Pattern/Core 输入刷新与七库完整交付

当前 Core 生成头已同步；ACME、xmail、POP3、SMTP、IMAP 的实现头及声明头共十个文件过期，重新生成后六份清单的一致性检查通过。以此冻结 7009 个基础输入，指纹为 dd6bff08e4f26034fa7abb496b3a784fe3e76bb578b9cd1dc0637f95aec6cd62。Windows 在 out/goal-temp/pattern-core-refresh-freeze-20261002 执行，Linux 使用独立原生目录 /root/xrt-goal/pattern-core-refresh-20261002，避免包工具的平台无关对象目录相互覆盖；Linux 的源码、原始对象和产物按字节归档至 out/goal-temp/pattern-core-linux-archive-20261002，报告保留原始编译路径。

七库两平台均重新完整测量，来源和计数均独立验证，不改写旧阶段绑定：

| 库 | Windows 行 | Windows 分支结果 | Linux 行 | Linux 分支结果 |
| --- | --- | --- | --- | --- |
| xacme | 87.31% | 71.89% | 87.08% | 71.85% |
| xmail | 79.13% | 65.28% | 79.14% | 65.28% |
| xpop3 | 76.99% | 60.29% | 76.99% | 60.29% |
| xsmtp | 81.99% | 63.92% | 81.99% | 63.92% |
| ximap | 80.22% | 61.78% | 80.18% | 61.78% |
| xjwt | 93.30% | 74.07% | 93.30% | 74.07% |
| xoauth2 | 92.41% | 77.39% | 92.25% | 77.20% |

范围仍为各库自有 C 文件；八份邮件私有头报告保持独立口径。每个平台实际执行邮件独立 TLS 60 场景、五家 DNS provider 716 场景及 mock CA 九项。每平台十项依赖外部环境的 ACME 模块测试显式 SKIP。认证独立/最小测试及范例每平台九个程序、RFC 5958 RSA/EC 的 OpenSSL 互操作均通过。五库的单头、裁剪、静态和共享包消费及登记的 ACME Obtain 实际签发范例均通过。

新增 tools/test_auth_package.py 及两个认证库包消费者，另以三个输入哈希绑定到冻结快照。分别编译 JWT/OAuth2 与一个 Core 运行时，验证静态及共享形式下的独立 JWT、独立 OAuth2 和两库组合，共双平台十二次执行；JWT 24 个、OAuth2 27 个公开导出准确匹配头文件。消费者由 Core 创建声明、释放库返回对象，并核对错误密钥导致的 Core 错误域和代码与 xjwtLastError 相同，证明库与调用方共享同一 Core 错误上下文。此门禁核查现有 C 源码交付方式，没有引入新的公开包装 API。CI 接入双平台消费者及产物上传，本地 YAML 核对通过；托管 CI 尚未运行验收。

初次 Linux 启动早于原生源码复制完成，四个进程因缺少入口脚本退出，未执行测试；等待复制进程完成后才重启。Windows 新版 objdump 的导出表格式导致第一版解析器拒绝实际正确的导出表，解析修正后重验通过。新增错误上下文断言初版把错误域误写成 xjwt，双平台按预期拒绝该错误断言；按实现约定 xrt.jwt 修正后全部通过。这些准备和测试工具失败的原始日志保留，未当作库实现故障或通过结果。

最终证据 out/goal-temp/pattern-core-refresh-evidence.json 核对十四份覆盖报告、八份邮件头报告、十二份执行元数据、7814 个原始 gcno/gcda 和 6789 个 release 文件的哈希。2026-10-02T12:04:13Z 核对时所有报告实际测量依赖与工作区一致；冻结快照与工作区的额外差异为未参与这些测量的 tests/pattern/test_pattern_edit.c 及本轮 CI 修改，已逐项记录。这个结论仅描述所列验收范围和输入，不代表整个仓库或生产验收完成。

下一步核查 ACME/JWT 私有头函数是否完整发射、邮件压缩和私有辅助函数未覆盖分支，继续 provider/OOM、宿主关闭及存储故障审计。真实 CA/DNS/邮件/OAuth 服务和托管 CI 尚未验收，历史 Huawei/POLICE Linux 退出检查失败仍未确定根因。

## 2026-10-02：ACME/JWT 私有头原始对象复算

新增 tools/measure_extension_header_coverage.py，复用已有静态函数库存与按函数合并器，不重新执行库测试，也不改写主 C 文件报告。首先从清单及独立探针的 C include 推导所属对象范围，以原始 gcno/gcda 复算主报告的完整源码集合、GCC/JSON 版本、每个文件的行/分支分母和未覆盖位置；完全一致后才统计私有头函数。ACME 的模块对象已累计 mock 执行，贡献顺序可能变化，因此核对最终覆盖集合而非复制旧贡献顺序。所有私有头均须已由原始依赖哈希绑定，库存函数必须全部发射，CFG 和输入须相同；不同编译单元中的重复函数只计一次。

| 私有头范围 | Windows | Linux |
| --- | --- | --- |
| ACME，28 个函数 | 260/293 行，88.74%；263/368 分支结果，71.47% | 259/292 行，88.70%；263/368 分支结果，71.47% |
| JWT，2 个函数 | 21/21 行，100%；19/34 分支结果，55.88% | 同 Windows |
| OAuth2 | 没有对应静态定义，不适用 | 不适用 |

范围为原始产品对象中的自有私有头定义，未改变主 C 文件的覆盖率。双平台 report-only 均从原始对象复算通过，原始主报告与计数文件字节保持不变。新增 13 项验证使用真实 GCC 对象，包含模块/独立嵌入的合并、重复执行分母、未发射定义、输入或计数在处理途中变化、伪造主/次报告、布尔假冒整数、重复 JSON 字段与非有限值、缺失 profile、不适用库存及后处理器变化；双平台均通过。

两个新工具另以输入哈希绑定到同一 7009 文件基础快照。六份次报告、原始主报告及 profile 身份、执行元数据和日志核对见 out/goal-temp/extension-private-header-evidence.json；Linux 次报告与工具按字节补充归档，不覆盖之前的原始产物。CI 双平台接入统计及 report-only，工件增加对应报告和原始 gcno/gcda，本地 YAML 核对通过；托管运行仍待验收。

统计暴露了明确的后续工作：xacmeDnsSplit 在原始对象中发射但没有执行，当前源代码检索未发现生产调用，需要核实并清理遗留实现；其他 DNS 辅助函数剩余输入/OOM 及容量分支、JWT RSA 参数防御分支仍需按实际调用路径审计。xmail 自有模块及独立 TLS 对象中 mail_net_deflate.c 仅 15/121 行、3/54 分支结果；已有 IMAP 压缩集成测试的 xmail 依赖对象未属于这份主报告，不能据此宣称压缩未执行或把未绑定对象混入已有分母。下一阶段先核对跨产品集成对象的输入与 CFG，再补压缩失败、取消、超时、解压上限和安装 OOM 场景。所有报告通过仍不代表生产验收完成。

## IMAP 压缩故障回归与邮件输入刷新（2026-10-02）

确认了生产行为缺陷：`xrtDeflateWrite(..., XDEFLATE_FLUSH_NONE, ...)` 可以只缓存正文而没有输出回调，原来的压缩 APPEND 因而跳过底层发送的取消和截止时间检查。TCP 与 TLS 下，预取消或已过期的 8 字节写入会返回成功并扣减剩余 literal 长度。修复在压缩入口、每个最多 16 KiB 的输入片段及返回成功前检查请求状态；解码推进和输出缓冲追加也检查请求状态。失败交给既有客户端终止逻辑，保留原始错误、最近完整回复及禁止复用的终态。

原实现双平台各执行 18 项独立用例：上述四项准确失败，其余十四项控制通过。Windows 原始二进制实际在 `D:\GIT\xrt` 编译，编译输入在当时的前后核对中与原始冻结快照一致；Linux 在原始本地快照编译。不能把 Windows 归档位置描述为它的编译位置。原始冻结输入 7,016 个文件，指纹 `d66ed5a7972f945a12d4cb9289c33d5a925edef125f5c40589f1801415b943e1`。

最终用例包含 TCP/TLS 各十种场景：缓冲写取消、缓冲写超时、部分解码回复后的取消/超时、非法 DEFLATE 块、截断回复、解码后线路超限、编码器和解码器安装 OOM，以及持续压缩流中的分段 40,000 字节 APPEND、结束和 LOGOUT。服务器逐字核对正文与命令边界；故障用例核对原始错误、最后完整回复、失败终态、禁止复用以及适用场景下销毁前的对端关闭。安装 OOM 在真实公开 COMPRESS 协商确认后注入其内部安装操作，精确验证两个构造点与无部分安装；不是整个公开 COMPRESS 调用的逐分配点矩阵。

Windows GCC 与 Linux GCC 的模块化和单头构建各执行全部 20 项并通过；Linux Clang ASan/UBSan 的模块化和单头也各执行 20 项并通过，启用 `detect_leaks=1:halt_on_error=1` 和 `halt_on_error=1:print_stacktrace=1`。最终冻结输入 7,016 个文件，指纹 `de9982eb27e99c785f345a102df92f504c0e792e6ad8a125adaaa46d3b61b372`，实际运行前后哈希一致。Core 与邮件四库生成检查通过，已同步 `xmail`、`xpop3`、`xsmtp`、`ximap` 的聚合头。公开 APPEND 文档说明成功写入可暂存于编码器、结束时刷新，以及失败后不能重试该会话。

四库主报告保持“自有 C 文件的模块套件与独立 TLS 范例对象”口径，均重新执行完整套件和独立 TLS 互操作，再复算报告。每个平台独立 TLS 范例执行 xmail 30 项、其他各 10 项，含 IPv6；八份私有头报告单列。该冻结版本验收时源码、测试和编译依赖与工作区一致，原报告没有重写为新输入；后续 Core/TLS 关闭改动另见下节。

| 库 | Windows 行 / 分支结果 | Linux 行 / 分支结果 |
| --- | --- | --- |
| `xmail` | 4522/5741（78.77%） / 3149/4846（64.98%） | 4526/5745（78.78%） / 3149/4846（64.98%） |
| `xpop3` | 669/869（76.99%） / 410/680（60.29%） | 669/869（76.99%） / 410/680（60.29%） |
| `xsmtp` | 806/983（81.99%） / 528/826（63.92%） | 806/983（81.99%） / 528/826（63.92%） |
| `ximap` | 2210/2755（80.22%） / 1457/2355（61.87%） | 2211/2755（80.25%） / 1457/2355（61.87%） |

共享 `mail_net_deflate.c` 另外按原始对象列出两个作用域。原输入中 xmail 模块套件为 15/121 行、3/54 分支，IMAP 集成为 105/121 行、32/54 分支；这些对象哈希已与上一轮封存的原始计数器核对。新输入中 xmail 模块套件为 15/147 行、3/76 分支，IMAP 集成为 133/147 行（90.48%）、55/76 分支（72.37%），两平台一致。编译控制流与源码在各自输入版本中核对后才作独立合并审计；不把依赖对象塞入既有主报告。新增代码增加了分母，因此 xmail 主报告比例下降。剩余 14 行、21 个分支结果用于后续补测，重点为解码输出分配失败、编码输出失败和缓冲输入边界。

CI 已新增 `Sanitize compressed IMAP failures and streaming uploads`，执行 `imap_compress_fault_runtime_tests` 的模块化与单头；YAML 与条件本地检查通过，托管运行尚未验收。初稿测试包装、过期聚合头和 Windows 启动 PATH 缺少 OpenSSL 的失败日志均保留。Windows 环境修正后完整重测，工具先清除旧计数器，再重新编译与执行；不把此前的环境失败当作完成验收。

本轮封存 `out/goal-temp/mail-compression-acceptance-evidence.json`：八份主报告、八份私有头报告、6,864 个原始 gcno/gcda 文件、六个压缩测试二进制、原始失败与控制结果、分离的压缩作用域、日志和辅助脚本哈希。Linux 最终归档 12,636 个文件，原始回归归档 7,193 个文件，逐字节验证，仅省略输出 `.o` 与 `.pyc`；保留本地原运行路径，归档目录没有重新执行或重定位报告。CI 和 API/工作单文档的后续变化不属于本轮编译输入。

当时的后续工作为刷新邮件包消费者、补独立 zlib raw DEFLATE 互操作和其余编解码/写入/读取 OOM 边界。此前 ACME、JWT 与 OAuth2 六份主报告和六份私有头报告保留原始文件与计数，在该次核对时依赖一致。最新输入状态与已完成的交付/互操作见下节；私有辅助函数、提供商与存储/关闭故障、真实服务、托管 CI 及历史 Huawei/POLICE Linux 退出失败仍待完成。

## 独立 zlib 互操作与 TLS 关闭尾部修复（2026-10-02）

新增 `tools/test_imap_compress_interop.py` 与 `tools/probes/imap_compress_client.c`，C 探针只使用公开 API；独立服务器使用 Python 标准库 SSL/zlib。模块化、单头分别覆盖明文、TLS、STARTTLS：stored/fixed/dynamic 块、FULL_FLUSH、9 位窗口、确认与压缩前缀合并发送、逐字节回复、协商 NO/BAD、非法块、zlib 包装、截断及解码后线路超限。服务器逐字核对 40,000 字节分段 APPEND，并验证持续压缩流；客户端拒绝空写片段后仍可完成上传。TLS 1.3 P-256 场景通过独立消息回调确认两个 ClientHello 与一个真实 HRR。完整 LOGOUT 回复之后的缺失 close_notify、关闭超时均须失败，保留最后 tagged OK 并禁止复用。

逐字节压缩 LOGOUT 暴露了实际关闭缺陷：客户端已经取得 BYE 和 tagged OK，后到的 DEFLATE 刷新尾字节仍在 TLS 明文队列中；读取背压挡住 close_notify，触发默认 5 秒关闭超时。独立时间记录显示命令完成约 2.36 秒，关闭失败约 7.37 秒，不是整次 20 秒调用预算不足。确定性回归先将 8,193 字节未消费明文放入真实 TLS 缓冲，原 Windows/Linux 实现均在客户端关闭处失败。

修复位于共享 `mail_net_tls.c`：请求认证关闭后按 ReadChunk 消费并丢弃剩余 TLS 明文，收到认证 EOF 后再等待连接关闭终态；不改变 Core TLS 的读取背压或公开 API。所有等待仍受调用方截止时间及 TLS 关闭计时器约束，截断、超时、Future/字节分配失败均保留原始错误并立即异常中止。普通接收仍不能把 EOF 当作成功结果。明文 TCP 若在最终空刷新块的最后五字节内遇到复位，只在客户端独立确认完整 tagged OK 后接受；TLS 始终要求独立服务端认证关闭，之前任何命令或 literal 错误仍拒绝。

严格 GCC O1 最小单头编译另外触发 Core ClientHello 尺寸变量告警。尺寸辅助函数的成功路径已经赋值，没有证据表明运行时读取了未初始化值；将本地变量显式初始化为零后告警消失。Core 单头及九个包含该实现的扩展聚合头均重新生成并检查；共享关闭改动同步到四个邮件聚合头。四库 README 已纠正覆盖率口径，独立互操作不混入原有覆盖率分母。

| 配置 | 公开 API 独立互操作 | 原有压缩故障执行 | 共享 TLS 回归 | 终态 |
| --- | --- | --- | --- | --- |
| Windows GCC | 90 项，两种构建 | 两种构建各 20 项 | 真实读写/取消、握手失败、STARTTLS 排队、关闭 OOM 与未消费尾部 | exit 0 |
| Linux GCC | 同 Windows | 同 Windows | 同 Windows | exit 0 |
| Linux Clang ASan/UBSan | 同 Windows | 同 Windows | 同 Windows，含泄漏检测 | exit 0 |

共 270 项互操作、120 次压缩故障执行与 18 个 C 回归二进制通过；sanitizer 启用 detect_leaks=1、halt_on_error=1 及 UBSan 栈记录。随后双平台四库单头、裁剪、静态和共享包消费者各 16 项全部通过，验证 2,580 个 Windows、2,576 个 Linux 发布文件。此前压缩修复版本的交付也另外核对并封存，不混同新输入。

最终冻结 7,018 文件，指纹 `afe3a441af07164608f505ef23a997965d8a1625ceaef798ad5d75d6ba2ef3e8`，各运行前后源码哈希一致。Windows 实际运行目录为 `out/goal-temp/mail-zlib-close-fixed-final-20261002`，Linux 为 `/root/xrt-goal/mail-zlib-close-fixed-final-20261002`；原实现回归指纹 `0c763e7e54ab1983fed4557c58586c7a32d785474c4725f1d42a799ba5739844`，相同尾部测试的两次预期失败保留。Linux 10,822 文件按字节归档到 `out/goal-temp/mail-zlib-close-linux-archive-20261002`，包含源码、全部发布对象与产物、正常/sanitizer 二进制、执行记录和原始失败二进制；归档不重新运行，不重写实际运行路径。

封存证据为 `out/goal-temp/mail-zlib-close-acceptance-evidence.json`。最初失败的分片矩阵、探针权限/空片段/SNI 预期修正及编译告警日志保留；证据核对器处理共享 stdout 造成的编译行与首个 PASS 行合并，仍精确要求每种压缩故障执行两次，不减少用例数。初次归档排除了发布对象，已补齐并保留初次清单；最终逐项哈希核对通过。CI 已配置独立互操作普通与 sanitizer 两次执行，尚无托管运行验收。

当前十四份主报告与对应私有头报告仍属于之前的源码输入；Core 单头及邮件传输已改变，需要重新执行并按自有源码口径复算，不能据旧百分比判定新实现覆盖率。后续继续补编解码/输出/读取分配失败与私有辅助分支，审计提供商、宿主关闭/恢复、存储故障与组合消费者；真实服务、托管 CI 及历史 Huawei/POLICE Linux 退出失败未解决。长期任务保持 active，生产验收未完成。

首次封存核对时工作区与上述冻结源码完全一致；最终工作单更新期间 `src/system/time.c` 的月份加法错误诊断又发生改动，不属于本轮实际编译输入。下一阶段先同步 Core/依赖聚合头并核验该新输入，再刷新当前工作区基线与交付；冻结运行结果和原始日志保持原版本，不据此宣称当前所有输入已通过。


## 2026-10-03 · Core 日历边界与七库当前输入基线

接续上轮 Core 日历错误诊断变化，完成当前输入的双平台基线与交付核验。最初使用底层分配器的一次失败测试被小对象池绕过，故障未触发，其失败日志不作为实现缺陷证据；改用 `xrtMemDebugFailAfter(0)` 并明确断言 `xrtMemDebugFailTriggered()` 后，原实现稳定把首次错误构造的 OOM 覆盖为 RANGE。当前实现直接传播辅助函数的诊断，并由辅助函数负责索引算术溢出，避免重复分配和覆盖首个错误。

同一测试加入年零一月加 `INT64_MIN` 月的边界，Linux UBSan 确认旧月份公式的向下取整商乘以 12 会溢出。月份改为规范化 `% 12` 余数后，边界安全返回 OVERFLOW，输出保持原值，首次诊断 OOM 仍被保留。测试使用单独的 `time_error_oom_tests` 根依赖 `time,memory_debug`，不扩大生产 `time` 模块依赖；Core 与九个依赖聚合头重新生成并检查，CI 增加时间算术/错误诊断 sanitizer 门禁，YAML 本地解析通过。

Windows GCC、Linux GCC、Linux Clang ASan/UBSan 各执行 9 个时间测试程序，共 27 个程序通过。模块与单头合计执行 42 个常规溢出控制、42 次逻辑分配故障；检查错误类型、原输出和释放后的分配状态。Linux sanitizer 启用泄漏检测及错误即停。两个原实现失败的二进制、源码、日志与哈希单独保留；Linux 溢出原实现按字节封存 7,039 文件，实际运行路径不重写。

重新执行七库 14 份自有 C 主覆盖率报告，从同批原始对象计数器复算并核对 14 份私有头报告。二者分母分开，不把 Core、其他库或私有头执行计入自有 C 百分比。以下为行覆盖率 / 分支结果覆盖率（%）：

| 库 | Windows | Linux |
| --- | --- | --- |
| xacme | 87.31 / 71.89 | 87.08 / 71.85 |
| xmail | 78.82 / 65.01 | 78.83 / 65.01 |
| xpop3 | 76.99 / 60.29 | 76.99 / 60.29 |
| xsmtp | 81.99 / 63.92 | 81.99 / 63.92 |
| ximap | 80.22 / 61.87 | 80.25 / 61.87 |
| xjwt | 93.30 / 74.07 | 93.30 / 74.07 |
| xoauth2 | 92.41 / 77.39 | 92.25 / 77.20 |

私有头单独记录：ACME 28 个函数，Windows 260/293 行、Linux 259/292 行，两个平台均 263/368 分支结果；JWT 2 个函数、21/21 行、19/34 分支结果；xmail 10 个函数、50/53 行、50/76 分支结果。POP3、SMTP、IMAP、OAuth2 没有自有私有静态函数定义，标记不适用，不宣称 100%。尚未调用的 ACME `xacmeDnsSplit` 等缺口保留为审计项，不通过直接调用未用代码提升指标。

双平台 ACME 与四邮件库分别通过单头、裁剪、静态及共享包的 20 项交付命令，另运行公共 ACME Obtain 范例门禁。JWT/OAuth2 各平台通过 9 个独立/最小/组合测试及范例、RFC 5958 OpenSSL 互操作；静态与共享的独立/组合消费者每个平台共 6 个通过，共用一个 Core 实例，严格核对 JWT 24、OAuth2 27 个共享导出。双平台 ACME 独立提供商 TLS 请求矩阵均完成，九个模拟 CA 场景全部通过、不允许跳过；模块内每个平台另有 10 个缺少外部配置的测试明确跳过，未计为成功。

冻结 7,020 文件，指纹 `63162ba9c60875f3472895e38bdd3309f33f7760439f23b5efe4f73538e5a2e5`。Windows 实际运行目录 `out/goal-temp/seven-core-time-refresh-20261003`，Linux `/root/xrt-goal/seven-core-time-refresh-20261003`。每个阶段运行前后源码哈希一致。Linux 按字节归档 18,452 文件至 `out/goal-temp/seven-core-time-linux-archive-20261003`，包括全部源码、原始计数器、正常/sanitizer 二进制、私有头报告、包对象与消费者；保留实际运行路径，归档不执行。另验证 3,395 个 Windows、3,394 个 Linux 发布文件。封存证据 `out/goal-temp/seven-core-refresh-acceptance-evidence.json`；14 份主报告依赖均与当前工作区一致，旧报告和上轮独立 zlib 矩阵仍按原输入保留，不重绑定到本轮。

后续优先压实压缩读取缓冲扩容/发送尾部 OOM 与解析器边界、私有辅助分支、ACME 流程/存储故障以及宿主关闭/恢复。根据受影响库重验，避免将无关源码变化变成重复的七库全量测试。真实服务与托管 CI 尚未验收；历史 Huawei/POLICE Linux 退出失败本轮未再现，但根因仍未确定，不据成功复跑判为已解决。长期任务保持 active，生产验收未完成。本节为验收后的文档追加，冻结源码与原始报告不修改；再次核对时仅本文件与冻结版本不同。


## 2026-10-03 · 压缩传输分配故障与互操作工具边界

本轮扩展现有 COMPRESS 故障用例，核验解码 Pending 初次分配、非空缓冲扩容、实际编码输出发送分配及 APPEND 结束刷新分配失败。读取故障在公开 COMPRESS/NOOP 后，把真实 TCP/TLS 响应预读到测试持有的压缩前缀，再在调用线程注入一次逻辑分配失败；避免先命中接收 Future 的分配。两个无故障控制逐字检查 4096 字节响应、缓冲确已扩容、完整 NOOP 和认证 LOGOUT，不能把这些定点注入描述为公开调用全分配点遍历。

发送用例使用 40,000 字节 APPEND。正文发送失败必须保留剩余 literal 长度；最终刷新故障先要求服务端实际解码非空正文前缀，再检查关闭前已排队数据仍是准确正文前缀、没有结束 CRLF 或后续命令。未要求网络上已部分发送的正文回滚。所有故障断言注入确已触发、原始 MEMORY 错误、最近完整回复、失败终态及拒绝复用；释放会话与引擎后核验逻辑分配已清空。上述生产路径已正确处理这些故障，本轮未修改库实现、公开 API 或聚合头。

每个模块化/单头二进制执行 TCP/TLS 合计 32 项（原有 20 项、新增 8 项故障与 4 项控制）。Windows GCC、Linux GCC、Linux Clang ASan/UBSan 合计通过 192 次 C 场景执行，其中新增定点故障 48 次、读取控制 24 次。sanitizer 启用泄漏检测与错误即停。三个配置各执行完整 90 项独立 SSL/zlib 互操作，另各实际编译并执行一个根目录之外的输出目录用例；共 270 项完整矩阵与 3 项外部目录执行通过。

修复 `tools/test_imap_compress_interop.py` 两个真实工具缺陷：明文单选 TLS 专属 HRR/close-truncated/close-stall 过去会成功提交零场景记录，现在在建目录和编译之前拒绝；外部输出目录过去在写 binary SHA256 时因 relative_to(ROOT) 报错，现在保存实际绝对路径，内部路径保持原格式。新增两项工具测试（空选择含三个子场景），原工具为 3 failures / 1 error，修复后双平台通过。外部路径的模拟单测仅验证工具格式，实际编译执行的三个用例单独提供 C 验收证据。CI IMAP 矩阵接入该工具回归；YAML 本地解析与条件核对通过，托管运行未验收。

四邮件库重新执行完整模块套件及独立 TLS 范例，产出八份自有 C 主报告和八份私有头报告；两类 report-only 均从同批原始对象计数器复算通过。主报告口径不变，行覆盖率 / 分支结果覆盖率（%）如下：

| 库 | Windows | Linux |
| --- | --- | --- |
| xmail | 78.82 / 65.01 | 78.83 / 65.01 |
| xpop3 | 76.99 / 60.29 | 76.99 / 60.29 |
| xsmtp | 81.99 / 63.92 | 81.99 / 63.92 |
| ximap | 80.25 / 61.87 | 80.25 / 61.87 |

共享 `mail_net_deflate.c` 单列对象审计：xmail 自有模块双平台均为 15/147 行、3/76 分支结果；IMAP 集成对象双平台均为 135/147 行（91.84%），分支结果 Windows 57/76（75.00%）、Linux 56/76（73.68%）。该依赖对象不混入 xmail 主报告，剩余 12 行和 Windows 19 / Linux 20 个分支结果继续按实际调用路径审计。私有头仍分开统计，POP3/SMTP/IMAP 的不适用范围不宣称 100%。ACME/JWT/OAuth2 六份主报告及六份私有头报告依赖与当前工作区逐项核对一致，保留上一轮原始报告及源码指纹，没有重写为新输入；七库上轮交付的生产源码也完全相同，发布产物与执行记录哈希再核验通过，本轮不重复无关交付。

最终冻结 7,021 文件，指纹 `dc5be15021030f857dc1da4f2beb7772ab3efc22ba8d20bcb05ad273c2b08be4`。Windows 实际运行于 `out/goal-temp/mail-compression-io-oom-20261003`，Linux 于 `/root/xrt-goal/mail-compression-io-oom-20261003`；各阶段源码运行前后哈希一致。Linux 15,806 文件按字节归档至 `out/goal-temp/mail-compression-io-linux-archive-20261003`，保留全部源码、对象、计数器、正常/sanitizer 二进制及外部目录工件与原路径映射；归档没有执行或重定位报告。额外对象作用域复算报告按原始 profile 哈希绑定到该归档，封存证据为 `out/goal-temp/compression-io-oom-acceptance-evidence.json`。两个 Windows 测试开发预检保留为探索性记录，不代替冻结输入验收。

初次证据核对因 WSL stdout/stderr 合并覆盖单测摘要而失败，日志保留；独立重跑两项轻量工具单测，统一输出后完整记录通过摘要，并核对原始完整驱动的 exit 0。没有为日志问题重跑 C 验收，也没有按相同分支计数改写双平台实测结果。

下一阶段继续解析器输入边界、未用/私有辅助实现、ACME 流程与存储故障及宿主关闭/恢复审计。历史 Huawei/POLICE Linux 退出失败仍未确定根因，真实服务和托管 CI 尚未完成；长期任务保持 active，生产验收未完成。本节为验收后的文档追加，冻结源码与原始报告保持原字节；再次核验时仅本文件与冻结版本不同。


## 2026-10-03 · BODYSTRUCTURE 诊断传播、字段边界与借用输出

原冻结实现分别在 Windows/Linux 复现：13 个解析错误场景中，12 个在一次逻辑分配故障确已触发后，把底层 MEMORY 替换成父层 PROTOCOL；无故障的 uint64 溢出也把 RANGE 替换成 PROTOCOL。未知扩展空列表原本能保留 MEMORY，作为控制保留。修复把读取/子解析失败与成功后的类型检查分开，直接传播下层诊断；成功仍不清除调用方已有错误。正常解析保持零分配，错误对象构造可能分配，失败输出保持原字节。游标迭代的语义失败不承诺整个底层游标位置回滚。

两个平台的原实现另外都接受七种缺少 SP 的非法字段连接：类型、参数、处置、语言、位置、未知扩展及 multipart subtype。BODY 层现在按 [RFC 9051 §9](https://www.rfc-editor.org/rfc/rfc9051.html#section-9) 校验字段分隔，保留 multipart `1*body` 允许的相邻子列表；通用 IMAP 数据扫描器未修改。直接子部分/参数输出不允许覆盖所借用的输入文本，检查在消费游标之前执行；子游标初始化检查覆盖整个游标对象。

新增测试根 `imap_body_error_tests` 只在测试闭包引入 `memory_debug`，生产 `imap_body`、`ximap` 没有新增该依赖。每个模块化/单头二进制执行 53 项：20 个错误场景各有正常/OOM 对照，三个成功且保留已有诊断的零分配控制，三个游标错误和两个借用输出重叠场景各有正常/OOM 对照。Windows GCC、Linux GCC、Linux Clang ASan/UBSan 合计 318 次新增场景全部通过，另有普通 IMAP 数据/BODYSTRUCTURE 测试；总计 18 个程序，sanitizer 启用泄漏检测与错误即停。CI IMAP 矩阵加入专门 sanitizer 步骤，YAML 与生产/测试闭包本地核对通过，未声称托管 CI 已运行。

ACME 的私有静态 `xacmeDnsSplit` 只有源定义和生成头中的副本，没有调用或取址引用；历史文档提及单独登记。原执行 profile 中该函数为 14 行、12 个分支结果，均未执行，现删除源定义并再生成聚合头。Windows 私有头由 28 个函数、260/293 行、263/368 分支结果变为 27 个函数、260/279 行、263/356 分支结果; Linux 私有头由 28 个函数、259/292 行、263/368 分支结果变为 27 个函数、259/278 行、263/356 分支结果。这属于移除死代码造成的分母变化，不声称增加了执行覆盖；ACME 自有 C 主口径不掺入私有头。

ACME 与邮件四库重新运行双平台完整模块、独立服务/TLS 范例及 ACME 九个 mock CA 场景，刷新十份自有 C 主报告、十份独立私有头报告；对应私有头/邮件 report-only 从同批原始对象复算通过。JWT/OAuth2 的四份主报告和四份私有头报告经当前依赖哈希核对仍有效，保持原报告、profile 与源码指纹。行覆盖率 / 分支结果覆盖率（%）如下，私有头不适用产品继续标记 N/A：

| 库 | Windows | Linux |
| --- | --- | --- |
| xacme | 87.31 / 71.89 | 87.08 / 71.85 |
| xmail | 78.82 / 65.01 | 78.83 / 65.01 |
| xpop3 | 76.99 / 60.29 | 76.99 / 60.29 |
| xsmtp | 81.99 / 63.92 | 81.99 / 63.92 |
| ximap | 80.79 / 62.94 | 80.79 / 62.94 |
| xjwt | 93.30 / 74.07 | 93.30 / 74.07 |
| xoauth2 | 92.41 / 77.39 | 92.25 / 77.20 |

修复影响的 xacme/ximap 重新验证模块选择单头、裁剪单头、静态库与动态库消费，两平台共 16 条交付命令通过；其他五库生产源及自有清单未改，保留上一轮执行记录并核对发布产物哈希。普通 BODY 回归以独立最小闭包执行，没有通过重跑整个 IMAP 网络压缩矩阵来改变上一阶段证据。

原缺陷冻结 7,023 文件，指纹 `2e90aa1de51b9cc8c8cd6406b379ca32c0875835f23c8fc2e3b6c3f5295d4417`；最终冻结 7,023 文件，指纹 `31df582aa4303833cafaa17934951eddc210c754656f5e1d4ee123ae9c3f02ad`。Windows 实际运行于 `out/goal-temp/imap-body-diagnostic-fixed-20261003`，Linux 于 `/root/xrt-goal/imap-body-diagnostic-fixed-20261003`。两个 Linux 原运行根分别按字节归档原缺陷 7,065 文件与修复验收 16,467 文件，保留源码、对象、计数器、正常/sanitizer 二进制及原路径；归档未执行、未重定位报告。正式各阶段源码运行前后 SHA256 一致，原缺陷探针的预期失败与修复通过日志同时保留。封存证据为 `out/goal-temp/body-diagnostic-acceptance-evidence.json`，未用函数审计为 `out/goal-temp/acme-unused-dns-split-audit.json`。

首次原实现收集器的名称正则漏掉含数字的 md5，两个平台因此在严格编译成功后报告长度断言失败；已修复正则，复用同一原二进制收集真实诊断，初次日志保留。Windows 主工作区预检只作探索性记录，不代替冻结验收。

初版 Linux 归档收录源码和 out/，漏收本轮新增交付的 release/。核对时补录 1,484 个原运行根发布文件，保留初版清单字节及 SHA256，旧文件全部再核对一致；补充记录单列，没有重跑或迁移消费程序。

初次最终核对把共享 suite 目录名里的 body_error 误当作二进制名称，导致计数断言失败；已按文件名识别两种诊断测试，保留失败日志并复算通过，没有重跑已通过的库验收。

下一阶段继续嵌套 ENVELOPE/FETCH/ESEARCH 等解析输入约束、ACME 流程/存储故障、宿主关闭与恢复审计；历史 Huawei/POLICE Linux 退出失败仍未确定根因，真实服务与托管 CI 未完成。长期任务保持 active，七库整体生产验收未完成。本节为验收后的文档追加，冻结工件不改写。


## 2026-10-03：IMAP DATA 诊断、游标事务与迭代复杂度

原实现的 39 个公开 API 场景各执行正常与单次真实分配失败对照，Windows/Linux 每平台 78 次，其中 58 次未满足契约。确认 16 条 OOM 首因被替换、13 个非法输入被接受、FETCH 缺值未设置诊断、9 条失败路径已推进游标、4 条数值溢出被降为协议错误；分类相互重叠，不能相加。后续新增的标量、字符串和借用重叠测试属于修复回归，没有冒称原版本已逐项复现。

DATA 包装层现在直接保留子解析错误；专用 FLAGS/STATUS/SEARCH/ESEARCH 游标仅在成功或 END 时提交状态，错误不修改游标与输出。FETCH 缺值生成稳定错误，属性及 literal 后续片段需要 SP，紧邻终止括号仍合法。LIST/STATUS 邮箱采用词法 astring，因此数字、NIL 和超出 uint64 的名称仍有效；LIST 分隔符只能是 NIL 或一个合法字符。STATUS/ESEARCH 已知标量检查类型和 uint32/uint64 范围，未知扩展值继续保留；SEARCH 标识非零且为 uint32，MODSEQ 必须是终项。ESEARCH 关联 TAG 验证结构、内容与间隔。语法依据 [RFC 9051 第 9 节](https://www.rfc-editor.org/rfc/rfc9051.html#section-9)。

StringWrite 提前拒绝长度输出/缓冲区与 view、Source、Value 的重叠，包含 Source/Value 分属不同存储的情况；容量不足仍返回所需长度且不改缓冲区。最小测试根只引入 DATA 与 memory_debug，生产闭包不增加调试分配器或网络依赖，公开 API 布局未改。

游标初始化完整检查文本一次，之后仅验证当前数据几何及所消费字节，消除每次迭代重扫整段输入的平方级路径。借用输入在游标生存期内须保持有效且不修改。独立公开 API 探针在原/修复版本、两平台各运行 SEARCH 10,000/20,000 和 FETCH 3,000/6,000 项，核对结果、结束状态、零分配及调用方已有诊断保留；耗时作为观察记录，不设跨平台硬阈值，不计入覆盖率。

Windows GCC、Linux GCC、Linux Clang ASan/UBSan 各运行模块化/单头 8 个程序。新增 DATA 每二进制 143 项，合计 858 次；既有 BODY 318 次回归继续通过，总计 24 个程序、1,176 次明确故障/成功对照。sanitizer 开启泄漏检查与错误即停。CI 专项加入 DATA/BODY 最小套件，YAML 及依赖闭包本地核对通过，尚未声称托管 CI 已执行。

双平台完整邮件模块及独立 TLS 范例重新通过，四库各自 C 主报告与独立私有头报告共 8 对刷新并由同批原始 profile 复算。ACME/JWT/OAuth2 的 6 对报告经当前依赖、原始对象和计数器哈希核对保持有效，未改绑报告输入。私有头不适用仍标记 N/A，不混入主口径。行覆盖 / 分支结果覆盖为：

| 库 | Windows | Linux |
| --- | --- | --- |
| `xacme` | 87.31% / 71.89% | 87.08% / 71.85% |
| `xmail` | 78.82% / 65.01% | 78.83% / 65.01% |
| `xpop3` | 76.99% / 60.29% | 76.99% / 60.29% |
| `xsmtp` | 81.99% / 63.92% | 81.99% / 63.92% |
| `ximap` | 82.31% / 65.45% | 82.31% / 65.45% |
| `xjwt` | 93.30% / 74.07% | 93.30% / 74.07% |
| `xoauth2` | 92.41% / 77.39% | 92.25% / 77.20% |

受影响 ximap 的单头、裁剪、静态/动态包消费两平台 8 条命令全部通过；其他六库生产源和自有清单一致，保留已通过交付证据并复核产物字节。

Windows 首次完整邮件验收在 IMAP STARTTLS 身份拒绝的最后场景退出 1：客户端退出 1、SNI 为 None，Python 服务端收到 ConnectionResetError(10054)，现有脚本只接受 SSLError。原日志及失败二进制/原始 profile 共 504 文件单独保留。相同二进制在隔离 GCOV_PREFIX 下运行 40 次身份拒绝与 4 次成功对照，观测拒绝均发生在服务端握手阶段，但没有复现首次 reset；这些补充计数不加入主覆盖。仅 IMAP 重跑完整测量后通过，前三库已通过主报告字节保持一致，随后四库原始 profile 复算及私有头报告通过。原始 reset 的阶段与根因尚未确定，不能单凭重试通过宣称已修复；该项进入有限收尾清单。

最终冻结 7,025 文件，指纹 `eb35b10b94455ef7a757268b2b983bef6e9efd4cd831d411afbd512d9583c026`。Windows 实际根为 `out/goal-temp/imap-data-diagnostic-fixed-20261003`，Linux 为 `/root/xrt-goal/imap-data-diagnostic-fixed-20261003`；Linux 原缺陷和修复根分别按字节归档 7,115/14,718 文件，包含源码、out、release、原始对象/计数器及运行二进制。运行前后源输入 SHA256 一致，归档不执行或重定位。正式证据为 `out/goal-temp/data-diagnostic-acceptance-evidence.json`；主工作区早期预检仅为探索记录。

接下来依照原六条验收条件执行有限收尾清单 `out/goal-temp/seven-final-acceptance-queue.json`，沿用输入仍匹配的已通过证据；仅新增明确缺陷或输入变化时重验相关范围。剩余工作包括嵌套 ENVELOPE 与 ACME flow/存储、宿主关闭/恢复契约的最后核对，历史 Huawei/POLICE Linux 退出失败定位，以及真实服务、部署环境和托管 CI 验收。故障路径覆盖率不要求机械达到 100%，未执行外部验收明确列出。长期目标保持 active，七库整体生产验收尚未完成。本节为验收后文档追加，冻结工件保持原字节。


## 2026-10-03：IMAP ENVELOPE、string 类型与 63 位尺寸契约

原 DATA 已验版本作为本次原实现基线，冻结 7,025 文件，指纹 `eb35b10b94455ef7a757268b2b983bef6e9efd4cd831d411afbd512d9583c026`。新增 45 个语义场景各执行正常及单次真实分配失败对照，Windows/Linux 每平台 90 次，其中 89 次不符契约：44 个非法输入被接受，其 OOM 对照也未到达诊断分配；另一个 uint64 算术溢出返回协议错误而非范围错误，其 OOM 首因原本正确。原实现的显式 NONSYNC APPEND 在服务器没有 APPENDLIMIT 时接受 INT64_MAX+1，Begin 返回成功并保留超限剩余长度。原失败日志、二进制、源输入与最终修复夹具同时保存。

MESSAGE/RFC822 与 MESSAGE/GLOBAL 的 ENVELOPE 现在验证十个字段，其中六个地址字段只能是 NIL 或非空地址列表，每个地址恰为四个 nstring；各字段要求 SP，地址列表允许协议定义的紧邻括号。BODYSTRUCTURE 的 string/nstring 接受 quoted/NIL，不再把 bare atom 当作 string。空 quoted、合法组地址和嵌套扩展继续可用；跨行 literal 仍在已声明的限制内，不假称新增聚合支持。语法依据 [RFC 9051 第 9 节](https://www.rfc-editor.org/rfc/rfc9051.html#section-9)。

已知 BODY octets/lines、BODY 数字扩展、STATUS SIZE、literal 标记和 APPEND 声明长度采用 0..INT64_MAX。BODY octets 保留超过 4 GiB 的支持，依据 [RFC 9051 附录 D 的 63 位尺寸要求](https://www.rfc-editor.org/rfc/rfc9051.html#appendix-D)；第 9 节 body-fld-octets 的 number 记法与附录 D 不一致，此处没有宣称发现已核实的勘误。公开 uint64/size_t 字段和 ABI 不改，通用 DATA 数值与 MODSEQ 仍保留完整 uint64 范围。literal 的非法字符与范围溢出分别返回协议/范围错误，错误不提交输出。APPEND 超限在命令字段分配和线路发送前拒绝；已认证会话随后完成同步与显式非同步上传、LOGOUT 和清理。

Windows GCC、Linux GCC、Linux Clang ASan/UBSan 各通过模块化/单头 16 个程序，共 48 个程序。新语义故障与成功场景每二进制 95 项，合计 570 次；既有 DATA 858 次、BODY 318 次回归继续通过。OOM 必须实际触发，失败输出和专用游标按完整字节快照不变；合法输入零分配并保留调用方既有诊断。APPEND 两种服务器转录每配置各一轮，共六轮，其中三次超限拒绝后恢复检查通过。这些是明确场景执行数，不作独立覆盖率分子。

新诊断测试由既有 imap_body_error_tests 最小根收集，依赖只有 BODY/DATA 与 memory_debug；生产闭包不新增调试分配器或网络依赖。CI 已有 sanitizer 步骤收集新测试，YAML、配置与生成头一致性本地核对通过。双平台 ximap 单头、裁剪、静态及动态库消费共八条命令通过；其他六库生产输入未改，原消费记录和发布产物哈希重新核对一致。

没有重新启动完整邮件覆盖率测量。邮件四库双平台八对主/私有报告、原 profile 及计数器保持原字节并明确绑定上一 DATA 输入，当前标为待刷新；私有头不适用仍为 N/A。ACME/JWT/OAuth2 的六对报告经当前依赖及原 profile 核对继续有效。待剩余具名代码契约关闭后统一刷新受影响报告，避免把旧百分比列为当前值。

本次最终冻结 7,027 文件，指纹 `ba8308ca1cde7ad06e54d9e99057d49954aea5914f03ce6841c771707aa383f2`。Windows 实际根为 `out/goal-temp/imap-semantics-fixed-r2-20261003`，Linux 为 `/root/xrt-goal/imap-semantics-fixed-r2-20261003`。Linux 修复根按字节归档 7,930 文件；原基线沿用已有完整源归档，再保存 107 个探针/日志补充文件，记录原实际执行根，未在归档目录重定位执行。正式各阶段运行前后源 SHA256 一致。封存证据为 `out/goal-temp/imap-semantics-acceptance-evidence.json`。

探索性预检首次发现最小 APPEND O1 构建的局部布尔值告警，已显式初始化，成功路径原本会赋值。第二次预检的无 APPENDLIMIT 夹具只接受 NONSYNC，但 AUTO 模式保守使用同步，导致转录失败；测试现显式选择 NONSYNC，保留原 AUTO 策略。最初原实现收集器也因同一 AUTO/转录假设未取得预期成功返回，日志保留且不算已验证原复现。首两次最终证据核对分别因收集器错误常量及不同库报告 schema 假设失败；修复收集器后只重新核对既有工件，没有重跑已通过的库验收。相关失败均单列保存，不用后续通过覆盖原日志。

有限收尾清单中的 IMAP ENVELOPE/composed BODY 子项完成，ACME flow/Windows 存储与 OAuth 构造/宿主关闭恢复仍待核对。历史 Huawei/POLICE Linux 退出失败及 Windows STARTTLS reset 根因、最终覆盖率刷新、真实服务、部署环境与托管 CI 仍未关闭。长期目标保持 active，七库整体生产验收未完成。本节为验收后文档追加，冻结工件保持原字节。


## 2026-10-03：ACME 存储诊断、目录创建与 Windows 发布故障

沿用上一 IMAP 语义已验输入作为原实现，冻结 7,027 文件、指纹 `ba8308ca1cde7ad06e54d9e99057d49954aea5914f03ce6841c771707aa383f2`。新公开 API 与真实文件调用探针合计 40 次具名执行、13 次不符契约：Windows account/cert save 与 list、Linux account/cert save 与 list 把内存失败改为其他类型；Windows grant save 暴露已有目录 stat 的内存错误被改为权限错误，原生根目录检查也把内存错误改为 NOT_FOUND。Windows 账户部分写、flush、rename 的诊断分配失败又被上层改写为 IO。另一个正常分配对照先实际发布 current 再报告写故障，重试后错误变成源文件 NOT_FOUND，丢失首次发布故障。首次 22 项文件探针使用较宽松的已发布错误断言；最终严格夹具又单独在原实现执行 control/OOM，确认 NOT_FOUND 的问题，未把早期宽松通过当作正确。

存储层现在保留目录/文件、分配、PEM 和 X509 子操作的原始错误；仅自身参数、容量与结构错误由存储域诊断。ListDomains 的目录不存在仍保持原业务判定，但不再把分配错误当作不存在。目录创建检查已有目录与根时直接判断 stat 的结果和类型，成功仍保留调用方既有诊断。Windows current 发布重试持有首次 rename 失败对象，不新增分配；最后清理不把它替换为重试的 NOT_FOUND。暂时失败后成功发布仍返回成功，若调用报告失败但文件已发布，则保留完整版本供调用方重新读取核对。函数签名、结构布局与 ABI 不改；诊断所属子域可能变化，调用方应按 kind 识别失败，公开存储声明已说明该约定。

Windows GCC、Linux GCC、Linux Clang ASan/UBSan 正式通过 28 个程序，包括既有存取、并发、同步、IO 和通配符迁移回归。六种操作在模块与最小单头各扫描至首个不触发失败的成功分配点，三配置合计 36 轮、525 次实际 OOM；每次要求 MEMORY、零逻辑活动分配，整体授权的失败还核对旧私钥未被部分提交。目录已有路径/原生根共 30 次真实 OOM，另有恢复成功、既有诊断保留和普通文件父路径拒绝，共 18 次具名目录契约执行。Linux 正式两配置使用独立 XACME_TEST_ROOT；早期共用默认根的通过记录保留为补充，不加入正式次数。这些是故障执行计数，不作覆盖率百分比或独立缺陷数。

Windows 最小生产单头使用测试文件中的原生 API 拦截，共 23 场景：文本写、私钥部分写、current 写、账户部分写、三类 flush、三类 rename，各有正常分配与真实诊断 OOM 对照；另外验证 current 两次暂时失败后第三次成功、实际提交后报错的正常/OOM 对照。未命中选择的调用转发真实 Windows API，未在生产源添加注入入口。失败场景核对已提交证书/私钥对、旧账户、无临时文件和应保留的完整版本，并要求零活动分配。后提交的 control 保留 IO，OOM 保留 MEMORY，均可读到完整新版本。

CI 的 xacme_tests 现显式登记三项新增单头资产和核心目录模块测试。只把测试根放进 depends 并不会收集这些资产，首次核对发现此缺口后已补齐；Windows 专属故障资产在 Linux 排除，公共最小存储与目录测试在双平台选择一次。核心 all 测试根也收集目录回归，生产 acme_store/xacme 的闭包没有新增 memory_debug 或测试根。新增 Linux sanitizer 步骤、本地 YAML、实际 matrix 资产选择和最小依赖检查通过；未执行托管 CI。双平台 xacme 单头、裁剪、静态与动态库消费八条命令通过。共享 Core/config/生成头输入已变化，其余六库的旧发布产物字节已复核保留，不能作为当前源的消费验收。

本轮不重启完整七库覆盖率测量。十四对主/私有报告及原 object/profile 的字节和关联重新核对一致，全部仍绑定旧输入；共享 Core/config/生成头变化使其当前状态为过期，私有函数不适用的库仍标 N/A。待剩余具名代码契约关闭后统一刷新受影响报告，未把旧百分比改为当前值，也未给测试拦截调用人工覆盖信用。

最终冻结 7,033 文件，指纹 `b0d0e5129adfcbfd90b9d88014f7206a3816309b536cbf3bc2f8791688875199`；Windows 实际根 `out/goal-temp/acme-store-diagnostic-fixed-r2-20261003`，Linux 实际根 `/root/xrt-goal/acme-store-diagnostic-fixed-r2-20261003`。Linux 原实现输出补充 64 文件，沿用既有完整原源归档；最终输出按字节归档 1,619 文件，并引用与原生源逐文件相同的完整 Windows 冻结源，避免再复制数千个相同文件。原生执行前后及归档前后源 SHA256 相同，归档目录没有执行或重定位。共享工作区的额外进程模块/测试变化单独记录并保留，本节不扩展为进程模块全部行为验收。生成头在冻结根更新，随后从当前源重新生成，当前工作区全部输入与最终冻结一致。封存证据 `out/goal-temp/acme-store-acceptance-evidence.json`。

初始探索性构建曾因测试误用 xrtPathAbsolute 名称及 O1 snprintf 容量告警失败；夹具已使用实际 xrtPathAbs 并增加根缓冲容量。第一次冻结发现其他线程已新增两项进程测试，按实际额外输入记录后重建；第一次最终归档核对遇到 Linux 旧通配符目录中星号不被 Windows 文件 API 接受，现对这些既有字节通过 WSL 读取核对。失败日志、旧夹具和早期成功记录全部保留；最终没有因核对脚本问题重复运行已通过的库验收。

有限清单中的 ACME Windows 存储/原子发布子项完成。ACME flow response/account/signing、OAuth 请求构造与关闭恢复、历史 Huawei/POLICE Linux 退出失败和 Windows STARTTLS reset 根因、最终报告/其余交付刷新、真实服务、部署与托管 CI 仍未完成。整体长期目标保持 active。本节仅在验收后追加文档，冻结源和原始工件保持原字节。


## 2026-10-03：ACME 流程响应、nonce、CSR 与首个故障诊断

修复目录、账户、授权、轮询、换钥与停用流程中已复现的 JSON 类型、空值、
解码 NUL、状态与 HTTP 状态处理问题。解析、PEM、JWS、CSR、证书传输和
持久化失败保留原子错误的 kind/domain/code；真实解析分配失败直接停止，
不会被改成协议/内部错误或继续进行写操作。换钥只在服务器响应或只读查询
证实后应用新钥；持久化失败保留已生效新钥并返回存储子错误。

badNonce 只匹配 HTTP 400 的结构化问题 type，不能被 detail 内文字或 NUL
后缀误触发。响应 nonce 必须为非空合法 base64url；重试使用该响应提供的
新 nonce，最多三次 POST，缺少 nonce 时停止。普通问题响应也校验真实 JSON
对象，解析 OOM 不再变成账户不存在或继续换钥。已吊销证书只接受准确的
alreadyRevoked 问题类型。

Issue 在复制前校验域名非空、无 NUL、长度不超过 511 字节；最多 16 个
不同域名，先去重再检查数量。订单与 CSR 使用同一组去重域名，17 次重复
输入可正常签发。所有失败路径清空输出证书/私钥与 renewed 标志，并在
TXT 清理前持有首个错误，避免 Remove 的失败覆盖签发诊断。授权 identifier
的 NUL/缺失文本已覆盖；本节不声称其他尚未具名验证的授权策略均已完备。
证书私钥读取的格式探测在 OOM 时停止，CSR 构造不再覆盖已有错误。

新增公开聚合 Core 加实际 flow 源的 66 项夹具，运行 130 次（66 个正常控制、
64 个真实 allocator 故障），检查精确错误、无部分产物、失败后的同一客户端
和新客户端恢复、初次错误保留、非法/重复释放及活动分配归零。JSON、PEM、
JWS、CSR 与内存实现都是真实代码，HTTP/存储边界由夹具控制；此类边界
替换不计作真实传输或真实证书验证。另由实际 HTTP/TLS 模拟 CA 校验 CSR
签名有效、SAN 去重并与订单集合一致，以及实际证书链/私钥配对。

最终修复前对照沿用存储阶段冻结的 7,033 文件原实现，使用与最终夹具完全
相同的源码。Windows/Linux 各 130 次执行出现 94 个契约不符；该数字是
失败执行数，不是 94 个独立缺陷。Linux 原实现 600 字节域名控制由 UBSan
准确报告 char[512] 子对象的越界索引。原 host certificate key 的 MSan 控制
通过，因而本轮显式初始化 StackKey 只作为代码清晰度调整，不冒称已复现
未初始化读取。所有原始夹具、二进制、失败日志与哈希继续保留。

三种配置为 Windows GCC、Linux GCC、Linux Clang ASan/UBSan。模块/单头
分别收集 32/31/31 项，共 94 个程序角色；每种配置的 130 次新流程检查通过，
合计 390 次（其中 192 次 OOM）。每配置的十个真实服务入口仍明确 SKIP；
其中 test_flow 随后由模拟 CA 配置单独启用。本节没有用这些 SKIP 证明真实
云服务通过。三种配置各接受十个具名模拟 CA 测试项，其中两个 DNS 结果
未知测试项各含三种错误子例；另各通过一个 nonce 模型单测和十八个独立
证书核验反例，合计 30 个模拟 CA 测试项、3 个模型单测、54 个核验检查。
双平台各四条单头、裁剪、静态/动态包消费命令通过。sanitizer 未报告实际
内存、泄漏或未定义行为错误。

验证按实际输入复用，未为测试模型修改重新跑相同生产对象：R3 冻结指纹
`a417188499ac41a677883980fefa1cc6c8ae81a61cce895bb9ccb3641b72c1e1`，
所有 C 程序完成后 Python 核验器因错误的直接脚本调用方式退出，改为模块
调用。R4 只改模拟 CA、Python 编排器及 test_flow 夹具三文件；补齐 badNonce
响应的新 nonce，旧 IO 断言修正时误把普通文件路径组件故障固定成 TYPE，
因此每配置完整十项中九项通过、一项失败。R5 只修改该存储失败分支，先
直接调用相同存储 API 得到独立的 kind/domain/code，再精确核对换钥传播，
三配置集中重跑该项通过。其余九项与模型/核验测试复用 R4 原始成功记录，
模块/单头和生产对象以及八条交付命令复用 R3；源码桥、对象、二进制、日志
逐项绑定，失败的 R3/R4 完整运行从未改为成功。R2 的单头链接错误与严格
编译容量告警、早期模型失败、旧控制记录均保留，不混入最终成功计数。

R5 最终冻结 7,039 文件，指纹
`c852226cb93241665a9dadedaaee4ad9c9d1b03dbe62b4303e270334d02ea215`。
Windows 实际根为 out/goal-temp/acme-flow-fixed-r5-20261003，Linux 实际根
为 /root/xrt-goal/acme-flow-fixed-r5-20261003；R3/R4 的复用工件仍在各自原
执行根。Linux 原对照及 R3/R4/R5 输出分别按字节归档并引用完整 Windows
冻结源，全部原生源、原生输出、归档及源引用再次逐文件 SHA256 核对，未
从归档目录执行或重定位。其他线程的 Core Future/process 改动作为实际
输入保留，不归为本轮修复或独立完整验收。

最小 acme_flow_fault_tests 根和显式单头资产进入 xacme_tests；Linux CI
sanitizer 步骤包含该根，YAML、实际平台资产收集和生产最小依赖通过本地
检查，Core 与九个扩展生成头的一致性检查通过。未执行托管 CI。十四对
历史主/私有覆盖率报告字节仍保持原样，均绑定旧共享输入；本节没有更新
百分比，仍待剩余具名代码契约关闭后刷新受影响报告和其余六库交付。

本节范围完成，证据为 out/goal-temp/acme-flow-acceptance-evidence.json。
继续待办包括 DNS resolver 传播分配故障诊断（Query 覆盖分配错误、flow
继续回退的路径需隔离复现和修复）、OAuth 请求构造与关闭恢复、历史
Huawei/POLICE 退出及 Windows STARTTLS reset 根因、最终报告/范例/交付，
以及真实服务、部署和托管 CI。长期目标保持 active，七库完整生产验收未
完成。本节在验收后只追加工作单，冻结输入和历史工件保持原字节。


## 2026-10-03：ACME DNS 分配诊断与传播回退

DNS TXT Query 的编码、UDP 创建/发送、future 接收和重发真实分配失败现已
保留首个 XERR_MEMORY 的 kind/domain/code；接收 OOM 不再重发或继续等待。
其他失败维持既有 DNS 协议/网络诊断。解析器拒绝空 count 指针，失败时提供
的 count 始终为零，不再暴露部分 TXT 数量；坏响应重新给出自身协议错误，
不沿用调用者的旧错误。清理保存并恢复首个错误。

传播查询遇到 OOM 立即停止 resolver 回退、传播重试和 CA challenge；Issue
清理已铺设 TXT，清空输出，保留分配失败原因。普通 resolver 超时/不可达仍
是提示性探测，可回退或让 CA 判断 DNS；API 与 README 明确这一区别。

三种配置（Windows GCC、Linux GCC、Linux Clang ASan/UBSan）分别接受
33/32/32 个模块/单头程序角色。每配置的最终流程夹具 136 次检查（66 次
OOM）与 DNS 夹具 14 次检查（6 次 OOM）通过，共 408/198 和 42/18 次。
真实 UDP 服务器各执行 8 个生命周期场景，真实 HTTP/TLS 模拟 CA 各执行
1 个签发/吊销/存储测试项；合计 24 个 UDP 场景、3 个 CA 测试项。双平台
各四条单头、裁剪和静态/动态包消费命令通过。sanitizer 未报告内存、泄漏
或未定义行为错误。每配置十个真实服务入口仍 SKIP，不计为云服务验收。

原实现对照使用冻结的 7,039 文件，三配置各 14 个 DNS 场景与 6 个最终
传播夹具场景，合计 60 次执行，其中 30 次契约不符；这不是 30 个独立缺陷。
空 count 原例在 Windows 触发 0xC0000005、Linux 普通构建触发 SIGSEGV，
UBSan 准确报告 null pointer 写入。两条 resolver OOM 原例均错误地继续签发。
正常超时、resolver 回退与提示性探测控制通过。夹具包含失败后复用/新建
恢复、精确错误、输出清空、TXT 清理、借用引擎仍可工作及分配归零检查。
接收超时与流程边界由夹具控制；实际 UDP 响应、HTTP/TLS 与证书/CSR 核验
由独立互操作测试验证，二者不混计。

R1 完整 C/网络/交付已通过。最终 R2 只把传播夹具的 1 毫秒实际超时改为
受控单调时钟，消除调度导致跳过故障注入的误报。逐文件证明生产源码及
其他测试/交付输入相同后，仅三配置重跑该夹具和原实现的六个传播对照；
136 次最终检查替换 R1 的同名检查，不重复累加。R2 冻结 7,040 文件，指纹
`28c5709d9ca4462417b2842f76823235f6b375b8fb26a1df5b5c54c233c723bc`；R1 指纹为
`495a8872bbfab187ae81f74e10e11481340c095a518ecfebe555b4aa9356c835`。
R1/R2 与原实现的真实 Linux 根均在 /root/xrt-goal，Windows 在
out/goal-temp 下各同名根。原生源、输出及字节归档逐项 SHA256 核对，归档
不执行。初次准备因其他线程 process/生成头变化失败，以及 Windows CRLF
计数器失败均原样保留；实际夹具通过，修正收集器后的复验另立成功记录。

acme_dns_fault_tests 最小根、显式单头资产与 Linux sanitizer CI 步骤已加入，
YAML、平台资产收集和生产最小依赖在冻结输入上通过。Core/扩展生成头与
API 引用检查通过。本范围的证据是 out/goal-temp/acme-dns-acceptance-evidence.json。
当前其他线程的 config/process/Core 聚合头变化已记录，未纳入本次生产
验收，也不回滚；最终集成需要刷新受影响输入。十四对旧主/私有覆盖率
报告仍保留原字节，当前均过期，不更新百分比或声称七库整体已完成。

本范围完成；继续 OAuth 请求构造与关闭恢复、历史 Huawei/POLICE 退出和
Windows STARTTLS reset 根因、最终范例/报告/交付及真实服务/部署/托管 CI。
长期目标保持 active。


## 2026-10-03：OAuth 请求、子错误与一次性会话清理

通用/微信 token 请求、Basic 头、token 字段、JSON 与 userinfo 的真实分配
失败现保留 XERR_MEMORY 及其原始错误对象，不再被 ARGUMENT、TOKEN_RESPONSE、
TOKEN_ENDPOINT 或 NETWORK 覆盖。已出现的非法字段类型/NUL 仍给出自己的
响应诊断。传输回调前清除旧线程错误，回调失败时保留它提供的原始 xerror；
回调未给诊断才补 NETWORK，部分响应体仍由库释放。非 OAuth 错误域会使
xoauth2LastError 返回 0，调用方须查看 xrtGetError；非法 URL 的便捷传输
ARGUMENT 也不再被改成 NETWORK。README 与公开注释说明这项错误语义变化，
公开结构布局及函数签名保持原样。

state 校验通过即消费会话；请求体构造失败也用 SecureZero 清除 verifier
与 challenge，认证头失败同样清理。错 state 保留合法会话且不调用传输。
库不自动重发授权码/刷新操作；本节验证每次操作至多一次回调。网络结果
未知的实际 HTTP/TLS 关闭和恢复另列下段待办，不用受控回调替代真实线验收。

新 test_flow_fault 执行实际表单编码、独立已知 Basic 编码向量、JSON、
token 与 allocator；HTTP 边界受控。十类分配扫描与三类额外控制覆盖登录/
刷新两种认证风格、token/非 2xx 响应、userinfo、微信登录/userinfo、错 state、
回调原错误及无诊断回调。Windows GCC、Linux GCC、Linux Clang ASan/UBSan
各执行 272 个实际分配故障、13 个正常控制，共 855 次主操作检查、816 次
OOM 检查，另逐次执行 816 次同一客户端恢复检查。所有分配故障返回空结果、
保留精确错误；消费过的 PKCE 字段归零，活动分配、非法与重复释放归零。
原实现使用完全相同的最终夹具，三配置各 13 场景出现 11 次失败，共 39 次
执行/33 次失败；不是 33 个独立缺陷，两个控制仍正常通过。

最终接受 17 个程序角色：Windows/Linux 普通配置各七个，Linux sanitizer
三个。含原有 OAuth 的 264/257/257 个检查、每配置十五个 OIDC 组合检查、
双平台各三个可运行离线范例；普通配置的最小消费者沿用相同生产输入下
已编译的 R1 二进制，集中重验并记录前后源哈希。sanitizer 无实际内存、
泄漏或未定义行为报告。现有 OAuth 回归中的真实 loopback HTTP 路径通过，
范例和受控回调不计为外部 provider、TLS 关闭或托管 CI 验收。

R1 的唯一旧回归失败是把不支持的 URL 诊断固定为 NETWORK，按新契约修正
Windows/POSIX 两处断言。R2/R3 新夹具先后遗漏 Core Base64 和错误对象创建
时的分配观察；实现已保留 MEMORY，补齐观察后 R4 全部通过。失败记录与
Linux R1 的原生捕获补充仍保留，未改成成功。R1 到 R4 逐文件证明只改两份
测试夹具；生产、公开注释、范例和工具输入一致，不为夹具修正改生产代码。
R4 冻结 7,043 文件，指纹
`45487c39f70b1a32e0951191f3bfa3de51ba94289c2fde14d534ef5fc55417bc`。完整源、实际执行根、二进制、原生日志与
Linux 字节归档由 out/goal-temp/oauth-flow-acceptance-evidence.json 绑定；
归档不执行。新夹具进入默认 auth 测试及 Linux sanitizer CI 选择，YAML
与选择已本地核对；没有执行托管 CI。测试工具增加独立输出目录，使普通
与 sanitizer 二进制可以分别保留。

本节范围完成，七库整体生产验收仍未完成。剩余具名协议范围为 OAuth
便捷 HTTP 终态 Future 的分配错误传播，以及 pending/结果未知时的关闭、
恢复契约；Core FutureWait 只等待状态，须核对 HTTP 层在 Destroy 前读取
FutureError 的路径。仍保留历史 Huawei/POLICE 退出和 Windows STARTTLS
reset 根因、十四对过期报告/最终范例与交付刷新、真实服务/部署/托管 CI
验收门槛。本节只追加记录，不改写历史源和报告，长期目标保持 active。


## 2026-10-03：OAuth HTTP Future 诊断、取消与未知结果

内置 HTTP 现先取得失败 Future 的生产错误，再释放观察引用。连接、发送、
响应头、正文与 EOF 确认中的实际 MEMORY 保留原始错误对象，普通 I/O 和
超时维持 NETWORK 分类；TCP 的异常读端结束也取出保存的 StreamError。
请求开始清除旧线程错误，失败收尾先保留请求诊断，Abort、Destroy 和缓冲
释放不再用次级清理错误覆盖它。未取得 Future 的连接失败同样给出诊断。

超时或其他放弃等待路径在释放 pending Future 前请求生产任务协作取消；
仅 Destroy 观察引用本身不代表停止生产。已取得连接仍 Abort，响应指针与
状态码清零，不交付部分正文。库不自动重发请求。部分 POST 可能已由服务
处理，宿主须依据提供方协议恢复未知结果，不能将传输失败当作未发送。
公开注释与 README 说明这项行为，逐项核对公开签名及结构布局没有变化。
本节结束上一节所列的具名 OAuth HTTP 契约范围。

test_http_fault 执行真实 Core Future/Promise、HTTP 组装与解析、网络字节
拥有权和 allocator；传输边界受控，路由标记不作为真实 Stream 解引用。
Windows GCC、Linux GCC、Linux Clang ASan/UBSan 各 31 项主检查，共 93 项：
30 个真实分配失败、27 个 pending 取消、30 个普通 I/O 和六个成功控制。
另有 87 次同一 HTTP 上下文的新请求恢复，逐次确认单次尝试、失败输出归零、
精确 MEMORY 和活动分配/非法/重复释放归零。原实现采用相同最终夹具，
三配置共 93 次执行/81 次失败/12 个通过控制；失败场景数不等于独立缺陷数。

最终接受 15 个程序角色：各配置新增夹具、完整 OAuth、最小消费者及两个
线验收客户端。原有 OAuth 检查为 Windows 264、Linux 257、sanitizer 257，
共 778 项、零失败。八组独立回环验收实际执行拥有权、清理超时与重试、
借用引擎、构造失败、TLS 握手中断与超时、部分上传超时与断流、认证关闭、
非认证断流及 TCP EOF；服务端观察 POST 前缀、返回前的连接中止，并检查
没有第二次连接或自动重发。sanitizer 无泄漏、非法访问或未定义行为报告。
这些是真实本地 socket/TLS 与生命周期检查，不代替外部提供方的发布验收。

R1 首次 Windows 执行已通过 MEMORY/IO 的二十个场景，随后新夹具遗漏释放
PromiseCancelToken 返回的新增引用，被活动分配断言发现。R2 只修正该夹具
释放，没有再次修改生产代码；保留原失败及 R1 原生源，不把它计为正式通过。
R2 冻结 7,044 个文件，指纹
`0160805a00710d2028680a2fda1bf41021c1352b5629a7d233035bc673af1f02`。原实现证明、正式执行根、二进制、原生日志、
完整 Windows 源和 Linux 字节归档由 out/goal-temp/oauth-http-acceptance-evidence.json
绑定，归档不执行。夹具进入默认双平台 auth 测试和 Linux sanitizer CI
选择，YAML 与选择已本地验证；测试工具可保留独立二进制输出目录。

具名协议/故障契约队列已经完成，七库整体生产验收仍未完成。下一步为原始
Huawei/POLICE Linux 退出与 Windows STARTTLS reset 的根因，不以之后的
通过替代原因证明；十四对过期报告、最终范例/交付刷新、托管 CI、真实服务
互操作和部署恢复仍保留为独立门槛。长期目标保持 active。


## 2026-10-03：邮件 TLS 拒绝分类与历史原件取证

Windows IMAP STARTTLS 身份拒绝的原始失败已经归因于测试的异常分类。
保留的十次 IMAP 范例执行覆盖率原件直接记录：两次证书名称不匹配、
一次 STARTTLS 升级失败、零次升级后的 CAPABILITY 失败；与原日志中
第十次预期身份拒绝的客户端退出 1、SNI 为 None、服务端
ConnectionResetError(10054) 对应。证书检查在握手中返回失败，客户端
没有转入已认证通信。测试原先只接受 Python SSLError，从而将该握手
传输终止误记为失败；本节使用原始失败 counters 归因，之后通过的重复
执行并非这个结论的依据。原日志与 504 个二进制/对象/profile 原件保持不变。

邮件 TLS 工具现记录 accept、upgrade、handshake、protocol 与 complete
阶段。只有预期身份拒绝、客户端退出 1、SNI 正确且错误在 handshake 阶段
时，才接受 SSLError 或 ConnectionResetError。握手成功后的同类错误、
TimeoutError 和其他失败仍拒绝。新增 --rejection-policy 回归进入
Windows/Linux CI；与主覆盖率测量分开，避免把受控异常表面计入主要 profile。
普通执行仍使用原来的真实协议操作与 TLS 服务端。

最终双平台共 72 项分类检查：十二项实际成功握手、十二项实际身份拒绝、
十二项实际拒绝后的受控 reset 异常表面，以及三十六项超时/已成功握手后的
SSL/reset 排除控制。受控异常表面明确标记，不称为强制复现原生 reset。
首版对照还证明原工具双平台共十二项错误分类；首版结果保留，最终验收
采用完全相同的最终脚本另跑的 72 项，不把两个版本相加。客户端使用
已冻结 7,025 源输入下的真实 POP3/SMTP/IMAP 二进制，执行前后逐项核对
源与原有 profile，补充 counters 写到隔离目录。Linux 实际执行根、
三个原生客户端及输出字节归档已校验，归档不执行。修改仅为 Python 工具
和 CI 的回归选择；本节没有改变生产 C 行为、API 或布局。

封存期间工作区新增 process_file.c、Core 单头和 xruntime 单头变更，
首次严格冻结因此停止，现场保留。最终分类快照保留之前完整 C 基线，
只叠加当前测试、CI 和工作单；7,044 文件指纹
`0b2765cf5f9b80169db77f387a854c6ba4d60fc321aa9f0d1f2e48f40a15b04a`。三个工作区差异显式保存，当前全量交付
没有宣称通过。首次证据校验的函数名称/行号断言和 Windows 长路径访问
限制也保留为失败记录；核对真实 StartTls 调用并用扩展路径读取已校验的
Linux 原件后，最终验证通过，没有为这些校验器问题改生产代码或测试判定。
out/goal-temp/mail-rejection-acceptance-evidence.json 绑定输入、原始 counters、
实际执行、归档、局部 CI 选择与上述差异，未执行托管 CI。

Huawei/POLICE 的历史 Linux 静默退出仍未完全归因。原构造及请求成功、
无 stderr，且该成功单实例路径不向失败构造队列提交 owner，排除了 pending
清理失败；剩余可达的静默失败是 LiveCount、LiveBytes、InvalidFreeCount
或 DoubleFreeCount 检查。旧记录没有保存具体字段及分配来源，不能判断
是未退休资源、真实泄漏还是释放计数异常。现夹具已有详细退出诊断；已有
250 次诊断重复与后续全套通过仍不作为该原始原因的证明。本项继续开放，
下一步推进当前输入的最终范例、交付与十四对报告刷新，长期目标保持 active。

封存后 config/modules.json 又出现变更，连同进程实现和两个单头的当前差异
一并记录；首次最终账本验证因此停止。分类脚本及 CI 选择没有改变，局部
验收仍绑定上述实际原件。当前配置/实现/生成头组合的交付验证继续待办。


## 2026-10-03：最终交付刷新与邮件范例清理

本轮核验当前 Core process_file.c、模块清单及已同步聚合头。7,636 文件冻结输入
seven-delivery-audit-fixed-20261003 的 Windows 与实际原生 Linux 执行结果分别保存。
八个依赖聚合头已按当前源码同步，Core 加九个扩展的十项只读生成检查通过；
五个清单产品的 API 文档、声明索引和成熟度检查共十五项通过。最初 ACME 成熟度
检查失败还包含冻结目录漏收 dev/bench/bench_common.h，补齐冻结材料后通过，
没有把该材料缺失解释成仓库实现缺陷。

邮件范例共享清理原本忽略 Resolver/Engine 销毁结果并直接清空指针。真实 Engine
Pin 在双平台确定性证明：旧代码丢失创建者指针，同时覆盖原始协议错误；探针
借助额外保存的拥有权恢复资源，退出时内存统计归零。新清理使用 TryDestroy，
在五秒预算内推进退休；忙碌或失败时保留未消费的句柄及 TLS 资源，Resolver 未
退出时保留 Engine 运行，之后允许重试。既有错误不被替换，清理未完成时范例
返回失败。错误诊断只输出阶段、错误类别和代码。永久回归覆盖有/无原始错误的
Engine Pin、仍在运行的 Resolver 查询及空闲拥有者：Windows GCC、Linux GCC
和 Linux Clang ASan/UBSan 均通过，重试后活动分配及非法/重复释放计数为零。
此修复限于范例及其清理辅助代码，没有改动七库的生产 C 源码或公开 ABI。

双平台真实 TLS 邮件范例各 54 项检查通过，包括 DNS、IPv4/IPv6 身份、两种 TLS
模式、截断响应、预期身份拒绝以及 reset/timeout/握手后故障的分类控制；离线
邮件处理、POP3 取信、SMTP 提交和 IMAP 邮箱读取范例通过。五个清单产品的单头、
裁剪、静态/共享消费者，以及 JWT/OAuth2 的分别与组合消费者双平台通过。
ACME 公开范例在模块化和单头形态下实际签发、持久化、缓存复用及 TXT 清理，
并验证未确认变更不被重放。以上仍是离线和本地跨实现验收，不替代实云或托管 CI。

邮件四库双平台主报告及私有函数报告已按原始计数重测、再读核验；JWT/OAuth2
主报告及私有函数报告在同一冻结输入下也已重测，共十二组报告对。两平台 ACME
模块与独立探针运行完毕后，十一项 mock 测试全部通过，但统计驱动仍硬编码要求
testsRun==9，最终退出 1，因此没有生成/接受 ACME 主报告。这两个失败原件保留。
当前驱动已改为核对非空的完整加载数量，仍拒绝失败和跳过；五项直接运行真实
守卫字符串的控制验证：完整十一/十二项接受，空套件、单项失败、单项跳过拒绝。
修正后的完整 ACME 统计尚待执行。该统计工具改动还使共享后处理工具对 auth
报告的保守依赖哈希不同，auth 私有报告的当前工具绑定须刷新，未换哈希包装旧计数。

范例审查另发现 JWT handle_login 未释放借用签名输入，原生 Linux 成功登录探针
留下七项、778 字节活动分配；OAuth api_review 刷新失败后有再次释放旧 token
的路径，场景失败不能传播到 main；OIDC 范例有未检查分配、必需 claim 和失败
清理路径。这些具体缺口留在最终范例门禁继续修复，不能以普通运行成功关闭。

原生归档保留 base 7,457 项、fixed 21,596 项文件，源码、二进制、原始计数及日志
逐字节核对，归档本身未执行。所有终端已收集，证据见
out/goal-temp/seven-delivery-audit-acceptance-evidence.json。当前工具修正、本文追加
与已执行冻结版本的差异明确保存。Huawei 原始退出仍仅能定位到内存统计检查，
具体计数和分配来源未捕获；真实服务、部署恢复演练及托管 CI 仍未完成。
整体长期任务保持 active，未标记生产验收完成。


## 2026-10-03：认证范例故障收敛与当前七库报告核验

本轮收敛最后三个认证离线范例，并完成修正后的 ACME 全量测量。
最终源码冻结 7,640 文件，指纹 `5e1cf0b33378fa9f003a9a9941c7a4f1f758b57902b0bb079ee079e18b06c07c`；
测量冻结指纹 `7e614c518046ec36ae6da7dc71bf407cfeaf5d31186b0d61de81cf8ad32af2eb`。两者仅 JWT 范例、其故障测试及 README
三项不同：最终版本补上 IdP audience 校验，Windows/GCC、Linux/GCC 与 Linux/Clang ASan/UBSan
重新执行该范例及故障测试。其他范例和七库统计输入逐字核对，统计未用新哈希包装旧计数。

确认并修复：`handle_login` 在签发后释放借用 claims；中间件要求非空、完整、无内嵌 NUL 的
用户和角色字段，失败清空输出；JWKS 先解析替代项、成功后提交，刷新失败保留有效缓存；
第三方 IdP 同时核对 issuer/audience。OAuth 刷新成功后才替换旧 token，五个场景统一清理并
向主函数传递失败。OIDC duplication、claims 构造、签发、响应格式、JWKS/验证/userinfo
逐步检查结果，必需身份及 nonce 不截断，userinfo 的 sub 与已验证 ID token 一致；
退出清理 URL、token、JWKS、claims、userinfo 和 mock nonce，失败返回非零。

原实现的成功签发泄漏 7 个分配 / 778 字节证据继续保留。原中间件接受缺少 role 的有效签名
令牌且沿用 admin、原刷新失败再次读取已释放 token、原 dupstr 在实际分配失败时进入空指针
strcpy 均已执行并保留二进制、原始失败和诊断。Core 分配器会毒化退休存储，ASan 该次诊断为
SEGV，不能冒称标准堆的 heap-use-after-free 标签；小 helper 被内联时按实际调用点及受控
空分配核对。前期观察器标签/栈帧断言失败日志仍完整保留。

新增三个范例故障 case 接入默认 14-case 驱动及 Linux sanitizer CI。三范例严格构建和三故障
case 在 Windows/Linux 全部通过，Linux 三故障 case 的 ASan/UBSan 通过。JWT 11 组身份字段
边界、51 个失败分配点、失败缓存刷新、可信签名但错误 audience 和主函数失败/恢复均通过；
OAuth 5 个正常场景、5 个响应 OOM、5 个启动 OOM、刷新失败退出/恢复通过；OIDC 空分配
复制、3 个响应 OOM、启动失败/恢复、58 个签发/构造分配失败点和 5 组非法 nonce 通过。
成功及每条注入失败后活动分配/字节、非法释放和重复释放全部为零。

修正后的 ACME 全量配方在两个平台均通过：模块、独立 HTTP/提供商/授权验证探针及 11 个
已注册 mock CA 场景全部执行，禁止跳过。此前固定要求 9 个测试导致的失败报告继续保留。
JWT/OAuth 当前主报告和私有头统计已刷新；四邮件库报告的统计输入与原始 profile 字节
均未变，按精确绑定复用。每个平台七个主/私有报告对全部核对原始 gcno/gcda 和工具、
实现、头、测试/Core 依赖；OAuth 私有函数为 N/A，没有把它宣称为测试充分性。

| 库 | Windows 自有 C 行 / 分支结果 | Windows 私有头 | Linux 自有 C 行 / 分支结果 | Linux 私有头 |
| --- | --- | --- | --- | --- |
| `xacme` | 5411/6225（86.92%），4349/6063（71.73%） | 27 函数；93.19% / 73.88% | 5406/6234（86.72%），4360/6079（71.72%） | 27 函数；93.17% / 73.88% |
| `xmail` | 4536/5755（78.82%），3158/4858（65.01%） | 10 函数；94.34% / 65.79% | 4540/5759（78.83%），3158/4858（65.01%） | 10 函数；94.34% / 65.79% |
| `xpop3` | 669/869（76.99%），410/680（60.29%） | 无自有私有静态函数，N/A | 669/869（76.99%），410/680（60.29%） | 无自有私有静态函数，N/A |
| `xsmtp` | 806/983（81.99%），528/826（63.92%） | 无自有私有静态函数，N/A | 806/983（81.99%），528/826（63.92%） | 无自有私有静态函数，N/A |
| `ximap` | 2429/2936（82.73%），1792/2697（66.44%） | 无自有私有静态函数，N/A | 2429/2936（82.73%），1792/2697（66.44%） | 无自有私有静态函数，N/A |
| `xjwt` | 1128/1209（93.30%），800/1080（74.07%） | 2 函数；100.00% / 55.88% | 1128/1209（93.30%），800/1080（74.07%） | 2 函数；100.00% / 55.88% |
| `xoauth2` | 1175/1271（92.45%），829/1074（77.19%） | 无自有私有静态函数，N/A | 1175/1271（92.45%），828/1074（77.09%） | 无自有私有静态函数，N/A |

前一封存阶段的单头、模块、裁剪、静态/共享消费者及公开声明检查仍可复用：本轮逐字核对
2,382 个 Core/实现/公开与生成头/清单输入，未变化。
Linux 实际运行目录保留；原始 profile、日志、二进制和失败复现的
1,012 个归档文件均核对前后字节，既有 Windows 冻结源与
native 源逐文件相同，归档自身未用于执行。

完整证据：`out/goal-temp/auth-example-acceptance-evidence.json`；当前输入核验报告
`out/goal-temp/auth-example-acceptance-verified.json`；原始 native 归档
`out/goal-temp/auth-example-linux-archive-20261003`。本轮全部执行句柄已终结。
本地离线交付审计门禁关闭；**完整生产验收仍未完成，长期目标保持 active**。
剩余为原 Huawei/POLICE Linux exit 1 的具体内存计数/分配来源、最终整合提交的真实托管 CI、
显式配置的真实 CA/DNS/邮件/OAuth 服务及部署文件系统/宿主退出恢复；本地 mock、回环和
后续成功运行不能替代这些证据。


## 2026-10-04：ACME Link 解析与独立 Pebble 备用链验收

本轮确认并修复 `xacme` 的两处备用链互操作缺陷：HTTP 层仅复制第一条
`Link` 字段，丢失 Pebble 随后发送的 `alternate`；流程解析器把后续链接的
`rel` 关联到最先出现的 URL。原实现的组合头返回目录 URL，而严格根验证
证明真实服务下偏好备用链仍交付主链。原失败保存在
`out/goal-temp/acme-link-original-proof-20261003/results.json` 与 Linux 归档
`original-pebble/results.json`。早期 R4 同时信任两根，只证明链有效，不能
证明偏好生效，故不计作备用选择通过。

HTTP 层按字段顺序合并全部 `Link`，两遍计算与复制使用一次分配并检查
聚合长度溢出；其他响应字段维持其独立语义。解析器在各链接内处理参数，
正确处理 URI/引号中的逗号、转义、多关系值、参数和注册关系大小写；仅
第一条 `rel` 生效，不支持的 `anchor` 链接整条跳过。输出不足或解析失败
不交付截断 URL，下载失败仍沿用主链。依据
[RFC 9110 §5.3](https://www.rfc-editor.org/rfc/rfc9110.html#section-5.3) 与
[RFC 8288](https://www.rfc-editor.org/rfc/rfc8288.html)。

22 个解析与容量向量、实际 TLS 重复/组合头、空字段与长度溢出、五种
原复制分配故障及新的聚合分配故障均通过。分配故障核对首个错误对象身份，
并核对退出时无存活分配、无非法或重复释放。完整流程故障夹具在 Windows、
Linux 普通编译和 Linux Clang ASan/UBSan 下通过；三个版本各输出
137 个 `[PASS] FLOW` 标记（其中一个标记汇总 22 个解析向量）。
首轮模块构建发现缺少显式 HTTP 声明头，补充后 R2 双平台构建通过，首轮
失败保留。Linux 独立客户端副本的首次运行因复制丢失执行位失败；恢复执行
位并改用保留属性的复制后重验，二进制字节哈希始终一致。

新增 `tools/prepare_acme_pebble.py` 与 `tools/test_acme_pebble.py`。实际构建
上游提交 `1fcb30cabf594e047cb92432b9392bc3dfa469f7`，Go 1.26.0、vendor 依赖、CGO 禁用；
两次独立构建的 Linux/Windows 四个服务二进制逐字节一致。服务只监听
回环地址，使用真实 DNS-01 验证与显式 CA 信任，不改系统信任库、不设置
验证绕过。固定 90 天 profile 核对 30 天续签阈值；先前默认随机六天证书
触发重新签发符合预期，失败原件与判定探针保留。

Windows 与 Linux 各通过主链、授权复用、备用链三组公开流程：签发、
链/私钥/SAN/信任根验证、两条存储路径与复用、账户密钥轮换、停用和
吊销。独立管理端确认证书已吊销且 reason=1。手工备用签发必须验证到
根 1，两条默认存储链必须验证到根 0；复用用例的 TXT 查询数少于主链
用例。全部服务已退出。CI 已增加双平台独立服务任务并固定 Go action
提交与 1.26.0 工具链；当前整合修订的托管 CI 仍未执行。

本轮只刷新受影响的 ACME 计量与交付；其余六库原报告字节与输入绑定
均未改变。ACME 完整计量含全部 11 个 mock CA 场景，不允许跳过：

| 平台 | 所属 C 行覆盖 | 所属 C 分支结果覆盖 | 私有头函数行覆盖 | 私有头分支覆盖 |
| --- | --- | --- | --- | --- |
| Windows | 5489/6306 (87.04%) | 4429/6203 (71.40%) | 260/279 (93.19%) | 263/356 (73.88%) |
| Linux | 5482/6315 (86.81%) | 4439/6219 (71.38%) | 259/278 | 263/356 |

双平台静态/动态库及实际消费者、单文件与裁剪交付、注册的模块/单文件
范例通过。最终源码快照 `acme-link-final-20261003` 相对实际执行的 R2
只变更 CI 的 Go 工具链准备；运行时、测试和覆盖率依赖完全相同。14 组
七库报告（每组所属 C/私有头两个报告）已按最终源码核对。Linux 归档
保留 2772 个实际输出并逐字节核验，含编译产物、原失败和服务证据。
权威汇总为 `out/goal-temp/acme-link-acceptance-evidence.json`。

长期目标仍为 active，生产整体验收未完成。原 Huawei 退出内存计数/来源、
实际云 CA/DNS、独立邮件/OAuth 服务、最终整合托管 CI 与部署卷恢复仍未
关闭；后续定向审查包括相对备用 URI 的解析及证书响应的 PEM/叶证书
身份验证。本轮 Pebble 测试 CA 不能替代这些证据。


## 2026-10-04：ACME 下载证书完整性与相对备用 URI 验收

原实现公开签发接口将 HTTP 200 的非空正文直接作为证书交付。五个负向
用例证明 JSON、无效 DER、夹带私钥、错误公钥、错误域名均被接受；原件
及源文件哈希保存在 `out/goal-temp/acme-cert-original-proof-20261004/`。
相对 Link 的独立原始客户端试验中，绝对链接控制通过，相对路径、点路径
及网络路径均未请求备用链；原件为
`out/goal-temp/acme-relative-original-proof-20261004/results.json`，原非计量
二进制字节在试验前后保持一致。

下载现在只接受 `application/pem-certificate-chain`，正文仅含证书 PEM 与
空白；每张 DER 最大 256 KiB，每链最多 16 张。叶证书必须匹配本次 CSR
公钥、完整 DNS SAN 集合及当前有效期；逐项检查所提供链的发行者、CA
标志、可选 certSign KeyUsage 与相邻签名。备用链必须有同一 DER 叶证书，
否则保留已验证的主链。只保留临时 CSR 公钥快照，临时私钥立即擦除，
失败产物使用同一 grant 析构函数擦除导出的私钥。已知当前账户钥不得作为
证书钥，订单前拒绝；不支持的签名算法保留 UNSUPPORTED 诊断。此检查
没有执行部署的根信任、完整路径、用途或撤销策略，不声明取代完整 PKIX。
依据 [RFC 8555](https://www.rfc-editor.org/rfc/rfc8555.html) §7.4.2、§11.1、§11.4。

相对备用 URI 以实际主证书下载 URL 解析，支持相对路径、网络路径、查询
与点段；保留查询和百分号转义，片段从 HTTP 请求和保护 URL 一致去除。
拒绝非 HTTP(S)、无 authority、userinfo、非法文本及超容量输出，不交付
截断地址。依据 [RFC 3986 §5.2](https://www.rfc-editor.org/rfc/rfc3986.html#section-5.2)。
22 个 Link 解析向量和 59 个 URI/容量向量均通过。原 mock 的备用响应
重新签发了另一张叶证书，现改为同一叶证书加交叉签发发行者；绝对、相对、
点路径、网络路径分别执行真实 TLS/JWS/DNS/签发与存储流程，备用用例
仅信任备用根，主链不能通过其判定。

Windows、Linux 普通编译及 Linux Clang ASan/UBSan 各输出 176 个
`[PASS] FLOW` 标记（其中两个分别汇总上述解析向量），覆盖媒体类型、
证书/私钥混入、DER、期限、SAN、密钥、签名、链长度、分配故障、相同
客户端恢复与备用回退，退出无存活分配或非法/重复释放。实际 HTTP 头、
重复 Link、聚合分配与错误对象身份检查同样通过。初次完整交付在 `acme_flow -> http_target` 的缺依赖裁剪探针上失败；
已补齐公共头对 HTTP target 与新增 X.509 闭包的显式宏检查，双平台
定向裁剪通过。R2 相对 R1 只改公共声明头及生成的两份头，随后按当前
输入刷新计量和交付，原计量与失败归档保留。早期编译的 X.509 公钥
API 参数错误、两个没有触发真实分配的故障假设和 URI 非层次引用问题的
失败记录均保留；最终夹具用超过栈内容量的 DN 验证实际名称分配失败，
RSA 验签只作为正常签名用例，不声称不存在的堆故障。

双平台独立 Go Pebble 均通过主链、授权复用、备用链三组，手工备用链仅
验证到根 1，默认两条存储链验证到根 0；真实 DNS 查询、账户轮换/停用、
吊销及独立管理端状态验证通过，全部服务已结束。Windows 初次调用因
临时编排脚本传入相对运行时路径未启动服务，换成绝对路径后重验通过，
失败原件保留。这些是测试 CA 证据，未替代真实云 CA/DNS 或部署验证。

ACME 双平台完整计量、私有头报告及原 profile 绑定、单文件与裁剪产物、
静态/动态包及实际消费者、模块与单文件范例全部通过。完整计量执行全部
11 个 mock CA 测试，不允许跳过；其中备用测试包含四种 URI 子场景。
其余六库 12 组平台报告的字节及输入绑定未变，直接复用。

| 平台 | 所属 C 行 | 所属 C 分支结果 | 私有头函数行 | 私有头分支结果 |
| --- | --- | --- | --- | --- |
| windows | 5690/6556 | 4594/6551 | 260/279 | 263/356 |
| linux | 5683/6565 | 4604/6567 | 259/278 | 263/356 |

固定源码 `out/goal-temp/acme-cert-fixed-r2-20261004` 包含 7643 个输入，
Linux 实际执行于 `/root/xrt-goal/acme-cert-fixed-r2-20261004`，验收前后逐项
哈希一致。Linux 原始输出与 release 保留 3317 个实际文件并
逐字节归档；原计量二进制与归档副本没有追加运行。七库共 14 组平台报告
已对当前冻结源码核对。权威证据为
`out/goal-temp/acme-cert-acceptance-evidence.json`。

长期目标仍为 active，生产整体验收未完成。原 Huawei 退出内存计数/来源、
独立邮件/OAuth 服务、当前整合修订托管 CI、真实云 CA/DNS 与部署卷恢复
仍未关闭；下一步继续这些有明确验收条件的事项。

## 2026-10-04：独立 Dovecot/Postfix 邮件互操作验收

新增 `tools/prepare_mail_servers.py` 和 `tools/test_mail_real_servers.py`，使用
Ubuntu 26.04 仓库的实际 Dovecot 2.4.2、Postfix 3.10.6 实现。下载包按 APT
元数据 SHA256 核验后解包，不安装系统邮件服务；隔离的 mount namespace
提供编译时固定的模块路径与测试 syslog。所有监听在 IPv4 回环地址，虚拟
邮箱位于临时目录，LMTP 只投递本次测试邮件。用法见
[独立邮件服务器验收](mail-real-server-testing.md)。

Windows 与 Linux 各通过十八个 C 范例场景：SMTP/POP3/IMAP × 隐式 TLS
或 STARTTLS/STLS × 正常访问、错误密码、错误证书身份。SMTP 检查实际
LMTP 投递后的 From、To、Subject 和正文（含点行与空行）；POP3 检查实际
取回的邮件内容；IMAP 检查实际 INBOX 计数。错误密码必须返回
`XERR_PERMISSION / XMAIL_ERROR_AUTH`，同时由服务端日志证明 TLS 已建立
及认证被拒绝。错误证书身份必须在 open 阶段失败，日志显示 TLS 握手终止，
未完成认证或投递。独立 Python SMTP 提交仅用于建立种子邮件和验证环境，
不计入三十六个 C 客户端通过项。

输入固定于 `mail-real-fixed-r2-20261004`，源码与构建输入共 849 个文件，
指纹 `192a68dbb6558760278362fa01bbda42140ef572aecf0612a915c8345d607af3`。
Windows/Linux 输入一致，运行前后源码、客户端二进制和解包服务文件哈希
未变。正常及异常退出清理测试密码、CA/服务私钥与 Dovecot 内部认证令牌，
检查本次服务进程结束、所有监听端口关闭。早期配置、缺依赖、准备目录尚未
复制完成及 WSL 路径转义失败保留原日志；这些不是库缺陷。内部认证令牌
清理补齐后重新执行最终双平台场景。原计量二进制和 profile 没有追加运行；
七库先前报告所依赖的 C、头、测试、清单和原构建工具未变。

实际客户端二进制、服务记录、投递邮件、软件包及环境库哈希归档并逐字节
核验，权威汇总为 `out/goal-temp/mail-real-acceptance-r2-evidence.json`。
首次归档误收仍在写入的封存日志，最终校验因此失败；R2 排除活动日志，
保留该失败及已结束的原日志后重新封存，没有重复执行客户端或计量产物。
新增 `mail-real-servers.yml` 为 Linux 提供固定 Ubuntu 镜像的 CI 路径；本轮
没有运行托管容器任务，不能将配置存在视作托管 CI 验收通过。

本轮关闭独立本地邮件服务器互操作项，不代替真实邮件运营商、其他认证机制、
IPv6 或部署环境验证。长期目标仍为 active，生产整体验收未完成。原 Huawei
退出计数/来源、独立 OAuth 服务、当前整合修订托管 CI、真实云 CA/DNS 与
部署卷恢复仍待完成。


## 2026-10-04：JWT 时间边界修复与真实 Hydra C 客户端原型

原实现把 `now - leeway > exp` 作为到期条件，在 `exp` 以及容差后的
到期瞬间仍接受令牌。独立冻结原实现和测试，直接 claims 校验与已签名
令牌验签均复现；Linux Clang UBSan 另确认 `now + leeway` 在
`INT64_MAX` 与 `INT_MAX` 组合时发生有符号溢出。原失败源码、二进制
和日志没有重写，保存在 `out/goal-temp/jwt-time-linux-archive-r2-20261004/original`。

修复先比较有符号时间点顺序，再用无符号距离比较非负容差，避免加减
溢出。`exp` 边界为开区间，`nbf` 生效边界为闭区间；负容差报 ARGUMENT。
`NowOverride` 按头文件既定约定支持全部非零值，负 Unix 时间也能注入。
17 个时间向量分别经过直接校验和已签名验签，共 34 个检查；向量同时
进入主测试及独立程序，主测试输出为 335 pass / 0 fail。

Windows GCC、Linux GCC、Linux Clang ASan/UBSan 各执行 15 个
独立/最小/组合程序及离线范例，共 45 个程序通过。Sanitizer 启用泄漏
检测及错误即停。双平台静态/共享 JWT、OAuth2 和组合消费者共 12 个
通过；两个库仍共享一个 Core 实例，并验证公共导出。

JWT 双平台主覆盖率重新计量，私有头从同批原始对象 profile 复算并核对。
时间向量头文件绑定到计量输入；统计工具未修改。封存时严格检查发现
共享 Core 已由其他工作更新，涉及对象 ownership discovery API、内部
对象 backing、module 配置及两个 Core 聚合头。本轮 JWT 和 Hydra 从
冻结起就使用了新 Core（SHA256 `93bf95c80a6e35cc6e88e9bcbd52ce7a5887aa63be3525eed36e7027d0855a76`）。
其余六库 12 组平台报告绑定旧 Core（SHA256
`4036b6ab81abb67dd6615a33c9af0d0ed2665ebd28c1c1c2ef952a507fee43ae`），
原字节作为历史证据保留，逐项记录输入漂移，不再宣称是当前七库基线。
首次封存因此失败，日志及未获接受的部分归档保留；R2 修正证据范围，
未重复已通过的测试，也未用新哈希包装旧计数。Core 自身单头生成检查通过。
主报告只计自有 C；私有头两函数保持独立分母，不合并到下表：

| 平台 | 自有 C 行 | 自有 C 分支结果 |
| --- | --- | --- |
| windows | 1134/1212 (93.56%) | 811/1086 (74.68%) |
| linux | 1134/1212 (93.56%) | 811/1086 (74.68%) |

本轮冻结 144 个文件，Windows 运行目录为
`out/goal-temp/jwt-time-fixed-20261004`，Linux 运行目录为
`/var/tmp/xrt-jwt-time-fixed-20261004`。整轮编译/测试前后核对冻结输入。
存量测试产物未作为测试入口；所有本轮客户端均重新编译到独立输出。
Native 固定实现和原始失败证据按原字节归档 245 个文件，
归档不执行。权威时间修复证据为 `out/goal-temp/jwt-time-acceptance-evidence.json`。

独立下载、校验并运行 Ory Hydra v2.3.0 的 SQLite 构建，以内存 DSN、
随机临时客户端凭据、真实 HTTPS 和测试 CA 运行。独立 Python 控制完成
discovery、S256 PKCE、授权码、ID token、refresh token 和 userinfo。
C 客户端在 Windows GCC、Linux GCC、Linux ASan/UBSan 中各完成成功、
错误 PKCE、错误 secret 三个用例，共 9 个原型用例通过；成功用例验证
真实 RS256/JWKS、issuer/audience/签名拒绝、state 在联网前拒绝、nonce
错误与重放、userinfo sub 绑定，以及刷新后 userinfo。所有 C 客户端清理后
核对 live/bytes/invalid/double-free 为零。Hydra 已停止，临时私钥已移除。
原型证据 `out/goal-temp/hydra-c-prototype-evidence-20261004.json` 按字节
封存；客户端密钥、授权码、access/refresh token 未写入归档。

Hydra 原型只验证 confidential Basic 认证，未关闭 OAuth 全面互操作门禁。
后续须交付可重复的正式工具/CI，补充 Body 认证和公共 PKCE 客户端，
压实现有离线及新的网络 OIDC 示例所需的 exp/iat、签名算法、azp 与刷新
身份策略；这些策略不强加给通用 JWT API。现有整数 NumericDate 限制的
说明也需与 RFC 允许非整数的定义区分。托管 CI、实际服务商账号与部署
验证、历史 Huawei 退出计数/来源仍未关闭。长期目标保持 active，生产
整体验收未完成。

本轮随后重新生成九个依赖 Core 的扩展聚合产品，共 18 个完整/声明头，
所有生成检查通过；对应 Core 五个输入的哈希保持不变。生成记录与
原字节归档为 `out/goal-temp/current-core-aggregate-refresh-20261004.json`
及 `out/goal-temp/current-core-aggregate-archive-20261004`。该生成一致性
检查未替代其余六库的当前 Core 覆盖率与交付验收，12 组报告仍待刷新。

## 2026-10-04：OIDC 应用策略与正式 Hydra 互操作

原离线登录示例只组合通用 JWT 校验与 nonce，未要求 OIDC 的 exp/iat，
允许不可信额外 audience 和另一种有效签名算法；32 字节 subject 缓冲区还
拒绝合法的 255 字节身份。独立 Python/cryptography 签发器驱动实际示例，
19 项原行为观察复现这些缺陷，原输入、结果和二进制按原字节保留。
可选 email 被示例误当作必需字段的问题也已修正。

私有 `examples/oidc_example_support.h` 实现应用策略，离线的
`oidc_login.c`、`api_review.c` 和真实 HTTPS 的 `oidc_live.c` 共用。
验证必需声明、ASCII subject 边界、全部受众与注册算法，检查所选签发
年龄和时钟策略；azp、at_hash 出现时校验，授权码流程不强制它们存在。
所有 ID token 检查成功后消费 nonce，userinfo subject 须匹配已验证身份。
库的公开 ABI、通用 JWT 可选声明职责及 xoauth2/xjwt 独立链接边界未变。
整数 NumericDate 属于接口支持范围，README 与源码注释已消除“规范强制
整数”的错误表述。

刷新响应可不返回 ID token；返回时重新验签，绑定首次不可变身份对象的
issuer、subject 和 audience 接收者集合。刷新 nonce 可省略，出现时与
首次一致；已知 auth_time 不允许改变。首次未返回 auth_time 时无法证明
后续时间等于最初认证时间。示例不支持 max_age、essential auth_time、acr、
加密 ID token 或动态注册，这些限制明确写入文档。

22 个首次登录及 20 个刷新用例，在 Windows GCC、Linux GCC、Linux
Clang ASan/UBSan 各执行一次，共 126 项策略检查通过。夹具包含独立有效
ES256/RS256 签名、合法 255 字节 subject、可选声明与刷新身份边界；未来
iat 夹具单独设置更晚 exp，避免被 iat-after-exp 规则掩盖。校验层 39 个
实际分配失败位置均保留原始 MEMORY 诊断，不消费 nonce，资源回到基线。
原有 58 个签发故障及完整示例故障检查保持通过。双平台各 16 个认证、
最小/组合程序和示例通过；另有 4 个 Linux sanitizer 程序通过。

正式工具为 `tools/prepare_hydra.py`、`tools/test_hydra_interop.py`，
Windows 桥接和输入哈希模块随附。固定 Ory Hydra v2.3.0 SQLite 归档及
二进制 SHA256，实际下载/提取脚本已实跑。独立 Python HTTPS 控制用
cryptography 验证实际 RS256 与 JWKS，执行 S256 授权码、userinfo、刷新；
Basic、表单 secret、公开客户端分别运行，共 6 组独立控制通过。
三个 C 编译配置分别执行 11 个真实用例，共 33 项通过，包括实际 HTTPS
示例和错误 PKCE/凭据，以及 state、签名、issuer/audience、nonce 拒绝。
C 探针最终 live/bytes/invalid/double-free 为零，sanitizer 开启泄漏检测及
错误即停。服务仅用回环端口与指定测试 CA；结束后停止服务并移除临时
私钥，secret、code、cookie、access/refresh token 不记录到归档。
OIDC 策略与真实 Hydra 测试已接入 CI；只验证了配置，未执行托管 runner。

最终策略输入冻结于 `out/goal-temp/oidc-policy-fixed-r2-20261004` 与
`/var/tmp/xrt-oidc-policy-fixed-r2-native-clean-20261004`，57 个输入逐字节
核验。R2 只改变独立夹具生产器的两处未来时间构造，C 实现及 Hydra 输入
未变化；R1 的 32 个正常程序、4 个 sanitizer 程序和 33 个 Hydra 用例
按原绑定复用，没有更换其原哈希。一次 native 复制误带入正在生成的
Windows 输出目录，fresh-output 守卫在编译前失败；日志保留，复制改为
仅遍历 57 项源文件清单后通过，不执行或复用误复制的 Windows 二进制。

权威证据为 `out/goal-temp/oidc-policy-acceptance-evidence.json`，SHA256
`a114e5f978bb1e31505dfa16be5779778d52ab2ed5f00a0a0ad31b91d9638014`。
749 个源文件、二进制、日志与报告按原字节归档到
`out/goal-temp/oidc-policy-acceptance-archive-20261004`，归档不执行。
已关闭正式独立 OAuth/OIDC 测试服务互操作项，不能代替任意真实提供方或
生产部署验证。JWT 注释改动也使上一轮两份报告的源码绑定失效；本轮审计
确认当前 14 组七库覆盖率/私有头报告均需刷新，旧计数未重写。长期目标
保持 active，历史 Huawei 退出来源、当前整合托管 CI 及部署验收仍待处理。

继续复核发现：刷新不返回 ID token 时，Bearer 应用会跳过 token_type 检查。
冻结 R2 原 C 实现与独立新夹具已复现：MAC 响应携带 ID token 时被拒绝，
省略 ID token 时却被接受。应用现将 Bearer 要求独立于 ID token 校验，
新增两项拒绝回归；Windows 当前 44 项策略检查通过，Linux、sanitizer 和
更新后的真实 Hydra 输入仍在验证。上述已封存的 126/33 结果对应 R2，
没有将旧哈希改成当前实现。本轮共享 Core 的七库计量与交付也正在独立
目录运行，复制只遍历源文件清单，交付操作不接触覆盖率的二进制和 profile。

## 2026-10-04：OIDC 刷新 Bearer 要求补齐

上述刷新类型遗漏已经完成修复和验收。`oidcExampleRequireBearer` 独立
检查此应用支持的 token 类型；初次 ID token 校验、真实示例刷新、独立
策略探针和真实 Hydra 探针统一使用。刷新不返回 ID token 时也不跳过。
这是 Bearer 应用的支持范围，不限制通用 OAuth 库可解析的 token_type。

Windows、Linux、Linux ASan/UBSan 各 44 项策略检查通过，共 132 项；
新增 MAC with-ID/without-ID 拒绝用例经过独立签名与内存归零检查。
两个实际离线示例故障程序再次通过 Linux ASan/UBSan，包括 39 个应用
校验分配故障与 58 个签发故障。更新后的 C 源码重新编译，并通过 33 个
真实 Hydra 用例；三个认证方式、六个独立 Python 控制、服务停止及私钥
清理均核验。未重复正常路径的全部 32 个旧程序，也未把其哈希换成当前
源码；当前行为由新策略、故障与真实服务验证。

固定输入为 `out/goal-temp/oidc-bearer-fixed-20261004` 与
`/var/tmp/xrt-oidc-bearer-fixed-20261004`。原绕过证据在
`/var/tmp/xrt-oidc-bearer-original-20261004`；独立夹具在旧 C 实现上得到
without-ID 接受，并按预期令测试工具失败，作为原因明确的原行为证据。
400 个源文件、二进制、报告和日志按字节归档到
`out/goal-temp/oidc-bearer-acceptance-archive-20261004`，归档不执行。
权威记录为 `out/goal-temp/oidc-bearer-acceptance-evidence.json`，SHA256
`8930a3ddda1b3d7121d3d51b6a4adefbf7077e795ed199043c62355b776e6e44`。
长期目标保持 active；当前 Core 的覆盖率/交付、历史退出来源和外部
发布验收仍继续处理。

## 2026-10-04：当前 Core 的七库计量与交付封存

Windows 的最后三个 ACME 计量命令已结束；两平台各 22 个计量/生成检查命令及各 23 个独立包消费/交付命令全部返回 0。计量树与交付树分开，未再次执行已计量或归档的二进制。14 份主报告与 14 份私有头报告核对了当前实际消费源码/依赖、原始 gcno/gcda 及主报告 SHA256。所有报告使用新计数，没有把历史结果重绑当前输入。

| 库 | Windows 自有 C 行 / 分支结果 | Linux 自有 C 行 / 分支结果 |
| --- | --- | --- |
| `xjwt` | 1134/1212 (93.56%) / 811/1086 (74.68%) | 1134/1212 (93.56%) / 811/1086 (74.68%) |
| `xoauth2` | 1175/1271 (92.45%) / 829/1074 (77.19%) | 1173/1271 (92.29%) / 827/1074 (77.00%) |
| `xmail` | 4536/5755 (78.82%) / 3158/4858 (65.01%) | 4540/5759 (78.83%) / 3158/4858 (65.01%) |
| `xpop3` | 669/869 (76.99%) / 410/680 (60.29%) | 669/869 (76.99%) / 410/680 (60.29%) |
| `xsmtp` | 806/983 (81.99%) / 528/826 (63.92%) | 806/983 (81.99%) / 528/826 (63.92%) |
| `ximap` | 2429/2936 (82.73%) / 1792/2697 (66.44%) | 2429/2936 (82.73%) / 1791/2697 (66.41%) |
| `xacme` | 5688/6556 (86.76%) / 4593/6551 (70.11%) | 5683/6565 (86.57%) / 4604/6567 (70.11%) |

私有头单独统计，不能并入以上自有 C 分母：

| 库 | Windows 私有实现行 / 分支结果 | Linux 私有实现行 / 分支结果 |
| --- | --- | --- |
| `xjwt` | 21/21 (100.00%) / 19/34 (55.88%) | 21/21 (100.00%) / 19/34 (55.88%) |
| `xoauth2` | 无适用的私有头实现 | 无适用的私有头实现 |
| `xmail` | 50/53 (94.34%) / 50/76 (65.79%) | 50/53 (94.34%) / 50/76 (65.79%) |
| `xpop3` | 无适用的私有头实现 | 无适用的私有头实现 |
| `xsmtp` | 无适用的私有头实现 | 无适用的私有头实现 |
| `ximap` | 无适用的私有头实现 | 无适用的私有头实现 |
| `xacme` | 260/279 (93.19%) / 263/356 (73.88%) | 259/278 (93.17%) / 263/356 (73.88%) |

原件及副本逐字节核对后封存了 54370 个文件，证据为 `out/goal-temp/seven-current-core-acceptance-evidence.json`，SHA256 `7554d6952c449c070789641fe9790141f177a960b6eb9b2e1b8e17d389a248e3`。认证包每平台六个消费者、五个清单产品的显式单头/裁剪/静态与共享包消费以及模块/单头 ACME 核心操作范例均通过。OIDC 私有应用策略与本轮计量冻结后发生的七个范例/夹具/文档差异，分别保留最新 Bearer 专项证据和差异清单；这些差异不参与库自有 C 计量或包构建输入。

本地交付门禁已通过。托管 CI 尚未运行这份补丁；GitHub 现有 Nightly 使用旧提交，不能作替代证据。历史 Huawei/POLICE 退出失败只定位到最终全局内存检查，原计数与分配来源未保存，继续保留未解决记录。Pebble、Dovecot/Postfix 和 Hydra 是独立实现的受控服务验收，真实运营商账户及用户指定部署环境仍待明确和执行；不据此宣告生产整体验收完成。

## 2026-10-04：Buypass 历史预设与服务状态说明

交付复查发现 `xacme` README 把 Buypass 与其他签发 CA 并列。根据
[Buypass 官方公告](https://www.buypass.com/products/tls-ssl-certificates/discontinues-issuance-of-tls-ssl-certificates)，
TLS/SSL 申请与续签已于 2025-10-15 停止，最后签发日期为 2025-10-31。
README 现明确标记 `XACME_DIRECTORY_BUYPASS` 与
`XACME_DIRECTORY_BUYPASS_TEST` 为历史兼容 URL 常量，不能作为新的签发
或续签服务。该说明只修改文档，公开宏值、API、生成头和编译输入未变；
当前 14 对覆盖率及包消费证据继续保留原始输入绑定。生成的 API 符号参考
继续登记常量，并通过其 README 链接提供服务状态说明，不手工改动生成文件。

## 2026-10-04：公共 API 清单检查器的误报与缺失输入修复

原检查器直接扫描头文件全文，把注释中的 `XACME_DIRECTORY_*` 示例、
字符串及 include 路径中的标识符登记为公共符号。它还在验证路径存在之前
过滤扩展公共头，因此清单登记的头文件缺失时可能返回成功。冻结原工具和
真实参考文档后，独立夹具复现了这两项缺陷；原证据保留于
`out/goal-temp/api-inventory-original-r2-20261004/original-proof.json`。

检查器现在先验证所有登记输入，再提取声明；扫描前处理 C 反斜杠续行，
掩去注释、字符串、字符字面量和 include 路径，生成器使用同一规则。
Windows、Linux 各 10 项回归测试通过；六个 Core 家族与九个扩展清单
消费方共 34 个生成/文档检查命令通过，连同两个平台的回归命令共 36 项。
15 份参考文档中仅四份变化，没有真实导出函数增删。ACME 仍有 43 个
函数，常量由 52 个纠正为 51 个，类型由 24 个纠正为 21 个。其他变化
删除注释假符号，或将符号归属从注释所在头移到实际声明所在头。

这项修复仅影响文档清单工具与生成参考，不修改 C API、ABI 或生成 C 头。
验收证据为 `out/goal-temp/api-inventory-acceptance-evidence-20261004.json`；
本轮对原始主报告/私有头报告及实际消费输入重新核对，旧报告计数与哈希
不重写。长期目标仍为 active；托管 CI、历史 Huawei 退出分配来源及真实
服务/部署验收的未完成边界继续保留。

## 2026-10-04：当前 Core 的独立邮件服务验收

输入审计确认：此前 Dovecot/Postfix 验收与当前工作区有 18 项已登记输入
差异，包括 `include/xrt/value.h`、`src/value/value_container.c`、两个 Core
聚合头及八个邮件聚合头；旧服务结果保留原始指纹，不作为当前 Core 的
互操作证明。本轮冻结当前 854 个源码/构建输入，两平台指纹一致，并在
独立新目录重新编译三个 C 范例，没有执行或修改覆盖率二进制及 profile。

Windows、Linux 各 18 项实际服务场景通过，共 36 项。三协议在隐式 TLS
与 STARTTLS/STLS 下均验证正常访问、错误密码和错误证书身份。SMTP
核对实际 LMTP/Maildir 投递的发件人、收件人、主题与含点行正文；POP3
核对实际取回内容，IMAP 核对实际邮箱计数。负向场景检查客户端错误及
独立服务端日志，确认没有认证成功或额外投递。Dovecot 2.4.2 与 Postfix
3.10.6 使用原来已校验的解包运行时，运行前后文件哈希一致；临时密码与
私钥清理完成，另行检查原服务 PID 已消失、全部监听关闭。

权威证据为 `out/goal-temp/mail-current-core-acceptance-evidence-20261004.json`。
当前四个邮件产品在两平台的八对主报告/私有头报告仍按原字节和实际消费
输入核对，未重绑或改写其计数。新证据只关闭当前 Core 的受控独立邮件
服务互操作项，真实提供方策略、托管 CI 与指定部署环境的边界继续保留。

## 2026-10-04：当前 Core 的独立 Pebble 兼容性验收

此前 ACME 独立服务记录对应旧 Value 接口/实现、Core 与 ACME 聚合头及
模块清单。本轮在新目录冻结当前源码及构建输入，重新编译非覆盖率
`test_flow` 客户端；Windows/Linux GCC 严格构建通过，Linux 另用 Clang
ASan/UBSan 编译，开启泄漏检测和错误即停。三种配置各运行签发基线、
授权复用及备用证书链三个实际 Pebble 场景，共 9 项全部通过。

独立运行器确认真实 DNS TXT 验证、证书/私钥/SAN 配对、吊销状态和原因，
以及两套落盘授权的完整链。备用链场景只用预期的另一根 CA 验证手动
下载链，避免同时信任两根 CA 掩盖偏好选择失效；两套存储授权仍对应
默认根。C 流程同时完成账户换钥、停用、存储签发与阈值内复用。
所有测试服务已退出，系统 CA 信任库未改动，未使用真实云账户。

R1 最小冻结目录漏了构建器在编译前读取的 Core 单头测试文件，两平台
构建均在 `_single_test_defines` 中因缺失文件退出，尚未编译客户端。
原目录与日志保留；R2 补齐测试输入，冻结 2240 个文件并在全新目录
执行正常与 sanitizer 验证，没有在已执行目录修改源码或复用旧二进制。
原覆盖率计数与输入绑定也未改写。

权威证据为 `out/goal-temp/acme-current-core-pebble-acceptance-evidence-20261004.json`。
这项证据补齐当前 Core 的受控独立 ACME 兼容性，原证书完整性/相对 URI
专项记录继续按其原始输入保存；托管 CI、历史 Huawei 原计数/分配来源
以及真实提供方与用户指定部署验收仍未完成。

## 阿里云 DNS 与 QQ 邮箱实际验收

本轮修复和在线验证已完成；生产整体验收仍未完成。修改保留在 master 工作区，没有创建分支、提交、推送或 PR。

Windows/Linux 的 test.xxrpa.com 均完成 Let’s Encrypt staging DNS-01 签发、账户持久化、存储授权读取和缓存复用。独立核对证书/私钥公钥、SAN、有效期、链签名及官方 staging 根；Linux 私钥为 0600，Windows 测试存储仅当前用户和 SYSTEM 可访问。最终只读 DNS 盘点为零条测试 TXT。staging 是测试 CA，系统信任库没有修改。

两平台 SMTP 均获得提交成功；POP3 使用同一封明确复制到 INBOX 的自有测试样本，输出均为 1899 字节且 SHA256 相同；IMAP 完成只读 EXAMINE。样本只复制一次，最终 IMAP 逐项头扫描及 BODY.PEEK 对账确认原件与副本完全一致。该证据不能冒充无需复制的 SMTP 到收件箱自动投递。QQ 的业务完成后 TLS 关闭警告单独保留，库的严格 TLS 关闭要求没有放宽。

修复 Ali 的 ENABLE/DISABLE 完整状态值及准确的 InvalidRR.NoExist 缺失响应；修复 POP3 对数字/可打印标点扩展标签的拒绝，未知 XOAUTH2 不启用已知能力或 SASL；修复 Windows 范例的 CRCRLF 输出。双平台 72 项非计量 TLS 回归通过，旧 CAPA 与旧 Windows 输出二进制均被新夹具检出。完整模块计量、独立 TLS/HTTP/provider/mock 场景及私有头检查重新执行；当前生成头和静态/共享包消费者也通过。

| 库 | Windows 行 / 分支 | Linux 行 / 分支 |
| --- | --- | --- |
| xacme | 5691/6557 (86.79%) / 4601/6557 (70.17%) | 5684/6566 (86.57%) / 4611/6573 (70.15%) |
| xmail | 4536/5755 (78.82%) / 3160/4858 (65.05%) | 4540/5759 (78.83%) / 3160/4858 (65.05%) |
| xpop3 | 675/867 (77.85%) / 417/676 (61.69%) | 675/867 (77.85%) / 417/676 (61.69%) |
| xsmtp | 808/983 (82.20%) / 529/826 (64.04%) | 808/983 (82.20%) / 529/826 (64.04%) |
| ximap | 2429/2936 (82.73%) / 1793/2697 (66.48%) | 2429/2936 (82.73%) / 1792/2697 (66.44%) |

表中只统计库自有 C；私有头分开计量，无适用实现的产品为 N/A。旧报告、原失败和原输入保留。详细报告包含实际编译输入、二进制、原始计数及日志绑定；归档只复制白名单证据，不含凭据或私钥。

尚缺当前主线托管 CI、历史 Huawei 原退出计数/分配来源，以及最终部署/OAuth 目标确认。QQ 自动投递边界继续单独记录。长期目标没有标记完成。

Windows 的测试宿主在生成密钥前限制了存储根 ACL；本项实测不表示库会替宿主自动设置 Windows ACL。生产部署应遵循 xacme README 的存储权限要求。

权威证据：`out/goal-temp/seven-real-provider-acceptance-evidence-20261004.json`；当前运行时/测量输入核对 1035 个，当前十对报告的原始对象计数另行核验。此前阶段文字中的“当前”指该阶段的冻结输入，最新状态以本节为准。

## 共享 TCP 发送与关闭的引用释放竞争

后续 IMAP 压缩故障计量首次在引擎退休后的内存清理断言失败。ThreadSanitizer 随后在原源码中捕获 `ActiveDepth` 的跨线程读写：发送调用线程递减普通活动计数，同时 Worker 的关闭路径决定是否延迟释放运行时引用。受控交错在 Windows/Linux 均复现发送者先退出、Worker 后设置 `ReleasePending`，导致无人再执行释放；遗留一个 656 字节 Stream 和一个 124 字节错误对象，并触发同一清理断言。该复现说明一个具体原因；首次失败没有记录活动分配点，不能补称当时已经观测到这些分配。

活动深度现在只保护 Worker 上的同步重入，跨线程提交继续由原有原子提交计数保护；跨线程快路径也不再提前读取 Worker 所属的普通缓冲状态。公开 API、结构布局及协议行为不变。Windows/Linux 的受控旧代码均退出 1，修复后均退出 0；Linux 的 IMAP 压缩故障、TCP 生命周期和多线程发送测试通过 ThreadSanitizer。新增 Linux CI 步骤持续检查 IMAP 故障生命周期的数据竞争。

同时修复 Compose 范例的 Windows CRLF 转换以及忽略写入/刷新失败的问题，新增独立 MIME 解析与真实失败输出句柄回归。中、英、俄文十二个邮件章节已核对构建命令、API 归属及离线业务行为。该阶段的双平台回归、八对邮件计量及原始计数复核证据为 `out/goal-temp/seven-mail-book-compose-acceptance-evidence-20261004.json`。

本次共享 TCP 修改后，十份受影响生成单头刷新及二十条生成/只读检查通过。Windows/Linux 重新执行 44 条覆盖率验收和 63 条交付命令，单文件、裁剪、静态/共享包、认证库六种包消费者及核心离线范例通过。两平台 Compose 新二进制通过独立 MIME/规范 CRLF 解析及真实只读 stdout 失败句柄检查。十四对主报告/私有头报告已按原始计数重建复核，没有把旧计数改绑新源码。

重建后的独立客户端通过 Dovecot/Postfix 36 场景、Hydra 33 场景和 Pebble 9 场景，包含 Linux ASan/UBSan。Pebble 使用真实 DNS 验证，证书/私钥/SAN/链/吊销和两套存储授权由独立实现核对；默认根、授权复用与备用链的根偏好均正确。服务全部退出，临时服务凭据清除。受控独立服务和此前真实云账户验收分别记录。

本轮库自有 C 覆盖率如下；私有头单独统计，详细报告保留编译输入、原始计数和未覆盖分支。

| 库 | Windows 行 / 分支 | Linux 行 / 分支 |
| --- | --- | --- |
| xacme | 5691/6557 (86.79%) / 4601/6557 (70.17%) | 5684/6566 (86.57%) / 4611/6573 (70.15%) |
| xmail | 4536/5755 (78.82%) / 3160/4858 (65.05%) | 4540/5759 (78.83%) / 3160/4858 (65.05%) |
| xpop3 | 675/867 (77.85%) / 417/676 (61.69%) | 675/867 (77.85%) / 417/676 (61.69%) |
| xsmtp | 808/983 (82.20%) / 529/826 (64.04%) | 808/983 (82.20%) / 529/826 (64.04%) |
| ximap | 2429/2936 (82.73%) / 1793/2697 (66.48%) | 2429/2936 (82.73%) / 1793/2697 (66.48%) |
| xjwt | 1134/1212 (93.56%) / 811/1086 (74.68%) | 1134/1212 (93.56%) / 811/1086 (74.68%) |
| xoauth2 | 1175/1271 (92.45%) / 829/1074 (77.19%) | 1173/1271 (92.29%) / 827/1074 (77.00%) |

权威证据为 `out/goal-temp/seven-tcp-send-acceptance-evidence-20261005.json`。历史 Huawei 的具体退出计数仍未取得，当前主线托管 CI、仓库基线文档门禁和最终部署/OAuth 范围继续保留，整体验收未完成。首次历史 IMAP 分配位置未被补录；当前复现的引用释放原因已修复并通过专项和全量验收。
